# RiceFZ experimental port

Goal: integrate the Rice renderer from `fzurita/mupen64plus-ae` (`fz-master`) as a separate renderer without replacing the existing libretro Rice port.

Planned identity:

- Existing renderer: `rice`
- Experimental renderer: `ricefz`
- Existing source tree remains untouched.
- FZ behavior and per-game configuration are the reference.

Initial milestones:

1. Vendor the FZ Rice GLES2 source into `mupen64plus-video-rice-fz/`.
2. Compile it as a distinct internal video plugin.
3. Add `GFX_RICE_FZ` routing while preserving `GFX_RICE`.
4. Add `parallel-n64-gfxplugin = "ricefz"` / `Rice FZ (Experimental)`.
5. Keep FZ's `RiceVideoLinux.ini` and per-ROM logic intact where practical.
6. Only after basic rendering works, expose FZ-specific options through libretro.

The first test target is simply: build, load a ROM, create a GLES2 context, render, and unload cleanly. No existing renderer should be removed during this experiment.
