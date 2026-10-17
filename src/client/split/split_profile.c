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

// split_profile.c -- player profiles and guests (design 5.2; the Quake3e
// fork's design 12.3, 13.1, 14.3 and code/client/cl_splitprofile.c)
//
// Settings follow the person, not the player number. A profile is one file
//   <homedir>/<game>/profiles/<Name>.cfg
// of plain "keyword value" lines, slot-free: the player settings (name,
// skin, hand, fov, gender, msg, rate), the pad feel (joy_*) and the pad
// binds ("pbind <padkey> <command>"). Loading it into player n sets that
// slot's p<N>_* shadows (player 1: the shared cvars) and its pbind table;
// the reader ignores what it does not know.
//
// A player 2..8 without a profile is a guest: "Player N", the default
// player settings, player 1's pad feel and the default pad binds. A guest's
// changes last for its stay: everything goes back to the guest defaults
// when the player leaves. A named profile's changes are saved (polled,
// written about a second after the last change, and on leave / quit).
//
// Player 1 plays with its own config (q2config.cfg, exactly as upstream)
// unless a profile is picked for it; its own settings are then kept in
// profiles/_p1keep.cfg and put back when it drops the profile, at quit, or
// at the next start after a crash.
//
// profiles/_pads.cfg remembers the profile last used with each pad (by its
// device key, guid#n): an extra joining with that pad plays as it again.
//
// Nothing here runs or writes unless a profile is used.

#include "../client.h"
#include "gamepad.h"

#define PROF_DIR            "profiles"
#define PROF_PADS           PROF_DIR "/_pads.cfg"
#define PROF_P1KEEP         PROF_DIR "/_p1keep.cfg"
#define PROF_MAX            64
#define PROF_TEXT_MAX       8192
#define PROF_LINE_MAX       512
#define PROF_POLL_MSEC      500
#define PROF_SAVE_DELAY     1000    // a change is written this long after it was first seen
#define PROF_MAX_PADS       32

// the per-player settings a profile holds (base cvar names)
static const char *const prof_userinfo[] = {
    "name", "skin", "hand", "fov", "gender", "msg", "rate"
};

static const char *const prof_feel[] = {
    "joy_invertPitch", "joy_yawSpeed", "joy_pitchSpeed", "joy_deadzone", "joy_deadzoneOuter",
    "joy_aimCurve", "joy_aimExponent", "joy_turnBoost", "joy_turnBoostTime", "joy_zoomScale",
    "joy_triggerThreshold", "joy_stickThreshold",
    // HUD look (0.0.0.23: screen.c reads p<N>_ in the player's cell)
    "scr_alpha", "ch_colour", "ch_red", "ch_green", "ch_blue",
    // 0.0.0.25: crosshair picture / size, text size, both menu sizes
    "crosshair", "ch_size", "cl_splitTextSize", "cl_splitMenuSize", "cl_splitMenuScale"
};

typedef struct {
    bool        named;
    bool        fromPad;            // player 1: loaded because its pad remembers it
    char        name[PROFILE_NAME_MAX + 1];
    char        *saved;             // what the file holds (Z_Malloc'd PROF_TEXT_MAX)
    bool        dirty;
    unsigned    dirtyTime;
} profSlot_t;

static profSlot_t   profSlots[MAX_SPLITVIEW];
static bool         prof_init;
static unsigned     prof_nextPoll;

static char         profList[PROF_MAX][PROFILE_NAME_MAX + 1];
static int          profCount;

static struct {
    char    device[64];
    char    name[PROFILE_NAME_MAX + 1];
} profPads[PROF_MAX_PADS];
static int          profNumPads;

static char         *prof_p1keep;   // player 1's own settings while it plays as a profile

/*
===============================================================================

NAMES AND FILES

===============================================================================
*/

static bool Prof_DeviceName(const char *s)
{
    static const char *const dev[] = { "con", "nul", "aux", "prn", "com", "lpt", "conin$", "conout$" };
    int i;
    size_t len;

    for (i = 0; i < q_countof(dev); i++) {
        len = strlen(dev[i]);
        if (Q_strncasecmp(s, dev[i], len))
            continue;
        if (i == 4 || i == 5) {     // COM1-9, LPT1-9
            if (s[len] < '1' || s[len] > '9')
                continue;
            len++;
        }
        if (!s[len])
            return true;
    }
    return false;
}

/*
====================
CL_ProfileSanitize

Letters, digits, '-' and '_' (a space becomes '_'); color codes and other
characters dropped; no leading / trailing separators; at most
PROFILE_NAME_MAX characters. False if nothing is left or the name is
reserved ("guest", Windows device names). The result is the file name.
====================
*/
bool CL_ProfileSanitize(const char *in, char *out, size_t size)
{
    size_t len = 0, max = min(size - 1, PROFILE_NAME_MAX);
    int c;

    out[0] = 0;
    if (!in)
        return false;
    for (; *in && len < max; in++) {
        if (*in == '^' && in[1]) {      // color code
            in++;
            continue;
        }
        c = *in;
        if (c == ' ' || c == '\t')
            c = '_';
        if (!Q_isalnum(c) && c != '_' && c != '-')
            continue;
        if (!len && (c == '_' || c == '-'))
            continue;   // '_' starts the engine's own files
        if (c == '_' && len && out[len - 1] == '_')
            continue;
        out[len++] = c;
    }
    while (len > 0 && (out[len - 1] == '_' || out[len - 1] == '-'))
        len--;
    out[len] = 0;
    if (!len || !Q_stricmp(out, "guest") || Prof_DeviceName(out)) {
        out[0] = 0;
        return false;
    }
    return true;
}

static const char *Prof_Path(const char *name)
{
    return va(PROF_DIR "/%s.cfg", name);
}

// a whole file from the home game dir, NUL-terminated (FS_FreeFile), or NULL
static char *Prof_ReadFile(const char *path)
{
    void *buf;

    if (FS_LoadFileEx(path, &buf, FS_TYPE_REAL | FS_PATH_GAME, TAG_FILESYSTEM) < 0)
        return NULL;
    return buf;
}

static bool Prof_WriteFile(const char *path, const char *header, const char *text)
{
    qhandle_t f;
    int64_t ret;
    bool ok;

    ret = FS_OpenFile(path, &f, FS_MODE_WRITE | FS_FLAG_TEXT);
    if (!f) {
        Com_EPrintf("profile: couldn't write %s: %s\n", path, Q_ErrorString(ret));
        return false;
    }
    FS_FPrintf(f, "// %s\n", header);
    ok = FS_Write(text, strlen(text), f) == (int)strlen(text);
    if (FS_CloseFile(f))
        ok = false;
    if (!ok)
        Com_EPrintf("profile: error writing %s\n", path);
    return ok;
}

static void Prof_RemoveFile(const char *path)
{
    char ospath[MAX_OSPATH];

    if (Q_snprintf(ospath, sizeof(ospath), "%s/%s", fs_gamedir, path) < sizeof(ospath))
        remove(ospath);
}

static bool Prof_Exists(const char *name)
{
    return FS_FileExistsEx(Prof_Path(name), FS_TYPE_REAL | FS_PATH_GAME);
}

/*
===============================================================================

SETTINGS <-> TEXT

===============================================================================
*/

// the cvar holding player n's value of base setting `base' (NULL: none)
static cvar_t *Prof_Var(int n, const char *base, bool create)
{
    cvar_t *b, *v;

    if (!n)
        return Cvar_FindVar(base);
    v = Cvar_FindVar(va("p%d_%s", n + 1, base));
    if (v || !create)
        return v;
    b = Cvar_FindVar(base);
    if (!b)
        return NULL;    // e.g. joy_* in a build without pads
    return Cvar_Get(va("p%d_%s", n + 1, base), b->string, 0);
}

static void Prof_AddLine(char *buf, size_t size, const char *key, const char *value)
{
    if (strchr(value, '"') || strchr(value, '\n'))
        return;     // never valid in userinfo / these cvars; keep the file parseable
    Q_strlcat(buf, va("%s \"%s\"\n", key, value), size);
}

/*
Bind set revisions: a profile written before a revision gets its upgrade
once, when it is loaded (Prof_Apply); the next save writes the revision.
  2 (0.0.0.31): the default LT bind is `lens` (TNG: zoom cycle that wraps
    6x -> 1x) instead of `lens in` (stops at 6x); an LT bind of exactly
    `lens in` becomes `lens`. "Zoom in" picked afterwards stays.
*/
#define PROF_BINDS_KEY  "binds"
#define PROF_BINDS_REV  2

static bool prof_upgraded;      // the last Prof_Apply changed the text's binds

// player n's current settings as profile text
static void Prof_Build(int n, char *buf, size_t size)
{
    cvar_t *v, *b;
    int i;

    buf[0] = 0;
    for (i = 0; i < q_countof(prof_userinfo); i++) {
        v = Prof_Var(n, prof_userinfo[i], false);
        if (v)
            Prof_AddLine(buf, size, prof_userinfo[i], v->string);
    }
    for (i = 0; i < q_countof(prof_feel); i++) {
        // a slot that never changed its feel uses player 1's (no p<N>_ cvar)
        if (!(b = Cvar_FindVar(prof_feel[i])))
            continue;
        v = Prof_Var(n, prof_feel[i], false);
        Prof_AddLine(buf, size, prof_feel[i], v ? v->string : b->string);
    }
    // the bind set's revision (Prof_Apply's one-time upgrades)
    Q_strlcat(buf, va("%s \"%d\"\n", PROF_BINDS_KEY, PROF_BINDS_REV), size);
    PAD_ProfileWriteBinds(n, buf, size);
}

static bool Prof_IsSetting(const char *key)
{
    int i;

    for (i = 0; i < q_countof(prof_userinfo); i++)
        if (!Q_stricmp(key, prof_userinfo[i]))
            return true;
    for (i = 0; i < q_countof(prof_feel); i++)
        if (!Q_stricmp(key, prof_feel[i]))
            return true;
    return false;
}

static void Prof_Set(int n, const char *base, const char *value)
{
    cvar_t *v = Prof_Var(n, base, true);

    // from the console: the shadows' callbacks resend the userinfo
    if (v && strcmp(v->string, value))
        Cvar_SetByVar(v, value, FROM_CONSOLE);
}

/*
====================
Prof_Apply

Profile text into player n: known settings set, pad binds replaced (the
default set if the text has none). Returns the number of lines ignored.
====================
*/
static int Prof_Apply(int n, const char *text)
{
    // any line Prof_Build can write fits (a pbind of a long command too)
    static char line[PROF_TEXT_MAX], value[PROF_TEXT_MAX];
    char key[MAX_QPATH], arg[MAX_QPATH];
    const char *s = text, *p, *e;
    int binds = 0, bad = 0, rev = 1;
    size_t len;

    prof_upgraded = false;
    PAD_ProfileClearBinds(n);

    while (*s) {
        e = strchr(s, '\n');
        len = e ? (size_t)(e - s) : strlen(s);
        if (len < sizeof(line)) {
            memcpy(line, s, len);
            line[len] = 0;
            p = line;
            Q_strlcpy(key, COM_Parse(&p), sizeof(key));
            if (!key[0]) {
                ;   // blank or comment
            } else if (!Q_stricmp(key, "pbind")) {
                Q_strlcpy(arg, COM_Parse(&p), sizeof(arg));
                Q_strlcpy(value, COM_Parse(&p), sizeof(value));
                if (value[0] && PAD_ProfileSetBind(n, arg, value))
                    binds++;
                else if (value[0])
                    bad++;
            } else if (!Q_stricmp(key, PROF_BINDS_KEY)) {
                rev = Q_atoi(COM_Parse(&p));
            } else if (Prof_IsSetting(key)) {
                Q_strlcpy(value, COM_Parse(&p), sizeof(value));
                Prof_Set(n, key, value);
            } else {
                bad++;
            }
        } else {
            bad++;
        }
        s += len;
        if (*s == '\n')
            s++;
    }

    if (!binds) {
        PAD_ProfileDefaultBinds(n);
    } else if (rev < 2 && !strcmp(PAD_GetBind(n, "PAD_LT"), "lens in")) {
        PAD_ProfileSetBind(n, "PAD_LT", "lens");
        prof_upgraded = true;
        Com_Printf("profile: P%d's LT is now Zoom (cycle) (`lens`, wraps to 1x)\n", n + 1);
    }
    return bad;
}

// player 2..8 back to the guest defaults (Q3 13.1)
static void Prof_ApplyGuest(int n)
{
    cvar_t *v, *b;
    int i;

    if (n < 1)
        return;
    for (i = 0; i < q_countof(prof_userinfo); i++) {
        v = Prof_Var(n, prof_userinfo[i], false);
        if (v)
            Cvar_SetByVar(v, i ? v->default_string : va("Player %d", n + 1), FROM_CONSOLE);
    }
    for (i = 0; i < q_countof(prof_feel); i++) {
        v = Prof_Var(n, prof_feel[i], false);
        b = Cvar_FindVar(prof_feel[i]);
        if (v && b)
            Cvar_SetByVar(v, b->string, FROM_CONSOLE);
    }
    PAD_ProfileDefaultBinds(n);
}

/*
===============================================================================

PAD -> LAST PROFILE

===============================================================================
*/

static void Prof_SavePads(void)
{
    char text[PROF_TEXT_MAX];
    int i;

    text[0] = 0;
    for (i = 0; i < profNumPads; i++)
        Q_strlcat(text, va("pad \"%s\" \"%s\"\n", profPads[i].device, profPads[i].name), sizeof(text));
    Prof_WriteFile(PROF_PADS, "AQtion splitscreen: the profile last used with each pad. Written by the engine.", text);
}

static void Prof_LoadPads(void)
{
    char *text, line[PROF_LINE_MAX], dev[64], name[PROFILE_NAME_MAX + 1];
    const char *s, *p, *e;
    size_t len;

    profNumPads = 0;
    if (!(text = Prof_ReadFile(PROF_PADS)))
        return;
    for (s = text; *s && profNumPads < PROF_MAX_PADS; ) {
        e = strchr(s, '\n');
        len = e ? (size_t)(e - s) : strlen(s);
        if (len < sizeof(line)) {
            memcpy(line, s, len);
            line[len] = 0;
            p = line;
            if (!Q_stricmp(COM_Parse(&p), "pad")) {
                Q_strlcpy(dev, COM_Parse(&p), sizeof(dev));
                if (dev[0] && CL_ProfileSanitize(COM_Parse(&p), name, sizeof(name))) {
                    Q_strlcpy(profPads[profNumPads].device, dev, sizeof(profPads[0].device));
                    Q_strlcpy(profPads[profNumPads].name, name, sizeof(profPads[0].name));
                    profNumPads++;
                }
            }
        }
        s += len;
        if (*s == '\n')
            s++;
    }
    FS_FreeFile(text);
}

// remember (name) or forget (NULL) the profile of a pad; written when changed
static void Prof_SetPad(const char *device, const char *name)
{
    int i;

    if (!device || !device[0])
        return;
    for (i = 0; i < profNumPads && strcmp(profPads[i].device, device); i++)
        ;
    if (!name) {
        if (i == profNumPads)
            return;
        profPads[i] = profPads[--profNumPads];
    } else {
        if (i < profNumPads && !Q_stricmp(profPads[i].name, name))
            return;
        if (i == profNumPads) {
            if (profNumPads == PROF_MAX_PADS) {
                memmove(profPads, profPads + 1, sizeof(profPads[0]) * (PROF_MAX_PADS - 1));
                i = --profNumPads;
            }
            profNumPads++;
            Q_strlcpy(profPads[i].device, device, sizeof(profPads[i].device));
        }
        Q_strlcpy(profPads[i].name, name, sizeof(profPads[i].name));
        Com_Printf("profile: pad %s remembers \"%s\"\n", device, name);
    }
    Prof_SavePads();
}

static const char *Prof_PadProfile(const char *device)
{
    int i;

    if (!device)
        return NULL;
    for (i = 0; i < profNumPads; i++)
        if (!strcmp(profPads[i].device, device))
            return profPads[i].name;
    return NULL;
}

/*
===============================================================================

THE LIST

===============================================================================
*/

static int Prof_Compare(const void *a, const void *b)
{
    return Q_stricmp((const char *)a, (const char *)b);
}

void CL_ProfileRefresh(void)
{
    char **files, name[PROFILE_NAME_MAX + 1];
    int i, count = 0;

    profCount = 0;
    files = (char **)FS_ListFiles(PROF_DIR, ".cfg", FS_SEARCH_STRIPEXT | FS_TYPE_REAL | FS_PATH_GAME, &count);
    for (i = 0; files && i < count && profCount < PROF_MAX; i++) {
        // only names the engine could have written (_pads, _p1keep are not profiles)
        if (!CL_ProfileSanitize(files[i], name, sizeof(name)) || strcmp(name, files[i]))
            continue;
        Q_strlcpy(profList[profCount++], name, sizeof(profList[0]));
    }
    if (files)
        FS_FreeList((void **)files);
    qsort(profList, profCount, sizeof(profList[0]), Prof_Compare);
}

int CL_ProfileCount(void)
{
    return profCount;
}

const char *CL_ProfileListName(int i)
{
    return (i >= 0 && i < profCount) ? profList[i] : "";
}

static int Prof_UserOf(const char *name)
{
    int n;

    for (n = 0; n < MAX_SPLITVIEW; n++)
        if (profSlots[n].named && !Q_stricmp(profSlots[n].name, name))
            return n;
    return -1;
}

int CL_ProfileListUser(int i)
{
    return (i >= 0 && i < profCount) ? Prof_UserOf(profList[i]) : -1;
}

bool CL_ProfileNamed(int n)
{
    return n >= 0 && n < MAX_SPLITVIEW && profSlots[n].named;
}

const char *CL_ProfileName(int n)
{
    return CL_ProfileNamed(n) ? profSlots[n].name : "";
}

/*
===============================================================================

SLOTS

===============================================================================
*/

static char *Prof_Saved(int n)
{
    if (!profSlots[n].saved)
        profSlots[n].saved = Z_Malloc(PROF_TEXT_MAX);
    return profSlots[n].saved;
}

static bool Prof_Write(int n)
{
    profSlot_t *p = &profSlots[n];
    char *text = Prof_Saved(n);

    Prof_Build(n, text, PROF_TEXT_MAX);
    p->dirty = false;
    if (!Prof_WriteFile(Prof_Path(p->name), va("AQtion splitscreen player profile \"%s\". Written by the engine.", p->name), text))
        return false;
    Com_Printf("profile: P%d's settings saved to \"%s\"\n", n + 1, p->name);
    return true;
}

// a named slot's changes not yet on disk: written now
static void Prof_Flush(int n)
{
    profSlot_t *p = &profSlots[n];
    char text[PROF_TEXT_MAX];

    if (!p->named)
        return;
    Prof_Build(n, text, sizeof(text));
    if (p->dirty || strcmp(text, Prof_Saved(n)))
        Prof_Write(n);
}

// player 1 is about to play as a profile: its own settings are kept
static void Prof_KeepP1(void)
{
    if (prof_p1keep)
        return;
    prof_p1keep = Z_Malloc(PROF_TEXT_MAX);
    Prof_Build(0, prof_p1keep, PROF_TEXT_MAX);
    Prof_WriteFile(PROF_P1KEEP, "AQtion splitscreen: player 1's own settings while it plays as a profile "
                   "(restored at the next start if the game stopped). Written by the engine.", prof_p1keep);
}

static void Prof_RestoreP1(void)
{
    if (!prof_p1keep)
        return;
    Prof_Apply(0, prof_p1keep);
    Z_Free(prof_p1keep);
    prof_p1keep = NULL;
    Prof_RemoveFile(PROF_P1KEEP);
    Com_Printf("profile: P1 has its own settings again\n");
}

// player n stops playing as its profile (saved first)
static void Prof_Release(int n)
{
    profSlot_t *p = &profSlots[n];

    Prof_Flush(n);
    p->named = false;
    p->fromPad = false;
    p->name[0] = 0;
    p->dirty = false;
    if (n == 0)
        Prof_RestoreP1();
    else
        Prof_ApplyGuest(n);
}

static bool Prof_SlotOk(int n, char *err, size_t size)
{
    if (n < 0 || n >= MAX_SPLITVIEW) {
        Q_strlcpy(err, "no such player", size);
        return false;
    }
    if (n > 0 && !CL_SplitPlayerLive(n)) {
        Q_snprintf(err, size, "player %d is not in the game", n + 1);
        return false;
    }
    return true;
}

/*
====================
CL_ProfileLoad

Player n plays as profile `name' (NULL: as a guest; player 1: its own
settings). False with a reason if the profile does not exist or another
player uses it.
====================
*/
bool CL_ProfileLoad(int n, const char *name, char *err, size_t size)
{
    char clean[PROFILE_NAME_MAX + 1], *text;
    const char *device;
    profSlot_t *p;
    int user, bad;

    if (!Prof_SlotOk(n, err, size))
        return false;
    p = &profSlots[n];

    if (!name || !*name || !Q_stricmp(name, "guest")) {
        if (p->named)
            Prof_Release(n);
        else if (n > 0)
            Prof_ApplyGuest(n);
        if (n)
            Com_Printf("profile: P%d is a guest\n", n + 1);
        return true;
    }

    if (!CL_ProfileSanitize(name, clean, sizeof(clean)) || !Prof_Exists(clean)) {
        Q_snprintf(err, size, "no profile \"%s\"", name);
        return false;
    }
    user = Prof_UserOf(clean);
    if (user == n)
        return true;
    if (user >= 0) {
        Q_snprintf(err, size, "\"%s\" is in use by player %d", clean, user + 1);
        return false;
    }
    if (!(text = Prof_ReadFile(Prof_Path(clean)))) {
        Q_snprintf(err, size, "couldn't read profile \"%s\"", clean);
        return false;
    }

    if (p->named)
        Prof_Flush(n);
    else if (n == 0)
        Prof_KeepP1();
    PAD_ReleasePlayer(n);

    // missing keys: the guest defaults (player 1: its own values, also when
    // it switches from another profile, which must not leak into this one)
    if (n > 0)
        Prof_ApplyGuest(n);
    else if (prof_p1keep)
        Prof_Apply(0, prof_p1keep);
    bad = Prof_Apply(n, text);
    FS_FreeFile(text);
    if (bad)
        Com_Printf("profile: %s: %d line(s) ignored\n", Prof_Path(clean), bad);

    p->named = true;
    p->fromPad = false;     // picked by hand (CL_ProfilePadJoined sets it after)
    Q_strlcpy(p->name, clean, sizeof(p->name));
    p->dirty = false;
    Prof_Build(n, Prof_Saved(n), PROF_TEXT_MAX);
    if (prof_upgraded)
        Prof_Write(n);      // the upgrade happens once

    device = PAD_PlayerDevice(n);
    if (device)
        Prof_SetPad(device, clean);
    Com_Printf("profile: P%d plays as \"%s\"\n", n + 1, clean);
    return true;
}

/*
====================
CL_ProfileSave

name NULL: write player n's profile now. Otherwise a new profile `name'
from player n's current settings (a guest keeping its settings), which the
player then plays as; its in-game name becomes the profile name (Q3).
====================
*/
bool CL_ProfileSave(int n, const char *name, char *err, size_t size)
{
    char clean[PROFILE_NAME_MAX + 1];
    const char *device;
    profSlot_t *p;
    cvar_t *v;

    if (!Prof_SlotOk(n, err, size))
        return false;
    p = &profSlots[n];

    if (!name) {
        if (!p->named) {
            Q_strlcpy(err, "no profile in use: save as a new one", size);
            return false;
        }
        if (!Prof_Write(n)) {
            Q_strlcpy(err, "couldn't write the profile", size);
            return false;
        }
        return true;
    }

    if (!CL_ProfileSanitize(name, clean, sizeof(clean))) {
        Q_strlcpy(err, "type a name (letters, digits, - _)", size);
        return false;
    }
    if (Prof_UserOf(clean) == n)
        return CL_ProfileSave(n, NULL, err, size);     // its own name: just save
    if (Prof_UserOf(clean) >= 0) {
        Q_snprintf(err, size, "\"%s\" is in use by player %d", clean, Prof_UserOf(clean) + 1);
        return false;
    }
    if (Prof_Exists(clean)) {
        Q_snprintf(err, size, "\"%s\" exists: pick another name", clean);
        return false;
    }

    if (p->named)
        Prof_Flush(n);      // the old one keeps its own changes
    else if (n == 0)
        Prof_KeepP1();

    v = Prof_Var(n, "name", false);
    if (v)
        Cvar_SetByVar(v, clean, FROM_CONSOLE);

    p->named = true;
    Q_strlcpy(p->name, clean, sizeof(p->name));
    if (!Prof_Write(n)) {
        p->named = false;
        p->name[0] = 0;
        Q_strlcpy(err, "couldn't write the profile", size);
        return false;
    }
    CL_ProfileRefresh();

    device = PAD_PlayerDevice(n);
    if (device)
        Prof_SetPad(device, clean);
    Com_Printf("profile: P%d created profile \"%s\"%s\n", n + 1, clean,
               strcmp(clean, name) ? va(" (typed \"%s\")", name) : "");
    return true;
}

/*
===============================================================================

HOOKS

===============================================================================
*/

void CL_ProfilePadJoined(int n, const char *device)
{
    const char *name = Prof_PadProfile(device);
    char err[MAX_QPATH * 2];

    if (!prof_init || n < 0 || n >= MAX_SPLITVIEW)
        return;

    // player 1 (Q3 14.3): the profile follows its pad; one the previous pad
    // brought goes (player 1's own settings back) unless this pad has it too
    if (n == 0 && profSlots[0].fromPad && (!name || Q_stricmp(name, profSlots[0].name))) {
        Com_Printf("profile: P1's pad changed: \"%s\" put away\n", profSlots[0].name);
        Prof_Release(0);
    }

    if (!name || profSlots[n].named)
        return;
    if (!CL_ProfileLoad(n, name, err, sizeof(err))) {
        if (n)
            Com_Printf("profile: P%d joins as a guest (pad %s's \"%s\": %s)\n", n + 1, device, name, err);
        else
            Com_Printf("profile: P1 keeps its own settings (pad %s's \"%s\": %s)\n", device, name, err);
        return;
    }
    if (n == 0) {
        profSlots[0].fromPad = true;
        Com_Printf("profile: P1's pad %s brought \"%s\" (own settings kept in " PROF_P1KEEP ")\n",
                   device, name);
    }
}

// the pad layer: player n's pad stepped out (not unplugged); player 1 puts
// away a profile its pad brought
void CL_ProfilePadReleased(int n)
{
    if (!prof_init || n != 0 || !profSlots[0].fromPad)
        return;
    Com_Printf("profile: P1's pad released: \"%s\" put away\n", profSlots[0].name);
    Prof_Release(0);
}

void CL_ProfilePadAssigned(int n, const char *device)
{
    if (prof_init && n >= 0 && n < MAX_SPLITVIEW && profSlots[n].named)
        Prof_SetPad(device, profSlots[n].name);
}

// split.c: player n (2..8) is gone
void CL_ProfileLeave(int n)
{
    if (!prof_init || n < 1 || n >= MAX_SPLITVIEW)
        return;
    if (profSlots[n].named)
        Com_Printf("profile: P%d (\"%s\") left\n", n + 1, profSlots[n].name);
    Prof_Release(n);    // named: saved; everyone: the guest defaults for the next one
}

void CL_ProfileFrame(void)
{
    unsigned now = Sys_Milliseconds();
    char text[PROF_TEXT_MAX];
    profSlot_t *p;
    int n;

    if (!prof_init || now - prof_nextPoll > INT_MAX)
        return;     // not yet
    prof_nextPoll = now + PROF_POLL_MSEC;

    for (n = 0, p = profSlots; n < MAX_SPLITVIEW; n++, p++) {
        if (!p->named)
            continue;
        Prof_Build(n, text, sizeof(text));
        if (!strcmp(text, Prof_Saved(n))) {
            p->dirty = false;
            continue;
        }
        if (!p->dirty) {
            p->dirty = true;
            p->dirtyTime = now;
        } else if (now - p->dirtyTime >= PROF_SAVE_DELAY) {
            Prof_Write(n);
        }
    }
}

/*
===============================================================================

CONSOLE

===============================================================================
*/

static int Prof_SlotArg(const char *s)
{
    int n = Q_atoi(s) - 1;

    if (!COM_IsUint(s) || n < 0 || n >= MAX_SPLITVIEW) {
        Com_Printf("player must be 1..%d, not \"%s\"\n", MAX_SPLITVIEW, s);
        return -1;
    }
    return n;
}

static void Prof_Usage(void)
{
    Com_Printf("Usage: profile list\n"
               "       profile load <player> <name>\n"
               "       profile save <player> [new name]\n"
               "       profile guest <player>\n");
}

static void Prof_f(void)
{
    char err[MAX_QPATH * 2];
    const char *cmd = Cmd_Argv(1);
    int i, n, user;

    if (!Q_stricmp(cmd, "list")) {
        CL_ProfileRefresh();
        Com_Printf("%d profile(s) in %s/" PROF_DIR "\n", profCount, fs_gamedir);
        for (i = 0; i < profCount; i++) {
            user = CL_ProfileListUser(i);
            Com_Printf("  %-16s %s\n", profList[i], user >= 0 ? va("(player %d)", user + 1) : "");
        }
        for (n = 0; n < MAX_SPLITVIEW; n++) {
            if (n == 0 || CL_SplitPlayerLive(n))
                Com_Printf("P%d: %s\n", n + 1, profSlots[n].named ? va("\"%s\"", profSlots[n].name) :
                           n ? "guest" : "own settings (config)");
        }
        for (i = 0; i < profNumPads; i++)
            Com_Printf("pad %s -> \"%s\"\n", profPads[i].device, profPads[i].name);
        return;
    }

    if (Cmd_Argc() < 3) {
        Prof_Usage();
        return;
    }
    if ((n = Prof_SlotArg(Cmd_Argv(2))) < 0)
        return;

    if (!Q_stricmp(cmd, "load") && Cmd_Argc() == 4) {
        if (!CL_ProfileLoad(n, Cmd_Argv(3), err, sizeof(err)))
            Com_Printf("profile: %s\n", err);
    } else if (!Q_stricmp(cmd, "save") && Cmd_Argc() <= 4) {
        if (!CL_ProfileSave(n, Cmd_Argc() == 4 ? Cmd_Argv(3) : NULL, err, sizeof(err)))
            Com_Printf("profile: %s\n", err);
    } else if (!Q_stricmp(cmd, "guest") && Cmd_Argc() == 3) {
        CL_ProfileLoad(n, NULL, err, sizeof(err));
    } else {
        Prof_Usage();
    }
}

static void Prof_c(genctx_t *ctx, int argnum)
{
    int i;

    if (argnum == 1) {
        Prompt_AddMatch(ctx, "list");
        Prompt_AddMatch(ctx, "load");
        Prompt_AddMatch(ctx, "save");
        Prompt_AddMatch(ctx, "guest");
    } else if (argnum == 3) {
        CL_ProfileRefresh();
        for (i = 0; i < profCount; i++)
            Prompt_AddMatch(ctx, profList[i]);
    }
}

static const cmdreg_t c_profile[] = {
    { "profile", Prof_f, Prof_c },
    { NULL }
};

/*
===============================================================================

INIT / SHUTDOWN

===============================================================================
*/

void CL_ProfileInit(void)
{
    char *text;
    int i;

    if (prof_init)
        return;

    Cmd_Register(c_profile);

    // the game stopped while player 1 played as a profile: its own settings back
    if ((text = Prof_ReadFile(PROF_P1KEEP)) != NULL) {
        Prof_Apply(0, text);
        FS_FreeFile(text);
        Prof_RemoveFile(PROF_P1KEEP);
        Com_Printf("profile: player 1's own settings restored from " PROF_P1KEEP "\n");
    }

    // players 2..8 start as guests (also drops p<N>_* / pbind values an
    // older config archived for them)
    for (i = 1; i < MAX_SPLITVIEW; i++)
        Prof_ApplyGuest(i);

    Prof_LoadPads();
    CL_ProfileRefresh();
    if (profCount || profNumPads) {
        Com_Printf("profile: %d profile(s), %d pad(s) remembered\n", profCount, profNumPads);
        for (i = 0; i < profNumPads; i++)
            Com_DPrintf("profile: pad %s -> \"%s\"\n", profPads[i].device, profPads[i].name);
    }
    prof_init = true;
}

/*
====================
CL_ProfileConfigBegin / CL_ProfileConfigEnd

Around every config write (q2config.cfg, writeconfig): while player 1 plays
as a profile, the config gets player 1's own settings (_p1keep), not the
profile's. The cvars' strings and the pad table are swapped in place for
the write only, so no userinfo goes out (no name change) and nothing else
sees it.
====================
*/
#define PROF_SWAP_MAX   32

static struct {
    cvar_t  *var;
    char    *string;
} profSwap[PROF_SWAP_MAX];
static int  profNumSwaps;
static bool profBindsStashed;

void CL_ProfileConfigBegin(void)
{
    static char line[PROF_TEXT_MAX], value[PROF_TEXT_MAX];
    char key[MAX_QPATH], arg[MAX_QPATH];
    const char *s, *p, *e;
    cvar_t *var;
    size_t len;

    if (!prof_init || !prof_p1keep || profNumSwaps || profBindsStashed)
        return;

    PAD_ProfileStashBinds(true);
    profBindsStashed = true;

    for (s = prof_p1keep; *s; ) {
        e = strchr(s, '\n');
        len = e ? (size_t)(e - s) : strlen(s);
        if (len < sizeof(line)) {
            memcpy(line, s, len);
            line[len] = 0;
            p = line;
            Q_strlcpy(key, COM_Parse(&p), sizeof(key));
            if (!Q_stricmp(key, "pbind")) {
                Q_strlcpy(arg, COM_Parse(&p), sizeof(arg));
                Q_strlcpy(value, COM_Parse(&p), sizeof(value));
                if (value[0])
                    PAD_ProfileSetBind(0, arg, value);
            } else if (key[0] && Prof_IsSetting(key) && profNumSwaps < PROF_SWAP_MAX) {
                Q_strlcpy(value, COM_Parse(&p), sizeof(value));
                var = Cvar_FindVar(key);
                if (var && strcmp(var->string, value)) {
                    profSwap[profNumSwaps].var = var;
                    profSwap[profNumSwaps].string = var->string;
                    profNumSwaps++;
                    var->string = Z_CopyString(value);
                }
            }
        }
        s += len;
        if (*s == '\n')
            s++;
    }
}

void CL_ProfileConfigEnd(void)
{
    int i;

    for (i = 0; i < profNumSwaps; i++) {
        Z_Free(profSwap[i].var->string);
        profSwap[i].var->string = profSwap[i].string;
    }
    profNumSwaps = 0;
    if (profBindsStashed) {
        PAD_ProfileStashBinds(false);
        profBindsStashed = false;
    }
}

void CL_ProfileShutdown(void)
{
    int n;

    if (!prof_init)
        return;
    for (n = 0; n < MAX_SPLITVIEW; n++) {
        if (profSlots[n].named || (n == 0 && prof_p1keep))
            Prof_Release(n);    // saved; player 1 gets its own settings back for the config
        if (profSlots[n].saved) {
            Z_Free(profSlots[n].saved);
            profSlots[n].saved = NULL;
        }
    }
    Cmd_Deregister(c_profile);
    prof_init = false;
}
