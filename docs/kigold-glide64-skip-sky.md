# KI Gold: experimental sky omission in Glide64

RetroArch: Core Options > Glide64 > **KI Gold skip sky (experimental)**.
Key: `parallel-n64-glide64-kigold-skip-sky`. Default: disabled. Restart content
after changing it. Save a per-game option override if desired.

The option omits raw shaded/textured, non-Z RDP triangles for these color keys:

| RDP mode | Color key | Evidence |
| --- | --- | --- |
| Two-cycle | 6134ab50 | Glide64's Combine.c labels it "sky, Killer Instinct"; user trace confirmed skip=1. |
| One-cycle | eb31eb31 | Remaining raw group in the user's trace; temporary isolation was reported to work after discussion of grey clouds in Sabrewulf/Spinal. |

The user approved keeping this experimental in the primary branch after the
visual test. The report did not enumerate every stage, effect or ROM revision.
A color key is not a unique texture identifier: other matching scenery might be
omitted, and other skies might remain. This is an optional visual tradeoff, not
an accuracy correction or a general-purpose background removal feature.

The filter is gated by the existing KI Gold game detection and only intercepts
raw triangle rendering. Texture-rectangle sprite/text/HUD drawing is not changed.
RDP list advancement and CPU/RSP execution continue. The filter runs before
triangle construction and texture setup. Frame clearing is enabled while the
option is on because KI Gold normally assumes the backdrop overwrites old pixels;
this prevents trails but adds work and may affect framebuffer-dependent effects.

Potential benefit: less rendering work when the omitted backdrop is expensive.
No measured FPS gain is claimed; the visual tests were on the user's x86_64 PC.
ARM64/Orange Pi performance and complete visual coverage remain to be tested.

No trace logging or `REARMED_KIGOLD_HIDE_EB31` environment switch is required or
included in this integration. Diagnostic versions remain on the test branch.
Disable the core option and restart to restore normal drawing.

## Validation

Predicate tests cover both signatures/modes, option off, other games, depth
triangles, missing shade/texture, alpha-field packing and neighboring signatures.
Option registration checks the disabled default, Glide64 category and allocation.
Both tests passed under ASan/UBSan (leak detection disabled for ptrace).
`git diff --check` passed. Glide64 renderer/configuration/main and libretro objects
compiled in the project's GLES2 configuration with official Khronos headers.
No full linked core, ARM build or automated ROM test was run here.
