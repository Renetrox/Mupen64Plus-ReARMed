#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"

text = PATH.read_text(encoding="utf-8")

MARKER = "RiceFZ frontend scissor restore test"
if MARKER in text:
    print("RiceFZ frontend scissor restore test already installed.")
    raise SystemExit(0)

if "fz_plugin_bridge_restore_frontend_viewport" not in text:
    raise SystemExit(
        "ERROR: frontend viewport restore experiment is not installed. "
        "Run tools/test-ricefz-restore-frontend-viewport.py first."
    )


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: {label}: expected exactly one anchor, found {count}")
    text = text.replace(old, new, 1)

# Keep the frontend's pre-Rice scissor state next to the saved viewport.
old_state = '''static int fz_frontend_viewport[4] = { 0, 0, 0, 0 };
static int fz_frontend_viewport_valid = 0;
'''
new_state = old_state + '''
/* RiceFZ frontend scissor restore test.
 * Standalone Rice enables GL_SCISSOR_TEST and leaves a 640x480-ish scissor
 * box active.  That state belongs to Rice's drawable, not RetroArch's
 * compositor, so save the frontend state before Rice starts touching GL. */
static int fz_frontend_scissor_box[4] = { 0, 0, 0, 0 };
static int fz_frontend_scissor_enabled = 0;
static int fz_frontend_scissor_valid = 0;
'''
replace_once(old_state, new_state, "frontend scissor bookkeeping")

# Extend the existing capture helper so viewport and scissor are captured at
# the same clean boundary: immediately before Rice's VidExt_SetVideoMode.
old_capture_types = '''   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   static fz_get_integerv_t get_integerv = NULL;
   int viewport[4] = { 0, 0, 0, 0 };
'''
new_capture_types = '''   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   typedef unsigned char (*fz_is_enabled_t)(unsigned int);
   static fz_get_integerv_t get_integerv = NULL;
   static fz_is_enabled_t is_enabled = NULL;
   int viewport[4] = { 0, 0, 0, 0 };
   int scissor[4] = { 0, 0, 0, 0 };
'''
replace_once(old_capture_types, new_capture_types, "capture helper GL types")

old_capture_resolve = '''   if (!get_integerv)
      get_integerv = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");

   if (!get_integerv)
      return;
'''
new_capture_resolve = '''   if (!get_integerv)
      get_integerv = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");
   if (!is_enabled)
      is_enabled = (fz_is_enabled_t)
         glsm_get_proc_address("glIsEnabled");

   if (!get_integerv)
      return;
'''
replace_once(old_capture_resolve, new_capture_resolve, "capture helper resolver")

old_capture_tail = '''   fz_frontend_viewport[3] = viewport[3];
   fz_frontend_viewport_valid = 1;

   DebugMessage(M64MSG_INFO,
         "RiceFZ bridge viewport: captured frontend=%d,%d %dx%d",
         viewport[0], viewport[1], viewport[2], viewport[3]);
'''
new_capture_tail = '''   fz_frontend_viewport[3] = viewport[3];
   fz_frontend_viewport_valid = 1;

   /* GL_SCISSOR_BOX = 0x0C10, GL_SCISSOR_TEST = 0x0C11. */
   get_integerv(0x0C10u, scissor);
   fz_frontend_scissor_box[0] = scissor[0];
   fz_frontend_scissor_box[1] = scissor[1];
   fz_frontend_scissor_box[2] = scissor[2];
   fz_frontend_scissor_box[3] = scissor[3];
   fz_frontend_scissor_enabled = is_enabled ? (is_enabled(0x0C11u) ? 1 : 0) : 0;
   fz_frontend_scissor_valid = 1;

   DebugMessage(M64MSG_INFO,
         "RiceFZ bridge viewport: captured frontend=%d,%d %dx%d",
         viewport[0], viewport[1], viewport[2], viewport[3]);
   DebugMessage(M64MSG_INFO,
         "RiceFZ bridge scissor restore: captured enabled=%d box=%d,%d %dx%d",
         fz_frontend_scissor_enabled,
         scissor[0], scissor[1], scissor[2], scissor[3]);
'''
replace_once(old_capture_tail, new_capture_tail, "capture scissor state")

# Extend the final presentation restore hook.  Rice still renders with its own
# viewport/scissor; only the frontend state is restored immediately before
# video_cb, where RetroArch composites/scales the HW frame.
old_restore_types = '''   typedef void (*fz_viewport_t)(int, int, int, int);
   static fz_viewport_t viewport_fn = NULL;
   static unsigned int restore_count = 0;
'''
new_restore_types = '''   typedef void (*fz_viewport_t)(int, int, int, int);
   typedef void (*fz_scissor_t)(int, int, int, int);
   typedef void (*fz_cap_t)(unsigned int);
   static fz_viewport_t viewport_fn = NULL;
   static fz_scissor_t scissor_fn = NULL;
   static fz_cap_t enable_fn = NULL;
   static fz_cap_t disable_fn = NULL;
   static unsigned int restore_count = 0;
'''
replace_once(old_restore_types, new_restore_types, "restore helper GL types")

old_restore_resolve = '''   if (!viewport_fn)
      viewport_fn = (fz_viewport_t) glsm_get_proc_address("glViewport");
   if (!viewport_fn)
      return;

   viewport_fn(fz_frontend_viewport[0], fz_frontend_viewport[1],
               fz_frontend_viewport[2], fz_frontend_viewport[3]);
'''
new_restore_resolve = '''   if (!viewport_fn)
      viewport_fn = (fz_viewport_t) glsm_get_proc_address("glViewport");
   if (!scissor_fn)
      scissor_fn = (fz_scissor_t) glsm_get_proc_address("glScissor");
   if (!enable_fn)
      enable_fn = (fz_cap_t) glsm_get_proc_address("glEnable");
   if (!disable_fn)
      disable_fn = (fz_cap_t) glsm_get_proc_address("glDisable");
   if (!viewport_fn)
      return;

   viewport_fn(fz_frontend_viewport[0], fz_frontend_viewport[1],
               fz_frontend_viewport[2], fz_frontend_viewport[3]);

   if (fz_frontend_scissor_valid && scissor_fn && enable_fn && disable_fn)
   {
      scissor_fn(fz_frontend_scissor_box[0], fz_frontend_scissor_box[1],
                 fz_frontend_scissor_box[2], fz_frontend_scissor_box[3]);
      if (fz_frontend_scissor_enabled)
         enable_fn(0x0C11u); /* GL_SCISSOR_TEST */
      else
         disable_fn(0x0C11u);
   }
'''
replace_once(old_restore_resolve, new_restore_resolve, "restore scissor state")

old_restore_log = '''      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge viewport: restore=%u frontend=%d,%d %dx%d",
            restore_count + 1,
            fz_frontend_viewport[0], fz_frontend_viewport[1],
            fz_frontend_viewport[2], fz_frontend_viewport[3]);
      restore_count++;
'''
new_restore_log = '''      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge viewport: restore=%u frontend=%d,%d %dx%d",
            restore_count + 1,
            fz_frontend_viewport[0], fz_frontend_viewport[1],
            fz_frontend_viewport[2], fz_frontend_viewport[3]);
      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge scissor restore: restore=%u enabled=%d box=%d,%d %dx%d",
            restore_count + 1,
            fz_frontend_scissor_enabled,
            fz_frontend_scissor_box[0], fz_frontend_scissor_box[1],
            fz_frontend_scissor_box[2], fz_frontend_scissor_box[3]);
      restore_count++;
'''
replace_once(old_restore_log, new_restore_log, "restore scissor diagnostic")

PATH.write_text(text, encoding="utf-8")
print("OK: RiceFZ frontend scissor restore experiment installed.")
print("Rebuild/install only parallel_n64_libretro.so, run Mario, and check whether the ventanita disappears.")
print("Then grep 'RiceFZ bridge scissor restore' from the verbose log.")
