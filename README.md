AQtion splitscreen
==================

Local splitscreen for [AQtion](https://store.steampowered.com/app/1978800/AQtion/)
(Action Quake 2): up to 8 players on one PC and one screen, each with their own
gamepad, view, HUD, scoreboard, menus and chat. Pads join by holding A, the
screen splits automatically, and every player is a normal player to the server,
so it works with the unmodified Action game on your own game and on regular
AQ2 / AQtion servers.

It is a fork of the AQtion client engine
([actionquake/q2pro](https://github.com/actionquake/q2pro), itself based on
[Q2PRO](https://github.com/skullernet/q2pro)). It installs **next to** your
Steam AQtion: nothing in the game is replaced, and Steam's AQtion keeps working
as before.

Install
-------

You need AQtion from Steam (installed) and Windows 10/11 x64, or Linux x86_64
(Steam Deck included).

Each release archive holds two files: the program (`q2pro-splitscreen.exe` on
Windows, `q2pro-splitscreen` on Linux) and a short `README.txt`.

**Windows**

1. Put `q2pro-splitscreen.exe` **in your AQtion folder** (Steam: right-click
   AQtion -> Manage -> Browse local files) and start it (double-click, or a
   desktop shortcut to it). Your settings live in the `splitscreen\` folder it
   creates there.
2. The first start is fullscreen at your desktop resolution. Windows may ask
   whether `q2pro-splitscreen.exe` may use the network: allow it for online
   play and for hosting. Extra options go into the shortcut's target after
   the exe, e.g. `"...\q2pro-splitscreen.exe" +set vid_fullscreen 0` or
   `+connect <server>`.

**Linux / Steam Deck**

Unpack the tar.gz and put `q2pro-splitscreen` in your AQtion folder, next to
Steam's `q2pro` (Steam: right-click AQtion -> Manage -> Browse local files;
`chmod +x q2pro-splitscreen` if needed). Run it from there, or add it to Steam
(Desktop mode: Games -> Add a Non-Steam Game; on a Steam Deck it then shows
up in Game mode and Steam Input handles the pads). It uses Steam's own game
files and keeps its settings in `AQtion/splitscreen/`. The Linux binary is
built on Ubuntu 22.04 and runs on SteamOS and any distribution with glibc
2.35 or newer, using the system's SDL2, OpenGL and Wayland or X11.

The exe also works from a folder of its own inside AQtion (e.g.
`<AQtion>\splitscreen-dev\q2pro-splitscreen.exe`, one level below `baseaq\`
and `action\`); it then keeps its settings in a `home\` folder beside itself.
That is the whole install. (Developers: `scripts\release\install.ps1` in this
repo installs into such a subfolder with a SHA-256 manifest and
`-Uninstall` / `-Purge`; players do not need it.)

**Where your settings are:** everything the exe writes goes to one folder
(unless you give `+set homedir ...`): `<AQtion>\splitscreen\` when the exe is
in the AQtion folder, or `home\` beside the exe when it is in a subfolder.
In there: the config (`action\q2config.cfg`), profiles (`action\profiles\`),
screenshots (`action\screenshots\`), demos, logs and the console history
(`baseaq\.conhistory`). Nothing is written into Steam's `<AQtion>\action\` or
`<AQtion>\baseaq\`, and this build does not use Steam Cloud, so it never
shares or changes the config of Steam's AQtion. Back up that folder to keep
your profiles.

**Upgrade:** replace the exe; your settings stay.
**Uninstall:** delete `q2pro-splitscreen.exe` and `<AQtion>\splitscreen\`
(keep it first if you want your profiles).

How to play
-----------

- **Player 1** is the first pad that presses a button (keyboard and mouse
  work too). With `cl_splitP1Input kbm` the keyboard is player 1 and the first
  pad joins as player 2.
- **Start a game:** at the main menu press **Start on any pad** (or **F1**):
  the Splitscreen page opens. Its first row, **Quick Local Match**, starts a
  local game with your last server options on a random map at once.
  Otherwise the page has layout, max players, game type, map, **Server
  options** for time limit, bots, weapons, bans and more, and "Start
  multiplayer game". AQtion's own menus work too; any multiplayer game
  (`deathmatch 1`) can take extra players. AQtion's own menus are drawn twice
  their usual size (Start -> Settings -> AQtion menu size, per player; in a
  split view they keep that size as long as they fit the view).
- **Names:** nobody is asked for a name at start; players are "Player 1",
  "Player 2", ... until they set one (Start -> Settings -> Name with the
  on-screen keyboard, or `name` in the console) and save a profile.
- **Join:** in a game, **hold A** (0.7 s) on a free pad; the screen splits and
  the new player appears. "Player N: hold A to join" shows when a free pad is
  used.
- **Leave:** hold **Back + Start** (2 s).
- **Start** opens that player's own menu in their view: **Team** (join a
  team or spectate), **Weapons** (Action's weapon and item menu; shown only
  when the game lets you choose), Scoreboard, Radio, **Settings** (name with
  an on-screen keyboard, gun hand, and **Controls**: look speeds, invert, aim
  curve, dead zones, trigger point, and a **Buttons** page to change what each
  pad button does; plus crosshair, crosshair size and colour, text size, HUD
  opacity and both menu sizes, each for that player's view only),
  **Profile: Guest** (or your profile's name), Leave game. Player 1's menu also has
  **Server options** (only in a game you host: time / frag / round limits,
  bots, friendly fire, weapon choice in deathmatch, all weapons / items,
  **Weapons** (All, or one weapon - MK23, dual pistols, MP5, M4, M3,
  handcannon, sniper, knife - that everyone spawns with), weapon and item
  bans, darkmatch, announcements, ... with "Restart map" and
  "Change map", **Infinite ammo**, which turns cheats on for your game and
  refills your local players' guns, and **Humor sounds** On / Off: AQtion's
  farts, laughs and quotes that a player who stands still plays every 15 s),
  Splitscreen settings and Game menu.
  **Back** shows the scoreboard.
- **Default buttons:** A jump, B crouch, X reload, Y bandage, RT fire, LT
  sniper zoom (1x, 2x, 4x, 6x, then back to 1x), LB zoom back to 1x, RB
  next weapon, R3 knife, L3 walk, d-pad drop item / drop weapon / IR vision
  / weapon mode, back paddles radio.
  Change them per player in Start -> Settings -> Controls -> Buttons (or
  `pbind <player> PAD_<button> <command>` in the console).
- **Profiles:** Start -> Profile -> "Save as new profile..." keeps a player's
  name, gun hand, look settings and pad buttons. A pad remembers its profile
  and brings it back the next time it joins; players without one are guests.
- **Layouts:** 2 players top/bottom (side by side: "Two players" on the
  settings page), 3 = 2 + 1 wide, 4 = 2x2, 5 = 3 + 2, 6 = 3x2, 7 = 4 + 3,
  8 = 4x2. "Cell layout" = Equal cells (`cl_splitFill 0`) gives same-size
  views with black gaps instead (3x3 for 7-8 players, larger text in a 720p
  window); "Wide cell for" picks who gets the wider bottom view.
- **Online:** join a server as player 1 the normal way, then hold A on more
  pads: each extra player connects to the same server on its own connection.
  Most servers allow **3 players from one IP** (`sv_iplimit 3`); a 4th is
  refused with that reason in its view. Servers that require an anticheat
  client will likely refuse the extra players. Extra players follow map
  changes; if the server restarts, `reconnect` as player 1 brings them back.

Known limitations
-----------------

- With 8 players in a 1280x720 window each view is 320x360 and its text is
  too small to read; use a bigger window / TV, or Equal cells.
- Players 5-8 were tested with virtual pads only so far.
- When several players join at the same moment, one may spawn on top of
  another and telefrag it (Action's spawn choice).
- Hosting uses Steam's own Action game DLL (TNG 2.82): bots only run maps
  that AQtion ships route files for, and Server options offers only what
  that DLL has (no Espionage). A bot game on the map `beer` never finishes
  loading (an AQtion bug).
- No Steam / Discord integration (achievements, Steam Cloud config, rich presence).

Build from source
-----------------

Windows: Visual Studio 2019 (or 2022) with the C++ x64 tools, Python 3 with
Meson (`python -m pip install --user meson`) and Ninja. Then
`scripts\build.ps1` (output in `build\`) and `scripts\package.ps1` for the
release zip (`build\aqtion-splitscreen-<VERSION>-windows-x86_64.zip`).
Linux: `scripts/build.sh` and `scripts/package.sh` (tar.gz); the package list
is in docs/BUILD-AND-PROVENANCE.md Recipe L. GitHub releases are built by
`.github/workflows/release.yml` on a `v<VERSION>` tag. Details, the exact
upstream base and paths are in
[docs/BUILD-AND-PROVENANCE.md](docs/BUILD-AND-PROVENANCE.md); the design is
in [docs/SPLITSCREEN-DESIGN.md](docs/SPLITSCREEN-DESIGN.md). Upstream's
generic build notes are in [INSTALL.md](INSTALL.md) and the client / server
manuals in `doc/`.

Credits and licence
-------------------

Q2PRO by Andrey Nazarov and contributors, the AQtion engine and the Action
game (TNG) by the AQtion team and the Action Quake 2 community, Quake II by
id Software. This fork adds the splitscreen and gamepad code. Licensed under
the GNU General Public License v2.0 or later, like Q2PRO: see
[LICENSE](LICENSE). The game data (maps, models, sounds) is not part of this
project; it comes from your AQtion install.
