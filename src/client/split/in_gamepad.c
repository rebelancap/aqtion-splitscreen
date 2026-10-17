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
// in_gamepad.c -- pad layer for the splitscreen fork (design 5.2, M2-T1).
//
// SDL2 is used for its joystick + gamecontroller subsystems ONLY: no SDL
// video, audio or event pump, so the native Win32 window and input paths
// are untouched. On Windows SDL2 is linked statically (meson option
// `gamepad`, subprojects/sdl2.wrap, built without video/audio/render); with
// -Dsdl2 (Linux) this shares the engine's SDL.
//
// Devices are polled from IN_Frame. Each device belongs to at most one local
// player slot: a pad nobody has joins by holding A (M2-T3, Q3 design 6.2
// "Join / drop flow", 14.1, 14.2, 17.1), leaves with Back+Start, and
// `padassign` overrides. Pad
// buttons, the d-pad, the triggers and the stick directions are virtual keys
// 256 + padKey with a bind table per player slot (`pbind`); a bind runs as
// that player (`p<N> +cmd <key> <time>`, so kbutton two-key tracking works
// per player). The sticks move and turn the owning context directly
// (CL_GamepadMove / CL_GamepadLook). `padinject` feeds virtual pads through
// the same paths for headless tests. Default binds (AQ2 set, M2-T2) fill
// a player slot that has none; while that player's layout (Action's menu,
// scoreboard) is up, d-pad / A / B drive it.
//
// Structure, key names, cvar names and values follow the Quake3e fork's
// code/client/in_gamepad.c (its design sections 6.2 and 14.4).

#include "../client.h"
#include "gamepad.h"

// shared.h defines these empty, SDL_config.h as 1 (MSVC / unix)
#undef HAVE__STRICMP
#undef HAVE_STRCASECMP
#undef HAVE_VSNPRINTF
#define SDL_MAIN_HANDLED
#include <SDL.h>

#define MAX_PADS        MAX_SPLITVIEW
#define PAD_KEYBASE     256             // pad key numbers in "+cmd <key> <time>"

typedef enum {
    PAD_A, PAD_B, PAD_X, PAD_Y,
    PAD_BACK, PAD_GUIDE, PAD_START,
    PAD_L3, PAD_R3, PAD_LB, PAD_RB,
    PAD_DPAD_UP, PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_DPAD_RIGHT,
    PAD_MISC1, PAD_PADDLE1, PAD_PADDLE2, PAD_PADDLE3, PAD_PADDLE4, PAD_TOUCHPAD,
    PAD_SDL_BUTTONS,    // = SDL_CONTROLLER_BUTTON_MAX, same order as SDL's
    PAD_LT = PAD_SDL_BUTTONS, PAD_RT,
    PAD_LSTICK_UP, PAD_LSTICK_DOWN, PAD_LSTICK_LEFT, PAD_LSTICK_RIGHT,
    PAD_RSTICK_UP, PAD_RSTICK_DOWN, PAD_RSTICK_LEFT, PAD_RSTICK_RIGHT,
    PAD_KEY_COUNT
} padKey_t;

static const char *const padKeyNames[PAD_KEY_COUNT] = {
    "PAD_A", "PAD_B", "PAD_X", "PAD_Y",
    "PAD_BACK", "PAD_GUIDE", "PAD_START",
    "PAD_L3", "PAD_R3", "PAD_LB", "PAD_RB",
    "PAD_DPAD_UP", "PAD_DPAD_DOWN", "PAD_DPAD_LEFT", "PAD_DPAD_RIGHT",
    "PAD_MISC1", "PAD_PADDLE1", "PAD_PADDLE2", "PAD_PADDLE3", "PAD_PADDLE4", "PAD_TOUCHPAD",
    "PAD_LT", "PAD_RT",
    "PAD_LSTICK_UP", "PAD_LSTICK_DOWN", "PAD_LSTICK_LEFT", "PAD_LSTICK_RIGHT",
    "PAD_RSTICK_UP", "PAD_RSTICK_DOWN", "PAD_RSTICK_LEFT", "PAD_RSTICK_RIGHT",
};

// other spellings accepted by pbind / padinject
static const struct {
    const char *name;
    padKey_t key;
} padKeyAliases[] = {
    { "PAD_LS", PAD_L3 },
    { "PAD_RS", PAD_R3 },
    { "PAD_SELECT", PAD_BACK },
    { "PAD_L1", PAD_LB },
    { "PAD_R1", PAD_RB },
    { "PAD_L2", PAD_LT },
    { "PAD_R2", PAD_RT },
};

/*
AQ2 default pad set (M2-T2): fills a player slot that has no pad binds when
a pad is given to it, and `padbinddefaults <player>`. Never written to the
config unless the player changed it. The maintainer's rule: the Quake3e
fork's Urban Terror layout (its in_gamepad.c padGameDefaults "q3ut4") with
the same button for the same idea, mapped to Action's commands (its
autoexec_example.cfg: `lens` = sniper zoom (plain `lens` cycles 1x 2x 4x
6x and back to 1x, `lens in` stops at 6x: TNG's Cmd_Lens_f, 0.0.0.31),
`weapon` = SPACE, weapon mode;
`reload`; `bandage`; `drop item` / `drop weapon`; `irvision`; `inven` = TAB,
the team / weapon / item menu; `score` = F1, a toggle); the buttons UrT
leaves free get AQ2 actions (opendoor, radio). The left stick moves, the
right stick looks (CL_GamepadMove / Look). UrT's equivalent on the right.
*/
static const struct {
    padKey_t    key;
    const char  *cmd;
} padDefaultBinds[] = {
    { PAD_A,            "+moveup" },            // jump              (UrT: jump)
    { PAD_B,            "+movedown" },          // crouch            (crouch)
    { PAD_X,            "reload" },             //                   (reload)
    { PAD_Y,            "bandage" },            //                   (bandage)
    { PAD_BACK,         "score" },              // scoreboard        (+scores)
    { PAD_START,        "padmenu" },            // overlay menu      (padmenu)
    { PAD_LB,           "lens 1" },             // reset zoom        (ut_zoomreset)
    { PAD_RB,           "weapnext" },           //                   (weapnext)
    { PAD_LT,           "lens" },               // zoom cycle, wraps to 1x (ut_zoomin)
    { PAD_RT,           "+attack" },            //                   (+attack)
    { PAD_L3,           "+speed" },             // walk / run        (sprint)
    { PAD_R3,           "use Combat Knife" },   // knife             (ut_weaptoggle knife)
    { PAD_DPAD_UP,      "drop item" },          //                   (ut_itemdrop)
    { PAD_DPAD_DOWN,    "drop weapon" },        //                   (ut_weapdrop)
    { PAD_DPAD_LEFT,    "irvision" },           // IR vision         (ut_itemuse nvg)
    { PAD_DPAD_RIGHT,   "weapon" },             // weapon mode       (+button3 fire mode)
    { PAD_MISC1,        "opendoor" },           // Share / capture   (ut_itemuse)
    { PAD_PADDLE1,      "radio enemys" },       // back paddles: radio (free in UrT)
    { PAD_PADDLE2,      "radio cover" },
    { PAD_PADDLE3,      "radio reportin" },
    { PAD_PADDLE4,      "radio taking_f" },
};

// while the player's layout is up (Action's menu, scoreboard, inventory)
static const struct {
    padKey_t    key;
    const char  *cmd;
} padMenuKeys[] = {
    { PAD_DPAD_UP,      "invprev" },
    { PAD_DPAD_DOWN,    "invnext" },
    { PAD_A,            "invuse" },
    { PAD_B,            "putaway" },        // what Escape does over a layout (keys.c)
};

typedef enum { PAX_LX, PAX_LY, PAX_RX, PAX_RY, PAX_LT, PAX_RT, PAX_COUNT } padAxis_t;   // SDL order
static const char *const padAxisNames[PAX_COUNT] = { "LX", "LY", "RX", "RY", "LT", "RT" };

typedef struct {
    bool        connected;
    bool        isVirtual;          // padinject
    SDL_GameController *ctrl;
    SDL_JoystickID  instanceId;     // SDL joystick instance (-1 for virtual)
    char        guid[40];
    char        key[64];            // stable identity: guid#n (n-th device with that guid)
    char        name[64];

    int         player;             // local player slot (0-based) or -1 = none
    float       axis[PAX_COUNT];    // sticks -1..1 (y down = +), triggers 0..1
    bool        down[PAD_KEY_COUNT];
    bool        sentDown[PAD_KEY_COUNT];    // a bind down went to the player: send the up
    bool        menuDown[PAD_KEY_COUNT];    // the press went to the menu layer: no up
    unsigned char uiKey[PAD_KEY_COUNT];     // engine UI key sent for the press (its up follows)
    unsigned    downTime[PAD_KEY_COUNT];    // Sys_Milliseconds of the last press
    unsigned    lastInput;          // last real input (join hint), 0 = never
    bool        joinDone;           // join hold handled / not allowed: wait for release
    unsigned    leaveStart;         // Back+Start held since (0 = not)
    bool        leaveDone;
    bool        chordPending[2];    // Back / Start pressed alone: acts on its release
    bool        repOn;              // overlay hold-repeat (Pad_OverlayRepeat) running
    int         repKey;             // the held direction pad key
    unsigned    repStart, repNext;  // Sys_Milliseconds of the press / of the next step
    uintptr_t   repView;            // CL_SplitMenuView at the press: another page stops it
} padDevice_t;

/*
Overlay hold-repeat (0.0.0.31): a d-pad / left-stick direction held on
player n's overlay menu or on-screen keyboard steps again after
PAD_REPEAT_DELAY, then every PAD_REPEAT_RATE, every PAD_REPEAT_FAST after
PAD_REPEAT_FAST_AFTER of holding. A tap (well under the delay) moves once.
*/
#define PAD_REPEAT_DELAY        400
#define PAD_REPEAT_RATE         120
#define PAD_REPEAT_FAST_AFTER   1500
#define PAD_REPEAT_FAST         60

// per local player slot: binds and look state
typedef struct {
    char        *binds[PAD_KEY_COUNT];
    char        lostKey[64];        // its pad went away: that device gets the slot back
    bool        lostPad;            // its pad disconnected while it played
    unsigned    lostTime;           // when (grace period, cl_splitPadGrace)
    bool        live;               // in the game at the last PAD_Frame
    float       boostMs;            // look stick near full deflection since
    float       dbgYaw, dbgMs;      // pad_debug: degrees turned / msec since the last report
    unsigned    dbgNext, moveDbgNext;
    float       dbgFov;             // pad_debug: the server fov last reported
} padSlot_t;

static padDevice_t  pads[MAX_PADS];
static padSlot_t    padSlots[MAX_SPLITVIEW];

static bool         pad_layer;      // PAD_Init done (virtual pads work without SDL)
static bool         pad_sdl;        // SDL joystick/gamecontroller subsystems up
static bool         pad_sdlTried;   // SDLG_Init ran (it is not retried)
static void         SDLG_Init(void);
static unsigned     pad_nextDetect;

#define MAX_IGNORED 32
static SDL_JoystickID   pad_ignored[MAX_IGNORED];   // devices logged once and left closed
static int              pad_numIgnored;

static cvar_t   *pad_enable;
static cvar_t   *pad_debug;
static cvar_t   *joy_deadzone;
static cvar_t   *joy_deadzoneOuter;
static cvar_t   *joy_yawSpeed;
static cvar_t   *joy_pitchSpeed;
static cvar_t   *joy_aimCurve;
static cvar_t   *joy_aimExponent;
static cvar_t   *joy_turnBoost;
static cvar_t   *joy_turnBoostTime;
static cvar_t   *joy_zoomScale;
static cvar_t   *joy_invertPitch;
static cvar_t   *joy_triggerThreshold;
static cvar_t   *joy_stickThreshold;

// join / leave (names and values from the Quake3e fork, its design 14.1,
// 14.2, 17.1 and in_gamepad.c)
static cvar_t   *cl_splitP1Input;
static cvar_t   *cl_splitJoinButton;
static cvar_t   *cl_splitJoinHold;
static cvar_t   *cl_splitLeaveHold;
static cvar_t   *cl_splitJoinHint;
static cvar_t   *cl_splitPadGrace;

#define PAD_HINT_TIME       10000   // ms: hint for an unjoined pad used this recently
#define PAD_REFUSAL_TIME    5000    // ms: a refused join's reason stays up
#define PAD_HINT_REMOTE     "online servers may limit extra players"

static char     pad_refusal[128];   // why the last pad join was refused
static unsigned pad_refusalUntil;

// input.c's movement cvars (registered before PAD_Init)
static cvar_t   *pad_cl_run;
static cvar_t   *pad_cl_forwardspeed;
static cvar_t   *pad_cl_sidespeed;

#define PAD_BOOST_THRESHOLD 0.9f    // rescaled look-stick magnitude that counts as "full"

static bool padFromCommand;         // inside padinject: binds run right after it
static bool padShuttingDown;        // PAD_Shutdown: no bind ups into the command buffer

/*
===============================================================================

KEYS AND BINDS

===============================================================================
*/

static int Pad_KeyFromName(const char *name)
{
    int i;

    if (!name || !name[0])
        return -1;

    for (i = 0; i < PAD_KEY_COUNT; i++) {
        // also accept the name without the PAD_ prefix ("A", "START")
        if (!Q_stricmp(name, padKeyNames[i]) || !Q_stricmp(name, padKeyNames[i] + 4))
            return i;
    }
    for (i = 0; i < q_countof(padKeyAliases); i++) {
        if (!Q_stricmp(name, padKeyAliases[i].name) || !Q_stricmp(name, padKeyAliases[i].name + 4))
            return padKeyAliases[i].key;
    }
    return -1;
}

static int Pad_AxisFromName(const char *name)
{
    int i;

    for (i = 0; i < PAX_COUNT; i++) {
        if (!Q_stricmp(name, padAxisNames[i]))
            return i;
    }
    return -1;
}

static void Pad_SetBind(int player, int key, const char *cmd)
{
    padSlot_t *s = &padSlots[player];

    if (s->binds[key]) {
        Z_Free(s->binds[key]);
        s->binds[key] = NULL;
    }
    if (cmd && cmd[0])
        s->binds[key] = Z_CopyString(cmd);
}

// local player slot n is in the game (has a client context)
static bool Pad_PlayerLive(int n)
{
    return n >= 0 && n < MAX_SPLITVIEW && clx[n] && clx[n]->inUse;
}

static const char *Pad_DefaultBind(int key)
{
    int i;

    for (i = 0; i < q_countof(padDefaultBinds); i++) {
        if (padDefaultBinds[i].key == key)
            return padDefaultBinds[i].cmd;
    }
    return NULL;
}

static bool Pad_SlotEmpty(int player)
{
    int k;

    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if (padSlots[player].binds[k])
            return false;
    }
    return true;
}

// the slot's table is exactly the default set (not written to the config)
static bool Pad_SlotIsDefault(int player)
{
    const char *def;
    int k;

    for (k = 0; k < PAD_KEY_COUNT; k++) {
        def = Pad_DefaultBind(k);
        if (!def != !padSlots[player].binds[k])
            return false;
        if (def && strcmp(def, padSlots[player].binds[k]))
            return false;
    }
    return true;
}

static void Pad_ApplyDefaults(int player)
{
    int i;

    for (i = 0; i < q_countof(padDefaultBinds); i++)
        Pad_SetBind(player, padDefaultBinds[i].key, padDefaultBinds[i].cmd);
}

// a pad is given to a player with no pad binds: the default set
static void Pad_EnsureBinds(int player)
{
    if (Pad_SlotEmpty(player)) {
        Pad_ApplyDefaults(player);
        Com_DPrintf("pad: P%d gets the default pad binds\n", player + 1);
    }
}

// player n's layout (Action's menu, scoreboard, inventory) is up
// (the `cl' macro would rewrite the ->cl member access)
#pragma push_macro("cl")
#undef cl
static bool Pad_LayoutUp(int n)
{
    return Pad_PlayerLive(n) &&
        (clx[n]->cl.frame.ps.stats[STAT_LAYOUTS] & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY));
}

// player n's fov from its last server frame (Action's sniper zoom)
static float Pad_ServerFov(int n)
{
    return Pad_PlayerLive(n) ? clx[n]->cl.frame.ps.fov : 0.0f;
}
#pragma pop_macro("cl")

// "p<N> ...\n" lines from a pad: inside padinject right after it, else in order
static void Pad_RunText(char *out, size_t len)
{
    if (!len || padShuttingDown)
        return;
    if (pad_debug->integer)
        Com_Printf("pad bind: %s", out);
    if (padFromCommand) {
        out[len - 1] = 0;   // Cbuf_InsertText adds the \n
        Cbuf_InsertText(&cmd_buffer, out);
    } else {
        Cbuf_AddText(&cmd_buffer, out);
    }
}

/*
====================
Pad_ExecBind

Runs player n's bind for a pad key as that player. Every ';'-separated
'+command' gets "<key> <time>" (key = 256 + pad key, never a keyboard key)
and its '-command' on release; other commands run on the press only.
====================
*/
static void Pad_ExecBind(int player, int key, bool down, unsigned time)
{
    char buf[MAX_STRING_CHARS], out[MAX_STRING_CHARS * 2], *p, *end;
    const char *bind = padSlots[player].binds[key];
    size_t len = 0;
    bool quote;

    if (!bind || !bind[0])
        return;

    out[0] = 0;
    Q_strlcpy(buf, bind, sizeof(buf));
    p = buf;
    while (1) {
        while (*p == ' ' || *p == '\t')
            p++;
        // next ';' outside quotes (say "a;b" stays one command)
        quote = false;
        for (end = p; *end; end++) {
            if (*end == '"')
                quote = !quote;
            else if (*end == ';' && !quote)
                break;
        }
        if (*end)
            *end = 0;
        else
            end = NULL;

        if (len < sizeof(out) - 1) {
            if (*p == '+')
                len += Q_scnprintf(out + len, sizeof(out) - len, "p%d %c%s %d %u\n",
                                   player + 1, down ? '+' : '-', p + 1, PAD_KEYBASE + key, time);
            else if (down && *p)
                len += Q_scnprintf(out + len, sizeof(out) - len, "p%d %s\n", player + 1, p);
        }
        if (!end)
            break;
        p = end + 1;
    }

    Pad_RunText(out, len);
}

/*
===============================================================================

DEVICES

===============================================================================
*/

static int Pad_Index(const padDevice_t *pad)
{
    return (int)(pad - pads);
}

static padDevice_t *Pad_ForPlayer(int player)
{
    int i;

    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected && pads[i].player == player)
            return &pads[i];
    }
    return NULL;
}

static bool Pad_UnjoinedPress(padDevice_t *pad, int key);

/*
====================
Pad_MenuKey

The menu layer (design 5.2): while player n's layout is up, d-pad up/down,
A and B navigate it (invprev / invnext / invuse / putaway) instead of their
binds. The release of such a press does nothing.
====================
*/
static bool Pad_MenuKey(padDevice_t *pad, int n, int key)
{
    char out[MAX_QPATH];
    size_t len;
    int i;

    if (!Pad_LayoutUp(n))
        return false;
    for (i = 0; i < q_countof(padMenuKeys); i++) {
        if (padMenuKeys[i].key == key)
            break;
    }
    if (i == q_countof(padMenuKeys))
        return false;

    pad->menuDown[key] = true;
    len = Q_scnprintf(out, sizeof(out), "p%d %s\n", n + 1, padMenuKeys[i].cmd);
    Pad_RunText(out, len);
    return true;
}

// a pad key as an overlay key (split_menu.c), or -1
static int Pad_OverlayKey(int key)
{
    switch (key) {
    case PAD_DPAD_UP:
    case PAD_LSTICK_UP:     return SMK_UP;
    case PAD_DPAD_DOWN:
    case PAD_LSTICK_DOWN:   return SMK_DOWN;
    case PAD_DPAD_LEFT:
    case PAD_LSTICK_LEFT:   return SMK_LEFT;
    case PAD_DPAD_RIGHT:
    case PAD_LSTICK_RIGHT:  return SMK_RIGHT;
    case PAD_A:             return SMK_A;
    case PAD_B:             return SMK_B;
    case PAD_X:             return SMK_X;
    case PAD_Y:             return SMK_Y;
    case PAD_START:         return SMK_START;
    case PAD_L3:            return SMK_L3;
    default:                return -1;
    }
}

/*
====================
Pad_UIKeyDown

Player 1's pad while the engine UI is up (Q3 design 13.2): d-pad / left
stick = arrows, A = Enter (on a text field: the on-screen keyboard), B,
Back and Start = Escape. The key's release follows on the pad key's
release (Key_Event counts downs for autorepeat).
====================
*/
static void Pad_UIKeyDown(padDevice_t *pad, int key)
{
    int k;

    switch (key) {
    case PAD_DPAD_UP:
    case PAD_LSTICK_UP:     k = K_UPARROW; break;
    case PAD_DPAD_DOWN:
    case PAD_LSTICK_DOWN:   k = K_DOWNARROW; break;
    case PAD_DPAD_LEFT:
    case PAD_LSTICK_LEFT:   k = K_LEFTARROW; break;
    case PAD_DPAD_RIGHT:
    case PAD_LSTICK_RIGHT:  k = K_RIGHTARROW; break;
    case PAD_A:             k = K_ENTER; break;
    case PAD_B:
    case PAD_BACK:
    case PAD_START:         k = K_ESCAPE; break;
    default:                return;
    }

    if (k == K_ENTER && CL_SplitMenuUIEnter()) {
        pad->menuDown[key] = true;
        return;
    }
    if (pad_debug->integer)
        Com_Printf("pad %d: engine UI %s\n", Pad_Index(pad), Key_KeynumToString(k));
    pad->uiKey[key] = k;
    Key_Event(k, true, com_eventTime);
}

static void Pad_UIKeyUp(padDevice_t *pad, int key)
{
    int k = pad->uiKey[key];

    pad->uiKey[key] = 0;
    Key_Event(k, false, com_eventTime);
}

// the repeating direction was let go: the other control (stick / d-pad)
// still held the same way takes the repeat over, else it stops
static void Pad_RepeatReleased(padDevice_t *pad, int key)
{
    const int smk = Pad_OverlayKey(key);
    int k;

    pad->repOn = false;
    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if (k != key && pad->down[k] && pad->menuDown[k] && Pad_OverlayKey(k) == smk) {
            pad->repOn = true;
            pad->repKey = k;
            return;
        }
    }
}

/*
====================
Pad_RouteKey

Every pad key event: a pad nobody has joins / takes over a player, a player
with a layout up drives it, otherwise the player's pad binds run.
====================
*/
static void Pad_KeyUp(padDevice_t *pad, int key)
{
    const int n = pad->player;

    if (pad->uiKey[key]) {
        Pad_UIKeyUp(pad, key);
        return;
    }
    if (pad->repOn && pad->repKey == key)
        Pad_RepeatReleased(pad, key);
    if (pad->menuDown[key]) {
        pad->menuDown[key] = false;
        return;
    }
    if (pad->sentDown[key]) {
        pad->sentDown[key] = false;
        if (Pad_PlayerLive(n))
            Pad_ExecBind(n, key, false, com_eventTime);
    }
}

static void Pad_KeyDown(padDevice_t *pad, int key);
static void Pad_Assign(padDevice_t *pad, int player);
static bool Pad_P1IsPad(void);

// player n's bind for key holds something ('+command' in any part)
static bool Pad_IsHoldBind(int n, int key)
{
    const char *b = padSlots[n].binds[key];

    while (b && *b) {
        while (*b == ' ' || *b == '\t' || *b == ';')
            b++;
        if (*b == '+')
            return true;
        b = strchr(b, ';');
    }
    return false;
}

// player n > 1 owns the engine UI that is up (split_menu.c CL_SplitUIOwner):
// its pad drives it and its game input rests, like player 1's
static bool Pad_ExtraInUI(int n)
{
    return n > 0 && (cls.key_dest & KEY_MENU) && n == CL_SplitUIOwner();
}

// a press of key would run player n's hold bind right now (not the
// overlay, the engine UI, a layout's menu keys): it goes down on the press
static bool Pad_ChordHoldBind(const padDevice_t *pad, int key)
{
    const int n = pad->player;
    int i;

    if (!Pad_PlayerLive(n) || CL_SplitMenuOpen(n))
        return false;
    if ((n == 0 && cls.key_dest != KEY_GAME) || Pad_ExtraInUI(n))
        return false;
    if (Pad_LayoutUp(n)) {
        for (i = 0; i < q_countof(padMenuKeys); i++)
            if (padMenuKeys[i].key == key)
                return false;
    }
    return Pad_IsHoldBind(n, key);
}

/*
====================
Pad_ChordKey

Back and Start of a player's pad act on their release, and only if the
other one was not pressed meanwhile: Back+Start is the leave chord (held
cl_splitLeaveHold ms, Pad_CheckHolds), so neither the overlay (Start) nor
the scoreboard (Back) flashes up while it is held. A '+command' bind on
either goes down on the press as usual (held for as long as the key), unless
the other key is already down. True if handled.
====================
*/
static bool Pad_ChordKey(padDevice_t *pad, int key, bool down)
{
    const int i = (key == PAD_START), other = i ? PAD_BACK : PAD_START;

    if (key != PAD_BACK && key != PAD_START)
        return false;
    if (down) {
        if (pad->player < 0)
            return false;       // a free pad: any button takes / joins as usual
        pad->chordPending[!i] = false;
        if (pad->down[other]) {
            pad->chordPending[i] = false;   // the chord: neither acts
            return true;
        }
        if (Pad_ChordHoldBind(pad, key)) {
            pad->chordPending[i] = false;   // routed now, its release as usual
            return false;
        }
        pad->chordPending[i] = true;
        return true;
    }
    if (!pad->chordPending[i])
        return false;           // the release of a routed press, or of a chord
    pad->chordPending[i] = false;
    if (pad->player < 0)
        return true;
    if (pad_debug->integer)
        Com_Printf("pad %d: %s alone: acts on release\n", Pad_Index(pad), padKeyNames[key]);
    Pad_KeyDown(pad, key);
    Pad_KeyUp(pad, key);
    return true;
}

static void Pad_RouteKey(padDevice_t *pad, int key, bool down)
{
    if (Pad_ChordKey(pad, key, down))
        return;
    if (!down)
        Pad_KeyUp(pad, key);
    else
        Pad_KeyDown(pad, key);
}

static void Pad_KeyDown(padDevice_t *pad, int key)
{
    const int n = pad->player;

    if (n < 0) {
        // Start on any pad out of a game opens the Splitscreen page, also when
        // another pad already is player 1's (it claimed P1 first: an earlier
        // button, a drifting stick): that pad steps out, this one is P1's
        // (0.0.0.23: the maintainer could not open the page at launch)
        if (key == PAD_START && !(cls.key_dest & KEY_CONSOLE) && CL_SplitMenuCanOpenHost()) {
            if (Pad_P1IsPad() && Pad_ForPlayer(0) != pad) {
                Pad_Assign(pad, 0);
                Com_Printf("pad %d: Start at the main menu: player 1's pad now\n", Pad_Index(pad));
            }
            if (CL_SplitMenuPadStart())
                pad->menuDown[key] = true;
            return;
        }
        // a claiming / joining press does nothing else, with one exception:
        // Start that makes a pad player 1's out of a game opens the host page
        // at once (first run), not on a 2nd press
        if (Pad_UnjoinedPress(pad, key) && pad->player == 0 &&
            !(cls.key_dest & KEY_CONSOLE) && CL_SplitMenuPadStart())
            pad->menuDown[key] = true;
        return;
    }
    if (!Pad_PlayerLive(n))
        return;
    // the player's overlay menu / on-screen keyboard takes every key
    if (CL_SplitMenuOpen(n)) {
        const int smk = Pad_OverlayKey(key);

        pad->menuDown[key] = true;
        // the same direction already held on the other control (d-pad and
        // stick together): no extra step, the held one keeps repeating
        if (smk >= 0 && pad->repOn && pad->repKey != key && pad->down[pad->repKey] &&
            Pad_OverlayKey(pad->repKey) == smk)
            return;
        pad->repOn = false;     // any other press stops a repeat
        if (smk < 0)
            return;
        CL_SplitMenuKey(n, smk);
        if (smk <= SMK_RIGHT && CL_SplitMenuOpen(n)) {
            pad->repOn = true;
            pad->repKey = key;
            pad->repStart = Sys_Milliseconds();
            pad->repNext = pad->repStart + PAD_REPEAT_DELAY;
            pad->repView = CL_SplitMenuView(n);
        }
        return;
    }
    // Start on player 1's pad out of a game: the overlay with the host page
    // (Q3 design 13.4); in a game Start over the engine UI is Escape
    if (n == 0 && key == PAD_START && !(cls.key_dest & KEY_CONSOLE) && CL_SplitMenuPadStart()) {
        pad->menuDown[key] = true;
        return;
    }
    // player 1's pad drives the engine menus (Q3 design 13.2); another
    // player's pad the engine menu it opened in its own view (0.0.0.25)
    if (n == CL_SplitUIOwner() && (cls.key_dest & KEY_MENU) && !(cls.key_dest & KEY_CONSOLE)) {
        Pad_UIKeyDown(pad, key);
        return;
    }
    // player 1's console and chat line are keyboard only
    if (n == 0 && cls.key_dest != KEY_GAME)
        return;
    if (Pad_MenuKey(pad, n, key))
        return;

    pad->sentDown[key] = true;
    Pad_ExecBind(n, key, true, com_eventTime);
}

static void Pad_SetButton(padDevice_t *pad, int key, bool down)
{
    if (pad->down[key] == down)
        return;
    pad->down[key] = down;
    if (down) {
        pad->downTime[key] = Sys_Milliseconds();
        pad->lastInput = pad->downTime[key] | 1;
    }
    if (pad_debug->integer && down)
        Com_Printf("pad %d: %s down\n", Pad_Index(pad), padKeyNames[key]);
    else if (pad_debug->integer)
        Com_Printf("pad %d: %s up (held %u ms)\n", Pad_Index(pad), padKeyNames[key],
                   Sys_Milliseconds() - pad->downTime[key]);
    Pad_RouteKey(pad, key, down);
}

static void Pad_SetAxis(padDevice_t *pad, int axis, float v)
{
    float st, tt;

    if (axis == PAX_LT || axis == PAX_RT)
        v = Q_clipf(v, 0.0f, 1.0f);
    else
        v = Q_clipf(v, -1.0f, 1.0f);
    pad->axis[axis] = v;

    // the analog values also act as digital pad keys
    st = joy_stickThreshold->value;
    tt = joy_triggerThreshold->value;

    // a real push, not an idle (phantom) pad: the join hint's activity
    if ((axis >= PAX_LT) ? (v >= tt) : (fabsf(v) > joy_deadzone->value))
        pad->lastInput = Sys_Milliseconds() | 1;

    switch (axis) {
    case PAX_LX:
        Pad_SetButton(pad, PAD_LSTICK_LEFT, -v >= st);
        Pad_SetButton(pad, PAD_LSTICK_RIGHT, v >= st);
        break;
    case PAX_LY:
        Pad_SetButton(pad, PAD_LSTICK_UP, -v >= st);
        Pad_SetButton(pad, PAD_LSTICK_DOWN, v >= st);
        break;
    case PAX_RX:
        Pad_SetButton(pad, PAD_RSTICK_LEFT, -v >= st);
        Pad_SetButton(pad, PAD_RSTICK_RIGHT, v >= st);
        break;
    case PAX_RY:
        Pad_SetButton(pad, PAD_RSTICK_UP, -v >= st);
        Pad_SetButton(pad, PAD_RSTICK_DOWN, v >= st);
        break;
    case PAX_LT:
        Pad_SetButton(pad, PAD_LT, v >= tt);
        break;
    case PAX_RT:
        Pad_SetButton(pad, PAD_RT, v >= tt);
        break;
    }
}

// every bind down this pad sent gets its up (the keys stay as they are)
static void Pad_ReleaseBinds(padDevice_t *pad)
{
    int k;

    for (k = 0; k < PAD_KEY_COUNT; k++) {
        pad->menuDown[k] = false;
        if (pad->uiKey[k])
            Pad_UIKeyUp(pad, k);
        if (pad->sentDown[k]) {
            pad->sentDown[k] = false;
            if (Pad_PlayerLive(pad->player))
                Pad_ExecBind(pad->player, k, false, com_eventTime);
        }
    }
}

static void Pad_ReleaseAll(padDevice_t *pad)
{
    int i;

    // a Back / Start still held is let go, not pressed
    pad->chordPending[0] = pad->chordPending[1] = false;
    for (i = 0; i < PAX_COUNT; i++)
        Pad_SetAxis(pad, i, 0.0f);
    for (i = 0; i < PAD_KEY_COUNT; i++)
        Pad_SetButton(pad, i, false);
}

// split_menu.c: player n's menu takes its pad; held binds let go
void PAD_ReleasePlayer(int player)
{
    padDevice_t *pad;

    if (!pad_layer || player < 0 || player >= MAX_SPLITVIEW)
        return;
    pad = Pad_ForPlayer(player);
    if (pad)
        Pad_ReleaseBinds(pad);
}

static void Pad_Unassign(padDevice_t *pad)
{
    if (pad->player < 0)
        return;
    Pad_ReleaseBinds(pad);
    Com_Printf("pad %d: released from P%d\n", Pad_Index(pad), pad->player + 1);
    if (pad->player == 0)
        CL_ProfilePadReleased(0);   // a profile this pad brought to player 1 goes
    pad->player = -1;
    pad->joinDone = true;       // buttons still held don't join again
    pad->leaveStart = 0;
    pad->leaveDone = false;
}

static void Pad_Assign(padDevice_t *pad, int player)
{
    padDevice_t *other;

    if (pad->player == player)
        return;
    Pad_Unassign(pad);
    // one pad per player: the one it had steps out
    other = Pad_ForPlayer(player);
    if (other)
        Pad_Unassign(other);
    pad->player = player;
    pad->joinDone = true;       // the press that joined is not a game input
    pad->leaveStart = 0;
    pad->leaveDone = false;
    padSlots[player].lostKey[0] = 0;
    padSlots[player].lostPad = false;
    padSlots[player].boostMs = 0;
    Pad_EnsureBinds(player);
    Com_Printf("pad %d: \"%s\" is P%d's pad%s\n", Pad_Index(pad), pad->name, player + 1,
               Pad_PlayerLive(player) ? "" : " (that player is not in the game yet)");
    if (player == 0)
        CL_ProfilePadJoined(0, pad->key);       // player 1 plays as the profile this pad remembers (Q3 14.3)
    CL_ProfilePadAssigned(player, pad->key);    // a named profile: this pad remembers it
}

static padDevice_t *Pad_Attach(bool isVirtual, const char *guid, const char *key, const char *name)
{
    padDevice_t *pad;
    int i;

    for (i = 0; i < MAX_PADS && pads[i].connected; i++)
        ;
    if (i == MAX_PADS) {
        Com_Printf("pad: no free pad slot for \"%s\" (max %d)\n", name, MAX_PADS);
        return NULL;
    }

    pad = &pads[i];
    memset(pad, 0, sizeof(*pad));
    pad->connected = true;
    pad->isVirtual = isVirtual;
    pad->instanceId = -1;
    pad->player = -1;
    Q_strlcpy(pad->guid, guid, sizeof(pad->guid));
    Q_strlcpy(pad->key, key, sizeof(pad->key));
    Q_strlcpy(pad->name, name, sizeof(pad->name));

    Com_Printf("pad %d: connected %s\"%s\" (guid %s, key %s)\n", i,
               isVirtual ? "virtual " : "", pad->name, pad->guid, pad->key);

    // the same device coming back takes its player again
    for (i = 0; i < MAX_SPLITVIEW; i++) {
        if (padSlots[i].lostKey[0] && !strcmp(padSlots[i].lostKey, pad->key) && !Pad_ForPlayer(i)) {
            Com_Printf("pad %d: reconnected, P%d has control again\n", Pad_Index(pad), i + 1);
            Pad_Assign(pad, i);
            break;
        }
    }
    return pad;
}

/*
====================
Pad_Detach

A device went away: every button it held is released (no stuck movement).
Its player keeps the slot and idles: the same device coming back takes it
again, another pad nobody has takes it with any button, and an extra
player leaves after cl_splitPadGrace seconds without one.
====================
*/
static void Pad_Detach(padDevice_t *pad)
{
    const int player = pad->player;
    padSlot_t *s;

    Pad_ReleaseAll(pad);
    if (player >= 0 && player < MAX_SPLITVIEW) {
        s = &padSlots[player];
        Q_strlcpy(s->lostKey, pad->key, sizeof(s->lostKey));
        s->lostPad = Pad_PlayerLive(player);
        s->lostTime = Sys_Milliseconds();
        if (player > 0 && s->lostPad && cl_splitPadGrace->integer > 0)
            Com_Printf("pad %d: disconnected, P%d waits %d s for it (or any button on a free pad)\n",
                       Pad_Index(pad), player + 1, cl_splitPadGrace->integer);
        else
            Com_Printf("pad %d: disconnected, P%d gets it back when it returns\n", Pad_Index(pad), player + 1);
    } else {
        Com_Printf("pad %d: disconnected\n", Pad_Index(pad));
    }
    if (pad->ctrl)
        SDL_GameControllerClose(pad->ctrl);
    memset(pad, 0, sizeof(*pad));
    pad->player = -1;
}

/*
===============================================================================

SDL BACKEND

===============================================================================
*/

static bool SDLG_Known(SDL_JoystickID id)
{
    int i;

    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected && pads[i].ctrl && pads[i].instanceId == id)
            return true;
    }
    for (i = 0; i < pad_numIgnored; i++) {
        if (pad_ignored[i] == id)
            return true;
    }
    return false;
}

static void SDLG_Ignore(SDL_JoystickID id)
{
    if (pad_numIgnored < MAX_IGNORED)
        pad_ignored[pad_numIgnored++] = id;
}

// n-th visible device with this guid (0-based), for the device key
static int SDLG_Ordinal(int index, const char *guid)
{
    char other[40];
    int i, n = 0;

    for (i = 0; i < index; i++) {
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), other, sizeof(other));
        if (!strcmp(other, guid))
            n++;
    }
    return n;
}

// open every game controller SDL lists that is not open yet
static void SDLG_Detect(void)
{
    char guid[40], key[64];
    const char *name;
    SDL_GameController *ctrl;
    SDL_JoystickID id;
    padDevice_t *pad;
    int i, j, n;

    n = SDL_NumJoysticks();

    // forget ignored devices that went away
    for (j = 0; j < pad_numIgnored; ) {
        for (i = 0; i < n && SDL_JoystickGetDeviceInstanceID(i) != pad_ignored[j]; i++)
            ;
        if (i == n)
            pad_ignored[j] = pad_ignored[--pad_numIgnored];
        else
            j++;
    }

    for (i = 0; i < n; i++) {
        id = SDL_JoystickGetDeviceInstanceID(i);
        if (id < 0 || SDLG_Known(id))
            continue;
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        name = SDL_JoystickNameForIndex(i);
        if (!name)
            name = "?";
        if (!SDL_IsGameController(i)) {
            Com_Printf("pad: SDL device %d \"%s\" (guid %s, vid %04x pid %04x) ignored: no game controller mapping\n",
                       i, name, guid, SDL_JoystickGetDeviceVendor(i), SDL_JoystickGetDeviceProduct(i));
            SDLG_Ignore(id);
            continue;
        }
        ctrl = SDL_GameControllerOpen(i);
        if (!ctrl) {
            Com_Printf("pad: SDL device %d \"%s\" could not be opened: %s\n", i, name, SDL_GetError());
            SDLG_Ignore(id);
            continue;
        }
        Q_snprintf(key, sizeof(key), "%s#%d", guid, SDLG_Ordinal(i, guid));
        if (SDL_GameControllerName(ctrl))
            name = SDL_GameControllerName(ctrl);
        pad = Pad_Attach(false, guid, key, name);
        if (!pad) {
            SDL_GameControllerClose(ctrl);
            SDLG_Ignore(id);
            continue;
        }
        pad->ctrl = ctrl;
        pad->instanceId = id;
    }
}

static void SDLG_Poll(void)
{
    static unsigned lastPoll;
    unsigned now = Sys_Milliseconds();
    padDevice_t *pad;
    int i, b;

    // at most every 2 ms (500 Hz): IN_Frame runs once per main loop, which
    // can be thousands of times a second
    if (now - lastPoll < 2)
        return;
    lastPoll = now;

    SDL_GameControllerUpdate();

    for (i = 0; i < MAX_PADS; i++) {
        pad = &pads[i];
        if (!pad->connected || !pad->ctrl)
            continue;
        if (!SDL_GameControllerGetAttached(pad->ctrl)) {
            Pad_Detach(pad);
            continue;
        }
        for (b = 0; b < PAD_SDL_BUTTONS; b++)
            Pad_SetButton(pad, b, SDL_GameControllerGetButton(pad->ctrl, (SDL_GameControllerButton)b) != 0);
        for (b = 0; b < PAX_COUNT; b++)
            Pad_SetAxis(pad, b, SDL_GameControllerGetAxis(pad->ctrl, (SDL_GameControllerAxis)b) / 32767.0f);
    }

    // hot-plug: the device list is refreshed by SDL_GameControllerUpdate,
    // which is SDL_JoystickUpdate, which runs every joystick driver's
    // Detect() (SDL 2.32 src/joystick/SDL_joystick.c), so SDL_NumJoysticks
    // sees a replug without an event pump; look for new devices twice a second
    if (cls.realtime >= pad_nextDetect) {
        pad_nextDetect = cls.realtime + 500;
        SDLG_Detect();
        // joystick + gamecontroller events only (0x600..0x6FF): with the
        // engine's SDL video driver the queue also carries its own events
        SDL_FlushEvents(0x600, 0x6FF);
    }
}

static void SDLG_CloseAll(void)
{
    int i;

    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected && !pads[i].isVirtual)
            Pad_Detach(&pads[i]);
    }
    pad_numIgnored = 0;
}

/*
===============================================================================

JOIN / LEAVE / HOT-PLUG (Q3 design 6.2 "Join / drop flow", 12.1, 14.2, 17.1)

===============================================================================
*/

// cl_splitP1Input pad (default): the first pad to press a button is player 1's
static bool Pad_P1IsPad(void)
{
    return Q_stricmp(cl_splitP1Input->string, "kbm");
}

static int Pad_JoinKey(void)
{
    return Pad_KeyFromName(cl_splitJoinButton->string);
}

/*
====================
Pad_HealTarget

A player who has no pad but should have one: lost it in game, an extra
added from the console, or player 1 with cl_splitP1Input pad. Lowest
first; -1 = none. Any button of a pad nobody has takes that player.
====================
*/
static int Pad_HealTarget(void)
{
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++) {
        if (!CL_SplitPlayerLive(i) || Pad_ForPlayer(i))
            continue;
        if (padSlots[i].lostPad || i > 0 || Pad_P1IsPad())
            return i;
    }
    return -1;
}

// player 1 in a game on a server, not a demo (P1 active)
static bool Pad_CanAddPlayers(void)
{
    return cla == clx[0] && cls.state == ca_active && !cls.demo.playback;
}

// the player number the next join would get, or -1
static int Pad_NextJoinSlot(void)
{
    int n, maxPlayers = CL_SplitMaxPlayers();

    if (Pad_P1IsPad() && !Pad_ForPlayer(0))
        return 0;
    if (!Pad_CanAddPlayers())
        return -1;
    for (n = 1; n < maxPlayers; n++) {
        if (!clx[n])
            return n;
    }
    return -1;
}

static void Pad_Refuse(padDevice_t *pad, const char *why)
{
    Com_Printf("pad %d: can't join: %s\n", Pad_Index(pad), why);
    Q_snprintf(pad_refusal, sizeof(pad_refusal), "Can't join: %s", why);
    pad_refusalUntil = Sys_Milliseconds() + PAD_REFUSAL_TIME;
}

static void Pad_Join(padDevice_t *pad)
{
    char why[128];
    int n;

    if (Pad_P1IsPad() && !Pad_ForPlayer(0)) {
        Pad_Assign(pad, 0);
        Com_Printf("pad %d: joined as P1 (keyboard/mouse still work too)\n", Pad_Index(pad));
        return;
    }
    if (cla != clx[0])
        return;     // (never: pads are polled with player 1 active)
    if (cls.demo.playback) {
        Pad_Refuse(pad, "not while a demo is playing");
        return;
    }
    if (cls.state != ca_active) {
        Pad_Refuse(pad, "start or join a multiplayer game first");
        return;
    }
    n = CL_SplitAddPlayer(why, sizeof(why));
    if (n < 0) {
        Pad_Refuse(pad, why);
        return;
    }
    pad_refusalUntil = 0;
    Pad_Assign(pad, n);
    Com_Printf("pad %d: joining as P%d\n", Pad_Index(pad), n + 1);
    CL_ProfilePadJoined(n, pad->key);   // the profile last used with this pad (Q3 14.3)
}

// a button of a pad that is nobody's yet; true if it was Start and took
// player 1 (the one claiming press that also acts, see Pad_KeyDown)
static bool Pad_UnjoinedPress(padDevice_t *pad, int key)
{
    int target;

    // any button takes a player who is missing a pad
    target = Pad_HealTarget();
    if (target >= 0) {
        Pad_Assign(pad, target);
        Com_Printf("pad %d: took over P%d (it had no pad)\n", Pad_Index(pad), target + 1);
        return target == 0 && key == PAD_START;
    }

    if (key != Pad_JoinKey())
        return false;
    pad->joinDone = false;
    if (cl_splitJoinHold->integer <= 0)
        Pad_Join(pad);
    // a hold is completed by Pad_CheckHolds
    return false;
}

static void Pad_Leave(padDevice_t *pad)
{
    const int n = pad->player;

    if (n == 0) {
        // player 1 is the host slot: its pad steps out, the session goes on
        Com_Printf("pad %d: released from P1 (P1 is the host; end the game from its menu)\n", Pad_Index(pad));
        Pad_Unassign(pad);
        return;
    }
    Com_Printf("pad %d: P%d leaves (Back+Start)\n", Pad_Index(pad), n + 1);
    Pad_Unassign(pad);
    CL_SplitRequestDrop(n, "left (Back+Start)");
}

static void Pad_CheckHolds(padDevice_t *pad, unsigned now)
{
    const int joinKey = Pad_JoinKey();

    // hold-to-join
    if (pad->player < 0) {
        if (joinKey >= 0 && pad->down[joinKey]) {
            if (!pad->joinDone && cl_splitJoinHold->integer > 0 &&
                now - pad->downTime[joinKey] >= (unsigned)cl_splitJoinHold->integer) {
                pad->joinDone = true;
                if (pad_debug->integer)
                    Com_Printf("pad %d: %s held %u ms\n", Pad_Index(pad), padKeyNames[joinKey],
                               now - pad->downTime[joinKey]);
                Pad_Join(pad);
            }
        } else {
            pad->joinDone = false;
        }
        return;
    }

    // hold Back+Start to leave
    if (pad->down[PAD_BACK] && pad->down[PAD_START]) {
        if (!pad->leaveStart) {
            pad->leaveStart = now | 1;
        } else if (!pad->leaveDone && now - pad->leaveStart >= (unsigned)cl_splitLeaveHold->integer) {
            pad->leaveDone = true;
            if (pad_debug->integer)
                Com_Printf("pad %d: Back+Start held %u ms\n", Pad_Index(pad), now - pad->leaveStart);
            Pad_Leave(pad);
        }
    } else {
        pad->leaveStart = 0;
        pad->leaveDone = false;
    }
}

/*
====================
Pad_OverlayRepeat

The held overlay direction steps again (see PAD_REPEAT_DELAY). Stops at
once on the release, when the overlay closes or shows another page, when
the press was taken from the menu layer (Pad_ReleaseBinds), or when the pad
changes hands.
====================
*/
static void Pad_OverlayRepeat(padDevice_t *pad, unsigned now)
{
    const int n = pad->player;

    if (!pad->repOn)
        return;
    if (!pad->down[pad->repKey] || !pad->menuDown[pad->repKey] || !Pad_PlayerLive(n) ||
        !CL_SplitMenuOpen(n) || CL_SplitMenuView(n) != pad->repView) {
        pad->repOn = false;
        return;
    }
    if ((int)(now - pad->repNext) < 0)
        return;
    if (pad_debug->integer)
        Com_Printf("pad %d: %s repeat (held %u ms)\n", Pad_Index(pad), padKeyNames[pad->repKey],
                   now - pad->repStart);
    CL_SplitMenuKey(n, Pad_OverlayKey(pad->repKey));
    // no catching up after a long frame: one step per frame at most
    pad->repNext = now + (now - pad->repStart >= PAD_REPEAT_FAST_AFTER ? PAD_REPEAT_FAST : PAD_REPEAT_RATE);
}

// cl_splitP1Input switched to kbm: player 1's pad steps out
static void cl_splitP1Input_changed(cvar_t *self)
{
    padDevice_t *pad = Pad_ForPlayer(0);

    if (pad && !Pad_P1IsPad()) {
        Com_Printf("pad %d: released from P1 (cl_splitP1Input kbm: player 1 is keyboard/mouse)\n",
                   Pad_Index(pad));
        Pad_Unassign(pad);
    }
}

/*
====================
PAD_HintText

The join hint (Q3 design 14.1), drawn by screen.c bottom-right over the
cells. cl_splitJoinHint 1: "Player N: hold A to join" for a pad nobody has
that produced real input in the last 10 s; 2: "Hold A on a controller to
join" for the first 10 s of each level while such a pad is connected; 0:
off. A refused join shows its reason here for 5 s instead.
====================
*/
const char *PAD_HintText(const char **note, bool *warn)
{
    static char text[128];
    static unsigned levelStart;
    static bool wasActive;
    const unsigned now = Sys_Milliseconds();
    const char *verb = cl_splitJoinHold->integer > 0 ? "hold" : "press";
    const char *button;
    padDevice_t *pad = NULL;
    int i, slot;

    *note = NULL;
    *warn = false;

    if (!pad_layer || cla != clx[0])
        return NULL;
    if (cls.state != ca_active) {
        wasActive = false;  // loading a level: mode 2 starts its 10 s again
        return NULL;
    }
    if (!wasActive) {
        wasActive = true;
        levelStart = now;
    }
    if (cls.demo.playback || !cl_splitJoinHint->integer)
        return NULL;

    if (pad_refusalUntil && (int)(pad_refusalUntil - now) > 0) {
        *warn = true;
        return pad_refusal;
    }
    pad_refusalUntil = 0;

    button = Pad_JoinKey() >= 0 ? padKeyNames[Pad_JoinKey()] + 4 : "A";

    if (cl_splitJoinHint->integer == 2) {
        if (now - levelStart > PAD_HINT_TIME || Pad_NextJoinSlot() < 0)
            return NULL;
        for (i = 0; i < MAX_PADS; i++) {
            if (pads[i].connected && pads[i].player < 0)
                break;
        }
        if (i == MAX_PADS)
            return NULL;
        Q_snprintf(text, sizeof(text), "%s %s on a controller to join", verb[0] == 'h' ? "Hold" : "Press", button);
    } else {
        for (i = 0; i < MAX_PADS; i++) {
            if (pads[i].connected && pads[i].player < 0 && pads[i].lastInput &&
                now - pads[i].lastInput < PAD_HINT_TIME) {
                pad = &pads[i];
                break;
            }
        }
        if (!pad)
            return NULL;
        slot = Pad_HealTarget();
        if (slot >= 0) {
            Q_snprintf(text, sizeof(text), "Player %d: press any button", slot + 1);
        } else {
            slot = Pad_NextJoinSlot();
            if (slot < 0)
                return NULL;
            Q_snprintf(text, sizeof(text), "Player %d: %s %s to join", slot + 1, verb, button);
        }
    }

    // on another machine's server the join may be refused
    if (cls.serverAddress.type != NA_LOOPBACK)
        *note = PAD_HINT_REMOTE;
    return text;
}

// "controller disconnected" line in player n's cell, or NULL
const char *PAD_CellStatus(int n)
{
    const padSlot_t *s;
    int left;

    if (!pad_layer || n < 0 || n >= MAX_SPLITVIEW)
        return NULL;
    s = &padSlots[n];
    if (!s->lostPad || Pad_ForPlayer(n) || !CL_SplitPlayerLive(n))
        return NULL;
    if (n == 0 || cl_splitPadGrace->integer <= 0)
        return va("Player %d: controller disconnected", n + 1);
    left = cl_splitPadGrace->integer - (int)(Sys_Milliseconds() - s->lostTime) / 1000;
    return va("Player %d: controller disconnected (leaves in %d s)", n + 1, max(left, 0));
}

// BUTTON_ANY for the active context: a button of its pad is down
bool PAD_AnyButtonDown(void)
{
    const int n = cla->playerNum;
    padDevice_t *pad;
    int k;

    if (!pad_layer || n < 0 || n >= MAX_SPLITVIEW)
        return false;
    if ((n == 0 && cls.key_dest != KEY_GAME) || Pad_ExtraInUI(n))
        return false;
    if (CL_SplitMenuOpen(n))
        return false;
    pad = Pad_ForPlayer(n);
    if (!pad)
        return false;
    for (k = 0; k < PAD_LSTICK_UP; k++) {      // buttons, d-pad, triggers
        if (pad->down[k])
            return true;
    }
    return false;
}

/*
===============================================================================

FRAME

===============================================================================
*/

void PAD_Frame(void)
{
    const unsigned now = Sys_Milliseconds();
    padDevice_t *pad;
    padSlot_t *s;
    bool live;
    int i;

    if (!pad_layer)
        return;

    // pad_enable 0 at startup: SDL's joystick subsystems (XInput, raw
    // input, HIDAPI, WGI) are only started once pad_enable is set
    if (!pad_sdlTried && pad_enable->integer)
        SDLG_Init();

    if (pad_sdl) {
        if (pad_enable->integer) {
            SDLG_Poll();
        } else {
            // pad_enable 0: real devices closed (padinject still works)
            for (i = 0; i < MAX_PADS && !(pads[i].connected && !pads[i].isVirtual); i++)
                ;
            if (i < MAX_PADS) {
                Com_Printf("pad: pad_enable 0, real devices closed\n");
                SDLG_CloseAll();
            }
            pad_numIgnored = 0;
        }
    }

    for (i = 0; i < MAX_SPLITVIEW; i++) {
        s = &padSlots[i];
        live = CL_SplitPlayerLive(i);

        // a player that left (dropplayer, server kick, P1 left): its pad is
        // free to join again, its held binds are forgotten (no up to send)
        if (s->live && !live) {
            pad = Pad_ForPlayer(i);
            if (pad) {
                memset(pad->sentDown, 0, sizeof(pad->sentDown));
                Com_Printf("pad %d: P%d is gone, pad is free to join again\n", Pad_Index(pad), i + 1);
                Pad_Unassign(pad);
            }
            s->lostPad = false;
            s->lostKey[0] = 0;
        }
        s->live = live;

        // pad_debug: a player's server fov changed (sniper zoom)
        if (pad_debug->integer && live && Pad_ServerFov(i) != s->dbgFov) {
            Com_Printf("P%d fov %.0f -> %.0f\n", i + 1, s->dbgFov, Pad_ServerFov(i));
            s->dbgFov = Pad_ServerFov(i);
        }

        // an extra whose pad stays away leaves after the grace period
        if (i > 0 && live && s->lostPad && !Pad_ForPlayer(i) && cl_splitPadGrace->integer > 0 &&
            now - s->lostTime >= (unsigned)cl_splitPadGrace->integer * 1000) {
            Com_Printf("P%d: controller disconnected for %d s, leaving\n", i + 1, cl_splitPadGrace->integer);
            s->lostPad = false;
            s->lostKey[0] = 0;
            CL_SplitRequestDrop(i, "controller disconnected");
        }
    }

    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected) {
            Pad_CheckHolds(&pads[i], now);
            Pad_OverlayRepeat(&pads[i], now);
        }
    }
}

/*
===============================================================================

ANALOG MOVE / LOOK (Q3 design 14.4)

===============================================================================
*/

// per-player value: p<N>_<cvar> if it exists, else the shared cvar
static float Pad_PlayerValue(int n, const cvar_t *base)
{
    cvar_t *var = Cvar_FindVar(va("p%d_%s", n + 1, base->name));

    return var ? var->value : base->value;
}

static const char *Pad_PlayerString(int n, const cvar_t *base)
{
    cvar_t *var = Cvar_FindVar(va("p%d_%s", n + 1, base->name));

    return var ? var->string : base->string;
}

// radial deadzone: returns the rescaled magnitude 0..1 and the direction
static float Pad_Radial(float x, float y, float inner, float outer, float *dx, float *dy)
{
    const float mag = sqrtf(x * x + y * y);
    float m;

    *dx = *dy = 0.0f;
    if (mag <= inner || mag <= 0.0f)
        return 0.0f;
    if (outer <= inner)
        outer = inner + 0.01f;
    m = (mag - inner) / (outer - inner);
    if (m > 1.0f)
        m = 1.0f;
    *dx = x / mag;
    *dy = y / mag;
    return m;
}

/*
====================
Pad_AimCurve

Look response on the rescaled stick magnitude m (0..1): linear; standard =
m^2; dynamic = S-curve (cubic ease-in-out); custom = m^joy_aimExponent.
====================
*/
static float Pad_AimCurve(int n, float m)
{
    const char *curve = Pad_PlayerString(n, joy_aimCurve);

    if (!Q_stricmp(curve, "linear"))
        return m;
    if (!Q_stricmp(curve, "dynamic"))
        return (m < 0.5f) ? 4.0f * m * m * m : 1.0f - 4.0f * (1.0f - m) * (1.0f - m) * (1.0f - m);
    if (!Q_stricmp(curve, "custom"))
        return powf(m, Q_clipf(Pad_PlayerValue(n, joy_aimExponent), 0.5f, 4.0f));
    return m * m;   // standard
}

// the active context's pad, unless its sticks are muted
static padDevice_t *Pad_ActivePad(void)
{
    const int n = cla->playerNum;

    if (!pad_layer || n < 0 || n >= MAX_SPLITVIEW)
        return NULL;
    // player 1's menus and console take the keyboard; the sticks rest too
    if ((n == 0 && (cls.key_dest & (KEY_MENU | KEY_CONSOLE))) || Pad_ExtraInUI(n))
        return NULL;
    // the player's overlay menu is up: its usercmds are idle (Q3 12.2)
    if (CL_SplitMenuOpen(n))
        return NULL;
    return Pad_ForPlayer(n);
}

/*
====================
CL_GamepadMove

From CL_BaseMove (active context): the left stick adds forward / side move.
Magnitude is speed; running follows cl_run and +speed like the keyboard.
====================
*/
void CL_GamepadMove(vec3_t move)
{
    const int n = cla->playerNum;
    padDevice_t *pad = Pad_ActivePad();
    padSlot_t *s;
    float m, dx, dy, scale;

    if (!pad)
        return;

    m = Pad_Radial(pad->axis[PAX_LX], pad->axis[PAX_LY],
                   Pad_PlayerValue(n, joy_deadzone), Pad_PlayerValue(n, joy_deadzoneOuter), &dx, &dy);
    if (m <= 0.0f)
        return;

    scale = ((cla->in.in_speed.state & 1) ^ pad_cl_run->integer) ? 2.0f : 1.0f;
    move[0] -= dy * m * pad_cl_forwardspeed->value * scale;
    move[1] += dx * m * pad_cl_sidespeed->value * scale;

    s = &padSlots[n];
    if (pad_debug->integer >= 2 && cls.realtime >= s->moveDbgNext) {
        s->moveDbgNext = cls.realtime + 500;
        Com_Printf("P%d move: stick %.2f,%.2f m %.3f %s -> forward %.0f side %.0f\n", n + 1,
                   pad->axis[PAX_LX], pad->axis[PAX_LY], m, scale > 1 ? "run" : "walk", move[0], move[1]);
    }
}

/*
====================
CL_GamepadLook

From CL_UpdateCmd (active context): the right stick turns, in degrees per
second: aim curve, turn boost near full deflection (yaw), and zoom scale.
Zoom follows the server fov like m_autosens does for the mouse (Action's
sniper zoom: fov 45 / 20 / 10), times joy_zoomScale while zoomed.
====================
*/
void CL_GamepadLook(int msec)
{
    const int n = cla->playerNum;
    padDevice_t *pad = Pad_ActivePad();
    padSlot_t *s;
    float m, dx, dy, c, ms, dt, boost, zoom, boostTime, yawRate, pitchRate;

    if (!pad || msec <= 0)
        return;

    s = &padSlots[n];
    ms = (float)msec;
    dt = ms * 0.001f;
    m = Pad_Radial(pad->axis[PAX_RX], pad->axis[PAX_RY],
                   Pad_PlayerValue(n, joy_deadzone), Pad_PlayerValue(n, joy_deadzoneOuter), &dx, &dy);
    if (m <= 0.0f) {
        s->boostMs = 0;
        s->dbgYaw = s->dbgMs = 0;
        s->dbgNext = cls.realtime + 500;
        return;
    }

    c = Pad_AimCurve(n, m);

    boost = 1.0f;
    if (m >= PAD_BOOST_THRESHOLD) {
        s->boostMs += ms;
        boostTime = Pad_PlayerValue(n, joy_turnBoostTime);
        boost = 1.0f + (Pad_PlayerValue(n, joy_turnBoost) - 1.0f)
            * (boostTime > 0.0f ? min(s->boostMs / boostTime, 1.0f) : 1.0f);
    } else {
        s->boostMs = 0;
    }

    zoom = 1.0f;
    if (cl.fov_x > 0.0f && cl.fov_x < 89.5f)
        zoom = cl.fov_x / 90.0f * Pad_PlayerValue(n, joy_zoomScale);

    yawRate = Pad_PlayerValue(n, joy_yawSpeed) * c * dx * boost * zoom;
    pitchRate = Pad_PlayerValue(n, joy_pitchSpeed) * c * dy * zoom;
    if (Pad_PlayerValue(n, joy_invertPitch))
        pitchRate = -pitchRate;

    cl.viewangles[YAW] -= yawRate * dt;
    cl.viewangles[PITCH] += pitchRate * dt;

    // pad_debug: the turn rate actually applied, twice a second
    if (pad_debug->integer) {
        s->dbgYaw += yawRate * dt;
        s->dbgMs += ms;
        if (cls.realtime >= s->dbgNext) {
            Com_Printf("P%d aim: stick %.2f,%.2f m %.3f curve %.3f boost %.2f fov %.0f zoom %.3f "
                       "-> yaw %.1f pitch %.1f deg/s (measured yaw %.1f deg/s over %.0f ms)\n",
                       n + 1, pad->axis[PAX_RX], pad->axis[PAX_RY], m, c, boost, cl.fov_x, zoom,
                       yawRate, pitchRate, s->dbgYaw * 1000.0f / s->dbgMs, s->dbgMs);
            s->dbgYaw = s->dbgMs = 0;
            s->dbgNext = cls.realtime + 500;
        }
    }
}

/*
===============================================================================

COMMANDS

===============================================================================
*/

static int Pad_PlayerArg(int arg)
{
    const char *s = Cmd_Argv(arg);
    int n = Q_atoi(s) - 1;

    if (!COM_IsUint(s) || n < 0 || n >= MAX_SPLITVIEW) {
        Com_Printf("player must be 1..%d, not \"%s\"\n", MAX_SPLITVIEW, s);
        return -1;
    }
    return n;
}

// a bind is about to change: the pad holding that key gets the old bind's up
static void Pad_ReleaseHeld(int player, int key)
{
    padDevice_t *pad = Pad_ForPlayer(player);
    int k;

    if (!pad)
        return;
    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if ((key < 0 || k == key) && pad->sentDown[k]) {
            pad->sentDown[k] = false;
            if (Pad_PlayerLive(player))
                Pad_ExecBind(player, k, false, com_eventTime);
        }
    }
}

static void Pad_PrintKeys(void)
{
    int k;

    Com_Printf("pad keys:");
    for (k = 0; k < PAD_KEY_COUNT; k++)
        Com_Printf(" %s", padKeyNames[k]);
    Com_Printf("\n");
}

static void Pad_Bind_f(void)
{
    int n, key;

    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: %s <player 1..%d> <padkey> [command]\n", Cmd_Argv(0), MAX_SPLITVIEW);
        Pad_PrintKeys();
        return;
    }
    if ((n = Pad_PlayerArg(1)) < 0)
        return;
    key = Pad_KeyFromName(Cmd_Argv(2));
    if (key < 0) {
        Com_Printf("%s: \"%s\" isn't a pad key\n", Cmd_Argv(0), Cmd_Argv(2));
        Pad_PrintKeys();
        return;
    }
    if (Cmd_Argc() == 3) {
        if (padSlots[n].binds[key])
            Com_Printf("P%d %s = \"%s\"\n", n + 1, padKeyNames[key], padSlots[n].binds[key]);
        else
            Com_Printf("P%d %s is not bound\n", n + 1, padKeyNames[key]);
        return;
    }
    Pad_ReleaseHeld(n, key);
    Pad_SetBind(n, key, Cmd_ArgsFrom(3));
}

static void Pad_Unbind_f(void)
{
    int n, key;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: %s <player 1..%d> <padkey>\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    if ((n = Pad_PlayerArg(1)) < 0)
        return;
    key = Pad_KeyFromName(Cmd_Argv(2));
    if (key < 0) {
        Com_Printf("%s: \"%s\" isn't a pad key\n", Cmd_Argv(0), Cmd_Argv(2));
        return;
    }
    Pad_ReleaseHeld(n, key);
    Pad_SetBind(n, key, NULL);
}

static void Pad_UnbindSlot(int player)
{
    int k;

    for (k = 0; k < PAD_KEY_COUNT; k++)
        Pad_SetBind(player, k, NULL);
}

// punbindall [player]: one player's pad binds, or every player's
static void Pad_UnbindAll_f(void)
{
    int n, first = 0, last = MAX_SPLITVIEW - 1;

    if (Cmd_Argc() > 2) {
        Com_Printf("Usage: %s [player 1..%d]\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    if (Cmd_Argc() == 2) {
        if ((first = last = Pad_PlayerArg(1)) < 0)
            return;
    }
    for (n = first; n <= last; n++) {
        Pad_ReleaseHeld(n, -1);
        Pad_UnbindSlot(n);
    }
}

// pbindlist [player]: one player's pad binds, or every player's
static void Pad_BindList_f(void)
{
    int n, k, count, first = 0, last = MAX_SPLITVIEW - 1;

    if (Cmd_Argc() > 2) {
        Com_Printf("Usage: %s [player 1..%d]\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    if (Cmd_Argc() == 2) {
        if ((first = last = Pad_PlayerArg(1)) < 0)
            return;
    }
    for (n = first; n <= last; n++) {
        count = 0;
        for (k = 0; k < PAD_KEY_COUNT; k++) {
            if (!padSlots[n].binds[k])
                continue;
            if (!count++)
                Com_Printf("P%d pad binds:\n", n + 1);
            Com_Printf("  %-16s \"%s\"\n", padKeyNames[k], padSlots[n].binds[k]);
        }
        if (!count && first == last)
            Com_Printf("P%d has no pad binds\n", n + 1);
    }
}

// padbinddefaults <player>: that player's pad binds back to the default set
static void Pad_BindDefaults_f(void)
{
    int n;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <player 1..%d>\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    if ((n = Pad_PlayerArg(1)) < 0)
        return;
    Pad_ReleaseHeld(n, -1);
    Pad_UnbindSlot(n);
    Pad_ApplyDefaults(n);
    Com_Printf("P%d has the default pad binds\n", n + 1);
}

/*
====================
PAD_WriteBindings

Only player 1's table goes to the config (M2-T5): players 2..8 keep theirs
in their profile (split_profile.c) or, as guests, for their stay only. A
table holding exactly the default set (or nothing) writes no lines: the
defaults are applied again when a pad is given to the player.
====================
*/
void PAD_WriteBindings(qhandle_t f)
{
    int k;

    if (Pad_SlotIsDefault(0))
        return;
    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if (padSlots[0].binds[k])
            FS_FPrintf(f, "pbind 1 %s \"%s\"\n", padKeyNames[k], padSlots[0].binds[k]);
    }
}

// profiles (split_profile.c): a player's table as slot-free lines
void PAD_ProfileWriteBinds(int player, char *buf, size_t size)
{
    int k;

    if (player < 0 || player >= MAX_SPLITVIEW)
        return;
    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if (padSlots[player].binds[k])
            Q_strlcat(buf, va("pbind %s \"%s\"\n", padKeyNames[k], padSlots[player].binds[k]), size);
    }
}

void PAD_ProfileClearBinds(int player)
{
    if (player < 0 || player >= MAX_SPLITVIEW)
        return;
    Pad_ReleaseHeld(player, -1);
    Pad_UnbindSlot(player);
}

bool PAD_ProfileSetBind(int player, const char *key, const char *cmd)
{
    int k = Pad_KeyFromName(key);

    if (player < 0 || player >= MAX_SPLITVIEW || k < 0)
        return false;
    Pad_SetBind(player, k, cmd);
    return true;
}

void PAD_ProfileDefaultBinds(int player)
{
    if (player < 0 || player >= MAX_SPLITVIEW)
        return;
    Pad_ReleaseHeld(player, -1);
    Pad_UnbindSlot(player);
    Pad_ApplyDefaults(player);
}

/*
====================
PAD_ProfileStashBinds

A config write while player 1 plays as a profile (split_profile.c): its
table is set aside (true) and an empty one filled with player 1's own
binds, then that is dropped and the table put back (false). Nothing runs
in between, so held keys are not touched.
====================
*/
void PAD_ProfileStashBinds(bool stash)
{
    static char *stashed[PAD_KEY_COUNT];
    int k;

    for (k = 0; k < PAD_KEY_COUNT; k++) {
        if (stash) {
            stashed[k] = padSlots[0].binds[k];
            padSlots[0].binds[k] = NULL;
        } else {
            Pad_SetBind(0, k, NULL);
            padSlots[0].binds[k] = stashed[k];
            stashed[k] = NULL;
        }
    }
}

// the overlay's Buttons page: one key of player n's table
const char *PAD_GetBind(int player, const char *key)
{
    int k = Pad_KeyFromName(key);

    if (player < 0 || player >= MAX_SPLITVIEW || k < 0 || !padSlots[player].binds[k])
        return "";
    return padSlots[player].binds[k];
}

void PAD_MenuSetBind(int player, const char *key, const char *cmd)
{
    int k = Pad_KeyFromName(key);

    if (player < 0 || player >= MAX_SPLITVIEW || k < 0)
        return;
    Pad_ReleaseHeld(player, k);
    Pad_SetBind(player, k, cmd);
}

// before the menu changes one key of an empty table (a player without a
// pad yet): the defaults first, else Pad_EnsureBinds would skip them later
void PAD_MenuEnsureBinds(int player)
{
    if (player >= 0 && player < MAX_SPLITVIEW)
        Pad_EnsureBinds(player);
}

int PAD_ConnectedCount(void)
{
    int i, count = 0;

    for (i = 0; pad_layer && i < MAX_PADS; i++) {
        if (pads[i].connected)
            count++;
    }
    return count;
}

// the device key (guid#n, virtual-N) of player n's pad, NULL if it has none
const char *PAD_PlayerDevice(int player)
{
    const padDevice_t *pad;

    if (!pad_layer || player < 0 || player >= MAX_SPLITVIEW)
        return NULL;
    pad = Pad_ForPlayer(player);
    return pad ? pad->key : NULL;
}

static void Pad_List_f(void)
{
    padDevice_t *pad;
    char guid[40];
    const char *s;
    int i, k, n, found = 0;

    if (!pad_layer) {
        Com_Printf("pad: not initialised\n");
        return;
    }

    if (pad_sdl) {
        SDL_version v;

        SDL_GetVersion(&v);
        n = SDL_NumJoysticks();
        Com_Printf("SDL %d.%d.%d, pad_enable %d, %d joystick device(s) visible to SDL\n",
                   v.major, v.minor, v.patch, pad_enable->integer, n);
        for (i = 0; i < n; i++) {
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
            s = SDL_JoystickNameForIndex(i);
            Com_Printf("  SDL %d: \"%s\" guid %s instance %d vid %04x pid %04x %s\n", i, s ? s : "?", guid,
                       (int)SDL_JoystickGetDeviceInstanceID(i), SDL_JoystickGetDeviceVendor(i),
                       SDL_JoystickGetDeviceProduct(i),
                       SDL_IsGameController(i) ? "game controller" : "no controller mapping (ignored)");
        }
    } else {
        Com_Printf("SDL joystick subsystem not available: virtual pads (padinject) only\n");
    }

    for (i = 0; i < MAX_PADS; i++) {
        pad = &pads[i];
        if (!pad->connected)
            continue;
        found++;
        Com_Printf("pad %d: %s\"%s\" guid %s instance %d key %s -> %s%s\n", i,
                   pad->isVirtual ? "virtual " : "", pad->name, pad->guid, (int)pad->instanceId, pad->key,
                   pad->player >= 0 ? va("P%d", pad->player + 1) : "no player",
                   pad->player >= 0 && !Pad_PlayerLive(pad->player) ? " (not in the game)" : "");
        Com_Printf("       axes LX %+.2f LY %+.2f RX %+.2f RY %+.2f LT %.2f RT %.2f  down:",
                   pad->axis[PAX_LX], pad->axis[PAX_LY], pad->axis[PAX_RX], pad->axis[PAX_RY],
                   pad->axis[PAX_LT], pad->axis[PAX_RT]);
        for (k = 0; k < PAD_KEY_COUNT; k++) {
            if (pad->down[k])
                Com_Printf(" %s", padKeyNames[k]);
        }
        Com_Printf("\n");
    }
    if (!found)
        Com_Printf("no pads connected\n");
    for (i = 0; i < MAX_SPLITVIEW; i++) {
        if (padSlots[i].lostKey[0])
            Com_Printf("P%d: its pad (key %s) is disconnected, waiting for it\n", i + 1, padSlots[i].lostKey);
    }
}

/*
====================
Pad_Assign_f

padassign <pad#> <player 1..8 | 0>: explicit device -> player mapping,
overriding hold-to-join / take-over. 0 / none releases the pad.
====================
*/
static void Pad_Assign_f(void)
{
    const char *s;
    int i, n;

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: %s <pad# from padlist> <player 1..%d | 0 = none>\n", Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    s = Cmd_Argv(1);
    i = Q_atoi(s);
    if (!COM_IsUint(s) || i < 0 || i >= MAX_PADS || !pads[i].connected) {
        Com_Printf("%s: no pad %s (see padlist)\n", Cmd_Argv(0), s);
        return;
    }
    s = Cmd_Argv(2);
    if (!strcmp(s, "0") || !Q_stricmp(s, "none")) {
        if (pads[i].player < 0)
            Com_Printf("pad %d is not anybody's\n", i);
        Pad_Unassign(&pads[i]);
        return;
    }
    if ((n = Pad_PlayerArg(2)) < 0)
        return;
    Pad_Assign(&pads[i], n);
}

/*
====================
Pad_Inject_f

padinject <vpad 1..8> <padkey> <0|1>
padinject <vpad 1..8> <LX|LY|RX|RY|LT|RT> <value>
padinject <vpad 1..8> disconnect

Drives virtual pad N (device key "virtual-<N>", connected on first use and
nobody's, like a real pad being plugged in) through the same paths as a
real pad, for headless tests: hold A to join, any button to take over a
player missing a pad, Back+Start to leave, disconnect / reconnect. Axis
values stay until changed. `padassign` gives it to a player directly.
====================
*/
static void Pad_Inject(void)
{
    padDevice_t *pad;
    const char *what;
    char guid[40];
    int n, k, i;

    if (!pad_layer) {
        Com_Printf("pad: not initialised\n");
        return;
    }
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: %s <virtual pad 1..%d> <padkey 0|1 | LX|LY|RX|RY|LT|RT value | disconnect>\n",
                   Cmd_Argv(0), MAX_PADS);
        return;
    }
    if ((n = Pad_PlayerArg(1)) < 0)     // 1..8, the same range
        return;
    what = Cmd_Argv(2);

    // the virtual pad with that number, by its device key
    Q_snprintf(guid, sizeof(guid), "virtual-%d", n + 1);
    pad = NULL;
    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected && pads[i].isVirtual && !strcmp(pads[i].key, guid)) {
            pad = &pads[i];
            break;
        }
    }

    if (!Q_stricmp(what, "disconnect")) {
        if (pad)
            Pad_Detach(pad);
        else
            Com_Printf("%s: virtual pad %d is not connected\n", Cmd_Argv(0), n + 1);
        return;
    }

    k = Pad_KeyFromName(what);
    i = Pad_AxisFromName(what);
    if (k < 0 && i < 0) {
        Com_Printf("%s: \"%s\" is not a pad key or axis (LX LY RX RY LT RT)\n", Cmd_Argv(0), what);
        return;
    }
    if (Cmd_Argc() < 4) {
        Com_Printf("%s: %s needs a value\n", Cmd_Argv(0), what);
        return;
    }

    if (!pad) {
        pad = Pad_Attach(true, guid, guid, va("Virtual pad %d", n + 1));
        if (!pad)
            return;
    }

    if (i >= 0)
        Pad_SetAxis(pad, i, Q_atof(Cmd_Argv(3)));
    else if (k == PAD_LT)
        Pad_SetAxis(pad, PAX_LT, Q_atof(Cmd_Argv(3)));
    else if (k == PAD_RT)
        Pad_SetAxis(pad, PAX_RT, Q_atof(Cmd_Argv(3)));
    else
        Pad_SetButton(pad, k, Q_atof(Cmd_Argv(3)) >= 0.5f);
}

static void Pad_Inject_f(void)
{
    padFromCommand = true;
    Pad_Inject();
    padFromCommand = false;
}

/*
===============================================================================

INIT / SHUTDOWN

===============================================================================
*/

static const cmdreg_t c_padbinds[] = {
    { "pbind", Pad_Bind_f },
    { "punbind", Pad_Unbind_f },
    { "punbindall", Pad_UnbindAll_f },
    { "pbindlist", Pad_BindList_f },
    { "padbinddefaults", Pad_BindDefaults_f },
    { NULL }
};

static const cmdreg_t c_pad[] = {
    { "padlist", Pad_List_f },
    { "padassign", Pad_Assign_f },
    { "padinject", Pad_Inject_f },
    { NULL }
};

// before config.cfg runs (its pbind lines)
void PAD_PreInit(void)
{
    Cmd_Register(c_padbinds);
}

static void SDLG_Init(void)
{
    SDL_version v;

    pad_sdlTried = true;
    SDL_GetVersion(&v);

    // pads only: keep working while the native window is unfocused, poll
    // from SDL's own joystick thread, positional face buttons, leave the
    // process's signal handlers alone (same hints as the Quake3e fork)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetMainReady();

    // with -Dsdl2 the engine's video driver already initialised SDL: this
    // only adds the joystick subsystems (reference counted)
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        Com_Printf("pad: SDL %d.%d.%d joystick/gamecontroller init failed: %s\n",
                   v.major, v.minor, v.patch, SDL_GetError());
        return;
    }

    // state is polled; no event queue is drained, so keep it empty
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_GameControllerEventState(SDL_IGNORE);

    pad_sdl = true;
    Com_Printf("pad: SDL %d.%d.%d joystick+gamecontroller initialised\n", v.major, v.minor, v.patch);
}

void PAD_Init(void)
{
    int i;

    if (pad_layer)
        return;

    pad_enable = Cvar_Get("pad_enable", "1", CVAR_ARCHIVE);
    pad_debug = Cvar_Get("pad_debug", "0", 0);
    joy_deadzone = Cvar_Get("joy_deadzone", "0.15", CVAR_ARCHIVE);
    joy_deadzoneOuter = Cvar_Get("joy_deadzoneOuter", "0.95", CVAR_ARCHIVE);
    joy_yawSpeed = Cvar_Get("joy_yawSpeed", "320", CVAR_ARCHIVE);
    joy_pitchSpeed = Cvar_Get("joy_pitchSpeed", "200", CVAR_ARCHIVE);
    joy_aimCurve = Cvar_Get("joy_aimCurve", "standard", CVAR_ARCHIVE);
    joy_aimExponent = Cvar_Get("joy_aimExponent", "2", CVAR_ARCHIVE);
    joy_turnBoost = Cvar_Get("joy_turnBoost", "1.5", CVAR_ARCHIVE);
    joy_turnBoostTime = Cvar_Get("joy_turnBoostTime", "250", CVAR_ARCHIVE);
    joy_zoomScale = Cvar_Get("joy_zoomScale", "1", CVAR_ARCHIVE);
    joy_invertPitch = Cvar_Get("joy_invertPitch", "0", CVAR_ARCHIVE);
    joy_triggerThreshold = Cvar_Get("joy_triggerThreshold", "0.3", CVAR_ARCHIVE);
    joy_stickThreshold = Cvar_Get("joy_stickThreshold", "0.5", CVAR_ARCHIVE);

    cl_splitP1Input = Cvar_Get("cl_splitP1Input", "pad", CVAR_ARCHIVE);
    cl_splitP1Input->changed = cl_splitP1Input_changed;
    cl_splitJoinButton = Cvar_Get("cl_splitJoinButton", "PAD_A", CVAR_ARCHIVE);
    cl_splitJoinHold = Cvar_Get("cl_splitJoinHold", "700", CVAR_ARCHIVE);
    cl_splitLeaveHold = Cvar_Get("cl_splitLeaveHold", "2000", CVAR_ARCHIVE);
    cl_splitJoinHint = Cvar_Get("cl_splitJoinHint", "1", CVAR_ARCHIVE);
    cl_splitPadGrace = Cvar_Get("cl_splitPadGrace", "30", CVAR_ARCHIVE);

    pad_cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE);
    pad_cl_forwardspeed = Cvar_Get("cl_forwardspeed", "200", 0);
    pad_cl_sidespeed = Cvar_Get("cl_sidespeed", "200", 0);

    memset(pads, 0, sizeof(pads));
    for (i = 0; i < MAX_PADS; i++)
        pads[i].player = -1;

    // pad_enable 0 (agent test runs): no pad device is enumerated or opened
    // at all, not even by SDL's init; PAD_Frame starts SDL if it is set later
    pad_sdlTried = false;
    if (pad_enable->integer)
        SDLG_Init();
    else
        Com_Printf("pad: pad_enable 0: SDL joystick subsystems not started, no pad device opened\n");
    pad_layer = true;
    pad_nextDetect = 0;

    if (pad_sdl && pad_enable->integer) {
        SDL_GameControllerUpdate();
        SDLG_Detect();
        Com_Printf("pad: %d SDL joystick device(s)\n", SDL_NumJoysticks());
    }

    Cmd_Register(c_pad);
}

void PAD_Shutdown(void)
{
    int i;

    if (!pad_layer)
        return;

    // no bind ups queued for a client that is going away
    padShuttingDown = true;
    for (i = 0; i < MAX_PADS; i++) {
        if (pads[i].connected)
            Pad_Detach(&pads[i]);
    }
    padShuttingDown = false;
    Cmd_Deregister(c_pad);

    if (pad_sdl) {
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
#if !USE_SDL
        SDL_Quit();     // our own SDL; the engine's (-Dsdl2) is the video driver's
#endif
        pad_sdl = false;
    }
    pad_numIgnored = 0;
    pad_layer = false;
    Com_Printf("pad: shut down\n");
}
