#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "mupen64plus-core/src/plugin/fz_plugin_bridge.c"

text = PATH.read_text(encoding="utf-8")

MARKER = "RiceFZ frontend raw-GL cleanup test"
if MARKER in text:
    print("RiceFZ frontend raw-GL cleanup test already installed.")
    raise SystemExit(0)

if "RiceFZ frontend scissor restore test" not in text:
    raise SystemExit(
        "ERROR: scissor restore experiment is not installed. Run "
        "tools/test-ricefz-restore-frontend-scissor.py first."
    )


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"ERROR: {label}: expected exactly one anchor, found {count}")
    text = text.replace(old, new, 1)

old_types = '''   typedef void (*fz_viewport_t)(int, int, int, int);
   typedef void (*fz_scissor_t)(int, int, int, int);
   typedef void (*fz_cap_t)(unsigned int);
   static fz_viewport_t viewport_fn = NULL;
   static fz_scissor_t scissor_fn = NULL;
   static fz_cap_t enable_fn = NULL;
   static fz_cap_t disable_fn = NULL;
   static unsigned int restore_count = 0;
'''

new_types = '''   typedef void (*fz_viewport_t)(int, int, int, int);
   typedef void (*fz_scissor_t)(int, int, int, int);
   typedef void (*fz_cap_t)(unsigned int);
   typedef void (*fz_active_texture_t)(unsigned int);
   typedef void (*fz_bind_texture_t)(unsigned int, unsigned int);
   typedef void (*fz_use_program_t)(unsigned int);
   typedef void (*fz_bind_buffer_t)(unsigned int, unsigned int);
   typedef void (*fz_get_integerv_t)(unsigned int, int *);
   static fz_viewport_t viewport_fn = NULL;
   static fz_scissor_t scissor_fn = NULL;
   static fz_cap_t enable_fn = NULL;
   static fz_cap_t disable_fn = NULL;
   static fz_active_texture_t active_texture_fn = NULL;
   static fz_bind_texture_t bind_texture_fn = NULL;
   static fz_use_program_t use_program_fn = NULL;
   static fz_bind_buffer_t bind_buffer_fn = NULL;
   static fz_get_integerv_t get_integerv_fn = NULL;
   static unsigned int restore_count = 0;
'''
replace_once(old_types, new_types, "restore helper raw GL types")

old_resolve = '''   if (!disable_fn)
      disable_fn = (fz_cap_t) glsm_get_proc_address("glDisable");
   if (!viewport_fn)
      return;
'''

new_resolve = '''   if (!disable_fn)
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
   if (!get_integerv_fn)
      get_integerv_fn = (fz_get_integerv_t)
         glsm_get_proc_address("glGetIntegerv");
   if (!viewport_fn)
      return;
'''
replace_once(old_resolve, new_resolve, "restore helper raw GL resolvers")

anchor = '''   if (fz_frontend_scissor_valid && scissor_fn && enable_fn && disable_fn)
   {
      scissor_fn(fz_frontend_scissor_box[0], fz_frontend_scissor_box[1],
                 fz_frontend_scissor_box[2], fz_frontend_scissor_box[3]);
      if (fz_frontend_scissor_enabled)
         enable_fn(0x0C11u); /* GL_SCISSOR_TEST */
      else
         disable_fn(0x0C11u);
   }
'''

cleanup = anchor + '''
   /* RiceFZ frontend raw-GL cleanup test.
    *
    * The external FZ plugin calls desktop OpenGL directly, bypassing GLSM's
    * state cache. GLSM's normal unbind therefore cannot know which Rice
    * textures/program/buffers are still live when RetroArch composites the
    * hardware frame. Neutralize only the same classes of state that GLSM's
    * own unbind normally resets. Rice has finished drawing at this point. */
   if (active_texture_fn && bind_texture_fn)
   {
      int unit;

      if (get_integerv_fn && restore_count < 8)
      {
         int program = -1;
         int active = -1;
         int tex0 = -1;
         int tex1 = -1;
         int array_buffer = -1;
         int element_buffer = -1;

         /* GL_CURRENT_PROGRAM=0x8B8D, GL_ACTIVE_TEXTURE=0x84E0,
          * GL_TEXTURE_BINDING_2D=0x8069, GL_ARRAY_BUFFER_BINDING=0x8894,
          * GL_ELEMENT_ARRAY_BUFFER_BINDING=0x8895. */
         get_integerv_fn(0x8B8Du, &program);
         get_integerv_fn(0x84E0u, &active);
         active_texture_fn(0x84C0u); /* GL_TEXTURE0 */
         get_integerv_fn(0x8069u, &tex0);
         active_texture_fn(0x84C1u); /* GL_TEXTURE1 */
         get_integerv_fn(0x8069u, &tex1);
         active_texture_fn((unsigned int) active);
         get_integerv_fn(0x8894u, &array_buffer);
         get_integerv_fn(0x8895u, &element_buffer);

         DebugMessage(M64MSG_INFO,
               "RiceFZ bridge raw state: before cleanup program=%d active=0x%x tex0=%d tex1=%d array=%d element=%d",
               program, active, tex0, tex1, array_buffer, element_buffer);
      }

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

   if (get_integerv_fn && restore_count < 8)
   {
      int program = -1;
      int active = -1;
      int tex0 = -1;
      int array_buffer = -1;
      int element_buffer = -1;

      get_integerv_fn(0x8B8Du, &program);
      get_integerv_fn(0x84E0u, &active);
      get_integerv_fn(0x8069u, &tex0);
      get_integerv_fn(0x8894u, &array_buffer);
      get_integerv_fn(0x8895u, &element_buffer);

      DebugMessage(M64MSG_INFO,
            "RiceFZ bridge raw state: after cleanup program=%d active=0x%x tex0=%d array=%d element=%d",
            program, active, tex0, array_buffer, element_buffer);
   }
'''
replace_once(anchor, cleanup, "frontend raw GL cleanup insertion")

PATH.write_text(text, encoding="utf-8")
print("OK: RiceFZ frontend raw-GL cleanup experiment installed.")
print("Rebuild/install only parallel_n64_libretro.so and test Mario again.")
print("Then grep 'RiceFZ bridge raw state' from the verbose log.")
