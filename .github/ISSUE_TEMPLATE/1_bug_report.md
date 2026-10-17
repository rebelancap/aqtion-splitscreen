---
name: 'Report a bug'
about: 'Create a bug report'
title: ''
labels: ''
assignees: ''

---

### Before reporting bugs

Make sure the bug happens with the latest aqtion-splitscreen release (the
exe / binary from the Releases page, placed in your AQtion folder). If it
also happens with Steam's own AQtion (`q2pro`), report it upstream at
actionquake/q2pro instead.

### Important information

Provide the following information:
- aqtion-splitscreen version (the `version` console command)
- Number of local players, pads in use (make / model), screen layout
- OS version
- GPU driver and version

For Linux:
- Linux distribution and version
- Window manager version

### Reproduction steps

Steps to reproduce the behavior.

### Expected behavior

A clear and concise description of what you expected to happen.

### Actual behavior

A clear and concise description of what actually happened.

### Screenshots

If reporting graphics glitches, provide screenshot or video.

### Log file

Provide a link to the log file created by launching `q2pro-splitscreen +set developer 1
+set logfile 1` (it is written to the `splitscreen` folder in your AQtion folder).

### Crash reports

If the game crashes, provide a crash report (Windows) or a backtrace (Linux). On
Linux, backtrace can be created by launching it with `gdb q2pro-splitscreen --args
[...]` and typing `bt` after the crash.

### Compilation issues

If reporting a building / compilation issue, provide `meson setup` command
line and full console output.
