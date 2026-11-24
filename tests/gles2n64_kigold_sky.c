#include <assert.h>
#include "../gles2n64/src/KIGoldSky.h"

int main(void)
{
   uint32_t cmd[8] = {0xce800000u | (100 * 4), (100 * 4) << 16,
         0, 0, 0, 0, 320u << 16, 0};
   assert(gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   assert(gln64_kigold_skip_sky(1, "KILLER INSTINCT GOLD", cmd));
   assert(!gln64_kigold_skip_sky(0, "Killer Instinct Gold", cmd));
   assert(!gln64_kigold_skip_sky(1, "SUPER MARIO 64", cmd));
   cmd[0] = 0xcf800000u | (100 * 4); /* depth-bearing floor */
   assert(!gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   cmd[0] = 0xe4000000u; /* HUD/text/sprite texture rectangle */
   assert(!gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   cmd[0] = 0xce800000u | (100 * 4);
   cmd[5] = 1; /* sloped major edge */
   assert(!gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   cmd[5] = 0;
   cmd[1] = (99 * 4) << 16; /* ordinary non-rectangular triangle */
   assert(!gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   cmd[1] = (100 * 4) << 16;
   cmd[6] = 16u << 16; /* small matching primitive */
   assert(!gln64_kigold_skip_sky(1, "Killer Instinct Gold", cmd));
   return 0;
}
