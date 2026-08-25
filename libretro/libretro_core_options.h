/*
 * Mupen64Plus-ReARMed core options wrapper.
 *
 * Keeps the inherited option definitions untouched in
 * libretro_core_options_base.h, then reorganizes plugin-related settings
 * into one "Plugin Configuration" category. Renderer-specific options are
 * shown only for the currently selected GFX plugin.
 */

#ifndef M64P_REARMED_CORE_OPTIONS_WRAPPER_H
#define M64P_REARMED_CORE_OPTIONS_WRAPPER_H

#include "libretro_core_options_base.h"

#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_MAX_PLUGIN_OPTIONS  512

struct m64p_rearmed_plugin_option_meta
{
   const char *key;
   const char *plugin;
};

static struct m64p_rearmed_plugin_option_meta
      m64p_rearmed_plugin_options[M64P_REARMED_MAX_PLUGIN_OPTIONS];
static size_t m64p_rearmed_plugin_option_count = 0;
static retro_environment_t m64p_rearmed_options_environ_cb = NULL;
static bool m64p_rearmed_options_prepared = false;

static bool m64p_rearmed_string_ends_with(const char *str, const char *suffix)
{
   size_t str_len;
   size_t suffix_len;

   if (!str || !suffix)
      return false;

   str_len = strlen(str);
   suffix_len = strlen(suffix);

   if (suffix_len > str_len)
      return false;

   return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool m64p_rearmed_is_renderer_category(const char *category)
{
   if (!category)
      return false;

   return strcmp(category, "parallel") == 0 ||
          strcmp(category, "angrylion") == 0 ||
          strcmp(category, "gliden64") == 0 ||
          strcmp(category, "glide64") == 0 ||
          strcmp(category, "gles2n64") == 0;
}

/*
 * Returns true when an option belongs in Plugin Configuration.
 * plugin_out is empty for shared plugin settings and otherwise contains
 * the GFX plugin name used by the visibility callback.
 */
static bool m64p_rearmed_classify_plugin_option(
      const struct retro_core_option_v2_definition *option,
      const char **plugin_out)
{
   const char *category;
   const char *key;

   if (!option || !option->key || !plugin_out)
      return false;

   category = option->category_key;
   key = option->key;

   if (m64p_rearmed_is_renderer_category(category))
   {
      *plugin_out = category;
      return true;
   }

   /* Rice currently has only its fork-specific frameskip option exposed. */
   if (m64p_rearmed_string_ends_with(key, "-rice-frameskip"))
   {
      *plugin_out = "rice";
      return true;
   }

   /* Shared plugin/RSP controls stay visible for every renderer. */
   if (m64p_rearmed_string_ends_with(key, "-gfxplugin") ||
       m64p_rearmed_string_ends_with(key, "-rspplugin") ||
       m64p_rearmed_string_ends_with(key, "-gfxplugin-accuracy") ||
       m64p_rearmed_string_ends_with(key, "-screensize") ||
       m64p_rearmed_string_ends_with(key, "-aspectratiohint") ||
       m64p_rearmed_string_ends_with(key, "-send_allist_to_hle_rsp") ||
       m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio") ||
       m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio-quality"))
   {
      *plugin_out = "";
      return true;
   }

   return false;
}

static void m64p_rearmed_compact_plugin_categories(void)
{
   size_t read_index = 0;
   size_t write_index = 0;
   bool plugin_category_added = false;

   while (option_cats_us[read_index].key)
   {
      if (m64p_rearmed_is_renderer_category(option_cats_us[read_index].key))
      {
         if (!plugin_category_added)
         {
            option_cats_us[write_index].key  = M64P_REARMED_PLUGIN_CATEGORY_KEY;
            option_cats_us[write_index].desc = "Plugin Configuration";
            option_cats_us[write_index].info =
                  "Select graphics/RSP plugins and configure the active renderer.";
            write_index++;
            plugin_category_added = true;
         }
      }
      else
      {
         if (write_index != read_index)
            option_cats_us[write_index] = option_cats_us[read_index];
         write_index++;
      }

      read_index++;
   }

   option_cats_us[write_index].key  = NULL;
   option_cats_us[write_index].desc = NULL;
   option_cats_us[write_index].info = NULL;

   /* Clear any stale entries after the new terminator. */
   while (write_index < read_index)
   {
      write_index++;
      option_cats_us[write_index].key  = NULL;
      option_cats_us[write_index].desc = NULL;
      option_cats_us[write_index].info = NULL;
   }
}

static void m64p_rearmed_prepare_plugin_options(void)
{
   size_t i = 0;

   if (m64p_rearmed_options_prepared)
      return;

   m64p_rearmed_compact_plugin_categories();

   while (option_defs_us[i].key)
   {
      const char *plugin = NULL;

      if (m64p_rearmed_classify_plugin_option(&option_defs_us[i], &plugin))
      {
         option_defs_us[i].category_key = M64P_REARMED_PLUGIN_CATEGORY_KEY;

         if (m64p_rearmed_plugin_option_count < M64P_REARMED_MAX_PLUGIN_OPTIONS)
         {
            m64p_rearmed_plugin_options[m64p_rearmed_plugin_option_count].key =
                  option_defs_us[i].key;
            m64p_rearmed_plugin_options[m64p_rearmed_plugin_option_count].plugin =
                  plugin;
            m64p_rearmed_plugin_option_count++;
         }
      }

      i++;
   }

   m64p_rearmed_options_prepared = true;
}

static bool m64p_rearmed_update_plugin_option_visibility(void)
{
   struct retro_variable gfx_var;
   struct retro_core_option_display display;
   const char *selected_plugin = NULL;
   size_t i;

   if (!m64p_rearmed_options_environ_cb)
      return false;

   gfx_var.key = CORE_NAME "-gfxplugin";
   gfx_var.value = NULL;

   if (m64p_rearmed_options_environ_cb(
          RETRO_ENVIRONMENT_GET_VARIABLE, &gfx_var) && gfx_var.value)
   {
      selected_plugin = gfx_var.value;

      /* The legacy gles2n64 renderer is exposed as "gln64" in the selector. */
      if (strcmp(selected_plugin, "gln64") == 0)
         selected_plugin = "gles2n64";
   }

   for (i = 0; i < m64p_rearmed_plugin_option_count; i++)
   {
      const char *option_plugin = m64p_rearmed_plugin_options[i].plugin;

      display.key = m64p_rearmed_plugin_options[i].key;
      display.visible = !selected_plugin || !option_plugin ||
                        option_plugin[0] == '\0' ||
                        strcmp(option_plugin, selected_plugin) == 0;

      m64p_rearmed_options_environ_cb(
            RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &display);
   }

   return true;
}

/*
 * Wrap the stock options registration without changing libretro.c.
 * The macro below affects calls appearing after this header is included;
 * the call inside this function still resolves to the inherited helper.
 */
static INLINE void m64p_rearmed_libretro_set_core_options(
      retro_environment_t environ_cb, bool *categories_supported)
{
   struct retro_core_options_update_display_callback update_display_cb;

   m64p_rearmed_prepare_plugin_options();
   m64p_rearmed_options_environ_cb = environ_cb;

   libretro_set_core_options(environ_cb, categories_supported);

   if (!environ_cb)
      return;

   update_display_cb.callback = m64p_rearmed_update_plugin_option_visibility;

   if (environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK,
                  &update_display_cb))
      m64p_rearmed_update_plugin_option_visibility();
}

#define libretro_set_core_options m64p_rearmed_libretro_set_core_options

#endif /* M64P_REARMED_CORE_OPTIONS_WRAPPER_H */
