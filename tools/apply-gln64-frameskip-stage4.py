#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "Makefile.common"
GLN64_MAIN = ROOT / "gles2n64/src/gles2N64.c"
GLN64_FS_C = ROOT / "gles2n64/src/FrameSkipper_gln64.c"
GLN64_FS_H = ROOT / "gles2n64/src/FrameSkipper_gln64.h"
LIBRETRO = ROOT / "libretro/libretro.c"
CORE_OPTIONS = ROOT / "libretro/libretro_core_options.h"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        print(f"SKIP: {label} ya aplicado")
        return text
    if old not in text:
        raise RuntimeError(f"No se encontro el ancla para {label}")
    print(f"OK:   {label}")
    return text.replace(old, new, 1)


def write_if_needed(path: Path, content: str, label: str) -> None:
    if path.exists():
        current = path.read_text(encoding="utf-8")
        if current == content:
            print(f"SKIP: {label} ya existe")
            return
        raise RuntimeError(f"{path} ya existe con contenido diferente")
    path.write_text(content, encoding="utf-8")
    print(f"OK:   {label}")


FS_H = r'''/*
 * glN64/gles2n64 frameskip restoration for parallel-n64.
 *
 * Based on the FrameSkipper used by the historical gles2n64 plugin and
 * removed from the libretro port by commit 8a36e267 (2013). The original
 * scheduler used a millisecond tick source; this port uses libretro-common's
 * microsecond clock and keeps the historical AUTO/MANUAL behaviour.
 */
#ifndef FRAMESKIPPER_GLN64_H
#define FRAMESKIPPER_GLN64_H

#ifdef __cplusplus
extern "C" {
#endif

enum gln64_frameskip_mode
{
   GLN64_FRAMESKIP_DISABLED = 0,
   GLN64_FRAMESKIP_AUTO     = 1,
   GLN64_FRAMESKIP_MANUAL   = 2
};

void gln64_frameskip_configure(int mode, int max_skips);
void gln64_frameskip_set_target_fps(int fps);
void gln64_frameskip_reset(void);
void gln64_frameskip_update(void);
int  gln64_frameskip_will_skip_next(void);

#ifdef __cplusplus
}
#endif

#endif
'''


FS_C = r'''/*
 * glN64/gles2n64 frameskip restoration for parallel-n64.
 *
 * The scheduler follows the old gles2n64 FrameSkipper by yongzh/Adventus:
 * AUTO compares emulated frame progress with elapsed wall time; MANUAL skips
 * N graphics frames and renders the next. The libretro port deliberately
 * removed this subsystem in 2013; this file restores it without SDL/ticks.c.
 */
#include "FrameSkipper_gln64.h"

#include <stdint.h>
#include <features/features_cpu.h>

typedef struct gln64_frame_skipper
{
   int mode;
   int max_skips;
   int target_fps;
   int skip_counter;
   retro_time_t initial_time_us;
   uint64_t virtual_frame;
} gln64_frame_skipper;

static gln64_frame_skipper g_frameskip = {
   GLN64_FRAMESKIP_DISABLED,
   0,
   60,
   0,
   0,
   0
};

void gln64_frameskip_reset(void)
{
   g_frameskip.skip_counter    = 0;
   g_frameskip.initial_time_us = 0;
   g_frameskip.virtual_frame   = 0;
}

void gln64_frameskip_configure(int mode, int max_skips)
{
   if (max_skips < 0)
      max_skips = 0;
   if (max_skips > 5)
      max_skips = 5;

   if (mode != GLN64_FRAMESKIP_AUTO && mode != GLN64_FRAMESKIP_MANUAL)
   {
      mode = GLN64_FRAMESKIP_DISABLED;
      max_skips = 0;
   }

   if (max_skips == 0)
      mode = GLN64_FRAMESKIP_DISABLED;

   if (g_frameskip.mode != mode || g_frameskip.max_skips != max_skips)
   {
      g_frameskip.mode = mode;
      g_frameskip.max_skips = max_skips;
      gln64_frameskip_reset();
   }
}

void gln64_frameskip_set_target_fps(int fps)
{
   if (fps <= 0)
      fps = 60;

   if (g_frameskip.target_fps != fps)
   {
      g_frameskip.target_fps = fps;
      gln64_frameskip_reset();
   }
}

int gln64_frameskip_will_skip_next(void)
{
   return g_frameskip.max_skips > 0 && g_frameskip.skip_counter > 0;
}

void gln64_frameskip_update(void)
{
   retro_time_t now_us;
   retro_time_t elapsed_us;
   uint64_t real_frame;

   if (g_frameskip.mode == GLN64_FRAMESKIP_DISABLED || g_frameskip.max_skips < 1)
   {
      g_frameskip.skip_counter = 0;
      return;
   }

   if (g_frameskip.mode == GLN64_FRAMESKIP_MANUAL)
   {
      g_frameskip.skip_counter++;
      if (g_frameskip.skip_counter > g_frameskip.max_skips)
         g_frameskip.skip_counter = 0;
      return;
   }

   /* AUTO: direct C adaptation of the historical gles2n64 scheduler. */
   now_us = cpu_features_get_time_usec();

   if (g_frameskip.initial_time_us <= 0)
   {
      g_frameskip.initial_time_us = now_us;
      g_frameskip.virtual_frame = 0;
      g_frameskip.skip_counter = 0;
      return;
   }

   elapsed_us = now_us - g_frameskip.initial_time_us;
   if (elapsed_us < 0)
   {
      gln64_frameskip_reset();
      g_frameskip.initial_time_us = now_us;
      return;
   }

   real_frame = ((uint64_t)elapsed_us * (uint64_t)g_frameskip.target_fps) / 1000000ULL;
   g_frameskip.virtual_frame++;

   if (real_frame >= g_frameskip.virtual_frame)
   {
      if (real_frame > g_frameskip.virtual_frame &&
          g_frameskip.skip_counter < g_frameskip.max_skips)
      {
         g_frameskip.skip_counter++;
      }
      else
      {
         g_frameskip.virtual_frame = real_frame;
         g_frameskip.skip_counter = 0;
      }
   }
}
'''


def main() -> int:
    write_if_needed(GLN64_FS_H, FS_H, "FrameSkipper_gln64.h")
    write_if_needed(GLN64_FS_C, FS_C, "FrameSkipper_gln64.c")

    # Build the restored scheduler only when glN64 itself is enabled.
    text = MAKEFILE.read_text(encoding="utf-8")
    old = "            $(VIDEODIR_GLN64)/glN64Config.c \\\n"
    new = old + "            $(VIDEODIR_GLN64)/FrameSkipper_gln64.c \\\n"
    text = replace_once(text, old, new, "Makefile: compilar FrameSkipper glN64")
    MAKEFILE.write_text(text, encoding="utf-8")

    # Restore the historical integration points: scheduler update on VI,
    # decision before RSP_ProcessDList(), PAL/NTSC target, and reset on resume.
    text = GLN64_MAIN.read_text(encoding="utf-8")

    old = '#include "3DMath.h"\n#include "../../libretro/libretro_private.h"\n'
    new = '#include "3DMath.h"\n#include "FrameSkipper_gln64.h"\n#include "../../libretro/libretro_private.h"\n'
    text = replace_once(text, old, new, "glN64: incluir FrameSkipper")

    old = '''void gln64ProcessDList(void)
{
    OGL.frame_dl++;

    RSP_ProcessDList();
    OGL.mustRenderDlist = true;
}
'''
    new = '''void gln64ProcessDList(void)
{
    OGL.frame_dl++;

    /* Historical gles2n64 skipped before walking the display list.  Keep the
     * RSP bookkeeping moving and raise both DP and SP interrupts exactly as
     * the old plugin did, otherwise a skipped task can leave the core waiting. */
    if (gln64_frameskip_will_skip_next())
    {
        __RSP.busy = false;
        __RSP.DList++;

        *gfx_info.MI_INTR_REG |= 0x20; /* MI_INTR_DP */
        if (gfx_info.CheckInterrupts)
            gfx_info.CheckInterrupts();
        *gfx_info.MI_INTR_REG |= 0x01; /* MI_INTR_SP */
        if (gfx_info.CheckInterrupts)
            gfx_info.CheckInterrupts();
        return;
    }

    RSP_ProcessDList();
    OGL.mustRenderDlist = true;
}
'''
    text = replace_once(text, old, new, "glN64: salto real antes de ProcessDList")

    old = '''void gln64RomClosed (void)
{
}
'''
    new = '''void gln64RomClosed (void)
{
    gln64_frameskip_reset();
}
'''
    text = replace_once(text, old, new, "glN64: reset al cerrar ROM")

    old = '''int gln64RomOpen (void)
{
    RSP_Init();
    OGL.frame_dl = 0;
    OGL.frame_prevdl = -1;
    OGL.mustRenderDlist = false;

    return 1;
}
'''
    new = '''int gln64RomOpen (void)
{
    RSP_Init();
    OGL.frame_dl = 0;
    OGL.frame_prevdl = -1;
    OGL.mustRenderDlist = false;

    gln64_frameskip_set_target_fps(config.romPAL ? 50 : 60);
    gln64_frameskip_reset();

    return 1;
}
'''
    text = replace_once(text, old, new, "glN64: objetivo PAL/NTSC")

    old = '''EXPORT void CALL RomResumed(void)
{
}
'''
    new = '''EXPORT void CALL RomResumed(void)
{
    gln64_frameskip_reset();
}
'''
    text = replace_once(text, old, new, "glN64: reset al reanudar")

    old = '''void gln64UpdateScreen (void)
{
   //has there been any display lists since last update ?
'''
    new = '''void gln64UpdateScreen (void)
{
   /* The historical FrameSkipper advances once per VI, then ProcessDList()
    * consumes that decision before doing the expensive graphics work. */
   gln64_frameskip_update();

   //has there been any display lists since last update ?
'''
    text = replace_once(text, old, new, "glN64: actualizar scheduler en VI")

    old = '''void gles2n64_reset(void)
{
   // HACK: Check for leaks!
   OGL_Stop();
   OGL_Start();
   RSP_Init();
}
'''
    new = '''void gles2n64_reset(void)
{
   // HACK: Check for leaks!
   OGL_Stop();
   OGL_Start();
   RSP_Init();
   gln64_frameskip_reset();
}
'''
    text = replace_once(text, old, new, "glN64: reset tras recrear contexto")

    GLN64_MAIN.write_text(text, encoding="utf-8")

    # Wire the new Core Option to the scheduler.  Keep it independent from
    # Glide64 so switching renderers cannot share stale timing state.
    text = LIBRETRO.read_text(encoding="utf-8")
    old = '''#ifdef HAVE_GLIDE64
#include "../glide2gl/src/Glide64/FrameSkipper_glide64.h"
#endif
'''
    new = '''#ifdef HAVE_GLIDE64
#include "../glide2gl/src/Glide64/FrameSkipper_glide64.h"
#endif
#ifdef HAVE_GLN64
#include "../gles2n64/src/FrameSkipper_gln64.h"
#endif
'''
    text = replace_once(text, old, new, "libretro: incluir FrameSkipper glN64")

    anchor = '''#if defined(HAVE_PARALLEL)
   var.key = "parallel-n64-parallel-rdp-synchronous";
'''
    block = '''#ifdef HAVE_GLN64
   var.key = "parallel-n64-gln64-frameskip";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "auto"))
         gln64_frameskip_configure(GLN64_FRAMESKIP_AUTO, 5);
      else if (!strcmp(var.value, "disabled"))
         gln64_frameskip_configure(GLN64_FRAMESKIP_DISABLED, 0);
      else
      {
         int max_skips = atoi(var.value);
         if (max_skips < 1)
            max_skips = 1;
         if (max_skips > 5)
            max_skips = 5;
         gln64_frameskip_configure(GLN64_FRAMESKIP_MANUAL, max_skips);
      }
   }
   else
      gln64_frameskip_configure(GLN64_FRAMESKIP_DISABLED, 0);
#endif

'''
    text = replace_once(text, anchor, block + anchor, "libretro: Core Option glN64 frameskip")
    LIBRETRO.write_text(text, encoding="utf-8")

    # Put the option after GFX Accuracy so the existing Stage 3 helper keeps
    # its Glide64/Rice anchor strings intact if it has not been run yet.
    text = CORE_OPTIONS.read_text(encoding="utf-8")
    anchor = '''#ifdef HAVE_PARALLEL
    {
        CORE_NAME "-parallel-rdp-synchronous",
'''
    option = '''#ifdef HAVE_GLN64
    {
        CORE_NAME "-gln64-frameskip",
        "(glN64) Frameskip",
        "Frameskip",
        "Restore the historical gles2n64 display-list frameskip. Automatic mode skips only when emulation falls behind; manual values skip that many graphics frames before rendering the next. Intended for low-power hardware.",
        NULL,
        NULL,
        {
            { "disabled", "Disabled" },
            { "auto", "Automatic" },
            { "1", "1" },
            { "2", "2" },
            { "3", "3" },
            { "4", "4" },
            { "5", "5" },
            { NULL, NULL },
        },
        "disabled"
    },
#endif
'''
    text = replace_once(text, anchor, option + anchor, "Core Options: glN64 Frameskip")
    CORE_OPTIONS.write_text(text, encoding="utf-8")

    print("\nStage 4 aplicado.")
    print("glN64 Frameskip: Disabled / Automatic / 1..5")
    print("Implementacion: salto antes de RSP_ProcessDList + interrupciones DP/SP historicas")
    print("Default temporal: Disabled hasta validar en PC/ARM.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
