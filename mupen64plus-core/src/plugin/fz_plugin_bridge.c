/*
 * Mupen64Plus-ReARMed - bridge for external Mupen64Plus FZ plugins.
 *
 * libretro/link.T intentionally hides the embedded Mupen64Plus core API from
 * the dynamic symbol table. FZ plugins normally receive a dlopen() handle and
 * resolve Config and VidExt functions with dlsym(). ReARMed keeps that symbol
 * isolation and instead passes this explicit resolver as CoreLibHandle.
 */

#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "../api/m64p_common.h"
#include "../api/m64p_config.h"
#include "../api/m64p_vidext.h"
#include "../../../libretro/fz_plugin_bridge.h"

/* Rice FZ asks the video extension to set attributes and immediately reads
 * them back. The libretro vidext shim historically implements SetAttribute
 * as a no-op and has no GetAttribute implementation, so retain the requested
 * values here. This is deliberately bridge-local and does not change the
 * behaviour of the existing statically linked renderers. */
#define FZ_GL_ATTR_MAX ((int) M64P_GL_CONTEXT_PROFILE_MASK)
static int fz_gl_attr_values[FZ_GL_ATTR_MAX + 1];
static unsigned char fz_gl_attr_valid[FZ_GL_ATTR_MAX + 1];

static m64p_error fz_bridge_gl_set_attribute(m64p_GLattr attr, int value)
{
   int index = (int) attr;

   if (index < 1 || index > FZ_GL_ATTR_MAX)
      return M64ERR_INPUT_INVALID;

   fz_gl_attr_values[index] = value;
   fz_gl_attr_valid[index]  = 1;

   return VidExt_GL_SetAttribute(attr, value);
}

static m64p_error fz_bridge_gl_get_attribute(m64p_GLattr attr, int *value)
{
   int index = (int) attr;

   if (!value)
      return M64ERR_INPUT_ASSERT;
   if (index < 1 || index > FZ_GL_ATTR_MAX)
      return M64ERR_INPUT_INVALID;

   if (fz_gl_attr_valid[index])
   {
      *value = fz_gl_attr_values[index];
      return M64ERR_SUCCESS;
   }

   /* Conservative libretro/GLES defaults for attributes queried before an
    * explicit SetAttribute. FZ Rice normally sets the important ones first. */
   switch (attr)
   {
      case M64P_GL_DOUBLEBUFFER:          *value = 1;  break;
      case M64P_GL_BUFFER_SIZE:           *value = 32; break;
      case M64P_GL_DEPTH_SIZE:            *value = 24; break;
      case M64P_GL_RED_SIZE:
      case M64P_GL_GREEN_SIZE:
      case M64P_GL_BLUE_SIZE:
      case M64P_GL_ALPHA_SIZE:            *value = 8;  break;
      case M64P_GL_CONTEXT_MAJOR_VERSION: *value = 2;  break;
      case M64P_GL_CONTEXT_MINOR_VERSION: *value = 0;  break;
      case M64P_GL_CONTEXT_PROFILE_MASK:  *value = M64P_GL_CONTEXT_PROFILE_ES; break;
      default:                            *value = 0;  break;
   }

   return M64ERR_SUCCESS;
}

static m64p_function fz_plugin_bridge_get_proc(const char *name)
{
   if (!name)
      return NULL;

#define FZ_PROC(symbol) \
   if (strcmp(name, #symbol) == 0) return (m64p_function) symbol

   FZ_PROC(CoreGetAPIVersions);

   FZ_PROC(ConfigOpenSection);
   FZ_PROC(ConfigSetParameter);
   FZ_PROC(ConfigSetParameterHelp);
   FZ_PROC(ConfigGetParameter);
   FZ_PROC(ConfigSetDefaultInt);
   FZ_PROC(ConfigSetDefaultFloat);
   FZ_PROC(ConfigSetDefaultBool);
   FZ_PROC(ConfigSetDefaultString);
   FZ_PROC(ConfigGetParamInt);
   FZ_PROC(ConfigGetParamFloat);
   FZ_PROC(ConfigGetParamBool);
   FZ_PROC(ConfigGetParamString);
   FZ_PROC(ConfigGetSharedDataFilepath);
   FZ_PROC(ConfigGetUserConfigPath);
   FZ_PROC(ConfigGetUserDataPath);
   FZ_PROC(ConfigGetUserCachePath);

   FZ_PROC(VidExt_Init);
   FZ_PROC(VidExt_Quit);
   FZ_PROC(VidExt_ListFullscreenModes);
   FZ_PROC(VidExt_SetVideoMode);
   FZ_PROC(VidExt_SetCaption);
   FZ_PROC(VidExt_ToggleFullScreen);
   FZ_PROC(VidExt_ResizeWindow);
   FZ_PROC(VidExt_GL_GetProcAddress);
   if (strcmp(name, "VidExt_GL_SetAttribute") == 0)
      return (m64p_function) fz_bridge_gl_set_attribute;
   if (strcmp(name, "VidExt_GL_GetAttribute") == 0)
      return (m64p_function) fz_bridge_gl_get_attribute;
   FZ_PROC(VidExt_GL_SwapBuffers);

#undef FZ_PROC
   return NULL;
}

static const struct fz_plugin_core_bridge fz_core_bridge = {
   FZ_PLUGIN_BRIDGE_MAGIC,
   FZ_PLUGIN_BRIDGE_ABI,
   fz_plugin_bridge_get_proc
};

const struct fz_plugin_core_bridge *fz_plugin_bridge_get(void)
{
   return &fz_core_bridge;
}
