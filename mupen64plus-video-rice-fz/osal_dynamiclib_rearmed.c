/*
 * ReARMed resolver adapter for the FZ Rice sidecar plugin.
 *
 * FZ normally calls dlsym(CoreLibHandle, ...). The libretro core hides its
 * Mupen64Plus symbols on purpose, so ReARMed passes an fz_plugin_core_bridge
 * as CoreLibHandle instead. Only this one source file differs from FZ's normal
 * Unix dynamic-library glue.
 */

#include "upstream/src/osal_dynamiclib.h"
#include "../libretro/fz_plugin_bridge.h"

m64p_function osal_dynlib_getproc(m64p_dynlib_handle LibHandle,
                                  const char *pccProcedureName)
{
   const struct fz_plugin_core_bridge *bridge =
      (const struct fz_plugin_core_bridge *) LibHandle;

   if (!bridge ||
       bridge->magic != FZ_PLUGIN_BRIDGE_MAGIC ||
       bridge->abi_version != FZ_PLUGIN_BRIDGE_ABI ||
       !bridge->get_proc)
      return NULL;

   return bridge->get_proc(pccProcedureName);
}
