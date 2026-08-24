#ifndef FZ_GFX_PLUGIN_H
#define FZ_GFX_PLUGIN_H

#include "api/m64p_types.h"
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load the external Rice renderer from the FZ lineage and fill the same
 * graphics-function table used by the statically linked renderers. */
m64p_error fz_gfx_plugin_load_rice(gfx_plugin_functions *out);

/* Keep the plugin loaded across ROMs; shut it down only when the libretro core
 * itself is deinitialised. */
void fz_gfx_plugin_unload(void);
int  fz_gfx_plugin_is_loaded(void);
const char *fz_gfx_plugin_loaded_path(void);

#ifdef __cplusplus
}
#endif

#endif /* FZ_GFX_PLUGIN_H */
