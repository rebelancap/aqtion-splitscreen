# Changelog

## 1.0.0.1 (dev)

- Linux: the binary no longer needs libjpeg.so.8 (libjpeg-turbo is linked
  in); runs on Fedora / Bazzite outside Steam.

## 1.0.0

First public release of aqtion-splitscreen: local splitscreen for AQtion
(Action Quake 2 on Steam), Windows x64 and Linux x86_64 / Steam Deck.

- **Splitscreen for up to 8 players** on one PC and one screen. The screen
  splits by player count; every player has their own view, HUD, scoreboard,
  team / weapon menus and chat.
- **Gamepads:** pads are assigned automatically; friends hold **A** to join
  and hold **Back+Start** to leave. Player 1 can use keyboard and mouse or a
  pad. Pad layout follows the Urban Terror splitscreen build, with Action's
  own actions (reload, bandage, zoom cycle, weapon mode, drop, radio).
- **Profiles** per player: name, gun hand, aim feel (curves, turn boost,
  dead zones), controls and buttons, crosshair type / size / colour, HUD
  opacity, AQtion menu size, splitscreen menu size and text size. Saved and
  reused; guests play as "Player N" until they save one.
- **Start menu on every pad.** Player 1 also gets Server options: Quick
  Local Match, game type (deathmatch, teamplay, CTF, team DM, domination,
  3 teams, tourney), map, limits, bots 0-20, items and weapon bans with one
  "<weapon> only" preset per weapon, darkmatch, infinite reserve ammo,
  Humor sounds off, respawn / spawn protection, saved option sets, next map.
- **Unmodified game and servers:** the program uses Steam's own Action game
  library and every local player is an ordinary client, so your own games
  and public AQ2 / AQtion servers see normal players.
- **Zero install:** one file next to Steam's AQtion; settings live in a
  `splitscreen` folder beside it; Steam's files are never changed.
- **Windows x64 and Linux x86_64 / Steam Deck** archives (Linux runs under
  the desktop, as a non-Steam game, and inside Gamescope).

The dev builds that led up to this release follow below.

## 0.0.0.34 (dev, Linux round L3: Linux release candidate)

- **Linux release archive:** `aqtion-splitscreen-0.0.0.34-linux-x86_64.tar.gz` with `q2pro-splitscreen` and `README.txt`. Put `q2pro-splitscreen` next to `q2pro` in your AQtion folder (`chmod +x` if needed) and run it, or add it to Steam as a non-Steam game (Steam Deck: Desktop mode, Add a Non-Steam Game; it then shows in Game mode).
- **Windows archive renamed:** `aqtion-splitscreen-0.0.0.34-windows-x86_64.zip` (was `aqtion-splitscreen-<version>.zip`); same two files, `q2pro-splitscreen.exe` and `README.txt`. Windows players also get the 0.0.0.33 fixes with this build (sniper zoom no longer refills the gun with Infinite ammo, Gun hand: Hidden gets spare top-ups, Quick Local Match never picks `credits`).
- **README.txt covers Windows and Linux / Steam Deck** (one file in both archives).
- **Fix (Linux): runs inside Gamescope.** Started under Gamescope (e.g. `gamescope -- %command%` in Steam's launch options, or a nested Gamescope window) on a Wayland desktop, the game opened its window on the desktop instead of inside Gamescope; it now uses Gamescope's own display. Normal desktop runs still use Wayland.
- Developers: `scripts/package.sh` (clean release build + tarball), `scripts/run.sh --gamescope` (nested Gamescope window at Steam Deck size, 1280x800) and `--size`; `run.sh --self-locate` no longer passes a homedir; `BUILD-INFO.txt` names the build configuration.

## 0.0.0.33 (dev, Linux round L2)

- **Splitscreen and gamepads verified on Linux** against Steam's own Action game library: joining with a pad (hold A) up to 4 players and 8 with `addplayer`, every screen layout, leaving (Back+Start), the Start menu with hold-to-scroll, profiles, Server options -> Quick Local Match, Infinite ammo, the weapon presets, sniper zoom, and sound (PipeWire through SDL, or OpenAL). Pads go through the game's own SDL2.
- **Fix (all platforms): Infinite ammo and the sniper scope.** Zooming in and out with the sniper refilled the gun (the game hides the gun model while zoomed, which looked like a respawn); now the rounds in the gun run down whether you zoom or not. Players with Gun hand: Hidden also get their spare ammo topped up now (before, they got none).
- **Fix (all platforms): Quick Local Match no longer picks the `credits` map** (AQtion's credits scene, where every player spawns on the same spot).
- `scripts/run.sh --pads` now really turns pads on (developers).

## 0.0.0.32 (dev, Linux)

- **Linux build:** the client now builds for Linux x86_64 (`scripts/build.sh`, output `build/linux/q2pro-splitscreen`) and runs with the system's own libraries (SDL2, OpenGL, Wayland or X11). Gamepads go through the same SDL2 the game already uses.
- **Finds its place in the AQtion folder on Linux too:** put next to `q2pro` in your AQtion folder, it uses Steam's own game files and game library, keeps its settings in `AQtion/splitscreen/`, and starts in the AQtion folder, like the Windows exe. In a subfolder it uses the AQtion folder above it and keeps its settings in `home/` beside itself.
- `scripts/run.sh` starts a windowed test game against the Steam install (developers).
- Not yet tested on Linux: splitscreen with pads, the menus, Steam Deck / Gamescope.

## 0.0.0.31 (dev)

- **Sniper zoom wraps:** LT (zoom) now steps 1x -> 2x -> 4x -> 6x and the next press goes back to 1x, so you no longer need the reset-zoom button (LB still resets). It is Action's own plain `lens` command; your saved profiles get it too (their LT `lens in` becomes `lens` once). The Buttons page has the new "Zoom (cycle)"; "Zoom in" is still there and still stops at 6x.
- **Hold to scroll in the Start menu:** holding the d-pad or the left stick up / down (or left / right on a slider or choice, and on the on-screen keyboard) keeps moving after 0.4 s, then about 8 rows a second, faster after 1.5 s. A normal press still moves exactly one row.

## 0.0.0.30 (dev)

- **Server options -> Weapons: one "only" preset per weapon, and you spawn holding it:** All, MK23 only, Dual pistols only, MP5 only, M4 only, M3 only, Handcannon only, Sniper only, Knife only (any other mix from Weapon bans shows "Custom"). Every player (bots too in deathmatch) spawns with that weapon loaded plus its usual spare magazines / shells, in deathmatch and teamplay, from the first spawn; the teamplay weapon menu lists only that weapon. Changes apply at the next spawn / round.
- Fixes 0.0.0.29's "Sniper only", which gave a pistol with no ammo: it banned the MK23 (and with it the pistol's rounds and magazines), and Action still spawns everyone with only the pistol unless told otherwise. The gun presets now keep the pistol as a working sidearm (the knife is banned with the other weapons).

## 0.0.0.29 (dev)

- **Infinite ammo keeps reloads:** the rounds in your gun now run down as you fire and the gun stops when it is empty, so you reload as usual; only your spare magazines / shells never run out (they are topped back up right after each reload). Thrown knives and grenades still come back one for one. Help line: "Now: endless spares, reloads; cheats on".

## 0.0.0.28 (dev)

- **Infinite ammo works at once, however the game was started:** switching it On in a running game now refills your players straight away (before, `cheats` only changed at the next map, so nothing happened until Restart map), and a game started from AQtion's own menus or the console gets it too (before, only games started from the Splitscreen page did). Off also takes effect at once.
- Guns are refilled right after every shot and reload (not twice a second), so a magazine never runs down, even with the M4 / MP5 on full auto; thrown knives and grenades come back one for one. Players who are spectating (teamplay before joining a team) are left alone.

## 0.0.0.27 (dev)

- **Server options -> Humor sounds** (On / Off, default On): Off silences AQtion's joke clips (farts, laughs, "Do I smell a newbie?", film quotes) for all your local players, at once. They came from Steam's own Action game: a player who stands still for 15 s plays a random one (and again every 15 s), so idle split-screen players set them off all the time. Off also turns that idle sound off in games you host (`ppl_idletime 0`) and blocks the same clips from the `voice` command and from other servers. Kept in saved option sets. It replaces the "Joke sounds" row that only showed with our development game DLL.
- Correction: 0.0.0.23 said Steam's game DLL plays none of these sounds; it does (the idle sound above).

## 0.0.0.26 (dev)

- **Infinite ammo no longer turns off a `cheats 1` you set yourself:** with the row off, starting a game, Restart / Next map, Reset to defaults and loading a saved set leave `cheats` alone; the row sets `cheats 1` while it is on and puts it back to 0 only when you switch it off.

## 0.0.0.25 (dev)

- **Each player's Settings** now has Crosshair, Crosshair size (default 3x, up to 4x), Crosshair colour, **Text size** (chat, kill and centre messages; default 2x, larger than before) and both menu sizes, each for that player's own view (and saved in its profile). The P1-only crosshair rows are gone.
- **AQtion's menus keep their size when the screen splits** (they only shrink as far as needed to fit the view; the old code read a wrong value and drew them at a random, mostly 1x, size).
- The profile row always says **"Profile: Guest"** or "Profile: <name>".
- **Infinite ammo now works:** Steam's Action DLL has no real infinite-ammo setting (the old row only made pickups give 1000 rounds), so the row turns cheats on for your game (next map / Restart map) and keeps your local players' magazines and clips full. Bots get nothing extra.

## 0.0.0.24 (dev)

- **Fixes from the first windowed test of the 0.0.0.23 menus:** "Bots: None" on the Splitscreen page / Quick Local Match no longer spawns the bot list AQtion keeps for some maps (e.g. 12 bots on terminal); "Bot skill" no longer shows an empty value on the main-menu Server options page; a key held when the Splitscreen menu opens no longer gets stuck down; Start on a second pad while the Splitscreen page is already open (or while a map loads) no longer takes player 1 and resets the page; Shift + digit / symbol types the shifted character on the on-screen keyboard.
- With `pad_enable 0` the game no longer starts SDL's controller support at all (no pad is opened or enumerated); it starts when `pad_enable` is set to 1.
- Developer only: `vid_noactivate 1` runs the game in a window that never takes focus, the mouse or the screen mode (`scripts\run.ps1 -Offscreen`), so tests can run while someone plays.

## 0.0.0.23 (dev)

- **Menus, the maintainer's list:** AQtion's own menus are **twice as big** by default (as big as still fits; player 1: Start -> Settings -> Menu size, 1x = AQtion's). Each player's Start menu is now **Team** (join team 1 / 2 / 3, spectate), **Weapons** (Action's weapon / item menu, hidden in a game you host that has no weapon choice, e.g. deathmatch without "Weapon choice"), Scoreboard, Radio, **Settings** (name, gun hand, **Controls** with look speeds, invert, aim curve, turn boost, dead zones, trigger point, and **Buttons** to remap every pad button, with reset), **Profile...** ("Profile: <name>" once saved; a player without one is a guest, player 1 too), and for player 1 **Server options**, Splitscreen settings, Game menu, End game.
- **Server options** (player 1, games you host; also on the main-menu Splitscreen page for the next game): game type (deathmatch, team DM, teamplay, CTF, 3 teams, tourney, domination), restart / change map, time / frag / round / round time / capture limits, bots and bot skill, bot chat, friendly fire, falling damage, weapon choice in deathmatch, spawn protection, all weapons / items, weapons / items carried, weapon and item bans, team grenades, spectating limits, darkmatch, item / weapon respawn, announcements, time / frag warnings, reset to defaults. Each row says whether it applies now or at the next map. Only options Steam's Action DLL has are shown; the choices are kept for the next session.
- **Start on any pad** (or **F1**) at the main menu opens the Splitscreen page (it used to work only on the pad that happened to be player 1, so with several pads it often did nothing); the main menu says so while a pad is connected. Its first row is **Quick Local Match**: one press starts a local game with your last server options on a random map; pads join as usual. Keyboard and mouse work in the Splitscreen menus too.
- More options: Settings has AQtion menu size, Splitscreen menu size, and per player HUD opacity and crosshair colour. Server options also has Next map, saved option sets (save / load / delete), a Weapons preset (all / knife only / sniper only), infinite ammo, respawn delay, spawn protection and up to 20 bots.
- The farting / laughing / "newbie" sounds come from our own development game DLL (bots, 5 % per frag); the release hosts with Steam's DLL, which has none of them. With our DLL, Server options shows a "Joke sounds" switch.

## 0.0.0.22 (dev, release candidate)

- **The game now runs in the AQtion folder itself** (as when Steam starts AQtion), wherever the exe sits. The Action game reads its own files from there: bots get their names and their map routes (`action\terrain\*.ltk`) and maps get their location names, as under Steam. Settings, profiles, screenshots and logs still go only to `splitscreen\` (or `home\` beside the exe). The game itself rewrites `<AQtion>\items.txt` (and possibly bot files) on map loads, exactly as Steam's own AQtion does. Known AQtion bug: a bot game on the map `beer` never finishes loading (Steam's own AQtion server hangs the same way); pick another map.
- Menus: an empty setting behind a list choice in AQtion's menus stays empty (only the bots page's map row picks the first map).
- Pads: the press that claims a pad or joins no longer also acts (no jump / Enter on joining with `cl_splitJoinHold 0`, or when a pad takes over player 1); Start on a new pad at the main menu still opens the splitscreen page at once.

## 0.0.0.21 (dev, release candidate)

- **Install = put `q2pro-splitscreen.exe` in your AQtion folder and start it.** It then keeps everything it writes (config, profiles, screenshots, demos, logs, console history) in a new `<AQtion>\splitscreen\` folder and never writes into Steam's `action\` or `baseaq\` (it also works in that folder, so even Steam's game DLL's own scratch files land there). A folder of its own inside AQtion still works as before (settings in `home\` beside the exe).
- Upgrading from 0.0.0.19 / 0.0.0.20: the old `<AQtion>\splitscreen\q2pro-splitscreen.exe`, `README.txt` and any `gamex86_64.dll` there can be deleted; to keep old profiles, move the contents of `splitscreen\home\` up into `splitscreen\`. A `q2config.cfg` an older copy left in `<AQtion>\action\` is no longer read by this build (delete it if you like).

## 0.0.0.20 (dev, release candidate)

- **The zip is now just two files:** `q2pro-splitscreen.exe` and a short `README.txt`. Unpack into a folder inside AQtion (e.g. `<AQtion>\splitscreen\`) and start the exe. Upgrading from 0.0.0.19: delete the old folder's files first (keep `home\`), or the old `gamex86_64.dll` beside the exe is still used.
- **Hosting uses Steam's own Action game DLL** (it used to crash this newer engine; the engine now speaks its older interface). Local games, bots, 4 players, team joins and map changes tested with it.
- **First start is fullscreen** at your desktop resolution (it was a small window: a stale `q2config.cfg` in Steam's `action\` folder was being read; only the one in `home\` counts now).
- **No "Choose your name" prompt**: player 1 is "Player 1" until you set a name (Start -> Settings, or a profile). The first button on player 1's pad now counts (Start at the main menu opens the splitscreen page straight away).
- AQtion's own "Play Offline (Bots)" page no longer starts with no map (it tried `maps/force.bsp`); the host page only offers installed maps; the two `Usage: set ...` lines (and AQtion's kill-counter warnings) are gone.

## 0.0.0.19 (dev, release candidate)

- **No launcher any more, installer optional:** unpack the zip into a folder inside your AQtion folder (e.g. `<AQtion>\splitscreen\`) and start `q2pro-splitscreen.exe` (or a shortcut to it). Sitting there, the exe finds the AQtion game data one folder up, its `home\` settings folder and its own game DLL by itself; `launch-splitscreen.bat` is gone. `+set basedir / homedir / sys_forcegamelib / game` on the command line (or in a shortcut) still win.

## 0.0.0.18 (dev, release candidate)

- **Settings stay in the splitscreen folder:** the launcher now writes the config, profiles, screenshots, demos and console history to `<AQtion>\splitscreen\home\` instead of Steam's `<AQtion>\action\` / `baseaq\`.
- `install.ps1`: an install that stopped half way (copy error, game still running) can simply be run again; it refuses up front while `q2pro-splitscreen.exe` runs. `-Uninstall` keeps `home\` (your settings) and says so; the new `-Purge` deletes the whole folder, settings included. Uninstalling from a window whose current folder is inside `splitscreen\` no longer ends in an error.

## 0.0.0.17 (dev, release candidate)

- **Release package:** `scripts\package.ps1` builds `aqtion-splitscreen-<version>.zip` (our client as `q2pro-splitscreen.exe`, our game DLL, `launch-splitscreen.bat`, `install.ps1`, README). `install.ps1` puts it into a new `<AQtion>\splitscreen\` folder and replaces nothing; `install.ps1 -Uninstall` removes exactly what it installed. The game DLL now carries the version too.
- With 2+ players, player 1's console messages (top-left lines) now stay inside player 1's view instead of running across the others.
- Chat lines no longer overlap the scoreboard or Action's menus in short views (e.g. 8 players with Equal cells): they move just above the status bar and show as many lines as fit.
- Splitscreen settings: the row "3, 5, 7 players" is now "Cell layout" (it also changes 8 players: 4x2 or 3x3).
- A "left the game" / "can't join" notice no longer takes a view another player needs; the post-process buffer (bloom, underwater warp) no longer flips size when only some players are under water. Message fix: "Server connection was reset. 3 local players rejoin when ...".

## 0.0.0.16 (dev)

- **Up to 8 players** (the default maximum is now 8). 5-6 players: three on top, two or three below; 7-8 players: four on top, three or four below. Splitscreen settings have two new rows: "3, 5, 7 players" (Fill the screen = the bottom row gets wider views, default; Equal cells = same-size views with black gaps, 3x3 for 7-8 players) and "Wide cell for" (who gets the first wider view: the player who joined last, or a chosen player).
- In small views (narrower than 640 pixels, e.g. 8 players at 1280x720) the HUD, scoreboards, Action's menus and chat are drawn smaller so they fit inside the view instead of being cut off. At 320 pixels wide that text is very small; a bigger window helps. `scr_scale` set by hand still applies to every view.
- `gl_bloom 1` with splitscreen: each view now gets its own bloom (before, all views showed the bloom of one view), and views of different sizes no longer rebuild the bloom buffers every frame.

## 0.0.0.15 (dev)

- **Online:** extra players now follow player 1 onto another machine's server (join, map changes, server restarts, servers with `sv_force_reconnect`), each on its own connection. A server that refuses one shows why in that player's view for 10 s, e.g. "can't join: this server allows only 3 players from one connection"; a kicked player's view shows the kick the same way.
- If the server goes away (quits, crashes) and player 1 `reconnect`s to it, the other players come back with their profiles.
- A server can no longer change player 1's settings through another player's connection: commands it sends to players 2-8 that would touch shared settings (binds, aliases, `quit`, most cvars) are ignored with a console note.

## 0.0.0.14 (dev)

- Saving the config (`writeconfig`, a game-folder change) while player 1 plays as a profile now saves player 1's own settings, not the profile's (as quitting already did).
- A hold bind on Back or Start (e.g. `pbind 2 PAD_BACK +forward`) is held for as long as the button again; the menu (Start) and the scoreboard (Back) still open on release, and Back+Start still leaves without either.
- The full-screen Splitscreen settings at the main menu no longer shrink into an old cell; a map whose name starts with a digit no longer shows as another map in the Map row.

## 0.0.0.13 (dev)

- **Splitscreen settings moved out of AQtion's menu** (no more "Splitscreen" entry there): at the main menu press **Start on player 1's pad** to open them full screen (with player 1's profile and "Start multiplayer game"); in a game they are in player 1's Start menu, "Splitscreen settings". The Map row lists the installed maps; the Game row no longer shows `???` before the first game.
- Player 1's pad now brings its saved profile too (the same pad as last time = your name and settings); player 1's own settings come back when that pad is let go and at quit.
- Holding **Back+Start** to leave no longer flashes your menu or the scoreboard (Back and Start now act when released). Opening your menu closes an open scoreboard / Action menu first.
- Sounds with 3+ players: fewer doubles (a weapon's "click" heard from far away, bullet ricochets).

## 0.0.0.12 (dev)

- **Profiles:** a player's name, gun hand, look settings and pad buttons can be saved as a named profile (pad menu -> Profile -> "Save as new profile", name typed on the on-screen keyboard). Changes are saved automatically. A pad remembers its profile: join with it again (or after a replug / restart) and you are back as yourself. Without a profile you play as a guest ("Player N", default settings) and your changes last until you leave. Console: `profile list | load <player> <name> | save <player> [name] | guest <player>`.
- **Sound with several players:** a sound is now heard from the nearest player's view instead of always player 1's, ambient loops near any player play, and with 3 or more players sounds are centred (no left/right; `s_splitPan` changes that). DirectSound only; OpenAL (`s_enable 2`) keeps player 1's view.

## 0.0.0.11 (dev)

- **Start** on a pad now opens that player's own menu in their part of the screen: Team & weapons (Action's team / weapon / item menu), Scoreboard, Radio, Settings (name, invert look, look speed, gun hand), Leave game. Player 1's also has Game menu and Splitscreen settings. D-pad + A pick, B goes back, Start closes; the game keeps running.
- Names can be typed with a pad on an on-screen keyboard (A types, X deletes, Y space, L3 shift, Start done, B cancel), also in the game's own "Choose your name" box.
- With 2+ players the Escape menu (and every engine menu) opens in player 1's part of the screen; the others keep playing. Player 1's pad drives it (d-pad, A, B).
- New "Splitscreen" page (main menu and Escape menu): player 1 input, 2-player layout, max players, join button hold, join hint, cell borders, map, and "start multiplayer game". Changes apply at once.
- A thin black line between the players' views (`cl_splitBorder 0` turns it off).

## 0.0.0.10 (dev)

- Join with a pad: in a multiplayer game, **hold A** (0.7 s) on a pad nobody uses and a new player joins in a new part of the screen; **hold Back+Start** (2 s) to leave (player 1's pad is just let go). "Player 2: hold A to join" shows bottom right for a pad that was just used; a refused join says why there ("single-player game", "4 players is the most"). At most 4 players for now (`cl_splitMaxPlayers`).
- By default the first pad to press any button is player 1's (keyboard and mouse still work); `cl_splitP1Input kbm` makes the keyboard player 1 and the first pad joins as player 2.
- Unplugging a pad: that player waits 30 s (`cl_splitPadGrace`) with "controller disconnected" in their screen part, then leaves; plugging the same pad back in (or pressing a button on a free pad) takes over again.
- Default pad buttons, laid out like the Urban Terror splitscreen build: A jump, B crouch, X reload, Y bandage, RT fire, LT sniper zoom, LB zoom back to 1x, RB next weapon, Back scores, Start team/weapon menu, sticks move/look, L3 walk, R3 knife, d-pad drop item / drop weapon / IR vision / weapon mode, Share opendoor, back paddles radio. While a game menu or the scoreboard is up, d-pad up/down + A pick and B closes it. `padbinddefaults <player>` restores them; they are only saved if you change them.
- `padinject <N>` now drives virtual pad N (a pad nobody has until it joins), for tests.

## 0.0.0.9 (dev)

- Gamepads (first part): every connected pad is opened and listed by `padlist` (with the player it belongs to); `padassign <pad#> <player>` gives a pad to a player (`padassign <pad#> 0` frees it). A pad that is unplugged and plugged back goes to the same player.
- Each player has their own pad buttons: `pbind 2 PAD_A +attack`, `punbind`, `punbindall`, `pbindlist [player]`; they are saved in your config. There are no default pad buttons yet (next build), so bind what you want to test.
- Left stick moves (push further to go faster; `cl_run` / +speed as on the keyboard), right stick turns with a response curve and a turn boost when held fully over; the turn slows down with the sniper zoom. Settings: `joy_yawSpeed`, `joy_pitchSpeed`, `joy_invertPitch`, `joy_aimCurve`, `joy_deadzone`, `joy_turnBoost`, `joy_zoomScale`; `pad_enable 0` turns real pads off. Player 1's pad is ignored while the console or a menu is open.

## 0.0.0.8 (dev)

- Fix: after the server kicks a local player (`kick 1`), `addplayer` works again (it used to hang on "loading..." forever; the kicked slot now frees itself after the usual 2 s, and a re-add before that joins in a new slot).
- Fix: `p2_hand 2` (no gun) now also tells the server for player 2 straight away, and `hand` only ever affects player 1.
- Fix: the sound de-duplication can no longer drop a sound in the first moments after launch.

## 0.0.0.7 (dev)

- Each extra player has their own name and settings: `p2_name`, `p2_skin`, `p2_hand`, `p2_fov`, `p2_gender`, `p2_spectator`, `p2_msg`, `p2_rate` (and p3..p8), saved in your config. Changing one updates only that player on the server (`p2_name Bob` renames only player 2, `name` only player 1); `p2 name Bob` does the same. `p2_hand 1` puts player 2's gun on the left in their part of the screen only. Extras never send the Steam/Discord ids. Note: Action keeps everyone's field of view at 90 (zoom only), so `fov`/`p2_fov` change nothing in Action, for player 1 as before.
- AQtion's synced settings (`cl_xerp`, `cl_antilag`, ...) can be set per player with `set p2_cl_xerp 0` or `p2 cl_xerp 0`; without one, player 2 uses the shared value.
- Sound with several players: each shot, footstep or chat beep is played once instead of once per player; positions are heard from player 1's view; every local player's own gun and footsteps are full volume. Debug counter `s_splitdebug 1`. One-player sound is unchanged.
- Fix: an error while drawing an extra player's view no longer leaves the screen state broken (a second such error could quit the game).

## 0.0.0.6 (dev)

- Splitscreen is visible: every local player gets their own part of the window with their own view, status bar, crosshair, scoreboard, menus, centerprints, chat lines and damage tint. 2 players top/bottom (`cl_splitVertical 1` for side by side), 3-4 players in quarters (with 3, the fourth quarter is black). The layout follows joins, leaves and window size changes live.
- HUD size is picked per screen part, so small parts get the smaller HUD (e.g. quarters of a 1080p window). Console and engine menus stay full window.
- `vid_restart` with extra players attached keeps their models and skins (the same re-copy runs after a sound restart, not yet tested). The Spike C test switch `cl_splitspike` is gone. One-player play is unchanged.

## 0.0.0.5 (dev)

- First real second player: in a multiplayer game (`deathmatch 1; map <name>`), `addplayer` adds local players 2..8 as separate clients of the same server (own connection, shown as "Player N" in `status` and on the scoreboard); `dropplayer <N>` removes one, `splitplayers` lists them. They are not drawn yet (next build): you see them from player 1's view.
- Console-driven input for now: `p2 +forward` / `p2 -forward`, `p2 inven` / `p2 invnext` / `p2 invuse` (Action's team menu), any `p<N> <command>` runs as that player.
- Map changes, server restarts, a kick or an error affecting player 2+ drop or reconnect only that player; `addplayer` in a single-player game explains how to start a multiplayer one. Play with one player is unchanged.

## 0.0.0.4 (dev)

- Internal refactor for splitscreen: all per-player client state (game state, entities, effects, scene, centerprint/chat/lag/crosshair animation, key buttons, stufftext buffer, connection fields) now lives in a per-player "client context". Still one player; play should be exactly as in 0.0.0.3.
- The console prints the per-player memory size at startup (`client context: … bytes`).

## 0.0.0.3 (dev)

- Renderer: new `R_SetDrawRect` so all 2D (HUD, layouts, crosshair, damage tint, bloom/waterwarp composite) can draw into a screen cell; no visible change with one view.
- Spike C test switch `cl_splitspike 1..4` draws the same view in 1-4 cells (top/bottom, 2x2) to check HUDs and effects; it is not splitscreen yet and will be removed.

## 0.0.0.2 (dev)

- Gamepad backend linked in: SDL2 2.32.8 static (joystick/gamecontroller only, no SDL video/audio), new Meson option `gamepad`, on in `scripts\build.ps1`; exe about 1 MiB larger.
- The console lists connected pads at startup and with `padlist`. Pads do not control anything yet.

## 0.0.0.1 (dev)

- First build of the unmodified `aqtion` HEAD engine with MSVC 2019 via `scripts\build.ps1`; `scripts\run.ps1` runs it windowed against the read-only AQtion install.
- Build fix: a dotted `VERSION` (`0.0.0.1`) now configures and is stamped into the exe version resource and console banner.
- Known: the Steam `action\gamex86_64.dll` (TNG 2.82) crashes current engine code; local games use our own `build\gamex86_64.dll`.
