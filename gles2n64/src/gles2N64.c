
#ifndef _WIN32
#include <dlfcn.h>
#endif

#include <string.h>

#include "m64p_types.h"
#include "m64p_plugin.h"

#include "gles2N64.h"
#include "Debug.h"
#include "OpenGL.h"
#include "N64.h"
#include "RSP.h"
#include "RDP.h"
#include "VI.h"
#include "Config.h"
#include "Textures.h"
#include "ShaderCombiner.h"
#include "3DMath.h"
#include "../../libretro/libretro_private.h"

uint32_t    last_good_ucode     = (uint32_t) -1;
void        (*renderCallback)() = NULL;

/* Native FrameSkipper ported from Mupen64Plus FZ.
 *
 * FZ updates the scheduler from UpdateScreen() (VI cadence) and consumes the
 * decision from ProcessDList(). All DLists before the next UpdateScreen share
 * the same skip state, so a scene and its overlays are not split apart.
 *
 * Modern parallel-n64 adaptation:
 * - timing uses libretro's monotonic performance clock;
 * - on a skipped HLE task we set MI_INTR_DP but do NOT call CheckInterrupts()
 *   and do NOT raise MI_INTR_SP here. Current rsp_core consumes DP at task end
 *   and schedules it with normal deferred timing, while rsp-hle already owns
 *   the SP task-done interrupt path. */
#define GLN64_FRAMESKIP_AUTO_MIN (-5)

typedef struct
{
   int mode;
   int max_skips;
   int target_fps;
   int skip_counter;
   uint64_t initial_usec;
   uint64_t virtual_count;
} gln64_frame_skipper_t;

static gln64_frame_skipper_t gln64_frame_skipper =
{
   999, /* force initial mode sync */
   0,
   60,
   0,
   0,
   0
};

static void gln64_frameskip_start(void)
{
   gln64_frame_skipper.initial_usec = 0;
   gln64_frame_skipper.virtual_count = 0;
   gln64_frame_skipper.skip_counter = 0;
}

static void gln64_frameskip_sync_mode(void)
{
   int mode = parallel_n64_get_gles2n64_frameskip_mode();

   if (mode < GLN64_FRAMESKIP_AUTO_MIN)
    mode = GLN64_FRAMESKIP_AUTO_MIN;
 if (mode > 5)
    mode = 5;

 if (gln64_frame_skipper.mode != mode)
 {
    gln64_frame_skipper.mode = mode;
    gln64_frame_skipper.max_skips =
       (mode < 0) ? -mode : ((mode > 0) ? mode : 0);
    gln64_frameskip_start();
 }
}

static int gln64_frameskip_will_skip_next(void)
{
   gln64_frameskip_sync_mode();

   return gln64_frame_skipper.mode != 0 &&
          gln64_frame_skipper.skip_counter > 0;
}

static void gln64_frameskip_update(void)
{
   uint64_t now;
   uint64_t elapsed;
   uint64_t real_count;

   gln64_frameskip_sync_mode();

   if (gln64_frame_skipper.mode == 0)
      return;

   /* FZ manual mode: render one update interval, skip N, repeat. */
   if (gln64_frame_skipper.mode > 0)
   {
      if (++gln64_frame_skipper.skip_counter >
          gln64_frame_skipper.max_skips)
         gln64_frame_skipper.skip_counter = 0;
      return;
   }

   /* FZ automatic virtual-frame catch-up. */
   now = parallel_n64_get_time_usec();

   if (gln64_frame_skipper.initial_usec == 0 ||
       now <= gln64_frame_skipper.initial_usec)
   {
      gln64_frame_skipper.initial_usec = now;
      gln64_frame_skipper.virtual_count = 0;
      gln64_frame_skipper.skip_counter = 0;
      return;
   }

   elapsed = now - gln64_frame_skipper.initial_usec;
   real_count =
      (elapsed * (uint64_t)gln64_frame_skipper.target_fps) / 1000000u;

   gln64_frame_skipper.virtual_count++;

   if (real_count >= gln64_frame_skipper.virtual_count)
   {
      if (real_count > gln64_frame_skipper.virtual_count &&
          gln64_frame_skipper.skip_counter <
             gln64_frame_skipper.max_skips)
      {
         gln64_frame_skipper.skip_counter++;
      }
      else
      {
         gln64_frame_skipper.virtual_count = real_count;
         gln64_frame_skipper.skip_counter = 0;
      }
   }
}

m64p_error gln64PluginStartup(m64p_dynlib_handle CoreLibHandle,
        void *Context, void (*DebugCallback)(void *, int, const char *))
{
   return M64ERR_SUCCESS;
}

m64p_error gln64PluginShutdown(void)
{
   OGL_Stop();  // paulscode, OGL_Stop missing from Yongzh's code
   return M64ERR_SUCCESS;
}

m64p_error gln64PluginGetVersion(m64p_plugin_type *PluginType,
        int *PluginVersion, int *APIVersion, const char **PluginNamePtr,
        int *Capabilities)
{
   /* set version info */
   if (PluginType != NULL)
      *PluginType = M64PLUGIN_GFX;

   if (PluginVersion != NULL)
      *PluginVersion = PLUGIN_VERSION;

   if (APIVersion != NULL)
      *APIVersion = PLUGIN_API_VERSION;

   if (PluginNamePtr != NULL)
      *PluginNamePtr = PLUGIN_NAME;

   if (Capabilities != NULL)
   {
      *Capabilities = 0;
   }

   return M64ERR_SUCCESS;
}

void gln64ChangeWindow (void)
{
}

void gln64MoveScreen (int xpos, int ypos)
{
}

int gln64InitiateGFX (GFX_INFO Gfx_Info)
{
    Config_gln64_LoadConfig();
    Config_gln64_LoadRomConfig(Gfx_Info.HEADER);
    Config_gln64_ApplyCoreOptions();

    OGL_Start();

    return 1;
}

void gln64ProcessDList(void)
{
    OGL.frame_dl++;

    /* FZ clears the new back buffer immediately after a real swap.
     * libretro has no plugin-owned swap here, so perform that clear
     * when the next graphics display list begins. */
    OGL_ApplyPendingBufferClear();

    if (gln64_frameskip_will_skip_next())
    {
        /* Mirror the renderer-side bookkeeping from Mupen64Plus FZ. */
        __RSP.busy = false;
        __RSP.DList++;

        /* The modern RSP core consumes this bit after the HLE task returns
         * and schedules DP_INT with the same deferred timing as normal. */
        *gfx_info.MI_INTR_REG |= MI_INTR_DP;
        return;
    }

    RSP_ProcessDList();
    OGL.mustRenderDlist = true;
}

void gln64ResizeVideoOutput(int Width, int Height)
{
}

void gln64RomClosed (void)
{
}

int gln64RomOpen (void)
{
    RSP_Init();
    OGL.frame_dl = 0;
    OGL.frame_prevdl = -1;
    OGL.mustRenderDlist = false;

    gln64_frameskip_sync_mode();
    gln64_frame_skipper.target_fps = config.romPAL ? 50 : 60;
    gln64_frameskip_start();

    return 1;
}

EXPORT void CALL RomResumed(void)
{
    gln64_frameskip_start();
}

void gln64ShowCFB (void)
{
}

void gln64UpdateScreen (void)
{
   /* FZ updates the skipper at VI/update cadence before deciding the next
    * ProcessDList() skip state. */
   gln64_frameskip_update();

   //has there been any display lists since last update ?
   if (OGL.frame_prevdl == OGL.frame_dl)
      return;

   OGL.frame_prevdl = OGL.frame_dl;

   if (OGL.mustRenderDlist)
   {
      OGL.screenUpdate=true;
      VI_UpdateScreen();
      OGL.mustRenderDlist = false;
   }
}

void gln64ViStatusChanged (void)
{
}

void gln64ViWidthChanged (void)
{
}

/******************************************************************
  Function: FrameBufferRead
  Purpose:  This function is called to notify the dll that the
            frame buffer memory is beening read at the given address.
            DLL should copy content from its render buffer to the frame buffer
            in N64 RDRAM
            DLL is responsible to maintain its own frame buffer memory addr list
            DLL should copy 4KB block content back to RDRAM frame buffer.
            Emulator should not call this function again if other memory
            is read within the same 4KB range

            Since depth buffer is also being watched, the reported addr
            may belong to depth buffer
  input:    addr        rdram address
            val         val
            size        1 = uint8, 2 = uint16, 4 = uint32
  output:   none
*******************************************************************/ 

void gln64FBRead(uint32_t addr)
{
}

/******************************************************************
  Function: FrameBufferWrite
  Purpose:  This function is called to notify the dll that the
            frame buffer has been modified by CPU at the given address.

            Since depth buffer is also being watched, the reported addr
            may belong to depth buffer

  input:    addr        rdram address
            val         val
            size        1 = uint8, 2 = uint16, 4 = uint32
  output:   none
*******************************************************************/ 

void gln64FBWrite(uint32_t addr, uint32_t size)
{
}

/************************************************************************
Function: FBGetFrameBufferInfo
Purpose:  This function is called by the emulator core to retrieve frame
          buffer information from the video plugin in order to be able
          to notify the video plugin about CPU frame buffer read/write
          operations

          size:
            = 1     byte
            = 2     word (16 bit) <-- this is N64 default depth buffer format
            = 4     dword (32 bit)

          when frame buffer information is not available yet, set all values
          in the FrameBufferInfo structure to 0

input:    FrameBufferInfo pinfo[6]
          pinfo is pointed to a FrameBufferInfo structure which to be
          filled in by this function
output:   Values are return in the FrameBufferInfo structure
          Plugin can return up to 6 frame buffer info
 ************************************************************************/

void gln64FBGetFrameBufferInfo(void *p)
{
}

// paulscode, API changed this to "ReadScreen2" in Mupen64Plus 1.99.4
void gln64ReadScreen2(void *dest, int *width, int *height, int front)
{
   /* TODO: 'int front' was added in 1.99.4.  What to do with this here? */
   OGL_ReadScreen(dest, width, height);
}

void gln64SetRenderingCallback(void (*callback)())
{
   renderCallback = callback;
}

EXPORT void CALL StartGL(void)
{
   OGL_Start();
}

EXPORT void CALL StopGL(void)
{
   OGL_Stop();
}

void gles2n64_reset(void)
{
   // HACK: Check for leaks!
   OGL_Stop();
   OGL_Start();
   RSP_Init();
   gln64_frameskip_start();
}
