/*
 * Mupen64Plus-ReARMed core options wrapper.
 *
 * Keeps the inherited option definitions untouched in
 * libretro_core_options_base.h, then reorganizes plugin-related settings
 * into one static "Plugin Configuration" category.
 *
 * All plugin options remain visible together. This is deliberately simpler
 * and more predictable than hiding/showing options dynamically when the GFX
 * plugin changes, and it also works better with older libretro frontends.
 */

#ifndef M64P_REARMED_CORE_OPTIONS_WRAPPER_H
#define M64P_REARMED_CORE_OPTIONS_WRAPPER_H

#include "libretro_core_options_base.h"

#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"

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
 * Keep the two frameskip layers obvious in the UI. The core-level mode and
 * a renderer's native frameskip must not be enabled at the same time.
 * Actual automatic mutual exclusion can be added later, after the static
 * menu layout has been validated.
 */
static void m64p_rearmed_customize_frameskip_option(
      struct retro_core_option_v2_definition *option)
{
   if (!option || !option->key)
      return;

   if (strcmp(option->key, CORE_NAME "-frameskip") == 0)
   {
      option->desc = "Core Frameskip";
      option->info =
            "Renderer-independent automatic HLE graphics frameskip handled "
            "by the core. Auto skips graphics work only when the emulator "
            "falls behind while CPU/audio emulation continues. Requires the "
            "HLE RSP. Do not enable together with a plugin-specific "
            "frameskip mode; use only one frameskip layer at a time.";
      return;
   }

   if (m64p_rearmed_string_ends_with(option->key, "-rice-frameskip"))
   {
      option->desc_categorized = "Rice Frameskip";
      option->info_categorized =
            "Native frameskip provided by the Rice video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
      return;
   }

   if (m64p_rearmed_string_ends_with(option->key, "-glide64-frameskip"))
   {
      option->desc_categorized = "Glide64 Frameskip";
      option->info_categorized =
            "Native frameskip provided by the Glide64 video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
      return;
   }

   if (m64p_rearmed_string_ends_with(option->key, "-gles2n64-frameskip"))
   {
      option->desc_categorized = "gles2n64 Frameskip";
      option->info_categorized =
            "Native frameskip provided by the gles2n64 video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
   }
}

/* Returns true when an option belongs in Plugin Configuration. */
static bool m64p_rearmed_is_plugin_option(
      const struct retro_core_option_v2_definition *option)
{
   const char *category;
   const char *key;

   if (!option || !option->key)
      return false;

   category = option->category_key;
   key = option->key;

   /* Every renderer-specific category is folded into one common category. */
   if (m64p_rearmed_is_renderer_category(category))
      return true;

   /* Rice has no inherited renderer category yet, so include its option here. */
   if (m64p_rearmed_string_ends_with(key, "-rice-frameskip"))
      return true;

   /* Shared graphics/RSP controls also belong with plugin configuration. */
   return m64p_rearmed_string_ends_with(key, "-gfxplugin") ||
          m64p_rearmed_string_ends_with(key, "-rspplugin") ||
          m64p_rearmed_string_ends_with(key, "-gfxplugin-accuracy") ||
          m64p_rearmed_string_ends_with(key, "-screensize") ||
          m64p_rearmed_string_ends_with(key, "-aspectratiohint") ||
          m64p_rearmed_string_ends_with(key, "-send_allist_to_hle_rsp") ||
          m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio") ||
          m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio-quality");
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
                  "Select graphics/RSP plugins and configure all available video plugin options.";
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
      m64p_rearmed_customize_frameskip_option(&option_defs_us[i]);

      if (m64p_rearmed_is_plugin_option(&option_defs_us[i]))
         option_defs_us[i].category_key = M64P_REARMED_PLUGIN_CATEGORY_KEY;

      i++;
   }

   m64p_rearmed_options_prepared = true;
}

/*
 * Wrap the stock options registration without changing libretro.c.
 * The macro below affects calls appearing after this header is included;
 * the call inside this function still resolves to the inherited helper.
 */
static INLINE void m64p_rearmed_libretro_set_core_options(
      retro_environment_t environ_cb, bool *categories_supported)
{
   m64p_rearmed_prepare_plugin_options();
   libretro_set_core_options(environ_cb, categories_supported);
}

#define libretro_set_core_options m64p_rearmed_libretro_set_core_options

#endif /* M64P_REARMED_CORE_OPTIONS_WRAPPER_H */
