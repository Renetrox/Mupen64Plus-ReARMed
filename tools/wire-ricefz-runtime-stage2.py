#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(rel, needle):
    path = ROOT / rel
    text = path.read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"ERROR: prerequisite missing in {rel}: {needle}")
    print(f"OK prerequisite: {rel}")


def replace_once(rel, old, new):
    path = ROOT / rel
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"OK already wired: {rel}")
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: expected exactly one anchor in {rel}, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"OK patched: {rel}")


# Stage 1 was already applied before the original helper stopped. Verify it.
require("Graphics/plugin.h", "GFX_RICEFZ")
require("mupen64plus-core/custom/mupen64plus-next_common.h", "RDP_PLUGIN_RICEFZ")
require("Makefile.common", "fz_plugin_bridge.c")
require("Makefile.common", "fz_gfx_plugin.c")
require("libretro/libretro_core_options.h", '"ricefz", "Rice FZ (Experimental)"')
require("mupen64plus-core/src/plugin/plugin.c", "fz_gfx_plugin_load_rice(&gfx)")

# The loader cache/reuse support is already part of the branch itself.
require("mupen64plus-core/src/plugin/fz_gfx_plugin.c", "static gfx_plugin_functions fz_cached_gfx;")
require("mupen64plus-core/src/plugin/fz_gfx_plugin.c", "*out = fz_cached_gfx;")
require("mupen64plus-core/src/plugin/fz_gfx_plugin.c", "fz_cached_gfx = *out;")
require("mupen64plus-core/src/plugin/fz_gfx_plugin.c", "memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));")

# libretro selection/lifecycle/routing.
replace_once(
    "libretro/libretro.c",
    '#include "plugin/plugin.h"\n#include "api/m64p_types.h"',
    '#include "plugin/plugin.h"\n#ifdef HAVE_RICEFZ\n#include "plugin/fz_gfx_plugin.h"\n#endif\n#include "api/m64p_types.h"',
)

replace_once(
    "libretro/libretro.c",
    '#ifdef HAVE_RICE\n      if (gfx_var.value && !strcmp(gfx_var.value, "rice") && gl_inited)\n         gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_GLIDE64',
    '#ifdef HAVE_RICE\n      if (gfx_var.value && !strcmp(gfx_var.value, "rice") && gl_inited)\n         gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_RICEFZ\n      if (gfx_var.value && !strcmp(gfx_var.value, "ricefz") && gl_inited)\n         gfx_plugin = GFX_RICEFZ;\n#endif\n#ifdef HAVE_GLIDE64',
)

replace_once(
    "libretro/libretro.c",
    '      case GFX_RICE:      current_rdp_type = RDP_PLUGIN_RICE;      break;\n      case GFX_GLN64:',
    '      case GFX_RICE:      current_rdp_type = RDP_PLUGIN_RICE;      break;\n      case GFX_RICEFZ:    current_rdp_type = RDP_PLUGIN_RICEFZ;    break;\n      case GFX_GLN64:',
)

replace_once(
    "libretro/libretro.c",
    '       case GFX_RICE:\n#ifdef HAVE_RICE\n          {',
    '       case GFX_RICEFZ:\n          /* The external plugin is opened by plugin_connect_all() and its\n           * RomOpen is driven by main_run(), exactly like a normal Mupen64Plus\n           * video plugin. Do not double-open it from the context-reset path. */\n          break;\n       case GFX_RICE:\n#ifdef HAVE_RICE\n          {',
)

replace_once(
    "libretro/libretro.c",
    '   mupen_main_stop();\n   mupen_main_exit();\n\n   deinit_audio_libretro();',
    '   mupen_main_stop();\n   mupen_main_exit();\n#ifdef HAVE_RICEFZ\n   fz_gfx_plugin_unload();\n#endif\n\n   deinit_audio_libretro();',
)

replace_once(
    "libretro/libretro.c",
    '        case GFX_RICE:\n#ifdef HAVE_RICE\n           /* TODO/FIXME */\n#endif\n           break;\n        case GFX_PARALLEL:',
    '        case GFX_RICE:\n#ifdef HAVE_RICE\n           /* TODO/FIXME */\n#endif\n           break;\n        case GFX_RICEFZ:\n           /* FZ Rice reads its own configuration through the M64P bridge. */\n           break;\n        case GFX_PARALLEL:',
)

replace_once(
    "libretro/libretro.c",
    '      case GFX_RICE:\n#ifdef HAVE_RICE\n         /* Stub */\n#endif\n         break;\n      case GFX_PARALLEL:',
    '      case GFX_RICE:\n#ifdef HAVE_RICE\n         /* Stub */\n#endif\n         break;\n      case GFX_RICEFZ:\n         break;\n      case GFX_PARALLEL:',
)

replace_once(
    "libretro/libretro.c",
    '#if defined(HAVE_GLN64) || defined(HAVE_GLIDEN64) || defined(HAVE_RICE) || defined(HAVE_GLIDE64) || defined(HAVE_THR_AL) || defined(HAVE_PARALLEL)',
    '#if defined(HAVE_GLN64) || defined(HAVE_GLIDEN64) || defined(HAVE_RICE) || defined(HAVE_RICEFZ) || defined(HAVE_GLIDE64) || defined(HAVE_THR_AL) || defined(HAVE_PARALLEL)',
)

replace_once(
    "libretro/libretro.c",
    '#ifdef HAVE_RICE\n         if (!strcmp(var.value, "rice"))\n            gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_GLIDE64',
    '#ifdef HAVE_RICE\n         if (!strcmp(var.value, "rice"))\n            gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_RICEFZ\n         if (!strcmp(var.value, "ricefz"))\n            gfx_plugin = GFX_RICEFZ;\n#endif\n#ifdef HAVE_GLIDE64',
)

path = ROOT / "libretro/libretro.c"
text = path.read_text(encoding="utf-8")
old = '         case GFX_GLIDEN64:\n         case GFX_RICE:\n#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)'
new = '         case GFX_GLIDEN64:\n         case GFX_RICE:\n         case GFX_RICEFZ:\n#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)'
if new not in text:
    count = text.count(old)
    if count != 2:
        raise SystemExit(f"ERROR: expected two GL run-loop anchors in libretro.c, found {count}")
    text = text.replace(old, new)
    path.write_text(text, encoding="utf-8")
    print("OK patched: libretro/libretro.c (two GL run-loop switches)")
else:
    print("OK already wired: libretro/libretro.c (GL run-loop switches)")

replace_once(
    "libretro/libretro.c",
    '   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);',
    '   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE && gfx_plugin != GFX_RICEFZ)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);',
)

replace_once(
    "libretro/libretro.c",
    '   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_BIND, NULL);',
    '   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE && gfx_plugin != GFX_RICEFZ)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_BIND, NULL);',
)

replace_once(
    "libretro/libretro.c",
    '               case GFX_RICE:\n#ifdef HAVE_RICE\n                  /* Stub */\n#endif\n                  break;\n               case GFX_GLN64:',
    '               case GFX_RICE:\n#ifdef HAVE_RICE\n                  /* Stub */\n#endif\n                  break;\n               case GFX_RICEFZ:\n                  /* FZ controls its own Rice aspect-related state for now. */\n                  break;\n               case GFX_GLN64:',
)

print("\nRiceFZ runtime wiring stage 2 complete.")
print("Next: git diff --check && make clean && make -j$(nproc)")
