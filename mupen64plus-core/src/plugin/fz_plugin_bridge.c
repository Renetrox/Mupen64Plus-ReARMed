/*
 * Mupen64Plus-ReARMed - bridge for external Mupen64Plus FZ plugins.
 *
 * libretro/link.T intentionally hides the embedded Mupen64Plus core API from
 * the dynamic symbol table. FZ plugins normally receive a dlopen() handle and
 * resolve Config and VidExt functions with dlsym(). ReARMed keeps that symbol
 * isolation and instead passes this explicit resolver as CoreLibHandle.
 */

#include <stddef.h>
#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "../api/m64p_common.h"
#include "../api/m64p_config.h"
#include "../api/m64p_vidext.h"
#include <glsm/glsm.h>
#include "../../../libretro/fz_plugin_bridge.h"

/* The current libretro config backend no longer stores per-parameter help
 * strings, but FZ Rice requires ConfigSetParameterHelp to be present during
 * PluginStartup and calls it while refreshing old config metadata. Keep that
 * ABI surface bridge-local: values are handled by the real Config* functions,
 * while help-text updates are a successful no-op. */
static m64p_error fz_bridge_config_set_parameter_help(
      m64p_handle ConfigSectionHandle,
      const char *ParamName,
      const char *ParamHelp)
{
   (void) ConfigSectionHandle;
   (void) ParamName;
   (void) ParamHelp;
   return M64ERR_SUCCESS;
}

/* ReARMed's embedded config backend still reports API 2.2.0. FZ Rice refuses
 * to start unless Config API 2.3.0+ is reported, because that generation of
 * the plugin expects the newer ConfigSetParameterHelp surface. The bridge
 * supplies that call above, so advertise 2.3.0 only to external FZ plugins.
 * Do not globally bump CONFIG_API_VERSION: the built-in libretro core keeps
 * reporting its real native API level to every other caller. */
static m64p_error fz_bridge_core_get_api_versions(
      int *ConfigVersion,
      int *DebugVersion,
      int *VidextVersion,
      int *ExtraVersion)
{
   m64p_error err = CoreGetAPIVersions(ConfigVersion, DebugVersion,
                                       VidextVersion, ExtraVersion);

   if (err != M64ERR_SUCCESS)
      return err;

   if (ConfigVersion != NULL && *ConfigVersion < 0x020300)
      *ConfigVersion = 0x020300;

   return M64ERR_SUCCESS;
}

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

/* The legacy vidext_libretro implementation currently asks GLSM for the proc
 * callback with a NULL data pointer and then dereferences an untouched local
 * callback. Keep that historical path unchanged for the built-in renderers;
 * external FZ plugins get the correct GLSM resolver through this bridge. */
static void *fz_bridge_gl_get_proc_address(const char *proc)
{
   if (!proc)
      return NULL;
   return glsm_get_proc_address(proc);
}

/* Standalone Mupen64Plus plugins render with raw OpenGL entry points. GLSM's
 * state manager, on the other hand, redirects the built-in renderers to the
 * framebuffer supplied by RetroArch. A raw external plugin therefore sees
 * framebuffer 0 unless we explicitly bind the libretro target. This helper is
 * called by the RiceFZ loader immediately before every entry point that may
 * draw or read the current frame.
 *
 * Use the raw frontend GL proc here as well: calling an rgl/glsm wrapper would
 * only update GLSM's cached state, which is precisely what the external plugin
 * bypasses. GL_FRAMEBUFFER is 0x8D40 for core/ARB/EXT/OES FBO APIs. */
void fz_plugin_bridge_bind_current_framebuffer(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_bind_framebuffer_t)(unsigned int, unsigned int);
   static fz_bind_framebuffer_t bind_framebuffer = NULL;
   unsigned int framebuffer;

   if (!bind_framebuffer)
   {
      bind_framebuffer = (fz_bind_framebuffer_t)
         glsm_get_proc_address("glBindFramebuffer");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferEXT");
      if (!bind_framebuffer)
         bind_framebuffer = (fz_bind_framebuffer_t)
            glsm_get_proc_address("glBindFramebufferOES");
   }

   if (!bind_framebuffer)
      return;

   framebuffer = (unsigned int) glsm_get_current_framebuffer();
   bind_framebuffer(0x8D40u, framebuffer);
#endif
}

static void *fz_plugin_bridge_get_proc(const char *name)
{
   if (!name)
      return NULL;

#define FZ_PROC(symbol) \
   if (strcmp(name, #symbol) == 0) return (void *) symbol

   if (strcmp(name, "CoreGetAPIVersions") == 0)
      return (void *) fz_bridge_core_get_api_versions;

   FZ_PROC(ConfigOpenSection);
   FZ_PROC(ConfigSetParameter);
   if (strcmp(name, "ConfigSetParameterHelp") == 0)
      return (void *) fz_bridge_config_set_parameter_help;
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
   if (strcmp(name, "VidExt_GL_GetProcAddress") == 0)
      return (void *) fz_bridge_gl_get_proc_address;
   if (strcmp(name, "VidExt_GL_SetAttribute") == 0)
      return (void *) fz_bridge_gl_set_attribute;
   if (strcmp(name, "VidExt_GL_GetAttribute") == 0)
      return (void *) fz_bridge_gl_get_attribute;
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
