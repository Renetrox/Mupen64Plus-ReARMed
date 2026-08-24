#ifndef FZ_PLUGIN_BRIDGE_H
#define FZ_PLUGIN_BRIDGE_H

#include <stdint.h>
#include "../mupen64plus-core/src/api/m64p_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FZ_PLUGIN_BRIDGE_MAGIC 0x465A4252u /* 'FZBR' */
#define FZ_PLUGIN_BRIDGE_ABI   1u

/* Match Mupen64Plus' osal_dynlib_getproc()/dlsym-style API: callers receive
 * an opaque symbol pointer and cast it to the required function type. */
typedef void *(*fz_plugin_get_proc_t)(const char *name);

struct fz_plugin_core_bridge
{
   uint32_t magic;
   uint32_t abi_version;
   fz_plugin_get_proc_t get_proc;
};

/* Internal to ReARMed. The returned pointer is passed as CoreLibHandle to
 * unmodified-style Mupen64Plus FZ plugins. It is not exported by libretro/link.T.
 */
const struct fz_plugin_core_bridge *fz_plugin_bridge_get(void);

#ifdef __cplusplus
}
#endif

#endif /* FZ_PLUGIN_BRIDGE_H */
