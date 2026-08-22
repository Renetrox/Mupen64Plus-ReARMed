/*
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
