#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"

text = PATH.read_text(encoding="utf-8")

if "RiceFZ bridge scissor diag:" in text:
    print("RiceFZ scissor diagnostics already installed.")
    raise SystemExit(0)

if "RiceFZ bridge swap diag:" not in text:
    raise SystemExit(
        "ERROR: swap diagnostics are not installed. Run "
        "tools/instrument-ricefz-swap-state.py first."
    )


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: expected exactly one anchor, found {count}: {old[:120]!r}")
    text = text.replace(old, new, 1)

replace_once(
    '''   typedef void (*fz_get_integerv_t)(unsigned int, int *);\n   static fz_get_integerv_t get_integerv = NULL;\n   static unsigned int swap_diag_count = 0;\n   int framebuffer = -1;\n   int viewport[4] = { -1, -1, -1, -1 };\n''',
    '''   typedef void (*fz_get_integerv_t)(unsigned int, int *);\n   typedef unsigned char (*fz_is_enabled_t)(unsigned int);\n   static fz_get_integerv_t get_integerv = NULL;\n   static fz_is_enabled_t is_enabled = NULL;\n   static unsigned int swap_diag_count = 0;\n   int framebuffer = -1;\n   int viewport[4] = { -1, -1, -1, -1 };\n   int scissor_box[4] = { -1, -1, -1, -1 };\n   int scissor_enabled = -1;\n'''
)

replace_once(
    '''   if (!get_integerv)\n      get_integerv = (fz_get_integerv_t)\n         glsm_get_proc_address("glGetIntegerv");\n\n   if (get_integerv && swap_diag_count < 8)\n   {\n''',
    '''   if (!get_integerv)\n      get_integerv = (fz_get_integerv_t)\n         glsm_get_proc_address("glGetIntegerv");\n   if (!is_enabled)\n      is_enabled = (fz_is_enabled_t)\n         glsm_get_proc_address("glIsEnabled");\n\n   if (get_integerv && swap_diag_count < 8)\n   {\n'''
)

replace_once(
    '''      get_integerv(0x8CA6u, &framebuffer);\n      get_integerv(0x0BA2u, viewport);\n\n      DebugMessage(M64MSG_INFO,\n            "RiceFZ bridge swap diag: call=%u fbo=%d viewport=%d,%d %dx%d requested=%dx%d",\n            swap_diag_count + 1, framebuffer,\n            viewport[0], viewport[1], viewport[2], viewport[3],\n            fz_requested_video_width, fz_requested_video_height);\n''',
    '''      get_integerv(0x8CA6u, &framebuffer);\n      get_integerv(0x0BA2u, viewport);\n      /* GL_SCISSOR_BOX = 0x0C10, GL_SCISSOR_TEST = 0x0C11. */\n      get_integerv(0x0C10u, scissor_box);\n      if (is_enabled)\n         scissor_enabled = is_enabled(0x0C11u) ? 1 : 0;\n\n      DebugMessage(M64MSG_INFO,\n            "RiceFZ bridge swap diag: call=%u fbo=%d viewport=%d,%d %dx%d requested=%dx%d",\n            swap_diag_count + 1, framebuffer,\n            viewport[0], viewport[1], viewport[2], viewport[3],\n            fz_requested_video_width, fz_requested_video_height);\n      DebugMessage(M64MSG_INFO,\n            "RiceFZ bridge scissor diag: call=%u enabled=%d box=%d,%d %dx%d",\n            swap_diag_count + 1, scissor_enabled,\n            scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);\n'''
)

PATH.write_text(text, encoding="utf-8")
print("OK: RiceFZ scissor diagnostics installed in fz_plugin_bridge.c")
print("Rebuild/install only the libretro core, run Mario, then grep 'RiceFZ bridge scissor diag'.")
