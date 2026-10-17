/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/
// gamepad.h -- pad layer (SDL2 joystick/gamecontroller only), splitscreen fork

#pragma once

#if USE_GAMEPAD
void PAD_PreInit(void);                 // CL_PreInit: bind commands, before config.cfg runs
void PAD_Init(void);                    // CL_Init: cvars, SDL devices, padlist / padinject ...
void PAD_Shutdown(void);
void PAD_Frame(void);                   // IN_Frame: poll devices, route pad keys
void PAD_WriteBindings(qhandle_t f);    // Key_WriteBindings: pbind lines
void CL_GamepadMove(vec3_t move);       // CL_BaseMove: active context's left stick
void CL_GamepadLook(int msec);          // CL_UpdateCmd: active context's right stick
bool PAD_AnyButtonDown(void);           // CL_FinalizeCmd: BUTTON_ANY for the active context
const char *PAD_HintText(const char **note, bool *warn);  // screen.c: join hint / refusal, or NULL
const char *PAD_CellStatus(int player); // screen.c: "controller disconnected" line, or NULL
void PAD_ReleasePlayer(int player);     // split_menu.c: the player's held pad binds let go
// profiles (split_profile.c)
void PAD_ProfileWriteBinds(int player, char *buf, size_t size); // append "pbind <key> \"cmd\"" lines
void PAD_ProfileClearBinds(int player);                         // held binds let go, table emptied
bool PAD_ProfileSetBind(int player, const char *key, const char *cmd);  // false: no such pad key
void PAD_ProfileDefaultBinds(int player);                       // the default set
const char *PAD_PlayerDevice(int player);                       // device key of the player's pad, or NULL
void PAD_ProfileStashBinds(bool stash);                         // config write: P1's table set aside / back
// the overlay's Buttons page (split_menu.c)
const char *PAD_GetBind(int player, const char *key);           // the key's command, "" if unbound
void PAD_MenuSetBind(int player, const char *key, const char *cmd); // held key let go first; "" unbinds
void PAD_MenuEnsureBinds(int player);                           // an empty table gets the default set
int  PAD_ConnectedCount(void);                                  // pads plugged in (real and virtual)
#else
#define PAD_ConnectedCount()                        0
#define PAD_GetBind(player, key)                    ((void)(player), (void)(key), "")
#define PAD_MenuSetBind(player, key, cmd)           (void)0
#define PAD_MenuEnsureBinds(player)                 (void)(player)
#define PAD_ProfileStashBinds(stash)                (void)(stash)
#define PAD_ProfileWriteBinds(player, buf, size)    (void)0
#define PAD_ProfileClearBinds(player)               (void)0
#define PAD_ProfileSetBind(player, key, cmd)        false
#define PAD_ProfileDefaultBinds(player)             (void)0
#define PAD_PlayerDevice(player)                    ((void)(player), (const char *)NULL)
#define PAD_AnyButtonDown()     false
#define PAD_HintText(note, warn)    ((void)(note), (void)(warn), (const char *)NULL)
#define PAD_CellStatus(player)  ((void)(player), (const char *)NULL)
#define PAD_ReleasePlayer(player)   (void)(player)
#define PAD_PreInit()           (void)0
#define PAD_Init()              (void)0
#define PAD_Shutdown()          (void)0
#define PAD_Frame()             (void)0
#define PAD_WriteBindings(f)    (void)0
#define CL_GamepadMove(move)    (void)0
#define CL_GamepadLook(msec)    (void)0
#endif
