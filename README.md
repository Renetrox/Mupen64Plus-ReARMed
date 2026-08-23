# Mupen64Plus-ReARMed

**Restore what was lost.**

Mupen64Plus-ReARMed is a restoration-focused libretro fork built around a simple idea: **Mupen64Plus already had most of what was needed. The problem was not a lack of features, but the growing distance between the original vision and the practical reality.**

For years, Mupen64Plus and its derivatives promised flexibility: multiple renderers, different accuracy/performance trade-offs, game-specific configuration and the ability to run well on very different kinds of hardware. In practice, that vision became increasingly uneven. The more accurate and modern rendering paths kept moving forward, while several of the fast legacy renderers that made N64 emulation practical on modest hardware lost options, behavior, fixes, maintenance and, in some cases, basic reliability.

That gap between **what the core was supposed to offer** and **what users could actually use** is the problem this project exists to attack.

## What ReARMed means

ReARMed is **not** an attempt to invent a new N64 emulator, and it is not a race to add more features.

The goal is to recover functionality that already existed across the Mupen64Plus family and was later removed, hidden, hardcoded, broken or left behind.

The project focuses on restoring the fast legacy paths as first-class options again, especially:

- **glN64 / gles2n64**
- **Rice**
- **Glide64 / Glide64mk2**
- renderer-specific hacks and compatibility options
- useful per-game configuration
- practical performance controls such as frameskip where historically appropriate
- behavior and options preserved in older Mupen64Plus, Android/FZ and standalone plugin lineages

The intention is not to blindly copy old code. Old implementations are compared against later forks and working references, then restored or adapted where that functionality was lost.

## Vision vs. reality

Accuracy is valuable, but accuracy only helps when the hardware can actually run it.

A renderer that is theoretically superior but unusable on a Raspberry Pi, Orange Pi, handheld or older PC does not replace a fast renderer that once worked well there. Both approaches should be allowed to exist.

Mupen64Plus-ReARMed therefore treats performance, compatibility and configurability as legitimate design goals rather than temporary compromises. Users should be able to choose the trade-off that fits their hardware and their game.

**The project is about making the old promise real again.**

## Project principles

- **Restore before adding.** If useful functionality existed before, recover it before inventing replacements.
- **Preserve choice.** Fast legacy renderers and accurate modern renderers solve different problems.
- **Do not hide useful controls.** Renderer options should be available when they materially affect compatibility or performance.
- **Test changes in isolation.** Restoration should be incremental and evidence-based, not a wholesale rollback.
- **Keep libretro integration useful.** Core Options and per-game overrides are valuable when they expose real renderer capabilities instead of masking them.
- **Modest hardware matters.** ARM SBCs, handhelds and older PCs are part of the target, not an afterthought.

## Status

This project is under active restoration and investigation. Some legacy renderers currently contain regressions or incomplete integrations inherited from the codebase. Compatibility will therefore vary while the original behavior is reconstructed renderer by renderer.

The current work is deliberately conservative: identify what existed, identify what was lost, restore it, test it, and only then move forward.

## Building

To enable a dynarec CPU core, pass the appropriate `WITH_DYNAREC` value to `make`:

```bash
make WITH_DYNAREC=x86
make WITH_DYNAREC=x86_64
make WITH_DYNAREC=arm
make WITH_DYNAREC=aarch64
```

Additional historical build options available in the codebase include:

- `USE_CXD4_NEW` — use the newer CXD4 RSP version verified on Android
- `USE_SSE2NEON` — enable SSE2 vectorized routines on ARMv7+ through SSE2NEON

Examples:

```bash
# Android hardfp with newer CXD4 RSP + NEON + Parallel RDP
ndk-build -j8 USE_SSE2NEON=1 APP_ABI=armeabi-v7a-hard

# Android arm64 with Parallel RDP + dynarec
ndk-build APP_ABI=arm64-v8a
```

## Acknowledgements

Mupen64Plus-ReARMed stands on the work of the Mupen64Plus, libretro, Mupen64Plus AE/FZ, renderer-plugin and emulation communities. Their code and history make this restoration possible.

---

**Mupen64Plus-ReARMed** — restoring the fast, configurable Mupen64Plus that hardware of every class was supposed to have.
