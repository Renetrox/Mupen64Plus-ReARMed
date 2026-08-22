#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

CORE_OPTIONS = ROOT / "libretro/libretro_core_options.h"
LIBRETRO_C = ROOT / "libretro/libretro.c"
MAKEFILE_COMMON = ROOT / "Makefile.common"
GLIDE_MAIN = ROOT / "glide2gl/src/Glide64/glidemain.c"
GLIDE_RDP = ROOT / "glide2gl/src/Glide64/glide64_rdp.c"
FRAMESKIP_H = ROOT / "glide2gl/src/Glide64/FrameSkipper_glide64.h"
FRAMESKIP_C = ROOT / "glide2gl/src/Glide64/FrameSkipper_glide64.c"


def replace_once(path: Path, old: str, new: str, marker: str) -> bool:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        print(f"SKIP: {path.relative_to(ROOT)} ya contiene {marker}")
        return False
    if old not in text:
        raise RuntimeError(
            f"No se encontro el ancla esperada en {path.relative_to(ROOT)}:\n{old[:240]}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"OK:   {path.relative_to(ROOT)}")
    return True


def write_new(path: Path, content: str) -> bool:
    if path.exists():
        current = path.read_text(encoding="utf-8")
        if current == content:
            print(f"SKIP: {path.relative_to(ROOT)} ya existe y coincide")
            return False
        raise RuntimeError(
            f"{path.relative_to(ROOT)} ya existe con contenido distinto; no se sobrescribe."
        )
    path.write_text(content, encoding="utf-8")
    print(f"OK:   {path.relative_to(ROOT)}")
    return True


FRAMESKIP_H_CONTENT = r'''/*
 * Glide64mk2 frameskip adapter for parallel-n64's C glide2gl port.
 *
 * The scheduling behaviour is based on the optional FrameSkipper added to
 * mupen64plus-video-glide64mk2 in 2015 (commit 717b587).  Frameskip remains
 * disabled by default and is controlled by a libretro Core Option.
 */
#ifndef FRAMESKIPPER_GLIDE64_H
#define FRAMESKIPPER_GLIDE64_H

#ifdef __cplusplus
extern "C" {
#endif

enum glide64_frameskip_mode
{
   GLIDE64_FRAMESKIP_DISABLED = 0,
   GLIDE64_FRAMESKIP_AUTO     = 1,
   GLIDE64_FRAMESKIP_MANUAL   = 2
};

void glide64_frameskip_configure(int mode, int max_skips);
void glide64_frameskip_set_target_fps(int fps);
void glide64_frameskip_reset(void);
void glide64_frameskip_update(void);
int  glide64_frameskip_will_skip_next(void);

#ifdef __cplusplus
}
#endif

#endif
'''


FRAMESKIP_C_CONTENT = r'''/*
 * Glide64mk2 frameskip adapter for parallel-n64's C glide2gl port.
 *
 * Algorithm derived from the optional FrameSkipper implementation merged in
 * mupen64plus-video-glide64mk2 (commit 717b587, 2015).  The original plugin
 * used SDL_GetTicks(); this libretro port uses libretro-common's monotonic-ish
 * microsecond clock so no SDL dependency is reintroduced.
 */
#include "FrameSkipper_glide64.h"

#include <stdint.h>
#include <features/features_cpu.h>

typedef struct glide64_frame_skipper
{
   int mode;
   int max_skips;
   int target_fps;
   int skip_counter;
   retro_time_t initial_time_us;
   uint64_t actual_frame;
} glide64_frame_skipper;

static glide64_frame_skipper g_frameskip = {
   GLIDE64_FRAMESKIP_DISABLED,
   0,
   60,
   0,
   0,
   0
};

void glide64_frameskip_reset(void)
{
   g_frameskip.skip_counter   = 0;
   g_frameskip.initial_time_us = 0;
   g_frameskip.actual_frame    = 0;
}

void glide64_frameskip_configure(int mode, int max_skips)
{
   if (max_skips < 0)
      max_skips = 0;
   if (max_skips > 5)
      max_skips = 5;

   if (mode != GLIDE64_FRAMESKIP_AUTO && mode != GLIDE64_FRAMESKIP_MANUAL)
   {
      mode = GLIDE64_FRAMESKIP_DISABLED;
      max_skips = 0;
   }

   if (max_skips == 0)
      mode = GLIDE64_FRAMESKIP_DISABLED;

   if (g_frameskip.mode != mode || g_frameskip.max_skips != max_skips)
   {
      g_frameskip.mode = mode;
      g_frameskip.max_skips = max_skips;
      glide64_frameskip_reset();
   }
}

void glide64_frameskip_set_target_fps(int fps)
{
   if (fps <= 0)
      fps = 60;

   if (g_frameskip.target_fps != fps)
   {
      g_frameskip.target_fps = fps;
      glide64_frameskip_reset();
   }
}

int glide64_frameskip_will_skip_next(void)
{
   return g_frameskip.max_skips > 0 && g_frameskip.skip_counter > 0;
}

void glide64_frameskip_update(void)
{
   retro_time_t now_us;
   retro_time_t elapsed_us;
   uint64_t desired_frame;

   if (g_frameskip.mode == GLIDE64_FRAMESKIP_DISABLED || g_frameskip.max_skips < 1)
   {
      g_frameskip.skip_counter = 0;
      return;
   }

   if (g_frameskip.mode == GLIDE64_FRAMESKIP_MANUAL)
   {
      g_frameskip.skip_counter++;
      if (g_frameskip.skip_counter > g_frameskip.max_skips)
         g_frameskip.skip_counter = 0;
      return;
   }

   /* AUTO: same policy as Glide64mk2's original FrameSkipper. */
   now_us = cpu_features_get_time_usec();

   if (g_frameskip.initial_time_us <= 0)
   {
      g_frameskip.initial_time_us = now_us;
      g_frameskip.actual_frame = 0;
      g_frameskip.skip_counter = 0;
      return;
   }

   elapsed_us = now_us - g_frameskip.initial_time_us;
   if (elapsed_us < 0)
   {
      /* Clock discontinuity: restart the scheduler rather than overskip. */
      glide64_frameskip_reset();
      g_frameskip.initial_time_us = now_us;
      return;
   }

   desired_frame = ((uint64_t)elapsed_us * (uint64_t)g_frameskip.target_fps) / 1000000ULL;
   g_frameskip.actual_frame++;

   if (desired_frame < g_frameskip.actual_frame)
   {
      /* Ahead of schedule: keep rendering. */
   }
   else if (desired_frame > g_frameskip.actual_frame &&
            g_frameskip.skip_counter < g_frameskip.max_skips)
   {
      /* Behind schedule: skip the next graphics task, up to the configured cap. */
      g_frameskip.skip_counter++;
   }
   else
   {
      /* On schedule, or the skip cap was reached: render and resynchronise. */
      g_frameskip.skip_counter = 0;
      g_frameskip.actual_frame = desired_frame;
   }
}
'''


def main() -> int:
    changed = 0

    # 1) Add the C port of Glide64mk2's FrameSkipper.
    changed += write_new(FRAMESKIP_H, FRAMESKIP_H_CONTENT)
    changed += write_new(FRAMESKIP_C, FRAMESKIP_C_CONTENT)

    # 2) Compile it whenever Glide64 is compiled.
    make_anchor = '''            $(VIDEODIR_GLIDE)/Glide64/glidemain.c \\
            $(VIDEODIR_GLIDE)/Glide64/glide64_util.c \\
'''
    make_replacement = '''            $(VIDEODIR_GLIDE)/Glide64/glidemain.c \\
            $(VIDEODIR_GLIDE)/Glide64/FrameSkipper_glide64.c \\
            $(VIDEODIR_GLIDE)/Glide64/glide64_util.c \\
'''
    changed += replace_once(
        MAKEFILE_COMMON,
        make_anchor,
        make_replacement,
        "FrameSkipper_glide64.c",
    )

    # 3) Expose Disabled / Automatic / 1..5 in the Glide64 Core Options category.
    option_anchor = '''    {
        CORE_NAME "-gfxplugin-accuracy",
'''
    option_replacement = '''#ifdef HAVE_GLIDE64
    {
        CORE_NAME "-glide64-frameskip",
        "Glide64: Frameskip",
        "Frameskip",
        "Skip Glide64 graphics display lists to reduce rendering load. Automatic mode skips only when emulation falls behind and may skip up to five consecutive frames. Disabled preserves normal rendering.",
        NULL,
        "glide64",
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
    {
        CORE_NAME "-gfxplugin-accuracy",
'''
    changed += replace_once(
        CORE_OPTIONS,
        option_anchor,
        option_replacement,
        'CORE_NAME "-glide64-frameskip"',
    )

    # 4) Let libretro Core Options configure the scheduler. This is runtime-safe:
    # it only changes small frameskip counters, not the graphics backend itself.
    include_anchor = '''#include "../Graphics/plugin.h"
'''
    include_replacement = '''#include "../Graphics/plugin.h"
#ifdef HAVE_GLIDE64
#include "../glide2gl/src/Glide64/FrameSkipper_glide64.h"
#endif
'''
    changed += replace_once(
        LIBRETRO_C,
        include_anchor,
        include_replacement,
        "FrameSkipper_glide64.h",
    )

    update_anchor = '''#if defined(HAVE_PARALLEL)
   var.key = "parallel-n64-parallel-rdp-synchronous";
'''
    update_replacement = '''#ifdef HAVE_GLIDE64
   var.key = "parallel-n64-glide64-frameskip";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "auto"))
         glide64_frameskip_configure(GLIDE64_FRAMESKIP_AUTO, 5);
      else if (!strcmp(var.value, "disabled"))
         glide64_frameskip_configure(GLIDE64_FRAMESKIP_DISABLED, 0);
      else
      {
         int max_skips = atoi(var.value);
         if (max_skips < 1)
            max_skips = 1;
         if (max_skips > 5)
            max_skips = 5;
         glide64_frameskip_configure(GLIDE64_FRAMESKIP_MANUAL, max_skips);
      }
   }
   else
      glide64_frameskip_configure(GLIDE64_FRAMESKIP_DISABLED, 0);
#endif

#if defined(HAVE_PARALLEL)
   var.key = "parallel-n64-parallel-rdp-synchronous";
'''
    changed += replace_once(
        LIBRETRO_C,
        update_anchor,
        update_replacement,
        '"parallel-n64-glide64-frameskip"',
    )

    # 5) Update scheduler once per VI, like the official Glide64mk2 feature;
    # reset timing on ROM transitions and use the PAL/NTSC target rate.
    main_include_anchor = '''#include "GlideExtensions.h"
#include <libretro.h>
'''
    main_include_replacement = '''#include "GlideExtensions.h"
#include "FrameSkipper_glide64.h"
#include <libretro.h>
'''
    changed += replace_once(
        GLIDE_MAIN,
        main_include_anchor,
        main_include_replacement,
        '"FrameSkipper_glide64.h"',
    )

    romclosed_anchor = '''void glide64RomClosed (void)
{
   ReleaseGfx ();
}
'''
    romclosed_replacement = '''void glide64RomClosed (void)
{
   glide64_frameskip_reset();
   ReleaseGfx ();
}
'''
    changed += replace_once(
        GLIDE_MAIN,
        romclosed_anchor,
        romclosed_replacement,
        "glide64_frameskip_reset();\n   ReleaseGfx",
    )

    romopen_anchor = '''   ReadSpecialSettings (name);

   // get the name of the ROM
'''
    romopen_replacement = '''   glide64_frameskip_set_target_fps(region == OS_TV_TYPE_PAL ? 50 : 60);
   glide64_frameskip_reset();

   ReadSpecialSettings (name);

   // get the name of the ROM
'''
    changed += replace_once(
        GLIDE_MAIN,
        romopen_anchor,
        romopen_replacement,
        "glide64_frameskip_set_target_fps",
    )

    update_screen_anchor = '''void glide64UpdateScreen (void)
{
   bool forced_update = false;
'''
    update_screen_replacement = '''void glide64UpdateScreen (void)
{
   bool forced_update = false;

   /* Official Glide64mk2 frameskip updates its scheduler on each VI and
    * applies the decision to the following graphics display list. */
   glide64_frameskip_update();
'''
    changed += replace_once(
        GLIDE_MAIN,
        update_screen_anchor,
        update_screen_replacement,
        "glide64_frameskip_update();",
    )

    # 6) Skip before display-list setup/rendering, and signal DP completion just
    # like the original optional Glide64mk2 FrameSkipper path. The modern core's
    # task-end logic consumes/defer-delivers this bit correctly.
    rdp_include_anchor = '''#include "GlideExtensions.h"
#include "rdp.h"
'''
    rdp_include_replacement = '''#include "GlideExtensions.h"
#include "FrameSkipper_glide64.h"
#include "rdp.h"
'''
    changed += replace_once(
        GLIDE_RDP,
        rdp_include_anchor,
        rdp_include_replacement,
        '"FrameSkipper_glide64.h"',
    )

    process_anchor = '''void glide64ProcessDList(void)
{
  uint32_t dlist_start, dlist_length, a;

  no_dlist            = false;
'''
    process_replacement = '''void glide64ProcessDList(void)
{
  uint32_t dlist_start, dlist_length, a;

  if (glide64_frameskip_will_skip_next())
  {
    /* MI_INTR_DP == 0x20. Preserve task completion while avoiding the costly
     * display-list parsing/rendering work for this frame. */
    *gfx_info.MI_INTR_REG |= 0x20;
    if (gfx_info.CheckInterrupts)
      gfx_info.CheckInterrupts();
    return;
  }

  no_dlist            = false;
'''
    changed += replace_once(
        GLIDE_RDP,
        process_anchor,
        process_replacement,
        "glide64_frameskip_will_skip_next()",
    )

    print()
    if changed:
        print("Glide64 Frameskip Stage 2 aplicado.")
        print("Modos: Disabled / Automatic / 1 / 2 / 3 / 4 / 5")
        print("Revisa con: git diff --check && git diff --stat")
    else:
        print("No hubo cambios: el parche ya estaba aplicado.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
