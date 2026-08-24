/*
 * Runtime loader for external Mupen64Plus-FZ graphics plugins.
 *
 * The plugin remains a normal Mupen64Plus video-plugin shared library. The
 * libretro core supplies its API through fz_plugin_bridge rather than exposing
 * all embedded Mupen64Plus symbols from parallel_n64_libretro.so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef HMODULE fz_dynlib_t;
#else
#include <dlfcn.h>
typedef void *fz_dynlib_t;
#endif

#include <libretro.h>

#include "api/callbacks.h"
#include "api/m64p_common.h"
#include "api/m64p_plugin.h"
#include "mupen64plus-next_common.h"
#include "fz_gfx_plugin.h"
#include "../../../libretro/fz_plugin_bridge.h"

#define FZ_PATH_MAX 4096

#if defined(_WIN32)
#define FZ_RICEFZ_FILENAME "mupen64plus-video-rice-fz.dll"
#else
#define FZ_RICEFZ_FILENAME "mupen64plus-video-rice-fz.so"
#endif

static fz_dynlib_t        fz_handle = 0;
static ptr_PluginShutdown fz_shutdown = NULL;
static gfx_plugin_functions fz_cached_gfx;
static char               fz_loaded_path[FZ_PATH_MAX];

static void fz_plugin_debug(void *context, int level, const char *message)
{
   (void) context;
   DebugMessage(level, "RiceFZ: %s", message ? message : "(null)");
}

static fz_dynlib_t fz_dynlib_open(const char *path)
{
#if defined(_WIN32)
   return LoadLibraryA(path);
#else
   return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void *fz_dynlib_symbol(fz_dynlib_t handle, const char *name)
{
#if defined(_WIN32)
   return (void *) GetProcAddress(handle, name);
#else
   return dlsym(handle, name);
#endif
}

static void fz_dynlib_close(fz_dynlib_t handle)
{
   if (!handle)
      return;
#if defined(_WIN32)
   FreeLibrary(handle);
#else
   dlclose(handle);
#endif
}

static const char *fz_dynlib_error(void)
{
#if defined(_WIN32)
   return "LoadLibrary/GetProcAddress failed";
#else
   {
      const char *err = dlerror();
      return err ? err : "unknown dynamic-loader error";
   }
#endif
}

static fz_dynlib_t fz_try_open(const char *path)
{
   fz_dynlib_t handle;

   if (!path || !*path)
      return 0;

#if !defined(_WIN32)
   dlerror();
#endif
   handle = fz_dynlib_open(path);
   if (handle)
   {
      strncpy(fz_loaded_path, path, sizeof(fz_loaded_path) - 1);
      fz_loaded_path[sizeof(fz_loaded_path) - 1] = '\0';
   }
   return handle;
}

static fz_dynlib_t fz_open_rice_library(void)
{
   const char *override_path = getenv("M64P_RICEFZ_PATH");
   const char *core_path = NULL;
   char candidate[FZ_PATH_MAX];

   /* Developer/test override. This is also handy while the sidecar is not yet
    * installed beside the libretro core. */
   if (override_path && *override_path)
   {
      fz_dynlib_t h = fz_try_open(override_path);
      if (h)
         return h;
      DebugMessage(M64MSG_WARNING, "RiceFZ: couldn't load M64P_RICEFZ_PATH '%s': %s",
                   override_path, fz_dynlib_error());
   }

   /* Production layout: sidecar beside parallel_n64_libretro.so. */
   if (environ_cb &&
       environ_cb(RETRO_ENVIRONMENT_GET_LIBRETRO_PATH, &core_path) &&
       core_path && *core_path)
   {
      const char *slash = strrchr(core_path, '/');
#if defined(_WIN32)
      const char *backslash = strrchr(core_path, '\\');
      if (!slash || (backslash && backslash > slash))
         slash = backslash;
#endif
      if (slash)
      {
         size_t dir_len = (size_t)(slash - core_path);
         if (dir_len + 1 + strlen(FZ_RICEFZ_FILENAME) + 1 < sizeof(candidate))
         {
            memcpy(candidate, core_path, dir_len);
            candidate[dir_len] = '\0';
#if defined(_WIN32)
            snprintf(candidate + dir_len, sizeof(candidate) - dir_len,
                     "\\%s", FZ_RICEFZ_FILENAME);
#else
            snprintf(candidate + dir_len, sizeof(candidate) - dir_len,
                     "/%s", FZ_RICEFZ_FILENAME);
#endif
            {
               fz_dynlib_t h = fz_try_open(candidate);
               if (h)
                  return h;
            }
         }
      }
   }

   /* Source-tree convenience for development builds. */
#if !defined(_WIN32)
   {
      fz_dynlib_t h = fz_try_open("./mupen64plus-video-rice-fz/mupen64plus-video-rice-fz.so");
      if (h)
         return h;
   }
#endif

   /* Finally let the platform dynamic-loader search its normal path. */
   return fz_try_open(FZ_RICEFZ_FILENAME);
}

#define FZ_RESOLVE_REQUIRED(field, type, symbol_name)                         \
   do {                                                                        \
      (field) = (type) fz_dynlib_symbol(fz_handle, (symbol_name));             \
      if (!(field))                                                            \
      {                                                                        \
         DebugMessage(M64MSG_ERROR, "RiceFZ: missing required export %s",     \
                      (symbol_name));                                           \
         goto fail;                                                            \
      }                                                                        \
   } while (0)

m64p_error fz_gfx_plugin_load_rice(gfx_plugin_functions *out)
{
   ptr_PluginStartup startup = NULL;
   ptr_PluginGetVersion get_version = NULL;
   m64p_plugin_type plugin_type = M64PLUGIN_NULL;
   int plugin_version = 0;
   int api_version = 0;
   int capabilities = 0;
   const char *plugin_name = NULL;
   m64p_error err;
   int started = 0;

   if (!out)
      return M64ERR_INPUT_ASSERT;

   if (fz_handle)
   {
      *out = fz_cached_gfx;
      return M64ERR_SUCCESS;
   }

   memset(out, 0, sizeof(*out));
   memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));
   fz_loaded_path[0] = '\0';

   fz_handle = fz_open_rice_library();
   if (!fz_handle)
   {
      DebugMessage(M64MSG_ERROR,
                   "RiceFZ: couldn't load %s. Put it beside the libretro core or set M64P_RICEFZ_PATH. Last error: %s",
                   FZ_RICEFZ_FILENAME, fz_dynlib_error());
      return M64ERR_PLUGIN_FAIL;
   }

   FZ_RESOLVE_REQUIRED(get_version, ptr_PluginGetVersion, "PluginGetVersion");
   FZ_RESOLVE_REQUIRED(startup, ptr_PluginStartup, "PluginStartup");
   FZ_RESOLVE_REQUIRED(fz_shutdown, ptr_PluginShutdown, "PluginShutdown");

   err = get_version(&plugin_type, &plugin_version, &api_version,
                     &plugin_name, &capabilities);
   if (err != M64ERR_SUCCESS || plugin_type != M64PLUGIN_GFX)
   {
      DebugMessage(M64MSG_ERROR, "RiceFZ: sidecar is not a compatible graphics plugin");
      goto fail;
   }

   if ((api_version & 0xffff0000) != (GFX_API_VERSION & 0xffff0000))
   {
      DebugMessage(M64MSG_ERROR,
                   "RiceFZ: graphics API mismatch (plugin %d.%d.%d, core %d.%d.%d)",
                   (api_version >> 16) & 0xffff, (api_version >> 8) & 0xff, api_version & 0xff,
                   (GFX_API_VERSION >> 16) & 0xffff, (GFX_API_VERSION >> 8) & 0xff, GFX_API_VERSION & 0xff);
      goto fail;
   }

   err = startup((m64p_dynlib_handle) fz_plugin_bridge_get(), NULL,
                 fz_plugin_debug);
   if (err != M64ERR_SUCCESS)
   {
      DebugMessage(M64MSG_ERROR, "RiceFZ: PluginStartup failed with error %d", err);
      goto fail;
   }
   started = 1;

   FZ_RESOLVE_REQUIRED(out->getVersion, ptr_PluginGetVersion, "PluginGetVersion");
   FZ_RESOLVE_REQUIRED(out->changeWindow, ptr_ChangeWindow, "ChangeWindow");
   FZ_RESOLVE_REQUIRED(out->initiateGFX, ptr_InitiateGFX, "InitiateGFX");
   FZ_RESOLVE_REQUIRED(out->moveScreen, ptr_MoveScreen, "MoveScreen");
   FZ_RESOLVE_REQUIRED(out->processDList, ptr_ProcessDList, "ProcessDList");
   FZ_RESOLVE_REQUIRED(out->processRDPList, ptr_ProcessRDPList, "ProcessRDPList");
   FZ_RESOLVE_REQUIRED(out->romClosed, ptr_RomClosed, "RomClosed");
   FZ_RESOLVE_REQUIRED(out->romOpen, ptr_RomOpen, "RomOpen");
   FZ_RESOLVE_REQUIRED(out->showCFB, ptr_ShowCFB, "ShowCFB");
   FZ_RESOLVE_REQUIRED(out->updateScreen, ptr_UpdateScreen, "UpdateScreen");
   FZ_RESOLVE_REQUIRED(out->viStatusChanged, ptr_ViStatusChanged, "ViStatusChanged");
   FZ_RESOLVE_REQUIRED(out->viWidthChanged, ptr_ViWidthChanged, "ViWidthChanged");
   FZ_RESOLVE_REQUIRED(out->readScreen, ptr_ReadScreen2, "ReadScreen2");
   FZ_RESOLVE_REQUIRED(out->setRenderingCallback, ptr_SetRenderingCallback, "SetRenderingCallback");
   FZ_RESOLVE_REQUIRED(out->resizeVideoOutput, ptr_ResizeVideoOutput, "ResizeVideoOutput");
   FZ_RESOLVE_REQUIRED(out->fBRead, ptr_FBRead, "FBRead");
   FZ_RESOLVE_REQUIRED(out->fBWrite, ptr_FBWrite, "FBWrite");
   FZ_RESOLVE_REQUIRED(out->fBGetFrameBufferInfo, ptr_FBGetFrameBufferInfo, "FBGetFrameBufferInfo");

   fz_cached_gfx = *out;

   DebugMessage(M64MSG_INFO, "RiceFZ: loaded %s v%d.%d.%d from %s",
                plugin_name ? plugin_name : "Rice FZ",
                (plugin_version >> 16) & 0xffff,
                (plugin_version >> 8) & 0xff,
                plugin_version & 0xff,
                fz_loaded_path[0] ? fz_loaded_path : FZ_RICEFZ_FILENAME);

   (void) capabilities;
   return M64ERR_SUCCESS;

fail:
   if (started && fz_shutdown)
      fz_shutdown();
   fz_shutdown = NULL;
   fz_dynlib_close(fz_handle);
   fz_handle = 0;
   fz_loaded_path[0] = '\0';
   memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));
   memset(out, 0, sizeof(*out));
   return M64ERR_PLUGIN_FAIL;
}

void fz_gfx_plugin_unload(void)
{
   if (!fz_handle)
      return;

   if (fz_shutdown)
      fz_shutdown();

   fz_shutdown = NULL;
   fz_dynlib_close(fz_handle);
   fz_handle = 0;
   fz_loaded_path[0] = '\0';
   memset(&fz_cached_gfx, 0, sizeof(fz_cached_gfx));
}

int fz_gfx_plugin_is_loaded(void)
{
   return fz_handle != 0;
}

const char *fz_gfx_plugin_loaded_path(void)
{
   return fz_loaded_path;
}

#undef FZ_RESOLVE_REQUIRED
