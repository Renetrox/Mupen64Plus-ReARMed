#ifndef GLIDE64_KIGOLD_SKY_H
#define GLIDE64_KIGOLD_SKY_H

#include <stdint.h>

/* Combine.c labels color key 0x6134ab50 as "sky, Killer Instinct":
 * (shade-prim)*t0+prim, (combined-env)*shade_alpha+env.
 * Use the same packing as Combine(), before update() or texture uploads.
 * This identifies one known sky combiner, not every possible stage backdrop.
 */
static int glide64_kigold_skip_sky(int enabled, int is_kigold,
      int shade, int texture, int zbuffer, unsigned cycle_type,
      uint32_t cycle1, uint32_t cycle2)
{
   const uint32_t color_key = (cycle1 << 16) | (cycle2 & 0xffffu);
   return enabled && is_kigold && shade && texture && !zbuffer &&
         cycle_type == 1 /* G_CYC_2CYCLE */ && color_key == 0x6134ab50u;
}
/* Diagnostic isolation only: eb31eb31 is NOT a confirmed sky identifier.
 * This intentionally may hide floors or other matching raw geometry. */
static int glide64_kigold_hide_eb31(int enabled, int is_kigold,
      int shade, int texture, int zbuffer, unsigned cycle_type,
      uint32_t cycle1, uint32_t cycle2)
{
   const uint32_t color_key = (cycle1 << 16) | (cycle2 & 0xffffu);
   return enabled && is_kigold && shade && texture && !zbuffer &&
         cycle_type == 0 && color_key == 0xeb31eb31u;
}
#endif
