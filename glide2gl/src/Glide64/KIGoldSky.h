#ifndef GLIDE64_KIGOLD_SKY_H
#define GLIDE64_KIGOLD_SKY_H

#include <stdint.h>

/* Color packing matches Combine().
 * 6134ab50 is documented as "sky, Killer Instinct" in Combine.c.
 * eb31eb31 was isolated following visible grey clouds in Sabrewulf/Spinal;
 * the user reported the isolation test worked. It remains experimental:
 * combiner keys are not unique texture identities or stage identifiers.
 */
static int glide64_kigold_skip_sky(int enabled, int is_kigold,
      int shade, int texture, int zbuffer, unsigned cycle_type,
      uint32_t cycle1, uint32_t cycle2)
{
   const uint32_t color_key = (cycle1 << 16) | (cycle2 & 0xffffu);
   if (!enabled || !is_kigold || !shade || !texture || zbuffer)
      return 0;
   return (cycle_type == 1 && color_key == 0x6134ab50u) ||
         (cycle_type == 0 && color_key == 0xeb31eb31u);
}
#endif
