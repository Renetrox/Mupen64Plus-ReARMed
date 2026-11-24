#include <assert.h>
#include "../glide2gl/src/Glide64/KIGoldSky.h"

int main(void)
{
   assert(glide64_kigold_skip_sky(1, 1, 1, 1, 0, 1, 0x6134, 0xab50));
   /* Alpha-combiner fields must not change the packed color key. */
   assert(glide64_kigold_skip_sky(1, 1, 1, 1, 0, 1, 0x01236134, 0x0456ab50));
   assert(!glide64_kigold_skip_sky(0, 1, 1, 1, 0, 1, 0x6134, 0xab50));
   assert(!glide64_kigold_skip_sky(1, 0, 1, 1, 0, 1, 0x6134, 0xab50));
   assert(!glide64_kigold_skip_sky(1, 1, 1, 1, 1, 1, 0x6134, 0xab50));
   assert(!glide64_kigold_skip_sky(1, 1, 0, 1, 0, 1, 0x6134, 0xab50));
   assert(!glide64_kigold_skip_sky(1, 1, 1, 0, 0, 1, 0x6134, 0xab50));
   assert(!glide64_kigold_skip_sky(1, 1, 1, 1, 0, 0, 0x6134, 0xab50));
   /* Neighboring combiners are used by other scenery/effects. */
   assert(!glide64_kigold_skip_sky(1, 1, 1, 1, 0, 1, 0x6134, 0xa750));
   assert(!glide64_kigold_skip_sky(1, 1, 1, 1, 0, 1, 0x6134, 0xe2f0));
   assert(glide64_kigold_hide_eb31(1, 1, 1, 1, 0, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(0, 1, 1, 1, 0, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 0, 1, 1, 0, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 1, 1, 1, 1, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 1, 1, 1, 0, 1, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 1, 0, 1, 0, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 1, 1, 0, 0, 0, 0xeb31, 0xeb31));
   assert(!glide64_kigold_hide_eb31(1, 1, 1, 1, 0, 0, 0x6134, 0xab50));
   return 0;
}
