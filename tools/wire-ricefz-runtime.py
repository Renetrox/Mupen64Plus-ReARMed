#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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


# 1) Renderer identities.
replace_once(
    "mupen64plus-core/custom/mupen64plus-next_common.h",
    "   RDP_PLUGIN_RICE,\n   RDP_PLUGIN_GLN64,",
    "   RDP_PLUGIN_RICE,\n   RDP_PLUGIN_RICEFZ,\n   RDP_PLUGIN_GLN64,",
)

# Graphics/plugin.h was introduced first while scaffolding the experiment.
replace_once(
    "Graphics/plugin.h",
    "   GFX_RICE,\n   GFX_GLN64,",
    "   GFX_RICE,\n   GFX_RICEFZ,\n   GFX_GLN64,",
)

# 2) Build the bridge/loader only on GL builds. The FZ sidecar is GLES/OpenGL.
replace_once(
    "Makefile.common",
    "            $(VIDEODIR_RICE)/Video.cpp\nendif\n\n# Libretro",
    "            $(VIDEODIR_RICE)/Video.cpp\nendif\n\n# Rice FZ external sidecar runtime bridge. The renderer itself remains a\n# separate shared library; only the small loader/ABI bridge is linked here.\nifeq ($(HAVE_OPENGL),1)\nCFLAGS   += -DHAVE_RICEFZ\nCXXFLAGS += -DHAVE_RICEFZ\nSOURCES_C += $(CORE_DIR)/src/plugin/fz_plugin_bridge.c \\\n             $(CORE_DIR)/src/plugin/fz_gfx_plugin.c\nendif\n\n# Libretro",
)

replace_once(
    "Makefile",
    "   LDFLAGS += -lrt\n",
    "   LDFLAGS += -lrt\n   # RiceFZ runtime sidecar uses dlopen/dlsym on Unix.\n   LDFLAGS += -ldl\n",
)

# 3) Add RiceFZ to the public core-option renderer list.
replace_once(
    "libretro/libretro_core_options.h",
    "            { \"gln64\", NULL },\n            { \"rice\", NULL },\n#endif",
    "            { \"gln64\", NULL },\n            { \"rice\", NULL },\n#ifdef HAVE_RICEFZ\n            { \"ricefz\", \"Rice FZ (Experimental)\" },\n#endif\n#endif",
)

# 4) Route the normal Mupen64Plus gfx-function table to the external plugin.
replace_once(
    "mupen64plus-core/src/plugin/plugin.c",
    "#include \"plugin.h\"\n#include \"mupen64plus-next_common.h\"",
    "#include \"plugin.h\"\n#ifdef HAVE_RICEFZ\n#include \"fz_gfx_plugin.h\"\n#endif\n#include \"mupen64plus-next_common.h\"",
)

replace_once(
    "mupen64plus-core/src/plugin/plugin.c",
    "       case RDP_PLUGIN_RICE:\n#ifdef HAVE_RICE\n          gfx = gfx_rice;\n#endif\n          break;\n       case RDP_PLUGIN_GLN64:",
    "       case RDP_PLUGIN_RICE:\n#ifdef HAVE_RICE\n          gfx = gfx_rice;\n#endif\n          break;\n       case RDP_PLUGIN_RICEFZ:\n#ifdef HAVE_RICEFZ\n          if (fz_gfx_plugin_load_rice(&gfx) != M64ERR_SUCCESS)\n          {\n#ifdef HAVE_RICE\n             DebugMessage(M64MSG_ERROR, \"RiceFZ: load failed; using built-in Rice fallback\");\n             gfx = gfx_rice;\n#else\n             DebugMessage(M64MSG_ERROR, \"RiceFZ: load failed and no built-in Rice fallback exists\");\n#endif\n          }\n#endif\n          break;\n       case RDP_PLUGIN_GLN64:",
)

# 5) Make repeated content loads re-use the already-started sidecar cleanly.
replace_once(
    "mupen64plus-core/src/plugin/fz_gfx_plugin.c",
    "static fz_dynlib_t       fz_handle = 0;\nstatic ptr_PluginShutdown fz_shutdown = NULL;\nstatic char               fz_loaded_path[FZ_PATH_MAX];",
    "static fz_dynlib_t        fz_handle = 0;\nstatic ptr_PluginShutdown fz_shutdown = NULL;\nstatic char                fz_loaded_path[FZ_PATH_MAX];\nstatic gfx_plugin_functions fz_cached_gfx;",
)

replace_once(
    "mupen64plus-core/src/plugin/fz_gfx_plugin.c",
    "   if (fz_handle)\n      return M64ERR_ALREADY_INIT;\n\n   memset(out, 0, sizeof(*out));",
    "   if (fz_handle)\n   {\n      *out = fz_cached_gfx;\n      return M64ERR_SUCCESS;\n   }\n\n   memset(out, 0, sizeof(*out));",
)

replace_once(
    "mupen64plus-core/src/plugin/fz_gfx_plugin.c",
    "   (void) capabilities;\n   return M64ERR_SUCCESS;",
    "   fz_cached_gfx = *out;\n   (void) capabilities;\n   return M64ERR_SUCCESS;",
)

replace_once(
    "mupen64plus-core/src/plugin/fz_gfx_plugin.c",
    "   memset(out, 0, sizeof(*out));\n   return M64ERR_PLUGIN_FAIL;",
    "   memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));\n   memset(out, 0, sizeof(*out));\n   return M64ERR_PLUGIN_FAIL;",
)

replace_once(
    "mupen64plus-core/src/plugin/fz_gfx_plugin.c",
    "   fz_loaded_path[0] = '\\0';\n}\n\nint fz_gfx_plugin_is_loaded",
    "   fz_loaded_path[0] = '\\0';\n   memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));\n}\n\nint fz_gfx_plugin_is_loaded",
)

# 6) libretro selection/lifecycle/routing.
replace_once(
    "libretro/libretro.c",
    "#include \"plugin/plugin.h\"\n#include \"api/m64p_types.h\"",
    "#include \"plugin/plugin.h\"\n#ifdef HAVE_RICEFZ\n#include \"plugin/fz_gfx_plugin.h\"\n#endif\n#include \"api/m64p_types.h\"",
)

replace_once(
    "libretro/libretro.c",
    "#ifdef HAVE_RICE\n      if (gfx_var.value && !strcmp(gfx_var.value, \"rice\") && gl_inited)\n         gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_GLIDE64",
    "#ifdef HAVE_RICE\n      if (gfx_var.value && !strcmp(gfx_var.value, \"rice\") && gl_inited)\n         gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_RICEFZ\n      if (gfx_var.value && !strcmp(gfx_var.value, \"ricefz\") && gl_inited)\n         gfx_plugin = GFX_RICEFZ;\n#endif\n#ifdef HAVE_GLIDE64",
)

replace_once(
    "libretro/libretro.c",
    "      case GFX_RICE:      current_rdp_type = RDP_PLUGIN_RICE;      break;\n      case GFX_GLN64:",
    "      case GFX_RICE:      current_rdp_type = RDP_PLUGIN_RICE;      break;\n      case GFX_RICEFZ:    current_rdp_type = RDP_PLUGIN_RICEFZ;    break;\n      case GFX_GLN64:",
)

replace_once(
    "libretro/libretro.c",
    "       case GFX_RICE:\n#ifdef HAVE_RICE\n          {",
    "       case GFX_RICEFZ:\n          /* The external plugin is opened by plugin_connect_all() and its\n           * RomOpen is driven by main_run(), exactly like a normal Mupen64Plus\n           * video plugin. Do not double-open it from the context-reset path. */\n          break;\n       case GFX_RICE:\n#ifdef HAVE_RICE\n          {",
)

replace_once(
    "libretro/libretro.c",
    "   mupen_main_stop();\n   mupen_main_exit();\n\n   deinit_audio_libretro();",
    "   mupen_main_stop();\n   mupen_main_exit();\n#ifdef HAVE_RICEFZ\n   fz_gfx_plugin_unload();\n#endif\n\n   deinit_audio_libretro();",
)

replace_once(
    "libretro/libretro.c",
    "        case GFX_RICE:\n#ifdef HAVE_RICE\n           /* TODO/FIXME */\n#endif\n           break;\n        case GFX_PARALLEL:",
    "        case GFX_RICE:\n#ifdef HAVE_RICE\n           /* TODO/FIXME */\n#endif\n           break;\n        case GFX_RICEFZ:\n           /* FZ Rice reads its own configuration through the M64P bridge. */\n           break;\n        case GFX_PARALLEL:",
)

replace_once(
    "libretro/libretro.c",
    "      case GFX_RICE:\n#ifdef HAVE_RICE\n         /* Stub */\n#endif\n         break;\n      case GFX_PARALLEL:",
    "      case GFX_RICE:\n#ifdef HAVE_RICE\n         /* Stub */\n#endif\n         break;\n      case GFX_RICEFZ:\n         break;\n      case GFX_PARALLEL:",
)

replace_once(
    "libretro/libretro.c",
    "#if defined(HAVE_GLN64) || defined(HAVE_GLIDEN64) || defined(HAVE_RICE) || defined(HAVE_GLIDE64) || defined(HAVE_THR_AL) || defined(HAVE_PARALLEL)",
    "#if defined(HAVE_GLN64) || defined(HAVE_GLIDEN64) || defined(HAVE_RICE) || defined(HAVE_RICEFZ) || defined(HAVE_GLIDE64) || defined(HAVE_THR_AL) || defined(HAVE_PARALLEL)",
)

replace_once(
    "libretro/libretro.c",
    "#ifdef HAVE_RICE\n         if (!strcmp(var.value, \"rice\"))\n            gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_GLIDE64",
    "#ifdef HAVE_RICE\n         if (!strcmp(var.value, \"rice\"))\n            gfx_plugin = GFX_RICE;\n#endif\n#ifdef HAVE_RICEFZ\n         if (!strcmp(var.value, \"ricefz\"))\n            gfx_plugin = GFX_RICEFZ;\n#endif\n#ifdef HAVE_GLIDE64",
)

# There are two matching per-frame GL switch groups; patch both by direct count.
path = ROOT / "libretro/libretro.c"
text = path.read_text(encoding="utf-8")
old = "         case GFX_GLIDEN64:\n         case GFX_RICE:\n#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)"
new = "         case GFX_GLIDEN64:\n         case GFX_RICE:\n         case GFX_RICEFZ:\n#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)"
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
    "   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);",
    "   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE && gfx_plugin != GFX_RICEFZ)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);",
)

replace_once(
    "libretro/libretro.c",
    "   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_BIND, NULL);",
    "   if (gfx_plugin != GFX_GLIDE64 && gfx_plugin != GFX_GLN64 && gfx_plugin != GFX_RICE && gfx_plugin != GFX_RICEFZ)\n      return;\n   glsm_ctl(GLSM_CTL_STATE_BIND, NULL);",
)

replace_once(
    "libretro/libretro.c",
    "               case GFX_RICE:\n#ifdef HAVE_RICE\n                  /* Stub */\n#endif\n                  break;\n               case GFX_GLN64:",
    "               case GFX_RICE:\n#ifdef HAVE_RICE\n                  /* Stub */\n#endif\n                  break;\n               case GFX_RICEFZ:\n                  /* FZ controls its own Rice aspect-related state for now. */\n                  break;\n               case GFX_GLN64:",
)

print("\nRiceFZ runtime wiring complete.")
print("Next: make clean && make -j$(nproc)")
