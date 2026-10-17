# STATUS

## Current state

**1.0.0.1 (dev, pending release):** the CI Linux binary links
libjpeg-turbo 3.1.0 statically (no libjpeg in NEEDED), so it starts on
Fedora / Bazzite outside Steam; verified in workflow run 39009418712 (not
released, no tag).

**1.0.0 (published 2026-10-17):**
https://github.com/rebelancap/aqtion-splitscreen/releases/tag/v1.0.0 with
`aqtion-splitscreen-1.0.0-windows-x86_64.zip` (1,432,954 bytes, SHA-256
`42692530af819d23...`) and `aqtion-splitscreen-1.0.0-linux-x86_64.tar.gz`
(762,482 bytes, SHA-256 `114943139bb0303c...`), both built by
`.github/workflows/release.yml` (run 39007863864, all jobs green: Linux job
glibc check passed, Windows VS 2022). Tree = 0.0.0.34 (Windows pass + Linux
L1-L3) + the workflow (replaces upstream's build / nightly / release), the
fork's bug-report template, Linux install text in README.md and the 1.0.0
CHANGELOG section. Public `main` = upstream `aqtion` history + one squashed
commit (6b004d0c) + follow-ups; the dev commits stay on `splitscreen` and
the Linux test box's mirror. Quirk: the `v1.0.0` tag push did not start the
workflow (twice; a `workflow_dispatch` run did), so the release was created
with `gh release create` from that run's artifacts; the `release` job is
untested until the next tag. Smoke test of the CI Windows exe from the
AQtion root with Steam's DLL: `work/steamtest-home/action/logs/rel100.log`
(version 1.0.0, 2 players, bots, screenshot `rel100-01-2players.jpg`). The
maintainer's installed exe is still the 0.0.0.34 build (same code).

**0.0.0.34 (Windows, installed in the maintainer's AQtion folder):**
`build\aqtion-splitscreen-0.0.0.34-windows-x86_64.zip` (the Windows
archive's new name, like the Linux `-linux-x86_64.tar.gz`; up to 0.0.0.33
it was `aqtion-splitscreen-<V>.zip`): `q2pro-splitscreen.exe` + the
two-platform `README.txt` (10 lines, CRLF, version filled in). Its exe
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.31; backup and both
SHA-256 in `work\maintainer-exe-backup\`). Built on `splitscreen` after the
Linux rounds L1-L3 were merged (MSVC 2019, no new warnings). New for
Windows players are the 0.0.0.33 fixes: **Infinite ammo with the sniper:
zooming no longer refills the gun** (the rounds in the gun run down
whether you zoom or not; the spares are topped up after each reload), a
player with **Gun hand: Hidden** now gets the spare top-ups too, and
**Quick Local Match never picks `credits`** (nor `beer`). Verified with
Steam's DLL in offscreen runs, also as the root-placed exe (Last round).
Everything 0.0.0.31 describes below still holds.

**Linux (branch `linux`, 0.0.0.34, round L3, 2026-10-16, the Linux test
box): the Linux release candidate is
`build/aqtion-splitscreen-0.0.0.34-linux-x86_64.tar.gz`** (`scripts/
package.sh`: `q2pro-splitscreen` 0755 + `README.txt` at the root, owner
0:0; built on Fedora 43 from `173a144c`, so it needs glibc 2.38: the
public Linux binary comes from CI on Ubuntu 22.04, Next steps 0). The same
binary is installed for the maintainer's hands-on test in
`<AQtion install>/splitscreen-test/` (`q2pro-splitscreen`, `README.txt`,
an empty `home/action` + `home/baseaq`; delete the folder when done; Next
steps 1). L3 verified: nested Gamescope at Steam Deck size (1280x800,
2-player split, engine screenshot 1280x800) after a fix
(`src/unix/video/wayland.c` probe: under Gamescope, which unsets
`WAYLAND_DISPLAY`, libwayland fell back to the desktop's `wayland-0` and
the window escaped Gamescope; the Wayland driver now needs
`WAYLAND_DISPLAY` / `WAYLAND_SOCKET`, so Gamescope gets x11 through its
Xwayland, the desktop still Wayland); the subfolder placement self-locates
(homedir `splitscreen-test/home`, Steam's `action/gamex86_64.so`, cwd the
AQtion root) and leaves the install unchanged. Everything L2 verified still
holds (splitscreen to 8, pads through SDL2, Start menu, profiles, Quick
Local Match, Infinite ammo, sound). Not tested by agents: Game mode, Steam
Input, pads pressed by a person, docked TV (Next steps 1). Build recipe
(distrobox `aqdev`, the dnf line incl. `libuuid-devel`, build / package /
run scripts, glibc note): `docs/BUILD-AND-PROVENANCE.md` Recipe L. L1
(0.0.0.32): build + self-locate; L2 (0.0.0.33): splitscreen and pads. The
Windows state below is unchanged except the 0.0.0.33 fixes and the
two-platform `README.txt` (0.0.0.34).

**0.0.0.31 (installed in the maintainer's AQtion folder):**
`build\aqtion-splitscreen-0.0.0.31.zip`; its `q2pro-splitscreen.exe`
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.30; backup and both
SHA-256 in `work\maintainer-exe-backup\`). **Sniper zoom wraps:** the
default LT is now Action's plain `lens` (TNG's own zoom cycle 1x -> 2x ->
4x -> 6x -> 1x, fov 90 / 45 / 20 / 10 / 90); LB still resets. Saved
profiles whose LT is exactly `lens in` (the old default) are switched to
`lens` once when loaded and saved with a new `binds "2"` line; the Buttons
page offers "Zoom (cycle)" and still "Zoom in" (which deliberately stops at
6x, as before). Keyboard / mouse players get the wrap with their own `bind
<key> lens` (Action's `autoexec_example.cfg` suggests it); `lens in` from a
keyboard bind still stops at 6x. **Hold-to-scroll on the Start menu:** a
d-pad or left-stick direction held on a player's overlay page (also left /
right on choices / sliders and the on-screen keyboard) steps again after
400 ms, then every 120 ms, every 60 ms after 1.5 s; a tap moves exactly one
row. Verified with Steam's DLL in offscreen runs, also as the root-placed
exe (Last round). The rest of this section describes 0.0.0.30 and still
holds.

**0.0.0.30 (installed in the maintainer's AQtion folder until 0.0.0.31):**
`build\aqtion-splitscreen-0.0.0.30.zip`; its `q2pro-splitscreen.exe`
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.29; backup and both
SHA-256 in `work\maintainer-exe-backup\`). Server options -> **Weapons**
now has one preset per weapon: All, MK23 only, Dual pistols only, MP5 only,
M4 only, M3 only, Handcannon only, Sniper only, Knife only ("Custom" for any
other Weapon bans mix). Every player spawns holding that weapon, loaded,
with its usual spares, in deathmatch and teamplay, from the first spawn;
the teamplay weapon menu lists only it; changes apply at the next spawn /
round. Levers with Steam's TNG 2.82: `wp_flags` (gun presets keep the MK23
as a working sidearm), the `dmweapon` cvar for deathmatch (bots too) and a
`choose <weapon>` per local player for teamplay / weapon choice. Verified
per preset with Steam's DLL in offscreen runs, also as the root-placed exe
(Last round). The rest of this section describes 0.0.0.29 and still holds.

**0.0.0.29 (installed in the maintainer's AQtion folder):**
`build\aqtion-splitscreen-0.0.0.29.zip`; its `q2pro-splitscreen.exe`
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.28; backup and both
SHA-256 in `work\maintainer-exe-backup\`). Server options -> **Infinite
ammo** now means an infinite reserve: the rounds in the gun run down as you
fire, the gun stops at 0 and you reload as usual; the spare magazines /
shells are topped back up right after each reload (`give <ammo item>`, which
never touches the loaded rounds), and knives / grenades still come back one
for one. Cheats handling and the live apply (every start path, On / Off at
once) are unchanged from 0.0.0.28. Verified per weapon with Steam's DLL in
offscreen runs, also as the root-placed exe (Last round). The rest of this
section describes 0.0.0.28 and still holds, except that guns are no longer
refilled after every shot.

**0.0.0.28 (installed in the maintainer's AQtion folder until 0.0.0.29):**
`build\aqtion-splitscreen-0.0.0.28.zip`; its `q2pro-splitscreen.exe`
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.27; backup and both
SHA-256 in `work\maintainer-exe-backup\`). Server options -> **Infinite
ammo** now works at once in every path: switched On in a running game (no
Restart map), on the main-menu page before Start / Quick Local Match, saved
On from an earlier session, loaded from a saved set, for players who join
later, in teamplay with Action's weapon menu open, and in games started from
AQtion's own menus or the console. `cheats` is written without the "next
game" latch, the guns are refilled right after every shot / reload (knives
and grenades one for one), and the refill goes straight onto each player's
connection. Verified with Steam's DLL in offscreen runs, also as the
root-placed exe (Last round). The rest of this section describes 0.0.0.27
and still holds.

**0.0.0.27 (installed in the maintainer's AQtion folder):**
`build\aqtion-splitscreen-0.0.0.27.zip`; its `q2pro-splitscreen.exe`
replaced `<AQtion>\q2pro-splitscreen.exe` (0.0.0.26; backup and both
SHA-256 in `work\maintainer-exe-backup\`). New: Server options ->
**Humor sounds** On / Off (default On). AQtion's joke clips (farts, laughs,
"newbie", quotes) are played by Steam's own game DLL whenever a living
player has been idle 15 s (`ppl_idletime`, again every 15 s while idle), as
`insane/insane1-11.wav` (AQtion replaced Quake 2's files with its clips);
`user/*.wav` come from the `voice` command. Off: no local player starts an
`insane/` or `user/` sound (client side, any server, live), and in a game
you host `ppl_idletime` (and `bot_randvoice` for our DLL) is set to 0, live
and at every start / map / reset / set load. The row replaces the dev-DLL
"Joke sounds" row. The rest of this section describes 0.0.0.26 and still
holds.

**0.0.0.26 (installed in the maintainer's AQtion folder):**
`build\aqtion-splitscreen-0.0.0.26.zip`; its `q2pro-splitscreen.exe`
replaced the maintainer's `<AQtion>\q2pro-splitscreen.exe` (0.0.0.24, his
copy from 2026-10-09; backup and both SHA-256 in
`work\maintainer-exe-backup\`). The only change from 0.0.0.25: Server
options -> Infinite ammo writes `cheats` only while the row is on (`cheats
1`) and when the row that turned it on is switched off (`cheats 0`); with
the row off a hand-set `cheats 1` survives starting a game, Restart / Next
map, Reset to defaults and loading a saved set. The rest of this section
describes 0.0.0.25 and still holds.

**0.0.0.25 (per-player look and menu sizes, infinite ammo):**
`build\aqtion-splitscreen-0.0.0.25.zip`. Every player's Settings page has
Crosshair, Crosshair size (default 3x, max 4x), Crosshair colour, Text size
(chat / kill / centre messages, default 2x), HUD opacity, AQtion menu size
and Splitscreen menu size, each applied in that player's own view and kept
in its profile. AQtion's own menus keep their size after the screen splits
(the cause was a missing prototype: ui.c read a float as an int) and shrink
only as far as needed to fit the view. The profile row reads "Profile:
Guest" / "Profile: <name>". Server options -> Infinite ammo works with
Steam's DLL (cheats on + `give ammo` for the local players; Steam's TNG
2.82 has no real infinite-ammo setting). All verified in offscreen runs
(Last round). The rest of this section describes 0.0.0.24 and still holds.

**0.0.0.24 (offscreen test window + the 0.0.0.23 windowed checks):**
`build\aqtion-splitscreen-0.0.0.24.zip`. The 0.0.0.23 menus passed their
windowed checks (Last round) after five small fixes; agent test runs now use
`vid_noactivate 1` / `scripts\run.ps1 -Offscreen`, a window that never takes
focus, the mouse, the sound device or the screen mode, and `pad_enable 0` no
longer starts SDL's controller code at all (docs/BUILD-AND-PROVENANCE.md
"Agent testing: offscreen window"). The rest of this section describes
0.0.0.23 and still holds.

**0.0.0.23 (menus round; compiled and packaged, not yet run windowed):**
`build\aqtion-splitscreen-0.0.0.23.zip`. AQtion's own menus draw 2x
(`cl_splitMenuSize`, limited so each menu fits); every player's Start menu
is Team / Weapons (Weapons hidden when a hosted game has no weapon choice) /
Scoreboard / Radio / Settings (name, hand, HUD opacity, crosshair colour,
Controls with look / dead zone / curve rows and a Buttons remap page) /
Profile... ; player 1 adds Server options (host only; Steam TNG 2.82 cvars,
saved sets, restart / change / next map), Splitscreen settings, Game menu.
At the main menu Start on **any** pad (or F1) opens the Splitscreen page,
whose first row is Quick Local Match; a hint line says so while a pad is
connected; keyboard and mouse drive our overlay. No entries in AQtion's
native menus. Design doc §7.1 "As built (0.0.0.23)". The rest of this
section describes 0.0.0.22 and still holds.

Dev build `0.0.0.22` builds and runs, and is packaged as a **release
candidate of two files** (`scripts\package.ps1` ->
`build\aqtion-splitscreen-0.0.0.22.zip`: `q2pro-splitscreen.exe` +
`README.txt`; **put the exe in the AQtion folder and start it**).
**Two placements (0.0.0.21, `split_self_locate`):** exe in the AQtion
root (its folder has `baseaq\` + `action\`) -> `basedir` / `libdir` = the
root, `homedir` = `<AQtion>\splitscreen` (created), never a root
`gamex86_64.dll`; exe in a subfolder of AQtion -> the 0.0.0.19/20 rule
below (`home\` beside it). **In both, the working directory is the AQtion
root (0.0.0.22)**, as under Steam, so the game DLL finds its own files
(bot names, `action\terrain\*.ltk` bot nodes, `action\tng\*.aqg`
locations) and writes `items.txt` / bot node files where Steam's AQtion
does. The `split:` log line names the placement and the working directory.
**It hosts with Steam's own `action\gamex86_64.dll`** (TNG 2.82, legacy
game API 4) through an adapter in `src/server/game.c` (0.0.0.20); our own
DLL is only used when one sits beside the exe (dev) or via
`sys_forcegamelib`. **First run** (no `home\action\q2config.cfg`):
fullscreen at the desktop resolution, no name prompt (player 1 is "Player
1"), AQtion's native bot menu and our host page start only installed maps;
first-run pass on the real Steam folder clean (`work/firstrun-*`). **The exe
finds the game data and its `home\` folder itself when installed in
`<AQtion>\splitscreen\`; no launcher** (`src/windows/system.c`
`split_self_locate`): if the exe's folder has no `baseaq\` but its parent
has `baseaq\` + `action\`, `basedir` and `libdir` = the parent, `homedir` =
`<exe dir>\home` (created), `sys_forcegamelib` = `<exe dir>\gamex86_64.dll`
only if that file exists, `game` = `action`, each only when not given on
the command line; one `split: running from ...` log line. From the repo's
`build\` nothing changes. So config, profiles, screenshots, demos, logs and
`.conhistory` go to the homedir (`<AQtion>\splitscreen\` or
`<exe dir>\home\`) and never into Steam's `action\` / `baseaq\`, and only
the homedir's `q2config.cfg` is read.
`scripts\release\install.ps1` (manifest install, `-Uninstall` / `-Purge`)
stays in the repo as an optional dev script, not shipped;
`scripts\build.ps1`, Meson 1.12.1 +
MSVC 2019 14.29 x64 -> `build\q2pro.exe`, `q2proded.exe`,
`gamex86_64.dll`, `BUILD-INFO.txt`; `scripts\run.ps1` runs it windowed
against the read-only install with our own game DLL). **M1 is complete and
passed clean-room acceptance** (`work/ACCEPT-M1.md`). In a multiplayer game
local players 2..8 (`cl_splitMaxPlayers`, default 8 since 0.0.0.16) join as separate
clients of player 1's server, each in its own cell with its own view, HUD,
layouts, chat and userinfo (`p<N>_*` shadows); one mixer,
dedupe (nearest listener since M2-T6). **Pads (M2-T1..T3):** SDL2 opens every game controller; a pad
nobody has **joins by holding A 0.7 s** (the `addplayer` path; refusals
shown in the hint corner), **leaves with Back+Start 2 s**, and any button
on a free pad takes over a player missing a pad. `cl_splitP1Input pad`
(default): the first pad to press a button is P1's (keyboard/mouse still
work); `kbm`: the first pad joins as P2. An unplugged extra's pad: "controller
disconnected" in its cell, the same pad back within `cl_splitPadGrace` (30 s)
reattaches, else the player leaves. Join hint "Player N: hold A to join"
(`cl_splitJoinHint`, Q3 §14.1). Every pad player gets the AQ2 default
bind table (UrT layout mapped to Action commands, design §5.2) unless it
has its own `pbind`s; while a player's menu/scoreboard is up, d-pad
up/down + A navigate it and B closes it (verified: a pad-only player joined
a team, picked weapon and item). Pad presses set that player's
`BUTTON_ANY`. `padinject <N>` drives virtual pad N for headless tests.
**Menus (M2-T4):** Start on a pad opens that player's own overlay menu in
its cell (Team & weapons = Action's menu, Scoreboard, Radio, Settings with
an on-screen keyboard for the name, Leave game; P1 also Game menu and
Splitscreen settings). With 2+ players every engine menu (Escape, "Choose
your name") draws inside P1's cell while the other cells keep playing, and
P1's pad drives it (d-pad / A / B; A on a text field opens the keyboard).
**Host settings page** (since 0.0.0.13, the maintainer's rule): a page of
P1's overlay, in P1's cell in game and full screen at the main menu by
**Start on P1's pad**; AQtion's own menus have no Splitscreen entry. It
applies live and has "Start multiplayer game". 1-2 px black borders between
cells (`cl_splitBorder`). One-player play and performance unchanged.
**Profiles (M2-T5):** a player's name, hand, look settings and pad binds can
be saved as a named profile (`<homedir>/action/profiles/<Name>.cfg`, overlay
Profile page or `profile save <p> <name>`); named profiles save themselves
(1 s after a change, on leave / quit); a pad remembers its profile
(`profiles/_pads.cfg`, by device key) and loads it when it joins again,
also after a restart. Players 2..8 without a profile are guests ("Player N",
defaults) whose changes last until they leave; `p<N>_*` / `pbind 2..8` are no
longer written to `q2config.cfg`. Player 1 keeps its config unless a profile
is picked for it or P1's pad remembers one (since 0.0.0.13: the pad's
profile comes with it and goes when the pad steps out / at quit; since 0.0.0.14 every config write, `writeconfig` and the fs_game-change write too, saves P1's own settings, not the profile's). **Sound (M2-T6):** on DirectSound (the default here,
`s_enable 1`) each sound is heard from the nearest player's view, mono with
3+ players (`s_splitPan`), and loop sounds in any player's view play;
OpenAL (`s_enable 2`) keeps player 1's listener + dedupe. **M2 is
accepted** (clean-room `work/ACCEPT-M2.md`, PASS with notes; notes 1-5 fixed
in 0.0.0.13, N6/N7 left) and waits only for the maintainer's pad pass.
**Online (M3-T1+T2, 0.0.0.15):** when player 1 plays on another machine's
server, extras join that server on their own connections (serialized
challenges, own qports, blank steamid/discordid), follow map changes
(`gamemap`: changing/reconnect; `map` on a q2pro dedicated server: a full
restart via `svc_reconnect`) and `sv_force_reconnect` each on their own; a
refusal or kick stays in that player's cell for 10 s ("can't join: this
server allows only 3 players from one connection", "Player 3 was kicked").
If the server quits or dies and player 1 `reconnect`s, the extras come back
with their profiles. Stufftext from an extra's server only changes that
player (`CL_SplitStufftext`, design §8); the rest is ignored with a console
note. Verified against a dedicated server from the same build on this
machine's LAN address (`scripts\run-ded.ps1`); real public servers are the
maintainer's M3-T3.
**5-8 players (M4-T1, 0.0.0.16):** layouts of the Q3 fork: 5 = 3+2, 6 = 3x2,
7 = 4+3, 8 = 4x2 with `cl_splitFill 1` (default; 3 players are now 2 on top
+ 1 wide), `cl_splitFill 0` = equal cells with black gaps (7-8: 3x3);
`cl_splitWidePlayer` picks who gets the first wide cell (0 = last joined);
both on the host page, live. Cells narrower than 640 px draw their HUD,
scoreboards, Action menus and chat at a fractional scale so they fit (at
320x360 the text is too small to read; 426x240 is legible). Bloom works per
cell (scissor bug fixed) and its FBO is no longer rebuilt per scene. Perf
(RTX 5090, uncapped): 8 views at 1440p 412-460 fps, 1 view ~700-930 fps,
CPU / driver bound (`cl_splitperf 1` shows the split).
**0.0.0.17:** player 1's console notify lines stay in P1's cell; chat in
short cells moves above the status bar instead of over scoreboards / menus;
host row "Cell layout"; notices never take a player's cell.

## Last round

**Linux release binary without libjpeg.so.8 (1.0.0.1), 2026-10-17,
branch `splitscreen` (tree synced to `main`).** The `linux-x86_64` job now
builds libjpeg-turbo from `subprojects/libjpeg-turbo.wrap` as a static
subproject (`--force-fallback-for=libjpeg-turbo` next to
`-Dwrap_mode=nofallback`; Meson 1.12.1 from pip because the wrap needs
>= 0.64 and 22.04's apt has 0.61) and the "Check glibc and NEEDED" step
fails if NEEDED lists libjpeg or the binary lacks libjpeg-turbo. No
meson.build change. Run 39009418712 (`workflow_dispatch` on `main`, all
jobs green, release job skipped): gcc 11.4 builds the subproject (only
`-Wimplicit-function-declaration` warnings for `setenv`, which gcc 15
makes an error); NEEDED = libz, libm, libpng16, libcurl.so.4, libSDL2-2.0,
libX11, libXi, libGL, libwayland-client / -cursor / -egl, libdecor-0,
libEGL, libc; max GLIBC_2.34; binary 2,167,016 bytes (1.0.0: 1,556,712),
contains "libjpeg-turbo version 3.1.0" and 1.0.0.1. On the Linux test box
(Bazzite 43, only `libjpeg.so.62`), `ldd` on the CI binary resolves every
library, none "not found" (one harmless "libcurl.so.4: no version
information" note, Fedora's curl has no symbol versions); the game was not
run there. Windows job unchanged and green (version resource 1.0.0.1).

## Previous rounds

**Windows pass of 0.0.0.34 after the Linux merge, 2026-10-16, branch
`splitscreen`.** `linux` (L1-L3) was fast-forward-merged into
`splitscreen` (`7c8c91d4`); this round builds, checks and installs it on
Windows and renames the Windows archive.

- **Build: PASS** (`work/w34-package.log`). `scripts\package.ps1` = clean
  `build.ps1` (MSVC 2019 14.29 x64, Meson 1.12.1, release). The warning list
  is identical to 0.0.0.31's (`work/zoom31-package.log`: 21 known C4090 /
  C4098 in `src/action`, `qal.c`, `qgl.c`, `entities.c`); none in the
  merged shared files (`in_gamepad.c`, `split_menu.c`, `inc/system/
  system.h`) or anything else of ours. `q2pro.exe` version resource
  0.0.0.34, 3,491,840 bytes.
- **Package: PASS.** `package.ps1` now writes
  `build\aqtion-splitscreen-0.0.0.34-windows-x86_64.zip` (SHA-256
  `5b2cc433491a27867bf7979fe766a770085cabb2895a71fbdd1913d8968fadb7`):
  `q2pro-splitscreen.exe` (SHA-256
  `486b2286a38843d02a580c155e2c8da29f3d3be3235c1a05e28acef09ec49e02`) and
  `README.txt` (930 bytes = the 10-line template with 10 CRLF, no bare
  LF, no BOM, `@VERSION@` -> 0.0.0.34). Docs naming the zip:
  `docs/BUILD-AND-PROVENANCE.md` (Recipe L paragraph), `README.md` (Build
  from source). The workflow's name is the other agent's (Next steps 0).
- **Tests** (offscreen `run.ps1 -Offscreen -AllowPrimary -GameLib steam
  -WorkDir work\scratch -Wait`, `pad_enable 0`, `s_enable 0`, Steam's
  `action\gamex86_64.dll`, two players, the game window foreground in 0 of
  every sample; exit code 0 every run):
  - **(a) Sniper + Infinite ammo, zoom never refills: PASS**
    (`work/w34-a.log`, cfg `w34-a-test.cfg`, `w34a-01..07`). Sniper only
    (`wp_flags 33`) + `cl_splitInfiniteAmmo 1`, urbanjungle dm: both spawn
    6 / 10 -> "spawn, give ammo" -> 6 / 20; fire: 6 -> 3; then **16 `lens`
    presses each (four full 2x / 4x / 6x / 1x cycles, fov 90 -> 45 -> 20 ->
    10 -> 90): no "spawn, give ammo", the loaded count stays 3**
    (`w34a-03-zoomed-4x`: both scopes at 4x, 3 in the gun, 20 spare); fire
    to 0, the gun stops; seven reloads, one round each: the first takes a
    spare -> "give AP Sniper Ammo x1" -> reserve 29, then 28 ... 24 (the
    known 0.0.0.29 overshoot) for both; a zoom cycle after the reloads,
    still nothing given.
  - **(b) Gun hand: Hidden gets top-ups: PASS** (same run): P2 had
    `p2_hand 2` (`cvarlist` in the log) and got the same spawn top-up and
    "p2 give AP Sniper Ammo x1" as P1.
  - **(c) Quick Local Match x10: PASS** (`work/w34-c.log`, cfg
    `w34-c-test.cfg`). 26 maps installed (`dir maps` lists `beer` and
    `credits`); 10 `splitquickmatch`: simplicity, serebryanka, icity,
    retro3, serebryanka, bwcity, wfall, junglecity, retro3, cityscape;
    never beer or credits (10 draws; the skip itself is the 0.0.0.33
    `SM_QuickMatchSkip`).
  - **(d) Layout + hold-to-scroll smoke: PASS** (`w34-a.log`):
    `splitplayers`: P1 loopback cell 0,0 1280x360, P2 127.0.0.1:27910 cell
    0,360 1280x360, both active, distinct qports (`w34a-01`); Server options
    (29 rows): d-pad tap 48 ms -> row 2, hold 1985 ms -> repeats at 400,
    521, 641 ... 1482 ms, 60 ms from 1602 ms, row 20 (`w34a-07`): the
    0.0.0.31 numbers.
  - **Root-placed exe: PASS** (`work/w34-g.log`, cfg `w34-g-test.cfg`,
    `w34g-01..03`). `<AQtion>\q2pro-splitscreen-test.exe` = the zip's exe,
    `-SelfLocate -HomeDir work\steamtest-home`: "split: running from ...
    (AQtion root)", Steam's DLL, `version` 0.0.0.34; Sniper only +
    Infinite ammo, Quick Local Match -> newrok, P2 with hand 2: both 6 / 20,
    fire to 3, three zoom cycles: no refill, empty, reloads with "give AP
    Sniper Ammo x1" for both. Test exe removed; the install root,
    `action\` and `action\terrain\` (40 files) identical by SHA-256 before /
    after (`work/w34-aqroot-backup/before.sha256`, `after.sha256`;
    `items.txt` unchanged); the steamtest-home q2config restored
    (`b43cb85d...`).
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.31
  (`54976fdc...bd8e68e6c`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.31.exe`, the 0.0.0.34
  exe from the zip copied over `<AQtion>\q2pro-splitscreen.exe` (version
  resource 0.0.0.34, 3,491,840 bytes, SHA-256 `486b2286...9ec49e02`).
  `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`).

**L3: Steam Deck shape, packaging, test install (0.0.0.34), 2026-10-16,
the Linux test box, branch `linux`.** Per `docs/LINUX-BOOTSTRAP.md` round
L3. All runs through `scripts/run.sh` with `pad_enable 0`, `s_enable 0`,
Steam's `action/gamex86_64.so`, one game at a time under `timeout`, exit
code 0 every time; `work/aqhome/action/config.cfg` restored byte-identical
after each run. Evidence in `work/l3/`.

- **1. `scripts/package.sh`: PASS** (`l3-package.log`,
  `l3-archive-listing.txt`). Clean release build (`build.sh --clean`, 10 s;
  `--no-build` packs as is), checks BUILD-INFO `version: 0.0.0.34` and the
  new `configuration: release` line, then `build/aqtion-splitscreen-
  0.0.0.34-linux-x86_64.tar.gz` (736 KiB, sha256 in the listing):
  `-rwxr-xr-x 0/0 1503040 q2pro-splitscreen`, `-rw-r--r-- 0/0 920
  README.txt`. Name: the Windows zip stays `aqtion-splitscreen-<V>.zip`
  (`package.ps1` not changed here); Open questions has the rename default.
- **2. README.txt for both platforms: PASS** (10 lines, max 100 columns;
  the archive's copy in `l3-archive-listing.txt`). Windows sentences kept
  ("Windows: put q2pro-splitscreen.exe ..."), new: "Linux / Steam Deck: put
  q2pro-splitscreen next to q2pro there (chmod +x if needed) and run it, or
  add it to Steam (Desktop mode: Add a Non-Steam Game; on Steam Deck it then
  shows in Game mode)." "delete the exe" -> "delete the program".
  `package.ps1` does a plain `.Replace('@VERSION@', $version)` (then CRLF);
  `package.sh` does the same with `sed` (LF).
- **3. Gamescope at 1280x800: PASS after a fix.** `run.sh --gamescope`
  (new: `gamescope -W 1280 -H 800 -w 1280 -h 800 -- <exe>`, game window
  1280x800, `--size WxH`, extra flags via `$AQ_GAMESCOPE_ARGS`). Gamescope
  gives its child `DISPLAY=:1` (its Xwayland) and no `WAYLAND_DISPLAY`
  (`GAMESCOPE_WAYLAND_DISPLAY=gamescope-0`, `SDL_VIDEO_MINIMIZE_ON_FOCUS_
  LOSS=0`). (a) Default driver, unfixed build: "Using video driver:
  wayland" on the desktop's `wayland-0`, the window outside Gamescope at
  desktop scale 2, screenshot 2560x1600 (`l3-gamescope-a-default-
  escaped.log`, `.jpg`). (b) `+set vid_driver x11`: inside Gamescope, bots,
  `addplayer` -> 2 players rows 1+1, cells 1280x400, screenshot 1280x800
  (`l3-gamescope-b-x11.log`, `l3-gamescope-b-x11-1280x800.jpg`). (c)
  `AQ_GAMESCOPE_ARGS=--expose-wayland`, default driver: Wayland inside
  Gamescope, same 2-player result, 1280x800 (`l3-gamescope-c-wayland.log`).
  **Fix:** the Wayland probe (`src/unix/video/wayland.c`) returns false
  without `WAYLAND_DISPLAY` / `WAYLAND_SOCKET` (an explicit `vid_driver
  wayland` is not probed). (d) Fixed build, default: "Using video driver:
  x11", 0.0.0.34, 2 players, 1280x800 (`l3-gamescope-d-default.log`,
  `l3-gamescope-d-default-1280x800.jpg`, `l3-gamescope-d-stdout.txt`);
  plain desktop run still "wayland" (`l3-e-desktop.log`). Clean exit every
  time (Gamescope: "Primary child shut down!"). `SDL_VIDEODRIVER` does not
  matter: the engine's own x11 / wayland drivers do the video (SDL only
  pads / sound), only `vid_driver sdl` would read it.
- **4. Install placement: PASS** (`l3-splitscreen-test.log`, `.jpg`,
  `l3-install-check.txt`). The tarball's binary + README copied to
  `<AQtion install>/splitscreen-test/` (sha256 = `build/linux/
  q2pro-splitscreen`; the SD card is exFAT, every file shows 0755 there);
  one run `--self-locate`, no homedir (new in run.sh: `--self-locate`
  passes none unless `--homedir`), bots 60 s: "split: running from
  <AQtion>/splitscreen-test (subfolder of AQtion), basedir <AQtion>,
  homedir <AQtion>/splitscreen-test/home, game library from libdir
  <AQtion>, working directory <AQtion>", "Loaded game library from
  <AQtion>/action/gamex86_64.so", API 4 adapted, screenshot written,
  version 0.0.0.34. After: `action/terrain/*` = `work/l1/terrain-before.
  sha256` (21 files); the game library wrote `<AQtion>/items.txt`
  ("Rejected item: worldspawn ...") -> deleted; the full install listing
  outside `splitscreen-test/` equals the one taken before the round. Left
  for the maintainer: `splitscreen-test/{q2pro-splitscreen, README.txt,
  home/action/, home/baseaq/}`; the run's `config.cfg` (it had archived
  `pad_enable 0`), log, screenshot and `.conhistory` were removed so his
  first start has defaults.
- **5. ldd / glibc: done, two findings** (`l3-ldd.txt`). NEEDED: libz,
  libm, libpng16, libcurl.so.4, libSDL2-2.0.so.0, libjpeg.so.62, libX11,
  libXi, libGL, libwayland-client / -cursor / -egl, libdecor-0, libEGL,
  libc; no libstdc++ (no GLIBCXX / CXXABI). Max GLIBC_2.38, only
  `__isoc23_sscanf` / `strtol` / `strtoul` (gcc 15 + glibc 2.42 C23
  redirects); everything else <= 2.34, so an Ubuntu 22.04 (2.35) build
  will need at most 2.35. SteamOS 3.x glibc could not be checked offline
  (Open questions). Findings: (a) under Steam's library path (pinned
  libs, host, scout runtime: the order Steam uses, read from the running
  client) libcurl.so.4 is the runtime's old one: `ldd -r` lacks
  `curl_multi_poll` / `curl_multi_wakeup`; a 30 s bot game with that
  `LD_LIBRARY_PATH` runs fine (`l3-f-steamrt.log`, lazy binding) because
  curl is only called for HTTP downloads; (b) an Ubuntu build links
  `libjpeg.so.8` (Steam's own AQtion `q2pro`, Ubuntu 20.04 gcc 9, does,
  and it is "not found" on this Fedora host); a static libjpeg-turbo
  fallback did not build with gcc 15 here (removed). Both in Open
  questions.
- **6. Maintainer's checklist:** Next steps 1.
- **Build:** VERSION 0.0.0.34; `scripts/package.sh` clean release build
  in `aqdev`, 0 warnings in our files (`src/client/split/`,
  `src/server/game.c`, `src/unix/`). Commits on `linux`: scripts /
  README / VERSION, the Wayland probe fix, then docs.

**L2: splitscreen and pads on Linux (0.0.0.33), 2026-10-16, the Linux
test box, branch `linux`.** Per `docs/LINUX-BOOTSTRAP.md` round L2. All
runs: `scripts/run.sh` (windowed 1280x720 on the desktop session, a
2560x1440 buffer at desktop scale 2), Steam's `action/gamex86_64.so` from
`$AQ_BASEPATH`, homedir `work/aqhome`, `+set sys_console 1` with commands
piped into stdin, `developer 1` / `pad_debug 1`, virtual pads via
`padinject`; one game at a time, each under `timeout`, exit code 0 every
time. Evidence in `work/l2/` (logs `l2-*.log`, screenshots `l2-*.jpg`).

- **1. Players and layouts: PASS** (`l2-a-players.log`, deathmatch
  urbanjungle with bots, `maxclients 12`). Virtual pad 1 A -> "took over
  P1"; pad 2 A held 700 ms -> "joining as P2"; pads 3 and 4 -> P3, P4; a
  299 ms tap on pad 5 joins nobody. `splitplayers`: all `active`, one
  server (loopback / 127.0.0.1:27910), distinct qports (137 / 174 / 211 /
  248) and sockets. Cells: 2 = rows 1+1 (2560x720 each,
  `l2-a1-2players.jpg`), 4 = 2x2 1280x720 (`l2-a2-4players.jpg`), 3 = 2+1
  wide. Pad 4 Back+Start "held 2001 ms" -> "Player 4 left: left
  (Back+Start)"; `dropplayer 3` -> "Player 3 left" and its pad freed
  (`l2-a5-after-leave.jpg`). Six `addplayer` -> 8 players, 4x2 cells of
  640x720, all active, qports distinct (`l2-a6-8players.jpg`).
- **2. Pads through the engine's SDL2: PASS.** A real controller IS
  attached on this box: the built-in pad (an Xbox 360 pad, unreadable to
  the user), the handheld daemon's virtual "Xbox 360 Controller" (045e:028f,
  the one SDL opens) and Steam Input's virtual pad (28de:11ff, ignored by
  SDL outside Steam). Nothing was pressed on them; SDL listed one device
  and it stayed "-> no player" throughout. With `pad_enable 1` from the
  start (`l2-b-sdlpads-wayland.log`, `l2-b-sdlpads-sdl.log` = `vid_driver
  sdl`, where SDL also runs the video; the engine's `Sys_Quit` stays the
  only `SDL_Quit`): "pad: SDL 2.32.64 joystick+gamecontroller
  initialised", "1 SDL joystick device(s)";
  virtual pads 1 and 2 join as P1 / P2 next to the real one; `set
  pad_enable 0` live -> "real devices closed", virtual pads keep their
  players; `set pad_enable 1` -> the real pad is back; quit -> "pad: shut
  down", exit code 0, no SDL error, the window ran on through every
  start / stop. `l2-b-sdlpads-runtime-start.log`: started with `pad_enable 0`, then `set pad_enable 1` live
  starts SDL's joystick subsystems (the `PAD_Frame` path). `pad_enable 0`
  at start: "SDL joystick subsystems not started" (L1, again in every
  other log). **run.sh fix:** `--pads` only dropped `+set pad_enable 0`,
  so the archived `pad_enable 0` in `config.cfg` won; it now passes `+set
  pad_enable 1`.
- **3. Start menu hold-to-scroll: PASS** (`l2-d-menu.log`). Injected Start
  on P1's pad -> "P1 menu: opened" (`l2-d0-p1-start-overlay.jpg`); a 52 ms
  tap -> row 1 -> 2. Server options (`padmenu 1 server`, 29 rows,
  `l2-d2-server-before.jpg`): two 50 ms taps -> rows 2, 3; d-pad down held
  1000 ms -> row 4 + repeats at 400, 520, 640, 760, 881 ms -> row 9 (Bot
  skill, `l2-d4-server-after-1s-hold.jpg`); up held 3 s -> 120 ms steps,
  60 ms from 1603 ms, wrapping 1 -> 29, 35 steps; stick (LY 1) tap 51 ms
  -> one row, held 1 s -> six rows; P2's own overlay in its cell repeats
  the same (`l2-d7-p2-overlay.jpg`). Identical to the Windows 0.0.0.31
  numbers.
- **4. Profiles: PASS** (`l2-e-profile-save.log`, `l2-f-profile-load.log`,
  the files as `l2-profile-L2Test.cfg`, `l2-profile-_pads.cfg`). `profile
  save 2 L2Test` -> `work/aqhome/action/profiles/L2Test.cfg` (`binds "2"`,
  LT `lens`) and `_pads.cfg` (`pad "virtual-2" "L2Test"`); quit, start
  again: "2 profile(s), 1 pad(s) remembered", pad 2 joins -> "P2 plays as
  L2Test", `p2_hand` 2 kept. An old-format copy (`l2-f-L2Old-before.cfg`:
  no `binds` line, LT `lens in`) via `addplayer L2Old` -> "P3's LT is now
  Zoom (cycle)", saved with `binds "2"` / LT `lens`
  (`l2-f-L2Old-after.cfg`); loaded again in a later run
  (`l2-g-quickmatch.log`, G11): no second message.
- **5. Server options -> Quick Local Match: PASS after a fix**
  (`l2-g-quickmatch.log`). Main menu, Start on virtual pad 1 -> "Start at
  the main menu: player 1's pad now", the Splitscreen page; up x2 + A ->
  Server options (26 rows); Bots -> 3, Infinite ammo -> On, Weapons ->
  Sniper only (wp_flags 33, dmweapon "Sniper Rifle"), Game type ->
  Deathmatch, all by d-pad (`l2-g0-server-options-set.jpg`); B, down x2
  wraps to row 1 Quick Local Match (`l2-g1-splitscreen-quickmatch-row.jpg`),
  A -> "quick local match on nightwalk", "starting nightwalk, game type
  dm, bots 3", pad 2 joins (`l2-g2-quickmatch-nightwalk-2p.jpg`). **Bug:**
  the first run picked `credits`: "Warning: failed to find deathmatch spawn
  point", the players and bots spawn on one spot and telefrag each other
  every second (`l2-k-credits-map.log` / `.jpg`: the same with `+map
  credits`). It is AQtion's credits scene (one deathmatch spawn in
  `pak0.pkz`); the Windows install has it too. **Fix:**
  `SM_QuickMatchSkip()` (`src/client/split/split_menu.c` ~3027) skips
  `credits` as it skipped `beer`.
- **6. Infinite ammo + Sniper only: PASS after a fix** (`l2-h-ammo.log`,
  `splitstartgame urbanjungle`, deathmatch, no bots, `wp_flags 33`, P2 =
  profile L2Test with `hand 2`). Both players spawn with the sniper 6 / 10
  ("spawn, give ammo" -> reserve 20, the carry limit); fire 3 s: 6 -> 3;
  LT x4: P1 and P2 fov 90 -> 45 -> 20 -> 10 -> 90, the loaded count stays 3
  (`l2-h1-zoom-6x.jpg`, both scopes at 6x); fire to 0, the gun stops; seven
  reloads: one round each, the first takes a spare -> "give AP Sniper Ammo
  x1" -> reserve 29, then 28 ... 24 (>= 20 full, the known 0.0.0.29
  overshoot) for both (`l2-h2-after-reloads.jpg`). **Bugs (every
  platform), shown by `l2-h-ammo-before-fix.log`, the same script on the
  unfixed build:** (a) zooming the sniper made Action send no gun model;
  the infinite-ammo code read that as "out of play", and the next unzoomed
  frame as a new life: "p1 spawn, give ammo (loaded 3 ...)" -> loaded 6, a
  free reload; (b) a player with Gun hand: Hidden (`hand 2`) has no gun
  model at all, so P2 never got a top-up (reserve 10 -> 4). **Fix:**
  `CL_SplitMenuFrame` (`split_menu.c` ~2103) counts a player as holding a
  weapon when the gun model or the HUD's ammo / clip icon is set. Teamplay
  before joining a team still gets nothing (`l2-t-teamplay-noteam.log`).
  Sniper-only spawn in deathmatch: P1 "v_sniper loaded 6 reserve 10" at the
  first spawn of the Quick Local Match too (`l2-g-quickmatch.log`).
- **7. Sound: PASS** (`l2-c-sound-s1.log`, `l2-c-sound-s2.log`,
  `--sound`, 2 players, both fire). `s_enable 1`: "Using SDL audio driver:
  pipewire", 44100 Hz, the split mixer (nearest listener) runs, "Shutting
  down SDL audio."; `s_enable 2`: "OpenAL initialized." (OpenAL Soft),
  "Shutting down OpenAL."; no crash, exit 0. Two `Couldn't load
  sound/weapons/*1b.wav: Missing data chunk` lines are Steam's data. All
  other runs had `s_enable 0`.
- **Build:** `scripts/build.sh` (aqdev, incremental after the two edits):
  0 warnings in our files (`src/client/split/`, `src/server/game.c`,
  `src/unix/system.c`). The changes are platform-independent C in
  `split_menu.c` (no `#if`); the Windows build is untested here and should
  be built once after the merge.
- **Install and homedir:** `action/terrain/*` hashes equal
  `work/l1/terrain-before.sha256` after the last run; no `items.txt` in the
  install root (every run's cwd was `work/cwd`); no exe copied into the
  install. `work/aqhome/action/config.cfg` restored byte-identical after
  every run (several runs archive `pad_enable` / `cl_split*` / `wp_flags`);
  the test profiles were removed from `work/aqhome` (copies in `work/l2/`).


**L1: Linux build, self-locate, bot game (0.0.0.32), 2026-10-16, the
Linux test box, branch `linux`.** Per `docs/LINUX-BOOTSTRAP.md` round L1.

- **Build:** `scripts/build.sh` (new; re-enters the `aqdev` distrobox via
  `$CONTAINER_ID`, VERSION check, `build/linux/BUILD-INFO.txt`; `--clean`,
  `debug`) -> `build/linux/{q2pro, q2pro-splitscreen (stripped copy),
  q2proded, gamex86_64.so}`. Meson needed one package beyond the brief's
  list: `libuuid-devel` (`meson.build` uuid dependency); `-Dwrap_mode=
  nofallback` worked, nothing was downloaded. Meson warns that jansson and
  uuid have no static library and links them shared (jansson only for our
  dev game library). Clean build: 124 compiler warnings, **0 in our files**
  (`src/client/split/`, `src/server/game.c`, `src/unix/system.c`); all are
  upstream's (`src/action/` 90-odd, `HAVE_*` redefinitions where upstream
  includes SDL.h, `mvd/parse.c`, `entities.c`, `effects.c`). Two of ours
  fixed: `in_gamepad.c` `Pad_Detach` bounds the slot index (gcc
  -Warray-bounds), and undefines `HAVE_STRCASECMP`/`HAVE_VSNPRINTF` before
  SDL.h as it already did `HAVE__STRICMP`.
- **Self-locate:** `split_self_locate()` + `Sys_PrintLocationNote()` in
  `src/unix/system.c` (called in `Sys_Init` after upstream's macOS bundle
  block, before the `basedir`/`homedir`/`libdir` cvars); the Windows logic
  with `readlink("/proc/self/exe")`, `stat`, `mkdir 0755`, `chdir`;
  `sys_forcegamelib` = `<exe dir>/gamex86_64.so` only in a subfolder and only
  if present. `inc/system/system.h` now declares `Sys_PrintLocationNote`
  on every platform (the unix no-op macro is gone); `src/windows/system.c`
  untouched. With `-Dsystem-wide=false` upstream's `~` homedir default is
  empty, so a build anywhere else behaves as upstream.
- **Gamepad audit (`-Dsdl2=enabled`, engine's SDL2 reused):**
  `SDLG_Init` only `SDL_InitSubSystem(JOYSTICK|GAMECONTROLLER)`,
  `PAD_Shutdown` only `SDL_QuitSubSystem` (`SDL_Quit` is `#if !USE_SDL`);
  the engine's SDL video driver quits only its own subsystem. `pad_enable 0`
  logs "SDL joystick subsystems not started" (0.0.0.24 behaviour kept).
  Backslash paths: none in our code (`split_snd.c` only strips a leading
  `/` or `\` from sound names).
- **Evidence (`work/l1/`):** `l1-bots.log` + `l1-bots-urbanjungle.jpg`:
  `scripts/run.sh --map urbanjungle -- +set am 1 +set am_botcount 4`, 75 s
  (teamjungle is not in Steam's install; its paks have urbanjungle, wizs,
  terminal, ...), Steam's `.so` from libdir, all 13 `G_CheckForExtension`
  lines, 4 bots connected, kills in the log, screenshot in game. Wayland
  driver: the 1280x720 window is a 2560x1440 buffer (desktop scale 2, the
  screenshot size). `l1-steamtest.log` + `l1-steamtest-root.jpg`: the exe
  copied as `<AQtion install>/q2pro-splitscreen-test`, `--self-locate
  --homedir work/steamtest-home`: `split: running from <AQtion install>
  (AQtion root), basedir <AQtion install>, homedir <repo>/work/steamtest-home,
  game library from libdir <AQtion install>, working directory <AQtion
  install>`, `Loaded game library from <AQtion install>/action/gamex86_64.so`,
  bots, 65 s, screenshot (health 31: the bots fight). Test exe deleted.
  `l1-subfolder-x11.log` + `.jpg`: a fake install in `work/l1/fake/`
  (symlinked `baseaq/`, `action/`, exe + our `gamex86_64.so` in
  `splitscreen-test/`): `split: ... (subfolder of AQtion) ... homedir
  <...>/splitscreen-test/home, game library <...>/splitscreen-test/gamex86_64.so,
  working directory <fake AQtion>`, our library loaded (Q2PRO API 400),
  `wizs` with bots on `vid_driver x11`. `build-clean.log`: the clean build.
- **Install check:** `action/terrain/*` (21 files) hashed before
  (`work/l1/terrain-before.sha256`) and unchanged after every run. Steam's
  library wrote `<AQtion install>/items.txt` (66 bytes, "Rejected item:
  worldspawn...") at shutdown of the root-placed run; it was not there
  before and was deleted. The subfolder test's symlinked `action/` let our
  dev library write `action/bots/prev_map_bots.txt` into the install: deleted
  and the folder mtime restored; a full listing (names, sizes, mtimes) of the
  install now matches the one taken before the first run. No `splitscreen/`
  folder was created (homedir was given). Lesson in Next steps.
- **No `q2config.cfg` existed** in `work/aqhome` on this box (fresh); the runs
  created `work/aqhome/action/config.cfg` (Linux settings, windowed).

**Zoom wraps at full zoom; hold-to-scroll in the Start menu (0.0.0.31),
2026-10-15.** The maintainer's two requests: (1) at max sniper zoom, zoom
again should go back to 1x instead of needing the reset-zoom button; (2)
holding d-pad / stick up / down on the long Server options page should keep
moving, without a single press ever moving several rows.

- **Zoom: no client code needed.** TNG's `Cmd_Lens_f`
  (`src/action/a_cmds.c`; Steam's 2.82 `action\doc\readme.txt` /
  `manual.txt` say the same): plain `lens` (or an argument it does not
  know) is `_ZoomIn(ent, true)` and wraps 6x -> 1x; `lens in` /
  `lens out` stop at 6x / 1x; `lens 1|2|4|6` sets a level; anything but the
  sniper in hand returns at once. The pad default was `lens in`. Changes:
  `padDefaultBinds` LT = `lens` (`src/client/split/in_gamepad.c` ~113);
  Buttons page `smBindActions` gains "Zoom (cycle)" = `lens`, "Zoom in"
  kept (`split_menu.c` ~337); profile upgrade in `split_profile.c`:
  `Prof_Build` writes `binds "2"`, `Prof_Apply` turns an LT bind of exactly
  `lens in` into `lens` when the text has no `binds` line (rev < 2) and
  `CL_ProfileLoad` saves it at once (`prof_upgraded`), so it happens once
  and a later deliberate "Zoom in" stays. Not upgraded: player 1's own pad
  binds in `q2config.cfg` (written there only if changed from the
  defaults; an unchanged P1 gets the new default automatically).
- **Hold-repeat (`in_gamepad.c`):** `padDevice_t` gains
  `repOn / repKey / repStart / repNext / repView`. `Pad_KeyDown` (overlay
  branch) arms it for SMK_UP/DOWN/LEFT/RIGHT after the first step;
  `Pad_OverlayRepeat` (called from `PAD_Frame` per pad) steps
  `CL_SplitMenuKey` again at `PAD_REPEAT_DELAY` 400 ms, then every
  `PAD_REPEAT_RATE` 120 ms, `PAD_REPEAT_FAST` 60 ms after
  `PAD_REPEAT_FAST_AFTER` 1500 ms (one step per frame at most, no
  catch-up). It stops on release (`Pad_KeyUp` -> `Pad_RepeatReleased`), on
  any other press, when the overlay closes, when the page / keyboard
  changes (`CL_SplitMenuView`, new in `split_menu.c`: top page pointer ^
  depth, 1 = keyboard), when `menuDown` is cleared (the menu took the pad,
  `PAD_ReleasePlayer`), or when the pad changes hands. D-pad and stick in
  the same direction: the second press adds no step, and it carries the
  repeat on if the first is let go. Only the overlay: game binds, the
  engine (AQtion) menus and Action's layouts are untouched. No cvars.
  Debug prints: `pad_debug 1` now logs `up (held N ms)`, `repeat (held N
  ms)` and `P<N> fov A -> B` (server fov changes); `developer 1` logs
  `P<N> menu: <title> row R of N` and keyboard row / column moves
  (`Com_Printf` gated on `developer`, as `Com_DPrintf` is compiled out of
  release builds).
- **Evidence** (offscreen `run.ps1 -Offscreen -AllowPrimary -GameLib steam
  -WorkDir work\scratch -Wait`, `pad_enable 0`, virtual pad 1 = P1, two
  players, deathmatch, `wp_flags 33` = Sniper only; the game window was
  foreground in 0 of every sample). `work/zoom31-b.log` (cfg
  `work/zoom31-b-test.cfg`, screenshots `zoom31-b01..b07`):
  - Server options (29 rows): d-pad tap 48 ms -> row 1 -> 2, again -> 3;
    hold 992 ms -> 1 + 5 repeats (400, 520, 640, 760, 880 ms) -> row 9;
    up held 2976 ms: 37 steps (12 at 120 ms, then 60 ms after 1600 ms,
    wrapping 1 -> 29); a 336 ms press: one row; stick (LY 1) tap 48 ms: one
    row; stick hold 992 ms: six rows; d-pad held + stick down 160 ms later:
    no extra step, d-pad repeats, d-pad released at 960 ms -> the stick
    carries on (1000, 1120 ... ms), stops at its release; held then the
    overlay closed (`padmenu 1`) at ~650 ms: repeats stop, reopened while
    still held: none; held then B (page back) at ~640 ms: none after.
  - Left / right: Game type row tap -> 1 change; 800 ms hold -> 1 + 4
    repeats (dm -> tdm -> tp -> ctf -> 3t -> tourney -> dom). On-screen
    keyboard (Profile -> Save as new): down tap one row, right held 992 ms
    -> column 1 -> 6. In game (overlay closed) d-pad left held 992 ms:
    `irvision` once, no repeat.
  - Zoom: P1's pad LT (`pbindlist 1`: PAD_LT "lens") x4: fov 90 -> 45 ->
    20 -> 10 -> 90; `p2 lens` x4 and console `lens` (P1) x4 the same; with
    the MK23 in hand LT / `lens`: no fov change. Profiles: an old-format
    profile (LT `lens in`, no `binds` line) loaded into P2 -> "P2's LT is
    now Zoom (cycle)", saved with `binds "2"` and `pbind PAD_LT "lens"`; a
    profile with `binds "2"` and LT `lens in` keeps `lens in`.
  - Root-placed exe (`<AQtion>\q2pro-splitscreen-test.exe` = the first
    0.0.0.31 zip's exe, `-SelfLocate -HomeDir work\steamtest-home`, "split:
    running from ... (AQtion root)", Steam's `action\gamex86_64.dll`,
    `work/zoom31-c.log`, `zoom31-c01..c03`): tap one row, 1 s hold six
    rows, stick tap one row; LT x4 with markers between presses: 45, 20,
    10, 90 (`zoom31-c02-6x`: P1 in the 6x scope); `p2 lens in` x5 with
    markers: 45, 20, 10, then nothing on the 4th and 5th, `lens 1` -> 90;
    `p2 lens` x4: 45, 20, 10, 90. Test exe removed; Steam's DLL rewrote
    `<AQtion>\items.txt`, restored from `work/zoom31-aqroot-backup/`; root,
    `action\` and `action\terrain\` identical by SHA-256 before / after;
    the steamtest-home q2config restored.
  - Final build: a function was moved after the first package (no
    behaviour change), so the zip was rebuilt and run D (`work/zoom31-d.log`,
    the same script as B) repeated every result above with the final exe.
- **Build:** clean MSVC 2019 x64, no warnings in the split files
  (`work/zoom31-package.log`); `-Dgamepad=disabled` compiles and links
  (`work/zoom31-stub-build.log`, separate `build-stub\`, deleted).
  `scripts\package.ps1` -> `build\aqtion-splitscreen-0.0.0.31.zip` (SHA-256
  `9356ce51...d4c56e09b082e3`), exe SHA-256 `54976fdc...bd8e68e6c`
  (3,491,840 bytes).
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.30
  (`752800b4...6dddb06`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.30.exe`, the 0.0.0.31
  exe copied over `<AQtion>\q2pro-splitscreen.exe` (version resource
  0.0.0.31, SHA-256 `54976fdc...bd8e68e6c`). `work/aqhome/action/q2config.cfg`
  restored byte-identical (`4b0f8926...b79f692c`); the test profiles were
  removed.

**Weapon presets: one per weapon, and they give the weapon (0.0.0.30),
2026-10-14.** The maintainer tested "Sniper only" in 0.0.0.29 (Steam's TNG
2.82 DLL, root placement, real pads): players got a pistol only, with no
ammo.

- **Root cause (reproduced with 0.0.0.29 + Steam's DLL, `work/wpn30-a.log`,
  `wpn30-a01..05`):** the preset wrote `wp_flags 32` (sniper bit only;
  `itm_flags` untouched). (1) Action always spawns a player with the MK23
  in hand and fills its rounds only if the MK23 bit is on; the pistol
  magazines' flag is MK23|dual, so with 32 the pistol had 0 rounds and 0
  magazines. (2) The DLL never gives the only allowed weapon by itself:
  deathmatch adds the weapon named by the `dmweapon` cvar (default "MK23
  Pistol", no `wp_flags` check), teamplay / `dm_choose 1` the player's
  chosen weapon (default MP5 -> MK23 -> knife, a banned one dropped).
  `wp_flags` itself only removes map pickups and menu entries. Bits
  confirmed with the DLL's own `gamesettings` per value: MK23 1, MP5 2, M4
  4, M3 8, handcannon 16, sniper 32, dual 64, knife 128, grenades 256.
  REA's Ghidra bridge again failed its handshake, so the DLL check is
  strings (`dmweapon` default "MK23 Pistol", pickup names, `choose`) plus
  runtime; src/action shows the same code.
- **Fix (`src/client/split/split_menu.c`):** `smWeapSetValues` /
  `smWeapSetLabels` (~1607): All 511, MK23 only 1, Dual pistols only 65,
  MP5 only 3, M4 only 5, M3 only 9, Handcannon only 17, Sniper only 33,
  Knife only 128 (gun presets keep the MK23 bit: a working sidearm; knife
  and grenades banned, so the weapon menu lists only the gun).
  `smStartWeapons` / `SM_StartWeapon` (~1905) derive the spawn weapon from
  any `wp_flags` (also Custom mixes and old saved 32), `SM_WeapPresetFrame`
  (~1948, called first in `CL_SplitMenuFrame` ~2037) sets `dmweapon` (back to
  "MK23 Pistol" at All) and sends `choose <weapon>` on each local player's
  connection once per map / preset after the value has been stable 1 s;
  back to All, players we chose for get `choose MP5/10 Submachinegun` (the
  game's default). Row help (~2268): "Now: at the next spawn" / "Everyone
  spawns with it". `developer 1` logs `weapon preset: p<N> gun <model>
  loaded A reserve R`; `work/wpn30-analyze.py` makes the tables.
- **Evidence** (offscreen, `run.ps1 -Offscreen -AllowPrimary -GameLib steam
  -WorkDir work\scratch -Wait`, `pad_enable 0`, two players, the game
  window foreground in 0 of every sample). Spawn = first frame alive after
  a death; "after fire" = lowest loaded count while fire was held:

  | preset | DM spawn (loaded / reserve) | DM after fire | TP spawn | TP after fire | TP weapon menu |
  |---|---|---|---|---|---|
  | Sniper only | sniper 6 / 10 | 3 | sniper 6 / 10 | 4 | sniper only |
  | MK23 only | MK23 12 / 0 (DLL's DM rule) | 4 | MK23 12 / 1 | 9 | no weapons (item menu) |
  | Dual pistols only | dual 24 / 2 | 8 | dual 24 / 2 | 18 | Akimbo only |
  | MP5 only | MP5 30 / 1 | 0 | MP5 30 / 1 | 19 | MP5 only |
  | M4 only | M4 24 / 1 | 0 | M4 24 / 1 | 12 | M4 only |
  | M3 only | M3 7 / 7 | 3 | M3 7 / 7 | 5 | M3 only |
  | Handcannon only | HC 2 / 12 | 0 | HC 2 / 12 | 0 | HC only |
  | Knife only | knives 10 | slash | knives 10 | slash | knives only |
  | All (control) | MK23 12 / 0 | 4 | MP5 30 / 1 | 15 | all 7 |

  Both players identical in every row (`work/wpn30-b-table.txt`,
  `wpn30-c-table.txt`; screenshots `wpn30-b00..b09-*`, `wpn30-c00..c09-*`).
  Teamplay first round with Sniper only set before the game: sniper 6 / 10
  for both ("Weapon selected: Sniper Rifle"). Row and bans page driven by
  virtual pad 1 (`wpn30-d*`, `wpn30-d-table.txt`): All -> Sniper only on the
  row (`wpn30-d03`), respawn sniper 6 / 10; Weapon bans -> MK23 off = 32,
  row shows "Custom" (`wpn30-d07`), respawn still sniper; row -> All, respawn
  MK23 12 / 0; cycling the row sent `choose` only for the value it stopped
  on. Teamplay Knife only -> All (`wpn30-e*`): knives, then MP5 30 / 1 and
  the full weapon menu. Root-placed exe (`<AQtion>\q2pro-splitscreen-test.exe`
  = the zip's exe, `-SelfLocate -HomeDir work\steamtest-home`, "split:
  running from ... (AQtion root)", Steam's `action\gamex86_64.dll`, Sniper
  only set before Quick Local Match): sniper 6 / 10 at the first spawn for
  P1 and P2, M3 only mid-game 7 / 7, All -> MK23 (`wpn30-g*`,
  `wpn30-g-table.txt`). Test exe removed; the DLL rewrote `<AQtion>\items.txt`,
  restored from `work/wpn30-aqroot-backup/`; root, `action\` and
  `action\terrain\` (40 files) identical by SHA-256 before / after; the
  steamtest-home q2config restored. The maintainer's exe (until the
  install), `splitscreen\`, `action\`, `baseaq\` and DLLs untouched.
- **Build:** clean MSVC 2019 x64 (known warnings only, none in
  split_menu.c; `work/wpn30-build.log`); `-Dgamepad=disabled` compiles and
  links (`work/wpn30-stub-build.log`; it reconfigured `build\` itself, which
  was then rebuilt with the normal options before packaging).
  `scripts\package.ps1` -> `build\aqtion-splitscreen-0.0.0.30.zip` (SHA-256
  `ba864125...b6840b1f`), exe SHA-256 `752800b4...6dddb06`.
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.29
  (`76764240...9d8cccd`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.29.exe`, the 0.0.0.30
  exe from the zip copied over `<AQtion>\q2pro-splitscreen.exe` (version
  resource 0.0.0.30, 3,489,280 bytes, SHA-256 `752800b4...6dddb06`).
  `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`).

**Infinite ammo = infinite reserve, reloads stay (0.0.0.29), 2026-10-14.**
The maintainer tested 0.0.0.28: it worked, but players must still reload;
only the reserve should be infinite. 0.0.0.28 sent `give ammo` after every
shot, which sets every gun's loaded rounds to the maximum.

- **Lever (Steam's TNG 2.82 DLL):** `give <pickup name>` of an ammo item
  adds one pickup's worth to the reserve (`inventory += quantity`, uncapped,
  no count argument) and leaves the loaded rounds alone; same code as
  src/action `Cmd_Give_f` (IT_AMMO branch). Names from the DLL's item
  table: `Pistol Magazine` (+1; MK23 and dual pistols), `MP5 Magazine` (+1),
  `M4 Magazine` (+1), `12 Gauge Shells` (+7; M3 and handcannon), `AP
  Sniper Ammo` (+10). Weapon pickups, `give ammo` and dmflags 8192 all
  refill the loaded rounds, so they are out. REA's Ghidra bridge failed its
  handshake twice on this box, so the DLL check is strings + runtime
  (`work/inf29-c.log`: row On with a dry pistol, `loaded 0 reserve 0` ->
  `loaded 0 reserve 1`, loaded count untouched).
- **Code** (`src/client/split/split_menu.c`): `smReserves[]` (~1810, clip
  icon -> item, quantity), `SM_ReserveKind` (~1851, from
  `STAT_CLIP_ICON`), `SM_InfAmmoSend` (~1866), `CL_SplitMenuFrame` (~1876):
  `give ammo` once per life at spawn (all guns are loaded then; it caps the
  reserve at the carry limit, bandolier / mode included), the reserve it
  leaves is "full" per kind; afterwards `STAT_CLIP` below full -> `give
  <item>` ceil(missing / quantity) times at once (dual pistols reload two
  magazines), one top-up outstanding per player (500 ms). Switched on
  mid-life: full = the reserve then (at least 1). Knives / grenades as in
  0.0.0.28; the periodic `give ammo` is gone. Help (~2093): "Now: endless
  spares, reloads; cheats on" / "Endless spare mags (turns cheats on)"
  (the requested longer sentence does not fit the ~40-character footer).
  `developer 1` logs every count change (`infinite ammo: p<N> loaded A
  reserve R (<item>) t=<ms>`) and each top-up; `work/inf29-analyze.py`
  turns a log into the table below.
- **Evidence** (offscreen, `run.ps1 -Offscreen -AllowPrimary -GameLib steam
  -WorkDir work\scratch -Wait`, `pad_enable 0`, virtual pad 1 for the menus,
  two players, fire held 900 frames per magazine; the game window was
  foreground in 0 of every sample). Per weapon, both players identical
  (`work/inf29-b.log`, `work/inf29-b-table.txt`, `work/inf29b-01..16.jpg`):

  | weapon | loaded count hits 0, gun stops | reload works | reserve (full) | longest below full-1 |
  |---|---|---|---|---|
  | MK23 | 12 -> 0, stays 0 | 0 -> 12 | 2, min 1 | 0 ms |
  | dual MK23 | 24 -> 0 | 0 -> 24 | 2, min 0 | 103 ms (reload takes 2) |
  | MP5 | 30 -> 0 | 0 -> 30 | 2, min 1 | 0 ms |
  | M4 | 24 -> 0 | 0 -> 24 | 1, min 0 | 0 ms |
  | M3 | 7 -> 0 | 0 -> 7 (one shell per reload) | 14, 13-20 | 0 ms |
  | sniper | 6 -> 0 | 0 -> 6 (one round per reload) | 20, 19-29 | 0 ms |
  | handcannon | 2 -> 0 | 0 -> 2 | 14, 12-19 | 99 ms (reload takes 2) |

  Shells / sniper rounds overshoot the limit by up to 6 / 9 (one `give` is
  7 / 10). Spawn top-up logged for both players ("spawn, give ammo").
  - Row Off control and On mid-game (`work/inf29-c.log`,
    `inf29-c-table.txt`, `inf29c-01..08`): Off: pistol 0 / 0, nothing sent,
    `cheats "0"`; On from the in-game page (help line fits): "cheats 1
    (now)", one magazine given at once, loaded stays 0 until reload, then
    3 more empty / reload cycles MK23 and MP5, reserve back within one
    frame; after `kill` + respawn: "spawn, give ammo", M4 empty / reload
    twice, reserve 1 kept.
  - Row Off live (`inf29-b.log` end, `inf29b-15/16`): `cheats "0"`, MP5 runs
    out (0 / 0, "Out of ammo").
  - Root-placed exe (`<AQtion>\q2pro-splitscreen-test.exe` = the zip's
    exe, `-SelfLocate -HomeDir work\steamtest-home`; row saved On in that
    homedir's q2config, game type dm): "split: running from ... (AQtion
    root)", Steam's `action\gamex86_64.dll`, Quick Local Match (nightwalk):
    "cheats 1 (now)", spawn top-up for P1 and P2, MK23 / MP5 / M3 empty and
    reload with the reserve kept full (`work/inf29-g.log`,
    `inf29-g-table.txt`, `inf29g-01..04`). Test exe removed; the DLL
    rewrote `<AQtion>\items.txt`, restored from
    `work/inf29-aqroot-backup/`; root, `action\` and `action\terrain\`
    (40 files) identical by SHA-256 before / after; the steamtest-home
    q2config restored. The maintainer's exe, `splitscreen\`, `action\`,
    `baseaq\` and DLLs untouched during the tests.
- **Build:** clean MSVC 2019 x64 (20 known warnings, none in
  split_menu.c; `work/inf29-build.log`, `work/inf29-package.log`);
  `-Dgamepad=disabled` compiles and links (`work/inf29-stub-build.log`,
  separate `build-stub\`, deleted). `scripts\package.ps1` ->
  `build\aqtion-splitscreen-0.0.0.29.zip` (SHA-256 `69afe25d...632d026a`),
  exe SHA-256 `76764240...9d8cccd`.
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.28
  (`c0ea4769...0b5aa64e`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.28.exe`, the 0.0.0.29
  exe from the zip copied over `<AQtion>\q2pro-splitscreen.exe` (version
  resource 0.0.0.29, 3,487,744 bytes, SHA-256 `76764240...9d8cccd`).
  `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`).

**Infinite ammo applies live in every path (0.0.0.28), 2026-10-14.** The
maintainer (real pads, 0.0.0.27 in his AQtion root, Steam's TNG 2.82 DLL,
row saved On in his config): "infinite ammo still doesn't work".

- **Causes (reproduced with the 0.0.0.27 build, Steam DLL, offscreen):**
  1. *Row turned On in a running game* (path a): `cheats` is `CVAR_LATCH`;
     the row wrote it `FROM_CONSOLE`, which while a server runs only latches
     ("cheats will be changed for next game"), so the live value stayed 0,
     the sender (gated on the live value) sent nothing and the gun emptied
     (`work/inf28-repro.log`, `work/inf28r-03-after-12.jpg`: 0 rounds); a
     hand `give ammo` got Steam's "You must run the server with '+set
     cheats 1' to enable this command." Only Restart map applied it, and
     the help line saying so was cut off in the footer.
  2. *Game started from AQtion's own menus or the console* (`map`): the row
     only wrote `cheats` from `splitstartgame` / Quick Local Match / Restart
     / Next map / reset / set load, so such a game ran with `cheats 0` and no
     refill at all (`work/inf28-repro2.log`, `cheats "0"`). Loading a saved
     set in a running game (path d) latched the same way as (1).
  3. *Refill through the command buffer:* `p<N> cmd give ammo` went through
     `Cbuf_InsertText`; a pending `wait` (a cfg or an alias with waits)
     freezes the buffer, so refills arrived in bursts after it (first test
     run, `work/inf28-a1-cbuf-stall.log`: the pistol went 12 -> 4 while
     fire was held). And a refill every 500 ms let a full-auto gun drop by
     several rounds in between.
  Paths that did work in 0.0.0.27: Quick Local Match / Start with the row
  already On (b, c; the set happens before the server runs), players joining
  later (e).
- **Steam's DLL `give`** (strings + behaviour; same as src/action
  `Cmd_Give_f`): `ClientCommand` refuses every `CMDF_CHEAT` command unless
  `sv_cheats->value` (the engine's live `cheats` cvar_t, shared through
  `gi.cvar`); spectators get "This command can't be used by spectators.";
  `give ammo` sets every gun's rounds (`mk23/dual/mp5/m4/shot/sniper/cannon
  _rds`) to its maximum and adds `IT_AMMO` clips to the carry limit, but
  knives and grenades are weapons: `give Combat Knife` / `give M26
  Fragmentation Grenade` add one each through the pickup code (capped).
- **Code** (`src/client/split/split_menu.c`): `SM_SetCheatsNow` (~1757)
  writes `cheats` with `FROM_CODE` (no latch; `Cvar_FixCheats` after 0),
  used by `SM_ApplyInfAmmo` (~1772; prints "... cheats 1 (now)");
  `CL_SplitMenuFrame` (~1824) puts `cheats 1` back whenever the row is on,
  we host and the live value is 0 (any entry point, from the first frame),
  then per alive player with a gun model (`gunindex`, so teamplay
  spectators are skipped) refills after any drop of `STAT_AMMO` /
  `STAT_CLIP` with the same gun (one per 100 ms, missed drops counted), plus
  `give ammo` every second for the guns not in hand; knife / grenade in hand
  (from the `STAT_AMMO_ICON` image name) get their own `give`. Sent with
  `CL_ClientCommand` in the player's context, not the command buffer.
  `developer 1` logs each refill. Help line (~2003): hosting "Now: guns
  never run dry; cheats on", else "Guns never run dry (turns cheats on)".
- **Evidence after the fix** (offscreen, `run.ps1 -Offscreen -AllowPrimary
  -GameLib steam -WorkDir work\scratch -Wait`, `pad_enable 0`, virtual pad 1
  on the menus, sound off; the game window was foreground in 0 of every
  sample). Logs / cfgs / screenshots `work/inf28-*`, `work/inf28?-*.jpg`:
  - (a) row Off, 12 presses: nothing sent (`cheats "0"`); row On from the
    in-game page: "cheats 1 (now)", `cheats "1"`, refilled at once (0 ->
    12); 24 more presses, reload, 12 more: never below 11 (P1, P2); row Off
    live: `cheats "0"`, 12 presses empty the pistol again
    (`work/inf28-a.log`, `inf28a-01..16`).
  - every default weapon (`allweapon 1`, fire held 200 frames twice,
    reload): MK23 >= 11/12, dual >= 22/24, MP5 >= 28/30, M4 >= 22/24, M3
    >= 6/7, sniper >= 5/6, handcannon 0-2 (both barrels, back to 2), thrown
    knives 9 -> 10 each (`inf28-a.log`); grenades 1 -> 2 per throw
    (`work/inf28-gren.log`; a third throw bounced back and killed the
    thrower, as in any game).
  - (b) row On on the main-menu Server options page ("cheats 1 (now)" before
    any server), Start: `cheats "1"`, P1 + P2 stay at 11-12 through fire and
    reload (`inf28-b.log`, `inf28b-01/02`).
  - (c) row saved On, fresh start, Quick Local Match: see (g).
  - (d) game started with the row off (`cheats "0"`, 6 / 0 after six shots,
    nothing sent), then Saved settings -> load a set with it On: "cheats 1
    (now)", refilled to 12 / 2 at once, stays full through fire and reload
    (`inf28-c.log`, `inf28c-01..03`).
  - (e) a third player joining after the row was on: refilled from its first
    frame, 11-12 through held fire (`inf28-a.log`, `inf28a-05`).
  - (f) teamplay, round running, P1 with Action's weapon menu (`inven`) open
    and P2 firing the MP5: 28-30 all the time, no message for the spectating
    phase (`inf28-c.log`, `inf28c-04..07`).
  - native start: console `map wizs force` with the row on and `cheats 0`:
    "cheats 1 (now)" in the first frames, pistol 11-12 (`inf28-b.log`,
    `inf28b-05`).
  - (g) the test-named exe in the AQtion root (`-SelfLocate -HomeDir
    work\steamtest-home`, cfg as that homedir's autoexec, removed
    afterwards): "split: running from ... (AQtion root)", Steam's
    `action\gamex86_64.dll`; row saved On + Quick Local Match: "cheats 1
    (now)" before the map, 11-12 through fire and reload; row Off in game
    -> `cheats "0"`, 6 / 2 after six shots; row On again -> refilled
    immediately (6 -> 12) (`work/inf28-g.log`, `inf28g-01..05`).
    `<AQtion>\items.txt` was rewritten by the DLL and restored from
    `work/inf28-aqroot-backup/` (all root, `action\` and `action\terrain\`
    files compared by SHA-256 before / after: identical); the maintainer's
    exe, `splitscreen\`, `action\`, `baseaq\` and DLLs untouched.
- **Build:** clean MSVC 2019 x64 (known warnings only, none in
  split_menu.c; `work/inf28-build.log`); `-Dgamepad=disabled` compiles and
  links (`work/inf28-stub-build.log`, separate `build-stub\`, deleted).
  `scripts\package.ps1` (clean) -> `build\aqtion-splitscreen-0.0.0.28.zip`
  (SHA-256 `ed251226...ff11e3fde`), exe SHA-256 `c0ea4769...0b5aa64e`.
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.27
  (`f1743984...6c1f8`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.27.exe`, the 0.0.0.28
  exe from the zip copied over `<AQtion>\q2pro-splitscreen.exe` (version
  resource 0.0.0.28, hash matches). `work/aqhome/action/q2config.cfg`
  restored byte-identical (`4b0f8926...b79f692c`).

**Humor sounds off switch in Server options (0.0.0.27), 2026-10-12.** The
maintainer hears farts, Arnold imitations, "do I smell a noobie" and laughs
about every 20 s in local games hosted by 0.0.0.26 with Steam's DLL.

- **The sounds:** `<AQtion>\baseaq\pak0.pkz` (no loose `sound\` dirs in the
  install). `sound/user/*.wav`: 78 joke clips (`baseaq\sndlist.ini` lists
  77 of them: fart1-7, laugh, lol, rofl, newbie, terminat, killme, ...).
  `sound/insane/insane1-11.wav`: Quake 2's insane-marine screams, replaced
  by AQtion (2022) with joke clips; same audio as user/killme (insane2),
  lol (3), muu1 (4), newbie (5), shootorwhat (6), fart4 (7); 1, 8-11 own.
- **Who plays them:** Steam's `action\gamex86_64.dll` ("TNG 2.82
  2023-10-25", has symbols; Ghidra): `ClientBeginServerFrame` plays
  `insane/insane%i.wav` (rand 1-11) on CHAN_VOICE, ATTN_NORM for every
  living player idle (no input) for a multiple of `ppl_idletime` seconds
  (Steam default 15, confirmed by `cvarlist`; src/action has the same
  code). An idle split-screen player, or one sitting in a menu, sets one off
  every 15 s. `user/` clips: only `Cmd_Voice_f` (`voice <file>`,
  `use_voice` 1) in Steam's DLL; `Add_Frag` has no voice code and there is
  no `bot_randvoice` (that is only in our src/action). LTK bot chat is text
  only. Engine and configs (`triggers.cfg`, `default.cfg`, bot cfgs) play
  none. The 0.0.0.23 claim ("Steam's DLL has none of those sounds") missed
  the `insane/` path; corrected in the design doc and CHANGELOG.
- **Evidence** (offscreen, `run.ps1 -Offscreen -AllowPrimary -GameLib steam
  -WorkDir work\scratch -Wait`, `pad_enable 0`, sound off, deathmatch wizs,
  2 bots, P1 + P2 (`addplayer`) idle, `developer 1` + new `sv_showsounds 1`
  logging): `work/humor27-run2.log` (cfg `work/humor27-test.cfg`, demos
  `work/humor27-on.dm2` / `-off.dm2`, screenshots
  `work/humor27-0[1-3]-*.jpg`): A, row On, 3 min: 17 insane sounds (each
  idle player every 15.0 s, e.g. entity 1 at 28.1, 43.1, 58.1 s), all
  played by both local players. B, the row switched Off on the Server
  options page by virtual pad (`P1 menu: humor sounds off`, `ppl_idletime
  "0"`), 3 min: the server sends none. C, `set ppl_idletime 15` by hand with
  the row Off, 2 min: 13 sent, all 26 client starts "blocked". D, `play
  user/fart1` / `play insane/insane5` blocked with the row off, played on.
  F, row On: `ppl_idletime` back to 15. `work/humor27-run3.log` (cfg
  `work/humor27-test3.cfg`, `work/humor27-04-help-line.jpg`): row off,
  hand `ppl_idletime 15`, `splitstartgame` -> 0; 1 min idle: nothing; hand
  15 again, Restart map from the page -> 0. `work/humor27-run1.log`: the
  first run (P1 dead most of the time, one insane5 heard). The game window
  was never foreground (0 of 3440 / 4553 / 845 samples). Runs 2-3 used a
  build whose version resource still said 0.0.0.26 (reconfigure did not
  pick up VERSION; the packaged build is `-Clean`).
- **Code:** `split_snd.c` `CL_SplitSoundAllowed` (hooked in parse.c
  `svc_sound` and main.c `play`), `cl_splitHumorSounds` (archived);
  `split_menu.c` row "Humor sounds" (all DLLs, help "Now: farts, laughs
  from idle players"), `SM_ApplyHumor` (`ppl_idletime` / `bot_randvoice`
  0 when off, 15 / 5 where 0 when on; not on another machine's server),
  in `smSrvDefaults` (saved sets, Reset); the "Joke sounds" row and its
  `bot_randvoice` special cases are gone. `server/game.c`: `sv_showsounds`
  1 (humor) / 2 (all) logs each game sound with its entity.
- **Build:** clean MSVC 2019 x64 (20 known warnings, none new;
  `work/humor27-build.log`); `-Dgamepad=disabled` compiles and links
  (`work/humor27-stub-build.log`; it reconfigured `build\` itself, which
  was then rebuilt `-Clean`). `scripts\package.ps1 -NoBuild` ->
  `build\aqtion-splitscreen-0.0.0.27.zip` (SHA-256
  `919fa067...3c990b5`), exe SHA-256 `f1743984...6c1f8`.
- **Install (authorised):** no `q2pro*.exe` running; 0.0.0.26
  (`b95674be...5f456e313c`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.26.exe`, the 0.0.0.27
  exe from the zip copied over `<AQtion>\q2pro-splitscreen.exe` (version
  0.0.0.27, hash matches). Nothing else in the AQtion folder touched
  (tests used a scratch working directory; `items.txt` unchanged since
  2026-10-10). `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`).

**Infinite ammo no longer clears a hand-set `cheats`; 0.0.0.26 installed
for the maintainer, 2026-10-12.**

- **Fix** (`split_menu.c` `SM_ApplyInfAmmo`, the Sonnet scan's finding):
  it wrote `cheats 0` on every map load / start / reset / set load with the
  row off. Now a static `smInfAmmoCheats` remembers that the row wrote
  `cheats 1`; with the row on it writes `cheats 1`, with the row off it
  writes `cheats 0` only if `smInfAmmoCheats` is set (then clears it), else
  leaves `cheats` alone. Each write prints `P1 menu: infinite ammo on,
  cheats 1` / `... off, cheats 0`.
- **Smoke run** (offscreen, `run.ps1 -Offscreen -AllowPrimary -GameLib
  steam -WorkDir work\scratch -Wait`, `pad_enable 0`, virtual pad 1 driving
  P1's Server options page; no virtual display attached, fallback corner;
  the game window was foreground in 0 of 1733 samples). Log
  `work/inf26-run.log`, cfg `work/inf26-test.cfg`, screenshots
  `work/inf26-0[1-5]-*.jpg`. Steps and `cheats` afterwards: A hand `set
  cheats 1` with the row off, `splitstartgame wizs` -> 1, no write; B
  Restart map (row off) -> 1, no write; C hand `set cheats 0` + Restart ->
  0; D row On -> one "on, cheats 1" write (latched), Restart map -> "on,
  cheats 1" again (same value, no change) -> 1; E row Off -> one "off,
  cheats 0" write, Restart -> 0; F Restart again with the row off -> 0, no
  write. `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`); the install was not written (scratch cwd).
- **Build:** MSVC 2019 x64 (19 known warnings, none in split_menu.c;
  `work/inf26-build.log`), `-Dgamepad=disabled` compiles
  (`work/inf26-stub-build.log`, dir deleted); `scripts\package.ps1
  -NoBuild` -> `build\aqtion-splitscreen-0.0.0.26.zip` (`build\` itself
  could not be deleted by `-Clean`, a handle was open on the empty
  directory, so the build configured into it fresh). Exe SHA-256
  `b95674be...5f456e313c`.
- **Install (authorised by the maintainer):** no `q2pro*.exe` was running;
  `<AQtion>\q2pro-splitscreen.exe` 0.0.0.24 (2026-10-09, SHA-256
  `724415f5...6726f781`) copied to
  `work\maintainer-exe-backup\q2pro-splitscreen-0.0.0.24.exe`, then the
  0.0.0.26 exe from the zip copied over it (version resource 0.0.0.26, hash
  matches the zip). Nothing else in the AQtion folder was touched.
  `work\maintainer-exe-backup\README.txt` has both hashes and the restore
  step.

**Per-player menu, crosshair and text scale; Profile: Guest; infinite ammo
(0.0.0.25), 2026-10-12.** From the maintainer's pad pass of 0.0.0.24. All
runs offscreen (`run.ps1 -Offscreen -AllowPrimary`, `pad_enable 0`, virtual
pads only, Steam game DLL, scratch working directory); NucleusCoop was
running. This time the agent shell's desktop was a disconnected 1024x768
session (no foreground window at all, `WinDisc`), so the window was never
visible to anyone; 0 of ~4000 focus samples had the game in front.

- **1 Profile wording** (`split_menu.c` `SM_ProfileLabel`): "Profile: Guest"
  / "Profile: <name>" on the pause page and the main-menu Splitscreen page.
  Evidence `work/scale/scale-1080-07-4p-pause-profile-guest.jpg`.
- **2 Infinite ammo** (`split_menu.c` `SM_ApplyInfAmmo`, `CL_SplitMenuFrame`,
  hooked from `split.c` `CL_SplitFrame`): Steam's DLL string scan (no
  `infinite`/`unlimited` cvar; 8192 = "infinite ammo" only in `dmflags`
  text) and `src/action`: 8192 makes weapon pickups give 1000 rounds and
  deletes ammo boxes, reloads still use clips unconditionally;
  `allweapon`/`allitem`/`item_kit_mode` only change spawn equipment. What
  works is the game's `give ammo` (all magazines full, clips to the carry
  limit) with `cheats 1`: probe on Steam's DLL, mag 4 -> 12, clips 0 -> 2.
  The row now sets `cl_splitInfiniteAmmo` + `cheats` (latched), clears a
  leftover 8192 bit, and each live local player sends `give ammo` every
  500 ms. Run with 2 players, 60 fire presses + reload:
  `work/scale/ammo-on-0[1-4]*.jpg` 12 rounds / 2 clips each at the end;
  control `ammo-off-*`: 0 / 0 and "Out of ammo" (`ammo-off.log`).
- **3 AQtion menu size after a split** (`ui.c` `UI_MenuSizeScale`,
  `inc/client/client.h`): **root cause** ui.c includes the public
  `inc/client/client.h`, which had no prototype for `CL_SplitUIScale`, so
  MSVC (C4013 is off at our warning level) read its float return as an int:
  the cell's base scale was garbage, and the menu size came out random
  (mostly 1x) once the screen split; full screen does not call it. Now
  declared there (with the new UI functions). The size is relative to the
  window (size x window scale), capped by what fits the cell; a menu of
  items is fitted to its own height (the 240 floor stays for self-sizing
  menus). Measured (temporary debug print, removed): size 2 at 1080p = 4x
  full screen, 3.07x in a 1920x540 cell, 2.4x in a 960x540 cell. Evidence
  `scale-1080-01` (full screen), `-02` (P1's cell, 2 players), `-03` (P2's
  cell), `-08/-09/-10` (4 players: P1 1.5, P2 2, P4 3, all limited by the
  960-wide cell), and the same at 3840x2160 (`scale-4k-*`).
- **4 Crosshair per player** (`screen.c` `SCR_CrosshairPic`,
  `SCR_CrosshairSize`; rows in `SM_BuildSettings`): picture `crosshair`,
  size `ch_size` (ours, default 3, times upstream's `ch_scale`, so the
  sniper scope keeps its size), colour as before; P1-only rows removed.
  Evidence `scale-1080-04` / `scale-4k-04`: P1 white #1 1x, P2 red #3 2x,
  P3 green #5 3x, P4 yellow #7 4x.
- **5 Both menu sizes per player** (`cl_splitMenuSize`, `cl_splitMenuScale`
  with `p<N>_`): the engine UI now belongs to the player whose context
  opened it (`CL_SplitUIOpened`, `CL_SplitUIOwner`; that player's cell,
  size and pad, `in_gamepad.c` `Pad_ExtraInUI`). Overlay sizes evidence
  `scale-1080-06-4p-settings-pages.jpg` (0.75x / 1x / 1.5x / 2x).
- **6 Text size** (`screen.c` `SCR_TextScale`, chat HUD, centre prints,
  `SCR_SplitNotifyCell` for P1's notify lines): `cl_splitTextSize` default
  2x, per player; never narrower than 416 virtual pixels (52 columns, so
  Action's 51-column MOTD fits; in a 960-wide 1080p cell 2.5x and 3x both
  hit that limit, and in a 1920-wide 4K cell too). Evidence `scale-*-04`
  (chat / kill lines) and `-05` (MOTD centre prints) at 1x / 2x / 2.5x / 3x.
- **Test window:** `src/windows/client.c` `WM_GETMINMAXINFO` lets a
  `vid_noactivate` window be larger than the desktop (1920x1080 / 3840x2160
  on the 1024x768 session).
- **Crashes found and fixed during the round:** a temporary debug print
  read `ui_debug` before `UI_Init` (only in the debug build of this round,
  never committed); found with the crash dialog's report (session 0, so
  the dialog was invisible: a "hang" in `run.ps1 -Wait` can be that dialog;
  look for `#32770 AQtion Unhandled Exception` windows of the pid).
- Build: MSVC 2019 clean (`work/scale-build.log`); `-Dgamepad=disabled`
  compiles (`work/scale-stub-build.log`, dir deleted). Install untouched
  (`items.txt` SHA-256 unchanged, the DLL wrote into the scratch cwd).
  `work/aqhome/action/q2config.cfg` restored byte-identical
  (`4b0f8926...b79f692c`). Test cfgs: `work/aqhome/action/scale-1080.cfg`,
  `scale-4k.cfg`, `ammo-on.cfg`, `ammo-off.cfg`. An empty `build-dbg\`
  directory (from the debug build) could not be removed while a handle was
  open; delete it if it is still there.

**Offscreen no-activate test window; 0.0.0.23 menu checks (0.0.0.24),
2026-10-09.** Ran while the maintainer played Minecraft (Nucleus Co-op,
pads only) on the only display.

- **Displays:** no virtual display is attached to the desktop. Outputs that
  exist: RTX 5090 4 (only `DISPLAY1` attached, primary, 3840x2160), AMD iGPU
  5, SudoMaker 10, Parsec 16, Sunshine 12, all detached. Display settings
  were not touched. As the maintainer allowed, every run used the fallback:
  a 1280x720 `vid_noactivate` window at the primary display's bottom-right
  corner (`+2544+1292`), bottom of the z-order.
- **Focus:** `run.ps1 -Wait` sampled `GetForegroundWindow` every 100 ms in
  all 11 runs (about 3400 samples): the game window was **never** the
  foreground window, its style had `WS_EX_NOACTIVATE`, and the foreground
  stayed on Minecraft (`javaw`) except once when it moved to Explorer during
  the sub-placement run (not our process; most likely the maintainer).
  No sound device (`s_enable 0`); pads: every log has `pad: pad_enable 0:
  SDL joystick subsystems not started, no pad device opened` and only
  `Virtual pad N` devices (the 0.0.0.22 attempt on 2026-10-08 opened the
  maintainer's three 8BitDo pads, `work/aqhome/action/logs/cwd22-repo.log`).
  Frame rate while never focused: 120 fps in game (`cl_splitperf`,
  `work/aqhome/action/logs/offscreen-smoke.log`), screenshots render while
  covered.
- **Engine (`src/windows/client.c`, `client.h`):** `vid_noactivate` (see the
  doc paragraph). Default 0 = upstream code paths unchanged.
- **Pads (`in_gamepad.c`):** `SDLG_Init` only when `pad_enable` is set (at
  init, or later from `PAD_Frame`); before, `pad_enable 0` still started
  SDL's XInput / raw input / HIDAPI / WGI enumeration.
- **Checklist (0.0.0.23 menus, Steam DLL, repo exe, scratch cwd), evidence
  `work/aqhome/action/screenshots/menus-*.jpg` and `logs/menus-a.log`
  (final full rerun), `menus-a2.log`, `menus-b.log`:**
  - [x] 01 main menu 2x, 02 Video page fits (smaller), 03 1x = upstream.
  - [x] 04 Start on a free pad at the main menu -> Splitscreen page, Quick
    Local Match first ("player 1's pad now").
  - [x] 05 Server options at the main menu. **Bug found and fixed:** "Bot
    skill" was blank (AQtion's own `q2pro.menu` creates `ltk_skill` empty);
    `SM_GameVar` now gives an empty, never-registered game cvar the game
    default (`menus-05b-...botskill.jpg`, `menus-a2.log`).
  - [x] Teamplay, `am 2`, `am_botcount 2`, maxclients 10.
  - [x] 06 P2 pause (Team, Weapons, Scoreboard, Radio, Settings, Profile...,
    Leave game), 07 Team page, P2 joined RED, 08 Weapons = Action's weapon
    menu. (First run failed only because the script's first P2 press took
    over P2 for the pad; `menus-a.cfg` now claims the pad first.)
  - [x] 09/10 Buttons A Jump -> Crouch (`pbind 2 PAD_A` = +movedown); 11
    Controls.
  - [x] 12 P1 pause; 13 Server options hosting; 13b set "q" saved (file
    deleted); timelimit 5; 14 DM "changes waiting"; restart -> teamplay 0,
    P2 still in; 15 P2 pause without Team / Weapons; 16/17 dm_choose 1 +
    restart -> Weapons back.
  - [x] 18 "Playing as: Guest"; 19 P1 settings (HUD opacity, crosshair
    colour, AQtion / Splitscreen menu size); 20 engine game menu in P1's
    cell; 22 main-menu hint line; 23 quick match on a random map
    (adriatic, then junglecity).
  - [x] menus-b (our DLL): 21 "Joke sounds" row. With sound off the proof is
    the demos' sound configstrings: JOKE-ON 7 `user/*.wav`, JOKE-OFF none
    (`work/aqhome/action/demos/menus-joke-{on,off}.dm2`).
  - [ ] Keyboard (F1, arrows / Enter / Escape, typing): **not testable**
    in a never-focused window (no keyboard input); maintainer, Next steps 2.
  - [x] 0.0.0.22 strings spinner: test row stays empty ("???"), nextserver
    -> adriatic (`logs/cwd22-menu.log`, `cwd-03-repo-strings-menu.jpg`).
  - [x] 0.0.0.22 first-press cases T1-T5 (`logs/cwd22-repo.log`, 2026-10-09
    part).
  - [x] Native bot page on terminal, root and sub placement, test exe
    `q2pro-splitscreen-test.exe` (root: `<AQtion>\`, sub:
    `<AQtion>\splitscreen-agenttest\`, both removed), homedir
    `work/steamtest-home`: `split:` line right for both, AQtion's "Play"
    starts a bot game on terminal with named bots (Igor[ROCK] etc.)
    (`work/steamtest-home/action/logs/menus-cwd22-{root,sub}.log`,
    screenshots there). **Bug found and fixed:** our host page with Bots
    None then got **12 bots** on terminal: `ltk_loadbots` (game default 1)
    loads `action\bots\<map>.cfg`, found since the cwd became the AQtion
    root (0.0.0.22). `SM_ApplyBots` now sets `ltk_loadbots 0` with no bots
    (sub run: no bots). Script note: a menu item's command is queued behind
    the running cfg, so part 2 now runs from `cl_beginmapcmd`.
  - The Steam DLL rewrote `<AQtion>\items.txt` and `action\terrain\terminal.ltk`
    (same content); both were restored from a backup taken before the runs
    (content and timestamps as before). Nothing else in the install changed.
- **Sonnet review of 7bb17e43, folded in:** (1) `CL_SplitMenuKeyEvent` no
  longer swallows key releases (stuck +attack / arrows); (2)
  `CL_SplitMenuCanOpenHost` false while the page is open or a map loads
  (a second pad's Start took P1 and reset the page); (3) Shift on the
  on-screen keyboard maps digits / symbols (US layout table, as keys.c).
- Build: clean MSVC 2019 (`work/offscreen-build.log`), `-Dgamepad=disabled`
  stub compiles (`work/offscreen-stub-build.log`, dir deleted).
  `work/aqhome/action/q2config.cfg` restored byte-identical (SHA-256
  `4b0f8926...b79f692c`).


**Menus: 2x native UI, Settings and Controls pages, Team/Weapons split,
host Server options (0.0.0.23), 2026-10-09.** Code, docs, build and package
done; **no windowed run**: the maintainer played (Quake 3 splitscreen, then
Minecraft via Nucleus Co-op) for the whole session, so only hidden loopback
dedicated servers ran. The windowed checks are Next steps 1.

- **AQtion menus 2x** (`src/client/ui/ui.c` `UI_MenuSizeScale` /
  `UI_CheckMenuSize`, `menu.c` `Menu_NeededHeight`): auto scale times
  `cl_splitMenuSize` (default 2), lowered per top menu so it fits (its
  laid-out height, at least 400x240 virtual), never below upstream; an
  explicit `ui_scale` = upstream. Settings rows "AQtion menu size" and
  "Splitscreen menu size" (`cl_splitMenuScale`, our overlay, `SM_DrawPlayer`).
- **Profile wording:** "Profile..." / "Profile: <name>" (pause page and the
  main-menu page); the page says "Playing as: Guest" and "Use guest".
- **Settings / Controls / Buttons** (`split_menu.c` `SM_BuildSettings`,
  `SM_BuildControls`, `SM_BuildButtons`): the Q3 Controls rows our pad layer
  has (look H/V, invert, aim curve + exponent, turn boost + ramp, zoomed
  speed, dead zones, trigger point, reset); per-button remap through
  `PAD_MenuSetBind` (= `pbind`; new in `in_gamepad.c` with `PAD_GetBind`,
  `PAD_MenuEnsureBinds`, `PAD_ConnectedCount`) over a 35-entry AQ2 action
  list; reset buttons. No aim assist (dropped by the maintainer), vibration
  or swap sticks. Per-player **HUD opacity** and **crosshair colour**:
  `screen.c` `SCR_PlayerVar` / `SCR_HudAlpha` / `SCR_CrosshairColor` read
  `p<N>_scr_alpha` / `p<N>_ch_*` in that player's cell; in profiles
  (`split_profile.c` `prof_feel`). Crosshair picture and size stay one shared
  setting (P1 rows).
- **Team / Weapons:** Team = our page (`team 1/2/3/none`, Action's join
  menu); Weapons = `inven`. The mode is read from the game cvars when P1
  hosts (`SM_Hosting`; `SM_WeaponChoice` = Action's GS_WEAPONCHOOSE rules);
  online it is unknown (Q2 sends no serverinfo), so both show.
- **Server options** (`SM_BuildServer` and its pages): rows only for cvars
  Steam's DLL has (`work/menus-steam-cvarlist.txt`: `cvarlist -v` on a
  hidden loopback q2proded with that DLL). Bots: live `sv addbots` /
  `removebot` with attract mode gave unstable counts
  (`work/menus-bots.log`), so Bots 0-20 apply at the next map / Restart
  (`am 2` + `am_botcount`). Saved sets in
  `<homedir>/action/splitsrv/<name>.cfg` (read back only as known `set`
  lines). Restart / Change map / Next map = `map <m> force`.
- **Start at the main menu (bug found):** with several pads, whichever pad
  first pressed anything (or drifted) became P1 and Start on any other pad
  did nothing (it was treated as an unjoined press, which only joins with
  A). `in_gamepad.c` `Pad_KeyDown`: Start on any pad out of a game makes it
  P1's and opens the page; F1 does the same (`CL_SplitMenuKeyEvent`, hooked
  in `keys.c`, which also lets keyboard and mouse drive P1's overlay); a
  hint line is drawn by us at the main menu while a pad is connected.
  Native-menu entries were briefly added and then removed when the
  maintainer withdrew that request; none ship.
- **Joke sounds:** our `src/action` `Add_Frag` (a bot frag, `bot_randvoice` %,
  default 5, gated by `use_voice`) plays one of 85 `user/*.wav`; Steam's TNG
  2.82 DLL has neither the cvar nor any of those names (string scan of the
  DLL), so the release never plays them. The "Joke sounds" row shows only
  with a DLL that has the cvar. A server-side proof (MVD recording) failed
  (files stayed empty); the client `soundlist` check is
  `work/aqhome/action/menus-b.cfg` (Next steps 1).
- Build: clean MSVC 2019 (20 known warnings, `work/menus-package.log`) and the
  `-Dgamepad=disabled` stub (`work/menus-stub-build.log`, dir deleted).
  `work/aqhome/action/q2config.cfg` untouched (no client run).

**Working directory = AQtion root + two review fixes (0.0.0.22),
2026-10-08.** Partly verified: the windowed client checks could not be run
(last bullets; Next steps 1).

- **Rule** (`src/windows/system.c` `split_self_locate`, the block after the
  `sys_forcegamelib` default): for both placements `SetCurrentDirectoryW`
  to the detected AQtion root, only when `basedir`, `libdir`, a non-empty
  `homedir` and a non-empty `sys_forcegamelib` are absolute. The 0.0.0.21
  "homedir is also the working directory" is gone. Engine audit: nothing
  after `Sys_Init` depends on cwd == exe dir (paths absolute; crash dumps
  and `anticheat` use the exe folder; `./SteamCloud/` is used only with a
  Steam `steamid`). Log line: `split: running from <AQtion> (AQtion root),
  basedir <AQtion>, homedir <AQtion>\splitscreen, game library from libdir
  <AQtion>, working directory <AQtion>`.
- **Proven on the real Steam folder** (dedicated server only: hidden, root
  placement, test-named exe, loopback; `work/cwd-run.log`): cwd = root;
  Steam's DLL loaded `action\tng\beer.aqg` ("Location file: beer", "Found
  18 locations"), ACE node tables `action\terrain\{beer,wfall,terminal}.ltk`
  ("ACE: Loading node table...done."), bot names from
  `action\bots\botdata.cfg` ("[BOT]KaniZ"), and bots fight on urbanjungle
  and terminal (kill messages; urbanjungle's `.ltk` is an 11-byte stub).
  `action/action.ini` is still "Unable to read": Steam keeps `action.ini`,
  `sndlist.ini`, `tourney.ini` in `baseaq\`, so Steam's own AQtion prints
  the same. **Found:** Steam's DLL with ACE bots never finishes spawning
  `beer`; Steam's own `q2proded.exe` hangs identically (AQtion bug; before
  this build the bots had no nodes there). Writes into Steam's folder
  (`work/cwd-steam-folder-diff.txt`): `<AQtion>\items.txt` (rewritten,
  1651 -> 67 bytes) and `action\terrain\beer.ltk` (node table re-saved at
  shutdown, same size). Everything created was removed; the maintainer's
  three files have unchanged SHA-256.
- **Strings spin** (`src/client/ui/menu.c` `Strings_Push`): the
  "unset -> entry 0" rule now applies only to `nextserver`. Compile-checked;
  the menu test (`work/aqhome/action/cwd22-menu.cfg` + test menu
  `cwd22-q2pro.menu.txt`) is not yet run.
- **First press** (`src/client/split/in_gamepad.c`): `Pad_UnjoinedPress`
  returns true only when Start took player 1; `Pad_KeyDown` then only tries
  `CL_SplitMenuPadStart` (host page); otherwise a claiming / joining press
  does nothing else. One repo run (`work/aqhome/action/cwd22-repo.cfg`)
  was spoiled by three real pads in use (one claimed P1 before the script
  did); in it, default hold-to-join was unchanged (tap A nothing, hold A
  -> P2) and an instant join (`cl_splitJoinHold 0`) as P3 ran no bind.
- **Not done:** the 22:32 windowed repo run started while the maintainer
  was playing (Nucleus Co-op + three HorizonMW instances since 22:22, three
  8BitDo pads); that session restarted at 22:44 and was still running at
  23:20, so no further windowed runs were made. `work/aqhome/action/q2config.cfg`
  restored byte-identical from a backup (that run had rewritten it).
- Build: clean MSVC 2019 (20 known warnings) + `-Dgamepad=disabled` stub
  (dir deleted), `work/cwd-build.log`; zip
  `build\aqtion-splitscreen-0.0.0.22.zip`.

**Run from the AQtion root (0.0.0.21), 2026-10-08.** The maintainer's
decision: players drop the exe straight into the AQtion folder.

- **Engine:** `src/windows/system.c` `split_self_locate()` (+
  `split_is_aqtion_root`, `split_is_absolute`): root placement as in
  Current state; subfolder placement unchanged; `+set basedir / homedir /
  libdir / sys_forcegamelib / game` still win; anywhere without game data
  (repo `build\`) nothing changes. Detection is by folders, not the file
  name (tested as `q2pro-splitscreen-test.exe`). `FS_FirstRunDefaults` /
  config-from-homedir-only apply in both placements (first run in the root:
  `vid_modelist "desktop ..."`, name "Player 1").
- **Found on the way (and fixed):** Steam's TNG 2.82 game DLL writes
  `items.txt` (a debug item dump) into the working directory at every map
  load. The first build of this round left the working directory at the
  root, and its one map load **overwrote `<AQtion>\items.txt`** (Steam's
  203-byte 2023 file -> 1651 bytes). No copy of the original exists here;
  Steam's "Verify integrity of game files" restores it (Next steps). Steam's
  own AQtion rewrites that file the same way on every map load, so it is
  harmless, but it was a write into Steam's folder: now in root placement
  the working directory is `<AQtion>\splitscreen` (only when `basedir` and
  `libdir` are absolute), log "(also the working directory)"; the re-run
  wrote `splitscreen\items.txt` and left the root file unchanged.
- **Verification on the real Steam folder** (`work/rootrun-run.log`
  sanitised, `work/rootrun-01-root-mainmenu.jpg`, `-root-hostpage.jpg`,
  `-root-wizs-n2.jpg`, `-subfolder-wizs-n2.jpg`,
  `work/rootrun-steam-folder-diff.txt`). The root already held the
  maintainer's 0.0.0.19 `q2pro-splitscreen.exe` (21:07), so the new exe ran
  as `<AQtion>\q2pro-splitscreen-test.exe`, cwd = a scratch folder, only
  `+set vid_fullscreen 0 +set vid_geometry 1280x720`: with no
  `splitscreen\` present it created `<AQtion>\splitscreen\` (q2config +
  `.conhistory` only); then (harness autoexec there, q2config deleted =
  first run) `split: running from <AQtion> (AQtion root), basedir <AQtion>,
  homedir <AQtion>\splitscreen (also the working directory), game library
  from libdir <AQtion>`; Start on virtual pad 1 -> host page -> "Start
  multiplayer game" -> wizs on **Steam's** `action\gamex86_64.dll` (API-4
  adapter), `addplayer` -> P2 active, screenshot, quit. Every file written
  is under `<AQtion>\splitscreen\` (config, log, condumps, screenshots,
  `.conhistory`, `items.txt`). Subfolder: the same exe as
  `<AQtion>\splitscreen-test\q2pro-splitscreen.exe` -> `(subfolder of
  AQtion)`, homedir `...\splitscreen-test\home`, wizs + P2. All test files
  removed; listing after = before except `items.txt` (above); the
  maintainer's three files (root exe, `action\q2config.cfg`,
  `baseaq\.conhistory`) unchanged, same SHA-256.
- Docs: `scripts\release\README.txt` (8 lines), README install + "where
  your settings are", CHANGELOG (upgrade notes), design doc §10.
- Build: clean MSVC 2019 (20 known warnings) + `-Dgamepad=disabled` stub
  (dir deleted), `work/rootrun-build.log`; zip
  `build\aqtion-splitscreen-0.0.0.21.zip` (exe + README.txt).
  `work/aqhome/action/q2config.cfg` byte-identical.

**First-run experience + two-file release (0.0.0.20), 2026-10-08.** After
the maintainer's first real run of 0.0.0.19 ("nothing like quake3e").

- **Tiny window:** cause: with an empty `home\`, `q2config.cfg` was taken
  from the next search path entry, Steam's `<AQtion>\action\q2config.cfg`
  (stale, `vid_fullscreen 0`, `vid_geometry 800x600`; written at 21:18 by
  a run of the `q2pro-splitscreen.exe` copy that sits in the AQtion root).
  Fix: `src/common/files.c` `FS_AddConfigFiles`: with a homedir the config
  is read only from it (`FS_DIR_HOME`); none there = first run,
  `FS_FirstRunDefaults` sets `vid_fullscreen 1` with "desktop" first in
  `vid_modelist` (borderless at the desktop size; AQtion's `default.cfg`
  already says so). q2config / autoexec / `+set` still win. Verified once
  fullscreen (a few seconds, condump + engine screenshot 3840x2160,
  `work/firstrun-00-fullscreen-mainmenu.jpg`).
- **Name prompt:** Steam's `baseaq\postinit.cfg` runs `if $name ==
  AQtionMan then pushmenu first_start`. Fix: `src/client/ui/ui.c`
  `UI_PushMenu_f` skips `first_start`; `src/common/common.c`
  `Com_ExecPostinit` renames "AQtionMan" to "Player 1" and hides a name
  with spaces from that `if` (else "Unknown operator '1'"), restoring it
  with a `set name` queued behind the file.
- **`maps/force.bsp`:** the path was AQtion's native "Play Offline (Bots)"
  page (pak1 `q2pro.menu`: `strings "map" nextserver $com_maplist`, Play =
  `map $nextserver force`): `nextserver` unset -> blank row -> `map force`.
  Fix: `src/client/ui/menu.c` `Strings_Push`: an empty cvar shows and gets
  the first entry (adriatic). Our host page already listed `maps/*.bsp`;
  now a saved map that is gone falls back to wizs / the first map
  (`split_menu.c` `SM_MapInstalled` / `SM_DefaultMap`), also in
  `splitstartgame`.
- **Two `Usage: set ...`:** the same native pages run `set allweapon
  $allweapon; set allitem $allitem` before any game DLL created them.
  Fix: `CL_SplitMenuInit` pre-creates both at "0", and `kill[#]` /
  `round[on]` (AQtion's `triggers.cfg`, "kill[#] is not a variable" /
  "Unknown operator '1'" on a kill before "entered the game").
- **Steam game DLL:** `src/server/game.c`: legacy API-4 adapter (import
  with `CheckForExtension` appended for libraries without `GetGameAPIEx`;
  exports read through `game_export_aq4_t`, `ge` = a wrapper copy that
  refreshes edicts/num_edicts after every call; `FetchGameExtension` for
  the two extension hooks). Spike A's crash was only half of it: the old
  export struct also has `FetchGameExtension` before `edicts`. Self-locate
  sets `libdir` (no forcegamelib unless a DLL is beside the exe). Proven:
  `run.ps1 -GameLib steam`: 4 local players, teams, `gamemap urbanjungle`
  (`work/firstrun-steamdll-*.jpg`); q2proded with the Steam DLL + client
  with 2 extras at the LAN address; our DLL still on the extended API
  (`work/firstrun-steamdll.log`).
- **Two-file zip:** `scripts\package.ps1` -> exe + `README.txt`
  (`scripts\release\README.txt`, 9 lines, repo link); `install.ps1` kept in
  the repo (new file list, removes an old `gamex86_64.dll` on upgrade);
  README install = unpack and run.
- **First-run pass** (zip unpacked by hand into the real `<AQtion>\splitscreen\`,
  cwd elsewhere, `pad_enable 0`, virtual pads, windowed after the one
  fullscreen check): main menu, no prompt, native bot page -> adriatic
  with bots on the Steam DLL, no Usage lines; Start on P1's pad -> host
  page (Map wizs, cycles installed maps; Game Teamplay) -> wizs; P2 holds A
  -> joins; Action's team menu with P2's A -> RED; P2 Start -> Profile ->
  Save as new -> on-screen keyboard "ace" -> saved; Back+Start leaves,
  hold A rejoins as ace; quit, relaunch: P2's pad -> ace
  (`work/firstrun-01..15*.jpg`, `work/firstrun-run.log`). Found and fixed on
  the way: the first press of P1's pad only claimed it (Start had to be
  pressed twice): `in_gamepad.c` `Pad_KeyDown` now passes that press on.
  Folder listing before/after identical (`work/firstrun-steam-folder-diff.txt`).
- Build: clean MSVC 2019 (20 known warnings) + `-Dgamepad=disabled` stub
  (dir deleted), `work/firstrun-build.log`. `work/aqhome/action/q2config.cfg`
  restored byte-identical.

**Self-locating exe, no launcher (0.0.0.19), 2026-10-08.** The maintainer's
decision: the player just double-clicks the exe.

- **Engine:** `src/windows/system.c` `split_self_locate()` (called first in
  `Sys_Init`, after `Com_AddEarlyCommands(false)` has applied the command
  line) and `Sys_PrintLocationNote()` (`inc/system/system.h`; called in
  `Qcommon_Init` right after the console log opens, so the line is in
  `console.log`). Rule above; `+set basedir / homedir / sys_forcegamelib /
  game` on the command line still win (checked with `Cvar_FindVar` /
  empty `sys_forcegamelib`), so `scripts\run.ps1` is unaffected.
- **Packaging:** `scripts\release\launch-splitscreen.bat` deleted; removed
  from `install.ps1` (file list, description, end message "Start it with
  ...\q2pro-splitscreen.exe; a desktop shortcut to it works too"),
  `scripts\package.ps1`, README (install step 3: the exe or a shortcut,
  options as `+set ...` in the shortcut), CHANGELOG, design doc §10. New in
  `install.ps1`: on an upgrade, a file an older install of ours put there
  and this release no longer ships (the launcher) is removed if unchanged.
- **Verification** (`work/selfloc-install-test.txt`, `work/selfloc-run.log`
  sanitised): repo `build\` via `run.ps1` windowed: no `split:` line, Steam
  data via `AQ_BASEPATH`, `work\aqhome`, our DLL, wizs
  (`work/selfloc-00-repo-build.jpg`); `build\q2pro.exe` with no arguments:
  stops with "Couldn't load pics/colormap.pcx" as before (basedir `build\`,
  no parent picked up). Install of the 0.0.0.19 zip into the Steam folder
  (only `splitscreen\` + 7 files + manifest + `home\` added); the
  installed exe started directly, cwd = scratch dir: **no arguments**
  (autoexec.cfg in `home\action` did condump + quit, then deleted):
  `split: running from <AQtion>\splitscreen, basedir <AQtion>, homedir
  <AQtion>\splitscreen\home, game library ...\splitscreen\gamex86_64.dll`,
  search path = `splitscreen\home\action`, Steam `action\`, `baseaq\`;
  **windowed** (`+set vid_fullscreen 0 +set vid_geometry 1280x720 ... +map
  wizs`): our DLL loaded from `splitscreen\`, wizs from Steam data,
  `addplayer` -> P2 active, screenshot
  `work/selfloc-01-installed-direct-n2.jpg`; only `splitscreen\home\...`
  changed (q2config, log, condump, screenshot, `.conhistory`), nothing in
  `action\` / `baseaq\`. `-Uninstall` from a shell inside `home\` (kept
  `home\`), `-Purge` from the zip: listing after = before (2483 entries, 0
  differences, same SHA-256). Upgrade over a fake 0.0.0.18 install (scratch
  folder) removes the launcher. `-Dgamepad=disabled` stub compiles (dir
  deleted). Package: clean MSVC 2019, `work/selfloc-package.log`.
  `work/aqhome/action/q2config.cfg` restored byte-identical.

**RC installer polish (0.0.0.18), 2026-10-08.** Scripts and docs only, no C
changes.

- **Settings in our folder:** `launch-splitscreen.bat` creates
  `<AQtion>\splitscreen\home` if missing and passes it as an absolute
  `homedir`. `install.ps1` creates `home\` (empty, not in the manifest);
  `-Uninstall` removes the manifest files, keeps `home\` and says where it is
  and how to delete it; new **`-Purge`** removes the manifest files and the
  whole `splitscreen\` folder (also works after an `-Uninstall`, when only
  `home\` is left, run from the zip). README: install, "Where your settings
  are", uninstall / purge.
- **Recoverable install:** each file's manifest entry is written before its
  copy and rewritten with the real hash after it, so a run that fails half
  way leaves everything it touched listed and a re-run carries on; a file
  that is not listed but has exactly the shipped bytes (the 0.0.0.17
  installer's half-install state) is taken as ours. The installer (and
  uninstall) refuses up front while `q2pro-splitscreen.exe` runs; a copy
  error prints a plain "fix the cause and run again" message.
- **cwd inside the folder:** before removing the folder the script steps
  this shell (PowerShell location and process cwd) out to the AQtion folder;
  if another window still holds it, a plain note instead of a red error.
- PowerShell 5.1 safe (`-LiteralPath` throughout, also in
  `scripts\package.ps1`; no `&&` / `??` / `?.` / ternary).
- **Verification** against the real Steam folder (`work/rcfix-install-test.txt`,
  three cycles, final one in full): listing of all 2483 entries before;
  install from the zip (`splitscreen\` + 8 files + manifest + empty
  `home\`); launch via the installed launcher, windowed, **no homedir
  override**: our DLL from `splitscreen\`, wizs, P2 joined, screenshot
  (`work/rcfix-01-installed-homedir-n2.jpg`, sanitised log
  `work/rcfix-run.log`); `home\action\q2config.cfg`,
  `home\action\screenshots\aqtion000.jpg`, `home\action\logs\console.log`,
  `home\baseaq\.conhistory` written and nothing outside `splitscreen\`
  changed; install while the game runs -> refused; upgrade install with
  the installed exe locked -> fails on it with the plain message, re-run
  OK; `-Uninstall` from a shell inside `splitscreen\home` -> no error,
  `home\` kept; half install (zip's `CHANGELOG.md` locked, 5 of 8 copied,
  plus the DLL's manifest line deleted) -> re-run recovers all 8;
  `-Purge` from inside `splitscreen\home\action` -> folder gone; listing
  after = before (0 differences, same SHA-256).
- Package: clean MSVC 2019 build, `work/rcfix-package.log`.

**M4 fixes + M4-T3 release-candidate packaging (0.0.0.17), 2026-10-08.**
Fixes for the M3+M4-T1 acceptance notes (`work/ACCEPT-M3M4.md`) and the
Sonnet review of `2a49beb8`, then the release candidate package.

- **A1 (N3) notify lines in P1's cell:** `console.c` `Con_DrawNotify` +
  `screen.c` `SCR_SplitNotifyCell` / `SCR_CellScaleFor`: with 2+ cells
  player 1's notify lines (and chat prompt) draw inside P1's cell at the
  cell rule applied to the console scale (fit-to-640 like the HUD), cut at
  the cell width; the console stays full screen. `m4fix-a01-notify-n4`
  (readable, 1x font), `m4fix-a02-notify-n8` (inside the 320x360 cell, tiny
  like that cell's HUD).
- **A2 (N4) chat vs layouts:** `SCR_DrawChatHUD`: in cells, when a layout or
  the inventory is up and the chat would run into the layout's 320x240 box,
  it moves bottom-left just above the status bar and keeps only the lines
  below the box (3 at 640x360 virtual). 426x240 clean with the DM
  scoreboard (`m4fix-a04`), Action's team menu (`b01`) and the teamplay
  scoreboard (`b02`); 320x360 likewise (`a05`, still unreadable as
  documented); no layout = normal position (`a03`).
- **A3 (N5):** host row "3, 5, 7 players" renamed **"Cell layout"** (Fill the
  screen / Equal cells), cvar unchanged (`m4fix-a07-host-page-n8`).
- **A4 (N7):** rejoin notice now "splitscreen: Server connection was reset.
  3 local players rejoin when player 1 is back on ... (reconnect)."
  (trailing punctuation of the error stripped, one sentence each; singular
  "rejoins"). Compile-checked only (needs a killed remote server).
- **A5 (N1, N2):** design doc §3.4 / §8 and code hints say `set developer
  1` (the release build has no `developer` command); perf recipe =
  `gl_swapinterval 0` (`timedemo` and `gl_showbloom` are cheat-protected
  on a normal listen server).
- **A6:** N6 (telefrag on simultaneous joins) and N8 (`prev_map_bots.txt`
  CWD warning) left game-side; both under Open questions.
- **A7 (review) notice cells:** `CL_SplitLayout` gives cells to players in
  use first (slot order, wide rule among them), notices only the cells after
  them; the wide row's first cell is the sum of the rows above; the layout
  re-runs when the set of slots changes (bitmask), not only the count. Run:
  P1+P2+P3, kick P2: P1 top-left, notice top-right, P3 wide bottom
  (`m4fix-a08`); a third join takes the free slot, everyone keeps a cell
  (`a09`); with 4 players kick P3 -> notice cell, rejoin (`a10`, `a11`).
- **A8 (review) FBO alternation:** every world scene of the frame counts
  toward the post-process FBO size (`R_RenderFrame`), not only the ones
  that need it; N=1 sizes unchanged. `set developer 1`, `gl_shaders 1`,
  `gl_bloom 1`, N=3 fill: two `GL_SizeFramebuffers` lines on the first
  frame (640x360 then 1280x360), none in 300 steady frames
  (`m4fix-run-a.log`, `m4fix-a12`). The underwater-only case was not
  exercised: no reachable water on `wizs` and no `setpos`.
- **B1 `scripts\package.ps1`:** `-Clean` build, then
  `build\aqtion-splitscreen-<VERSION>.zip` with `q2pro-splitscreen.exe`
  (our `q2pro.exe` renamed; the engine chdirs to the exe folder and
  `basedir` defaults to "." , so the launcher sets it), `gamex86_64.dll`
  (ours), `launch-splitscreen.bat`, `install.ps1` (`-Uninstall`),
  `BUILD-INFO.txt`, `CHANGELOG.md`, `README.md`, `LICENSE`. It checks that
  BUILD-INFO and the exe's version resource match `VERSION`. `dumpbin
  /dependents`: only Windows DLLs + VCRUNTIME140 / UCRT (which AQtion's own
  q2pro.exe needs too), so no DLL is shipped. Sources:
  `scripts\release\install.ps1`, `scripts\release\launch-splitscreen.bat`.
- **B2 install test** against the real Steam folder: listing (path, size,
  mtime, attributes of all 2483 entries) before; `install.ps1` found the
  folder (env var; the Steam registry + `libraryfolders.vdf` path was
  checked separately and finds it too), created `<AQtion>\splitscreen\`
  with 8 files + `INSTALL-MANIFEST.txt` (SHA-256 each) and nothing else; a
  second install over it (upgrade) works; the launcher started the
  installed exe (`+set homedir` passed through to keep the test's writes
  out of the Steam folder): our DLL loaded from `<AQtion>\splitscreen\`,
  wizs from the Steam data, P2 joined (`m4fix-rc01-installed-n2`,
  `m4fix-install-test.txt`); `-Uninstall` from the installed copy removed
  the 9 files and the folder; the listing afterwards is identical to the
  one before (0 differences). Found and fixed on the way: the first
  launcher used `( ... )` blocks, which break on the "(x86)" in the path.
- **B3** `README.md` (root) rewritten for players: install, play, limits,
  build, credits / GPL-2.0 (upstream LICENSE kept and shipped).
- **B4** version: `VERSION` 0.0.0.17 is in the exe's version resource, in
  `BUILD-INFO.txt` and the console banner ("AQtion 0.0.0.17"); **the game
  DLL had no version resource** (upstream adds it to the unused
  `game_src`): `src/windows/meson.build` now links it into the Action DLL
  too (FileVersion 0.0.0.17).
- **Build:** clean MSVC 2019 (`-Clean`, 0 errors, the known 20 warnings) and
  the `-Dgamepad=disabled` stub (dir deleted): `work/m4fix-build.log`.
  `q2config.cfg` restored byte-identical (SHA-256 `4b0f8926...b79f692c`).

**M4-T1 5-8 players (0.0.0.16), 2026-10-08**: layouts 5-8 (`cl_splitFill`,
`cl_splitWidePlayer`), fit-to-640 per-cell HUD scale, post-process FBO
sizing + the bloom scissor fix, perf pass (8 views at 1440p 412-460 fps).
Clean-room acceptance of M3 + M4-T1: `work/ACCEPT-M3M4.md` (PASS with notes,
fixed in 0.0.0.17).

## Next steps

0. **After 1.0.0:** smoke-test the Linux tar.gz on the Linux test box (the
   box was offline at release time; its mirror also needs `git push
   <linux-box> splitscreen`), then the hands-on checks below. On the next
   tag, check whether the tag push starts the workflow; if not again, keep
   the manual path (`gh workflow run release --ref main`, `gh run download`,
   `gh release create v<V> --notes-file <CHANGELOG section>`). Dev builds
   now number `1.0.0.N`.

1. **The maintainer's Linux / Steam Deck hands-on checklist** (0.0.0.34;
   the box or a Deck, real pads). The build is in
   `<AQtion install>/splitscreen-test/` (`q2pro-splitscreen`,
   `README.txt`, an empty `home/`); settings go to
   `splitscreen-test/home/`. Delete `splitscreen-test/` when done (nothing
   else in the install was changed). A Deck can take the tarball's binary
   the same way (or next to `q2pro`: settings then in `AQtion/splitscreen/`).
   - **Desktop mode**
     - [ ] Run `splitscreen-test/q2pro-splitscreen` from the file manager
       or a terminal: AQtion's menu, `version` in the console says
       0.0.0.34. Quick Local Match with bots works; quit.
     - [ ] Steam -> Games -> Add a Non-Steam Game to My Library -> Browse
       -> `splitscreen-test/q2pro-splitscreen`. Start it from Steam once in
       Desktop mode. If it fails to start or crashes when joining an
       online server that sends files, tell us (Steam puts its old libcurl
       first; workaround in the launch options: `LD_LIBRARY_PATH=
       %command%`).
   - **Game mode** (Steam Deck / Bazzite game mode)
     - [ ] The shortcut appears in the library; it starts full screen at
       1280x800 (Deck) / the TV's size docked; the picture fills the screen,
       no black border or desktop window frame.
     - [ ] Steam Input: the built-in pad's Start at the main menu opens
       the Splitscreen page; Quick Local Match.
     - [ ] 2-4 more pads (Bluetooth / USB) each held A ~1 s join as P2..P4
       in their own views; Back+Start 2 s leaves; unplug / replug within
       30 s keeps the player.
     - [ ] **Does the built-in pad appear twice** (one press of A joins
       two players, or Start opens two menus)? Steam Input's virtual pad
       plus the raw device, or the handheld daemon's pad: tell us which.
     - [ ] Each player's Start menu in its own view; hold d-pad down on
       Server options: keeps moving, faster after ~1.5 s; taps move one row.
     - [ ] Sound under Gamescope: guns / footsteps from each player's view,
       no crackle; volume follows the Deck's volume keys.
     - [ ] Suspend (power button) mid-game and resume: the game, the pads
       and the sound come back (a pad may need A held to rejoin; tell us).
     - [ ] Quit from the game menu: back to Steam's library cleanly.
   - **Carried over from Windows (by reference, Next steps 3):** the open
     items of the 0.0.0.25-0.0.0.31 passes apply to Linux too, in brief:
     menu / text / crosshair sizes per player (0.0.0.25), Infinite ammo
     reloads and zoom (0.0.0.26-0.0.0.29, + "zooming never refills the
     gun", 0.0.0.33), Humor sounds Off (0.0.0.27), the Weapons presets
     (0.0.0.30), zoom wrap and hold-to-scroll (0.0.0.31). Try them in Game
     mode if you have time; the Windows pass is the reference.
   - **Linux run recipe for agents:** `export AQ_BASEPATH=<AQtion
     install>`; `scripts/build.sh`; `scripts/run.sh --map urbanjungle
     --timeout 90 --logname <name> -- +set am 1 +set am_botcount 4`
     (Steam's `.so`, homedir `work/aqhome`, cwd `work/cwd`); `--gamescope`
     for a nested 1280x800 Gamescope window; `--self-locate --exe <path>`
     for an exe placed in the install (no homedir: it picks its own);
     commands at runtime: `+set sys_console 1` and pipe them into stdin.
     Back up / restore `work/aqhome/action/config.cfg` around runs. Never
     symlink Steam's folders into a test tree (the game library writes
     through them); the game library writes `items.txt` into its cwd.

2. **Agent test runs (from 0.0.0.24): always `scripts\run.ps1 -Offscreen`**
   (add `-AllowPrimary` while no virtual display is attached; the maintainer
   allowed the primary-display corner as long as the window never takes
   focus). It adds `pad_enable 0`, `vid_noactivate 1`, `s_enable 0`, checks
   focus with `-Wait`, and refuses to start while another q2pro / AQtion
   runs. Back up `work/aqhome/action/q2config.cfg` (SHA-256
   `4b0f8926...b79f692c`) and restore it. Steam-folder runs: `-Exe
   <AQtion>\q2pro-splitscreen-test.exe -SelfLocate -HomeDir
   work\steamtest-home` (cfg as that homedir's `action\autoexec.cfg`), back up
   `<AQtion>\items.txt` and `action\terrain\*` first and restore them, remove
   the test exe. Larger test windows (1080p / 4K cells) work with
   `-Extra '+set vid_geometry 1920x1080+-1936+-1120'` (0.0.0.25). If the
   agent shell runs in session 0 / a disconnected desktop, a crash shows no
   dialog: a run that "hangs" in `-Wait` may be the "AQtion Unhandled
   Exception" box (enumerate the pid's windows; "Yes" writes
   `Q2PRO_CrashReport00.txt` beside the exe; a `debugoptimized` build gives
   symbols). Nothing is pending for agents except what needs a keyboard or
   real pads (item 3).
3. **The maintainer's Windows pass of 0.0.0.34** (your PC, your pads,
   your TV). **0.0.0.34 is already installed** as
   `<AQtion>\q2pro-splitscreen.exe` (your 0.0.0.31 copy is in
   `work\maintainer-exe-backup\` with its hash); just start it. Console
   `version` says 0.0.0.34.
   - **New in 0.0.0.34 (5 minutes)**
     - [ ] **Sniper zoom never refills the gun.** Server options ->
       Weapons -> Sniper only, Infinite ammo -> On, Quick Local Match, 2+
       players. Fire 2-3 rounds, then LT several times (through 2x / 4x / 6x
       and back): the count in the gun stays where it was. Fire it empty and
       reload: the gun fills again, and the spare count (right of the gun
       count) does not run out.
     - [ ] One player sets Start -> Settings -> Gun hand -> Hidden: that
       player's spares stay topped up after reloads too.
     - [ ] **Quick Local Match never starts `credits`** (the map where
       everyone spawns on one spot and dies at once): start it a few times
       (back to the main menu, then Start -> Quick Local Match again).
   - **0.0.0.31 (still open if not yet tried)**
     - [ ] **Zoom wraps with the sniper.** Server options -> Weapons ->
       Sniper only, Quick Local Match, 2+ players. Each player: LT four
       times: 2x, 4x, 6x, then back to normal view (no LB needed). LB still
       resets from any zoom. With another weapon LT does nothing.
     - [ ] **Your profiles:** load a saved profile (or join with a pad that
       remembers one): LT wraps too (the console shows "P<N>'s LT is now
       Zoom (cycle)" once). Start -> Settings -> Controls -> Buttons -> LT
       shows "Zoom (cycle)"; picking "Zoom in" there gives back the old
       stop-at-6x step, and it stays that way.
     - [ ] **Hold to scroll:** Start -> Server options. Hold d-pad down:
       after a short moment it keeps moving, then faster after about 1.5 s;
       let go: it stops at once. Same with the left stick, and up.
     - [ ] **A tap never skips a row:** quick presses of d-pad down (and
       quick stick flicks): exactly one row each. Tell us if the delay
       (0.4 s) or the speed (about 8 rows a second, then 16) feels wrong.
     - [ ] Hold left / right on a choice row (e.g. Time limit): the value
       keeps stepping. On the on-screen keyboard (Profile -> Save as new
       profile...) holding a direction moves along the keys.
   - **0.0.0.30 (still open if not yet tried)**
     - [ ] **Weapons: Sniper only -> spawn with the sniper and ammo.**
       Player 1: Start -> Server options -> **Weapons** -> Sniper only (help
       line "Now: at the next spawn" in a game, "Everyone spawns with it" at
       the main menu). Quick Local Match (or the next spawn / round in a
       running game), 2+ players: everyone holds the sniper rifle, 6 in the
       gun and 10 spare rounds; it fires and reloads. The pistol is still
       there as a sidearm with its rounds.
     - [ ] **Try two other presets** (e.g. M4 only and Knife only; also in
       teamplay if you like): everyone spawns with that weapon and its
       spares; in teamplay the weapon menu (Start -> Weapons) lists only
       that weapon.
     - [ ] Back to **All**: the normal spawn again (deathmatch: pistol;
       teamplay: your own pick in the weapon menu, MP5 by default).
     - [ ] Optional: Weapon bans -> switch one weapon off: the Weapons row
       shows "Custom".
   - **0.0.0.29 (still open if not yet tried)**
     - [ ] **Infinite ammo: empty the gun, it stops firing; reload; spare
       magazines never run out.** Infinite ammo On (Start -> Server options;
       help line "Now: endless spares, reloads; cheats on"), 2+ players. Each
       player: hold fire until the gun is empty: the count reaches 0 and the
       gun stops (clicks). Reload: full again. The spare-magazine number
       (right of the ammo count) stays where it was. Do it with a few guns
       (MP5 / M4 on full auto, M3 or sniper: one shell / round per reload).
     - [ ] Turn it On mid-game with an empty gun and no spares: you get one
       magazine at once (the gun stays empty until you reload). After the
       next death you get the full carry limit.
     - [ ] Leave it On, quit, start again, Quick Local Match: same from the
       first spawn. Thrown knives / grenades still come back.
     - [ ] Off in the game: guns and spares run out again at once.
   - **0.0.0.28's checks still apply, except "the count never runs down"
     (that was the behaviour you did not want):** On mid-game without a
     restart, a game started from AQtion's own menus, teamplay with the
     weapon menu open, Off at once (`cheats` 0).
   - **New in 0.0.0.27 (5 minutes)**
     - [ ] Host a game with bots (Splitscreen page -> Start multiplayer
       game, or Quick Local Match), 2+ players, Humor sounds On (the
       default): leave a player standing still: a joke clip about every
       15 s, as before.
     - [ ] Player 1: Start -> Server options -> **Humor sounds** -> Off
       (row under "Bot chat"). **Humor sounds off: play 3 minutes with bots,
       none heard** (stand still on purpose too). Other sounds (guns,
       radio, announcements) still play.
     - [ ] Restart map / Next map / quit and start again: still Off and
       still silent. Save a set with it Off, Reset to defaults (On again),
       load the set: Off.
     - [ ] Switch it back On: the idle clips come back within 15 s.
   - **New in 0.0.0.26 (2 minutes)**
     - [ ] With Infinite ammo Off: console `set cheats 1`, then Server
       options -> Restart map: `cheats` is still 1 (`cheats` in the
       console). Infinite ammo On -> Restart map -> Off -> Restart map:
       `cheats` is 0 again.
   - **New in 0.0.0.25 (about 20 minutes, 2-4 pads)**
     - [ ] Start -> the row says **"Profile: Guest"** (after saving a
       profile: "Profile: <name>"); the main-menu Splitscreen page too.
     - [ ] Each player: Start -> Settings -> **Crosshair**, **Crosshair
       size** (new default 3x, up to 4x), **Crosshair colour**: each change
       shows only in that player's view. Is 3x a good default on the TV?
     - [ ] Each player: Settings -> **Text size**: chat, "X killed Y" lines
       and Action's centre messages in that player's view get bigger /
       smaller (default 2x). Readable with 2, 3 and 4 players? Note: in a
       quarter view the largest sizes stop growing (each line keeps room
       for about 52 characters).
     - [ ] Player 1 with 2-4 players in: Escape (or Start -> Game menu):
       AQtion's menu in player 1's view at about the same size as at the
       main menu, smaller only if it would not fit the view. Settings ->
       **AQtion menu size** 1x / 2x / 3x changes it. Each player's
       **Splitscreen menu size** changes only that player's Start menu.
     - [ ] Optional: in the console `p2 pushmenu game`: AQtion's menu in
       player 2's view at player 2's AQtion menu size, driven by player 2's
       pad (B / Start closes it). (No menu row for this, as you decided:
       AQtion's game menu has Disconnect / Play Now, which end everyone's
       game.)
     - [ ] (Infinite ammo: replaced by the 0.0.0.28 checks above; no Restart
       map needed any more.)
     - [ ] Profiles: change a player's crosshair / text size, save the
       profile, leave and rejoin on that pad: the settings come back.
   - **Still open from the 0.0.0.24 list (unchanged)**
   - **Before you start (0.0.0.19 leftovers; delete them if you like)**
     - [ ] `<AQtion>\q2pro-splitscreen.exe`: already replaced by 0.0.0.26
       (2026-10-12).
     - [ ] `<AQtion>\action\q2config.cfg` and `<AQtion>\baseaq\.conhistory`
       (21:18, written by that 0.0.0.19 run): this build never reads or
       writes them, and Steam's AQtion keeps its config in `SteamCloud\`.
     - [ ] Any old `<AQtion>\splitscreen\` from an earlier unpack: its
       `q2pro-splitscreen.exe`, `README.txt` and any `gamex86_64.dll` can go;
       the new exe keeps its settings directly in `splitscreen\` (old
       profiles: move the contents of `splitscreen\home\` up one level), or
       just delete the whole folder for a true first run.
     - [ ] `<AQtion>\items.txt` and `<AQtion>\action\terrain\beer.ltk`:
       rewritten by Steam's game DLL in our tests (it writes them into the
       working directory, the AQtion folder, as under Steam's own AQtion;
       since 0.0.0.22 our exe gives it the same folder on purpose). Steam ->
       AQtion -> Properties -> Installed Files -> "Verify integrity of game
       files" puts the originals back if you like.
   - **New menus (0.0.0.23)**
     - [ ] At the main menu press Start on **any** of your pads (also one that
       pressed nothing before): the Splitscreen page opens every time; F1 on
       the keyboard too. The hint line at the bottom of the main menu.
     - [ ] Quick Local Match (first row): a game starts on a random map.
     - [ ] AQtion's own menus (Escape) are 2x at the main menu: readable on
       the TV?
     - [ ] Each player's Start menu: Team (join, spectate), Weapons, Settings
       -> Controls -> Buttons (change a button, reset); HUD opacity and
       crosshair colour change only that player's view.
     - [ ] Player 1, in a game you host: Server options: time limit (now),
       game type + Restart map, Bots 0-20 (next map), weapon / item bans,
       Weapons: knife only, saved settings (save, quit, load).
     - [ ] Deathmatch without "Weapon choice": no Weapons row; with it: there.
     - [ ] **Keyboard (agents cannot test it, 0.0.0.24):** at the main menu F1
       opens the Splitscreen page; arrows / Enter / Escape drive it; on the
       on-screen keyboard (Settings -> Name) typing works, Shift+1 gives
       "!". Hold the left mouse button or an arrow key while pressing F1:
       nothing stays stuck after you let go.
     - [ ] **Bots None (fixed in 0.0.0.24):** after a bot game from AQtion's
       own "Play Offline (Bots)" page, start terminal from the Splitscreen
       page with Bots = None: no bots join.
   - **First run, the way a player does it**
     - [ ] Put `q2pro-splitscreen.exe` from the zip into the **AQtion
       folder** and double-click it. Allow the firewall prompt. Expect:
       **fullscreen at your desktop resolution**, AQtion's main menu, **no
       "Choose your name"**, a `splitscreen\` folder appears in the AQtion
       folder. Read `README.txt`: enough to get going?
     - [ ] With only the 8BitDo: press **Start** once: the Splitscreen page
       opens (first press counts now). Map shows wizs and cycles only maps
       you have; Game = Teamplay; "Start multiplayer game": the map loads
       on the first try (Steam's own Action game runs it now; the console
       says "Game library uses the legacy AQtion API 4 ..., adapted").
     - [ ] Also once: Escape -> Disconnect, main menu "Play Offline
       (Bots)" -> Deathmatch -> the map row shows a map (adriatic) -> Play:
       a bot game starts (no `force.bsp`, no `Usage: set` in the console).
       Avoid the map **beer** (AQtion's own bug: a bot game there never
       finishes loading). New in 0.0.0.22: on maps with bot routes (e.g.
       terminal, wfall, urban*) the bots have names and run the map.
       Note: these AQtion pages set `am 2` (bots), which then sticks for
       later games too (AQtion's behaviour); `am 0` turns bots off.
     - [ ] Console (`~`): `version` says 0.0.0.22; `split: running from
       <AQtion> (AQtion root), basedir <AQtion>, homedir
       <AQtion>\splitscreen, game library from libdir <AQtion>, working
       directory <AQtion>`; your name is "Player 1".
   - **Pads (the M2 checklist, condensed)**
     - [ ] Sticks move / look, keyboard and mouse still work for player 1.
     - [ ] Second pad: wiggle -> "Player 2: hold A to join"; tap A ->
       nothing; **hold A** -> the screen splits. P2's team menu: d-pad
       up/down, A picks, B closes.
     - [ ] Play a minute: RT fires, A jumps, B crouches, X reloads, Y
       bandages, LT zooms / LB back to 1x, RB next weapon, d-pad right weapon
       mode, R3 knife. Say which buttons feel wrong.
     - [ ] **Start** on P2's pad: P2's menu in P2's view; Settings (invert,
       hand, name with the on-screen keyboard). Profile -> "Save as new
       profile...", your name, Start. Quit, start again, hold A on the same
       pad: you come back as yourself.
     - [ ] P2 holds **Back+Start** 2 s -> P2 leaves, the others stay; hold A
       again -> back with the profile. Unplug P2's pad: "controller
       disconnected (leaves in 30 s)"; plug back within 30 s -> plays on.
     - [ ] Escape with 2 players: the menu in P1's half. **Console messages
       (top left) stay inside player 1's view** (new in 0.0.0.17).
     - [ ] Sound: P2 shoots a wall far from P1: sounds close; nothing heard
       twice.
     - [ ] Say if the timings (0.7 s join, 2 s leave, 30 s grace), the menu
       text size and the borders feel right.
   - **5-8 players** (`addplayer` in the console fills what you have no pad
     for)
     - [ ] Players 5, 6, 7, 8 by holding A: 3+2, 3x2, 4+3, 4x2; every view
       has its own HUD; black lines between all views.
     - [ ] Start -> Splitscreen settings -> **"Cell layout"** = Equal cells:
       7-8 players become 3x3 with black gaps; "Wide cell for" = Player 2
       moves P2 to the wider bottom view. Back to Fill the screen.
     - [ ] On your TV: can you read the HUD, the scoreboard (Back) and the
       team menu in a 1/8 view? In a 3x3 view? With the scoreboard up in a
       3x3 view, the chat sits above the status bar, not over the
       scoreboard. Say the window / TV size.
     - [ ] Play a minute with 6-8 views: smooth, nothing heard twice;
       Back+Start leaves only that player.
     - [ ] Optional: `gl_shaders 1; gl_bloom 1; vid_restart`: every view
       keeps its own glow, nothing flickers on join / leave.
   - **Real servers (M3-T3; online, only you do this)**
     - [ ] Join a public AQ2 / AQtion server as player 1; hold A on a second
       pad: P2 joins the same server with its own HUD / menu / chat. Pick a
       team with the pad.
     - [ ] Add player 3 and 4: with the usual `sv_iplimit 3` the 4th is
       refused in its own view for 10 s ("this server allows only 3
       players from one connection"); say what each server did.
     - [ ] Play a few minutes with 2-3 players: no "Bad challenge" loops,
       nobody stuck "connecting...", ping about the same, nothing heard
       twice. Stay through a **map change**: everyone follows.
     - [ ] `splitplayers`: all `active`, same server, different qports.
     - [ ] P2 Back+Start leaves only P2; hold A -> back. A kick shows only in
       that player's view.
     - [ ] Anticheat-required servers: note what the extras show. Servers
       with `sv_force_reconnect`: the extras reconnect and come in.
     - [ ] Console: copy any `split: P<N> ignored stufftext "..."` lines here.
   - **Relaunch and uninstall**
     - [ ] Quit, start the exe again: same window mode, P2's pad comes back
       with its profile. Then delete `<AQtion>\q2pro-splitscreen.exe` and
       `<AQtion>\splitscreen\`: Steam's AQtion still starts normally.
4. **Create the public GitHub repo** `rebelancap/aqtion-splitscreen` (or
   tell us the real URL): `README.txt` in the zip links there for the full
   README, changelog and the GPL source.
5. **Pick a release number and tag it** (your call: the first public
   number, its timing, and whether this candidate is it). After your pass,
   a release is the zip from `scripts\package.ps1` on a clean commit
   uploaded as a GitHub release; nothing is published by agents.
6. Fix whatever the RC pass finds (new dev builds 0.0.0.27, ...).
7. Bot data: resolved in 0.0.0.22 for Steam's DLL (cwd = AQtion root);
   with our own `src/action` DLL (dev) bots still need its own nav files.

## Open questions

- **Archive names: resolved (0.0.0.34, default taken).** Windows
  `aqtion-splitscreen-<V>-windows-x86_64.zip` (`package.ps1`), Linux
  `aqtion-splitscreen-<V>-linux-x86_64.tar.gz`, as Quake3e-splitscreen
  does; the workflow follows (Next steps 0).
- **Linux binary's libjpeg (L3; resolved in 1.0.0.1):** 1.0.0's Ubuntu
  22.04 CI build linked `libjpeg.so.8` (missing on Fedora / Bazzite
  outside Steam). Since 1.0.0.1 CI links libjpeg-turbo statically
  (`--force-fallback-for=libjpeg-turbo`, gcc 11 builds it) and checks
  NEEDED; left open only for the maintainer: when to release it (1.0.0
  users on Fedora-based desktops still need Steam's runtime).
- **Steam's old libcurl (new, L3):** Steam puts its runtime's libcurl
  (7.x, no `curl_multi_poll`) first for games and non-Steam shortcuts; the
  game runs, but an HTTP download from a server would stop it. AQtion's
  own `launch.sh` has the same workaround need. Default: no code change
  now; the checklist suggests `LD_LIBRARY_PATH= %command%` if it bites;
  a later round can load curl only when needed or fall back to UDP
  downloads.
- **SteamOS glibc (new, L3):** not checkable offline; SteamOS 3.5+ is
  believed to ship glibc >= 2.37, and the CI build needs <= 2.35 anyway.
  Default: build on Ubuntu 22.04 and check the max GLIBC symbol in CI; the
  maintainer's Deck run confirms.
- **Wayland probe needs WAYLAND_DISPLAY (new, L3):** a Gamescope child
  gets only Gamescope's Xwayland, so the game now uses x11 there (before:
  it opened on the desktop's `wayland-0`, outside Gamescope).
  `--expose-wayland` Gamescope also works (Wayland). Default: keep (a
  one-function change in `wayland.c`; upstream PR candidate).
- **Test install left in the Steam folder (new, L3):**
  `<AQtion install>/splitscreen-test/` stays for the maintainer's checks.
  Default: the maintainer deletes it when done; agents do not touch it.
- **Quick Local Match skips `credits` (new, L2):** AQtion's credits map
  is a scene with one spawn point; it is now skipped like `beer`. It can
  still be picked by hand on the Map row. Default: keep (no general
  "playable map" test; the 26 installed maps were checked, only credits
  has a single deathmatch spawn).
- **Infinite ammo "holding a weapon" (new, L2):** now gun model or HUD
  ammo / clip icon (Action hides the model while zoomed and for Gun hand:
  Hidden). Default: keep; the Windows box re-runs one 0.0.0.29 ammo check
  with a zoom after the merge.
- **The Linux test box has real pads attached (new, L2):** agent runs with
  `--pads` open the handheld daemon's virtual pad (never pressed by an
  agent). Default: agents keep `pad_enable 0` except the one SDL init /
  shutdown check per round that needs it.
- **libdecor and Steam Deck (new, L1):** the Fedora build links
  `libdecor-0.so.0` directly (upstream's Wayland driver uses it). SteamOS 3
  ships it, but the release binary comes from CI (Ubuntu 22.04). Default:
  keep upstream's Wayland option set. L3: `ldd` done on the Fedora binary
  (`work/l3/l3-ldd.txt`); the CI job checks its own NEEDED list (Next
  steps 0).
- **Linux video driver (new, L1):** upstream's default order picks Wayland
  on KDE; X11 / XWayland (`vid_driver x11`) works too. Default: keep
  upstream's default. L3: both work under Gamescope; the default now picks
  x11 there (the Wayland probe change above).
- **Map for bot smoke tests (new, L1):** the brief's `teamjungle` is not in
  Steam's install; L1 used `urbanjungle` (and `wizs`). Default: urbanjungle.

- **Hold-repeat timing (new, 0.0.0.31):** 400 ms before the first repeat,
  then 120 ms, 60 ms after 1.5 s held (`PAD_REPEAT_*` in in_gamepad.c, no
  cvar). Default: keep until the maintainer says it feels slow / fast.
- **Player 1's own pad binds in q2config.cfg are not upgraded (new,
  0.0.0.31):** if player 1 (no profile) once changed its pad buttons, its
  LT keeps `lens in` (stops at 6x). Default: leave it (Buttons -> LT ->
  "Zoom (cycle)", or Reset buttons, fixes it); profiles and unchanged
  tables already get `lens`.
- **Keyboard / mouse zoom (new, 0.0.0.31):** no client change: a keyboard
  `bind <key> "lens in"` still stops at 6x, `bind <key> lens` wraps (Action's
  own command). Default: keep (the player's own binds are theirs).

- **Gun presets keep the pistol, ban the knife (new, 0.0.0.30):** "Sniper
  only" is sniper + MK23 (the sidearm every player carries; without its
  bit Action gives it no rounds, the 0.0.0.29 bug) with knife and grenades
  banned, so the teamplay weapon menu lists only the sniper. Default: keep.
  Alternatives: also allow the knife (the menu then lists it too), or ban
  the pistol (an empty MK23 stays in the inventory; Steam's DLL always adds
  it).
- **MK23 only in deathmatch has no spare magazine (new, 0.0.0.30):** 12
  rounds, 0 spares is Steam's DLL's normal deathmatch pistol spawn
  (teamplay gives 1 magazine); pistol magazines on the map still work.
  Default: keep (the game's rule; Infinite ammo On tops spares up).

- **Infinite ammo overshoot for shells / sniper rounds (new, 0.0.0.29):**
  the game gives 7 shells / 10 sniper rounds per `give`, so after a reload
  the reserve can show up to 6 / 9 more than the carry limit (e.g. 20
  shells instead of 14). Magazines (pistol, MP5, M4) are exact. Default:
  keep (it never drops below the limit minus one).
- **Infinite ammo help line (0.0.0.29):** the footer fits about 40
  characters, so it reads "Now: endless spares, reloads; cheats on".
  Default: keep.
- **Humor sounds Off also blocks clips other players send (new, 0.0.0.27):**
  with the row Off your local players hear no `insane/` or `user/` sound on
  any server, including a human's `voice` command online. Default: keep
  (one switch, "no joke clips"). The row is On by default, so the game is
  unchanged unless you choose Off.
- **AQtion's menu for players 2-8 (decided by the maintainer,
  2026-10-12):** no AQtion engine menu row for players 2-8; Team and
  Weapons in their Start menu suffice (AQtion's game menu has Disconnect /
  Play Now, which end everyone's game). The console path `p<N> pushmenu
  game` and their AQtion menu size row stay.
- **Infinite ammo = cheats (decided by the maintainer, 2026-10-12):**
  accepted. Steam's Action DLL has no other way, so the row turns `cheats`
  on for the hosted game (which also allows `noclip`, `give` etc. to anyone
  on it) and keeps only your local players' ammo full; bots and players
  from other machines run out as usual. Since 0.0.0.26 the row never turns
  off a `cheats 1` it did not set.
- **Text size limit (0.0.0.25):** chat / centre text never gets narrower
  than about 52 characters per line, so in a quarter view 2.5x and 3x look
  the same. Default if no answer: keep (Action's welcome message needs 51).
- **A virtual display for agent test runs (0.0.0.24):** none is attached
  (Sunshine / Parsec / SudoMaker outputs all detached), so agent runs use a
  never-focused window in the bottom-right corner of your display, under
  your windows. If you would rather have them on a virtual display, attach
  one yourself (agents never change display settings) and `run.ps1
  -Offscreen` picks it. Default if no answer: keep the corner fallback.
- **Suggested further options (0.0.0.23; in Steam's TNG 2.82 DLL, not
  exposed):** `use_randoms` (random weapons and items), `item_kit_mode`
  (item kits), `use_punch`, `hc_single` (handcannon single shot),
  `use_flashlight`, `ir` (IR goggles allowed), `knifelimit`, `medkit_drop` /
  `medkit_time`, `sv_gravity`, `warmup` / `warmup_bots`, `eventeams` /
  `use_balancer` (team balance), `use_mapvote` / `use_kickvote` (votes),
  `ltk_jumpy` (jumpy bots), `am_team` (which team bots join), `radio_max`,
  `hud_team_icon` / `hud_noscore`, `ff_afterround`, `maxteamkills`,
  `punishkills`, `matchmode`. Default: leave them out until asked.
- **Bots apply at the next map (new, 0.0.0.23):** Steam's attract mode
  (`am 2`) adds bots at map start and refills departures unpredictably
  (live `sv addbots 3` ended with 5). Default: the row applies at Restart /
  next map, as its help line says.
- **Dropped Server options rows (new, 0.0.0.23):** Espionage (no `esp` in
  Steam's DLL), "bandaging required" (no lever). (Joke sounds with Steam's
  DLL were wrongly listed here as "no such feature"; 0.0.0.27 has the Humor
  sounds row.) "Infinite ammo" is Action's dmflags 8192: pickups give
  1000 rounds and grenades are never used up; magazines still reload.
  Default: keep as is.
- **F1 = Splitscreen page at the main menu (new):** the keyboard key for
  Start. Default: F1.
- **Team / Weapons online (new):** Q2 servers send no serverinfo, so on
  another machine's server both rows always show. Default: keep.

- **Install layout, exe name, launcher (resolved by the maintainer,
  0.0.0.19; 0.0.0.20: two files; 0.0.0.21: in the AQtion root):**
  `<AQtion>\q2pro-splitscreen.exe` (+ `README.txt`), started directly (no
  launcher); the exe locates the data and Steam's game DLL itself (also
  from a subfolder). Steam's files are never
  touched. A folder outside the Steam library would need `+set basedir` in
  a shortcut (not planned).
- **Exe in the AQtion folder itself (resolved by the maintainer,
  0.0.0.21):** that is now the documented install. Detected by `baseaq\` +
  `action\` beside the exe (not by its name); settings in
  `<AQtion>\splitscreen\`, so the engine never writes into Steam's folders
  (the game DLL does, as under Steam: see "Working directory"). A subfolder
  still works (`home\` beside the exe); the README mentions it as an
  alternative.
- **Working directory (resolved, 0.0.0.22):** the AQtion root in both
  placements, as Steam's launch gives the game DLL. Consequence: the game
  DLL's own files (`items.txt` at every map load, ACE node tables
  `action\terrain\<map>.ltk` re-saved at shutdown, possibly bot files under
  `action\bots\`) land where Steam's own AQtion writes them; the engine
  itself still writes only into the homedir.
- **`beer` + bots hangs (new, 0.0.0.22, AQtion bug):** Steam's TNG 2.82 with
  ACE bots never finishes spawning `beer`; Steam's own `q2proded.exe` hangs
  the same way. Default: leave it (not ours); noted in the CHANGELOG.
- **Default name / first-run video mode (resolved, 0.0.0.20):** never
  prompt (Q3 fork); P1 is "Player 1" (Steam's default "AQtionMan" is
  renamed, also in an older saved config); names are set in the overlay
  or console. First run = fullscreen at the desktop resolution
  (borderless, `vid_fullscreen 1` + "desktop"), saved configs and `+set`
  win.
- **AQtion's native bot pages (decided, 0.0.0.20):** kept, made to work
  (first map preselected when `nextserver` is empty, `allweapon` /
  `allitem` pre-created) rather than hidden. Left as AQtion has it: its
  Team bot page's `if $teammode == 1` prints "Unknown operator" (that cvar
  never exists; the TeamDM/Teamplay row still works), and `am 2` sticks.
  Default: keep.
- **Repo URL in README.txt (new):** `https://github.com/rebelancap/aqtion-splitscreen`
  (assumed, like the Q3 fork's). Default: that; change
  `scripts\release\README.txt` if the repo gets another name.
- **Where settings go (resolved, 0.0.0.18 / 0.0.0.19; 0.0.0.21: root
  placement -> `<AQtion>\splitscreen`):** the exe defaults
  `homedir` to `<AQtion>\splitscreen\home` when it runs from there
  (self-contained; `-Uninstall` keeps it, `-Purge` deletes it); `+set
  homedir ...` overrides.
- **No Steam / Discord in the release build** (resolved by default): no
  `steamid`, no Steam Cloud config, no achievements. Default: keep.
- **Notify lines in P1's cell (new, 0.0.0.17):** drawn at the cell's scale
  (the HUD's fit rule), so at 320-wide cells they are as small as the HUD.
  Default: keep; revisit with the "can you read it" answer.
- **Chat placement with a layout up (new, 0.0.0.17):** only when a layout /
  the inventory is up in a cell and the chat would overlap its box; the chat
  then shows fewer lines (3 at 426x240). Default: keep.
- **Notice cells (new, 0.0.0.17):** a left / refused player's notice keeps
  a cell for 10 s (Q3 R11) but only after every player in use has one; a
  new join usually takes the same slot and ends the notice. Default: keep.
- **N6 telefrag on simultaneous joins (game side):** several extras joining
  at once on `wizs` DM often spawn one onto player 1's spot (Action's spawn
  choice, same family as M2 N6). Default: leave it to the game; a client
  workaround (spacing joins further apart than the serialized connects
  already do) only if it bothers in real play.
- **N8 `prev_map_bots.txt` (game side):** the game DLL writes
  `.\action\bots\prev_map_bots.txt` relative to the working directory (the
  exe folder) and prints "Could not write" when that folder is missing.
  Default: leave it (harmless; part of the CWD bot-path leak).

- Pad timings (M2-T3): join hold 700 ms and leave hold 2000 ms are the Q3
  fork's values (`cl_splitJoinHold`, `cl_splitLeaveHold`; the brief said
  ~1 s for both). Default: keep Q3's; tune after the maintainer's check.
- Lost-pad grace (M2-T3): the Q3 fork lets a player whose pad vanished idle
  forever; here an extra leaves after `cl_splitPadGrace` 30 s (0 = Q3's
  behaviour). Default: 30 s.
- Overlay keyboard buttons (M2-T4): Q3 §12.4's mapping (X delete, Y space,
  Start done, **B cancel**), not the brief's "B backspace". Default: Q3's.
- Overlay rows (M2-T4): no "Resume" row (B / Start close); look speed H/V,
  invert, hand, name are the per-player settings; `p<N>_joy_*` are created
  on first change and not archived (T5 profiles decide persistence).
  Default: keep.
- Default pad table (M2-T2, design §5.2): UrT layout mapped to Action.
  Departures: L3 = `+speed`
  (walk; AQ2 has no sprint); Share = `opendoor`; back paddles = four radio
  calls; B over a menu = `putaway` (Escape's command) instead of the
  brief's `inven`. Default: keep; adjust from the maintainer's pass.
- `cl_splitMaxPlayers` (resolved M4-T1): default raised to 8; it still
  caps `addplayer` as well as pad joins (Q3 does the same).
- Pad zoom (M2-T1): the look speed while zoomed follows the zoom
  (`fov / 90`: 2x = half speed, 4x = 0.22, 6x = 0.11, like `m_autosens`
  for the mouse), times `joy_zoomScale` (default 1). The Q3 fork used a
  fixed 0.5. Default: keep the proportional rule; tune after the
  maintainer's pad check.
- `padinject` works without `cheats`/`developer` (the Q3 fork required
  one). Default: leave it open — it only presses buttons of local players,
  which a server's stufftext can already do (since 0.0.0.10 that includes
  joining / leaving like `addplayer` / `dropplayer`, also stufftext-able).
- Look/aim cvars use the Q3 fork's shared `joy_*` names; a player's own
  value is `p<N>_joy_<name>` (profiles store all 12). A guest's feel is
  player 1's current `joy_*`. Default: keep (Q3's separate editable Guest
  defaults page, its §14.3, is not built).

- Fork base: `aqtion` HEAD rather than the Steam tag. Default: HEAD (it
  stays compatible with the Steam data and live servers).
  **API-4 compatibility (resolved, 0.0.0.20):** added (about 100 lines in
  `src/server/game.c`, more than the estimated 15 because the old export
  struct differs too); the release hosts with Steam's game DLL and ships
  no DLL. Our `src/action` DLL stays the dev default (`run.ps1`) and is
  used if placed beside the exe.
- Pad backend: **decided by Spike P** — static SDL2 2.32.8 from the WrapDB
  wrap (joystick/gamecontroller only, about +1 MiB exe). XInput dropped
  unless a later problem appears.
- Keep Discord/Steam-shim integration for player 1? Default: build
  without both (`-Ddiscord-sdk=false`; the shim values are plain cvars).
- Teamplay scoreboard / HUD in small cells (resolved M4-T1): a fractional
  per-cell scale makes every cell at least 640 virtual px wide (one rule,
  `SCR_CellScale`); scoreboards, menus and the status bar fit at 320x360
  and 426x240, and the teamplay scoreboard fits 640x360 at scale 1.
  **New:** at 320 wide that text is 4 px high and unreadable. Options: a
  lower minimum (e.g. 480: 0.67x, a little clipping of wide layouts) or
  accept it (8 players at 1280x720 is the extreme; at 1440p a 1/8 cell is
  640 wide and draws at 1x). Default: keep 640 until the maintainer's
  "can you read it" check above.
- Top/bottom 2-player FOV (Spike C): Hor+ gives a 1280×360 cell about
  139° horizontal. Default: keep Hor+ (same as the Q3 fork) and offer
  `cl_splitVertical` for side-by-side.
- Per-cell auto HUD scale (resolved M4-T1): the M1-T4 integer rule (auto
  rule applied to the cell, capped by the window's, no DPI floor per cell)
  plus the fit-to-640 rule above; an explicit `scr_scale` still applies to
  every cell unchanged.
- Fit rule scope (new, M4-T1): our overlay menu and the engine UI in P1's
  cell are not shrunk by it (the overlay lays itself out to the cell width
  and stays readable at 320x360; the engine UI keeps `CL_SplitUIScale`).
  Default: keep.
- 3 players now 2+1 (new, M4-T1): `cl_splitFill` defaults to 1 as in the
  Q3 fork, which changes 3 players from 2x2-with-a-black-cell to two on
  top and one wide view at the bottom (the last joined player, or
  `cl_splitWidePlayer`). Default: keep (Q3's policy); `cl_splitFill 0`
  brings the old look back.
- `cl_splitperf` (new, M4-T1): a developer cvar printing CPU time per frame
  phase every 2 s, kept in the build (costs nothing when 0). Default: keep.
- Extras that are not drawable (connecting, or player 1 reloading) show a
  black cell with one status line. Default: keep.
- Toolchain: VS 2019 (pinned) vs the VS 2022 Build Tools now on this box
  (what upstream CI uses). Default: stay on 2019; `-Vs 2022` is available.
- Upstream lagometer over-read with `scr_lag_draw_scale` > 1 (M1-T1
  note). Default: leave it (upstream code, harmless read inside the
  context struct); fix only if it ever shows garbage.
- Stufftext from an extra's server (resolved M3, 0.0.0.15): the
  conservative rule of the M3 brief (`CL_SplitStufftext`, design §8): only
  per-player / forwarded commands and this player's shadow cvars run, new
  cvars only for sv_force_reconnect's junk names, the rest is ignored with
  a console note; `password` is not a shadowed cvar and is ignored too
  (the extras send player 1's). Default: keep; add commands when the
  maintainer's M3-T3 notes show a legitimate one being skipped.
- Rejoin after a lost server (new, M3): when a remote server drops player 1
  by an error (quit, crash, timeout, kick) the extras are remembered for 10
  minutes and rejoin with their profiles once player 1 is back on that
  address (`reconnect`); pads re-attach on any button. Player 1's own
  `disconnect`, another server or a local game forgets them. Default: keep.
- Cell notice (new, M3): a refused / kicked / timed-out extra keeps its cell
  10 s with the reason (Q3 R11); the slot can be taken again at once.
  Default: keep.
- `sv_split_localexempt` (M3 brief fallback) not built: the LAN-address
  rig worked and a dedicated server already treats 127/8 as remote.
  Default: not needed.
- Extras' non-chat server prints (kill messages, "entered the game") go
  to that player's chat HUD, nowhere on the shared console. Re-judged with
  cells drawn (T4): reads well per cell. Default: keep.
- Our listen server never times out a 127/8 client (design §4.3, same as
  loopback); since 0.0.0.8 a kicked/dropped one's zombie slot is still freed
  after `sv_zombietime`. Default: keep (they live in the same process).
- Per-player field of view: Action pins `ps.fov` at 90 for everyone, so
  `p<N>_fov` (and P1's `fov`) has no visible effect. Default: leave it
  (same as upstream for P1); a client-side per-cell FOV override would be
  a new feature and differ from what servers expect.
- `p<N>_*` shadows and `pbind 2..8` are no longer archived (M2-T5): guests
  are session-only, named players live in their profile files. Default:
  keep.
- Cvar-sync shadows (`p2_cl_xerp`, ...) are not pre-created or archived;
  they exist once set (`set p2_cl_xerp 0` or `p2 cl_xerp 0`). Default:
  keep until M2 profiles decide which settings are per player.
- Sound dedupe window 40 ms (entity) / 50 ms (local): over the rig's
  dedicated server every copy arrived 0 ms after the first (all of a
  server frame's packets come together), 93 of 93 merged. Default: keep;
  re-check only if the maintainer hears doubles on a real server (jitter
  over the internet is per packet, but the copies travel together).
- Ricochet roll (0.0.0.13): with extras in the game tent.c's ricochet roll
  is a hash of the impact point (so every connection agrees and the copies
  merge); the same exact point always gives the same result. Default:
  keep (upstream `Q_rand()` with one player).
- Profile join picker (M2-T5): Q3 §12.3 shows a profile picker before a
  pad joins; here the pad's remembered profile loads directly (else guest)
  and others are picked from the overlay's Profile page. Default: keep (two
  presses fewer); add the picker if the maintainer misses it.
- Profile identity (M2-T5): the profile name is its file name; renaming
  yourself in game changes only the saved `name` (Q3 renames the profile
  file). Pads are remembered by device key `guid#n` (Q3: the bare GUID), so
  two identical pads keep separate profiles, but swapping their plug order
  swaps them. Default: keep both.
- Player 1 and profiles (0.0.0.13): P1's pad brings the profile it
  remembers (Q3 §14.3) and takes it away when the pad steps out; a profile
  picked by hand for P1 stays until `profile guest 1`. Default: keep.
- Profile switches vs the server's name limit (found 0.0.0.13): a profile
  change is a name change; q2pro servers allow 5 per minute
  (`sv_namechange_limit`, Action servers likewise) and stuff the old name
  back, which then is what the profile / P1's kept settings save.
  Default: leave it (only rapid switching hits it); revisit at M3 (e.g.
  never save a name the server refused).
- Sound backends (M2-T6): nearest listener + loop merge on DirectSound
  (default, `s_enable 1`); OpenAL (`s_enable 2`) keeps P1's listener.
  Default: keep; no automatic backend switch with N > 1.

## Live claims

(none)
