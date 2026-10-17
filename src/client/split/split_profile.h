/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

// split_profile.h -- player profiles and guests (design 5.2, Q3 12.3, 13.1, 14.3)

#pragma once

#define PROFILE_NAME_MAX    15      // = a Quake 2 player name

void CL_ProfileInit(void);              // CL_Init, after PAD_Init
void CL_ProfileShutdown(void);          // CL_Shutdown, before the config is written
void CL_ProfileConfigBegin(void);       // a config write: player 1's own settings stand in for a profile's
void CL_ProfileConfigEnd(void);         // ... and the profile's are back
void CL_ProfileFrame(void);             // CL_SplitFrame: debounced saves
void CL_ProfileLeave(int n);            // player n (2..8) left: save, back to the guest defaults

// the pad layer
void CL_ProfilePadJoined(int n, const char *device);    // an extra joined / P1 took this pad: its profile
void CL_ProfilePadAssigned(int n, const char *device);  // a pad was given to player n
void CL_ProfilePadReleased(int n);                      // player n's pad stepped out (P1: its pad's profile goes)

// the overlay menu and the console
void CL_ProfileRefresh(void);           // re-read the list of profile files
int  CL_ProfileCount(void);
const char *CL_ProfileListName(int i);
int  CL_ProfileListUser(int i);         // the player slot using it, -1 = none
bool CL_ProfileNamed(int n);            // player n plays as a named profile
const char *CL_ProfileName(int n);      // its name, "" if none
bool CL_ProfileLoad(int n, const char *name, char *err, size_t size);   // NULL = guest / own settings
bool CL_ProfileSave(int n, const char *name, char *err, size_t size);   // NULL = the current one
bool CL_ProfileSanitize(const char *in, char *out, size_t size);
