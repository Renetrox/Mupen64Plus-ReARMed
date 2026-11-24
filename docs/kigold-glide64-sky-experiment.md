# Glide64: KI Gold sky experiment

This branch starts from `frameskip-modern` at f204e53 and does not include the
separate GLES2N64 experiment.

## Evidence and scope

Glide64 already labels color-combiner key `0x6134ab50` as **sky, Killer Instinct**
in `glide2gl/src/Glide64/Combine.c`. The key uses the exact same packing as
`Combine()`: `(rdp.cycle1 << 16) | (rdp.cycle2 & 0xffff)`.
The recompilation's [RT64 patch](https://github.com/geno55/kigold-recomp/blob/main/patches/rt64/0001-rdp-triangle-clipping.patch)
also documents CPU-built raw RDP sky geometry, motivating interception of raw
triangles rather than text/sprite texture rectangles.

The new option `parallel-n64-glide64-kigold-skip-sky` defaults to disabled.
When enabled it skips only raw shaded, textured, non-Z triangles in two-cycle
mode, with the KI game hack active and the documented sky color key. The early
return happens before geometry reconstruction, texture-cache setup and drawing.
It does not alter command-list advancement, interrupts or CPU/RSP execution.

This is an experiment based on an existing code annotation, not a command trace
captured from a tested fight. It may miss other sky modes or match another effect
that reuses this combiner. No blanket removal of scenery is implemented.

KI Gold normally disables frame clearing. The experiment enables it to prevent
old sky pixels remaining behind moving geometry. This also clears depth and may
have a cost or affect framebuffer-dependent effects; measure the net result.
The frame-clear override applies only while this option is enabled for KI Gold.

## Device test

Build this branch using your normal working RetroPie settings; keep the installed
core as a backup. In RetroArch select Glide64 and leave the option disabled first.
Check title, selection, full combat, victory and return to menu. Then enable
**Glide64: KI Gold skip sky (experimental)** and restart content.

Compare identical fighters/stages and settings. Check sky, floor, characters,
shadows, health bars, timer, menu letters, smoke and projectiles. Try training and
several stages; record ROM revision and RSP plugin. Measure audio stability and
frame times with identical resolution/frameskip. Disable and restart to restore
normal behavior if anything besides the intended sky disappears. Another game
must behave identically with the option on and off.

## Automated checks

```
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined tests/glide64_kigold_sky.c -o /tmp/kigold-sky-test
ASAN_OPTIONS=detect_leaks=0 /tmp/kigold-sky-test
cc -std=c99 -DHAVE_GLIDE64 -DHAVE_OPENGLES -Ilibretro -Ilibretro-common/include -fsanitize=address,undefined tests/glide64_kigold_options.c -o /tmp/kigold-options-test
ASAN_OPTIONS=detect_leaks=0 /tmp/kigold-options-test
```

The tests check exclusion gates, adjacent non-sky color keys, alpha-field packing,
option category/default, option count and the terminating allocation. Disabling
leak detection is needed in the ptrace-based execution environment. These tests
do not establish visual correctness or a speed improvement.

Validation on this branch: both sanitizer tests passed; `git diff --check`
passed. The affected `glide64_gDP.c`, `Glide64_Ini.c` and `libretro.c` objects
compiled successfully using the project's Makefile in GLES2/Glide64 mode and
unmodified Khronos GLES2 headers. No full linked core or ARM64 binary was built;
no ROM/device test was possible. The pre-existing CORE_NAME redefinition warning
is still present.

## Diagnostic update after no visible change

The cycle gate now reads `g_gdp.other_modes.cycle_type`, written directly by
raw RDP SetOtherModes. The trace also records the legacy `gDP` mirror to check
whether the values diverge on the device; this is not a confirmed root cause.
Set `REARMED_KIGOLD_SKY_TRACE=1` to log option/game detection and up to 128 unique
raw-triangle signatures (color key, both cycle modes, shading/texture/depth,
option and skip decisions). No broader geometry filter is enabled. Enter a fight
for a few seconds, quit, and collect the `[KI-SKY]` lines. Diagnostic logging can
affect timing, so do not use this run to measure performance.
