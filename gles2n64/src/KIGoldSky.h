#ifndef GLN64_KIGOLD_SKY_H
#define GLN64_KIGOLD_SKY_H

#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* KI Gold's CPU-built sky can use a raw RDP triangle packet as a rectangle:
 * vertical H/M edges, with YM == YL. This is a conservative candidate filter,
 * NOT a proven identifier of every sky in every stage/revision.
 * Reference: geno55/kigold-recomp, patches/rt64/0001-rdp-triangle-clipping.patch.
 * Texture-rectangle commands are intentionally excluded, as are Z triangles.
 */
static int gln64_kigold_skip_sky(int enabled, const char *rom_name,
      const uint32_t *cmd)
{
   static const char name[] = "KILLER INSTINCT GOLD";
   unsigned i;
   int yh, ym, yl;
   int64_t width;
   if (!enabled || strlen(rom_name) != sizeof(name) - 1)
      return 0;
   for (i = 0; i < sizeof(name) - 1; ++i)
      if (toupper((unsigned char)rom_name[i]) != name[i])
         return 0;

   /* Shaded + textured, no Z (0x0e or its 0xce encoding). */
   if (((cmd[0] >> 24) & 0x3f) != 0x0e)
      return 0;
   yh = (int)(cmd[1] & 0x3fff);
   ym = (int)((cmd[1] >> 16) & 0x3fff);
   yl = (int)(cmd[0] & 0x3fff);
   if (yh & 0x2000) yh -= 0x4000;
   if (ym & 0x2000) ym -= 0x4000;
   if (yl & 0x2000) yl -= 0x4000;
   width = (int64_t)(int32_t)cmd[6] - (int64_t)(int32_t)cmd[4];
   if (width < 0) width = -width;

   /* Require a large flat-topped rectangle. Sizes are conservative heuristic
    * thresholds in native RDP units, independent of output resolution. */
   return ym == yl && yl - yh >= 32 * 4 &&
         cmd[5] == 0 && cmd[7] == 0 && width >= 128LL * 65536;
}
#endif
