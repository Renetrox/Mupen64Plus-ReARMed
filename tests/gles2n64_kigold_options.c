#include <assert.h>
#include "libretro_core_options.h"
int main(void) {
 unsigned i, found = 0;
 m64p_rearmed_prepare_plugin_options();
 assert(m64p_rearmed_option_defs);
 for (i = 0; m64p_rearmed_option_defs[i].key; ++i) {
  const struct retro_core_option_v2_definition *o = &m64p_rearmed_option_defs[i];
  if (!strcmp(o->key, CORE_NAME "-gln64-kigold-skip-sky")) {
   ++found;
   assert(!strcmp(o->default_value, "disabled"));
   assert(!strcmp(o->category_key, "gles2n64"));
  }
 }
 assert(found == 1);
 assert(i == m64p_rearmed_count_base_options() + 2);
 free(m64p_rearmed_option_defs);
 return 0;
}
