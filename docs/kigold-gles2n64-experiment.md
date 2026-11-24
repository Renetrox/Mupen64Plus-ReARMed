# KI Gold / GLES2N64 experiment

This branch fixes the raw RDP triangle left-major flag: bit 23, not opcode bit 27.
It also adds `parallel-n64-gln64-kigold-skip-sky`, disabled by default and applied
at content startup. It affects only the legacy GLES2N64 renderer and ROMs whose
internal name is Killer Instinct Gold (case-insensitive).

The sky filter is a hypothesis based on the rectangle-shaped raw triangle
explained in [kigold-recomp's triangle patch](https://github.com/geno55/kigold-recomp/blob/main/patches/rt64/0001-rdp-triangle-clipping.patch).
It requires a shaded/textured non-Z RDP triangle, YM == YL, vertical H/M edges,
width >= 128 native pixels and height >= 32 pixels. These size thresholds are
heuristic, not values verified from a KI Gold command capture. Ordinary texture
rectangles never enter this filter. Other raw geometry can still match; some
skies may not. Do not treat this as a complete scene classifier.

## Validation performed

- Synthetic command tests: option off, other games, depth triangles, texture
  rectangles, sloped/non-rectangular and small geometry are excluded.
- Core option table registration: exactly one experimental option in GLES2N64,
  disabled by default; allocation and terminator checked under ASan/UBSan
  (leak detection disabled because the execution environment uses ptrace).
- Full renderer object compilation attempted but blocked by missing system
  `GLES2/gl2.h`. No binary, ROM, visual or performance validation performed.

## Device test

1. Build this branch using the existing working RetroPie build settings. Keep a
   copy of the installed core and existing per-game options before replacing it.
2. Select GLES2N64 and leave the new option disabled. Restart KI Gold and check
   title, character selection, a full fight and return to menu. This isolates
   the orientation correction. Record ROM revision, RSP plugin and core settings.
3. Enable **KI Gold skip sky (experimental)** under GLES2N64 options and restart.
   Compare the same character/stage, checking sky, fighters, floor, life bars,
   timer, text, projectiles and smoke. Keep resolution/frameskip identical.
4. Measure frame times/audio stability in both modes. Test training plus several
   stages. Disable the option if scenery besides the sky disappears or if the
   expected sky does not match. Such results require a captured RDP list before
   widening the predicate; do not skip all raw triangles.
5. Regression-test another raw-RDP game with the orientation fix (e.g. SCARS).

No claims are made that this resolves the black screen or improves performance.
No RT64 shader conversion, floor clipping, or expensive sky restoration was ported.
