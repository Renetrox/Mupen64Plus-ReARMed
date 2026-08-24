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
#include "../api/callbacks.h"
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

/* Frontend viewport state for the external FZ renderer.
 *
 * A standalone Mupen64Plus video plugin expects VidExt_SetVideoMode() to make
 * its requested 640x480 mode the whole drawable window.  libretro deliberately
 * does not create a second window, so raw glViewport() calls from the plugin
 * can leak into RetroArch's compositor.  Save RetroArch's viewport before Rice
 * begins changing it; presentation restores it later without altering Rice's
 * 640x480 rendering itself. */
static int fz_frontend_viewport[4] = { 0, 0, 0, 0 };
static int fz_frontend_viewport_valid = 0;

/* Frontend scissor state for the external FZ renderer.
 * Standalone Rice enables GL_SCISSOR_TEST and leaves a 640x480-ish scissor
 * box active.  That state belongs to Rice's drawable, not RetroArch's
 * compositor, so save the frontend state before Rice starts touching GL. */
static int fz_frontend_scissor_box[4] = { 0, 0, 0, 0 };
static int fz_frontend_scissor_enabled = 0;
static int fz_frontend_scissor_valid = 0;

static void fz_bridge_capture_frontend_viewport(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   typedef unsigned char (*fz_is_enabled_t)(unsigned int);
   static fz_get_integerv_t get_integerv = NULL;
   static fz_is_enabled_t is_enabled = NULL;
   int viewport[4] = { 0, 0, 0, 0 };
   int scissor[4] = { 0, 0, 0, 0 };

   if (!get_integerv)
      get_integerv = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");
   if (!is_enabled)
      is_enabled = (fz_is_enabled_t)
         glsm_get_proc_address("glIsEnabled");

   if (!get_integerv)
      return;

   /* GL_VIEWPORT = 0x0BA2. */
   get_integerv(0x0BA2u, viewport);
   if (viewport[2] <= 0 || viewport[3] <= 0)
      return;

   fz_frontend_viewport[0] = viewport[0];
   fz_frontend_viewport[1] = viewport[1];
   fz_frontend_viewport[2] = viewport[2];
   fz_frontend_viewport[3] = viewport[3];
   fz_frontend_viewport_valid = 1;

   /* GL_SCISSOR_BOX = 0x0C10, GL_SCISSOR_TEST = 0x0C11. */
   get_integerv(0x0C10u, scissor);
   fz_frontend_scissor_box[0] = scissor[0];
   fz_frontend_scissor_box[1] = scissor[1];
   fz_frontend_scissor_box[2] = scissor[2];
   fz_frontend_scissor_box[3] = scissor[3];
   fz_frontend_scissor_enabled = is_enabled ? (is_enabled(0x0C11u) ? 1 : 0) : 0;
   fz_frontend_scissor_valid = 1;

#endif
}

static m64p_error fz_bridge_set_video_mode(int Width, int Height,
      int BitsPerPixel, m64p_video_mode ScreenMode, m64p_video_flags Flags)
{
   fz_bridge_capture_frontend_viewport();

   return VidExt_SetVideoMode(Width, Height, BitsPerPixel, ScreenMode, Flags);
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

/* Route the standalone plugin's swap request through the libretro
 * video-extension timing path. */
static m64p_error fz_bridge_gl_swap_buffers(void)
{
   return VidExt_GL_SwapBuffers();
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

/* RiceFZ TWO-WAY STATE TEST
 *
 * Save Rice's raw GL state immediately before frontend presentation.
 * The frontend-facing cleanup may then change GL freely.  On the next
 * external-plugin entry, restore Rice's state exactly once.
 */

#define FZ_RICE_ATTRIBS 5
#define FZ_RICE_TEXUNITS 8

struct fz_saved_attrib
{
   int enabled;
   int size;
   int type;
   int normalized;
   int stride;
   int buffer;
   void *pointer;
};

static int fz_saved_rice_valid = 0;
static int fz_restore_rice_pending = 0;

static int fz_saved_rice_viewport[4];
static int fz_saved_rice_scissor[4];
static int fz_saved_rice_scissor_enabled;

/* RiceFZ CULL DEPTH TWO-WAY TEST */
static int fz_saved_rice_cull_enabled;
static int fz_saved_rice_cull_face_mode;
static int fz_saved_rice_front_face;
static int fz_saved_rice_depth_test_enabled;
static int fz_saved_rice_depth_func;
static int fz_saved_rice_depth_mask;

static int fz_saved_rice_program;
static int fz_saved_rice_active_texture;
static int fz_saved_rice_array_buffer;
static int fz_saved_rice_element_buffer;
static int fz_saved_rice_texture[FZ_RICE_TEXUNITS];

static struct fz_saved_attrib
   fz_saved_rice_attrib[FZ_RICE_ATTRIBS];


static void fz_save_rice_gl_state(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)

   typedef void (*get_int_t)(unsigned int, int *);
   typedef unsigned char (*is_enabled_t)(unsigned int);
   typedef void (*active_tex_t)(unsigned int);
   typedef void (*get_attrib_t)(unsigned int, unsigned int, int *);
   typedef void (*get_attrib_ptr_t)(unsigned int, unsigned int, void **);
   typedef void (*disable_attrib_t)(unsigned int);

   static get_int_t get_int = NULL;
   static is_enabled_t is_enabled = NULL;
   static active_tex_t active_tex = NULL;
   static get_attrib_t get_attrib = NULL;
   static get_attrib_ptr_t get_attrib_ptr = NULL;
   static disable_attrib_t disable_attrib = NULL;

   int i;

   if (!get_int)
      get_int = (get_int_t)glsm_get_proc_address("glGetIntegerv");

   if (!is_enabled)
      is_enabled = (is_enabled_t)glsm_get_proc_address("glIsEnabled");

   if (!active_tex)
      active_tex = (active_tex_t)glsm_get_proc_address("glActiveTexture");

   if (!get_attrib)
      get_attrib =
         (get_attrib_t)glsm_get_proc_address("glGetVertexAttribiv");

   if (!get_attrib_ptr)
      get_attrib_ptr =
         (get_attrib_ptr_t)glsm_get_proc_address("glGetVertexAttribPointerv");

   if (!disable_attrib)
      disable_attrib =
         (disable_attrib_t)glsm_get_proc_address("glDisableVertexAttribArray");

   if (!get_int || !active_tex)
      return;

   /* viewport / scissor */
   get_int(0x0BA2u, fz_saved_rice_viewport); /* GL_VIEWPORT */
   get_int(0x0C10u, fz_saved_rice_scissor);  /* GL_SCISSOR_BOX */

   fz_saved_rice_scissor_enabled =
      is_enabled && is_enabled(0x0C11u);      /* GL_SCISSOR_TEST */

   /* Raw Rice raster/depth state.
    * GL_CULL_FACE=0x0B44
    * GL_CULL_FACE_MODE=0x0B45
    * GL_FRONT_FACE=0x0B46
    * GL_DEPTH_TEST=0x0B71
    * GL_DEPTH_WRITEMASK=0x0B72
    * GL_DEPTH_FUNC=0x0B74 */
   fz_saved_rice_cull_enabled =
      is_enabled && is_enabled(0x0B44u);

   fz_saved_rice_depth_test_enabled =
      is_enabled && is_enabled(0x0B71u);

   get_int(0x0B45u, &fz_saved_rice_cull_face_mode);
   get_int(0x0B46u, &fz_saved_rice_front_face);
   get_int(0x0B74u, &fz_saved_rice_depth_func);
   get_int(0x0B72u, &fz_saved_rice_depth_mask);

   /* program / buffers / active texture */
   get_int(0x8B8Du, &fz_saved_rice_program);
   get_int(0x84E0u, &fz_saved_rice_active_texture);
   get_int(0x8894u, &fz_saved_rice_array_buffer);
   get_int(0x8895u, &fz_saved_rice_element_buffer);

   /* Rice texture bindings */
   for (i = 0; i < FZ_RICE_TEXUNITS; i++)
   {
      active_tex(0x84C0u + i);
      get_int(0x8069u, &fz_saved_rice_texture[i]);
   }

   active_tex((unsigned int)fz_saved_rice_active_texture);

   /* Rice attributes 0..4 */
   if (get_attrib)
   {
      for (i = 0; i < FZ_RICE_ATTRIBS; i++)
      {
         get_attrib(i, 0x8622u,
                    &fz_saved_rice_attrib[i].enabled);

         get_attrib(i, 0x8623u,
                    &fz_saved_rice_attrib[i].size);

         get_attrib(i, 0x8625u,
                    &fz_saved_rice_attrib[i].type);

         get_attrib(i, 0x886Au,
                    &fz_saved_rice_attrib[i].normalized);

         get_attrib(i, 0x8624u,
                    &fz_saved_rice_attrib[i].stride);

         get_attrib(i, 0x889Fu,
                    &fz_saved_rice_attrib[i].buffer);

         fz_saved_rice_attrib[i].pointer = NULL;

         if (get_attrib_ptr)
            get_attrib_ptr(
               i,
               0x8645u, /* GL_VERTEX_ATTRIB_ARRAY_POINTER */
               &fz_saved_rice_attrib[i].pointer);
      }
   }

   fz_saved_rice_valid = 1;
   fz_restore_rice_pending = 1;

   /* Neutralise Rice's vertex arrays for RetroArch.
    * They will be restored before Rice runs again. */
   if (disable_attrib)
      for (i = 0; i < FZ_RICE_ATTRIBS; i++)
         disable_attrib(i);


#endif
}


static void fz_restore_rice_gl_state(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)

   typedef void (*viewport_t)(int,int,int,int);
   typedef void (*scissor_t)(int,int,int,int);
   typedef void (*cap_t)(unsigned int);
   typedef void (*active_tex_t)(unsigned int);
   typedef void (*bind_tex_t)(unsigned int,unsigned int);
   typedef void (*use_program_t)(unsigned int);
   typedef void (*bind_buffer_t)(unsigned int,unsigned int);
   typedef void (*enum_state_t)(unsigned int);
   typedef void (*depth_mask_t)(unsigned char);

   typedef void (*attrib_ptr_t)(
         unsigned int, int, unsigned int,
         unsigned char, int, const void *);

   typedef void (*attrib_enable_t)(unsigned int);

   static viewport_t viewport = NULL;
   static scissor_t scissor = NULL;
   static cap_t enable = NULL;
   static cap_t disable = NULL;
   static active_tex_t active_tex = NULL;
   static bind_tex_t bind_tex = NULL;
   static use_program_t use_program = NULL;
   static bind_buffer_t bind_buffer = NULL;
   static enum_state_t cull_face = NULL;
   static enum_state_t front_face = NULL;
   static enum_state_t depth_func = NULL;
   static depth_mask_t depth_mask = NULL;
   static attrib_ptr_t attrib_ptr = NULL;
   static attrib_enable_t enable_attrib = NULL;
   static attrib_enable_t disable_attrib = NULL;

   int i;

   if (!fz_saved_rice_valid || !fz_restore_rice_pending)
      return;

   if (!viewport)
      viewport = (viewport_t)glsm_get_proc_address("glViewport");

   if (!scissor)
      scissor = (scissor_t)glsm_get_proc_address("glScissor");

   if (!enable)
      enable = (cap_t)glsm_get_proc_address("glEnable");

   if (!disable)
      disable = (cap_t)glsm_get_proc_address("glDisable");

   if (!active_tex)
      active_tex = (active_tex_t)glsm_get_proc_address("glActiveTexture");

   if (!bind_tex)
      bind_tex = (bind_tex_t)glsm_get_proc_address("glBindTexture");

   if (!use_program)
      use_program = (use_program_t)glsm_get_proc_address("glUseProgram");

   if (!bind_buffer)
      bind_buffer = (bind_buffer_t)glsm_get_proc_address("glBindBuffer");

   if (!cull_face)
      cull_face = (enum_state_t)glsm_get_proc_address("glCullFace");

   if (!front_face)
      front_face = (enum_state_t)glsm_get_proc_address("glFrontFace");

   if (!depth_func)
      depth_func = (enum_state_t)glsm_get_proc_address("glDepthFunc");

   if (!depth_mask)
      depth_mask = (depth_mask_t)glsm_get_proc_address("glDepthMask");

   if (!attrib_ptr)
      attrib_ptr =
         (attrib_ptr_t)glsm_get_proc_address("glVertexAttribPointer");

   if (!enable_attrib)
      enable_attrib =
         (attrib_enable_t)glsm_get_proc_address("glEnableVertexAttribArray");

   if (!disable_attrib)
      disable_attrib =
         (attrib_enable_t)glsm_get_proc_address("glDisableVertexAttribArray");


   /* restore Rice viewport */
   if (viewport)
      viewport(
         fz_saved_rice_viewport[0],
         fz_saved_rice_viewport[1],
         fz_saved_rice_viewport[2],
         fz_saved_rice_viewport[3]);


   /* restore Rice scissor */
   if (scissor && enable && disable)
   {
      scissor(
         fz_saved_rice_scissor[0],
         fz_saved_rice_scissor[1],
         fz_saved_rice_scissor[2],
         fz_saved_rice_scissor[3]);

      if (fz_saved_rice_scissor_enabled)
         enable(0x0C11u);
      else
         disable(0x0C11u);
   }


   /* Restore Rice's raster/depth state.
    * This is persistent raw GL state which GLSM cannot track because
    * the external plugin bypasses the rgl wrappers. */
   if (front_face)
      front_face((unsigned int)fz_saved_rice_front_face);

   if (cull_face)
      cull_face((unsigned int)fz_saved_rice_cull_face_mode);

   if (enable && disable)
   {
      if (fz_saved_rice_cull_enabled)
         enable(0x0B44u);       /* GL_CULL_FACE */
      else
         disable(0x0B44u);

      if (fz_saved_rice_depth_test_enabled)
         enable(0x0B71u);       /* GL_DEPTH_TEST */
      else
         disable(0x0B71u);
   }

   if (depth_func)
      depth_func((unsigned int)fz_saved_rice_depth_func);

   if (depth_mask)
      depth_mask((unsigned char)(fz_saved_rice_depth_mask ? 1 : 0));



   /* restore Rice textures */
   if (active_tex && bind_tex)
   {
      for (i = 0; i < FZ_RICE_TEXUNITS; i++)
      {
         active_tex(0x84C0u + i);

         bind_tex(
            0x0DE1u, /* GL_TEXTURE_2D */
            (unsigned int)fz_saved_rice_texture[i]);
      }

      active_tex((unsigned int)fz_saved_rice_active_texture);
   }


   if (use_program)
      use_program((unsigned int)fz_saved_rice_program);


   /* Restore pointers + enables. */
   if (bind_buffer && attrib_ptr &&
       enable_attrib && disable_attrib)
   {
      for (i = 0; i < FZ_RICE_ATTRIBS; i++)
      {
         bind_buffer(
            0x8892u, /* GL_ARRAY_BUFFER */
            (unsigned int)fz_saved_rice_attrib[i].buffer);

         attrib_ptr(
            i,
            fz_saved_rice_attrib[i].size,
            (unsigned int)fz_saved_rice_attrib[i].type,
            (unsigned char)
               (fz_saved_rice_attrib[i].normalized ? 1 : 0),
            fz_saved_rice_attrib[i].stride,
            fz_saved_rice_attrib[i].pointer);

         if (fz_saved_rice_attrib[i].enabled)
            enable_attrib(i);
         else
            disable_attrib(i);
      }

      bind_buffer(
         0x8892u,
         (unsigned int)fz_saved_rice_array_buffer);

      bind_buffer(
         0x8893u, /* GL_ELEMENT_ARRAY_BUFFER */
         (unsigned int)fz_saved_rice_element_buffer);
   }


   fz_restore_rice_pending = 0;


#endif
}

void fz_plugin_bridge_restore_frontend_viewport(void)
{
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   fz_save_rice_gl_state();
   typedef void (*fz_viewport_t)(int, int, int, int);
   typedef void (*fz_scissor_t)(int, int, int, int);
   typedef void (*fz_cap_t)(unsigned int);
   typedef void (*fz_active_texture_t)(unsigned int);
   typedef void (*fz_bind_texture_t)(unsigned int, unsigned int);
   typedef void (*fz_use_program_t)(unsigned int);
   typedef void (*fz_bind_buffer_t)(unsigned int, unsigned int);
   static fz_viewport_t viewport_fn = NULL;
   static fz_scissor_t scissor_fn = NULL;
   static fz_cap_t enable_fn = NULL;
   static fz_cap_t disable_fn = NULL;
   static fz_active_texture_t active_texture_fn = NULL;
   static fz_bind_texture_t bind_texture_fn = NULL;
   static fz_use_program_t use_program_fn = NULL;
   static fz_bind_buffer_t bind_buffer_fn = NULL;

   if (!fz_frontend_viewport_valid)
      return;

   if (!viewport_fn)
      viewport_fn = (fz_viewport_t) glsm_get_proc_address("glViewport");
   if (!scissor_fn)
      scissor_fn = (fz_scissor_t) glsm_get_proc_address("glScissor");
   if (!enable_fn)
      enable_fn = (fz_cap_t) glsm_get_proc_address("glEnable");
   if (!disable_fn)
      disable_fn = (fz_cap_t) glsm_get_proc_address("glDisable");
   if (!active_texture_fn)
      active_texture_fn = (fz_active_texture_t)
         glsm_get_proc_address("glActiveTexture");
   if (!bind_texture_fn)
      bind_texture_fn = (fz_bind_texture_t)
         glsm_get_proc_address("glBindTexture");
   if (!use_program_fn)
      use_program_fn = (fz_use_program_t)
         glsm_get_proc_address("glUseProgram");
   if (!bind_buffer_fn)
      bind_buffer_fn = (fz_bind_buffer_t)
         glsm_get_proc_address("glBindBuffer");
   if (!viewport_fn)
      return;

   viewport_fn(fz_frontend_viewport[0], fz_frontend_viewport[1],
               fz_frontend_viewport[2], fz_frontend_viewport[3]);

   if (fz_frontend_scissor_valid && scissor_fn && enable_fn && disable_fn)
   {
      scissor_fn(fz_frontend_scissor_box[0], fz_frontend_scissor_box[1],
                 fz_frontend_scissor_box[2], fz_frontend_scissor_box[3]);
      if (fz_frontend_scissor_enabled)
         enable_fn(0x0C11u); /* GL_SCISSOR_TEST */
      else
         disable_fn(0x0C11u);
   }

   /* Frontend raw-GL cleanup.
    *
    * The external FZ plugin calls desktop OpenGL directly, bypassing GLSM's
    * state cache. GLSM's normal unbind therefore cannot know which Rice
    * textures/program/buffers are still live when RetroArch composites the
    * hardware frame. Neutralize only the same classes of state that GLSM's
    * own unbind normally resets. Rice has finished drawing at this point. */
   if (active_texture_fn && bind_texture_fn)
   {
      int unit;


      /* FZ Rice caps the renderer to at most 8 texture units. */
      for (unit = 0; unit < 8; unit++)
      {
         active_texture_fn(0x84C0u + (unsigned int) unit); /* GL_TEXTURE0+n */
         bind_texture_fn(0x0DE1u, 0);                     /* GL_TEXTURE_2D */
      }
      active_texture_fn(0x84C0u); /* leave frontend on texture unit 0 */
   }

   if (use_program_fn)
      use_program_fn(0);
   if (bind_buffer_fn)
   {
      bind_buffer_fn(0x8892u, 0); /* GL_ARRAY_BUFFER */
      bind_buffer_fn(0x8893u, 0); /* GL_ELEMENT_ARRAY_BUFFER */
   }


#endif
}

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
   bind_framebuffer(0x8D40u, framebuffer); /* GL_FRAMEBUFFER */

   /* Frontend presentation temporarily replaces raw state owned by the
    * standalone renderer. Restore it once before Rice resumes rendering. */
   fz_restore_rice_gl_state();
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
   if (strcmp(name, "VidExt_SetVideoMode") == 0)
      return (void *) fz_bridge_set_video_mode;
   FZ_PROC(VidExt_SetCaption);
   FZ_PROC(VidExt_ToggleFullScreen);
   FZ_PROC(VidExt_ResizeWindow);
   if (strcmp(name, "VidExt_GL_GetProcAddress") == 0)
      return (void *) fz_bridge_gl_get_proc_address;
   if (strcmp(name, "VidExt_GL_SetAttribute") == 0)
      return (void *) fz_bridge_gl_set_attribute;
   if (strcmp(name, "VidExt_GL_GetAttribute") == 0)
      return (void *) fz_bridge_gl_get_attribute;
   if (strcmp(name, "VidExt_GL_SwapBuffers") == 0)
      return (void *) fz_bridge_gl_swap_buffers;

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
