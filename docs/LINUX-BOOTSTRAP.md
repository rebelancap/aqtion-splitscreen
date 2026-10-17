# Linux / Steam Deck bootstrap — brief for the Linux test box orchestrator

Written 2026-10-16 on the Windows dev box (Windows lead session). Read this
first, then `CLAUDE.md` (project charter), `STATUS.md` (Current state, Next
steps, Open questions), `docs/BUILD-AND-PROVENANCE.md` (how the Windows
build is made and tested; §2 Recipe B already mentions this box) and
`docs/SPLITSCREEN-DESIGN.md`. The program-wide rules for this box are
`~/dev/CLAUDE.md` here (the Linux counterpart of the Windows one; written
for the Quake3e fork, they apply unchanged). The Quake3e fork's
`docs/LINUX-BOOTSTRAP.md` in `~/dev/Quake3e-splitscreen` is the sister
brief; its rounds L1–L6 are the pattern, this project needs far less.

## What this project is

AQtion (Action Quake 2 on Steam) is a Q2PRO engine (`q2pro`) plus the TNG
game library (`action/gamex86_64.so` / `.dll`, TNG 2.82, legacy game API
4). This repo is a Q2PRO fork adding local splitscreen (2–8 players, one
exe, one process) and gamepads, the same product as the maintainer's
`Quake3e-splitscreen`. **Release shape:** one executable + an 8-line
`README.txt`, dropped straight into the Steam AQtion folder; no installer,
no launcher, no game library of ours. The exe finds its placement itself
(`split_self_locate()`), uses **Steam's own game library** through an
API-4 adapter in `src/server/game.c`, and writes only into its own home
folder, never into Steam's `action/` or `baseaq/`.

Windows state: 0.0.0.31 is the release candidate (see STATUS.md). Nothing
is on GitHub yet; the maintainer wants one release with a Windows zip and a
Linux tar.gz, like Quake3e-splitscreen 1.0.0, so the Linux build is the
last step before the first public release.

## Where things are on this box

- **Repo:** `~/dev/aqtion-splitscreen`, full history, branch `splitscreen`
  = the Windows dev box's `splitscreen` (0.0.0.31 + this brief). Branch
  `linux` is created from it for your work (see Sync model). No GitHub
  remote; `upstream` (actionquake/q2pro) is not configured here.
- **Game data:** Steam's own AQtion install, on the SD-card library
  (`steamapps/common/AQtion` under the SD-card Steam library; find it in
  `~/.local/share/Steam/steamapps/libraryfolders.vdf`). Export it as
  `AQ_BASEPATH` in your shell and scripts; never write a literal path into
  the repo. It is **read-only for us** except the one folder our exe
  creates (`<AQtion>/splitscreen/`), and in agent runs you always pass
  `+set homedir <repo>/work/aqhome` (or another folder under `work/`) so
  nothing lands in the install. Its `action/gamex86_64.so` is
  byte-identical to the Windows install's copy (SHA-256 `483a10ab0def…`),
  so everything verified against Steam's DLL on Windows applies here.
  Steam's launcher there is `launch.sh` → `./aqtion q2pro` with
  `LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/` (a libcurl compat shim for
  Steam's Ubuntu-built binary; ours links the host libcurl and needs none).
  Known: the game library rewrites `<AQtion>/items.txt` and re-saves
  `action/terrain/*.ltk` on shutdown; Steam "Verify integrity" restores
  them. Back up those files before the first run (`work/`), compare after.
- **Toolchain:** Bazzite is immutable; build inside a distrobox, never
  layer packages on the host. The `q3dev` box (Fedora 43) exists from the
  Quake3e rounds; make a new `aqdev` or reuse it (q2pro needs: gcc, meson,
  ninja-build, pkgconf, SDL2-devel, libcurl-devel, zlib-devel, libjpeg-
  turbo-devel, libpng-devel, openal-soft-devel, libogg/libvorbis-devel,
  wayland-devel + wayland-protocols-devel, libX11/libXi-devel, mesa-libGL-
  devel, jansson-devel; adjust to what `meson setup` asks for). The built
  binary must run **on the host** (check `ldd`).
- **Claude Code** is `~/.local/bin/claude`. The desktop session is KDE on
  Wayland (XWayland on `DISPLAY=:0`); an SSH session has no display
  variables — see the Quake3e brief for the export line. Prefer running
  from Konsole on the box.

## Sync model between the two boxes (no GitHub yet)

- The Windows dev box has this repo as a remote and **pushes into it**
  (`receive.denyCurrentBranch = updateInstead`). A push onto the
  checked-out branch fails if that branch is dirty, so:
- **Work on branch `linux` here, never commit on `splitscreen` here.**
  Keep `splitscreen` clean so the Windows box can keep pushing Windows
  rounds into it; `git merge splitscreen` into `linux` when one lands.
- The Windows box fetches your work (`git fetch <linux-box> linux`) and
  merges it into `splitscreen` there. Tell the maintainer at the end of
  each round what is on `linux` and whether it is ready to merge.
- Dev versions share the `0.0.0.N` counter (`VERSION`): bump by one per
  Linux round; a clash is re-numbered at the merge on Windows.

## Expected porting work (code survey, 2026-10-16)

Our code under `src/client/split/` (`split.c`, `split_view.c`,
`split_snd.c`, `split_menu.c`, `split_profile.c`, `in_gamepad.c`) and the
API-4 adapter in `src/server/game.c` have **no `_WIN32` references**. The
work is:

1. **Self-locate on Linux** — port `split_self_locate()` from
   `src/windows/system.c` (read its 40-line comment and the function) into
   `src/unix/system.c`, called first thing in `Sys_Init` before the
   `basedir`/`libdir`/`homedir` cvars are created. Same two placements by
   the folders beside the exe (`baseaq/` + `action/` → AQtion root; parent
   has them → subfolder), with `/proc/self/exe` (`readlink`) and
   `stat`/`mkdir` instead of the Win32 calls. Root placement: basedir =
   libdir = that folder, homedir `<AQtion>/splitscreen` (created), game
   `action`, `chdir` to the root (the game library opens
   `action/tng/<map>.aqg`, `action/terrain/*.ltk`, `action/bots/`, vote
   lists relative to the cwd). Subfolder placement: basedir/libdir =
   parent, homedir `<exe dir>/home`, `sys_forcegamelib` =
   `<exe dir>/gamex86_64.so` only if present. `+set` on the command line
   always wins; anywhere else (the repo's `build/`) nothing changes. Mind
   that upstream's unix `Sys_Init` has its own `~`-expanding homedir logic
   and a `system-wide`/`homedir` meson option; `-Dsystem-wide=false` and
   an empty default `homedir` for our build. Print the same one-line
   location note (`Sys_PrintLocationNote`).
2. **Game library name:** `SV_InitGameProgs` / `Sys_LoadLibrary` look for
   `game` + CPU string + `.so` → `gamex86_64.so` on x86_64, which is what
   Steam ships. Confirm the API-4 adapter path (`LEGACY AQTION GAME API 4`
   comment block in `src/server/game.c`) runs the same for the `.so`:
   log line "G_CheckForExtension" and a bot game in `+map` proves it.
3. **Gamepads:** `meson_options.txt` has `gamepad` (`USE_GAMEPAD`,
   `src/client/split/in_gamepad.c`). On Linux `-Dsdl2=enabled` is normal
   (upstream's Linux CI does), and `meson.build` then **reuses the
   engine's SDL2** for the pads instead of its own static SDL. Audit the
   init/shutdown: `PAD_Init` must not re-init or `SDL_Quit` the engine's
   SDL, only `SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)` and
   `SDL_QuitSubSystem`. Agent runs always `+set pad_enable 0` (never the
   real pads); `padinject` drives virtual pads.
4. **Video:** the splitscreen renderer is the GL path (`src/refresh/`,
   FBO per player); Wayland and X11 (`vid_driver`) both exist upstream.
   Our Windows-only pieces: `vid_noactivate` (agent test mode) in
   `src/windows/client.c`; not needed here (the desktop is free) —
   windowed runs at 1280x720 like the Quake3e L rounds, `+set
   vid_fullscreen 0`.
5. **Scripts:** `scripts/build.ps1`, `package.ps1`, `run.ps1`,
   `run-ded.ps1` are PowerShell. Write `scripts/build.sh` (distrobox
   re-entry, meson setup with the options `build.ps1` uses —
   `-Daqtion-build=true -Dgamepad=enabled -Dsystem-wide=false`, plus
   `-Ddiscord-sdk=false -Davcodec=disabled`), `scripts/package.sh`
   (tar.gz of `q2pro-splitscreen` + `README.txt`, same README as the zip)
   and `scripts/run.sh` (the `run.ps1` flags you need: homedir under
   `work/`, `+set pad_enable 0`, `+set allow_download 0`, `logfile 3`,
   timeout, `pgrep` guard). Keep `.cfg` test scripts portable.
6. **Case sensitivity:** the game data was made on Windows. Check that
   upstream's unix filesystem code already handles mixed-case pak/file
   names (Steam's own Linux AQtion runs on it, so this should be fine) and
   that nothing of ours builds paths with backslashes (`grep '\\\\'
   src/client/split`).
7. **CI:** the repo's `.github/workflows/release.yml` is still upstream's
   (it checks out `actionquake/q2pro`). The Windows lead replaces it with
   one modelled on Quake3e-splitscreen's (tag = `VERSION`, Windows zip +
   Linux tar.gz, Linux built in an Ubuntu 22.04 container for Steam
   Deck's glibc). Your `build.sh` option list is the input for that; put
   it in STATUS.md.

## Rounds (one executor at a time; Opus executes, Sonnet reviews lightly)

- **L1 — builds, locates itself, boots a bot game.** distrobox + meson;
  self-locate port; `scripts/build.sh`/`run.sh`. Run from a **test copy
  placement**: never put the exe into the Steam install for routine runs;
  use `+set basedir $AQ_BASEPATH +set libdir $AQ_BASEPATH +set homedir
  <repo>/work/aqhome +set game action` from `build/`, and once per round
  the real thing: copy the exe as `q2pro-splitscreen-test` into
  `$AQ_BASEPATH`, run it with `+set homedir <repo>/work/steamtest-home`
  (self-locate then only has to pick basedir/libdir/cwd), delete it after,
  verify `items.txt`/`.ltk` hashes. Evidence: the location note, the
  `G_CheckForExtension` line, `+map teamjungle` with `am 1 am_botcount 4`
  running 60 s, engine `screenshot` into `work/`. Done = 0 new compiler
  warnings in our files, bots play, nothing written outside `work/`.
- **L2 — splitscreen and pads.** `splitplayers`/join with `padinject`
  virtual pads (2 and 4 players, layouts), the Start menu (hold-to-scroll
  from 0.0.0.31), a profile save/load, Server options → Quick Local Match,
  Infinite ammo and a weapon preset against the `.so` (same expectations
  as STATUS.md Last round for 0.0.0.28–30). Pads through the engine's SDL.
- **L3 — Steam Deck shape.** Launch as a non-Steam game under Gamescope
  (1280x800), Steam Input pads: the maintainer's hands-on check; you leave
  the build installed in a subfolder of the install (`<AQtion>/
  splitscreen-test/`) and the checklist in STATUS.md. Then the tar.gz
  and `README.txt` wording for Linux ("put `q2pro-splitscreen` next to
  `q2pro` in your AQtion folder, `chmod +x`, add it to Steam as a
  non-Steam game or run it").

Each round: fresh executor with a self-contained brief; STATUS.md five
sections updated; Live claims emptied; commit on `linux` as
`rebelancap <rebelancap@protonmail.com>` with the Co-Authored-By line;
no personal names, user paths or host names in files or messages (write
`<repo>`, `<AQtion install>`, `$AQ_BASEPATH`, "the maintainer", "the
Linux test box"). Never change display settings, never run two game
instances, never kill a process you did not start, check `pgrep` for any
game before opening a window.

## Open from the Windows side (defaults in STATUS.md)

- Release number and the GitHub repo are the maintainer's; the README
  guesses `https://github.com/rebelancap/aqtion-splitscreen`.
- `README.txt` covers Windows and Linux / Steam Deck since 0.0.0.34 (L3).
