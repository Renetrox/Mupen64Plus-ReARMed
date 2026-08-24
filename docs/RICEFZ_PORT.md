# RiceFZ experimental port

Goal: integrate the Rice renderer from `fzurita/mupen64plus-ae` (`fz-master`) as a separate renderer without replacing the existing libretro Rice port.

## Identity

- Existing renderer: `rice`
- Experimental renderer: `ricefz`
- Existing `gles2rice/` source tree remains untouched.
- FZ behavior and per-game configuration are the reference.

## Architecture

RiceFZ is being built as a **separate Mupen64Plus video-plugin shared library**:

```text
parallel_n64_libretro.so
        |
        | explicit ReARMed core-API bridge
        v
mupen64plus-video-rice-fz.so
```

This avoids duplicate global symbols between the libretro Rice fork and the FZ Rice source tree. It also preserves the original Mupen64Plus plugin boundary, which should make the same approach reusable later for gln64FZ and Glide64mk2FZ.

The libretro core intentionally hides its embedded Mupen64Plus symbols via `libretro/link.T`, so RiceFZ cannot use normal `dlsym(CoreLibHandle, ...)`. Instead ReARMed passes a small `fz_plugin_core_bridge` as `CoreLibHandle`; RiceFZ's replacement `osal_dynamiclib_rearmed.c` resolves only the core API functions the plugin asks for.

The bridge also provides `VidExt_GL_GetAttribute`, which FZ Rice expects but the current libretro video-extension shim does not implement.

## Current milestones

- [x] Create isolated `ricefz-experimental` branch.
- [x] Add importer for the exact FZ `fz-master` Rice source and `RiceVideoLinux.ini`.
- [x] Add a standalone sidecar Makefile targeting `mupen64plus-video-rice-fz.so`.
- [x] Add the explicit core-API bridge and RiceFZ resolver adapter.
- [ ] Wire the bridge source into the main ReARMed build.
- [ ] Add the runtime `dlopen` loader for RiceFZ.
- [ ] Add `GFX_RICE_FZ` routing while preserving `GFX_RICE`.
- [ ] Add `parallel-n64-gfxplugin = "ricefz"` / `Rice FZ (Experimental)`.
- [ ] Make `RiceVideoLinux.ini` available through the ReARMed shared-data path.
- [ ] Load a ROM, create the GLES2 context, render, and unload cleanly.
- [ ] Only after basic rendering works, expose FZ-specific options through libretro.

## First local compile probe

```bash
git switch ricefz-experimental
bash tools/import-rice-fz.sh
make -C mupen64plus-video-rice-fz clean
make -C mupen64plus-video-rice-fz info
make -C mupen64plus-video-rice-fz -j$(nproc)
```

The first build is intentionally a probe. Compiler errors will tell us which assumptions from FZ's Android build need adaptation on Linux/GLES2 before the renderer is connected to RetroArch.

No existing renderer is removed or replaced during this experiment.
