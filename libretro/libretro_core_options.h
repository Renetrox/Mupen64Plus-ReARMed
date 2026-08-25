/*
 * Mupen64Plus-ReARMed core options wrapper.
 *
 * Keeps the inherited option definitions untouched in
 * libretro_core_options_base.h, then gives the menu a clearer hierarchy:
 *
 *   - general/core options remain at the top level
 *   - Plugin Configuration contains plugin selectors and shared settings
 *   - each video renderer keeps its own category and its own options
 *   - Rice gets a renderer category of its own
 *   - Pak/Controller and Aleck64 remain separate
 *
 * This preserves the original renderer-specific grouping instead of folding
 * every plugin option into one large category.
 */

#ifndef M64P_REARMED_CORE_OPTIONS_WRAPPER_H
#define M64P_REARMED_CORE_OPTIONS_WRAPPER_H

#include "libretro_core_options_base.h"

#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_RICE_CATEGORY_KEY   "rice"

static bool m64p_rearmed_options_prepared = false;

/*
 * ReARMed category order. We use our own category table instead of rewriting
 * the inherited one, so the upstream option definitions remain untouched.
 */
static struct retro_core_option_v2_category m64p_rearmed_option_cats[] = {
   {
      M64P_REARMED_PLUGIN_CATEGORY_KEY,
      "Plugin Configuration",
      "Select graphics/RSP plugins and configure settings shared by the video plugins."
   },
#ifdef HAVE_PARALLEL
   {
      "parallel",
      "ParaLLEl",
      "Configure ParaLLEl options."
   },
#endif
#ifdef HAVE_THR_AL
   {
      "angrylion",
      "Angrylion",
      "Configure Angrylion options."
   },
#endif
   {
      "gliden64",
      "GLideN64",
      "Configure GLideN64 options."
   },
   {
      "glide64",
      "Glide64",
      "Configure Glide64 options."
   },
#ifdef HAVE_RICE
   {
      M64P_REARMED_RICE_CATEGORY_KEY,
      "Rice",
      "Configure Rice video plugin options."
   },
#endif
   {
      "gles2n64",
      "gles2n64",
      "Configure the legacy lightweight gles2n64 renderer."
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

/*
 * Keep the two frameskip layers obvious in the UI. Core Frameskip is a core
 * feature; renderer-native frameskip stays inside that renderer's category.
 * The two modes must not be enabled at the same time.
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

/* Options that configure plugin selection or behaviour shared across plugins. */
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

static void m64p_rearmed_prepare_plugin_options(void)
{
   size_t i = 0;

   if (m64p_rearmed_options_prepared)
      return;

   /* Point the inherited options object at the ReARMed category hierarchy. */
   options_us.categories = m64p_rearmed_option_cats;

   while (option_defs_us[i].key)
   {
      m64p_rearmed_customize_frameskip_option(&option_defs_us[i]);

      /* Shared selectors/settings belong in Plugin Configuration. */
      if (m64p_rearmed_is_shared_plugin_option(&option_defs_us[i]))
         option_defs_us[i].category_key = M64P_REARMED_PLUGIN_CATEGORY_KEY;

      /* Rice had no category upstream; give its native options a home. */
      if (m64p_rearmed_string_ends_with(option_defs_us[i].key,
                                        "-rice-frameskip"))
         option_defs_us[i].category_key = M64P_REARMED_RICE_CATEGORY_KEY;

      /* All inherited renderer categories are otherwise preserved as-is. */
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
