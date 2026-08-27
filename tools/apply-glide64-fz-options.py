#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INI = ROOT / "glide2gl/src/Glide64/Glide64_Ini.c"
OPTS = ROOT / "libretro/libretro_core_options.h"
MARKER = "ReARMed Glide64 FZ options test"

if not INI.exists() or not OPTS.exists():
    raise SystemExit("Run this script from a Mupen64Plus-ReARMed checkout.")

ini = INI.read_text(encoding="utf-8-sig")
opts = OPTS.read_text(encoding="utf-8")

if MARKER in ini or MARKER in opts:
    print("Glide64 FZ options test patch is already applied.")
    raise SystemExit(0)

# Only expose controls whose backing state/bit handling is still present in
# this Glide64 tree. Deliberately excluded for this first pass:
#   filtering          - existing libretro Texture Filtering has different semantics
#   fb_hires/HWFBE     - implementation was removed/ifdef'd out historically
#   hires_buf_clear    - tied to HWFBE
#   fast_crc           - field survives but consumer needs a separate audit
#   use_sts1_only      - no backing field in this tree
#   texture_correction - backing field was removed
#   clip_zmax          - backing field was removed
#   aspect             - conflicts with the current frontend aspect path
#   wrpAnisotropic     - wrapper-side FZ setting, not this renderer state

options = [
    ("fog", "Fog", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("buff-clear", "Buffer Clear", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("swapmode", "Buffer Swap Mode", [("-1", "Game default"), ("0", "VI occurred"), ("1", "Conditional"), ("2", "Mix")]),
    ("lodmode", "LOD Mode", [("-1", "Game default"), ("0", "Disabled"), ("1", "Fast"), ("2", "Precise")]),
    ("fb-smart", "Smart Framebuffer", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("fb-render", "Framebuffer Render", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("fb-crc-mode", "Framebuffer CRC Mode", [("-1", "Game default"), ("0", "Disabled"), ("1", "Fast"), ("2", "Safe")]),
    ("read-back-to-screen", "Read Back to Screen", [("-1", "Game default"), ("0", "Disabled"), ("1", "Mode 1"), ("2", "Mode 2")]),
    ("detect-cpu-write", "Detect CPU Write", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("alt-tex-size", "Alternate Texture Size", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("force-microcheck", "Force Microcode Check", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("force-quad3d", "Force Quad3D", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("optimize-texrect", "Optimize Texrect", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("fb-read-alpha", "Framebuffer Read Alpha", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("force-calc-sphere", "Force Calc Sphere", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("increase-texrect-edge", "Increase Texrect Edge", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("decrease-fillrect-edge", "Decrease Fillrect Edge", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("stipple-mode", "Stipple Mode", [("-1", "Game default"), ("0", "Disabled"), ("1", "Pattern"), ("2", "Rotate")]),
    ("clip-zmin", "Clip Zmin", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("adjust-aspect", "Adjust Aspect", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("correct-viewport", "Correct Viewport", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("zmode-compare-less", "Zmode Compare Less", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("old-style-adither", "Old Style Alpha Dither", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("n64-z-scale", "N64 Z Scale", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("pal230", "PAL230", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("ignore-aux-copy", "Ignore Auxiliary Copy", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("useless-is-useless", "Useless Is Useless", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
    ("fb-read-always", "Framebuffer Read Always", [("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")]),
]

override_targets = [
    ("fog", "settings.fog"),
    ("buff-clear", "settings.buff_clear"),
    ("swapmode", "settings.swapmode"),
    ("lodmode", "settings.lodmode"),
    ("fb-smart", "smart_read"),
    ("fb-render", "depth_render"),
    ("fb-crc-mode", "fb_crc_mode"),
    ("read-back-to-screen", "read_back_to_screen"),
    ("detect-cpu-write", "cpu_write_hack"),
    ("alt-tex-size", "settings.alt_tex_size"),
    ("force-microcheck", "settings.force_microcheck"),
    ("force-quad3d", "settings.force_quad3d"),
    ("optimize-texrect", "optimize_texrect"),
    ("fb-read-alpha", "read_alpha"),
    ("force-calc-sphere", "settings.force_calc_sphere"),
    ("increase-texrect-edge", "settings.increase_texrect_edge"),
    ("decrease-fillrect-edge", "settings.decrease_fillrect_edge"),
    ("stipple-mode", "settings.stipple_mode"),
    ("clip-zmin", "settings.clip_zmin"),
    ("adjust-aspect", "settings.adjust_aspect"),
    ("correct-viewport", "settings.correct_viewport"),
    ("zmode-compare-less", "settings.zmode_compare_less"),
    ("old-style-adither", "settings.old_style_adither"),
    ("n64-z-scale", "settings.n64_z_scale"),
    ("pal230", "settings.pal230"),
    ("ignore-aux-copy", "ignore_aux_copy"),
    ("useless-is-useless", "useless_is_useless"),
    ("fb-read-always", "read_always"),
]


def option_block(suffix, title, values):
    vals = "\n".join(f'            {{ "{v}", "{label}" }},' for v, label in values)
    return f'''    {{
        CORE_NAME "-glide64-{suffix}",
        "Glide64: {title}",
        "{title}",
        "Mupen64Plus FZ-style Glide64 override. Game default preserves the renderer's built-in per-game profile; explicit values override it when Glide64 loads the game. Restart content after changing this option.",
        NULL,
        "glide64",
        {{
{vals}
            {{ NULL, NULL }},
        }},
        "-1"
    }},
'''

blocks = "\n".join(option_block(*o) for o in options)

core_anchor = '''        "0"\n    },\n#endif\n    {\n        CORE_NAME "-gfxplugin-accuracy",'''
if core_anchor not in opts:
    raise SystemExit("Core-options anchor not found; source layout changed.")
opts = opts.replace(
    core_anchor,
    f'''        "0"\n    }},\n\n    /* {MARKER}: options whose backing state still exists in this tree. */\n{blocks}#endif\n    {{\n        CORE_NAME "-gfxplugin-accuracy",''',
    1,
)

helper_anchor = 'extern void glide_set_filtering(unsigned value);\n'
if helper_anchor not in ini:
    raise SystemExit("Glide64 helper anchor not found; source layout changed.")
helper = f'''\n/* {MARKER}. Returns true when RetroArch supplied a numeric value. */\nstatic bool glide64_get_int_option(const char *key, int *value)\n{{\n   struct retro_variable var = {{ key, NULL }};\n   int parsed;\n\n   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&\n       var.value && sscanf(var.value, "%d", &parsed) == 1)\n   {{\n      *value = parsed;\n      return true;\n   }}\n\n   return false;\n}}\n'''
ini = ini.replace(helper_anchor, helper_anchor + helper, 1)

override_anchor = '''   if (settings.n64_z_scale)\n      ZLUT_init();\n\n   //frame buffer\n'''
if override_anchor not in ini:
    raise SystemExit("Glide64 override anchor not found; source layout changed.")

override_lines = []
for suffix, target in override_targets:
    override_lines.append(
        f'      if (glide64_get_int_option("parallel-n64-glide64-{suffix}", &v) && v >= 0)\n'
        f'         {target} = v;'
    )

overrides = f'''   /* {MARKER}.\n    * Apply these after the built-in game table and accuracy policy. A value\n    * of -1 is FZ's Game default sentinel and therefore leaves the table's\n    * decision untouched. */\n   {{\n      int v;\n{chr(10).join(override_lines)}\n   }}\n\n   if (settings.n64_z_scale)\n      ZLUT_init();\n\n   //frame buffer\n'''
ini = ini.replace(override_anchor, overrides, 1)

# Preserve the source file's existing UTF-8 BOM.
INI.write_text(ini, encoding="utf-8-sig")
OPTS.write_text(opts, encoding="utf-8")

print(f"Applied {len(options)} live Glide64 FZ-style options.")
print("Excluded intentionally: filtering conflict, HWFBE/hires paths, fast_crc pending audit, and fields removed from this renderer.")
print("Next: git diff --check && make -j$(nproc)")
