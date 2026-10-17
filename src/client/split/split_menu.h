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

// split_menu.h -- per-player overlay menu, on-screen keyboard, the host's
// splitscreen page (a page of player 1's overlay) and the engine UI inside player 1's cell (design 7.1)

#pragma once

// overlay keys (the pad layer maps pad buttons to these)
typedef enum {
    SMK_UP, SMK_DOWN, SMK_LEFT, SMK_RIGHT,
    SMK_A, SMK_B, SMK_X, SMK_Y, SMK_START, SMK_L3
} splitMenuKey_t;

void CL_SplitMenuInit(void);            // CL_InitContexts: commands, cvars
bool CL_SplitMenuOpen(int n);           // player n's overlay or keyboard is up
bool CL_SplitMenuPadStart(void);        // P1's Start out of a game: the overlay full screen
void CL_SplitMenuKey(int n, int key);   // a splitMenuKey_t from player n's pad
uintptr_t CL_SplitMenuView(int n);      // the page / keyboard player n's overlay shows, 0 = closed
bool CL_SplitMenuUIEnter(void);         // P1's pad A in the engine UI: keyboard for a text field
bool CL_SplitMenuKeyEvent(int key, bool down);  // keys.c: keyboard / mouse drive P1's overlay (true = taken)
bool CL_SplitMenuCanOpenHost(void);     // the pad layer: P1 out of a game, the Splitscreen page may open
void CL_SplitMenuDraw(void);            // SCR_UpdateScreen, after the engine UI

// the engine UI inside its owner's cell (ui.c; player 1 unless another
// player's context opened it)
bool  CL_SplitUIRect(vrect_t *rc);      // in game with N > 1: the owner's cell
float CL_SplitUIScale(float window_scale, const vrect_t *rc);
void  CL_SplitUIOpened(void);           // UI_PushMenu onto an empty stack: the active player owns it
int   CL_SplitUIOwner(void);            // 0-based player number
float CL_SplitUIMenuSize(void);         // the owner's AQtion menu size (cl_splitMenuSize / p<N>_)
void  CL_SplitMenuFrame(void);          // CL_SplitFrame: Server options' infinite ammo

// cell borders (split_view.c), after the cells
void CL_SplitDrawBorders(void);
