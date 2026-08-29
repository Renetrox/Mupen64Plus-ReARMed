/*
 * Mupen64Plus-ReARMed core options wrapper.
 *
 * Keeps the inherited option definitions untouched in
 * libretro_core_options_base.h, while presenting plugin categories together
 * in a predictable order. RetroArch places a category at the position of the
 * first option that belongs to it, so ReARMed builds an ordered copy of the
 * option definitions while preserving each renderer's real category key.
 *
 *   Core Frameskip
 *   Plugin Configuration
 *   Rice
 *   Glide64
 *   gles2n64
 *   GLideN64
 *   Angrylion
 *   ParaLLEl
 *   Pak/Controller Options
 *   Aleck64
 *   remaining general/core options
 *
 * Core Options v2 does not support nested categories. Each renderer therefore
 * remains a genuine RetroArch submenu; only the definition order is changed.
 */

#ifndef M64P_REARMED_CORE_OPTIONS_WRAPPER_H
#define M64P_REARMED_CORE_OPTIONS_WRAPPER_H

#include "libretro_core_options_base.h"

#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_SYSTEM_CATEGORY_KEY "system"
#define M64P_REARMED_RICE_CATEGORY_KEY   "rice"

static bool m64p_rearmed_options_prepared = false;
static struct retro_core_option_v2_definition *m64p_rearmed_option_defs = NULL;

/* glN64 in Mupen64Plus FZ exposes Texture 2xSAI, but the inherited libretro
 * option table never did. Keep this fork-specific definition in the wrapper
 * so the upstream-derived base table remains untouched. */
static const struct retro_core_option_v2_definition m64p_rearmed_gln64_2xsai_option =
{
   CORE_NAME "-gln64-2xsai",
   "gles2n64: Texture 2xSAI",
   "Texture 2xSAI",
   "Apply the legacy Mupen64Plus FZ 2xSAI scaler to glN64 textures before upload. This doubles texture width and height and increases texture-cache memory use. Restart content after changing.",
   NULL,
   "gles2n64",
   {
      { "disabled", "Disabled" },
      { "enabled",  "Enabled" },
      { NULL, NULL },
   },
   "disabled"
};

static struct retro_core_option_v2_category m64p_rearmed_option_cats[] = {
   {
      M64P_REARMED_PLUGIN_CATEGORY_KEY,
      "Plugin Configuration",
      "Select graphics/RSP plugins and configure settings shared by the video plugins."
   },
   {
      M64P_REARMED_SYSTEM_CATEGORY_KEY,
      "System Timing",
      "Advanced core timing controls. Auto preserves the game-specific defaults."
   },
   {
      M64P_REARMED_RICE_CATEGORY_KEY,
      "Rice",
      "Configure Rice video plugin options."
   },
   {
      "glide64",
      "Glide64",
      "Configure Glide64 options."
   },
   {
      "gles2n64",
      "gles2n64",
      "Configure the legacy lightweight gles2n64 renderer."
   },
   {
      "gliden64",
      "GLideN64",
      "Configure GLideN64 options."
   },
   {
      "angrylion",
      "Angrylion",
      "Configure Angrylion options."
   },
   {
      "parallel",
      "ParaLLEl",
      "Configure ParaLLEl options."
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

   return strcmp(category, "glide64") == 0 ||
          strcmp(category, "gles2n64") == 0 ||
          strcmp(category, "gliden64") == 0 ||
          strcmp(category, "angrylion") == 0 ||
          strcmp(category, "parallel") == 0;
}

static bool m64p_rearmed_is_rice_option(
      const struct retro_core_option_v2_definition *option)
{
   return option && option->key && strstr(option->key, "-rice-") != NULL;
}

/* Options that select plugins or configure behaviour shared by plugins. */
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

/*
 * Core Frameskip remains a general core option. Renderer-native frameskip is
 * simply called Frameskip inside the renderer's own submenu.
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

static void m64p_rearmed_append_category_options(size_t *write_index,
      const char *category)
{
   size_t i = 0;

   if (!category)
      return;

   while (option_defs_us[i].key)
   {
      if (option_defs_us[i].category_key &&
          strcmp(option_defs_us[i].category_key, category) == 0)
         m64p_rearmed_append_copy(write_index, &option_defs_us[i], category);

      i++;
   }
}

static bool m64p_rearmed_is_reordered_option(
      const struct retro_core_option_v2_definition *option)
{
   const char *category;

   if (!option || !option->key)
      return false;

   if (strcmp(option->key, CORE_NAME "-frameskip") == 0)
      return true;

   if (m64p_rearmed_is_shared_plugin_option(option) ||
       m64p_rearmed_is_rice_option(option))
      return true;

   category = option->category_key;

   return m64p_rearmed_is_renderer_category(category) ||
          (category && strcmp(category, M64P_REARMED_SYSTEM_CATEGORY_KEY) == 0) ||
          (category && strcmp(category, "input") == 0) ||
          (category && strcmp(category, "aleck64") == 0);
}

static void m64p_rearmed_prepare_plugin_options(void)
{
   size_t i;
   size_t write_index = 0;
   size_t base_count;

   if (m64p_rearmed_options_prepared)
      return;

   base_count = m64p_rearmed_count_base_options();

   /* One extra slot for the synthetic FZ 2xSAI option, plus the terminator. */
   m64p_rearmed_option_defs =
         (struct retro_core_option_v2_definition *)calloc(
               base_count + 2, sizeof(struct retro_core_option_v2_definition));

   if (!m64p_rearmed_option_defs)
   {
      m64p_rearmed_options_prepared = true;
      return;
   }

   /* Core-level frameskip stays first and outside renderer categories. */
   for (i = 0; i < base_count; i++)
   {
      if (strcmp(option_defs_us[i].key, CORE_NAME "-frameskip") == 0)
      {
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
         break;
      }
   }

   /* Shared plugin controls form the first real plugin submenu. */
   for (i = 0; i < base_count; i++)
   {
      if (m64p_rearmed_is_shared_plugin_option(&option_defs_us[i]))
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i],
               M64P_REARMED_PLUGIN_CATEGORY_KEY);
   }

   /* Advanced timing controls are core-wide, not renderer-specific. */
   m64p_rearmed_append_category_options(&write_index, M64P_REARMED_SYSTEM_CATEGORY_KEY);

   /* Rice has no inherited category, so collect its options explicitly. */
   for (i = 0; i < base_count; i++)
   {
      if (m64p_rearmed_is_rice_option(&option_defs_us[i]))
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i],
               M64P_REARMED_RICE_CATEGORY_KEY);
   }

   /* Real renderer categories, kept consecutive and in ReARMed's chosen order. */
   m64p_rearmed_append_category_options(&write_index, "glide64");
   m64p_rearmed_append_category_options(&write_index, "gles2n64");
   m64p_rearmed_append_copy(&write_index, &m64p_rearmed_gln64_2xsai_option,
         "gles2n64");
   m64p_rearmed_append_category_options(&write_index, "gliden64");
   m64p_rearmed_append_category_options(&write_index, "angrylion");
   m64p_rearmed_append_category_options(&write_index, "parallel");

   /* Other real categories follow the renderer block. */
   m64p_rearmed_append_category_options(&write_index, "input");
   m64p_rearmed_append_category_options(&write_index, "aleck64");

   /* Append all remaining general/core options in their inherited order. */
   for (i = 0; i < base_count; i++)
   {
      if (!m64p_rearmed_is_reordered_option(&option_defs_us[i]))
         m64p_rearmed_append_copy(&write_index, &option_defs_us[i], NULL);
   }

   /* calloc() supplies the terminating all-NULL definition. */
   options_us.categories  = m64p_rearmed_option_cats;
   options_us.definitions = m64p_rearmed_option_defs;

   m64p_rearmed_options_prepared = true;
}

/*
 * Wrap the stock options registration without changing libretro.c.
 */
static INLINE void m64p_rearmed_libretro_set_core_options(
      retro_environment_t environ_cb, bool *categories_supported)
{
   m64p_rearmed_prepare_plugin_options();
   libretro_set_core_options(environ_cb, categories_supported);
}

#define libretro_set_core_options m64p_rearmed_libretro_set_core_options

#endif /* M64P_REARMED_CORE_OPTIONS_WRAPPER_H */
