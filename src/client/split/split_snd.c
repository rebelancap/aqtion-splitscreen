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

// split_snd.c -- splitscreen sound: one mixer, nearest listener (design 6)
//
// Every local player's connection hears the same server events, so each
// S_StartSound is tagged with the issuing context and a copy of a sound
// another context already started a moment ago is dropped (dedupe). Sounds
// of any local player's own entity play at full volume. Moving sound
// origins resolve through the issuing context's entities when player 1's
// frame does not have the entity.
//
// M2-T6 (Q3 design 7): CL_CalcViewValues gives every extra's view to
// S_SplitSetListener; on the DMA backend each channel is spatialized against
// the nearest of player 1's and those listeners, with stereo pan only for 2
// players (mono, centred, from 3; s_splitPan overrides), and loop sounds are
// merged from every player's view by entity number. OpenAL has one
// listener: player 1's, as in M1 (dedupe and full volume still apply).
//
// Everything here is skipped while no extra is in the game
// (s_split_numents == 0), so one player is unchanged.

#include "../sound/sound.h"

int             s_split_numents;                // extras' entity numbers below
static int      s_split_entnums[MAX_SPLITVIEW];

static cvar_t   *s_splitdebug;
static cvar_t   *s_splitPan;

// extras' views from CL_CalcViewValues, and the ones current this frame
static struct {
    vec3_t      origin;
    vec3_t      right;
    unsigned    time;           // Sys_Milliseconds when set, 0 = never
} s_split_views[MAX_SPLITVIEW];

#define LISTENER_STALE_MSEC 250

s_split_listener_t  s_split_listeners[MAX_SPLITVIEW];
int                 s_split_numlisteners;

static s_split_loop_t   s_split_loops[MAX_EDICTS];
static byte             s_split_loopmark[MAX_EDICTS / 8];
static int              s_split_lastExtraLoops = -1;
static int              s_split_lastLoops = -1;

// same event from another connection: within this window
#define DEDUPE_ENTITY_MSEC  40
#define DEDUPE_LOCAL_MSEC   50
#define DEDUPE_RING         64

typedef struct {
    const void  *sfx;
    int         entnum;
    int         entchannel;
    int         context;
    bool        used;           // the zeroed ring must never match
    bool        fixed_origin;
    vec3_t      origin;
    unsigned    time;
} recent_sound_t;

static recent_sound_t   s_recent[DEDUPE_RING];
static int              s_recent_head;

static int      s_frame_started, s_frame_dropped;
static unsigned s_total_dropped;

#pragma push_macro("cl")
#undef cl
static client_state_t *S_ContextCl(client_context_t *ctx)
{
    return &ctx->cl;
}
#pragma pop_macro("cl")

/*
Humor sounds (0.0.0.27). AQtion's pak0.pkz has 78 joke clips in sound/user/
(farts, laughs, "newbie", film quotes; baseaq\sndlist.ini) and replaced
Quake 2's sound/insane/insane1-11.wav with more of them (insane2 = killme,
3 = lol, 5 = newbie, 7 = fart4, ...). The game plays them on a player's voice
channel: insane/ when a living player has been idle ppl_idletime seconds
(15, then every 15 s while idle; Steam's TNG 2.82 and our src/action), user/
through the `voice <file>' command and, in our src/action only, when a bot
frags (bot_randvoice %). With cl_splitHumorSounds 0 no local player's
connection starts either (svc_sound and the `play' command), with any game
library and on other machines' servers; the Server options row also sets
the game's own cvars when hosting (split_menu.c SM_ApplyHumor).
*/
static cvar_t   *cl_splitHumorSounds;
static cvar_t   *s_split_developer;     // `developer' (declared only in debug builds)

static bool S_SplitIsHumorSound(const char *name)
{
    if (*name == '/' || *name == '\\')
        name++;
    if (!Q_strncasecmp(name, "sound/", 6))
        name += 6;
    return !Q_strncasecmp(name, "user/", 5) || !Q_strncasecmp(name, "insane/", 7);
}

bool CL_SplitSoundAllowed(const char *name, int entnum)
{
    bool allow;

    if (!name || !*name || !S_SplitIsHumorSound(name))
        return true;
    allow = !cl_splitHumorSounds || cl_splitHumorSounds->integer;
    if (s_split_developer && s_split_developer->integer)
        Com_LPrintf(PRINT_DEVELOPER, "humor sound %s (entity %d, player %d): %s\n",
                    name, entnum, cla->playerNum + 1, allow ? "played" : "blocked");
    return allow;
}

void S_SplitInit(void)
{
    cl_splitHumorSounds = Cvar_Get("cl_splitHumorSounds", "1", CVAR_ARCHIVE);
    s_split_developer = Cvar_Get("developer", "0", 0);
    s_splitdebug = Cvar_Get("s_splitdebug", "0", 0);
    // -1: stereo pan with 2 players, mono from 3 (Q3 7); 0: always mono; 1: always pan
    s_splitPan = Cvar_Get("s_splitPan", "-1", 0);
}

// CL_CalcViewValues of player n (1..7)
void S_SplitSetListener(int player, const vec3_t origin, const vec3_t right)
{
    if (player < 1 || player >= MAX_SPLITVIEW)
        return;
    VectorCopy(origin, s_split_views[player].origin);
    VectorCopy(right, s_split_views[player].right);
    s_split_views[player].time = Sys_Milliseconds() | 1;
}

bool S_SplitMono(void)
{
    if (!s_split_numlisteners)
        return false;
    if (s_splitPan->integer >= 0)
        return !s_splitPan->integer;
    return s_split_numlisteners >= 2;   // 3+ players on one pair of speakers
}

/*
====================
S_SplitUpdate

From S_Update, player 1 active: collect the entity numbers of the extras in
the game on player 1's level, print the dedupe counter.
====================
*/
void S_SplitUpdate(void)
{
    client_context_t *ctx;
    client_state_t *ccl;
    s_split_listener_t *l;
    unsigned now = Sys_Milliseconds();
    int n, k = 0, numl = 0;

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        ctx = clx[n];
        if (!ctx || !ctx->inUse || ctx == cla)
            continue;
        if (ctx->conn.state != ca_active)
            continue;
        ccl = S_ContextCl(ctx);
        if (ccl->servercount != cl.servercount)
            continue;
        s_split_entnums[k++] = ccl->frame.clientNum + 1;

        // its view is a listener while it is being drawn
        if (s_split_views[n].time && now - s_split_views[n].time < LISTENER_STALE_MSEC) {
            l = &s_split_listeners[numl++];
            VectorCopy(s_split_views[n].origin, l->origin);
            VectorCopy(s_split_views[n].right, l->right);
            l->player = n;
        }
    }
    s_split_numents = k;
    s_split_numlisteners = k ? numl : 0;

    if (s_splitdebug && s_splitdebug->integer && s_frame_dropped) {
        Com_Printf("[snd %u] started %d, dropped %d duplicate%s (total %u)\n",
                   Sys_Milliseconds(), s_frame_started, s_frame_dropped,
                   s_frame_dropped == 1 ? "" : "s", s_total_dropped);
    }
    s_frame_started = s_frame_dropped = 0;
}

/*
====================
S_SplitImpactRand

tent.c's roll for a bullet impact's ricochet (none, ric1, ric2, ric3): with
extras in the game, a hash of the impact point, so every connection's copy
of the event picks the same sound (or none) and the dedupe merges them;
otherwise Q_rand() as upstream.
====================
*/
int S_SplitImpactRand(const vec3_t pos)
{
    uint32_t h = 2166136261u;
    int i, v;

    if (!s_split_numents)
        return Q_rand();
    // only the point: frame numbers and times are per connection in q2pro
    // (client->framenum), so the same spot hit again rolls the same
    for (i = 0; i < 3; i++) {
        v = (int)floorf(pos[i] * 8.0f + 0.5f);     // the wire's 1/8 unit
        h = (h ^ (uint32_t)v) * 16777619u;
    }
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (int)(h & 0x7fffffff);
}

// entnum is a local player's own entity (full volume)
bool S_SplitLocalEnt(int entnum)
{
    int i;

    for (i = 0; i < s_split_numents; i++)
        if (s_split_entnums[i] == entnum)
            return true;
    return false;
}

// two numbered variants of one sound ("world/ric1.wav", "world/ric3.wav"):
// the same name once the digits before the extension are dropped
static bool S_SplitSameFamily(const void *a, const void *b)
{
    const char *s = ((const sfx_t *)a)->name, *t = ((const sfx_t *)b)->name;
    const char *es = COM_FileExtension(s), *et = COM_FileExtension(t);

    while (es > s && Q_isdigit(es[-1]))
        es--;
    while (et > t && Q_isdigit(et[-1]))
        et--;
    if (es == COM_FileExtension(s) || et == COM_FileExtension(t))
        return false;   // not numbered
    return es - s == et - t && !Q_strncasecmp(s, t, es - s);
}

/*
====================
S_SplitDuplicate

From S_StartSound while extras are in the game. True if another context
started the same sound within the window: (entnum, entchannel) for an
entity's own channel, (sfx, entnum, entchannel[, origin]) for other world /
entity sounds (numbered world variants such as ric1-3 count as one sfx; a
copy with an explicit origin matches one without), sfx alone for local
sounds (chat beep, hit marker, menu). Kept sounds are remembered with the
issuing context; the first copy plays.
====================
*/
bool S_SplitDuplicate(const void *sfx, const vec3_t origin, int entnum, int entchannel, float attenuation)
{
    recent_sound_t *r;
    unsigned now = Sys_Milliseconds();
    bool local = attenuation == ATTN_NONE && entnum == listener_entnum;
    unsigned window = local ? DEDUPE_LOCAL_MSEC : DEDUPE_ENTITY_MSEC;
    int i, context = cla->playerNum;

    for (i = 0, r = s_recent; i < DEDUPE_RING; i++, r++) {
        if (!r->used || r->context == context || now - r->time > window)
            continue;
        if (local) {
            if (r->sfx == sfx)
                break;
            continue;
        }
        if (r->entnum != entnum || r->entchannel != entchannel)
            continue;
        // an entity's own channel holds one sound anyway (S_PickChannel), and
        // client-picked variants differ per context (footsteps, landing,
        // world impacts: ric1/2/3)
        if (r->sfx != sfx && !(entnum > 0 && entchannel) && !(!entnum && S_SplitSameFamily(r->sfx, sfx)))
            continue;
        // the server sends an entity sound with its origin to a client that
        // does not have the entity: the copies may differ in that flag, and
        // then only the entity tells them apart (acceptance M2 note 3)
        if (origin && r->fixed_origin && Distance(r->origin, origin) > 8)
            continue;
        if (!entnum && r->fixed_origin != !!origin)
            continue;
        break;
    }

    if (i < DEDUPE_RING) {
        s_frame_dropped++;
        s_total_dropped++;
        if (s_splitdebug->integer > 1)
            Com_Printf("[snd P%d] drop %s ent %d chan %d%s (P%d had it %u ms ago%s)\n",
                       context + 1, ((const sfx_t *)sfx)->name, entnum, entchannel,
                       (origin && entnum > 0) ? " sent with its origin" : "",
                       r->context + 1, now - r->time,
                       r->sfx != sfx ? va(": %s", ((const sfx_t *)r->sfx)->name) : "");
        return true;
    }

    r = &s_recent[s_recent_head++ & (DEDUPE_RING - 1)];
    r->sfx = sfx;
    r->entnum = entnum;
    r->entchannel = entchannel;
    r->context = context;
    r->used = true;
    r->fixed_origin = !!origin;
    if (origin)
        VectorCopy(origin, r->origin);
    r->time = now;

    s_frame_started++;
    if (s_splitdebug->integer > 1) {
        vec3_t org;
        bool full = entnum == -1 || attenuation == ATTN_NONE ||
                    entnum == listener_entnum || S_SplitLocalEnt(entnum);

        if (origin)
            VectorCopy(origin, org);
        else
            S_SplitEntityOrigin(context, entnum, org);
        Com_Printf("[snd P%d] play %s ent %d chan %d%s %s, %d units from player 1\n",
                   context + 1, ((const sfx_t *)sfx)->name, entnum, entchannel,
                   (origin && entnum > 0) ? " (sent with its origin)" : "",
                   full ? "FULL VOLUME" : "spatialized", (int)Distance(org, listener_origin));
    }
    return false;
}

// entity origin as seen by context c, false if c does not have it this frame
static bool S_ContextEntityOrigin(int c, int entnum, vec3_t org)
{
    client_context_t *ctx = clx[c];
    client_state_t *ccl;
    const centity_t *ent;
    const mmodel_t *mod;
    vec3_t mid;

    if (!ctx || !ctx->inUse)
        return false;

    if (ctx == cla) {
        if (entnum == listener_entnum ||
            (entnum < cl.csr.max_edicts && cl_entities[entnum].serverframe == cl.frame.number)) {
            CL_GetEntitySoundOrigin(entnum, org);
            return true;
        }
        return false;
    }

    if (ctx->conn.state != ca_active)
        return false;

    ccl = S_ContextCl(ctx);
    if (entnum == ccl->frame.clientNum + 1) {
        // that player's own sounds: from its view
        VectorCopy(ccl->refdef.vieworg, org);
        return true;
    }

    if (entnum >= ccl->csr.max_edicts)
        return false;
    ent = &ctx->entities[entnum];
    if (ent->serverframe != ccl->frame.number)
        return false;

    LerpVector(ent->prev.origin, ent->current.origin, ccl->lerpfrac, org);
    if (ent->current.solid == PACKED_BSP) {
        mod = ccl->model_clip[ent->current.modelindex];
        if (mod) {
            VectorAvg(mod->mins, mod->maxs, mid);
            VectorAdd(org, mid, org);
        }
    }
    return true;
}

/*
====================
S_SplitEntityOrigin

Spatialization origin of a moving sound while extras are in the game: from
the issuing context's entities, else player 1's, else any context that has
the entity in its current frame; player 1's (possibly stale) as the last
resort, as without splitscreen.
====================
*/
void S_SplitEntityOrigin(int context, int entnum, vec3_t org)
{
    int i;

    if (entnum <= 0) {
        CL_GetEntitySoundOrigin(entnum, org);
        return;
    }

    if (context > 0 && context < MAX_SPLITVIEW && S_ContextEntityOrigin(context, entnum, org))
        return;
    if (S_ContextEntityOrigin(0, entnum, org))
        return;
    for (i = 1; i < MAX_SPLITVIEW; i++)
        if (i != context && S_ContextEntityOrigin(i, entnum, org))
            return;

    CL_GetEntitySoundOrigin(entnum, org);
}

/*
====================
S_SplitLogChannel

s_splitdebug 2: a channel starting on the DMA backend and the listener it
is heard from (0 = player 1, -1 = full volume).
====================
*/
void S_SplitLogChannel(const channel_t *ch, int player, const vec3_t origin)
{
    const s_split_listener_t *l;
    int i, dist = -1;

    if (!s_splitdebug->integer || s_splitdebug->integer < 2 || !ch->sfx)
        return;

    if (player < 0) {
        Com_Printf("[snd] start %s ent %d: full volume (a local player's own / no attenuation)\n",
                   ch->sfx->name, ch->entnum);
        return;
    }
    for (i = 0, l = s_split_listeners; i < s_split_numlisteners; i++, l++)
        if (l->player == player)
            dist = (int)Distance(origin, l->origin);
    if (!player)
        dist = (int)Distance(origin, listener_origin);
    Com_Printf("[snd] start %s ent %d: nearest listener P%d (%d units; P1 %d units), "
               "vol L %.2f R %.2f, %s, %d listener(s)\n",
               ch->sfx->name, ch->entnum, player + 1, dist, (int)Distance(origin, listener_origin),
               ch->leftvol, ch->rightvol, S_SplitMono() ? "mono" : "stereo", s_split_numlisteners + 1);
}

static bool S_SplitLoopAllowed(const centity_state_t *ent)
{
    if (s_ambient->integer == 2 && !ent->modelindex)
        return false;
    if (s_ambient->integer == 3 && !S_IsListenerEnt(ent->number))
        return false;
    return ent->sound != 0;
}

/*
====================
S_SplitLoopSounds

DMA AddLoopSounds while extras are in the game: player 1's loop sounds, then
those only another player's frame has (merged by entity number), with
their origins from that player's entities. Rebuilt every frame; the caller
may clear `sound' of entries it merged.
====================
*/
s_split_loop_t *S_SplitLoopSounds(int *count)
{
    client_context_t *ctx;
    client_state_t *ccl;
    const centity_state_t *ent;
    s_split_loop_t *lp;
    int i, n, num, extra = 0;

    *count = 0;
    memset(s_split_loopmark, 0, sizeof(s_split_loopmark));

    for (i = 0; i < cl.frame.numEntities && *count < MAX_EDICTS; i++) {
        ent = &cl.entityStates[(cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK];
        if (!S_SplitLoopAllowed(ent) || ent->number >= MAX_EDICTS)
            continue;
        lp = &s_split_loops[(*count)++];
        lp->entnum = ent->number;
        lp->sound = ent->sound;
        lp->volume = S_GetEntityLoopVolume(ent);
        lp->dist_mult = S_GetEntityLoopDistMult(ent);
        CL_GetEntitySoundOrigin(ent->number, lp->origin);
        lp->player = 0;
        Q_SetBit(s_split_loopmark, ent->number);
    }

    for (n = 1; n < MAX_SPLITVIEW; n++) {
        ctx = clx[n];
        if (!ctx || !ctx->inUse || ctx == cla || ctx->conn.state != ca_active)
            continue;
        ccl = S_ContextCl(ctx);
        if (ccl->servercount != cl.servercount)
            continue;
        for (i = 0; i < ccl->frame.numEntities && *count < MAX_EDICTS; i++) {
            num = (ccl->frame.firstEntity + i) & PARSE_ENTITIES_MASK;
            ent = &ccl->entityStates[num];
            if (!S_SplitLoopAllowed(ent) || ent->number >= MAX_EDICTS)
                continue;
            if (Q_IsBitSet(s_split_loopmark, ent->number))
                continue;
            lp = &s_split_loops[*count];
            if (!S_ContextEntityOrigin(n, ent->number, lp->origin))
                continue;
            (*count)++;
            lp->entnum = ent->number;
            lp->sound = ent->sound;
            lp->volume = S_GetEntityLoopVolume(ent);
            lp->dist_mult = S_GetEntityLoopDistMult(ent);
            Q_SetBit(s_split_loopmark, ent->number);
            lp->player = n;
            extra++;
        }
    }

    if (s_splitdebug->integer && (extra != s_split_lastExtraLoops || *count != s_split_lastLoops)) {
        Com_Printf("[snd] loop sounds: %d, %d of them only in other players' views\n", *count, extra);
        for (i = *count - extra; i < *count; i++) {
            lp = &s_split_loops[i];
            Com_Printf("[snd] loop sound %s ent %d: only in P%d's view, %d units from P1\n",
                       cl.configstrings[cl.csr.sounds + lp->sound], lp->entnum, lp->player + 1,
                       (int)Distance(lp->origin, listener_origin));
        }
    }
    s_split_lastExtraLoops = extra;
    s_split_lastLoops = *count;
    return s_split_loops;
}
