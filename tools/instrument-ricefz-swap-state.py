#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"

text = PATH.read_text(encoding="utf-8")

if "RiceFZ bridge swap diag:" in text:
    print("RiceFZ swap diagnostics already installed.")
    raise SystemExit(0)

if "RiceFZ bridge diag:" not in text:
    raise SystemExit(
        "ERROR: stage-1 RiceFZ viewport diagnostics are not installed. "
        "Run tools/instrument-ricefz-viewport.py first."
    )


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: expected exactly one anchor, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)


anchor = '''static void *fz_bridge_gl_get_proc_address(const char *proc)\n{\n   if (!proc)\n      return NULL;\n   return glsm_get_proc_address(proc);\n}\n'''

replacement = anchor + '''\n/* Diagnostic wrapper around the standalone plugin's swap request. This is the\n * last point inside RiceFZ before control returns to the libretro timing path,\n * so it tells us which raw GL viewport/FBO Rice leaves behind for RetroArch. */\nstatic m64p_error fz_bridge_gl_swap_buffers(void)\n{\n#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)\n   typedef void (*fz_get_integerv_t)(unsigned int, int *);\n   static fz_get_integerv_t get_integerv = NULL;\n   static unsigned int swap_diag_count = 0;\n   int framebuffer = -1;\n   int viewport[4] = { -1, -1, -1, -1 };\n\n   if (!get_integerv)\n      get_integerv = (fz_get_integerv_t)\n         glsm_get_proc_address("glGetIntegerv");\n\n   if (get_integerv && swap_diag_count < 8)\n   {\n      /* GL_FRAMEBUFFER_BINDING = 0x8CA6, GL_VIEWPORT = 0x0BA2. */\n      get_integerv(0x8CA6u, &framebuffer);\n      get_integerv(0x0BA2u, viewport);\n\n      DebugMessage(M64MSG_INFO,\n            "RiceFZ bridge swap diag: call=%u fbo=%d viewport=%d,%d %dx%d requested=%dx%d",\n            swap_diag_count + 1, framebuffer,\n            viewport[0], viewport[1], viewport[2], viewport[3],\n            fz_requested_video_width, fz_requested_video_height);\n      swap_diag_count++;\n   }\n#endif\n\n   return VidExt_GL_SwapBuffers();\n}\n'''

replace_once(anchor, replacement)
replace_once(
    '   FZ_PROC(VidExt_GL_SwapBuffers);',
    '   if (strcmp(name, "VidExt_GL_SwapBuffers") == 0)\n      return (void *) fz_bridge_gl_swap_buffers;',
)

PATH.write_text(text, encoding="utf-8")
print("OK: RiceFZ swap-state diagnostics installed in fz_plugin_bridge.c")
print("Rebuild/install the core, run Mario, then grep 'RiceFZ bridge swap diag'.")
