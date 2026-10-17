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

// split.h -- splitscreen client contexts (included at the end of client.h;
// the context types and the cl / cl_entities / ... macros are there)

#pragma once

void CL_InitContexts(void);

// Make context i the active one: saves the cls connection slice of the
// current context into it and loads context i's slice into cls.
void CL_SetContext(int i);

// Nestable (depth 2) switch: CL_PushContext(i) ... CL_PopContext()
// restores the context that was active before the push.
void CL_PushContext(int i);
void CL_PopContext(void);

// Run fn once per context in use, with that context active.
void CL_ForEachContext(void (*fn)(void));

// Splitscreen players 2..8 (split.c). Hooks called from upstream files;
// each acts only when the active context is an extra (cla->playerNum > 0)
// unless noted.

// CL_ProcessEvents / CL_Frame, player 1 active: run every extra's packets and
// stufftext, then its per-context frame, each under the error guard.
void CL_SplitProcessEvents(void);
void CL_SplitFrame(int msec, bool phys_frame);
bool CL_SplitSendPacketNow(void);       // cl.sendPacketNow of any context

// connect path
bool CL_SplitMayResend(void);           // serialized joins, give-up limit
// per-player userinfo / cvar sync (shadows p<N>_name, ...)
void CL_SplitInitUserinfo(void);        // CL_InitLocal, after the userinfo cvars
size_t CL_SplitUserinfo(char *userinfo, size_t len);   // overlay, new length
bool CL_SplitUserinfoChanged(cvar_t *var);  // any context: true if handled
void CL_SplitCvarSyncParsed(void);      // an extra got the cvar sync list
void CL_SplitRefused(const char *text); // OOB print while connecting
void CL_SplitPrecache(void);            // `precache': copy player 1's level
void CL_SplitActive(void);              // first frame received
bool CL_SplitStufftext(const char *text);   // exec_server_string: false = handled / ignored
bool CL_SplitServerDisconnect(void);    // svc_disconnect: true = deferred (stufftext to run)

// join / leave for the pad layer (player 1 active)
int  CL_SplitMaxPlayers(void);          // cl_splitMaxPlayers, 1..MAX_SPLITVIEW
int  CL_SplitAddPlayer(char *why, size_t size);    // slot, or -1 + short reason
void CL_SplitRequestDrop(int n, const char *reason);
bool CL_SplitPlayerLive(int n);         // in use and not leaving

// lifecycle: true if handled (an extra); player 1 drops every extra first
bool CL_SplitDisconnect(error_type_t type);
void CL_SplitClearState(void);

// server prints for an extra: its own chat HUD / centerprint, no console
void CL_SplitPrint(int level, const char *s);

// viewports (split_view.c): cells in window pixels, one per context in use
void CL_SplitInitView(void);
int  CL_SplitViewCount(void);           // cells drawn: contexts in use + notices
void CL_SplitSetNotice(int n, const char *text);   // a left player's reason in its cell (NULL = off)
void CL_SplitClearNotices(void);
const char *CL_SplitCellNotice(int i, vrect_t *rc); // slot i's notice and cell, NULL = none
void CL_SplitLayout(bool force);        // recompute cells if anything changed
void CL_SplitDrawEmptyCells(void);      // black for unused cells (N = 3)
bool CL_SplitViewReady(void);           // active context's view may be drawn
bool CL_SplitPushView(int i);           // CL_PushContext unless the stack is full
int  CL_SplitServerCount(const client_context_t *ctx);

// cl_splitperf 1: where the frame time goes, printed every 2 s (split_view.c).
// A mark ends the current phase and starts the given one.
typedef enum {
    SPLIT_PERF_NONE,        // everything else (game, network, sleep, console, menus)
    SPLIT_PERF_SCENE,       // V_RenderView up to R_RenderFrame (CL_AddEntities, ...)
    SPLIT_PERF_RENDER,      // R_RenderFrame
    SPLIT_PERF_2D,          // the cell's HUD after R_RenderFrame
    SPLIT_PERF_SWAP,        // R_EndFrame
    SPLIT_PERF_MAX
} splitPerf_t;
void CL_SplitPerfMark(splitPerf_t phase);
void CL_SplitPerfFrame(void);           // end of SCR_UpdateScreen

// after the active context re-registered models / images / sounds
// (CL_PrepRefresh, CL_RegisterSounds): re-copy into the other contexts
void CL_SplitRegistrationChanged(void);

// screen.c: undo an unfinished SCR_UpdateScreen after a trapped error
void SCR_ResetAfterError(void);

// sound (split_snd.c; the mixer hooks are declared in sound/sound.h)
void S_SplitInit(void);
void S_SplitSetListener(int player, const vec3_t origin, const vec3_t right);  // CL_CalcViewValues
int  S_SplitImpactRand(const vec3_t pos);   // tent.c: impact sound roll, the same in every context
// svc_sound (parse.c) / `play': false = a humor sound with cl_splitHumorSounds 0
bool CL_SplitSoundAllowed(const char *name, int entnum);

// main.c side: the per-context part of CL_ProcessEvents / CL_Frame
void CL_ContextEvents(void);
void CL_ContextFrame(int msec, bool phys_frame);
void CL_UpdateGunSetting(void);         // CLS_NOGUN on the active netchan

// overlay menu, on-screen keyboard, host page, engine UI in P1's cell
#include "split_menu.h"

// profiles and guests (split_profile.c)
#include "split_profile.h"
