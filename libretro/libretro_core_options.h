/*
 * Mupen64Plus-ReARMed core options wrapper.
 *
 * Keeps the inherited option definitions untouched in
 * libretro_core_options_base.h, then presents plugin settings as one ordered
 * Plugin Configuration page:
 *
 *   shared plugin/RSP settings
 *   --- Rice ---
 *   --- Glide64 ---
 *   --- gles2n64 ---
 *   --- GLideN64 ---
 *   --- Angrylion ---
 *   --- ParaLLEl ---
 *
 * Core Options v2 has no nested categories, so renderer names are represented
 * by harmless single-value marker rows. Each renderer keeps the options it
 * inherited from ParaLLEl; only their presentation category/order changes.
 */

#ifndef M64P_REARMED_CORE_OPTIONS_WRAPPER_H
#define M64P_REARMED_CORE_OPTIONS_WRAPPER_H

#include "libretro_core_options_base.h"

#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_SECTION_VALUE       "section"
#define M64P_REARMED_SECTION_LABEL       "-"
#define M64P_REARMED_EXTRA_SECTIONS      6

static bool m64p_rearmed_options_prepared = false;
static struct retro_core_option_v2_definition *m64p_rearmed_option_defs = NULL;

static struct retro_core_option_v2_category m64p_rearmed_option_cats[] = {
   {
      M64P_REARMED_PLUGIN_CATEGORY_KEY,
      "Plugin Configuration",
      "Select graphics/RSP plugins and configure video plugins in ordered renderer sections."
   },
   {
      "input",
      "Pak/Controller Options",
      "Configure Core Pak/Controller options."
   },
   {
      "aleck64",
      "Aleck64",
      "Aleck64 arcade dipswitches (only used by Aleck64 MAME romsets)."
   },
   { NULL, NULL, NULL },
};

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

static bool m64p_rearmed_is_rice_option(
      const struct retro_core_option_v2_definition *option)
{
   if (!option || !option->key)
      return false;

   /* Rice currently exposes ReARMed-specific options by key rather than by an
    * inherited renderer category. This also catches future parallel-n64-rice-*
    * options without needing to update this wrapper. */
   return strstr(option->key, "-rice-") != NULL;
}

/* Options that select plugins or affect behaviour shared by several plugins. */
static bool m64p_rearmed_is_shared_plugin_option(
      const struct retro_core_option_v2_definition *option)
{
   const char *key;

   if (!option || !option->key)
      return false;

   key = option->key;

   return m64p_rearmed_string_ends_with(key, "-gfxplugin") ||
          m64p_rearmed_string_ends_with(key, "-rspplugin") ||
          m64p_rearmed_string_ends_with(key, "-gfxplugin-accuracy") ||
          m64p_rearmed_string_ends_with(key, "-screensize") ||
          m64p_rearmed_string_ends_with(key, "-aspectratiohint") ||
          m64p_rearmed_string_ends_with(key, "-send_allist_to_hle_rsp") ||
          m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio") ||
          m64p_rearmed_string_ends_with(key, "-enhanced-hle-audio-quality");
}

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
      option->desc_categorized = "Frameskip";
      option->info_categorized =
            "Native frameskip provided by the Rice video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
      return;
   }

   if (m64p_rearmed_string_ends_with(option->key, "-glide64-frameskip"))
   {
      option->desc_categorized = "Frameskip";
      option->info_categorized =
            "Native frameskip provided by the Glide64 video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
      return;
   }

   if (m64p_rearmed_string_ends_with(option->key, "-gles2n64-frameskip"))
   {
      option->desc_categorized = "Frameskip";
      option->info_categorized =
            "Native frameskip provided by the gles2n64 video plugin. Do not "
            "enable together with Core Frameskip; use only one frameskip "
            "layer at a time.";
   }
}

static size_t m64p_rearmed_count_base_options(void)
{
   size_t count = 0;

   while (option_defs_us[count].key)
      count++;

   return count;
}

static void m64p_rearmed_append_copy(size_t *write_index,
      const struct retro_core_option_v2_definition *source,
      const char *category)
{
   struct retro_core_option_v2_definition *dest;

   if (!write_index || !source || !m64p_rearmed_option_defs)
      return;

   dest = &m64p_rearmed_option_defs[*write_index];
   *dest = *source;

   if (category)
      dest->category_key = category;

   m64p_rearmed_customize_frameskip_option(dest);
   (*write_index)++;
}

static void m64p_rearmed_append_section_marker(size_t *write_index,
      const char *key, const char *title, const char *info)
{
   struct retro_core_option_v2_definition *dest;

   if (!write_index || !m64p_rearmed_option_defs)
      return;

   dest = &m64p_rearmed_option_defs[*write_index];
   memset(dest, 0, sizeof(*dest));

   dest->key              = key;
   dest->desc             = title;
   dest->desc_categorized = title;
   dest->info             = info;
   dest->info_categorized = info;
   dest->category_key     = M64P_REARMED_PLUGIN_CATEGORY_KEY;
   dest->values[0].value  = M64P_REARMED_SECTION_VALUE;
   dest->values[0].label  = M64P_REARMED_SECTION_LABEL;
   dest->default_value    = M64P_REARMED_SECTION_VALUE;

   (*write_index)++;
}

static size_t m64p_rearmed_count_renderer_options(const char *category,
      bool rice)
{
   size_t i = 0;
   size_t count = 0;

   while (option_defs_us[i].key)
   {
      if (rice)
      {
         if (m64p_rearmed_is_rice_option(&option_defs_us[i]))
            count++;
      }
      else if (option_defs_us[i].category_key && category &&
               strcmp(option_defs_us[i].category_key, category) == 0)
         count++;

      i++;
   }

   return count;
}

static void m64p_rearmed_append_renderer_section(size_t *write_index,
      const char *category, bool rice,
      const char *marker_key, const char *title, const char *info)
{
   size_t i = 0;

   if (m64p_rearmed_count_renderer_options(category, rice) == 0)
      return;

   m64p_rearmed_append_section_marker(write_index,
         marker_key, title, info);

   while (option_defs_us[i].key)
   {
      bool matches = false;

      if (rice)
         matches = m64p_rearmed_is_rice_option(&option_defs_us[i]);
      else if (option_defs_us[i].category_key && category)
         matches = strcmp(option_defs_us[i].category_key, category) == 0;

      if (matches)
         m64p_rearmed_append_copy(write_index, &option_defs_us[i],
               M64P_REARMED_PLUGIN_CATEGORY_KEY);

      i++;
   }
}

static bool m64p_rearmed_is_plugin_owned_option(
      const struct retro_core_option_v2_definition *option)
{
   if (!option)
      return false;

   return m64p_rearmed_is_shared_plugin_option(option) ||
          m64p_rearmed_is_rice_option(option) ||
          m64p_rearmed_is_renderer_category(option->category_key);
}

static void m64p_rearmed_prepare_plugin_options(void)
{
   size_t i;
   size_t write_index = 0;
   size_t base_count;

   if (m64p_rearmed_options_prepared)
      return;

   base_count = m64p_rearmed_count_base_options();

   m64p_rearmed_option_defs =
         (struct retro_core_option_v2_definition *)calloc(
               base_count + M64P_REARMED_EXTRA_SECTIONS + 1,
               sizeof(struct retro_core_option_v2_definition));

   if (!m64p_rearmed_option_defs)
   {
      m64p_rearmed_options_prepared = true;
      return;
   }

   /* Core Frameskip stays at the top level and remains easy to distinguish
    * from the native frameskip exposed inside renderer sections. */
   for (i = 0; i < base_count; i++)
   {
      if (strcmp(option_defs_us[i].key, CORE_NAME "-frameskip") == 0)
      {
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
         break;
      }
   }

   /* Shared plugin controls appear first inside Plugin Configuration. */
   for (i = 0; i < base_count; i++)
   {
      if (m64p_rearmed_is_shared_plugin_option(&option_defs_us[i]))
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i],
               M64P_REARMED_PLUGIN_CATEGORY_KEY);
   }

   /* Practical/legacy renderers first, followed by the heavier alternatives. */
   m64p_rearmed_append_renderer_section(&write_index,
         NULL, true,
         CORE_NAME "-section-rice",
         "--- Rice ---",
         "Rice video plugin options.");

   m64p_rearmed_append_renderer_section(&write_index,
         "glide64", false,
         CORE_NAME "-section-glide64",
         "--- Glide64 ---",
         "Glide64 video plugin options.");

   m64p_rearmed_append_renderer_section(&write_index,
         "gles2n64", false,
         CORE_NAME "-section-gles2n64",
         "--- gles2n64 ---",
         "gles2n64 video plugin options.");

   m64p_rearmed_append_renderer_section(&write_index,
         "gliden64", false,
         CORE_NAME "-section-gliden64",
         "--- GLideN64 ---",
         "GLideN64 video plugin options.");

   m64p_rearmed_append_renderer_section(&write_index,
         "angrylion", false,
         CORE_NAME "-section-angrylion",
         "--- Angrylion ---",
         "Angrylion software renderer options.");

   m64p_rearmed_append_renderer_section(&write_index,
         "parallel", false,
         CORE_NAME "-section-parallel",
         "--- ParaLLEl ---",
         "ParaLLEl renderer options.");

   /* Preserve Pak/Controller and Aleck64 as their own top-level categories. */
   for (i = 0; i < base_count; i++)
   {
      if (option_defs_us[i].category_key &&
          strcmp(option_defs_us[i].category_key, "input") == 0)
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
   }

   for (i = 0; i < base_count; i++)
   {
      if (option_defs_us[i].category_key &&
          strcmp(option_defs_us[i].category_key, "aleck64") == 0)
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
   }

   /* Append all remaining general/core options in their inherited order. */
   for (i = 0; i < base_count; i++)
   {
      const char *category = option_defs_us[i].category_key;

      if (strcmp(option_defs_us[i].key, CORE_NAME "-frameskip") == 0)
         continue;

      if (m64p_rearmed_is_plugin_owned_option(&option_defs_us[i]))
         continue;

      if (category &&
          (strcmp(category, "input") == 0 || strcmp(category, "aleck64") == 0))
         continue;

      m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
   }

   /* calloc() already supplies the terminating all-NULL definition. */
   options_us.categories  = m64p_rearmed_option_cats;
   options_us.definitions = m64p_rearmed_option_defs;

   m64p_rearmed_options_prepared = true;
}

/*
 * Wrap the stock options registration without changing libretro.c.
 * The inherited helper registers options_us, which we point at our ordered
 * v2 definitions before the frontend receives them.
 */
static INLINE void m64p_rearmed_libretro_set_core_options(
      retro_environment_t environ_cb, bool *categories_supported)
{
   m64p_rearmed_prepare_plugin_options();
   libretro_set_core_options(environ_cb, categories_supported);
}

#define libretro_set_core_options m64p_rearmed_libretro_set_core_options

#endif /* M64P_REARMED_CORE_OPTIONS_WRAPPER_H */
