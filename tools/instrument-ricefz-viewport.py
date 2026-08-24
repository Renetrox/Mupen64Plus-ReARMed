#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"

text = PATH.read_text(encoding="utf-8")

if "RiceFZ bridge diag:" in text:
    print("RiceFZ viewport diagnostics already installed.")
    raise SystemExit(0)


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: expected exactly one anchor, found {count}: {old[:80]!r}")
    text = text.replace(old, new, 1)


replace_once(
    '#include "../api/m64p_common.h"\n#include "../api/m64p_config.h"',
    '#include "../api/m64p_common.h"\n#include "../api/callbacks.h"\n#include "../api/m64p_config.h"',
)

anchor = '''static m64p_error fz_bridge_core_get_api_versions(
      int *ConfigVersion,
      int *DebugVersion,
      int *VidextVersion,
      int *ExtraVersion)
{
   m64p_error err = CoreGetAPIVersions(ConfigVersion, DebugVersion,
                                       VidextVersion, ExtraVersion);

   if (err != M64ERR_SUCCESS)
      return err;

   if (ConfigVersion != NULL && *ConfigVersion < 0x020300)
      *ConfigVersion = 0x020300;

   return M64ERR_SUCCESS;
}
'''

replacement = anchor + '''
/* Diagnostic bookkeeping for the standalone-plugin/libretro video boundary.
 * Rice asks Mupen64Plus for a video mode; libretro does not create a new
 * window here, so remember the requested size and compare it with the actual
 * frontend FBO + GL viewport when the external renderer starts drawing. */
static int fz_requested_video_width = 0;
static int fz_requested_video_height = 0;

static m64p_error fz_bridge_set_video_mode(int Width, int Height,
      int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
   fz_requested_video_width = Width;
   fz_requested_video_height = Height;

   DebugMessage(M64MSG_INFO,
         "RiceFZ bridge diag: requested video mode=%dx%d bpp=%d",
         Width, Height, BitsPerPixel);

   return VidExt_SetVideoMode(Width, Height, BitsPerPixel, ScreenMode, Flags);
}
'''
replace_once(anchor, replacement)

old_bind = '''void fz_plugin_bridge_bind_current_framebuffer(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_bind_framebuffer_t)(unsigned int, unsigned int);
   static fz_bind_framebuffer_t bind_framebuffer = NULL;
   unsigned int framebuffer;

   if (!bind_framebuffer)
   {
      bind_framebuffer = (fz_bind_framebuffer_t)
         glsm_get_proc_address("glBindFramebuffer");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferEXT");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferOES");
   }

   if (!bind_framebuffer)
      return;

   framebuffer = (unsigned int) glsm_get_current_framebuffer();
   bind_framebuffer(0x8D40u, framebuffer);
#endif
}
'''

new_bind = '''void fz_plugin_bridge_bind_current_framebuffer(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_bind_framebuffer_t)(unsigned int, unsigned int);
   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   static fz_bind_framebuffer_t bind_framebuffer = NULL;
   static fz_get_integerv_t get_integerv = NULL;
   static unsigned int diag_count = 0;
   unsigned int framebuffer;
   int bound_before = -1;
   int bound_after = -1;
   int viewport[4] = { -1, -1, -1, -1 };

   if (!bind_framebuffer)
   {
      bind_framebuffer = (fz_bind_framebuffer_t)
         glsm_get_proc_address("glBindFramebuffer");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferEXT");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferOES");
   }

   if (!get_integerv)
      get_integerv = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");

   if (!bind_framebuffer)
      return;

   /* GL_FRAMEBUFFER_BINDING[_EXT] = 0x8CA6, GL_VIEWPORT = 0x0BA2. */
   if (get_integerv)
      get_integerv(0x8CA6u, &bound_before);

   framebuffer = (unsigned int) glsm_get_current_framebuffer();
   bind_framebuffer(0x8D40u, framebuffer);

   if (get_integerv)
   {
      get_integerv(0x8CA6u, &bound_after);
      get_integerv(0x0BA2u, viewport);
   }

   /* RomOpen reaches this helper once before Rice has requested its mode.
    * Start logging only after VidExt_SetVideoMode so the useful calls show the
    * relationship between Rice's 640x480 request and the actual GL target. */
   if (fz_requested_video_width > 0 && diag_count < 8)
   {
      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge diag: call=%u requested=%dx%d frontend_fbo=%u bound_before=%d bound_after=%d viewport=%d,%d %dx%d",
            diag_count + 1,
            fz_requested_video_width, fz_requested_video_height,
            framebuffer, bound_before, bound_after,
            viewport[0], viewport[1], viewport[2], viewport[3]);
      diag_count++;
   }
#endif
}
'''
replace_once(old_bind, new_bind)

replace_once(
    '   FZ_PROC(VidExt_SetVideoMode);',
    '   if (strcmp(name, "VidExt_SetVideoMode") == 0)\n      return (void *) fz_bridge_set_video_mode;',
)

PATH.write_text(text, encoding="utf-8")
print("OK: RiceFZ viewport/FBO diagnostics installed in fz_plugin_bridge.c")
print("Build the libretro core, install it, run Mario, then grep 'RiceFZ bridge diag'.")
