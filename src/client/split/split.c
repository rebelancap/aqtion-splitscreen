/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// split.c -- splitscreen client contexts and local players 2..8
//
// Context 0 is a static, zero-initialised object (BSS, no allocation for
// N=1) and cla points at it from load time, so anything that touches cl
// before CL_Init (cvar callbacks, Com_Error paths) is safe. Extra contexts
// are Z_Mallocz'd by `addplayer' and freed when the player leaves.
//
// Every extra is an ordinary client of the server it shares with player 1:
// its own UDP socket (NS_CLIENT2..8), qport, challenge, netchan and
// stufftext buffer. Its packets, stufftext and per-frame work run with its
// context active and under an error guard (CL_SplitTrapError), so an error
// in its processing drops only that player.
//
// Note: the `cl' macro would rewrite a `->cl' member access, so this file
// names that member only inside CL_ContextCl.

#include "../client.h"
#include <setjmp.h>

static client_context_t cl_context0;

client_context_t *clx[MAX_SPLITVIEW] = { &cl_context0 };
client_context_t *cla = &cl_context0;

// frame guard (1) + a nested p<N> from an extra's stufftext (2) + the screen
// loop, which can run inside either (3)
#define MAX_CONTEXT_STACK   3

static client_context_t *cl_contextstack[MAX_CONTEXT_STACK];
static int              cl_contextdepth;

#define SPLIT_MAX_TRIES     10      // connect attempts (CONNECT_DELAY apart)
#define SPLIT_MAX_REFUSALS  5       // "Bad challenge." retries

static cvar_t   *cl_splitdebug;
static cvar_t   *cl_splitMaxPlayers;

// error guard
static jmp_buf  split_guard;
static int      split_guarded;      // player whose work is guarded, 0 = none
static char     split_errmsg[MAXERRORMSG];
static int      split_debugerror;   // `splitdebug error <n>'
static char     split_p1error[MAXERRORMSG];     // player 1's error, until its CL_Disconnect

// extras to bring back when player 1 returns to the server it lost (M3)
#define SPLIT_REJOIN_TIME   (10 * 60 * 1000)

static struct {
    bool        valid;
    netadr_t    adr;
    unsigned    time;
    bool        slot[MAX_SPLITVIEW];
    char        profile[MAX_SPLITVIEW][PROFILE_NAME_MAX + 1];
} split_rejoin;

#pragma push_macro("cl")
#undef cl
static client_state_t *CL_ContextCl(client_context_t *ctx)
{
    return &ctx->cl;
}
#pragma pop_macro("cl")

// the connection slice of a context: in cls while it is active
static client_conn_t *CL_ContextConn(client_context_t *ctx)
{
    return ctx == cla ? &cls.conn : &ctx->conn;
}

static const char *CL_StateName(connstate_t state)
{
    static const char *const names[] = {
        "uninitialized", "disconnected", "challenging", "connecting",
        "connected", "loading", "precached", "active", "cinematic"
    };

    if (state < 0 || state >= q_countof(names))
        return "?";
    return names[state];
}

static void CL_SplitLog(int n, const char *fmt, ...) q_printf(2, 3);

static void CL_SplitLog(int n, const char *fmt, ...)
{
    char        buffer[MAX_STRING_CHARS];
    va_list     argptr;

    va_start(argptr, fmt);
    Q_vsnprintf(buffer, sizeof(buffer), fmt, argptr);
    va_end(argptr);

    Com_Printf("[P%d %u] %s", n + 1, Sys_Milliseconds(), buffer);
}

static void CL_InitContextDefaults(client_context_t *ctx, int playerNum)
{
    ctx->playerNum = playerNum;
    ctx->sock = playerNum ? (netsrc_t)(NS_CLIENT2 + playerNum - 1) : NS_CLIENT;
    ctx->inUse = true;

    // SCR_DrawXhair animation start values (were function statics)
    ctx->scr.xh_deltatime_factor = 0.001f;
    ctx->scr.xh_gap = 5;
    ctx->scr.xh_length = 4;
    ctx->scr.xh_elasticity = 0.04f;
}

static void CL_SplitSetHand(const client_context_t *ctx);
static cvar_t *CL_SplitShadowVar(int n, const char *name, bool create);
static const char *CL_SplitPlayerName(int n);

static void CL_ActivateContext(client_context_t *ctx)
{
    if (ctx == cla)
        return;

    memcpy(&cla->conn, &cls.conn, sizeof(cls.conn));
    cla = ctx;
    memcpy(&cls.conn, &cla->conn, sizeof(cls.conn));
    CL_SplitSetHand(ctx);
}

void CL_SetContext(int i)
{
    Q_assert(i >= 0 && i < MAX_SPLITVIEW);
    Q_assert(clx[i] && clx[i]->inUse);

    CL_ActivateContext(clx[i]);
}

void CL_PushContext(int i)
{
    Q_assert(cl_contextdepth < MAX_CONTEXT_STACK);

    cl_contextstack[cl_contextdepth++] = cla;
    CL_SetContext(i);
}

void CL_PopContext(void)
{
    Q_assert(cl_contextdepth > 0);

    CL_ActivateContext(cl_contextstack[--cl_contextdepth]);
}

// pop back to the given stack depth (error unwinding)
static void CL_UnwindContexts(int depth)
{
    while (cl_contextdepth > depth)
        CL_PopContext();
}

static bool CL_ContextOnStack(const client_context_t *ctx)
{
    int i;

    if (ctx == cla)
        return true;
    for (i = 0; i < cl_contextdepth; i++)
        if (cl_contextstack[i] == ctx)
            return true;
    return false;
}

// the screen loop's push: never asserts (a full stack skips the cell)
bool CL_SplitPushView(int i)
{
    if (cl_contextdepth >= MAX_CONTEXT_STACK)
        return false;
    CL_PushContext(i);
    return true;
}

int CL_SplitServerCount(const client_context_t *ctx)
{
    return CL_ContextCl((client_context_t *)ctx)->servercount;
}

void CL_ForEachContext(void (*fn)(void))
{
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++) {
        if (!clx[i] || !clx[i]->inUse)
            continue;
        CL_PushContext(i);
        fn();
        CL_PopContext();
    }
}

/*
===============================================================================

ERROR GUARD

===============================================================================
*/

/*
====================
CL_SplitRunGuarded

Runs fn with extra player n active. A Com_Error(ERR_DROP / ERR_DISCONNECT)
raised inside is trapped by CL_SplitTrapError and drops only player n.
Returns false if it did.
====================
*/
static bool CL_SplitRunGuarded(int n, void (*fn)(void *), void *arg)
{
    volatile int depth = cl_contextdepth;

    Q_assert(n > 0 && clx[n]);

    if (split_guarded) {
        // nested (an extra's stufftext running p<N>): the outer guard catches;
        // one stack slot stays free for the screen loop
        if (cl_contextdepth >= MAX_CONTEXT_STACK - 1) {
            Com_Printf("p%d: not from inside another player's command\n", n + 1);
            return true;
        }
        CL_PushContext(n);
        fn(arg);
        CL_PopContext();
        return true;
    }

    if (setjmp(split_guard)) {
        split_guarded = 0;
        CL_UnwindContexts(depth);
        SCR_ResetAfterError();
        if (clx[n] && !clx[n]->dropPending) {
            clx[n]->dropPending = true;
            clx[n]->dropNotice = true;  // the server's doing (or a broken packet)
            Q_strlcpy(clx[n]->dropReason, split_errmsg, sizeof(clx[n]->dropReason));
        }
        return false;
    }

    split_guarded = n;
    CL_PushContext(n);
    fn(arg);
    CL_PopContext();
    split_guarded = 0;
    return true;
}

/*
====================
CL_SplitTrapError

Called by Com_Error before it shuts anything down. Inside a guarded extra
player's work: longjmp back to the guard. Otherwise make player 1 the active
context again, so the stock error path disconnects player 1 (who drops the
extras first).
====================
*/
void CL_SplitTrapError(error_type_t code, const char *msg)
{
    if (split_guarded && code != ERR_FATAL) {
        Q_strlcpy(split_errmsg, msg, sizeof(split_errmsg));
        longjmp(split_guard, 1);
    }

    split_guarded = 0;
    CL_UnwindContexts(0);
    CL_ActivateContext(clx[0]);

    // CL_SplitDisconnect decides from it whether the extras come back
    Q_strlcpy(split_p1error, msg, sizeof(split_p1error));
}

/*
===============================================================================

JOIN / LEAVE

===============================================================================
*/

static int CL_PlayersIn(void)
{
    int i, count = 0;

    for (i = 0; i < MAX_SPLITVIEW; i++)
        if (clx[i] && CL_ContextConn(clx[i])->state >= ca_connected)
            count++;
    return count;
}

// called with player n's context NOT active and not on the stack
static void CL_SplitFree(int n, const char *reason)
{
    client_context_t *ctx = clx[n];
    netsrc_t sock = ctx->sock;

    Com_Printf("Player %d left: %s\n", n + 1, reason);

    CL_PushContext(n);
    CL_Disconnect(ERR_DISCONNECT);  // extra: own netchan and state only
    CL_PopContext();

    NET_CloseSplitSocket(sock);
#if USE_ZLIB
    inflateEnd(&ctx->z);
#endif
    if (cmd_current == &ctx->cmdbuf)
        cmd_current = &cmd_buffer;

    clx[n] = NULL;
    if (ctx->dropNotice) {
        // "Player 3 was kicked" needs no "Player 3: " in front
        const char *who = va("Player %d ", n + 1);
        CL_SplitSetNotice(n, strncmp(reason, who, strlen(who)) ? va("Player %d: %s", n + 1, reason) : reason);
    }
    Z_Free(ctx);

    CL_ProfileLeave(n);     // a profile is saved; the slot is a guest again

    CL_SplitLayout(true);
}

// drop now if safe, else at the next safe point
static void CL_SplitDropPlayer(int n, const char *reason)
{
    client_context_t *ctx = clx[n];

    if (!ctx || n == 0)
        return;

    if (CL_ContextOnStack(ctx) || split_guarded) {
        if (!ctx->dropPending) {
            ctx->dropPending = true;
            Q_strlcpy(ctx->dropReason, reason, sizeof(ctx->dropReason));
        }
        return;
    }

    if (ctx->dropPending) {
        char text[MAX_STRING_CHARS];

        Q_strlcpy(text, ctx->dropReason, sizeof(text));
        CL_SplitFree(n, text);
        return;
    }

    CL_SplitFree(n, reason);
}

// a drop that is the server's doing: the reason stays up in the cell
static void CL_SplitDropNotice(int n, const char *reason)
{
    if (n > 0 && clx[n] && !clx[n]->dropPending)
        clx[n]->dropNotice = true;
    CL_SplitDropPlayer(n, reason);
}

static void CL_SplitRunDrops(void)
{
    int n;

    for (n = 1; n < MAX_SPLITVIEW; n++)
        if (clx[n] && clx[n]->dropPending)
            CL_SplitDropPlayer(n, clx[n]->dropReason);
}

static void CL_SplitDropAll(const char *reason)
{
    int n;

    for (n = 1; n < MAX_SPLITVIEW; n++)
        if (clx[n])
            CL_SplitDropPlayer(n, reason);
}

/*
====================
CL_SplitTarget

Where extras connect: player 1's local server over 127.0.0.1 (its loopback
is player 1's only), or player 1's remote server.
====================
*/
static bool CL_SplitTarget(netadr_t *adr, char *why, size_t size)
{
    if (cls.serverAddress.type != NA_LOOPBACK) {
        *adr = cls.serverAddress;
        return true;
    }

    if (Cvar_VariableInteger("maxclients") <= 1 || !NET_GetAddress(NS_SERVER, adr)) {
        Com_Printf("addplayer: this is a single-player game (maxclients 1), "
                   "which has room for one player only.\n"
                   "Start a multiplayer game for splitscreen, e.g. "
                   "\"deathmatch 1; map <name>\".\n");
        Q_strlcpy(why, "single-player game: start a multiplayer one (deathmatch 1)", size);
        return false;
    }

    if (adr->type == NA_IP && !adr->ip.u32[0]) {
        // bound to any interface: use the loopback interface
        adr->ip.u8[0] = 127;
        adr->ip.u8[3] = 1;
    }
    return true;
}

// local players allowed at once (M1 layouts draw up to 4)
int CL_SplitMaxPlayers(void)
{
    return Q_clip(cl_splitMaxPlayers->integer, 1, MAX_SPLITVIEW);
}

/*
====================
CL_SplitAddPlayer

Join player 1's server as the lowest free local player slot below
cl_splitMaxPlayers. Returns the slot, or -1 with the reason printed and a
short form in why (the pad join hint). Player 1 must be active.
====================
*/
int CL_SplitAddPlayer(char *why, size_t size)
{
    client_context_t *ctx;
    netadr_t adr;
    int n, protocol, maxPlayers = CL_SplitMaxPlayers();

    if (cla->playerNum) {
        Com_Printf("addplayer: only player 1 can add players\n");
        Q_strlcpy(why, "only player 1 can add players", size);
        return -1;
    }
    if (cls.demo.playback) {
        Com_Printf("addplayer: not while a demo is playing\n");
        Q_strlcpy(why, "not while a demo is playing", size);
        return -1;
    }
    if (cls.state < ca_connected) {
        Com_Printf("addplayer: start or join a multiplayer game first, "
                   "e.g. \"deathmatch 1; map <name>\"\n");
        Q_strlcpy(why, "start or join a multiplayer game first", size);
        return -1;
    }

    for (n = 1; n < maxPlayers; n++)
        if (!clx[n])
            break;
    if (n >= maxPlayers) {
        Com_Printf("addplayer: all %d local player slots are in use (cl_splitMaxPlayers %d)\n",
                   maxPlayers, maxPlayers);
        Q_snprintf(why, size, "%d players is the most (cl_splitMaxPlayers %d)", maxPlayers, maxPlayers);
        return -1;
    }

    if (!CL_SplitTarget(&adr, why, size))
        return -1;

    if (!NET_OpenSplitSocket((netsrc_t)(NS_CLIENT2 + n - 1), adr.type == NA_IP6)) {
        Com_Printf("addplayer: couldn't open a UDP socket for player %d\n", n + 1);
        Q_snprintf(why, size, "couldn't open a network socket for player %d", n + 1);
        return -1;
    }

    ctx = Z_Mallocz(sizeof(*ctx));
    CL_InitContextDefaults(ctx, n);

    // own stufftext buffer, wired like player 1's (CL_Init)
    ctx->cmdbuf = cl_context0.cmdbuf;
    ctx->cmdbuf.text = ctx->cmdbuf_text;
    ctx->cmdbuf.cursize = 0;
    ctx->cmdbuf.waitCount = 0;
    ctx->cmdbuf.aliasCount = 0;

#if USE_ZLIB
    if (inflateInit2(&ctx->z, -MAX_WBITS) != Z_OK) {
        Com_Printf("addplayer: inflateInit2 failed\n");
        Q_strlcpy(why, "inflateInit2 failed", size);
        NET_CloseSplitSocket(ctx->sock);
        Z_Free(ctx);
        return -1;
    }
#endif

    protocol = cls.serverProtocol;
    clx[n] = ctx;
    CL_SplitSetNotice(n, NULL);

    CL_PushContext(n);
    CL_ClearEffects();
    CL_ClearTEnts();
    cls.serverAddress = adr;
    Q_strlcpy(cls.servername, NET_AdrToString(&adr), sizeof(cls.servername));
    cls.serverProtocol = protocol;
    cls.passive = false;
    cls.state = ca_challenging;
    cls.connect_time = cls.realtime - CONNECT_DELAY;
    cls.connect_count = 0;
    CL_PopContext();

    CL_SplitLog(n, "joining %s (socket %d)\n", NET_AdrToString(&adr), ctx->sock);

    CL_SplitLayout(true);
    return n;
}

// addplayer [profile]
static void CL_AddPlayer_f(void)
{
    char why[MAX_QPATH * 2];
    int n = CL_SplitAddPlayer(why, sizeof(why));

    if (n > 0 && Cmd_Argc() > 1 && !CL_ProfileLoad(n, Cmd_Argv(1), why, sizeof(why)))
        Com_Printf("addplayer: %s; player %d is a guest\n", why, n + 1);
}

// a pad's leave / grace drop (in_gamepad.c)
void CL_SplitRequestDrop(int n, const char *reason)
{
    if (n > 0 && n < MAX_SPLITVIEW)
        CL_SplitDropPlayer(n, reason);
}

// player n is in the game and not leaving
bool CL_SplitPlayerLive(int n)
{
    return n >= 0 && n < MAX_SPLITVIEW && clx[n] && clx[n]->inUse && !clx[n]->dropPending;
}

static void CL_DropPlayer_f(void)
{
    int n;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <player number 2..%d>\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }

    n = Q_atoi(Cmd_Argv(1)) - 1;
    if (n == 0) {
        Com_Printf("%s: player 1 is the host; use disconnect\n", Cmd_Argv(0));
        return;
    }
    if (n < 0 || n >= MAX_SPLITVIEW || !clx[n]) {
        Com_Printf("%s: no local player %s\n", Cmd_Argv(0), Cmd_Argv(1));
        return;
    }

    CL_SplitDropPlayer(n, "dropplayer");
}

static void CL_SplitPlayers_f(void)
{
    client_context_t *ctx;
    client_conn_t *conn;
    const player_state_t *ps;
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++) {
        ctx = clx[i];
        if (!ctx)
            continue;
        conn = CL_ContextConn(ctx);
        ps = &CL_ContextCl(ctx)->frame.ps;
        Com_Printf("P%d: %-12s socket %d  qport %5d  client %3d  origin %5d %5d %5d  "
                   "cell %d,%d %dx%d  server %s  name \"%s\"\n",
                   i + 1, CL_StateName(conn->state), ctx->sock, conn->quakePort,
                   conn->state >= ca_precached ? CL_ContextCl(ctx)->clientNum : -1,
                   (int)ps->pmove.origin[0] / 8, (int)ps->pmove.origin[1] / 8,
                   (int)ps->pmove.origin[2] / 8,
                   ctx->cell.x, ctx->cell.y, ctx->cell.width, ctx->cell.height,
                   conn->serverAddress.type ? NET_AdrToString(&conn->serverAddress) : "-",
                   CL_SplitPlayerName(i));
    }
}

static void CL_SplitExecFn(void *arg)
{
    Cmd_ExecuteString(&cmd_buffer, arg);
}

/*
====================
CL_SplitPlayerCmd_f

p<N> <command>: run one command as local player N (+forward style buttons
act on that player's input, unknown commands go to that player's server).
====================
*/
static void CL_SplitPlayerCmd_f(void)
{
    char text[MAX_STRING_CHARS];
    cvar_t *shadow;
    int n = Cmd_Argv(0)[1] - '1';

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <command>\n", Cmd_Argv(0));
        return;
    }
    if (n < 0 || n >= MAX_SPLITVIEW || !clx[n]) {
        Com_Printf("%s: no local player %d\n", Cmd_Argv(0), n + 1);
        return;
    }
    if (n > 0 && !Q_stricmp(Cmd_Argv(1), "disconnect")) {
        CL_SplitDropPlayer(n, "left");
        return;
    }

    // p<N> <userinfo / synced cvar> [value]: that player's p<N>_ shadow
    if (n > 0 && (shadow = CL_SplitShadowVar(n, Cmd_Argv(1), Cmd_Argc() > 2)) != NULL) {
        if (Cmd_Argc() == 2)
            Com_Printf("\"%s\" is \"%s\"\n", shadow->name, shadow->string);
        else
            Cvar_SetByVar(shadow, Cmd_ArgsFrom(2), FROM_CONSOLE);
        return;
    }

    Q_strlcpy(text, Cmd_RawArgs(), sizeof(text));

    if (n == 0 || clx[n] == cla) {
        Cmd_ExecuteString(&cmd_buffer, text);
        return;
    }

    CL_SplitRunGuarded(n, CL_SplitExecFn, text);
}

/*
====================
CL_SplitDebug_f

splitdebug error <n>: raise an ERR_DROP inside player n's next frame, to
exercise the error guard.
====================
*/
static void CL_SplitDebug_f(void)
{
    int n;

    if (Cmd_Argc() == 3 && !strcmp(Cmd_Argv(1), "error")) {
        n = Q_atoi(Cmd_Argv(2)) - 1;
        if (n > 0 && n < MAX_SPLITVIEW && clx[n]) {
            split_debugerror = n;
            return;
        }
    }
    Com_Printf("Usage: %s error <player number 2..%d>\n", Cmd_Argv(0), MAX_SPLITVIEW);
}

/*
===============================================================================

CONNECT PATH HOOKS (extra player active)

===============================================================================
*/

/*
====================
CL_SplitMayResend

From CL_CheckForResend. Extras join one at a time (the server keeps one
challenge per IP address, and a server that counts connecting clients late
should apply its limits to the next one): an extra sends `getchallenge'
only while player 1 is in the game, no other extra is between challenging
and active, and no lower slot is waiting to challenge. Gives up after
SPLIT_MAX_TRIES unanswered attempts.
====================
*/
bool CL_SplitMayResend(void)
{
    client_conn_t *conn;
    int i, n = cla->playerNum;

    if (!n)
        return true;

    if (cla->dropPending)
        return false;

    if (cls.connect_count >= SPLIT_MAX_TRIES) {
        CL_SplitLog(n, "no answer from %s after %d tries, giving up\n",
                    NET_AdrToString(&cls.serverAddress), SPLIT_MAX_TRIES);
        CL_SplitDropNotice(n, va("can't join: no answer from the server (%d tries)", SPLIT_MAX_TRIES));
        return false;
    }

    if (cls.state != ca_challenging)
        return true;

    if (CL_ContextConn(clx[0])->state != ca_active)
        return false;

    for (i = 1; i < MAX_SPLITVIEW; i++) {
        if (i == n || !clx[i])
            continue;
        conn = CL_ContextConn(clx[i]);
        if (conn->state >= ca_connecting && conn->state < ca_active)
            return false;
        if (conn->state == ca_challenging && (i < n || conn->connect_count > 0))
            return false;
    }

    if (!cls.connect_count)
        CL_SplitLog(n, "challenging %s\n", NET_AdrToString(&cls.serverAddress));
    return true;
}

/*
===============================================================================

PER-PLAYER USERINFO AND CVAR SYNC (design 5.2)

Players 2..8 send player 1's userinfo with the keys below taken from their
own p<N>_<key> shadow cvars (created at startup for every slot, not
archived: profiles keep them, split_profile.c). Account identities are
blanked; everything else (password, uf, version) stays player 1's. A
shadow change resends that player's whole userinfo on its own netchan.

AQtion cvar sync: an extra answers the server's list from p<N>_<cvar> when
that cvar exists, else from the shared cvar (polled, so a shadow created
later with `set' is picked up too).

===============================================================================
*/

typedef struct {
    const char  *name;      // base cvar = userinfo key
    const char  *def;       // shadow default, NULL = the base cvar's
} split_userinfo_t;

static const split_userinfo_t split_userinfo[] = {
    { "name",       "Player %d" },
    { "skin",       NULL },
    { "hand",       NULL },
    { "fov",        NULL },
    { "gender",     NULL },
    { "spectator",  NULL },
    { "msg",        NULL },
    { "rate",       NULL },
};

#define SPLIT_NUM_USERINFO  q_countof(split_userinfo)
#define SPLIT_UI_HAND       2   // index of "hand" above

// per-account identities, never sent by an extra
static const char *const split_blank_keys[] = {
    "steamid", "steamcloudappenabled", "steamclouduserenabled",
    "cl_discord_id", "discordid"
};

static cvar_t   *split_shadow[MAX_SPLITVIEW][SPLIT_NUM_USERINFO];
static cvar_t   *split_hand1;       // player 1's `hand' (info_hand while P1 is active)

#define CVARSYNC_POLL_MSEC  20

static int CL_SplitUserinfoIndex(const char *name)
{
    int i;

    for (i = 0; i < SPLIT_NUM_USERINFO; i++)
        if (!Q_stricmp(name, split_userinfo[i].name))
            return i;
    return -1;
}

// n from a "p<N>_..." cvar name, -1 if none
static int CL_SplitShadowSlot(const char *name)
{
    if (name[0] != 'p' || name[1] < '2' || name[1] > '0' + MAX_SPLITVIEW || name[2] != '_')
        return -1;
    return name[1] - '1';
}

static void CL_SplitShadowChanged(cvar_t *self)
{
    client_conn_t *conn;
    int n = CL_SplitShadowSlot(self->name);

    if (n < 1 || !clx[n])
        return;

    conn = CL_ContextConn(clx[n]);
    if (conn->state < ca_connected)
        return;     // goes out with the connect request

    conn->userinfo_modified = MAX_PACKET_USERINFOS;
    if (cl_splitdebug->integer)
        CL_SplitLog(n, "%s changed, userinfo queued\n", self->name);

    // hand 2 also means "no gun": resend the setting on this player's netchan
    if (self == split_shadow[n][SPLIT_UI_HAND] && CL_SplitPushView(n)) {
        CL_UpdateGunSetting();
        if (cl_splitdebug->integer)
            CL_SplitLog(n, "gun setting resent on socket %d (hand %d)\n",
                        cla->sock, info_hand->integer);
        CL_PopContext();
    }
}

/*
====================
CL_SplitInitUserinfo

Called by CL_InitLocal once the userinfo cvars exist.
====================
*/
void CL_SplitInitUserinfo(void)
{
    const split_userinfo_t *u;
    cvar_t *base, *var;
    const char *def;
    int n, i;

    split_hand1 = info_hand;

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        for (i = 0, u = split_userinfo; i < SPLIT_NUM_USERINFO; i++, u++) {
            base = Cvar_FindVar(u->name);
            if (u->def)
                def = va(u->def, n + 1);
            else
                def = base ? base->default_string : "";
            // not archived (M2-T5): a guest's settings last for its stay,
            // a named player's live in its profile (split_profile.c)
            var = Cvar_Get(va("p%d_%s", n + 1, u->name), def, 0);
            var->flags &= ~CVAR_ARCHIVE;    // a "seta" line of an older config
            var->changed = CL_SplitShadowChanged;
            split_shadow[n][i] = var;
        }
    }
}

/*
====================
CL_SplitUserinfo

Overlays the active extra's shadows on player 1's userinfo (connect request
and full userinfo updates). Returns the new length.
====================
*/
size_t CL_SplitUserinfo(char *userinfo, size_t len)
{
    const char *value;
    int i, n = cla->playerNum;

    if (!n)
        return len;

    for (i = 0; i < SPLIT_NUM_USERINFO; i++) {
        value = split_shadow[n][i] ? split_shadow[n][i]->string : "";
        if (!i && !*value)
            value = va("Player %d", n + 1);
        if (!Info_SetValueForKey(userinfo, split_userinfo[i].name, value)) {
            // invalid shadow value: never fall back to player 1's
            Info_RemoveKey(userinfo, split_userinfo[i].name);
            if (!i)
                Info_SetValueForKey(userinfo, "name", va("Player %d", n + 1));
        }
    }

    for (i = 0; i < q_countof(split_blank_keys); i++)
        Info_RemoveKey(userinfo, split_blank_keys[i]);

    if (cl_splitdebug->integer)
        CL_SplitLog(n, "userinfo on socket %d: %s\n", cla->sock, userinfo);

    return strlen(userinfo);
}

/*
====================
CL_SplitUserinfoChanged

From CL_UpdateUserinfo (a CVAR_USERINFO cvar changed, any context active).
The base cvars are player 1's: a key without shadows (password, uf, ...)
also resends every extra's userinfo. Returns true if handled here (an extra
is active: player 1's update is queued directly).
====================
*/
bool CL_SplitUserinfoChanged(cvar_t *var)
{
    client_conn_t *conn;
    int n;

    if (!(var->flags & CVAR_PRIVATE) && CL_SplitUserinfoIndex(var->name) < 0) {
        for (n = 1; n < MAX_SPLITVIEW; n++) {
            if (!clx[n])
                continue;
            conn = CL_ContextConn(clx[n]);
            if (conn->state >= ca_connected)
                conn->userinfo_modified = MAX_PACKET_USERINFOS;
        }
    }

    if (!cla->playerNum)
        return false;

    conn = CL_ContextConn(clx[0]);
    if (conn->state >= ca_connected && !(var->flags & CVAR_PRIVATE))
        conn->userinfo_modified = MAX_PACKET_USERINFOS;
    return true;
}

static const char *CL_SplitPlayerName(int n)
{
    if (!n)
        return Cvar_VariableString("name");
    return split_shadow[n][0] ? split_shadow[n][0]->string : va("Player %d", n + 1);
}

// info_hand follows the active context (gun side in the renderer, beams)
static void CL_SplitSetHand(const client_context_t *ctx)
{
    cvar_t *hand = ctx->playerNum ? split_shadow[ctx->playerNum][SPLIT_UI_HAND] : split_hand1;

    if (hand)
        info_hand = hand;
}

// the shadow of base cvar `name' for player n, NULL if there is none
static cvar_t *CL_SplitShadowVar(int n, const char *name, bool create)
{
    cvar_t *base;
    int i;

    if (n < 1)
        return NULL;

    i = CL_SplitUserinfoIndex(name);
    if (i >= 0)
        return split_shadow[n][i];

    // another per-player cvar that exists (p<N>_joy_*, a set cvar-sync shadow)
    if ((base = Cvar_FindVar(va("p%d_%s", n + 1, name))) != NULL)
        return base;

    for (i = 0; i < CVARSYNC_MAX; i++) {
        if (!Q_stricmp(name, CL_ContextCl(clx[n])->cvarsync[i].name))
            break;
    }
    if (i == CVARSYNC_MAX)
        return NULL;

    if (!create)
        return Cvar_FindVar(va("p%d_%s", n + 1, name));

    base = Cvar_FindVar(name);
    return Cvar_Get(va("p%d_%s", n + 1, name),
                    base ? base->default_string : CL_ContextCl(clx[n])->cvarsync[i].value, 0);
}

/*
====================
CL_SplitCvarSyncParsed

An extra parsed the server's cvar sync list: the server now assumes the
defaults; the next poll sends what differs.
====================
*/
void CL_SplitCvarSyncParsed(void)
{
    int i;

    for (i = 0; i < CVARSYNC_MAX; i++)
        Q_strlcpy(cla->cvarsync_sent[i], cl.cvarsync[i].value, CVARSYNC_MAXSIZE);
    cla->cvarsync_poll = 0;
}

static void CL_SplitCheckCvarSync(void)
{
    const char *value;
    cvar_t *var;
    int i, n = cla->playerNum;

    if (cls.state < ca_connected || cls.realtime < cla->cvarsync_poll)
        return;
    cla->cvarsync_poll = cls.realtime + CVARSYNC_POLL_MSEC;

    for (i = 0; i < CVARSYNC_MAX; i++) {
        if (!cl.cvarsync[i].name[0])
            continue;
        var = Cvar_FindVar(va("p%d_%s", n + 1, cl.cvarsync[i].name));
        if (!var)
            var = Cvar_FindVar(cl.cvarsync[i].name);
        value = var ? var->string : cl.cvarsync[i].value;
        if (!strncmp(value, cla->cvarsync_sent[i], CVARSYNC_MAXSIZE - 1))
            continue;
        if (!CL_WriteCvarSync(i, value))
            return;
        Q_strlcpy(cla->cvarsync_sent[i], value, CVARSYNC_MAXSIZE);
        if (cl_splitdebug->integer)
            CL_SplitLog(n, "cvarsync %s = \"%s\" (from %s)\n",
                        cl.cvarsync[i].name, cla->cvarsync_sent[i], var ? var->name : "default");
    }
}

/*
====================
CL_SplitRefused

An OOB print from the server while an extra is challenging / connecting.
"Bad challenge." (another join overwrote this IP's challenge) retries,
anything else drops the player with the server's text.
====================
*/
void CL_SplitRefused(const char *text)
{
    char reason[MAX_STRING_CHARS], *p;
    int n = cla->playerNum;

    Q_strlcpy(reason, text, sizeof(reason));
    if ((p = strchr(reason, '\n')) != NULL)
        *p = 0;

    if ((!strcmp(reason, "Bad challenge.") || !strcmp(reason, "No challenge for address.")) &&
        ++cla->refusals <= SPLIT_MAX_REFUSALS) {
        CL_SplitLog(n, "%s Retrying.\n", reason);
        cls.state = ca_challenging;
        return;
    }

    CL_SplitLog(n, "refused: %s\n", reason);

    // the server's per-address cap (sv_iplimit, Q3 R11 wording): it does not
    // say its number, so K is what got in from here
    if (strstr(reason, "Too many connections from your IP")) {
        int in = CL_PlayersIn();
        Q_snprintf(reason, sizeof(reason), "can't join: this server allows only %d player%s "
                   "from one connection", in, in == 1 ? "" : "s");
    } else {
        char text[MAX_STRING_CHARS];

        Q_strlcpy(text, reason, sizeof(text));
        Q_snprintf(reason, sizeof(reason), "can't join: %s", text);
    }

    Com_Printf("Player %d %s\n", n + 1, reason);
    cls.state = ca_disconnected;
    CL_SplitDropNotice(n, reason);
}

// copy what player 1 registered for this level (identical configstrings)
static void CL_SplitCopyRegistration(client_state_t *dst, const client_state_t *src)
{
    memcpy(dst->model_draw, src->model_draw, sizeof(dst->model_draw));
    memcpy(dst->image_precache, src->image_precache, sizeof(dst->image_precache));
    memcpy(dst->sound_precache, src->sound_precache, sizeof(dst->sound_precache));
    memcpy(dst->clientinfo, src->clientinfo, sizeof(dst->clientinfo));
    dst->baseclientinfo = src->baseclientinfo;
}

/*
====================
CL_SplitCheckPrecache

Extras never download or register anything: once player 1 has loaded the
same level (ca_precached, same servercount), copy its handles, load the BSP
(shared and ref-counted by BSP_Load) and send `begin'.
====================
*/
static void CL_SplitCheckPrecache(void)
{
    client_context_t *p1 = clx[0];

    if (!cla->precachePending)
        return;

    if (cls.state != ca_connected) {
        cla->precachePending = false;
        return;
    }

    if (CL_ContextConn(p1)->state < ca_precached)
        return;
    if (CL_ContextCl(p1)->servercount != cl.servercount)
        return;

    cla->precachePending = false;

    CL_RegisterBspModels();
    CL_SplitCopyRegistration(&cl, CL_ContextCl(p1));

    CL_SplitLog(cla->playerNum, "precache: copied player 1's registration, begin %d\n",
                cla->precache_spawncount);

    CL_Begin();
}

/*
====================
CL_SplitRegistrationChanged

The active context (normally player 1, after vid_restart / snd_restart /
a level load) has re-registered: every other context playing the same
level gets the new handles. Contexts still waiting in `precache' copy on
their own later.
====================
*/
void CL_SplitRegistrationChanged(void)
{
    client_context_t *ctx;
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++) {
        ctx = clx[i];
        if (!ctx || ctx == cla || ctx->precachePending)
            continue;
        if (CL_ContextConn(ctx)->state < ca_precached)
            continue;
        if (CL_ContextCl(ctx)->servercount != cl.servercount)
            continue;
        CL_SplitCopyRegistration(CL_ContextCl(ctx), &cl);
        CL_SplitLog(i, "re-copied player %d's registration\n", cla->playerNum + 1);
    }
}

void CL_SplitPrecache(void)
{
    cla->precache_spawncount = Q_atoi(Cmd_Argv(1));

    CL_RegisterVWepModels();

    cla->precachePending = true;
    if (CL_ContextConn(clx[0])->state < ca_precached)
        CL_SplitLog(cla->playerNum, "precache: waiting for player 1's level load\n");

    CL_SplitCheckPrecache();
}

void CL_SplitActive(void)
{
    cla->refusals = 0;
    CL_SplitLog(cla->playerNum, "active (client %d)\n", cl.clientNum);
}

/*
====================
CL_SplitRememberExtras

Player 1 is being disconnected (not a reconnect). If a remote server
dropped it (server quit / restart, timeout, kick: an error, not player 1's
own `disconnect'), remember who played with it; when player 1 is back in a
game on that server (`reconnect') they rejoin, serialized as usual, with
their profiles. Anything else forgets them.
====================
*/
static void CL_SplitRememberExtras(error_type_t type)
{
    char why[MAXERRORMSG];
    size_t len;
    int n, count = 0;

    split_rejoin.valid = false;

    if (type == ERR_FATAL || !split_p1error[0] || !strcmp(split_p1error, "Disconnected from server"))
        return;
    if (cls.state < ca_connected || cls.demo.playback || cls.serverAddress.type == NA_LOOPBACK)
        return;

    memset(&split_rejoin, 0, sizeof(split_rejoin));
    for (n = 1; n < MAX_SPLITVIEW; n++) {
        if (!clx[n] || CL_ContextConn(clx[n])->state < ca_connected)
            continue;
        split_rejoin.slot[n] = true;
        Q_strlcpy(split_rejoin.profile[n], CL_ProfileName(n), sizeof(split_rejoin.profile[n]));
        count++;
    }
    if (!count)
        return;

    split_rejoin.valid = true;
    split_rejoin.adr = cls.serverAddress;
    split_rejoin.time = cls.realtime;
    // "Server connection was reset." + "3 local players rejoin ...": one
    // sentence each, whatever punctuation the error text ends with
    Q_strlcpy(why, split_p1error, sizeof(why));
    len = strlen(why);
    while (len > 0 && (why[len - 1] == '.' || why[len - 1] == ';' || Q_isspace(why[len - 1])))
        why[--len] = 0;
    Com_Printf("splitscreen: %s. %d local player%s when player 1 is back on %s (reconnect).\n",
               why[0] ? why : "Disconnected", count, count == 1 ? " rejoins" : "s rejoin",
               NET_AdrToString(&split_rejoin.adr));
}

// player 1 active (CL_SplitFrame): bring the remembered extras back
static void CL_SplitCheckRejoin(void)
{
    char why[MAX_QPATH * 2];
    int n, slot;

    if (!split_rejoin.valid || cls.state != ca_active)
        return;
    split_rejoin.valid = false;

    if (cls.demo.playback || !NET_IsEqualAdr(&cls.serverAddress, &split_rejoin.adr) ||
        cls.realtime - split_rejoin.time > SPLIT_REJOIN_TIME)
        return;

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        if (!split_rejoin.slot[n])
            continue;
        slot = CL_SplitAddPlayer(why, sizeof(why));
        if (slot < 0) {
            Com_Printf("splitscreen: player %d can't rejoin: %s\n", n + 1, why);
            break;
        }
        Com_Printf("splitscreen: player %d rejoins as player %d%s%s\n", n + 1, slot + 1,
                   split_rejoin.profile[n][0] ? ", profile " : "", split_rejoin.profile[n]);
        if (split_rejoin.profile[n][0] && !CL_ProfileLoad(slot, split_rejoin.profile[n], why, sizeof(why)))
            Com_Printf("splitscreen: %s; player %d is a guest\n", why, slot + 1);
    }
}

/*
====================
CL_SplitDisconnect

From the top of CL_Disconnect. For an extra: send `disconnect' on its own
netchan and clear its own state, none of the shared side effects (returns
true). For player 1: drop every extra first unless this is a reconnect
(svc_reconnect / `reconnect': the extras follow on their own connection).
====================
*/
bool CL_SplitDisconnect(error_type_t type)
{
    if (!cla->playerNum) {
        if (type != ERR_RECONNECT) {
            CL_SplitRememberExtras(type);
            CL_SplitDropAll(type == ERR_FATAL ? "quit" : "player 1 disconnected");
            CL_SplitClearNotices();
        }
        split_p1error[0] = 0;
        return false;
    }

    cla->precachePending = false;

    if (!cls.state)
        return true;

    cls.passive = false;
#if USE_ICMP
    cls.errorReceived = false;
#endif

    if (cls.netchan.protocol) {
        MSG_WriteByte(clc_stringcmd);
        MSG_WriteData("disconnect", 11);
        Netchan_Transmit(&cls.netchan, msg_write.cursize, msg_write.data, 3);
        SZ_Clear(&msg_write);
        Netchan_Close(&cls.netchan);
    }

    CL_ClearState();

    cls.state = ca_disconnected;
    cls.userinfo_modified = 0;
    return true;
}

// CL_ClearState for an extra: its own cl, entities, effects, screen state
void CL_SplitClearState(void)
{
    SCR_ClearCenterPrints();
    CL_ClearEffects();
    CL_ClearTEnts();
#ifdef AQTION_EXTENSION
    CL_Clear3DGhudQueue();
#endif

    BSP_Free(cl.bsp);
    memset(&cl, 0, sizeof(cl));
    memset(&cl_entities, 0, sizeof(cl_entities));

    if (cls.state > ca_connected)
        cls.state = ca_connected;
}

void CL_SplitPrint(int level, const char *s)
{
    char *p;

    // a kick / drop comes as "<name> was kicked" right before svc_disconnect
    if (level == PRINT_HIGH) {
        Q_strlcpy(cla->lastPrint, s, sizeof(cla->lastPrint));
        if ((p = strchr(cla->lastPrint, '\n')) != NULL)
            *p = 0;
        cla->lastPrintTime = cls.realtime;
    }

    if (level == PRINT_CHAT) {
        if (!CL_CheckForIgnore(s))
            SCR_AddToChatHUD(s);
    } else if (cl.csr.extended && (level == PRINT_TYPEWRITER || level == PRINT_CENTER)) {
        SCR_CenterPrint(s, level == PRINT_TYPEWRITER);
    } else {
        SCR_AddToChatHUD(s);
    }

    if (cl_splitdebug->integer)
        Com_Printf("[P%d] %s%s", cla->playerNum + 1, s, *s && s[strlen(s) - 1] == '\n' ? "" : "\n");
}

/*
===============================================================================

STUFFTEXT FROM AN EXTRA'S SERVER (design 8, M3)

Player 1's stufftext runs as upstream. An extra's server may change only
that player: CL_SplitStufftext is the one rule, called by exec_server_string
(after `changing', `precache', `reconnect' and `cmd', which are per
connection already) with the line tokenized and macro-expanded.

===============================================================================
*/

// client commands (with a function) that act on the active player only
static const char *const split_stuff_allowed[] = {
    "cmd",          // forwards on this player's netchan
    "play",         // a local sound
    "wait",         // this player's stufftext buffer
    "echo",
    NULL
};

static bool CL_SplitInList(const char *const *list, const char *s)
{
    for (; *list; list++)
        if (!Q_stricmp(*list, s))
            return true;
    return false;
}

static void CL_SplitStuffNote(const char *text)
{
    char line[41];

    if (cla->stuffNoteTime && cls.realtime - cla->stuffNoteTime < 1000)
        return;
    cla->stuffNoteTime = cls.realtime | 1;
    Q_strlcpy(line, text, sizeof(line));
    Com_Printf("split: P%d ignored stufftext \"%s\"\n", cla->playerNum + 1, Com_MakePrintable(line));
}

static bool CL_SplitEnginePrefix(const char *name)
{
    static const char *const prefixes[] = {
        "cl_", "r_", "gl_", "s_", "vid_", "fs_", "net_", "sv_", "sys_", "in_", "m_",
        "joy_", "pad_", "con_", "scr_", "ui_", "com_", "rcon", "bot_", "p1_", "p2_",
        "p3_", "p4_", "p5_", "p6_", "p7_", "p8_", NULL
    };
    const char *const *p;

    for (p = prefixes; *p; p++)
        if (!Q_stricmpn(name, *p, strlen(*p)))
            return true;
    return false;
}

static bool CL_SplitStuffVarKnown(const char *name)
{
    int i;

    for (i = 0; i < cla->numStuffVars && i < q_countof(cla->stuffVars); i++)
        if (!strcmp(cla->stuffVars[i], name))
            return true;
    return false;
}

/*
====================
CL_SplitStuffCvar

A cvar write from an extra's server: `set name value', `seta ...', or the
bare `name value'. Returns true if it was applied (or is a harmless query).
- a userinfo / cvar-sync / other p<N>_ cvar of this player: its shadow
  (`set name X', `set cl_xerp 0', `set p2_cl_xerp 0' from P2's server);
- a cvar that does not exist yet, and then the ones this server created:
  sv_force_reconnect's random names (`set <junk> set', `$<junk> ...',
  server/user.c stuff_junk) need it, and a new cvar changes nothing the
  engine reads;
- anything else (an existing engine or player 1 cvar: cl_*, r_*, s_*, vid_*,
  fs_*, net_*, gl_*, ...) is ignored.
====================
*/
static bool CL_SplitStuffCvar(const char *name, int valueArg)
{
    int n = cla->playerNum;
    cvar_t *var;

    if (!*name)
        return false;

    if (Cmd_Argc() <= valueArg)
        return true;    // a query only prints

    // another player's shadow is not this server's business
    if (CL_SplitShadowSlot(name) >= 0 && CL_SplitShadowSlot(name) != n)
        return false;

    if (CL_SplitShadowSlot(name) == n) {
        var = Cvar_FindVar(name);
        if (!var)
            var = CL_SplitShadowVar(n, name + 3, true);   // p2_cl_xerp: a cvar-sync shadow
    } else {
        var = CL_SplitShadowVar(n, name, true);
    }
    if (var) {
        Cvar_SetByVar(var, Cmd_ArgsFrom(valueArg), FROM_STUFFTEXT);
        return true;
    }

    if (Cvar_FindVar(name) && !CL_SplitStuffVarKnown(name))
        return false;

    // nor an engine-style name that is simply not created yet (a renderer
    // or game cvar registered later would pick the value up)
    if (!CL_SplitStuffVarKnown(name) && CL_SplitEnginePrefix(name))
        return false;

    if (!CL_SplitStuffVarKnown(name)) {
        Q_strlcpy(cla->stuffVars[cla->numStuffVars % q_countof(cla->stuffVars)], name,
                  sizeof(cla->stuffVars[0]));
        cla->numStuffVars++;
    }
    Cvar_SetEx(name, Cmd_ArgsFrom(valueArg), FROM_STUFFTEXT);    // never archived (`seta' too)
    return true;
}

/*
====================
CL_SplitStuffConnect

`connect <address>' from an extra's server: sv_force_reconnect sends every
new client back to the server's own address. The same server: this player
alone reconnects on its own netchan (serialized like a join). Elsewhere: the
player leaves (it plays only where player 1 plays).
====================
*/
static void CL_SplitStuffConnect(void)
{
    netadr_t adr;
    int n = cla->playerNum;

    if (Cmd_Argc() < 2 || !NET_StringToAdr(Cmd_Argv(1), &adr, PORT_SERVER)) {
        CL_SplitDropNotice(n, "the server sent this player to a bad address");
        return;
    }
    if (!NET_IsEqualAdr(&adr, &cls.serverAddress)) {
        CL_SplitDropNotice(n, va("the server sent this player to %s", NET_AdrToString(&adr)));
        return;
    }

    CL_SplitLog(n, "server asks to reconnect (%s)\n", NET_AdrToString(&adr));

    cla->serverDropped = false;
    CL_Disconnect(ERR_RECONNECT);   // extra: its own netchan and state
    cls.serverAddress = adr;
    cls.passive = false;
    cls.state = ca_challenging;
    cls.connect_time = cls.realtime - CONNECT_DELAY;
    cls.connect_count = 0;
    cla->refusals = 0;
}

bool CL_SplitStufftext(const char *text)
{
    const char *s = Cmd_Argv(0);
    int n = cla->playerNum;
    xcommand_t func;

    if (!n)
        return true;

    // per connection: this player's own leave / reconnect
    if (!Q_stricmp(s, "disconnect")) {
        CL_SplitDropNotice(n, "disconnected by the server");
        return false;
    }
    if (!Q_stricmp(s, "connect")) {
        CL_SplitStuffConnect();
        return false;
    }

    // cvar writes
    if (!Q_stricmp(s, "set") || !Q_stricmp(s, "seta") || !Q_stricmp(s, "setu") ||
        !Q_stricmp(s, "sets")) {
        if (Cmd_Argc() < 2 || !CL_SplitStuffCvar(Cmd_Argv(1), 2))
            CL_SplitStuffNote(text);
        return false;
    }

    // `exec' with nothing to execute only prints its usage
    if (!Q_stricmp(s, "exec"))
        goto check_args;

    if (Cmd_Exists(s)) {
        func = Cmd_FindFunction(s);
        if (!func)
            return true;    // a forwarded command (inven, kill, ...): this player's server
        if (CL_SplitInList(split_stuff_allowed, s))
            return true;
        CL_SplitStuffNote(text);    // bind, alias, quit, map, kick, rcon, writeconfig, +attack, ...
        return false;
    }

    // user aliases are player 1's
    if (Cmd_AliasCommand(s)) {
        CL_SplitStuffNote(text);
        return false;
    }

    // bare `<cvar> <value>'
    if (Cvar_FindVar(s) || CL_SplitShadowVar(n, s, false)) {
        if (!CL_SplitStuffCvar(s, 1))
            CL_SplitStuffNote(text);
        return false;
    }

    // anything unknown goes to this player's server (%cpsi, %!fc, Action's
    // own commands): exactly what player 1's client does with it
    return true;

check_args:
    if (Cmd_Argc() < 2)
        return true;
    CL_SplitStuffNote(text);
    return false;
}

/*
====================
CL_SplitServerDisconnect

svc_disconnect on an extra's connection. The reason is the server's last
print if it came with it ("Player 2 was kicked"). With stufftext still to
run (sv_force_reconnect sends `connect' right before dropping) the netchan
closes now and the drop waits for that text (CL_SplitEventsFn); otherwise
the player drops at once (Com_Error, trapped by its guard).
====================
*/
bool CL_SplitServerDisconnect(void)
{
    char reason[MAX_QPATH * 2];

    if (cla->lastPrint[0] && cls.realtime - cla->lastPrintTime < 1000)
        Q_strlcpy(reason, cla->lastPrint, sizeof(reason));
    else
        Q_strlcpy(reason, "Server disconnected", sizeof(reason));

    if (!cl_cmdbuf.cursize)
        Com_Error(ERR_DISCONNECT, "%s", reason);

    CL_SplitLog(cla->playerNum, "server dropped this player (%s), running its last stufftext\n", reason);
    Q_strlcpy(cla->serverDropReason, reason, sizeof(cla->serverDropReason));
    cla->serverDropped = true;
    Netchan_Close(&cls.netchan);
    cls.state = ca_disconnected;
    return true;
}

/*
====================
CL_SplitStuffTest_f

split_stufftest <N> <text>: feed text to player N's stufftext buffer as if
its server had sent it (`set developer 1` only, also in a release build,
which has no developer command; tests the rule above).
====================
*/
static void CL_SplitStuffTest_f(void)
{
    int n;

    if (!Cvar_VariableInteger("developer")) {
        Com_Printf("%s: needs \"set developer 1\"\n", Cmd_Argv(0));
        return;
    }
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: %s <player number 2..%d> <text>\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    n = Q_atoi(Cmd_Argv(1)) - 1;
    if (n < 1 || n >= MAX_SPLITVIEW || !clx[n]) {
        Com_Printf("%s: no local player %s\n", Cmd_Argv(0), Cmd_Argv(1));
        return;
    }
    Cbuf_AddText(&clx[n]->cmdbuf, Cmd_ArgsFrom(2));
    Cbuf_AddText(&clx[n]->cmdbuf, "\n");
}

#if USE_ICMP
void CL_SplitErrorEvent(netsrc_t sock, const netadr_t *from)
{
    int n;

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        if (!clx[n] || clx[n]->sock != sock)
            continue;
        if (clx[n] == cla) {
            CL_ErrorEvent(from);
        } else if (cl_contextdepth < MAX_CONTEXT_STACK) {
            CL_PushContext(n);
            CL_ErrorEvent(from);
            CL_PopContext();
        }
        return;
    }
}
#endif

/*
===============================================================================

FRAME

===============================================================================
*/

bool CL_SplitSendPacketNow(void)
{
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++)
        if (clx[i] && CL_ContextCl(clx[i])->sendPacketNow)
            return true;
    return false;
}

static void CL_SplitEventsFn(void *arg)
{
    CL_ContextEvents();

    // the server dropped this player and its last stufftext has run without
    // a `connect' (sv_force_reconnect) taking it back: drop with the reason
    if (cla->serverDropped && !cl_cmdbuf.cursize) {
        cla->serverDropped = false;
        Com_Error(ERR_DISCONNECT, "%s", cla->serverDropReason);
    }
}

void CL_SplitProcessEvents(void)
{
    int n;

    if (cl_contextdepth || cla != clx[0])
        return;

    for (n = 1; n < MAX_SPLITVIEW; n++)
        if (clx[n] && !clx[n]->dropPending)
            CL_SplitRunGuarded(n, CL_SplitEventsFn, NULL);

    CL_SplitRunDrops();
}

typedef struct {
    int     msec;
    bool    phys_frame;
} split_frame_t;

#define SPLIT_P1_SILENT_MSEC    1000

// player 1 has a netchan to a remote server and heard nothing for a second
static bool CL_SplitP1Silent(void)
{
    const client_conn_t *p1 = CL_ContextConn(clx[0]);

    if (!p1->netchan.protocol || NET_IsLocalAddress(&p1->netchan.remote_address))
        return false;
    return com_localTime - p1->netchan.last_received > SPLIT_P1_SILENT_MSEC;
}

static void CL_SplitFrameFn(void *arg)
{
    const split_frame_t *f = arg;

    // reopen the socket if NET_Config(NET_NONE) closed it (net_restart)
    if (cls.state >= ca_challenging)
        NET_OpenSplitSocket(cla->sock, cls.serverAddress.type == NA_IP6);

    CL_SplitCheckPrecache();
    CL_SplitCheckCvarSync();

    // the server gone silent for player 1 too (it quit or crashed): player
    // 1's own timeout decides, so the extras go with it and can rejoin
    // (CL_SplitRememberExtras) instead of timing out one by one first
    if (CL_SplitP1Silent()) {
        cl.timeoutcount = 0;
#if USE_ICMP
        cls.errorReceived = false;
#endif
    }

    CL_ContextFrame(f->msec, f->phys_frame);

    if (split_debugerror == cla->playerNum) {
        split_debugerror = 0;
        Com_Error(ERR_DROP, "splitdebug: simulated error for player %d", cla->playerNum + 1);
    }
}

// an extra stays only while it plays on player 1's server
static bool CL_SplitFollowsP1(const client_context_t *ctx)
{
    const netadr_t *adr = &ctx->conn.serverAddress;
    netadr_t local;

    if (cls.state < ca_challenging || cls.demo.playback)
        return false;

    if (cls.serverAddress.type == NA_LOOPBACK)
        return NET_GetAddress(NS_SERVER, &local) && adr->type == local.type && adr->port == local.port;

    return NET_IsEqualAdr(adr, &cls.serverAddress);
}

void CL_SplitFrame(int msec, bool phys_frame)
{
    split_frame_t f = { msec, phys_frame };
    int n;

    if (cl_contextdepth || cla != clx[0])
        return;

    CL_ProfileFrame();
    CL_SplitCheckRejoin();
    CL_SplitMenuFrame();

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        if (!clx[n] || clx[n]->dropPending)
            continue;
        if (!CL_SplitFollowsP1(clx[n])) {
            CL_SplitDropPlayer(n, cls.state < ca_challenging ? "player 1 disconnected" :
                               "player 1 left the server");
            continue;
        }
        CL_SplitRunGuarded(n, CL_SplitFrameFn, &f);
    }

    CL_SplitRunDrops();
}

/*
===============================================================================

INIT

===============================================================================
*/

static const cmdreg_t c_split[] = {
    { "p1", CL_SplitPlayerCmd_f },
    { "p2", CL_SplitPlayerCmd_f },
    { "p3", CL_SplitPlayerCmd_f },
    { "p4", CL_SplitPlayerCmd_f },
    { "p5", CL_SplitPlayerCmd_f },
    { "p6", CL_SplitPlayerCmd_f },
    { "p7", CL_SplitPlayerCmd_f },
    { "p8", CL_SplitPlayerCmd_f },
    { "addplayer", CL_AddPlayer_f },
    { "dropplayer", CL_DropPlayer_f },
    { "splitplayers", CL_SplitPlayers_f },
    { "splitdebug", CL_SplitDebug_f },
    { "split_stufftest", CL_SplitStuffTest_f },
    { NULL }
};

/*
====================
CL_InitContexts

Called first thing in CL_Init.
====================
*/
void CL_InitContexts(void)
{
    client_context_t *ctx = &cl_context0;

    Q_assert(cla == ctx && clx[0] == ctx);
    CL_InitContextDefaults(ctx, 0);

    Com_Printf("client context: %zu bytes (cl %zu, entities %zu, fx %zu, "
               "scene %zu, scr %zu, in %zu, conn %zu)\n",
               sizeof(*ctx), sizeof(cl), sizeof(ctx->entities),
               sizeof(ctx->fx), sizeof(ctx->scene), sizeof(ctx->scr),
               sizeof(ctx->in), sizeof(ctx->conn));

    cl_splitdebug = Cvar_Get("cl_splitdebug", "0", 0);
    cl_splitMaxPlayers = Cvar_Get("cl_splitMaxPlayers", "8", CVAR_ARCHIVE);
    S_SplitInit();
    CL_SplitInitView();
    CL_SplitLayout(true);
    CL_SplitMenuInit();

    Cmd_Register(c_split);
}
