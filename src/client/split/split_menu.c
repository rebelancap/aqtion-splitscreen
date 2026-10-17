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

// split_menu.c -- splitscreen menus (design 7.1, M2-T4)
//
// - The per-player overlay (the Quake3e fork's design 12.2 / 13.3): Start on
//   a player's pad opens a small engine-drawn menu in that player's cell
//   only; d-pad + A / B navigate it; the game keeps running and that
//   player's pad does nothing else while it is up. Action's real choices are
//   server menus, so its first item just opens them (`p<N> inven`).
// - The on-screen keyboard (Q3 12.4): name entry from the overlay, and any
//   text field of the engine UI when player 1 presses A on it with a pad.
// - The host's splitscreen page (Q3 13.4): a page of player 1's overlay,
//   in P1's cell in game ("Splitscreen settings") and full screen at the
//   main menu (Start on P1's pad, or `padmenu 1`). AQtion's own menus get
//   no entry (the maintainer's rule); its items apply live.
// - The engine UI inside player 1's cell while N > 1 in game (ui.c asks
//   CL_SplitUIRect).
//
// Layout, wording and pad policy follow the Quake3e fork's cl_splitmenu.c.

#include "../client.h"
#include "gamepad.h"
#if USE_UI
#include "../ui/ui.h"
#endif

#define SM_MAX_ROWS     128         // a page longer than its panel scrolls (map list)
#define SM_MAX_DEPTH    5
#define SM_COLS         40          // panel width in characters (fewer in a narrow cell)
#define SM_NOTE_TIME    3000
#define SM_NAME_MAX     15          // Quake 2 player names

typedef enum {
    SMR_ACTION,     // A runs func
    SMR_CHOICE,     // left / right / A cycle the cvar through values[]
    SMR_SLIDER,     // left / right step the cvar between min and max
    SMR_SUB,        // A pushes page
    SMR_TEXT,       // static, never selected
    SMR_ENTRY,      // A edits the cvar on the on-screen keyboard
    SMR_FLAG,       // left / right / A toggle bit `arg' of the cvar (invert: set = Off)
    SMR_BIND        // left / right / A cycle pad key `cvar' through smBindActions
} smRowType_t;

typedef struct smPage_s {
    void        (*build)(int n);
    void        (*enter)(int n);    // when pushed (optional)
} smPage_t;

typedef struct {
    smRowType_t         type;
    char                label[48];
    char                cvar[MAX_QPATH];
    const char *const   *values;
    const char *const   *labels;
    int                 numValues;
    float               min, max, step;
    const char          *fmt;
    const smPage_t      *page;
    void                (*func)(int n, int arg);
    int                 arg;
    bool                invert;     // SMR_FLAG: the bit set means Off
    bool                disabled;   // shown dim, nothing changes (its help says why)
    char                help[72];   // footer line while selected
    void                (*changed)(int n);  // after a CHOICE / SLIDER / FLAG change
} smRow_t;

typedef struct {
    int         depth;              // 0 = no page up
    bool        mainMenu;           // player 1 out of a game: full screen, host page
    struct {
        const smPage_t  *page;
        int             sel, top;
    } stack[SM_MAX_DEPTH];

    char        title[64];
    smRow_t     rows[SM_MAX_ROWS];
    int         numRows;

    char        note[96];
    unsigned    noteTime;

    struct {                        // on-screen keyboard (Q3 12.4)
        bool    open;
        bool    uiField;            // edits the engine UI's focused text field (player 1)
        char    title[48];
        char    text[64];
        int     max;
        int     row, col;
        bool    shift;
        char    cvar[MAX_QPATH];    // done: set this cvar (if not uiField)
        void    (*done)(int n, const char *text);   // done: or call this
    } kb;
} smPlayer_t;

static smPlayer_t   sm[MAX_SPLITVIEW];
static smPlayer_t   *smb;           // the player whose page is being built
static smRow_t      smScratch;      // rows past SM_MAX_ROWS land here

static cvar_t       *cl_splitStartMap;
static cvar_t       *cl_splitMenuScale;    // our overlay's size (player 1; p<N>_ for the others)
static qhandle_t    sm_font;

static const smPage_t smPause, smRadio, smSettings, smLeave, smProfile, smHost;
static const smPage_t smTeam, smControls, smButtons, smServer, smSrvMaps, smWeapBans, smItemBans;
static const smPage_t smSrvSets, smSrvDelete;

static bool SM_P1Active(void);
static void SM_ApplyInfAmmo(void);
static void SM_DrawMainMenuHint(void);
static bool SM_LayoutUp(int n);

// developer 1: the overlay logs its moves (Com_DPrintf is compiled out of releases)
static bool SM_Dev(void)
{
    static cvar_t *dev;

    if (!dev)
        dev = Cvar_Get("developer", "0", 0);
    return dev->integer != 0;
}

static smPlayer_t *SM_Get(int n)
{
    return (n >= 0 && n < MAX_SPLITVIEW) ? &sm[n] : NULL;
}

bool CL_SplitMenuOpen(int n)
{
    const smPlayer_t *s = SM_Get(n);

    return s && (s->depth > 0 || s->kb.open);
}

// which page / keyboard player n's overlay shows (0 = closed): the pad
// layer's hold-repeat stops when it changes
uintptr_t CL_SplitMenuView(int n)
{
    const smPlayer_t *s = SM_Get(n);

    if (!s || !(s->depth > 0 || s->kb.open))
        return 0;
    if (s->kb.open)
        return 1;
    return (uintptr_t)s->stack[s->depth - 1].page ^ (uintptr_t)s->depth;
}

static void SM_Note(int n, const char *text)
{
    Q_strlcpy(sm[n].note, text, sizeof(sm[n].note));
    sm[n].noteTime = cls.realtime;
}

// commands from a menu key run next, ahead of whatever the buffer holds
// (one action per key press; inside `padinject` they follow it, as the
// pad layer's binds do)
static void SM_Exec(const char *text)
{
    Cbuf_InsertText(&cmd_buffer, text);
}

// a command as player n ("p<N> cmd": that player's input / server)
static void SM_Cmd(int n, const char *cmd)
{
    Com_Printf("P%d menu: %s\n", n + 1, cmd);
    SM_Exec(va("p%d %s", n + 1, cmd));
}

// the cvar that holds player n's value of a per-player setting: the shared
// one for player 1, p<N>_<name> for the others (created from the shared
// value if missing; the pad layer reads p<N>_joy_* when it exists)
static const char *SM_Var(int n, const char *name)
{
    static char buf[MAX_QPATH];
    cvar_t *base;

    if (n == 0)
        return name;
    Q_snprintf(buf, sizeof(buf), "p%d_%s", n + 1, name);
    if (!Cvar_FindVar(buf)) {
        base = Cvar_FindVar(name);
        Cvar_Get(buf, base ? base->string : "0", 0);
    }
    return buf;
}

// player n's value of a per-player setting: p<N>_<name> if it exists, else
// the shared cvar (player 1's)
static float SM_PlayerValue(int n, const char *name)
{
    const cvar_t *v = NULL;

    if (n > 0)
        v = Cvar_FindVar(va("p%d_%s", n + 1, name));
    if (!v)
        v = Cvar_FindVar(name);
    return v ? v->value : 0;
}

/*
===============================================================================

ROWS

===============================================================================
*/

static smRow_t *SM_Row(smRowType_t type, const char *label)
{
    smRow_t *r = smb->numRows < SM_MAX_ROWS ? &smb->rows[smb->numRows++] : &smScratch;

    memset(r, 0, sizeof(*r));
    r->type = type;
    Q_strlcpy(r->label, label, sizeof(r->label));
    return r;
}

static void SM_Title(const char *title)
{
    Q_strlcpy(smb->title, title, sizeof(smb->title));
}

static smRow_t *SM_Action(const char *label, void (*func)(int n, int arg), int arg)
{
    smRow_t *r = SM_Row(SMR_ACTION, label);

    r->func = func;
    r->arg = arg;
    return r;
}

static smRow_t *SM_Sub(const char *label, const smPage_t *page)
{
    smRow_t *r = SM_Row(SMR_SUB, label);

    r->page = page;
    return r;
}

static smRow_t *SM_Text(const char *text)
{
    return SM_Row(SMR_TEXT, text);
}

static smRow_t *SM_Choice(const char *label, const char *cvar, const char *const *values,
                          const char *const *labels, int num)
{
    smRow_t *r = SM_Row(SMR_CHOICE, label);

    Q_strlcpy(r->cvar, cvar, sizeof(r->cvar));
    r->values = values;
    r->labels = labels;
    r->numValues = num;
    return r;
}

static smRow_t *SM_Slider(const char *label, const char *cvar, float min, float max, float step, const char *fmt)
{
    smRow_t *r = SM_Row(SMR_SLIDER, label);

    Q_strlcpy(r->cvar, cvar, sizeof(r->cvar));
    r->min = min;
    r->max = max;
    r->step = step;
    r->fmt = fmt;
    return r;
}

static smRow_t *SM_Entry(const char *label, const char *cvar)
{
    smRow_t *r = SM_Row(SMR_ENTRY, label);

    Q_strlcpy(r->cvar, cvar, sizeof(r->cvar));
    return r;
}

static smRow_t *SM_Flag(const char *label, const char *cvar, int bit, bool invert)
{
    smRow_t *r = SM_Row(SMR_FLAG, label);

    Q_strlcpy(r->cvar, cvar, sizeof(r->cvar));
    r->arg = bit;
    r->invert = invert;
    return r;
}

static smRow_t *SM_Help(smRow_t *r, const char *help)
{
    Q_strlcpy(r->help, help, sizeof(r->help));
    return r;
}

static bool SM_Selectable(const smRow_t *r)
{
    return r->type != SMR_TEXT;
}

// the top page's rows, rebuilt before every use (values follow the cvars)
static void SM_Build(int n)
{
    smPlayer_t *s = &sm[n];
    int *sel, i;

    s->numRows = 0;
    s->title[0] = 0;
    if (s->depth <= 0)
        return;
    smb = s;
    s->stack[s->depth - 1].page->build(n);
    smb = NULL;

    // the selection on a selectable row
    sel = &s->stack[s->depth - 1].sel;
    if (*sel >= s->numRows)
        *sel = s->numRows - 1;
    if (*sel < 0)
        *sel = 0;
    for (i = 0; i < s->numRows && !SM_Selectable(&s->rows[*sel]); i++)
        *sel = (*sel + 1) % s->numRows;
}

/*
The actions a pad button can get on the Buttons page (Action's commands,
its autoexec_example.cfg; the default set in in_gamepad.c uses the same
strings, so a default bind shows its name). "" = nothing.
*/
static const struct {
    const char  *cmd;
    const char  *label;
} smBindActions[] = {
    { "",                   "--" },
    { "+attack",            "Fire" },
    { "+moveup",            "Jump" },
    { "+movedown",          "Crouch" },
    { "reload",             "Reload" },
    { "bandage",            "Bandage" },
    { "lens",               "Zoom (cycle)" },      // 1x 2x 4x 6x, then back to 1x
    { "lens in",            "Zoom in" },           // stops at 6x
    { "lens out",           "Zoom out" },
    { "lens 1",             "Reset zoom" },
    { "weapon",             "Weapon mode" },
    { "weapnext",           "Next weapon" },
    { "weapprev",           "Previous weapon" },
    { "weaplast",           "Last weapon" },
    { "use Combat Knife",   "Knife" },
    { "use MK23 Pistol",    "Pistol" },
    { "use M26 Fragmentation Grenade", "Grenade" },
    { "drop item",          "Drop item" },
    { "drop weapon",        "Drop weapon" },
    { "irvision",           "IR vision" },
    { "flashlight",         "Flashlight" },
    { "opendoor",           "Use / open door" },
    { "+speed",             "Walk" },
    { "score",              "Scoreboard" },
    { "inven",              "Action menu (team, weapons)" },
    { "padmenu",            "This menu" },
    { "padmenu radio",      "Radio menu" },
    { "radio enemys",       "Radio: enemy spotted" },
    { "radio enemyd",       "Radio: enemy down" },
    { "radio cover",        "Radio: cover me" },
    { "radio taking_f",     "Radio: taking fire" },
    { "radio reportin",     "Radio: report in" },
    { "radio im_hit",       "Radio: I'm hit" },
    { "radio go",           "Radio: go" },
    { "radio back",         "Radio: back" },
    { "centerview",         "Center view" },
};

static int SM_BindIndex(const char *cmd)
{
    int i;

    for (i = 0; i < q_countof(smBindActions); i++) {
        if (!Q_stricmp(cmd, smBindActions[i].cmd))
            return i;
    }
    return -1;
}

static int SM_ChoiceIndex(const smRow_t *r)
{
    const cvar_t *var = Cvar_FindVar(r->cvar);
    const char *v;
    float f;
    int i;

    // a cvar nobody created yet (teamplay before the game DLL ran): the
    // first value, which is its default
    if (!var)
        return r->numValues ? 0 : -1;
    // a latched game cvar shows what the next map gets
    v = var->latched_string ? var->latched_string : var->string;
    f = Q_atof(v);
    for (i = 0; i < r->numValues; i++) {
        if (!Q_stricmp(v, r->values[i]))
            return i;
    }
    // numbers written another way ("1.0" for "1"); never for map names
    // (a map named "0..." or any name, Q_atof 0, would match)
    if (!Q_stricmp(r->cvar, "cl_splitStartMap"))
        return -1;
    for (i = 0; i < r->numValues; i++) {
        if (Q_atof(r->values[i]) == f && (Q_isdigit(*v) || *v == '-'))
            return i;
    }
    return -1;
}

static void SM_RowValue(const smRow_t *r, char *buf, size_t size)
{
    int i;

    buf[0] = 0;
    switch (r->type) {
    case SMR_CHOICE:
        i = SM_ChoiceIndex(r);
        // arg 1: a value not in the list is "Custom" (the Weapons preset row)
        Q_strlcpy(buf, i >= 0 ? r->labels[i] : r->arg == 1 ? "Custom" : Cvar_VariableString(r->cvar), size);
        break;
    case SMR_SLIDER:
        Q_snprintf(buf, size, r->fmt, Cvar_VariableValue(r->cvar));
        break;
    case SMR_ENTRY:
        Q_strlcpy(buf, Cvar_VariableString(r->cvar), size);
        break;
    case SMR_SUB:
        Q_strlcpy(buf, ">", size);
        break;
    case SMR_FLAG:
        i = (Cvar_VariableInteger(r->cvar) & r->arg) != 0;
        Q_strlcpy(buf, (i != r->invert) ? "On" : "Off", size);
        break;
    case SMR_BIND:
        i = SM_BindIndex(PAD_GetBind(r->arg, r->cvar));
        Q_strlcpy(buf, i >= 0 ? smBindActions[i].label : PAD_GetBind(r->arg, r->cvar), size);
        break;
    default:
        break;
    }
}

static void SM_Change(int n, const smRow_t *r, int dir)
{
    float v;
    int i;

    if (r->disabled)
        return;
    switch (r->type) {
    case SMR_FLAG:
        i = Cvar_VariableInteger(r->cvar) ^ r->arg;
        Cvar_SetEx(r->cvar, va("%d", i), FROM_CONSOLE);
        Com_Printf("P%d menu: %s = %d\n", n + 1, r->cvar, i);
        break;
    case SMR_BIND:
        // r->arg: the player; r->cvar: the pad key
        PAD_MenuEnsureBinds(r->arg);
        i = SM_BindIndex(PAD_GetBind(r->arg, r->cvar));
        i = (i < 0) ? 0 : (i + dir + q_countof(smBindActions)) % q_countof(smBindActions);
        PAD_MenuSetBind(r->arg, r->cvar, smBindActions[i].cmd);
        Com_Printf("P%d menu: pbind %d %s \"%s\"\n", n + 1, r->arg + 1, r->cvar, smBindActions[i].cmd);
        break;
    case SMR_CHOICE:
        i = SM_ChoiceIndex(r);
        i = (i < 0) ? 0 : (i + dir + r->numValues) % r->numValues;
        Cvar_SetEx(r->cvar, r->values[i], FROM_CONSOLE);   // FROM_CODE skips ->changed (userinfo)
        Com_Printf("P%d menu: %s = \"%s\"\n", n + 1, r->cvar, r->values[i]);
        break;
    case SMR_SLIDER:
        v = Cvar_VariableValue(r->cvar) + dir * r->step;
        v = Q_clipf(v, r->min, r->max);
        Cvar_SetEx(r->cvar, va("%g", v), FROM_CONSOLE);
        Com_Printf("P%d menu: %s = %g\n", n + 1, r->cvar, v);
        break;
    default:
        return;
    }
    if (r->changed)
        r->changed(n);
}

/*
===============================================================================

OPEN / CLOSE / NAVIGATE

===============================================================================
*/

static void SM_Close(int n)
{
    smPlayer_t *s = &sm[n];

    if (s->depth > 0 || s->kb.open)
        Com_DPrintf("P%d menu: closed\n", n + 1);
    s->depth = 0;
    memset(&s->kb, 0, sizeof(s->kb));
}

static void SM_Push(int n, const smPage_t *page)
{
    smPlayer_t *s = &sm[n];

    if (s->depth >= SM_MAX_DEPTH)
        return;
    s->stack[s->depth].page = page;
    s->stack[s->depth].sel = 0;
    s->stack[s->depth].top = 0;
    s->depth++;
    if (page->enter)
        page->enter(n);
}

static void SM_Back(int n)
{
    smPlayer_t *s = &sm[n];

    if (s->depth > 0)
        s->depth--;
}

// page: a page to open over the first one (a `padmenu radio' bind), or NULL
static void SM_Open(int n, const smPage_t *page)
{
    smPlayer_t *s = &sm[n];

    SM_Close(n);
    PAD_ReleasePlayer(n);       // held buttons let go: the menu takes the pad
    // player 1 out of a game: the host page full screen (Q3 13.4)
    s->mainMenu = n == 0 && !SM_P1Active();
    // a server layout (Action's menu, scoreboard) goes away first, so the
    // overlay is not drawn over it (acceptance note 4)
    if (!s->mainMenu && SM_LayoutUp(n))
        SM_Exec(va("p%d putaway\n", n + 1));
    SM_Push(n, s->mainMenu ? &smHost : &smPause);
    if (page)
        SM_Push(n, page);
    s->note[0] = 0;
    Com_Printf("P%d menu: opened%s\n", n + 1, s->mainMenu ? " (main menu: splitscreen settings)" : "");
}

static void SM_OpenKeyboard(int n, const char *title, const char *text, int max, const char *cvar, bool uiField)
{
    smPlayer_t *s = &sm[n];

    memset(&s->kb, 0, sizeof(s->kb));
    s->kb.open = true;
    s->kb.uiField = uiField;
    s->kb.max = min(max, (int)sizeof(s->kb.text) - 1);
    Q_strlcpy(s->kb.title, title, sizeof(s->kb.title));
    Q_strlcpy(s->kb.text, text, s->kb.max + 1);
    if (cvar)
        Q_strlcpy(s->kb.cvar, cvar, sizeof(s->kb.cvar));
    s->kb.row = 1;      // on 'q'
    Com_Printf("P%d keyboard: \"%s\"\n", n + 1, s->kb.title);
}

static void SM_Activate(int n, const smRow_t *r)
{
    if (r->disabled) {
        if (r->help[0])
            SM_Note(n, r->help);
        return;
    }
    switch (r->type) {
    case SMR_FLAG:
    case SMR_BIND:
        SM_Change(n, r, 1);
        break;
    case SMR_ACTION:
        if (r->func)
            r->func(n, r->arg);
        break;
    case SMR_SUB:
        SM_Push(n, r->page);
        break;
    case SMR_CHOICE:
        SM_Change(n, r, 1);
        break;
    case SMR_ENTRY:
        SM_OpenKeyboard(n, va("Player %d: %s", n + 1, r->label), Cvar_VariableString(r->cvar),
                        SM_NAME_MAX, r->cvar, false);
        break;
    default:
        break;
    }
}

/*
===============================================================================

ON-SCREEN KEYBOARD (Q3 12.4)

A grid of keys in the owner's cell: d-pad moves, A types the key, X deletes,
Y types a space, L3 or the Shift key switches case, Start is done, B cancels.

===============================================================================
*/

#define KB_ROWS     5
#define KB_COLS     10
#define KB_DEL      1
#define KB_SHIFT    2
#define KB_DONE     3

// every row is KB_COLS columns; a key spanning columns repeats its code
static const char *const smKbRows[KB_ROWS] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl-",
    "zxcvbnm_\001\001",
    "\002\002\002    \003\003\003"
};

static int SM_KbCode(int row, int col)
{
    return (unsigned char)smKbRows[row][col];
}

// nothing but spaces
static bool SM_Blank(const char *s)
{
    while (*s == ' ')
        s++;
    return !*s;
}

static void SM_KbDone(int n)
{
    smPlayer_t *s = &sm[n];
    void (*done)(int n, const char *text) = s->kb.done;
    char text[sizeof(s->kb.text)];

    Com_Printf("P%d keyboard: done \"%s\"\n", n + 1, s->kb.text);
    Q_strlcpy(text, s->kb.text, sizeof(text));
#if USE_UI
    if (s->kb.uiField) {
        UI_SetFocusedField(text);
        memset(&s->kb, 0, sizeof(s->kb));
        return;
    }
#endif
    // names (the only cvar entries) and profile names: never empty
    if (SM_Blank(text) && (s->kb.cvar[0] || done)) {
        memset(&s->kb, 0, sizeof(s->kb));
        SM_Note(n, "Empty name: nothing changed");
        return;
    }
    if (s->kb.cvar[0])
        Cvar_SetEx(s->kb.cvar, text, FROM_CONSOLE);
    memset(&s->kb, 0, sizeof(s->kb));
    if (done)
        done(n, text);
}

static void SM_KbPress(int n, int code)
{
    smPlayer_t *s = &sm[n];
    int len = strlen(s->kb.text);

    switch (code) {
    case KB_DEL:
        if (len > 0)
            s->kb.text[len - 1] = 0;
        break;
    case KB_SHIFT:
        s->kb.shift = !s->kb.shift;
        break;
    case KB_DONE:
        SM_KbDone(n);
        return;
    default:
        if (s->kb.shift && code >= 'a' && code <= 'z')
            code += 'A' - 'a';
        if (len >= s->kb.max) {
            SM_Note(n, va("At most %d characters", s->kb.max));
            return;
        }
        s->kb.text[len] = code;
        s->kb.text[len + 1] = 0;
        break;
    }
    Com_DPrintf("P%d keyboard: \"%s\"%s\n", n + 1, s->kb.text, s->kb.shift ? " (shift)" : "");
}

static void SM_KeyboardKey(int n, int key)
{
    smPlayer_t *s = &sm[n];
    int code, i;

    switch (key) {
    case SMK_UP:
    case SMK_DOWN:
        s->kb.row = (s->kb.row + (key == SMK_UP ? KB_ROWS - 1 : 1)) % KB_ROWS;
        if (SM_Dev())
            Com_Printf("P%d keyboard: row %d\n", n + 1, s->kb.row + 1);
        break;
    case SMK_LEFT:
    case SMK_RIGHT:
        // skip the rest of a wide key, land on the first column of the next
        code = SM_KbCode(s->kb.row, s->kb.col);
        for (i = 0; i < KB_COLS; i++) {
            s->kb.col = (s->kb.col + (key == SMK_LEFT ? KB_COLS - 1 : 1)) % KB_COLS;
            if (SM_KbCode(s->kb.row, s->kb.col) != code)
                break;
        }
        code = SM_KbCode(s->kb.row, s->kb.col);
        while (s->kb.col > 0 && SM_KbCode(s->kb.row, s->kb.col - 1) == code)
            s->kb.col--;
        if (SM_Dev())
            Com_Printf("P%d keyboard: column %d\n", n + 1, s->kb.col + 1);
        break;
    case SMK_A:
        SM_KbPress(n, SM_KbCode(s->kb.row, s->kb.col));
        break;
    case SMK_X:
        SM_KbPress(n, KB_DEL);
        break;
    case SMK_Y:
        SM_KbPress(n, ' ');
        break;
    case SMK_L3:
        SM_KbPress(n, KB_SHIFT);
        break;
    case SMK_START:
        SM_KbPress(n, KB_DONE);
        break;
    case SMK_B:
        Com_Printf("P%d keyboard: cancelled\n", n + 1);
        memset(&s->kb, 0, sizeof(s->kb));
        break;
    }
}

/*
====================
CL_SplitMenuKey

A key from player n's pad while its overlay or keyboard is up (the pad
layer sends every key there; nothing reaches the player's binds).
====================
*/
void CL_SplitMenuKey(int n, int key)
{
    smPlayer_t *s = SM_Get(n);
    int *sel, i, h;

    if (!s)
        return;
    if (s->kb.open) {
        SM_KeyboardKey(n, key);
        return;
    }
    if (s->depth <= 0)
        return;

    SM_Build(n);
    sel = &s->stack[s->depth - 1].sel;

    switch (key) {
    case SMK_UP:
    case SMK_DOWN:
        for (i = 1; i <= s->numRows; i++) {
            h = (*sel + (key == SMK_UP ? -i : i) + s->numRows * 2) % s->numRows;
            if (SM_Selectable(&s->rows[h])) {
                *sel = h;
                break;
            }
        }
        if (SM_Dev())
            Com_Printf("P%d menu: %s row %d of %d\n", n + 1, s->title, *sel + 1, s->numRows);
        break;
    case SMK_LEFT:
    case SMK_RIGHT:
        if (*sel < s->numRows)
            SM_Change(n, &s->rows[*sel], key == SMK_LEFT ? -1 : 1);
        break;
    case SMK_A:
        if (*sel < s->numRows)
            SM_Activate(n, &s->rows[*sel]);
        break;
    case SMK_B:
        SM_Back(n);
        break;
    case SMK_START:
        SM_Close(n);
        break;
    default:
        break;
    }
}

/*
===============================================================================

PAGES

===============================================================================
*/

static const char *const smOffOn[] = { "Off", "On" };
static const char *const sm01[] = { "0", "1" };
static const char *const smHandValues[] = { "0", "1", "2" };
static const char *const smHandLabels[] = { "Right", "Left", "Hidden" };

/*
===============================================================================

THE GAME PLAYER 1 HOSTS

Player 1's own listen server: the game DLL's cvars are this process's, so
the menus read the running game from them (Team / Weapons rows) and the
Server options page sets them. On another machine's server nothing is
known (Q2 servers send no serverinfo to clients): both rows show and the
Server options page is host only.

===============================================================================
*/

static cvar_t       *cl_splitGameType;
static cvar_t       *cl_splitBots;

static const netadr_t *SM_P1Address(void)
{
    const client_context_t *p1 = clx[0];

    return p1 == cla ? &cls.serverAddress : &p1->conn.serverAddress;
}

// player 1 plays on its own listen server
static bool SM_Hosting(void)
{
    return sv_running->integer && SM_P1Active() && SM_P1Address()->type == NA_LOOPBACK;
}

// player 1 plays on another machine's server
static bool SM_Remote(void)
{
    return SM_P1Active() && SM_P1Address()->type != NA_LOOPBACK;
}

static int SM_GameInt(const char *name)
{
    return Cvar_VariableInteger(name);
}

// Action's GS_WEAPONCHOOSE (g_spawn.c, the same in Steam's TNG 2.82): the
// running game has a weapon / item menu; unknown (not hosting) = yes
static bool SM_WeaponChoice(void)
{
    if (!SM_Hosting())
        return true;
    if (SM_GameInt("jump"))
        return false;
    if (SM_GameInt("ctf") || SM_GameInt("esp") || SM_GameInt("dom"))
        return true;
    if (SM_GameInt("teamdm"))
        return SM_GameInt("dm_choose") != 0;
    if (SM_GameInt("use_3teams") || SM_GameInt("matchmode") || SM_GameInt("use_tourney") ||
        SM_GameInt("teamplay"))
        return true;
    return SM_GameInt("dm_choose") != 0;
}

// teams to join: a team game that is not a tourney (its `team' is off)
static bool SM_TeamChoice(void)
{
    return !SM_Hosting() || (SM_GameInt("teamplay") && !SM_GameInt("use_tourney"));
}

/*
Game types of Steam's TNG 2.82 (its g_spawn.c checks, in that order: ctf,
dom, teamdm, use_3teams, use_tourney, teamplay); espionage (`esp') is not
in that DLL. cl_splitGameType is ours (archived); its values go into the
game cvars when a game starts from the overlay and when the row changes in
a hosted game (latched: next map / restart).
*/
static const char *const smGameTypeValues[] = { "dm", "tdm", "tp", "ctf", "3t", "tourney", "dom" };
static const char *const smGameTypeLabels[] = { "Deathmatch", "Team deathmatch", "Teamplay (rounds)",
                                                "Capture the flag", "3 teams (rounds)", "Tourney",
                                                "Domination" };

// a game cvar to value, latched while a server runs ("next game")
static void SM_SetGameCvar(const char *name, const char *value)
{
    const cvar_t *v = Cvar_FindVar(name);
    const char *cur = v ? (v->latched_string ? v->latched_string : v->string) : NULL;

    if (cur && !strcmp(cur, value))
        return;
    Cvar_SetEx(name, value, FROM_CONSOLE);
}

static void SM_ApplyGameType(void)
{
    const char *gt = cl_splitGameType->string;

    SM_SetGameCvar("teamplay", strcmp(gt, "dm") ? "1" : "0");
    SM_SetGameCvar("teamdm", !strcmp(gt, "tdm") ? "1" : "0");
    SM_SetGameCvar("ctf", !strcmp(gt, "ctf") ? "1" : "0");
    SM_SetGameCvar("use_3teams", !strcmp(gt, "3t") ? "1" : "0");
    SM_SetGameCvar("use_tourney", !strcmp(gt, "tourney") ? "1" : "0");
    SM_SetGameCvar("dom", !strcmp(gt, "dom") ? "1" : "0");
}

// a game cvar's value for the next game (the latched one if any)
static int SM_NextInt(const char *name)
{
    const cvar_t *v = Cvar_FindVar(name);

    return v ? Q_atoi(v->latched_string ? v->latched_string : v->string) : 0;
}

// the running (or next) game's type, back into cl_splitGameType (a game
// started from AQtion's own pages or the console)
static void SM_DetectGameType(void)
{
    const char *gt;

    if (SM_NextInt("ctf"))
        gt = "ctf";
    else if (SM_NextInt("dom"))
        gt = "dom";
    else if (SM_NextInt("teamdm"))
        gt = "tdm";
    else if (SM_NextInt("use_3teams"))
        gt = "3t";
    else if (SM_NextInt("use_tourney"))
        gt = "tourney";
    else if (SM_NextInt("teamplay"))
        gt = "tp";
    else
        gt = "dm";
    Cvar_SetByVar(cl_splitGameType, gt, FROM_CODE);
}

// bots: Steam's DLL fills a game with am 2 + am_botcount at map start
// (live changes do not add or remove them reliably: next map / restart)
static void SM_ApplyBots(void)
{
    int b = cl_splitBots->integer;

    if (b > 0)
        SM_SetGameCvar("am_botcount", va("%d", b));
    // ltk_loadbots (game default 1) also spawns the bot list of
    // action\bots\<map>.cfg; since the working directory is the AQtion root
    // (0.0.0.22) those files are found, so "Bots: None" must turn it off
    SM_SetGameCvar("ltk_loadbots", b > 0 ? "1" : "0");
    SM_SetGameCvar("am", b > 0 ? "2" : "0");
}

static void SM_DetectBots(void)
{
    Cvar_SetByVar(cl_splitBots, SM_NextInt("am") ? va("%d", SM_NextInt("am_botcount")) : "0", FROM_CODE);
}

/*
A game cvar of the Server options page: created with the game's default if
no game ran yet (the DLL keeps the value when it registers it) and archived,
so the settings stay for the next session. Returns the name.
*/
static const char *SM_GameVar(const char *name, const char *def)
{
    cvar_t *v = Cvar_FindVar(name);

    if (!v)
        v = Cvar_Get(name, def, 0);
    // created empty before any game ran (AQtion's own menu scripts do that
    // to ltk_skill): the row would show a blank value; use the game default
    else if (!*v->string && *def && !sv_running->integer)
        Cvar_SetByVar(v, def, FROM_CODE);
    if (v && !(v->flags & CVAR_NOARCHIVEMASK))
        v->flags |= CVAR_ARCHIVE;
    return name;
}

// the maps the filesystem has (what the engine menu's $$com_maplist lists),
// read when a page with a map row is entered
static char         **smMaps;
static int          smNumMaps;

// a map name that resolves to maps/<name>.bsp on the search path (paks too)
static bool SM_MapInstalled(const char *map)
{
    if (!*map || strpbrk(map, "\";\n\r") || strlen(map) >= MAX_QPATH - 9)
        return false;
    return FS_FileExists(va("maps/%s.bsp", map));
}

// wizs if the install has it, else its first map ("" if none)
static const char *SM_DefaultMap(void)
{
    static char first[MAX_QPATH];
    void **list;
    int n;

    if (SM_MapInstalled("wizs"))
        return "wizs";
    list = FS_ListFiles("maps", ".bsp", FS_SEARCH_STRIPEXT, &n);
    first[0] = 0;
    if (list && n > 0)
        Q_strlcpy(first, list[0], sizeof(first));
    if (list)
        FS_FreeList(list);
    return first;
}

static void SM_ReadMaps(void)
{
    if (smMaps)
        FS_FreeList((void **)smMaps);
    smMaps = (char **)FS_ListFiles("maps", ".bsp", FS_SEARCH_STRIPEXT, &smNumMaps);
    if (!smMaps)
        smNumMaps = 0;
    // the Map row only offers installed maps; a saved one that is gone
    // becomes one that exists
    if (smNumMaps > 0 && !SM_MapInstalled(cl_splitStartMap->string))
        Cvar_SetByVar(cl_splitStartMap, SM_DefaultMap(), FROM_CODE);
}

/*
===============================================================================

PAGES

===============================================================================
*/

static const char *SM_ProfileLabel(int n)
{
    // the maintainer's wording (0.0.0.25): always "Profile: <who>"
    return va("Profile: %s", CL_ProfileNamed(n) ? CL_ProfileName(n) : "Guest");
}

// --- pause page (the order the maintainer asked for, 0.0.0.23) ---

static void SM_Weapons(int n, int arg)
{
    SM_Close(n);
    SM_Cmd(n, "inven");     // Action's weapon / item menu (its join menu first if on no team)
}

static void SM_Score(int n, int arg)
{
    SM_Close(n);
    SM_Cmd(n, "score");
}

static void SM_GameMenu(int n, int arg)
{
    SM_Close(n);
    UI_OpenMenu(UIMENU_GAME);
}

static void SM_BuildPause(int n)
{
    SM_Title(va("Player %d", n + 1));
    if (SM_TeamChoice())
        SM_Sub("Team", &smTeam);
    if (SM_WeaponChoice())
        SM_Help(SM_Action("Weapons", SM_Weapons, 0), "Action's weapon and item menu");
    SM_Action("Scoreboard", SM_Score, 0);
    SM_Sub("Radio", &smRadio);
    SM_Sub("Settings", &smSettings);
    SM_Sub(SM_ProfileLabel(n), &smProfile);
    if (n == 0) {
        if (SM_Remote())
            SM_Help(SM_Sub("Server options - host only", &smServer),
                    "Only for games you host")->disabled = true;
        else
            SM_Sub("Server options", &smServer);
        SM_Sub("Splitscreen settings", &smHost);
        SM_Action("Game menu", SM_GameMenu, 0);
    }
    SM_Sub(n == 0 ? "End game" : "Leave game", &smLeave);
}

static const smPage_t smPause = { SM_BuildPause };

// --- team (Action's `team <1|2|3|none>'; its join menu is `inven' with no team) ---

static void SM_JoinTeam(int n, int arg)
{
    SM_Close(n);
    if (arg < 0)
        SM_Cmd(n, "inven");
    else if (arg == 0)
        SM_Cmd(n, "team none");
    else
        SM_Cmd(n, va("team %d", arg));
}

static void SM_BuildTeam(int n)
{
    const bool ctf = SM_Hosting() && SM_GameInt("ctf");

    SM_Title(va("Player %d - Team", n + 1));
    SM_Help(SM_Action(ctf ? "Join red team" : "Join team 1", SM_JoinTeam, 1),
            "Switching team again waits 5 s");
    SM_Help(SM_Action(ctf ? "Join blue team" : "Join team 2", SM_JoinTeam, 2),
            "Switching team again waits 5 s");
    if (SM_Hosting() && SM_GameInt("use_3teams"))
        SM_Help(SM_Action("Join team 3", SM_JoinTeam, 3), "Switching team again waits 5 s");
    SM_Help(SM_Action("Spectate", SM_JoinTeam, 0), "Leave your team and watch");
    SM_Help(SM_Action("Action's join menu", SM_JoinTeam, -1),
            "Auto-join, MOTD, votes (when on no team)");
}

static const smPage_t smTeam = { SM_BuildTeam };

// --- radio (Action's `radio <message>', its a_radio.c names) ---

static const struct {
    const char  *msg;
    const char  *label;
} smRadioMsgs[] = {
    { "enemys", "Enemy spotted" },
    { "enemyd", "Enemy down" },
    { "cover", "Cover me" },
    { "taking_f", "Taking fire" },
    { "im_hit", "I'm hit" },
    { "teamdown", "Teammate down" },
    { "reportin", "Report in" },
    { "treport", "Team report" },
    { "go", "Go" },
    { "forward", "Forward" },
    { "back", "Back" },
    { "left", "Left" },
    { "right", "Right" },
    { "up", "Up" },
    { "down", "Down" },
};

static void SM_SendRadio(int n, int arg)
{
    SM_Close(n);
    SM_Cmd(n, va("radio %s", smRadioMsgs[arg].msg));
}

static void SM_BuildRadio(int n)
{
    int i;

    SM_Title(va("Player %d - Radio", n + 1));
    for (i = 0; i < q_countof(smRadioMsgs); i++)
        SM_Action(smRadioMsgs[i].label, SM_SendRadio, i);
}

static const smPage_t smRadio = { SM_BuildRadio };

// --- settings: player n's own (p<N>_* shadows; player 1's shared cvars) ---

static const char *const smCrossValues[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                             "11", "12", "13", "14", "15", "16", "17", "18", "19", "20" };
static const char *const smCrossLabels[] = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                             "11", "12", "13", "14", "15", "16", "17", "18", "19", "20" };
// crosshair size: ch_size (ours, screen.c) times upstream's ch_scale picture
// size; 1x = the size before 0.0.0.25, default 3x
static const char *const smChSizeValues[] = { "1", "1.5", "2", "2.5", "3", "3.5", "4" };
static const char *const smChSizeLabels[] = { "1x", "1.5x", "2x", "2.5x", "3x", "3.5x", "4x" };
// chat / kill / centre message size in a split view (cl_splitTextSize, screen.c)
static const char *const smTextSizeValues[] = { "1", "1.5", "2", "2.5", "3" };
static const char *const smTextSizeLabels[] = { "1x (HUD size)", "1.5x", "2x", "2.5x", "3x" };
static const char *const smMenuSizeValues[] = { "1", "1.5", "2", "2.5", "3" };
static const char *const smMenuSizeLabels[] = { "1x (AQtion's own)", "1.5x", "2x", "2.5x", "3x" };
static const char *const smMenuScaleValues[] = { "0.75", "1", "1.25", "1.5", "2" };
static const char *const smMenuScaleLabels[] = { "0.75x", "1x", "1.25x", "1.5x", "2x" };
static const char *const smAlphaValues[] = { "0.25", "0.5", "0.75", "1" };
static const char *const smAlphaLabels[] = { "25%", "50%", "75%", "100%" };

// crosshair colours: ch_colour (ours) names one, the engine's ch_red /
// ch_green / ch_blue carry it (p<N>_ in that player's cell, screen.c)
static const struct {
    const char  *name;
    const char  *label;
    const char  *rgb[3];
} smCrossColours[] = {
    { "white",   "White",   { "1", "1", "1" } },
    { "red",     "Red",     { "1", "0", "0" } },
    { "green",   "Green",   { "0", "1", "0" } },
    { "yellow",  "Yellow",  { "1", "1", "0" } },
    { "cyan",    "Cyan",    { "0", "1", "1" } },
    { "blue",    "Blue",    { "0.3", "0.5", "1" } },
    { "magenta", "Magenta", { "1", "0", "1" } },
};
static const char *smCrossColourValues[q_countof(smCrossColours)];
static const char *smCrossColourLabels[q_countof(smCrossColours)];

// player 1's HUD look is the engine's shared cvars (not archived upstream):
// kept in the config once changed here
static void SM_KeepP1(const char *name)
{
    cvar_t *v = Cvar_FindVar(name);

    if (v && !(v->flags & CVAR_NOARCHIVEMASK))
        v->flags |= CVAR_ARCHIVE;
}

static void SM_HudAlphaChanged(int n)
{
    if (n == 0)
        SM_KeepP1("scr_alpha");
}

static void SM_CrossColourChanged(int n)
{
    char var[MAX_QPATH];
    const char *c;
    int i;

    Q_strlcpy(var, SM_Var(n, "ch_colour"), sizeof(var));
    c = Cvar_VariableString(var);
    for (i = 0; i < q_countof(smCrossColours); i++) {
        if (!Q_stricmp(c, smCrossColours[i].name)) {
            Cvar_SetEx(SM_Var(n, "ch_red"), smCrossColours[i].rgb[0], FROM_CONSOLE);
            Cvar_SetEx(SM_Var(n, "ch_green"), smCrossColours[i].rgb[1], FROM_CONSOLE);
            Cvar_SetEx(SM_Var(n, "ch_blue"), smCrossColours[i].rgb[2], FROM_CONSOLE);
            if (n == 0) {
                SM_KeepP1("ch_red");
                SM_KeepP1("ch_green");
                SM_KeepP1("ch_blue");
            }
            return;
        }
    }
}

static void SM_BuildSettings(int n)
{
    int i;

    for (i = 0; i < q_countof(smCrossColours); i++) {
        smCrossColourValues[i] = smCrossColours[i].name;
        smCrossColourLabels[i] = smCrossColours[i].label;
    }
    SM_Title(va("Player %d - Settings", n + 1));
    SM_Entry("Name", SM_Var(n, "name"));
    SM_Choice("Gun hand", SM_Var(n, "hand"), smHandValues, smHandLabels, 3);
    // the pad layer's settings exist only in a build with pads
    if (Cvar_FindVar("joy_yawSpeed"))
        SM_Help(SM_Sub("Controls", &smControls), "Look speed, dead zone, buttons");
    // drawn per cell from p<N>_scr_alpha / p<N>_crosshair / p<N>_ch_* /
    // p<N>_cl_splitTextSize (screen.c); the menu sizes per owner (ui.c, below)
    SM_Help(SM_Choice("HUD opacity", SM_Var(n, "scr_alpha"), smAlphaValues, smAlphaLabels,
                      q_countof(smAlphaValues)), "Your HUD in your view")->changed = SM_HudAlphaChanged;
    SM_Help(SM_Choice("Crosshair", SM_Var(n, "crosshair"), smCrossValues, smCrossLabels,
                      q_countof(smCrossValues)), "Your crosshair picture in your view");
    SM_Help(SM_Choice("Crosshair size", SM_Var(n, "ch_size"), smChSizeValues, smChSizeLabels,
                      q_countof(smChSizeValues)), "Your crosshair in your view");
    SM_Help(SM_Choice("Crosshair colour", SM_Var(n, "ch_colour"), smCrossColourValues, smCrossColourLabels,
                      q_countof(smCrossColours)), "Your crosshair in your view")->changed = SM_CrossColourChanged;
    SM_Help(SM_Choice("Text size", SM_Var(n, "cl_splitTextSize"), smTextSizeValues, smTextSizeLabels,
                      q_countof(smTextSizeValues)), "Chat, kill and centre messages in your view");
    SM_Help(SM_Choice("AQtion menu size", SM_Var(n, "cl_splitMenuSize"), smMenuSizeValues, smMenuSizeLabels,
                      q_countof(smMenuSizeValues)), "AQtion's own menus in your view, as fits");
    SM_Help(SM_Choice("Splitscreen menu size", SM_Var(n, "cl_splitMenuScale"), smMenuScaleValues,
                      smMenuScaleLabels, q_countof(smMenuScaleValues)), "These menus in your view");
}

static const smPage_t smSettings = { SM_BuildSettings };

// --- controls (the Quake3e fork's Controls page, the rows our pad layer has) ---

static const char *const smCurveValues[] = { "linear", "standard", "dynamic", "custom" };
static const char *const smCurveLabels[] = { "Linear", "Standard", "Dynamic", "Custom" };

// the pad feel a profile holds (split_profile.c prof_feel)
static const char *const smFeelCvars[] = {
    "joy_invertPitch", "joy_yawSpeed", "joy_pitchSpeed", "joy_deadzone", "joy_deadzoneOuter",
    "joy_aimCurve", "joy_aimExponent", "joy_turnBoost", "joy_turnBoostTime", "joy_zoomScale",
    "joy_triggerThreshold", "joy_stickThreshold"
};

static void SM_ResetControls(int n, int arg)
{
    const cvar_t *base;
    int i;

    for (i = 0; i < q_countof(smFeelCvars); i++) {
        if ((base = Cvar_FindVar(smFeelCvars[i])))
            Cvar_SetEx(SM_Var(n, smFeelCvars[i]), base->default_string, FROM_CONSOLE);
    }
    SM_Note(n, "Controls reset to defaults");
}

static void SM_BuildControls(int n)
{
    smRow_t *r;

    SM_Title(va("Player %d - Controls", n + 1));
    SM_Slider("Look speed horizontal", SM_Var(n, "joy_yawSpeed"), 60, 900, 20, "%.0f deg/s");
    SM_Slider("Look speed vertical", SM_Var(n, "joy_pitchSpeed"), 40, 600, 20, "%.0f deg/s");
    SM_Choice("Invert look up/down", SM_Var(n, "joy_invertPitch"), sm01, smOffOn, 2);
    SM_Help(SM_Choice("Aim curve", SM_Var(n, "joy_aimCurve"), smCurveValues, smCurveLabels, 4),
            "Standard: slow near the centre, fast at the edge");
    r = SM_Help(SM_Slider("Custom curve exponent", SM_Var(n, "joy_aimExponent"), 0.5f, 4, 0.1f, "%.1f"),
                "Used with Aim curve: Custom");
    r->disabled = Q_stricmp(Cvar_VariableString(SM_Var(n, "joy_aimCurve")), "custom") != 0;
    SM_Help(SM_Slider("Turn boost (full stick)", SM_Var(n, "joy_turnBoost"), 1, 3, 0.1f, "%.1fx"),
            "Faster turning with the stick all the way over");
    SM_Help(SM_Slider("Turn boost ramp", SM_Var(n, "joy_turnBoostTime"), 0, 1000, 50, "%.0f ms"),
            "How long the boost takes to build up");
    SM_Help(SM_Slider("Look speed zoomed", SM_Var(n, "joy_zoomScale"), 0.1f, 2, 0.05f, "%.2fx"),
            "Times the zoom's own slow-down");
    SM_Help(SM_Slider("Stick dead zone", SM_Var(n, "joy_deadzone"), 0, 0.5f, 0.01f, "%.2f"),
            "Stick travel that does nothing");
    SM_Help(SM_Slider("Full speed at tilt", SM_Var(n, "joy_deadzoneOuter"), 0.5f, 1, 0.01f, "%.2f"),
            "Lower: full speed sooner");
    SM_Help(SM_Slider("Trigger press point", SM_Var(n, "joy_triggerThreshold"), 0.05f, 0.95f, 0.05f, "%.2f"),
            "How far LT / RT go before they press");
    SM_Help(SM_Sub("Buttons", &smButtons), "Pick what each button does");
    SM_Action("Reset controls to defaults", SM_ResetControls, 0);
}

static const smPage_t smControls = { SM_BuildControls };

// --- buttons (per-button remapping = `pbind <player> PAD_<button> <command>') ---

static const struct {
    const char  *key;
    const char  *label;
} smPadKeys[] = {
    { "PAD_A", "A" },
    { "PAD_B", "B" },
    { "PAD_X", "X" },
    { "PAD_Y", "Y" },
    { "PAD_LB", "LB" },
    { "PAD_RB", "RB" },
    { "PAD_LT", "LT" },
    { "PAD_RT", "RT" },
    { "PAD_L3", "Left stick click" },
    { "PAD_R3", "Right stick click" },
    { "PAD_BACK", "Back" },
    { "PAD_DPAD_UP", "D-pad up" },
    { "PAD_DPAD_DOWN", "D-pad down" },
    { "PAD_DPAD_LEFT", "D-pad left" },
    { "PAD_DPAD_RIGHT", "D-pad right" },
    { "PAD_MISC1", "Share" },
    { "PAD_PADDLE1", "Paddle 1" },
    { "PAD_PADDLE2", "Paddle 2" },
    { "PAD_PADDLE3", "Paddle 3" },
    { "PAD_PADDLE4", "Paddle 4" },
};

static void SM_ResetButtons(int n, int arg)
{
    PAD_ProfileDefaultBinds(n);
    SM_Note(n, "Buttons reset to defaults");
}

// a player without a pad yet has an empty table: the defaults to edit
static void SM_ButtonsEnter(int n)
{
    PAD_MenuEnsureBinds(n);
}

static void SM_BuildButtons(int n)
{
    smRow_t *r;
    int i;

    SM_Title(va("Player %d - Buttons", n + 1));
    for (i = 0; i < q_countof(smPadKeys); i++) {
        r = SM_Row(SMR_BIND, smPadKeys[i].label);
        Q_strlcpy(r->cvar, smPadKeys[i].key, sizeof(r->cvar));
        r->arg = n;
        SM_Help(r, "Left / right: pick the action");
    }
    SM_Text("Start: this menu (fixed)");
    SM_Action("Reset buttons to defaults", SM_ResetButtons, 0);
}

static const smPage_t smButtons = { SM_BuildButtons, SM_ButtonsEnter };

// --- leave / end game ---

static void SM_DoLeave(int n, int arg)
{
    SM_Close(n);
    if (n == 0) {
        Com_Printf("P1 menu: end game\n");
        SM_Exec("disconnect");
    } else {
        CL_SplitRequestDrop(n, "left (menu)");
    }
}

static void SM_Cancel(int n, int arg)
{
    SM_Back(n);
}

static void SM_BuildLeave(int n)
{
    if (n == 0) {
        SM_Title("End the game?");
        SM_Text("Everyone leaves; back to the main menu.");
        SM_Action("Cancel", SM_Cancel, 0);
        SM_Action("End game for everyone", SM_DoLeave, 0);
    } else {
        SM_Title(va("Player %d: leave the game?", n + 1));
        SM_Text("The others keep playing.");
        SM_Action("Cancel", SM_Cancel, 0);
        SM_Action("Leave game", SM_DoLeave, 0);
    }
}

static const smPage_t smLeave = { SM_BuildLeave };

// --- profile (split_profile.c; Q3 12.3, 13.1, 14.3) ---

static void SM_ProfileResult(int n, bool ok, const char *done, const char *err)
{
    SM_Note(n, ok ? done : va("Can't: %s", err));
}

static void SM_ProfileLoad(int n, int arg)
{
    char err[MAX_QPATH * 2], name[PROFILE_NAME_MAX + 1];

    bool ok;

    Q_strlcpy(name, CL_ProfileListName(arg), sizeof(name));
    ok = CL_ProfileLoad(n, name, err, sizeof(err));
    SM_ProfileResult(n, ok, va("Playing as %s", name), err);
}

static void SM_ProfileSave(int n, int arg)
{
    char err[MAX_QPATH * 2];
    bool ok = CL_ProfileSave(n, NULL, err, sizeof(err));

    SM_ProfileResult(n, ok, "Saved", err);
}

static void SM_ProfileSaveAsDone(int n, const char *text)
{
    char err[MAX_QPATH * 2];
    bool ok = CL_ProfileSave(n, text, err, sizeof(err));

    SM_ProfileResult(n, ok, va("Saved as %s", CL_ProfileName(n)), err);
}

static void SM_ProfileSaveAs(int n, int arg)
{
    char clean[PROFILE_NAME_MAX + 1];

    // the in-game name to start from, unless it is the guest's "Player N"
    if (!CL_ProfileSanitize(Cvar_VariableString(SM_Var(n, "name")), clean, sizeof(clean)) ||
        !Q_strncasecmp(clean, "Player", 6))
        clean[0] = 0;
    SM_OpenKeyboard(n, va("Player %d: new profile name", n + 1), clean, PROFILE_NAME_MAX, NULL, false);
    sm[n].kb.done = SM_ProfileSaveAsDone;
}

static void SM_ProfileGuest(int n, int arg)
{
    char err[MAX_QPATH * 2];
    bool ok = CL_ProfileLoad(n, NULL, err, sizeof(err));

    SM_ProfileResult(n, ok, "Playing as a guest", err);
}

// player 1 is a guest too until a profile is saved (its guest settings stay
// in the config; the others' last until they leave)
static void SM_BuildProfile(int n)
{
    int i, user;

    SM_Title(va("Player %d - Profile", n + 1));
    SM_Text(CL_ProfileNamed(n) ? va("Playing as: %s", CL_ProfileName(n)) :
            n ? "Playing as: Guest (until you leave)" : "Playing as: Guest");
    if (CL_ProfileNamed(n))
        SM_Action("Save", SM_ProfileSave, 0);
    SM_Action("Save as new profile...", SM_ProfileSaveAs, 0);
    if (CL_ProfileNamed(n))
        SM_Action("Use guest", SM_ProfileGuest, 0);
    CL_ProfileRefresh();
    SM_Text(CL_ProfileCount() ? "Profiles:" : "No saved profiles yet");
    for (i = 0; i < CL_ProfileCount() && i < SM_MAX_ROWS - 8; i++) {
        user = CL_ProfileListUser(i);
        if (user == n)
            SM_Text(va("  %s (you)", CL_ProfileListName(i)));
        else if (user >= 0)
            SM_Text(va("  %s (player %d)", CL_ProfileListName(i), user + 1));
        else
            SM_Action(va("  %s", CL_ProfileListName(i)), SM_ProfileLoad, i);
    }
}

static const smPage_t smProfile = { SM_BuildProfile };

// --- the host's splitscreen settings (Q3 13.4, 14.1): player 1 only ---

static const char *const smP1InputValues[] = { "pad", "kbm" };
static const char *const smP1InputLabels[] = { "Gamepad", "Keyboard + mouse" };
static const char *const smVerticalLabels[] = { "Top / bottom", "Side by side" };
// cl_splitFill: 3, 5 and 7 players get a wider bottom row; 8 is 4x2 (fill)
// or 3x3 (equal cells), so the row says what it does, not which counts
static const char *const smFillLabels[] = { "Equal cells", "Fill the screen" };
static const char *const smWideValues[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8" };
static const char *const smWideLabels[] = { "Last joined", "Player 1", "Player 2", "Player 3",
                                            "Player 4", "Player 5", "Player 6", "Player 7", "Player 8" };
static const char *const smJoinHoldValues[] = { "0", "300", "500", "700", "1000", "1500" };
static const char *const smJoinHoldLabels[] = { "Press A", "Hold A 0.3 s", "Hold A 0.5 s",
                                                "Hold A 0.7 s", "Hold A 1 s", "Hold A 1.5 s" };
static const char *const smJoinHintValues[] = { "0", "1", "2" };
static const char *const smJoinHintLabels[] = { "Off", "When a pad is used", "10 s at start" };

static void SM_HostEnter(int n)
{
    SM_ReadMaps();
}

static void SM_QuickMatch(int n, int arg)
{
    SM_Close(n);
    Com_Printf("P1 menu: splitquickmatch\n");
    SM_Exec("splitquickmatch\n");
}

static void SM_HostStart(int n, int arg)
{
    SM_Close(n);
    Com_Printf("P1 menu: splitstartgame\n");
    SM_Exec("splitstartgame\n");
}

static void SM_GameTypeChanged(int n)
{
    if (SM_Hosting())
        SM_ApplyGameType();     // latched: the next map / Restart
}

// the game type row and (main-menu page) the map row
static void SM_GameRows(bool withMap)
{
    SM_Help(SM_Choice("Game type", "cl_splitGameType", smGameTypeValues, smGameTypeLabels,
                      q_countof(smGameTypeValues)),
            SM_Hosting() ? "Next map (Restart map applies it)" : "CTF / Domination: maps made for them")->changed =
        SM_GameTypeChanged;
    if (withMap) {
        if (smNumMaps > 0)
            SM_Choice("Map", "cl_splitStartMap", (const char *const *)smMaps, (const char *const *)smMaps, smNumMaps);
        else
            SM_Text(va("Map: %s", cl_splitStartMap->string));
    }
}

static void SM_BuildHost(int n)
{
    const bool mainMenu = sm[n].mainMenu;

    SM_Title(mainMenu ? "Splitscreen" : "Splitscreen settings");
    if (mainMenu) {
        SM_Help(SM_Action("Quick Local Match", SM_QuickMatch, 0),
                "Your last server options on a random map");
        SM_Sub(SM_ProfileLabel(0), &smProfile);
    }
    // the pad layer's settings exist only in a build with pads
    if (Cvar_FindVar("cl_splitP1Input"))
        SM_Choice("Player 1 uses", "cl_splitP1Input", smP1InputValues, smP1InputLabels, 2);
    SM_Choice("Two players", "cl_splitVertical", sm01, smVerticalLabels, 2);
    SM_Choice("Cell layout", "cl_splitFill", sm01, smFillLabels, 2);
    SM_Choice("Wide cell for", "cl_splitWidePlayer", smWideValues, smWideLabels, q_countof(smWideValues));
    SM_Slider("Max players", "cl_splitMaxPlayers", 1, MAX_SPLITVIEW, 1, "%.0f");
    if (Cvar_FindVar("cl_splitJoinHold"))
        SM_Choice("Join by", "cl_splitJoinHold", smJoinHoldValues, smJoinHoldLabels, q_countof(smJoinHoldValues));
    if (Cvar_FindVar("cl_splitJoinHint"))
        SM_Choice("Join hint", "cl_splitJoinHint", smJoinHintValues, smJoinHintLabels, q_countof(smJoinHintValues));
    SM_Choice("Cell borders", "cl_splitBorder", sm01, smOffOn, 2);
    SM_Text("Join: hold A  Leave: hold Back+Start");
    if (mainMenu) {
        SM_GameRows(true);
        SM_Help(SM_Sub("Server options", &smServer), "Time limit, bots, weapons... for this game");
        SM_Action("Start multiplayer game", SM_HostStart, 0);
    }
}

static const smPage_t smHost = { SM_BuildHost, SM_HostEnter };

/*
--- Server options (the Quake3e fork's 18): player 1, its own listen server ---

Every row is a cvar of Steam's TNG 2.82 game DLL (checked with cvarlist on
that DLL, work/menus-steam-cvarlist.txt); a row whose cvar the running game
does not have is left out. "Now": the game reads it live. "Next map": the
cvar is latched (or read at map start); Restart map applies it.
*/

static const char *const smTimeValues[] = { "0", "5", "10", "15", "20", "30", "45", "60" };
static const char *const smTimeLabels[] = { "None", "5 min", "10 min", "15 min", "20 min", "30 min", "45 min", "60 min" };
static const char *const smFragValues[] = { "0", "5", "10", "15", "20", "25", "30", "50", "100" };
static const char *const smFragLabels[] = { "None", "5", "10", "15", "20", "25", "30", "50", "100" };
static const char *const smRoundValues[] = { "0", "3", "5", "7", "10", "15", "20" };
static const char *const smRoundLabels[] = { "None", "3", "5", "7", "10", "15", "20" };
static const char *const smRoundTimeValues[] = { "0", "1", "2", "3", "4", "5", "6", "8", "10" };
static const char *const smRoundTimeLabels[] = { "None", "1 min", "2 min", "3 min", "4 min", "5 min", "6 min", "8 min", "10 min" };
static const char *const smCapValues[] = { "0", "1", "2", "3", "5", "7", "10" };
static const char *const smCapLabels[] = { "None", "1", "2", "3", "5", "7", "10" };
static const char *const smBotValues[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                           "11", "12", "13", "14", "15", "16", "17", "18", "19", "20" };
static const char *const smBotLabels[] = { "None", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                           "11", "12", "13", "14", "15", "16", "17", "18", "19", "20" };
static const char *const smSkillLabels[] = { "0 (easiest)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10 (hardest)" };
static const char *const smCarryValues[] = { "1", "2", "3", "4", "5" };
static const char *const smDarkValues[] = { "0", "1", "2", "3" };
static const char *const smDarkLabels[] = { "Off", "Total darkness", "Near darkness", "Day and night" };
static const char *const smTgrenValues[] = { "0", "1", "2", "3", "4" };
static const char *const smTgrenLabels[] = { "None", "1", "2", "3", "4" };
static const char *const smChaseValues[] = { "0", "1", "2" };
static const char *const smChaseLabels[] = { "Anyone", "Own team", "Own team, first person" };
static const char *const smRespawnValues[] = { "10", "20", "30", "45", "59", "74", "90", "120" };
static const char *const smRespawnLabels[] = { "10 s", "20 s", "30 s", "45 s", "59 s", "74 s", "90 s", "120 s" };
/*
Weapons presets (0.0.0.30): wp_flags values (the game's items[].flag bits,
the same in Steam's TNG 2.82: MK23 1, MP5 2, M4 4, M3 8, handcannon 16,
sniper 32, dual MK23 64, knife 128, grenades 256). A gun preset keeps the
MK23 bit: every player always carries the pistol, and with its bit off the
game gives it no rounds and no magazines (0.0.0.29's "Sniper only" = 32 was
a pistol with no ammo). The game then still spawns everyone with only the
MK23 unless told otherwise; SM_WeapPresetFrame does that (dmweapon / choose).
*/
static const char *const smWeapSetValues[] = { "511", "1", "65", "3", "5", "9", "17", "33", "128" };
static const char *const smWeapSetLabels[] = {
    "All", "MK23 only", "Dual pistols only", "MP5 only", "M4 only", "M3 only", "Handcannon only",
    "Sniper only", "Knife only"
};
static const char *const smDelayValues[] = { "0", "1", "2", "3", "4", "5", "7", "10" };
static const char *const smDelayLabels[] = { "None", "1 s", "2 s", "3 s", "4 s", "5 s", "7 s", "10 s" };
static const char *const smProtValues[] = { "0", "10", "20", "30", "40", "60", "100" };
static const char *const smProtLabels[] = { "Off", "1 s", "2 s", "3 s", "4 s", "6 s", "10 s" };

#define DF_NO_FALLING_BIT       8       // Action's dmflags
#define DF_NO_FRIENDLY_FIRE_BIT 256
#define DF_FORCE_RESPAWN_BIT    1024
#define DF_INFINITE_AMMO_BIT    8192    // Action: weapon pickups give 1000 rounds and the
                                        // map loses its ammo boxes; magazines still run dry
                                        // (0.0.0.24's row; cleared, see SM_ApplyInfAmmo)

// game cvars the page changes and their defaults (Steam's TNG 2.82); a
// saved set holds these (those the game has), dmflags, the game type, bots
static const struct {
    const char  *name;
    const char  *def;
} smSrvDefaults[] = {
    { "timelimit", "0" }, { "fraglimit", "0" }, { "roundlimit", "0" }, { "roundtimelimit", "0" },
    { "capturelimit", "0" }, { "ltk_skill", "5" }, { "ltk_chat", "1" }, { "cl_splitHumorSounds", "1" },
    { "dm_choose", "0" }, { "allweapon", "0" }, { "allitem", "0" },
    { "weapons", "1" }, { "items", "1" }, { "darkmatch", "0" }, { "use_rewards", "1" },
    { "use_warnings", "1" }, { "wp_flags", "511" }, { "itm_flags", "63" }, { "tgren", "0" },
    { "limchasecam", "0" }, { "item_respawn", "59" }, { "weapon_respawn", "74" },
    { "uvtime", "40" }, { "dm_shield", "0" }, { "teamdm_respawn", "2" }, { "ctf_respawn", "4" },
    { "cl_splitInfiniteAmmo", "0" },
};

static const char *SM_SrvDefault(const char *name)
{
    int i;

    for (i = 0; i < q_countof(smSrvDefaults); i++) {
        if (!strcmp(name, smSrvDefaults[i].name))
            return smSrvDefaults[i].def;
    }
    return "0";
}

// a game cvar row is offered when the running game has the cvar; with no
// game running the rows of Steam's DLL
static bool SM_SrvHas(const char *name)
{
    if (sv_running->integer)
        return Cvar_FindVar(name) != NULL;
    return true;
}

static smRow_t *SM_SrvChoice(const char *label, const char *name, const char *const *values,
                             const char *const *labels, int num, const char *help)
{
    return SM_Help(SM_Choice(label, SM_GameVar(name, SM_SrvDefault(name)), values, labels, num), help);
}

/*
Humor sounds (0.0.0.27): AQtion's joke clips (farts, laughs, "newbie", film
quotes) in sound/insane/ and sound/user/ (split_snd.c). The game plays
insane/ when a living player has been idle ppl_idletime seconds (15, then
every 15 s while idle; Steam's TNG 2.82 too, which is what the maintainer
heard) and user/ through `voice' and, in our src/action only, when a bot
frags (bot_randvoice %). The row is cl_splitHumorSounds: off, no local
player starts one (any game library or server, live), and when hosting the
game's own cvars are set too so players on other machines hear none either:
ppl_idletime and bot_randvoice 0 (created if the game has not registered
them yet: the game keeps the value); on, back to the game's 15 / 5 where 0.
Both cvars are read live by the game.
*/
static cvar_t *cl_splitHumorSounds;

static void SM_HumorCvar(const char *name, const char *on)
{
    cvar_t *v = Cvar_FindVar(name);

    if (!cl_splitHumorSounds->integer) {
        if (!v)
            v = Cvar_Get(name, "0", 0);
        if (v->integer)
            Cvar_SetByVar(v, "0", FROM_CONSOLE);
    } else if (v && !v->integer) {
        Cvar_SetByVar(v, on, FROM_CONSOLE);
    }
}

static void SM_ApplyHumor(void)
{
    if (SM_Remote())
        return;     // another machine's server: only the client side applies
    SM_HumorCvar("ppl_idletime", "15");
    SM_HumorCvar("bot_randvoice", "5");
}

static void SM_HumorChanged(int n)
{
    SM_ApplyHumor();
    Com_Printf("P1 menu: humor sounds %s\n", cl_splitHumorSounds->integer ? "on" : "off");
}

static void SM_LoadMap(int n, const char *map)
{
    SM_Close(n);
    if (!*map || strpbrk(map, "\";\n\r"))
        return;
    SM_ApplyGameType();
    SM_ApplyBots();
    SM_ApplyInfAmmo();
    SM_ApplyHumor();
    Com_Printf("P1 menu: map %s\n", map);
    SM_Exec(va("map \"%s\" force\n", map));
}

static void SM_SrvRestart(int n, int arg)
{
    SM_LoadMap(n, Cvar_VariableString("mapname"));
}

// the installed map after the current one (alphabetical, wrapping)
static void SM_SrvNextMap(int n, int arg)
{
    const char *cur = Cvar_VariableString("mapname");
    int i;

    SM_ReadMaps();
    if (!smNumMaps)
        return;
    for (i = 0; i < smNumMaps && Q_stricmp(smMaps[i], cur); i++)
        ;
    i = (i + 1) % smNumMaps;
    Cvar_SetByVar(cl_splitStartMap, smMaps[i], FROM_CONSOLE);
    SM_LoadMap(n, smMaps[i]);
}

/*
Infinite ammo (0.0.0.25, live since 0.0.0.28). Steam's TNG 2.82 has no
setting that keeps a magazine from running dry: dmflags 8192 only makes
weapon pickups give 1000 rounds (and removes the map's ammo boxes). What
works is the game's `give', which needs `cheats 1'. So the row turns cheats on
for the hosted game and keeps every local player's spare ammo full
(CL_SplitMenuFrame). Since 0.0.0.29 the rounds in the gun are never touched:
players fire them down and reload as usual; only the reserve is infinite.
`give ammo' (which also fills every gun) is sent once per life, at spawn, when
all guns are loaded anyway: it sets the spare magazines / shells to the carry
limit (bandolier, game mode), which is "full". After that each reload that
uses a magazine (or shells) is answered with `give Pistol Magazine' / `MP5
Magazine' / `M4 Magazine' / `12 Gauge Shells' / `AP Sniper Ammo' (the HUD's
clip icon tells which): the game adds one pickup's worth to the reserve only
(uncapped; shells come 7 and sniper rounds 10 at a time, so those can sit a
few above the limit). Knives and grenades have no reload: `give Combat Knife'
/ `give M26 Fragmentation Grenade' add one back for each one thrown. Bots and
players on other machines get nothing extra.
`cheats' is CVAR_LATCH, so a plain set while a server runs only takes effect
at the next map, and the game reads the live value (it holds the engine's
cvar_t): 0.0.0.25-27 sent nothing until a restart, and nothing at all in a
game started from AQtion's own menus or the console. Since 0.0.0.28 the row
writes `cheats' with FROM_CODE (no latch: the running game sees it at once),
and while the row is on and we host, CL_SplitMenuFrame puts `cheats 1' back
whenever the live value is 0, whichever way the game was started.
`cheats' is only written while the row is on, and set back to 0 only when
the row turned it on and is now off (0.0.0.26): a `cheats 1' set by hand
survives map loads, starts, resets and set loads with the row off.
*/
static cvar_t *cl_splitInfiniteAmmo;
static bool smInfAmmoCheats;    // the row has written `cheats 1'

// cheats to value at once, also while a server runs (no "next game" latch)
static void SM_SetCheatsNow(const char *value)
{
    cvar_t *v = Cvar_FindVar("cheats");

    if (!v) {
        Cvar_SetEx("cheats", value, FROM_CODE);
        return;
    }
    if (!strcmp(v->string, value) && !v->latched_string)
        return;
    Cvar_SetByVar(v, value, FROM_CODE);
    if (!v->integer)
        Cvar_FixCheats();   // cheat-protected client cvars back to defaults
}

static void SM_ApplyInfAmmo(void)
{
    const int df = SM_NextInt("dmflags");

    if (cl_splitInfiniteAmmo->integer) {
        Com_Printf("P1 menu: infinite ammo on, cheats 1 (now)\n");
        SM_SetCheatsNow("1");
        smInfAmmoCheats = true;
    } else if (smInfAmmoCheats) {
        Com_Printf("P1 menu: infinite ammo off, cheats 0 (now)\n");
        SM_SetCheatsNow("0");
        smInfAmmoCheats = false;
    }
    // a set saved with 0.0.0.24's dmflags row: the ammo boxes come back
    if (df & DF_INFINITE_AMMO_BIT)
        Cvar_SetEx("dmflags", va("%d", df & ~DF_INFINITE_AMMO_BIT), FROM_CONSOLE);
}

static void SM_InfAmmoChanged(int n)
{
    SM_ApplyInfAmmo();
}

#define SM_INFAMMO_WAIT     500     // ms to wait for a sent top-up to show before sending again

typedef enum { SM_HAND_GUN, SM_HAND_KNIFE, SM_HAND_GRENADE } smHand_t;

// the spare ammo items, by the HUD's clip icon (dual pistols use the MK23's
// magazines, the handcannon the shotgun's shells); `give <item>' adds one
// pickup's worth (1 magazine, 7 shells, 10 sniper rounds) to the reserve and
// never touches the rounds in the gun
static const struct {
    const char *icon, *item;
    int         quantity;   // what one `give' adds
} smReserves[] = {
    { "a_clip",    "Pistol Magazine",  1 },
    { "a_mag",     "MP5 Magazine",     1 },
    { "a_m4",      "M4 Magazine",      1 },
    { "a_shells",  "12 Gauge Shells",  7 },
    { "a_bullets", "AP Sniper Ammo",  10 },
};
#define SM_NUM_RESERVES q_countof(smReserves)

static struct {
    int         gun, ammo, clip, kind;  // last frame's gun model, HUD counts, reserve kind
    smHand_t    hand;
    int         full[SM_NUM_RESERVES];  // the reserve when full (highest seen before our own top-up)
    bool        topped[SM_NUM_RESERVES];// we have topped this kind up (full is frozen)
    unsigned    gap;                    // no knife / grenade refill before
    unsigned    sent;                   // time of the outstanding reserve top-up, 0 none
    int         sentClip;               // the reserve when it was sent
    int         pending;                // knives / grenades used, not yet refilled
    bool        valid, seenOut;         // tracked last frame; seen dead / not yet in play
} smInfAmmo[MAX_SPLITVIEW];

// what the current context holds, from the HUD's ammo icon
static smHand_t SM_HandKind(void)
{
    const int icon = cl.frame.ps.stats[STAT_AMMO_ICON];
    const char *name;

    if (icon <= 0 || icon >= cl.csr.max_images)
        return SM_HAND_GUN;
    name = cl.configstrings[cl.csr.images + icon];
    if (!Q_stricmp(name, "w_knife"))
        return SM_HAND_KNIFE;
    if (!Q_stricmp(name, "a_m61frag"))
        return SM_HAND_GRENADE;
    return SM_HAND_GUN;
}

// the reserve kind of the gun in hand, from the HUD's clip icon, -1 none
static int SM_ReserveKind(void)
{
    const int icon = cl.frame.ps.stats[STAT_CLIP_ICON];
    const char *name;
    int i;

    if (icon <= 0 || icon >= cl.csr.max_images)
        return -1;
    name = cl.configstrings[cl.csr.images + icon];
    for (i = 0; i < SM_NUM_RESERVES; i++)
        if (!Q_stricmp(name, smReserves[i].icon))
            return i;
    return -1;
}

static void SM_InfAmmoSend(int n, const char *give)
{
    // straight onto that player's connection: the command buffer stalls
    // while a `wait' (a cfg, an alias) is pending
    if (CL_SplitPushView(n)) {
        CL_ClientCommand(give);
        CL_PopContext();
    }
}

/*
The weapon a Weapons preset (or a Weapon bans mix) gives at spawn (0.0.0.30).
Steam's TNG 2.82 (and src/action) spawn a player with the MK23 only, whatever
wp_flags allows: in deathmatch without weapon choice EquipClientDM adds the
`dmweapon' cvar's weapon (default "MK23 Pistol"; read at every spawn, no
wp_flags check, the gun loaded plus its usual spares, bots too); in teamplay
and with dm_choose EquipClient gives the player's chosen weapon, whose default
is MP5, else MK23, else knife, and nothing when that is banned. So while the
allowed weapons call for another start weapon we set dmweapon to it and send
`choose <it>' once per map for every local player (the game ignores `choose'
where there is no weapon choice; bots pick an allowed weapon themselves);
back to all weapons, the players we chose for get the game's default choice
(MP5) again. NULL = leave the game's own default.
*/
static const struct {
    int         bit;
    const char *name;   // the game's pickup name (`dmweapon', `choose')
} smStartWeapons[] = {
    { 2, "MP5/10 Submachinegun" }, { 4, "M4 Assault Rifle" }, { 8, "M3 Super 90 Assault Shotgun" },
    { 16, "Handcannon" }, { 32, "Sniper Rifle" }, { 64, "Dual MK23 Pistols" },
};
#define SM_WPF_MK23     1
#define SM_WPF_KNIFE    128
#define SM_WPF_MASK     511
#define SM_DMWEAPON_DEF "MK23 Pistol"

static const char *SM_StartWeapon(int flags)
{
    const char *first = NULL;
    int i, count = 0;

    if ((flags & SM_WPF_MASK) == SM_WPF_MASK)
        return NULL;
    for (i = 0; i < q_countof(smStartWeapons); i++) {
        if (flags & smStartWeapons[i].bit) {
            if (!count++)
                first = smStartWeapons[i].name;
        }
    }
    if (count == 1)
        return first;           // "<gun> only" (with or without the pistol)
    if (flags & SM_WPF_MK23)
        return count ? NULL : SM_DMWEAPON_DEF;
    if (count)
        return first;           // pistol banned, several guns: the first one
    if (flags & SM_WPF_KNIFE)
        return "Combat Knife";
    return NULL;
}

static struct {
    int     servercount;        // map the choice was sent for, -1 none
    char    chosen[32];         // what was sent
    int     gun, ammo, clip;    // developer log: last HUD state
} smWeapStart[MAX_SPLITVIEW];
static bool smDmWeaponSet;      // we changed dmweapon from the game's default
static const char *smWantLast;  // SM_StartWeapon's last answer, and since when
static unsigned smWantSince;
#define SM_CHOOSE_SETTLE    1000    // ms: no `choose' per step while the row is being cycled

static void SM_WeapPresetFrame(void)
{
    static cvar_t *dev;
    const char *want, *model;
    cvar_t *dmw;
    int n, sc, gun, ammo, clip;
    bool active, settled;

    if (SM_Remote())
        return;     // another machine's server: its own rules
    if (!dev)
        dev = Cvar_Get("developer", "0", 0);
    want = SM_StartWeapon(Cvar_VariableInteger("wp_flags"));
    if (want != smWantLast) {
        smWantLast = want;
        smWantSince = cls.realtime;
    }

    // deathmatch start weapon: read by the game at every spawn
    dmw = Cvar_FindVar("dmweapon");
    if (want) {
        if (!dmw)
            dmw = Cvar_Get("dmweapon", SM_DMWEAPON_DEF, 0);
        if (strcmp(dmw->string, want)) {
            Com_Printf("P1 menu: dmweapon \"%s\" (Weapons preset)\n", want);
            Cvar_SetByVar(dmw, want, FROM_CONSOLE);
        }
        smDmWeaponSet = true;
    } else if (smDmWeaponSet) {
        if (dmw && strcmp(dmw->string, SM_DMWEAPON_DEF)) {
            Com_Printf("P1 menu: dmweapon \"%s\" (Weapons: all)\n", SM_DMWEAPON_DEF);
            Cvar_SetByVar(dmw, SM_DMWEAPON_DEF, FROM_CONSOLE);
        }
        smDmWeaponSet = false;
    }

    if (!SM_Hosting()) {
        for (n = 0; n < MAX_SPLITVIEW; n++)
            smWeapStart[n].servercount = -1;
        return;
    }
    // weapon choice games: the choice, once per map and preset per player
    // (the MK23 is not a choice: the game's default gives it)
    for (n = 0; n < MAX_SPLITVIEW; n++) {
        if (!CL_SplitPlayerLive(n) || !CL_SplitPushView(n)) {
            smWeapStart[n].servercount = -1;
            continue;
        }
        active = cls.state == ca_active && cl.frame.valid;
        settled = cls.realtime - smWantSince >= SM_CHOOSE_SETTLE;
        sc = cl.servercount;
        gun = cl.frame.ps.gunindex;
        ammo = cl.frame.ps.stats[STAT_AMMO];
        clip = cl.frame.ps.stats[STAT_CLIP];
        model = gun > 0 && gun < cl.csr.max_models ? cl.configstrings[cl.csr.models + gun] : "none";
        if (active && settled && want && strcmp(want, SM_DMWEAPON_DEF) &&
            (smWeapStart[n].servercount != sc || strcmp(smWeapStart[n].chosen, want))) {
            CL_ClientCommand(va("choose %s", want));
            smWeapStart[n].servercount = sc;
            Q_strlcpy(smWeapStart[n].chosen, want, sizeof(smWeapStart[n].chosen));
            Com_Printf("P1 menu: p%d choose %s (Weapons preset)\n", n + 1, want);
        } else if (active && settled && !want && smWeapStart[n].chosen[0]) {
            // back to all weapons: our choice goes, the game's default (MP5) returns
            if (Cvar_VariableInteger("wp_flags") & 2) {
                CL_ClientCommand("choose MP5/10 Submachinegun");
                Com_Printf("P1 menu: p%d choose MP5/10 Submachinegun (Weapons: game default)\n", n + 1);
            }
            smWeapStart[n].chosen[0] = 0;
        }
        CL_PopContext();
        if (dev->integer && active && (gun != smWeapStart[n].gun || ammo != smWeapStart[n].ammo ||
                                       clip != smWeapStart[n].clip))
            Com_Printf("weapon preset: p%d gun %s loaded %d reserve %d t=%u\n", n + 1, model, ammo, clip,
                       cls.realtime);
        smWeapStart[n].gun = gun;
        smWeapStart[n].ammo = ammo;
        smWeapStart[n].clip = clip;
    }
}

void CL_SplitMenuFrame(void)
{
    static cvar_t *dev;
    const char *give;
    cvar_t *cheats;
    int n, i, gun, ammo, clip, kind, count;
    smHand_t hand;
    bool alive, same;

    SM_WeapPresetFrame();
    if (!cl_splitInfiniteAmmo || !cl_splitInfiniteAmmo->integer) {
        memset(smInfAmmo, 0, sizeof(smInfAmmo));
        return;
    }
    if (!SM_Hosting()) {
        // no game yet: whoever is in play when it starts has just spawned
        memset(smInfAmmo, 0, sizeof(smInfAmmo));
        for (n = 0; n < MAX_SPLITVIEW; n++)
            smInfAmmo[n].seenOut = true;
        return;
    }
    if (!dev)
        dev = Cvar_Get("developer", "0", 0);
    // a game started from AQtion's own menus, the console or a cfg: the row
    // still means cheats on, from the first frame of the game
    cheats = Cvar_FindVar("cheats");
    if (!cheats || !cheats->integer || cheats->latched_string) {
        Com_Printf("P1 menu: infinite ammo on, cheats 1 (now)\n");
        SM_SetCheatsNow("1");
        smInfAmmoCheats = true;
    }

    for (n = 0; n < MAX_SPLITVIEW; n++) {
        if (!CL_SplitPlayerLive(n) || !CL_SplitPushView(n)) {
            smInfAmmo[n].valid = false;
            smInfAmmo[n].seenOut = true;
            continue;
        }
        // in play with a weapon in hand: not dead, not a spectator (teamplay
        // before joining a team is PM_NORMAL, no gun, and `give' refuses it).
        // The gun model alone is not enough: Action sends none while the
        // sniper is zoomed and for "hand 2" (Gun hand: Hidden), so the HUD's
        // ammo / clip icons count too; else a zoom looked like a new life
        // (`give ammo' reloaded the gun) and a hidden gun never got spares
        gun = cl.frame.ps.gunindex;
        alive = cls.state == ca_active && cl.frame.valid && cl.frame.ps.pmove.pm_type == PM_NORMAL &&
                (gun || cl.frame.ps.stats[STAT_AMMO_ICON] > 0 || cl.frame.ps.stats[STAT_CLIP_ICON] > 0);
        ammo = cl.frame.ps.stats[STAT_AMMO];
        clip = cl.frame.ps.stats[STAT_CLIP];
        hand = alive ? SM_HandKind() : SM_HAND_GUN;
        kind = alive && hand == SM_HAND_GUN ? SM_ReserveKind() : -1;
        CL_PopContext();
        if (!alive) {
            smInfAmmo[n].valid = false;
            smInfAmmo[n].seenOut = true;
            continue;
        }

        if (!smInfAmmo[n].valid && smInfAmmo[n].seenOut) {
            // a new life: every gun is loaded, so the game's `give ammo'
            // only fills the spare magazines / shells to the carry limit
            // (bandolier and game mode included), which is "full" from now on
            memset(smInfAmmo[n].full, 0, sizeof(smInfAmmo[n].full));
            memset(smInfAmmo[n].topped, 0, sizeof(smInfAmmo[n].topped));
            smInfAmmo[n].seenOut = false;
            smInfAmmo[n].sent = 0;
            if (dev->integer)
                Com_Printf("infinite ammo: p%d spawn, give ammo (loaded %d, reserve %d)\n", n + 1, ammo, clip);
            SM_InfAmmoSend(n, "give ammo");
        }
        if (dev->integer && (!smInfAmmo[n].valid || ammo != smInfAmmo[n].ammo || clip != smInfAmmo[n].clip ||
                             kind != smInfAmmo[n].kind))
            Com_Printf("infinite ammo: p%d loaded %d reserve %d (%s) t=%u\n", n + 1, ammo, clip,
                       kind >= 0 ? smReserves[kind].item : hand == SM_HAND_KNIFE ? "knife" :
                       hand == SM_HAND_GRENADE ? "grenade" : "none", cls.realtime);

        // knives and grenades have no reload: one back for each one thrown
        same = smInfAmmo[n].valid && smInfAmmo[n].gun == gun && smInfAmmo[n].hand == hand;
        if (!same || hand == SM_HAND_GUN)
            smInfAmmo[n].pending = 0;
        else if (ammo < smInfAmmo[n].ammo)
            smInfAmmo[n].pending += smInfAmmo[n].ammo - ammo;
        smInfAmmo[n].gun = gun;
        smInfAmmo[n].ammo = ammo;
        smInfAmmo[n].clip = clip;
        smInfAmmo[n].kind = kind;
        smInfAmmo[n].hand = hand;
        smInfAmmo[n].valid = true;

        if (hand != SM_HAND_GUN) {
            if (smInfAmmo[n].pending <= 0 || cls.realtime < smInfAmmo[n].gap)
                continue;
            give = hand == SM_HAND_KNIFE ? "give Combat Knife" : "give M26 Fragmentation Grenade";
            smInfAmmo[n].pending--;
            smInfAmmo[n].gap = cls.realtime + 100;
            if (dev->integer)
                Com_Printf("infinite ammo: p%d %s (ammo %d, cheats %d)\n", n + 1, give, ammo,
                           cheats ? cheats->integer : -1);
            SM_InfAmmoSend(n, give);
            continue;
        }

        // a gun: the rounds in it are left alone (fire and reload as usual);
        // the spare magazines / shells go back to full when a reload uses one
        if (kind < 0)
            continue;
        if (!smInfAmmo[n].topped[kind] && clip > smInfAmmo[n].full[kind])
            smInfAmmo[n].full[kind] = clip;
        if (clip >= smInfAmmo[n].full[kind] && smInfAmmo[n].full[kind] > 0)
            continue;
        // switched on with nothing spare: one pickup's worth is full from now on
        if (smInfAmmo[n].full[kind] <= 0)
            smInfAmmo[n].full[kind] = 1;
        // one top-up at a time: wait for the last one to show
        if (smInfAmmo[n].sent && clip <= smInfAmmo[n].sentClip &&
            cls.realtime - smInfAmmo[n].sent < SM_INFAMMO_WAIT)
            continue;
        smInfAmmo[n].topped[kind] = true;
        smInfAmmo[n].sent = cls.realtime ? cls.realtime : 1;
        smInfAmmo[n].sentClip = clip;
        // all that is missing at once (the dual pistols reload two magazines)
        count = (smInfAmmo[n].full[kind] - clip + smReserves[kind].quantity - 1) / smReserves[kind].quantity;
        give = va("give %s", smReserves[kind].item);
        if (dev->integer)
            Com_Printf("infinite ammo: p%d %s x%d (loaded %d, reserve %d, full %d, cheats %d)\n", n + 1, give,
                       count, ammo, clip, smInfAmmo[n].full[kind], cheats ? cheats->integer : -1);
        for (i = 0; i < count; i++)
            SM_InfAmmoSend(n, give);
    }
}

static void SM_SrvReset(int n, int arg)
{
    int i;

    for (i = 0; i < q_countof(smSrvDefaults); i++) {
        if (Cvar_FindVar(smSrvDefaults[i].name))
            Cvar_SetEx(smSrvDefaults[i].name, smSrvDefaults[i].def, FROM_CONSOLE);
    }
    // friendly fire, falling damage on, no auto respawn / infinite ammo
    // (the other dmflags bits stay)
    Cvar_SetEx("dmflags", va("%d", Cvar_VariableInteger("dmflags") &
                             ~(DF_NO_FALLING_BIT | DF_NO_FRIENDLY_FIRE_BIT | DF_FORCE_RESPAWN_BIT |
                               DF_INFINITE_AMMO_BIT)), FROM_CONSOLE);
    Cvar_SetByVar(cl_splitBots, "0", FROM_CONSOLE);
    SM_ApplyBots();
    SM_ApplyInfAmmo();
    SM_ApplyHumor();
    Cvar_Set("cl_splitSrvSet", "");
    SM_Note(n, "Server options reset to defaults");
}

static void SM_BotsChanged(int n)
{
    SM_ApplyBots();
}

// deathmatch spawn protection needs dm_shield besides uvtime (p_client.c)
static void SM_ProtectChanged(int n)
{
    Cvar_SetEx("dm_shield", Cvar_VariableInteger("uvtime") > 0 ? "1" : "0", FROM_CONSOLE);
}

static void SM_SrvEnter(int n)
{
    SM_ReadMaps();
    // a hosted game started some other way: show what it runs
    if (SM_Hosting()) {
        SM_DetectGameType();
        SM_DetectBots();
    }
}

static void SM_BuildServer(int n)
{
    const bool hosting = SM_Hosting();
    const bool remote = SM_Remote();
    const char *map = Cvar_VariableString("mapname");
    const char *set = Cvar_VariableString("cl_splitSrvSet");
    const char *nowHelp = hosting ? "Now" : "For the next game";
    const char *nextHelp = hosting ? "Next map (Restart map applies it)" : "For the next game";
    const char *gt = cl_splitGameType->string;
    const bool team = strcmp(gt, "dm") != 0;
    const bool rounds = !strcmp(gt, "tp") || !strcmp(gt, "3t") || !strcmp(gt, "tourney");
    smRow_t *first = &smb->rows[smb->numRows], *r;

    SM_Title(remote ? "Server options - host only" : *set ? va("Server options - %s", set) : "Server options");
    if (remote)
        SM_Text("You are on another machine's server.");
    if (hosting) {
        SM_Help(SM_Action(Cvar_CountLatchedVars() ? "Restart map (changes waiting)" : "Restart map",
                          SM_SrvRestart, 0), "Starts this map again now; scores reset");
        SM_Help(SM_Sub(va("Change map now (%s)", map), &smSrvMaps), "Loads the map you pick at once");
        SM_Help(SM_Action("Next map", SM_SrvNextMap, 0), "The next installed map, now");
    } else if (!remote) {
        SM_Help(SM_Sub(va("Map: %s", cl_splitStartMap->string), &smSrvMaps), "The map the game starts on");
    }
    SM_Help(SM_Sub("Saved settings", &smSrvSets), "Save, load or delete sets of these options");
    SM_GameRows(false);
    SM_SrvChoice("Time limit", "timelimit", smTimeValues, smTimeLabels, q_countof(smTimeValues), nowHelp);
    if (!rounds && strcmp(gt, "ctf"))
        SM_SrvChoice("Frag limit", "fraglimit", smFragValues, smFragLabels, q_countof(smFragValues), nowHelp);
    if (rounds) {
        SM_SrvChoice("Round limit", "roundlimit", smRoundValues, smRoundLabels, q_countof(smRoundValues),
                     hosting ? "Now: rounds a team must win" : "Rounds a team must win");
        SM_SrvChoice("Round time limit", "roundtimelimit", smRoundTimeValues, smRoundTimeLabels,
                     q_countof(smRoundTimeValues), nowHelp);
    }
    if (!strcmp(gt, "ctf"))
        SM_SrvChoice("Capture limit", "capturelimit", smCapValues, smCapLabels, q_countof(smCapValues), nowHelp);
    if (SM_SrvHas("am")) {
        SM_Help(SM_Choice("Bots", "cl_splitBots", smBotValues, smBotLabels, q_countof(smBotValues)),
                hosting ? "Next map (Restart map applies it)" : "Computer players")->changed = SM_BotsChanged;
        SM_SrvChoice("Bot skill", "ltk_skill", smBotValues, smSkillLabels, q_countof(smSkillLabels), nowHelp);
    }
    if (SM_SrvHas("ltk_chat"))
        SM_SrvChoice("Bot chat", "ltk_chat", sm01, smOffOn, 2, hosting ? "Now: bots' text taunts" : "Bots' text taunts");
    SM_Help(SM_Choice("Humor sounds", "cl_splitHumorSounds", sm01, smOffOn, 2),
            "Now: farts, laughs from idle players")->changed = SM_HumorChanged;
    if (team)
        SM_Help(SM_Flag("Friendly fire", SM_GameVar("dmflags", "0"), DF_NO_FRIENDLY_FIRE_BIT, true), nowHelp);
    SM_Help(SM_Flag("Falling damage", SM_GameVar("dmflags", "0"), DF_NO_FALLING_BIT, true), nowHelp);
    SM_Help(SM_Choice("Infinite ammo", SM_GameVar("cl_splitInfiniteAmmo", "0"), sm01, smOffOn, 2),
            hosting ? "Now: endless spares, reloads; cheats on" :
                      "Endless spare mags (turns cheats on)")->changed = SM_InfAmmoChanged;
    if (!rounds) {
        if (!strcmp(gt, "tdm") && SM_SrvHas("teamdm_respawn"))
            SM_SrvChoice("Respawn delay", "teamdm_respawn", smDelayValues, smDelayLabels, q_countof(smDelayValues), nowHelp);
        else if (!strcmp(gt, "ctf") && SM_SrvHas("ctf_respawn"))
            SM_SrvChoice("Respawn delay", "ctf_respawn", smDelayValues, smDelayLabels, q_countof(smDelayValues), nowHelp);
        else if (!strcmp(gt, "dm"))
            SM_Help(SM_Flag("Respawn by itself", SM_GameVar("dmflags", "0"), DF_FORCE_RESPAWN_BIT, false),
                    "Dead players come back without pressing fire");
        if (SM_SrvHas("uvtime"))
            SM_SrvChoice("Spawn protection", "uvtime", smProtValues, smProtLabels, q_countof(smProtValues),
                         "Can't be hurt right after spawning")->changed = SM_ProtectChanged;
    }
    if (!strcmp(gt, "dm") || !strcmp(gt, "tdm"))
        SM_SrvChoice("Weapon choice (deathmatch)", "dm_choose", sm01, smOffOn, 2,
                     hosting ? "Next map: pick a weapon, not pickups" : "Pick a weapon instead of pickups");
    SM_SrvChoice("All weapons", "allweapon", sm01, smOffOn, 2, hosting ? "Now: at the next spawn" : "Spawn with every weapon");
    SM_SrvChoice("All items", "allitem", sm01, smOffOn, 2, hosting ? "Now: at the next spawn" : "Spawn with every item");
    SM_SrvChoice("Weapons carried", "weapons", smCarryValues, smCarryValues, q_countof(smCarryValues), nextHelp);
    SM_SrvChoice("Items carried", "items", smCarryValues, smCarryValues, q_countof(smCarryValues), nextHelp);
    r = SM_SrvChoice("Weapons", "wp_flags", smWeapSetValues, smWeapSetLabels, q_countof(smWeapSetValues),
                     hosting ? "Now: at the next spawn" : "Everyone spawns with it");
    r->arg = 1;     // another mix shows as "Custom"
    SM_Help(SM_Sub("Weapon bans", &smWeapBans), "Weapons nobody can choose or pick up");
    SM_Help(SM_Sub("Item bans", &smItemBans), "Items nobody can choose or pick up");
    if (team)
        SM_SrvChoice("Team grenades", "tgren", smTgrenValues, smTgrenLabels, q_countof(smTgrenValues),
                     hosting ? "Now: grenades each player gets" : "Grenades each player gets");
    if (team)
        SM_SrvChoice("Spectating dead players", "limchasecam", smChaseValues, smChaseLabels,
                     q_countof(smChaseValues), nextHelp);
    SM_SrvChoice("Darkmatch", "darkmatch", smDarkValues, smDarkLabels, q_countof(smDarkValues), nextHelp);
    if (!rounds) {
        SM_SrvChoice("Item respawn", "item_respawn", smRespawnValues, smRespawnLabels, q_countof(smRespawnValues), nowHelp);
        SM_SrvChoice("Weapon respawn", "weapon_respawn", smRespawnValues, smRespawnLabels,
                     q_countof(smRespawnValues), nowHelp);
    }
    SM_SrvChoice("Announcements", "use_rewards", sm01, smOffOn, 2,
                 hosting ? "Now: impressive, excellent, accuracy" : "Impressive, excellent, accuracy");
    SM_SrvChoice("Time / frag warnings", "use_warnings", sm01, smOffOn, 2,
                 hosting ? "Now: 3 minutes, 1 frag left..." : "3 minutes, 1 frag left...");
    SM_Help(SM_Action("Reset to defaults", SM_SrvReset, 0), "Every option back; the map stays");

    // another machine's server: everything shown, nothing changes
    if (remote) {
        for (; first < &smb->rows[smb->numRows]; first++) {
            if (first->type != SMR_TEXT) {
                first->disabled = true;
                Q_strlcpy(first->help, "Only for games you host", sizeof(first->help));
            }
        }
    }
}

static const smPage_t smServer = { SM_BuildServer, SM_SrvEnter };

// --- Change map: every installed map ---

static void SM_SrvPickMap(int n, int arg)
{
    const char *map;

    if (arg < 0 || arg >= smNumMaps)
        return;
    map = smMaps[arg];
    Cvar_SetByVar(cl_splitStartMap, map, FROM_CONSOLE);
    if (!SM_Hosting()) {
        SM_Back(n);
        SM_Note(n, va("Map: %s", map));
        return;
    }
    SM_LoadMap(n, map);
}

static void SM_BuildSrvMaps(int n)
{
    const char *cur = SM_Hosting() ? Cvar_VariableString("mapname") : cl_splitStartMap->string;
    int i;

    SM_Title(SM_Hosting() ? "Change map" : "Map");
    if (!smNumMaps)
        SM_Text("No maps found.");
    for (i = 0; i < smNumMaps && i < SM_MAX_ROWS - 2; i++)
        SM_Action(!Q_stricmp(smMaps[i], cur) ? va("%s (now)", smMaps[i]) : smMaps[i], SM_SrvPickMap, i);
}

static void SM_SrvMapsEnter(int n)
{
    const char *cur = SM_Hosting() ? Cvar_VariableString("mapname") : cl_splitStartMap->string;
    int i;

    SM_ReadMaps();
    // the selection starts on the current map
    for (i = 0; i < smNumMaps; i++) {
        if (!Q_stricmp(smMaps[i], cur)) {
            sm[n].stack[sm[n].depth - 1].sel = i;
            break;
        }
    }
}

static const smPage_t smSrvMaps = { SM_BuildSrvMaps, SM_SrvMapsEnter };

// --- weapon / item bans (wp_flags / itm_flags bits, Action's items[].flag) ---

static const char *const smWeapNames[] = {
    "MK23 pistol", "MP5/10 submachinegun", "M4 assault rifle", "M3 shotgun", "Handcannon",
    "Sniper rifle", "Dual MK23 pistols", "Combat knife", "Grenades"
};
static const char *const smItemNames[] = {
    "Silencer", "Stealth slippers", "Bandolier", "Kevlar vest", "Laser sight", "Kevlar helmet"
};

static void SM_BuildBans(int n, const char *title, const char *cvar, const char *def,
                         const char *const *names, int count)
{
    int i;

    SM_Title(title);
    for (i = 0; i < count; i++)
        SM_Help(SM_Flag(names[i], SM_GameVar(cvar, def), 1 << i, false),
                SM_Hosting() ? "On: allowed. Now: next spawn / pickup" : "On: allowed");
    SM_Text("Off = banned for everybody.");
}

static void SM_BuildWeapBans(int n)
{
    SM_BuildBans(n, "Weapon bans", "wp_flags", "511", smWeapNames, q_countof(smWeapNames));
}

static void SM_BuildItemBans(int n)
{
    SM_BuildBans(n, "Item bans", "itm_flags", "63", smItemNames, q_countof(smItemNames));
}

static const smPage_t smWeapBans = { SM_BuildWeapBans };
static const smPage_t smItemBans = { SM_BuildItemBans };

/*
--- saved server settings (the Quake3e fork's cl_splitsrv.c sets) ---

<homedir>/action/splitsrv/<name>.cfg: `set <cvar> "<value>"' lines of the
options above, dmflags, cl_splitGameType and cl_splitBots. Loading sets only
those names (a file is never executed). cl_splitSrvSet names the set in use.
*/

#define SM_SRV_DIR      "splitsrv"
#define SM_SRV_MAX      64
#define SM_SRV_TEXT     4096

static char     smSrvSetNames[SM_SRV_MAX][PROFILE_NAME_MAX + 1];
static int      smSrvNumSets;

static bool SM_SrvSetCvar(const char *name)
{
    int i;

    if (!strcmp(name, "dmflags") || !strcmp(name, "cl_splitGameType") || !strcmp(name, "cl_splitBots"))
        return true;
    for (i = 0; i < q_countof(smSrvDefaults); i++) {
        if (!strcmp(name, smSrvDefaults[i].name))
            return true;
    }
    return false;
}

static void SM_SrvReadSets(void)
{
    char **files, name[PROFILE_NAME_MAX + 1];
    int i, count = 0;

    smSrvNumSets = 0;
    files = (char **)FS_ListFiles(SM_SRV_DIR, ".cfg", FS_SEARCH_STRIPEXT | FS_TYPE_REAL | FS_PATH_GAME, &count);
    for (i = 0; files && i < count && smSrvNumSets < SM_SRV_MAX; i++) {
        if (!CL_ProfileSanitize(files[i], name, sizeof(name)) || strcmp(name, files[i]))
            continue;
        Q_strlcpy(smSrvSetNames[smSrvNumSets++], name, sizeof(smSrvSetNames[0]));
    }
    if (files)
        FS_FreeList((void **)files);
}

static void SM_SrvAddLine(char *buf, size_t size, const char *name)
{
    const cvar_t *v = Cvar_FindVar(name);
    const char *value;

    if (!v)
        return;
    value = v->latched_string ? v->latched_string : v->string;
    if (strpbrk(value, "\"\n\r;"))
        return;
    Q_strlcat(buf, va("set %s \"%s\"\n", name, value), size);
}

static void SM_SrvSaveDone(int n, const char *text)
{
    char name[PROFILE_NAME_MAX + 1], buf[SM_SRV_TEXT];
    qhandle_t f;
    int i;

    if (!CL_ProfileSanitize(text, name, sizeof(name))) {
        SM_Note(n, "Can't: not a usable name");
        return;
    }
    buf[0] = 0;
    SM_SrvAddLine(buf, sizeof(buf), "cl_splitGameType");
    SM_SrvAddLine(buf, sizeof(buf), "cl_splitBots");
    SM_SrvAddLine(buf, sizeof(buf), "dmflags");
    for (i = 0; i < q_countof(smSrvDefaults); i++)
        SM_SrvAddLine(buf, sizeof(buf), smSrvDefaults[i].name);
    FS_OpenFile(va(SM_SRV_DIR "/%s.cfg", name), &f, FS_MODE_WRITE | FS_FLAG_TEXT);
    if (!f) {
        SM_Note(n, "Can't write the file");
        return;
    }
    FS_FPrintf(f, "// splitscreen server options \"%s\"\n%s", name, buf);
    if (FS_CloseFile(f)) {
        SM_Note(n, "Can't write the file");
        return;
    }
    Cvar_Set("cl_splitSrvSet", name);
    Com_Printf("P1 menu: saved server options %s\n", name);
    SM_Note(n, va("Saved %s", name));
}

static void SM_SrvSaveAs(int n, int arg)
{
    SM_OpenKeyboard(n, "Name for these server options", Cvar_VariableString("cl_splitSrvSet"),
                    PROFILE_NAME_MAX, NULL, false);
    sm[n].kb.done = SM_SrvSaveDone;
}

static void SM_SrvLoad(int n, int arg)
{
    static char line[MAX_STRING_CHARS];
    char key[MAX_QPATH], name[MAX_QPATH], value[MAX_QPATH];
    const char *s, *p, *e;
    void *raw;
    size_t len;
    int count = 0;

    if (arg < 0 || arg >= smSrvNumSets)
        return;
    if (FS_LoadFileEx(va(SM_SRV_DIR "/%s.cfg", smSrvSetNames[arg]), &raw, FS_TYPE_REAL | FS_PATH_GAME,
                      TAG_FILESYSTEM) < 0) {
        SM_Note(n, "That set could not be read");
        return;
    }
    for (s = raw; *s; s = e ? e + 1 : s + len) {
        e = strchr(s, '\n');
        len = e ? (size_t)(e - s) : strlen(s);
        if (len >= sizeof(line))
            continue;
        memcpy(line, s, len);
        line[len] = 0;
        p = line;
        Q_strlcpy(key, COM_Parse(&p), sizeof(key));
        if (Q_stricmp(key, "set"))
            continue;
        Q_strlcpy(name, COM_Parse(&p), sizeof(name));
        Q_strlcpy(value, COM_Parse(&p), sizeof(value));
        if (!SM_SrvSetCvar(name))
            continue;
        SM_GameVar(name, SM_SrvDefault(name));
        Cvar_SetEx(name, value, FROM_CONSOLE);
        count++;
    }
    FS_FreeFile(raw);
    // the game type and bots into the game cvars (latched while hosting)
    if (SM_Hosting() || !sv_running->integer) {
        SM_ApplyGameType();
        SM_ApplyBots();
        SM_ApplyInfAmmo();
    }
    SM_ApplyHumor();
    Cvar_Set("cl_splitSrvSet", smSrvSetNames[arg]);
    Com_Printf("P1 menu: loaded server options %s (%d values)\n", smSrvSetNames[arg], count);
    SM_Back(n);
    SM_Note(n, SM_Hosting() ? va("Loaded %s; Restart map applies all", smSrvSetNames[arg])
                            : va("Loaded %s", smSrvSetNames[arg]));
}

static void SM_SrvDelete(int n, int arg)
{
    char ospath[MAX_OSPATH];

    if (arg < 0 || arg >= smSrvNumSets)
        return;
    if (Q_snprintf(ospath, sizeof(ospath), "%s/" SM_SRV_DIR "/%s.cfg", fs_gamedir, smSrvSetNames[arg]) < sizeof(ospath))
        remove(ospath);
    if (!Q_stricmp(Cvar_VariableString("cl_splitSrvSet"), smSrvSetNames[arg]))
        Cvar_Set("cl_splitSrvSet", "");
    SM_Note(n, va("Deleted %s", smSrvSetNames[arg]));
    SM_SrvReadSets();
}

static void SM_SrvSetsEnter(int n)
{
    SM_SrvReadSets();
}

static void SM_BuildSrvSets(int n)
{
    int i;

    SM_Title("Saved server settings");
    SM_Help(SM_Action("Save these settings as...", SM_SrvSaveAs, 0), "Name them on the keyboard");
    SM_Text(smSrvNumSets ? "Load:" : "No saved settings yet");
    for (i = 0; i < smSrvNumSets && i < SM_MAX_ROWS - 6; i++)
        SM_Help(SM_Action(va("  %s", smSrvSetNames[i]), SM_SrvLoad, i), "Load this set");
    if (smSrvNumSets)
        SM_Sub("Delete a set", &smSrvDelete);
}

static void SM_BuildSrvDelete(int n)
{
    int i;

    SM_Title("Delete saved settings");
    if (!smSrvNumSets)
        SM_Text("No saved settings.");
    for (i = 0; i < smSrvNumSets && i < SM_MAX_ROWS - 2; i++)
        SM_Help(SM_Action(va("Delete %s", smSrvSetNames[i]), SM_SrvDelete, i), "Removes the file at once");
}

static const smPage_t smSrvSets = { SM_BuildSrvSets, SM_SrvSetsEnter };
static const smPage_t smSrvDelete = { SM_BuildSrvDelete, SM_SrvSetsEnter };

/*
===============================================================================

DRAWING

===============================================================================
*/

#define SM_ACCENT       MakeColor( 15, 128, 235, 255)   // the AQtion menu blue
#define SM_ACCENT_DIM   MakeColor(  8,  64, 118, 242)
#define SM_SEL_BAR      MakeColor( 15, 128, 235,  77)
#define SM_SEL_TEXT     MakeColor(150, 200, 247, 255)
#define SM_PANEL        MakeColor( 10,   8,   8, 255)   // opaque: readable over a server layout
#define SM_BACKDROP     MakeColor(  0,   0,   0, 176)   // main menu: the engine menu dimmed behind
#define SM_TEXT         MakeColor(235, 235, 235, 255)
#define SM_VALUE        MakeColor(199, 199, 199, 255)
#define SM_DIM          MakeColor(153, 153, 153, 255)
#define SM_FOOT_BG      MakeColor(  0,   0,   0, 140)
#define SM_KEY_BG       MakeColor( 41,  38,  38, 242)
#define SM_FIELD_BG     MakeColor(  0,   0,   0, 217)

static void SM_Text_(int x, int y, const char *s, uint32_t color, int maxChars)
{
    R_SetColor(color);
    R_DrawString(x, y, 0, maxChars, s, sm_font);
}

// player 1 in a game (the active context may be another)
static bool SM_P1Active(void)
{
    const client_context_t *p1 = clx[0];

    return (p1 == cla ? cls.state : p1->conn.state) == ca_active;
}

// player n has a server layout up (Action's menu, scoreboard, inventory)
#pragma push_macro("cl")
#undef cl
static bool SM_LayoutUp(int n)
{
    const client_context_t *c = clx[n];

    return c && c->inUse && (c == cla ? cls.state : c->conn.state) == ca_active &&
        (c->cl.frame.ps.stats[STAT_LAYOUTS] & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY));
}
#pragma pop_macro("cl")

// where player n's overlay goes: its cell with N > 1, else the window
// (player 1's main-menu overlay: always the window, whatever cells are left)
static void SM_Rect(int n, vrect_t *rc)
{
    if (!(n == 0 && sm[0].mainMenu) && CL_SplitViewCount() > 1 && clx[n] && clx[n]->inUse) {
        *rc = clx[n]->cell;
    } else {
        rc->x = rc->y = 0;
        rc->width = r_config.width;
        rc->height = r_config.height;
    }
}

static void SM_DrawKeyboard(int n, int vw, int vh, int cols)
{
    smPlayer_t *s = &sm[n];
    char text[96], label[16];
    const char *lbl;
    int panelW, panelH, titleH, fieldH, kw, kh, footH, px, py, kx, ky, x, y, r, c, e, code, len;
    bool sel;

    panelW = (cols + 2) * CONCHAR_WIDTH;
    titleH = 14;
    fieldH = 14;
    kw = (panelW - 2 * CONCHAR_WIDTH) / KB_COLS;
    kh = 13;
    footH = 22;
    panelH = titleH + fieldH + KB_ROWS * kh + 4 + footH;
    px = (vw - panelW) / 2;
    py = (vh - panelH) / 2;

    R_DrawFill32(px, py, panelW, panelH, SM_PANEL);
    R_DrawFill32(px, py, panelW, titleH, SM_ACCENT_DIM);
    R_DrawFill32(px, py + titleH - 2, panelW, 2, SM_ACCENT);
    SM_Text_(px + CONCHAR_WIDTH, py + 3, s->kb.title, U32_WHITE, cols);

    // the text, a caret, n/max
    y = py + titleH + 2;
    R_DrawFill32(px + CONCHAR_WIDTH, y, panelW - 2 * CONCHAR_WIDTH, 11, SM_FIELD_BG);
    Q_snprintf(label, sizeof(label), "%d/%d", (int)strlen(s->kb.text), s->kb.max);
    len = strlen(s->kb.text);
    c = cols - 3 - (int)strlen(label);
    Q_snprintf(text, sizeof(text), "%s%s", len > c ? s->kb.text + len - c : s->kb.text,
               ((cls.realtime >> 9) & 1) ? "_" : "");
    SM_Text_(px + CONCHAR_WIDTH + 4, y + 2, text, U32_WHITE, c + 1);
    SM_Text_(px + panelW - CONCHAR_WIDTH - 4 - (int)strlen(label) * CONCHAR_WIDTH, y + 2, label, SM_DIM, 8);

    // keys
    kx = px + CONCHAR_WIDTH;
    ky = py + titleH + fieldH;
    for (r = 0; r < KB_ROWS; r++) {
        for (c = 0; c < KB_COLS; c = e) {
            code = SM_KbCode(r, c);
            for (e = c + 1; e < KB_COLS && SM_KbCode(r, e) == code; e++)
                ;
            x = kx + c * kw;
            y = ky + r * kh;
            sel = s->kb.row == r && s->kb.col >= c && s->kb.col < e;
            if (sel) {
                R_DrawFill32(x + 1, y + 1, (e - c) * kw - 2, kh - 2, SM_ACCENT);
                R_DrawFill32(x + 2, y + 2, (e - c) * kw - 4, kh - 4, SM_SEL_BAR);
            } else {
                R_DrawFill32(x + 1, y + 1, (e - c) * kw - 2, kh - 2, SM_KEY_BG);
            }
            switch (code) {
            case KB_DEL:    lbl = "Del"; break;
            case KB_SHIFT:  lbl = s->kb.shift ? "SHIFT" : "Shift"; break;
            case KB_DONE:   lbl = "Done"; break;
            case ' ':       lbl = "Space"; break;
            default:
                label[0] = (s->kb.shift && code >= 'a' && code <= 'z') ? code + 'A' - 'a' : code;
                label[1] = 0;
                lbl = label;
                break;
            }
            len = min((int)strlen(lbl), ((e - c) * kw - 2) / CONCHAR_WIDTH);
            SM_Text_(x + ((e - c) * kw - len * CONCHAR_WIDTH) / 2, y + (kh - CONCHAR_HEIGHT) / 2, lbl,
                     (code == KB_SHIFT && s->kb.shift) ? SM_SEL_TEXT : (sel ? U32_WHITE : SM_TEXT), len);
        }
    }

    // footer: the note, or the buttons
    y = py + panelH - footH;
    R_DrawFill32(px, y, panelW, footH, SM_FOOT_BG);
    if (s->note[0] && cls.realtime - s->noteTime < SM_NOTE_TIME)
        SM_Text_(px + CONCHAR_WIDTH, y + 2, s->note, SM_SEL_TEXT, cols);
    else
        SM_Text_(px + CONCHAR_WIDTH, y + 2, "A type  X delete  Y space  L3 shift", SM_DIM, cols);
    SM_Text_(px + CONCHAR_WIDTH, y + 12, "START done  B cancel", SM_DIM, cols);
}

static void SM_DrawPage(int n, int vw, int vh, int cols)
{
    smPlayer_t *s = &sm[n];
    char value[64], foot[96];
    const smRow_t *r;
    uint32_t col;
    int panelW, panelH, titleH, footH, lineH, maxVis, vis, px, py, rowY, x, y, i, idx, len, avail;
    int *sel, *top;

    sel = &s->stack[s->depth - 1].sel;
    top = &s->stack[s->depth - 1].top;

    lineH = 10;
    panelW = (cols + 2) * CONCHAR_WIDTH;
    titleH = 14;
    footH = 12;
    maxVis = max(3, (vh - 8 - titleH - footH - 4) / lineH);
    vis = min(s->numRows, maxVis);
    panelH = titleH + vis * lineH + 4 + footH;
    px = (vw - panelW) / 2;
    py = (vh - panelH) / 2;

    // keep the selection visible
    if (*sel < *top)
        *top = *sel;
    if (*sel >= *top + vis)
        *top = *sel - vis + 1;
    *top = max(0, min(*top, s->numRows - vis));

    R_DrawFill32(px, py, panelW, panelH, SM_PANEL);
    R_DrawFill32(px, py, panelW, titleH, SM_ACCENT_DIM);
    R_DrawFill32(px, py + titleH - 2, panelW, 2, SM_ACCENT);
    SM_Text_(px + CONCHAR_WIDTH, py + 3, s->title, U32_WHITE, cols);

    rowY = py + titleH + 2;
    for (i = 0; i < vis; i++) {
        idx = *top + i;
        if (idx >= s->numRows)
            break;
        r = &s->rows[idx];
        y = rowY + i * lineH;
        x = px + CONCHAR_WIDTH;

        if (idx == *sel) {
            R_DrawFill32(px + 2, y, panelW - 4, lineH, SM_SEL_BAR);
            R_DrawFill32(px + 2, y, 3, lineH, SM_ACCENT);
        }

        SM_RowValue(r, value, sizeof(value));
        if (idx == *sel && !r->disabled &&
            (r->type == SMR_CHOICE || r->type == SMR_SLIDER || r->type == SMR_FLAG || r->type == SMR_BIND)) {
            Q_strlcpy(foot, value, sizeof(foot));
            Q_snprintf(value, sizeof(value), "< %s >", foot);
        }
        len = strlen(value);
        if (len > max(cols / 2, cols - (int)strlen(r->label) - 1) && r->type != SMR_TEXT) {
            len = max(cols / 2, cols - (int)strlen(r->label) - 1);
            value[len] = 0;
        }
        avail = cols - (len ? len + 1 : 0);

        if (r->type == SMR_TEXT || r->disabled)
            col = SM_DIM;
        else
            col = (idx == *sel) ? SM_SEL_TEXT : SM_TEXT;
        SM_Text_(x, y + 1, r->label, col, r->type == SMR_TEXT ? cols : avail);
        if (len)
            SM_Text_(px + panelW - CONCHAR_WIDTH - len * CONCHAR_WIDTH, y + 1, value,
                     r->disabled ? SM_DIM : SM_VALUE, len);
    }

    // more rows above / below
    if (*top > 0)
        SM_Text_(px + panelW - CONCHAR_WIDTH, rowY - 2, "^", SM_DIM, 1);
    if (*top + vis < s->numRows)
        SM_Text_(px + panelW - CONCHAR_WIDTH, rowY + vis * lineH - 6, "v", SM_DIM, 1);

    // footer: the last note, or what the buttons do
    y = py + panelH - footH;
    R_DrawFill32(px, y, panelW, footH, SM_FOOT_BG);
    if (s->note[0] && cls.realtime - s->noteTime < SM_NOTE_TIME) {
        SM_Text_(px + CONCHAR_WIDTH, y + 2, s->note, SM_SEL_TEXT, cols);
    } else {
        r = (*sel < s->numRows) ? &s->rows[*sel] : NULL;
        if (r && r->help[0])
            Q_strlcpy(foot, r->help, sizeof(foot));     // what the row does / when it applies
        else
            Q_snprintf(foot, sizeof(foot), "A select  B back%s",
                       (r && (r->type == SMR_CHOICE || r->type == SMR_SLIDER || r->type == SMR_FLAG ||
                              r->type == SMR_BIND)) ? "  <> change" : "");
        SM_Text_(px + CONCHAR_WIDTH, y + 2, foot, r && r->help[0] ? SM_VALUE : SM_DIM, cols);
    }
}

static void SM_DrawPlayer(int n)
{
    smPlayer_t *s = &sm[n];
    vrect_t rc;
    float scale, kf;
    int k, vw, vh, cols;

    SM_Rect(n, &rc);
    if (rc.width <= 0 || rc.height <= 0)
        return;

    // console font, magnified so the cell is about 320..640 lines tall,
    // times the player's Splitscreen menu size (p<N>_cl_splitMenuScale, or
    // cl_splitMenuScale), as long as the panel keeps 200 lines
    k = max(1, rc.height / 320);
    kf = k * Q_clipf(SM_PlayerValue(n, cl_splitMenuScale->name), 0.5f, 3.0f);
    kf = max(min(kf, rc.height / 200.0f), 0.5f);
    scale = 1.0f / kf;
    vw = (int)(rc.width / kf);
    vh = (int)(rc.height / kf);
    cols = min(SM_COLS, vw / CONCHAR_WIDTH - 4);
    if (cols < 16)
        cols = max(8, vw / CONCHAR_WIDTH - 2);

    R_SetDrawRect(&rc);
    R_SetScale(scale);

    if (n == 0 && s->mainMenu)
        R_DrawFill32(0, 0, vw, vh, SM_BACKDROP);

    if (s->kb.open) {
        SM_DrawKeyboard(n, vw, vh, cols);
    } else {
        SM_Build(n);
        if (s->depth > 0)
            SM_DrawPage(n, vw, vh, cols);
    }

    R_ClearColor();
    R_SetScale(1.0f);
    R_SetDrawRect(NULL);
}

/*
====================
CL_SplitMenuDraw

Every open overlay / keyboard in its owner's cell, over the views and the
engine UI, under the console. Closes the ones that lost their player.
====================
*/
void CL_SplitMenuDraw(void)
{
    smPlayer_t *s;
    int n;

    SM_DrawMainMenuHint();

    for (n = 0; n < MAX_SPLITVIEW; n++) {
        s = &sm[n];
        if (s->depth <= 0 && !s->kb.open)
            continue;

        if (n == 0) {
            if (s->mainMenu) {
                // opened out of a game: a game starting closes it
                if (SM_P1Active()) {
                    s->depth = 0;
                    if (!s->kb.uiField)
                        memset(&s->kb, 0, sizeof(s->kb));
                }
            } else {
                // in game: the engine menu replaces it, leaving the game closes it
                if (s->depth > 0 && (!SM_P1Active() || (cls.key_dest & KEY_MENU)))
                    s->depth = 0;
                if (s->kb.open && !s->kb.uiField && !SM_P1Active())
                    memset(&s->kb, 0, sizeof(s->kb));
            }
            // the keyboard of an engine text field goes with the engine UI
            if (s->kb.open && s->kb.uiField && !(cls.key_dest & KEY_MENU))
                memset(&s->kb, 0, sizeof(s->kb));
        } else if (!CL_SplitPlayerLive(n)) {
            SM_Close(n);
        } else if (s->kb.open && s->kb.uiField && !(cls.key_dest & KEY_MENU)) {
            memset(&s->kb, 0, sizeof(s->kb));  // the owner's engine text field
        }
        if (s->depth <= 0 && !s->kb.open)
            continue;

        // looked up when drawn: the handle is renewed by a renderer restart
        sm_font = R_RegisterFont("conchars");
        SM_DrawPlayer(n);
    }
}

/*
===============================================================================

ENGINE UI (design 7.1): player 1's cell, the host page, text fields

===============================================================================
*/

/*
====================
CL_SplitMenuKeyEvent

The keyboard and mouse drive player 1's overlay while it is up (keys.c,
before the Escape handling): arrows / wheel move, Enter / left click select,
Escape / right click back, Tab closes; on the on-screen keyboard typed
characters go in, Backspace deletes. True if the key was taken.
====================
*/
// US layout shifted character, as keys.c's keyshift[] table
static int SM_ShiftChar(int key)
{
    static const char from[] = "1234567890-=,./;'[]`\\";
    static const char to[]   = "!@#$%^&*()_+<>?:\"{}~|";
    const char *p;

    if (key >= 'a' && key <= 'z')
        return key - 'a' + 'A';
    p = strchr(from, key);
    return p ? to[p - from] : key;
}

bool CL_SplitMenuKeyEvent(int key, bool down)
{
    smPlayer_t *s = &sm[0];
    int k;

    if (cls.key_dest & KEY_CONSOLE)
        return false;
    // F1 at the main menu: the Splitscreen page (Start's keyboard key)
    if (!CL_SplitMenuOpen(0)) {
        if (key != K_F1 || !(cls.key_dest & KEY_MENU) || !CL_SplitMenuCanOpenHost())
            return false;
        if (down && Key_IsDown(key) == 1)
            SM_Open(0, NULL);
        return down;    // never swallow a release (keys.c sends -commands)
    }
    switch (key) {
    case K_UPARROW: case K_KP_UPARROW: case K_MWHEELUP:         k = SMK_UP; break;
    case K_DOWNARROW: case K_KP_DOWNARROW: case K_MWHEELDOWN:   k = SMK_DOWN; break;
    case K_LEFTARROW: case K_KP_LEFTARROW:                      k = SMK_LEFT; break;
    case K_RIGHTARROW: case K_KP_RIGHTARROW:                    k = SMK_RIGHT; break;
    case K_ENTER: case K_KP_ENTER: case K_MOUSE1:               k = s->kb.open ? SMK_START : SMK_A; break;
    case K_ESCAPE: case K_MOUSE2:                               k = SMK_B; break;
    case K_TAB:                                                 k = SMK_START; break;
    case K_BACKSPACE:                                           k = s->kb.open ? SMK_X : SMK_B; break;
    default:
        // typing on the on-screen keyboard
        if (s->kb.open && key >= 32 && key < 127) {
            if (down) {
                if (Key_IsDown(K_SHIFT))
                    key = SM_ShiftChar(key);
                SM_KbPress(0, key);
            }
            return down;
        }
        return false;
    }
    if (down)
        CL_SplitMenuKey(0, k);
    return down;
}

// the Splitscreen page may open full screen (player 1 out of a game)
bool CL_SplitMenuCanOpenHost(void)
{
    // not while the page is already up (another pad's Start would take
    // P1 and reset the page) nor while a game is loading
    return !SM_P1Active() && CL_SplitPlayerLive(0) && !CL_SplitMenuOpen(0) &&
           cls.state <= ca_disconnected;
}

// splitmenu: the Splitscreen page out of a game; in a game player 1's own menu
static void SM_SplitMenu_f(void)
{
    if (!CL_SplitPlayerLive(0))
        return;
    if (!CL_SplitMenuOpen(0))
        SM_Open(0, NULL);
}

// splitquickmatch: the Splitscreen page's "Quick Local Match": the last used
// server options on a random installed map, never one of these: beer (a bot
// game there never finishes loading with Steam's DLL) and credits (AQtion's
// credits scene: one spawn point, players telefrag each other at once)
static bool SM_QuickMatchSkip(const char *map)
{
    return !Q_stricmp(map, "beer") || !Q_stricmp(map, "credits") || !SM_MapInstalled(map);
}

static void SM_QuickMatch_f(void)
{
    void **list;
    int n, i, tries;
    char map[MAX_QPATH];

    list = FS_ListFiles("maps", ".bsp", FS_SEARCH_STRIPEXT, &n);
    if (!list || n <= 0) {
        if (list)
            FS_FreeList(list);
        Com_Printf("%s: no maps found\n", Cmd_Argv(0));
        return;
    }
    i = Q_rand() % n;
    for (tries = 0; tries < n && SM_QuickMatchSkip((const char *)list[i]); tries++)
        i = (i + 1) % n;
    Q_strlcpy(map, (const char *)list[i], sizeof(map));
    FS_FreeList(list);
    Com_Printf("splitscreen: quick local match on %s\n", map);
    Cvar_SetByVar(cl_splitStartMap, map, FROM_CODE);
    Cbuf_InsertText(&cmd_buffer, va("splitstartgame \"%s\"\n", map));
}

// the main menu tells a pad player where the Splitscreen page is
static void SM_DrawMainMenuHint(void)
{
    static const char text[] = "Press Start on a pad (or F1) for Splitscreen";
    int k, vw, vh, len = sizeof(text) - 1;

    if (SM_P1Active() || CL_SplitMenuOpen(0) || !(cls.key_dest & KEY_MENU) ||
        (cls.key_dest & KEY_CONSOLE) || cls.state > ca_disconnected || PAD_ConnectedCount() <= 0)
        return;
    k = max(1, r_config.height / 360);
    vw = r_config.width / k;
    vh = r_config.height / k;
    sm_font = R_RegisterFont("conchars");
    R_SetScale(1.0f / k);
    R_DrawFill32((vw - (len + 2) * CONCHAR_WIDTH) / 2, vh - 16, (len + 2) * CONCHAR_WIDTH, 12, SM_FOOT_BG);
    SM_Text_((vw - len * CONCHAR_WIDTH) / 2, vh - 14, text, SM_SEL_TEXT, len);
    R_ClearColor();
    R_SetScale(1.0f);
}

// the player whose cell holds the engine UI: the one whose context opened
// it (player 1: keyboard, Escape, its pad's Start, its Game menu row;
// another player only through "p<N> menu_game" / "p<N> pushmenu ...")
static int smUIOwner;

void CL_SplitUIOpened(void)
{
    const int n = cla->playerNum;

    smUIOwner = (n > 0 && CL_SplitPlayerLive(n)) ? n : 0;
    if (smUIOwner)
        Com_Printf("P%d menu: AQtion's menu in player %d's view\n", n + 1, n + 1);
}

int CL_SplitUIOwner(void)
{
    if (smUIOwner > 0 && !CL_SplitPlayerLive(smUIOwner))
        smUIOwner = 0;
    return smUIOwner;
}

// the owner's AQtion menu size (p<N>_cl_splitMenuSize, player 1: the shared one)
float CL_SplitUIMenuSize(void)
{
    return SM_PlayerValue(CL_SplitUIOwner(), "cl_splitMenuSize");
}

// in game with N > 1 the engine UI is drawn in its owner's cell
bool CL_SplitUIRect(vrect_t *rc)
{
    const client_context_t *owner = clx[CL_SplitUIOwner()];

    if (CL_SplitViewCount() <= 1 || !owner || !owner->inUse || !SM_P1Active())
        return false;
    CL_SplitLayout(false);
    *rc = owner->cell;
    return rc->width > 0 && rc->height > 0;
}

// the UI scale in a cell: the per-cell HUD rule (screen.c SCR_CellScale),
// never larger than the window's
float CL_SplitUIScale(float window_scale, const vrect_t *rc)
{
    int k = Q_rint(1.0f / window_scale), c = 1;
    int s = min(rc->width * 9 / 16, rc->height);    // short side of a 16:9 equivalent

    if (s >= 2160)
        c = 4;
    else if (s >= 1080)
        c = 2;
    return 1.0f / max(1, min(k, c));
}

/*
====================
CL_SplitMenuUIEnter

Player 1's pad pressed A in the engine UI: on a text field ("Choose your
name", the host page's nothing) the on-screen keyboard edits it.
====================
*/
bool CL_SplitMenuUIEnter(void)
{
#if USE_UI
    char text[MAX_FIELD_TEXT], label[48];
    int maxChars;

    if (!UI_GetFocusedField(text, sizeof(text), label, sizeof(label), &maxChars))
        return false;
    SM_OpenKeyboard(CL_SplitUIOwner(), label[0] ? label : "Type", text, maxChars, NULL, true);
    return true;
#else
    return false;
#endif
}

/*
===============================================================================

COMMANDS

===============================================================================
*/

// pages `padmenu <page>' opens directly (a pad bind like "padmenu radio")
static const struct {
    const char      *name;
    const smPage_t  *page;
} smNamedPages[] = {
    { "radio", &smRadio },
    { "team", &smTeam },
    { "settings", &smSettings },
    { "controls", &smControls },
    { "buttons", &smButtons },
    { "profile", &smProfile },
    { "server", &smServer },
    { "splitscreen", &smHost },
};

// padmenu [player] [page]: toggles the active player's overlay (the default
// Start bind); a page name opens that page over the pause page
static void SM_PadMenu_f(void)
{
    const smPage_t *page = NULL;
    const char *arg;
    int n = cla->playerNum, i, a;

    for (a = 1; a < Cmd_Argc(); a++) {
        arg = Cmd_Argv(a);
        if (COM_IsUint(arg)) {
            n = Q_atoi(arg) - 1;
            continue;
        }
        for (i = 0; i < q_countof(smNamedPages) && Q_stricmp(arg, smNamedPages[i].name); i++)
            ;
        if (i == q_countof(smNamedPages)) {
            n = -1;
            break;
        }
        page = smNamedPages[i].page;
    }
    // the host's pages are player 1's
    if ((page == &smServer || page == &smHost) && n != 0)
        n = -1;
    if (n < 0 || n >= MAX_SPLITVIEW || !CL_SplitPlayerLive(n)) {
        Com_Printf("Usage: %s [player 1-%d in the game] [radio|team|settings|controls|buttons|profile|server|splitscreen]\n",
                   Cmd_Argv(0), MAX_SPLITVIEW);
        return;
    }
    if (CL_SplitMenuOpen(n)) {
        SM_Close(n);
        return;
    }
    SM_Open(n, page);   // player 1 out of a game: the host page full screen
}

/*
====================
CL_SplitMenuPadStart

Start on player 1's pad out of a game (main menu, engine UI up): player 1's
overlay full screen with the host page and player 1's profile (Q3 13.4).
False in a game, where Start over the engine UI is Escape.
====================
*/
bool CL_SplitMenuPadStart(void)
{
    if (SM_P1Active() || !CL_SplitPlayerLive(0))
        return false;
    SM_Open(0, NULL);
    return true;
}

// splitstartgame [map]: the host page's "Start multiplayer game" (design 11.2)
static void SM_StartGame_f(void)
{
    const char *map = Cmd_Argc() > 1 ? Cmd_Argv(1) : cl_splitStartMap->string;
    int maxc = Cvar_VariableInteger("maxclients");

    if (strpbrk(map, "\";\n\r")) {
        Com_Printf("%s: \"%s\" is not a map name\n", Cmd_Argv(0), map);
        return;
    }
    // never start on a map the install does not have (first run, a stale
    // saved choice): take one it has
    if (!SM_MapInstalled(map)) {
        const char *def = SM_DefaultMap();
        if (!*def) {
            Com_Printf("%s: no maps found\n", Cmd_Argv(0));
            return;
        }
        if (*map)
            Com_Printf("%s: map \"%s\" is not installed, using %s\n", Cmd_Argv(0), map, def);
        map = def;
        if (Cmd_Argc() < 2)
            Cvar_SetByVar(cl_splitStartMap, map, FROM_CODE);
    }
    // the overlay's game type and bots (Server options) go into the game cvars
    SM_ApplyGameType();
    SM_ApplyBots();
    SM_ApplyInfAmmo();
    SM_ApplyHumor();
    // room for 8 local players and the bots
    maxc = max(maxc, 8 + max(cl_splitBots->integer, 0));
    Com_Printf("splitscreen: starting %s, game type %s, bots %d, maxclients %d\n", map,
               cl_splitGameType->string, cl_splitBots->integer, maxc);
    // next, ahead of whatever the buffer holds (a menu row or a test script);
    // force: a full restart also while a game runs (latched cvars apply)
    Cbuf_InsertText(&cmd_buffer, va("forcemenuoff\nset deathmatch 1\nset coop 0\nset maxclients %d\nmap \"%s\" force\n",
                                    maxc, map));
}

static const cmdreg_t c_splitmenu[] = {
    { "padmenu", SM_PadMenu_f },
    { "splitstartgame", SM_StartGame_f },
    { "splitmenu", SM_SplitMenu_f },
    { "splitquickmatch", SM_QuickMatch_f },
    { NULL }
};

void CL_SplitMenuInit(void)
{
    memset(sm, 0, sizeof(sm));
    cl_splitStartMap = Cvar_Get("cl_splitStartMap", "wizs", CVAR_ARCHIVE);
    cl_splitGameType = Cvar_Get("cl_splitGameType", "tp", CVAR_ARCHIVE);
    cl_splitBots = Cvar_Get("cl_splitBots", "0", CVAR_ARCHIVE);
    cl_splitMenuScale = Cvar_Get("cl_splitMenuScale", "1", CVAR_ARCHIVE);
    cl_splitInfiniteAmmo = Cvar_Get("cl_splitInfiniteAmmo", "0", CVAR_ARCHIVE);
    cl_splitHumorSounds = Cvar_Get("cl_splitHumorSounds", "1", CVAR_ARCHIVE);
    Cvar_Get("cl_splitSrvSet", "", CVAR_ARCHIVE);
    Cvar_Get("ch_colour", "white", CVAR_ARCHIVE);
    Cmd_Register(c_splitmenu);
    // AQtion's "Play with bots" pages run "set allweapon $allweapon; set
    // allitem $allitem": before a game DLL has created them that printed
    // "Usage: set ..." twice. The game's own defaults:
    Cvar_Get("allweapon", "0", 0);
    Cvar_Get("allitem", "0", 0);
    // likewise AQtion's triggers.cfg counts kills in kill[#] / round[on],
    // set only once "<name> entered the game" was seen: a kill before that
    // printed "kill[#] is not a variable" and "Unknown operator '1'"
    Cvar_Get("kill[#]", "0", 0);
    Cvar_Get("round[on]", "0", 0);
}
