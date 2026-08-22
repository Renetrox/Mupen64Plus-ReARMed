/*
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
