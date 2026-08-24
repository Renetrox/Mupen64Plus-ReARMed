#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE_C = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"
BRIDGE_H = ROOT / "libretro/fz_plugin_bridge.h"
LIBRETRO_C = ROOT / "libretro/libretro.c"

bridge_c = BRIDGE_C.read_text(encoding="utf-8")
bridge_h = BRIDGE_H.read_text(encoding="utf-8")
libretro_c = LIBRETRO_C.read_text(encoding="utf-8")

MARKER = "RiceFZ frontend viewport restore test"
if MARKER in bridge_c and MARKER in libretro_c:
    print("RiceFZ frontend viewport restore test already installed.")
    raise SystemExit(0)


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: {label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)

# This experiment builds on the viewport/FBO instrumentation from
# instrument-ricefz-viewport.py.  That wrapper is the cleanest place to grab
# RetroArch's viewport: Rice calls VidExt_SetVideoMode before it starts changing
# GL_VIEWPORT, while libretro intentionally leaves the frontend window alone.
if "static m64p_error fz_bridge_set_video_mode" not in bridge_c:
    raise SystemExit(
        "ERROR: viewport instrumentation is not installed. Run "
        "tools/instrument-ricefz-viewport.py first."
    )

# Public restore hook used only at the libretro presentation boundary.
if "fz_plugin_bridge_restore_frontend_viewport" not in bridge_h:
    bridge_h = replace_once(
        bridge_h,
        "void fz_plugin_bridge_bind_current_framebuffer(void);\n",
        "void fz_plugin_bridge_bind_current_framebuffer(void);\n\n"
        "/* External standalone renderers can change GL_VIEWPORT with raw GL.\n"
        " * Restore the viewport RetroArch had before the plugin requested its\n"
        " * standalone video mode, immediately before frontend presentation. */\n"
        "void fz_plugin_bridge_restore_frontend_viewport(void);\n",
        "bridge header restore declaration",
    )

bookkeeping = '''static int fz_requested_video_width = 0;
static int fz_requested_video_height = 0;
'''

capture_code = bookkeeping + '''
/* RiceFZ frontend viewport restore test.
 *
 * A standalone Mupen64Plus video plugin expects VidExt_SetVideoMode() to make
 * its requested 640x480 mode the whole drawable window.  libretro deliberately
 * does not create a second window, so raw glViewport() calls from the plugin
 * can leak into RetroArch's compositor.  Save RetroArch's viewport before Rice
 * begins changing it; presentation restores it later without altering Rice's
 * 640x480 rendering itself. */
static int fz_frontend_viewport[4] = { 0, 0, 0, 0 };
static int fz_frontend_viewport_valid = 0;

static void fz_bridge_capture_frontend_viewport(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   static fz_get_integerv_t get_integerv = NULL;
   int viewport[4] = { 0, 0, 0, 0 };

   if (!get_integerv)
      get_integerv = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");

   if (!get_integerv)
      return;

   /* GL_VIEWPORT = 0x0BA2. */
   get_integerv(0x0BA2u, viewport);
   if (viewport[2] <= 0 || viewport[3] <= 0)
      return;

   fz_frontend_viewport[0] = viewport[0];
   fz_frontend_viewport[1] = viewport[1];
   fz_frontend_viewport[2] = viewport[2];
   fz_frontend_viewport[3] = viewport[3];
   fz_frontend_viewport_valid = 1;

   DebugMessage(M64MSG_INFO,
         "RiceFZ bridge viewport: captured frontend=%d,%d %dx%d",
         viewport[0], viewport[1], viewport[2], viewport[3]);
#endif
}
'''

if MARKER not in bridge_c:
    bridge_c = replace_once(
        bridge_c,
        bookkeeping,
        capture_code,
        "frontend viewport bookkeeping",
    )

    old_setmode_start = '''static m64p_error fz_bridge_set_video_mode(int Width, int Height,
      int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
   fz_requested_video_width = Width;
'''
    new_setmode_start = '''static m64p_error fz_bridge_set_video_mode(int Width, int Height,
      int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
   fz_bridge_capture_frontend_viewport();
   fz_requested_video_width = Width;
'''
    bridge_c = replace_once(
        bridge_c,
        old_setmode_start,
        new_setmode_start,
        "capture before VidExt_SetVideoMode",
    )

    bind_anchor = "void fz_plugin_bridge_bind_current_framebuffer(void)\n"
    restore_code = '''void fz_plugin_bridge_restore_frontend_viewport(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_viewport_t)(int, int, int, int);
   static fz_viewport_t viewport_fn = NULL;
   static unsigned int restore_count = 0;

   if (!fz_frontend_viewport_valid)
      return;

   if (!viewport_fn)
      viewport_fn = (fz_viewport_t) glsm_get_proc_address("glViewport");
   if (!viewport_fn)
      return;

   viewport_fn(fz_frontend_viewport[0], fz_frontend_viewport[1],
               fz_frontend_viewport[2], fz_frontend_viewport[3]);

   if (restore_count < 8)
   {
      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge viewport: restore=%u frontend=%d,%d %dx%d",
            restore_count + 1,
            fz_frontend_viewport[0], fz_frontend_viewport[1],
            fz_frontend_viewport[2], fz_frontend_viewport[3]);
      restore_count++;
   }
#endif
}

'''
    bridge_c = replace_once(
        bridge_c,
        bind_anchor,
        restore_code + bind_anchor,
        "restore function insertion",
    )

# Give libretro.c the restore declaration without depending on transitive
# plugin headers from the local RiceFZ wiring helper.
if '#include "fz_plugin_bridge.h"' not in libretro_c:
    libretro_c = replace_once(
        libretro_c,
        '#include "libretro_core_options.h"\n',
        '#include "libretro_core_options.h"\n'
        '#ifdef HAVE_RICEFZ\n'
        '#include "fz_plugin_bridge.h"\n'
        '#endif\n',
        "libretro bridge include",
    )

if MARKER not in libretro_c:
    present_anchor = '''            aleck64_e90_gl_draw(screen_width, screen_height);
            video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);
'''
    present_replacement = '''            aleck64_e90_gl_draw(screen_width, screen_height);
#ifdef HAVE_RICEFZ
            /* RiceFZ frontend viewport restore test: the external plugin uses
             * raw glViewport(), so GLSM cannot know it dirtied the frontend's
             * viewport. Restore RetroArch's saved viewport only at the final
             * presentation boundary; Rice still renders internally at 640x480. */
            if (gfx_plugin == GFX_RICEFZ)
               fz_plugin_bridge_restore_frontend_viewport();
#endif
            video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);
'''
    libretro_c = replace_once(
        libretro_c,
        present_anchor,
        present_replacement,
        "hardware presentation viewport restore",
    )

BRIDGE_C.write_text(bridge_c, encoding="utf-8")
BRIDGE_H.write_text(bridge_h, encoding="utf-8")
LIBRETRO_C.write_text(libretro_c, encoding="utf-8")

print("OK: RiceFZ frontend viewport restore experiment installed.")
print("Rebuild/install only parallel_n64_libretro.so; the RiceFZ sidecar is unchanged.")
print("After testing, grep 'RiceFZ bridge viewport' from the verbose log.")
