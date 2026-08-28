#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE_OPTS = ROOT / "libretro/libretro_core_options_base.h"
WRAPPER = ROOT / "libretro/libretro_core_options.h"
MAIN = ROOT / "mupen64plus-core/src/main/main.c"
GLIDE_INI = ROOT / "glide2gl/src/Glide64/Glide64_Ini.c"
MARKER = "ReARMed final Glide64 + system timing"

for path in (BASE_OPTS, WRAPPER, MAIN, GLIDE_INI):
    if not path.exists():
        raise SystemExit(f"Missing expected file: {path}")

base = BASE_OPTS.read_text(encoding="utf-8")
wrapper = WRAPPER.read_text(encoding="utf-8")
main = MAIN.read_text(encoding="utf-8")
glide = GLIDE_INI.read_text(encoding="utf-8-sig")

if MARKER in base or MARKER in wrapper or MARKER in main or MARKER in glide:
    print("Final Glide64/system-timing patch is already applied.")
    raise SystemExit(0)


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"Anchor not found for {label}; source layout changed.")
    return text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# 1) System Timing: Count Per Op + SI DMA Duration
# ---------------------------------------------------------------------------
timing_anchor = '''    {
        CORE_NAME "-virefresh",
        "VI Refresh (Overclock)",'''

timing_block = f'''    /* {MARKER}: advanced core timing controls. */
    {{
        CORE_NAME "-count-per-op",
        "Count Per Op",
        "Count Per Op",
        "Define cuántos ciclos se contabilizan por operación emulada de la CPU. Cambia el timing general del sistema y puede afectar velocidad, sincronización y compatibilidad de juegos sensibles. Valores incorrectos pueden causar aceleración, ralentización, bloqueos o errores de lógica. Auto conserva el valor específico de la base de compatibilidad del core.",
        NULL,
        "system",
        {{
            {{ "auto", "Auto" }},
            {{ "1", "1" }},
            {{ "2", "2" }},
            {{ "3", "3" }},
            {{ NULL, NULL }},
        }},
        "auto"
    }},
    {{
        CORE_NAME "-si-dma-duration",
        "SI DMA Duration",
        "SI DMA Duration",
        "Define la cantidad de ciclos que tarda una transferencia DMA del Serial Interface (SI). Este temporizado afecta al PIF, mandos, Controller Pak y EEPROM. Valores incorrectos pueden provocar problemas de entrada, guardado o bloqueos en juegos sensibles al timing. Auto utiliza el valor definido por la base de compatibilidad del core; 0x900 es el valor general y 0x64 es el valor histórico usado por Tetris 64.",
        NULL,
        "system",
        {{
            {{ "auto", "Auto" }},
            {{ "0", "0 (Immediate)" }},
            {{ "0x64", "0x64 (Tetris 64)" }},
            {{ "0x100", "0x100" }},
            {{ "0x900", "0x900 (Default)" }},
            {{ NULL, NULL }},
        }},
        "auto"
    }},
'''
base = replace_once(base, timing_anchor, timing_block + timing_anchor, "timing core options")

main_anchor = '''    if (count_per_op <= 0)
        count_per_op = ROM_SETTINGS.countperop;

    if (count_per_op_denom_pot > 20)
        count_per_op_denom_pot = 20;

    si_dma_duration = ROM_SETTINGS.sidmaduration;
'''

main_replacement = f'''    if (count_per_op <= 0)
        count_per_op = ROM_SETTINGS.countperop;

    /* {MARKER}: explicit core-option values override the ROM database only
     * for this content start. Auto leaves the database/default untouched. */
    {{
        struct retro_variable timing_var = {{ "parallel-n64-count-per-op", NULL }};
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &timing_var) &&
            timing_var.value && strcmp(timing_var.value, "auto") != 0)
        {{
            long value = strtol(timing_var.value, NULL, 0);
            if (value >= 1 && value <= 3)
                count_per_op = (uint32_t)value;
        }}
    }}

    if (count_per_op_denom_pot > 20)
        count_per_op_denom_pot = 20;

    si_dma_duration = ROM_SETTINGS.sidmaduration;
    {{
        struct retro_variable timing_var = {{ "parallel-n64-si-dma-duration", NULL }};
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &timing_var) &&
            timing_var.value && strcmp(timing_var.value, "auto") != 0)
        {{
            long value = strtol(timing_var.value, NULL, 0);
            if (value >= 0 && value <= 0x10000)
                si_dma_duration = (int32_t)value;
        }}
    }}
'''
main = replace_once(main, main_anchor, main_replacement, "main_run timing overrides")

# Add a real System Timing submenu to the plugin-configuration wrapper.
macro_anchor = '''#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_RICE_CATEGORY_KEY   "rice"
'''
macro_repl = '''#define M64P_REARMED_PLUGIN_CATEGORY_KEY "plugin"
#define M64P_REARMED_SYSTEM_CATEGORY_KEY "system"
#define M64P_REARMED_RICE_CATEGORY_KEY   "rice"
'''
wrapper = replace_once(wrapper, macro_anchor, macro_repl, "system category key")

category_anchor = '''   {
      M64P_REARMED_RICE_CATEGORY_KEY,
      "Rice",
      "Configure Rice video plugin options."
   },
'''
category_repl = '''   {
      M64P_REARMED_SYSTEM_CATEGORY_KEY,
      "System Timing",
      "Ajustes avanzados de timing del núcleo. Auto conserva los valores específicos de cada juego."
   },
''' + category_anchor
wrapper = replace_once(wrapper, category_anchor, category_repl, "system category definition")

append_anchor = '''   /* Rice has no inherited category, so collect its options explicitly. */
'''
append_repl = '''   /* Advanced timing controls are core-wide, not renderer-specific. */
   m64p_rearmed_append_category_options(&write_index, M64P_REARMED_SYSTEM_CATEGORY_KEY);

''' + append_anchor
wrapper = replace_once(wrapper, append_anchor, append_repl, "system category ordering")

reordered_anchor = '''   return m64p_rearmed_is_renderer_category(category) ||
          (category && strcmp(category, "input") == 0) ||
          (category && strcmp(category, "aleck64") == 0);
'''
reordered_repl = '''   return m64p_rearmed_is_renderer_category(category) ||
          (category && strcmp(category, M64P_REARMED_SYSTEM_CATEGORY_KEY) == 0) ||
          (category && strcmp(category, "input") == 0) ||
          (category && strcmp(category, "aleck64") == 0);
'''
wrapper = replace_once(wrapper, reordered_anchor, reordered_repl, "system category deduplication")

# ---------------------------------------------------------------------------
# 2) Glide64: permanently restore the 28 live FZ-style controls.
#    -1 = preserve built-in per-game profile; explicit values override it.
# ---------------------------------------------------------------------------

def vals(*pairs):
    return list(pairs)

options = [
    ("fog", "Fog", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Controla la emulación de niebla del N64. Desactivarla puede reducir ligeramente la carga gráfica, pero elimina efectos de profundidad y atmósfera y puede dejar escenas incorrectas. Game default conserva el perfil específico del juego."),
    ("buff-clear", "Buffer Clear", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Controla el borrado de los buffers usados por Glide64 entre renderizados. Activarlo puede corregir restos de cuadros, estelas o contenido antiguo, con un pequeño costo de rendimiento; desactivarlo puede ser necesario en juegos cuyo perfil evita el borrado. Game default conserva ese perfil."),
    ("swapmode", "Buffer Swap Mode", vals(("-1", "Game default"), ("0", "VI occurred"), ("1", "Conditional"), ("2", "Mix")),
     "Define cuándo Glide64 intercambia el buffer presentado. Cambiar el momento del swap puede corregir parpadeos o cuadros faltantes, pero un modo inadecuado puede producir imagen inestable, presentación duplicada o errores de sincronización. Game default usa el modo elegido para cada juego."),
    ("lodmode", "LOD Mode", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Fast"), ("2", "Precise")),
     "Controla el cálculo de Level of Detail (LOD) de texturas. Disabled evita el cálculo, Fast usa una aproximación menos costosa y Precise prioriza exactitud. Un valor incorrecto puede seleccionar texturas o mipmaps equivocados; los modos más precisos pueden aumentar la carga gráfica."),
    ("fb-smart", "Smart Framebuffer", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Activa la emulación inteligente del framebuffer de Glide64. Es necesaria para juegos que renderizan efectos, menús o imágenes intermedias en memoria, pero aumenta el trabajo de copia y detección. Desactivarla puede mejorar rendimiento y también romper esos efectos. Game default conserva la decisión por juego."),
    ("fb-render", "Framebuffer Render", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Controla el renderizado por software del depth buffer dentro de la emulación de framebuffer. Puede ser necesario para efectos que dependen de información de profundidad almacenada en RDRAM, pero es una ruta costosa. Desactivarlo mejora rendimiento cuando el juego no lo necesita y puede causar errores de profundidad si se fuerza incorrectamente."),
    ("fb-crc-mode", "Framebuffer CRC Mode", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Fast"), ("2", "Safe")),
     "Define cómo Glide64 usa CRC para detectar cambios en framebuffers. Fast reduce comprobaciones, Safe prioriza una detección más fiable y Disabled evita el control. Modos más estrictos pueden costar rendimiento; modos más ligeros pueden no detectar una actualización y mostrar contenido antiguo."),
    ("read-back-to-screen", "Read Back to Screen", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Mode 1"), ("2", "Mode 2")),
     "Controla las rutas legacy que vuelven a presentar en pantalla una imagen almacenada en el framebuffer del N64. Algunos juegos dibujan o procesan imágenes en RDRAM y necesitan esta lectura. Activarla incrementa copias de memoria; un modo incorrecto puede causar pantallas faltantes, imágenes duplicadas o pérdida de rendimiento."),
    ("detect-cpu-write", "Detect CPU Write", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Detecta escrituras realizadas por la CPU directamente sobre el framebuffer para que Glide64 pueda mostrar imágenes que no fueron dibujadas por el RDP. Es necesaria en ciertos juegos y añade trabajo de seguimiento/copia; desactivarla puede ocultar vídeos, menús o efectos escritos por CPU."),
    ("alt-tex-size", "Alternate Texture Size", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Usa el cálculo alternativo legacy del tamaño de textura. Es un hack de compatibilidad para juegos con tamaños o cargas de textura que el cálculo normal interpreta mal. Forzarlo sin necesidad puede producir texturas deformadas, desplazadas o con tamaño incorrecto."),
    ("force-microcheck", "Force Microcode Check", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Fuerza a Glide64 a volver a comprobar el microcódigo gráfico durante la ejecución. Está pensado para juegos que mezclan microcódigos, como F3DEX y S2DEX. Añade una pequeña sobrecarga, pero desactivarlo en un juego que cambia de microcódigo puede provocar geometría o sprites incorrectos."),
    ("force-quad3d", "Force Quad3D", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Fuerza a interpretar el comando gráfico 0xB5 como Quad3D en lugar de Line3D. Es un hack específico para microcódigos/juegos que usan esa codificación. Activarlo en un título que no lo necesita puede convertir líneas o polígonos en geometría incorrecta."),
    ("optimize-texrect", "Optimize Texrect", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Activa la ruta rápida para rectángulos texturizados cuando interviene la emulación de framebuffer. Puede reducir carga en elementos 2D, pero algunos juegos necesitan la ruta completa para efectos que leen o reutilizan el framebuffer. Desactivarlo puede mejorar compatibilidad a costa de rendimiento."),
    ("fb-read-alpha", "Framebuffer Read Alpha", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Incluye el canal alfa al leer el framebuffer. Es necesario para ciertos efectos de transparencia y composición, pero aumenta el trabajo de lectura/copia. Desactivarlo puede mejorar rendimiento y también producir transparencias, máscaras o capas incorrectas."),
    ("force-calc-sphere", "Force Calc Sphere", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Fuerza el cálculo de mapeado esférico de texturas. Es un workaround legacy destinado principalmente a juegos que dependen de ese cálculo, como Ridge Racer 64. Forzarlo en otros títulos puede alterar reflejos o coordenadas de textura."),
    ("increase-texrect-edge", "Increase Texrect Edge", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Añade un píxel/coordenada al borde inferior derecho de los texture rectangles. Corrige líneas, huecos o bordes faltantes en juegos concretos; aplicado donde no corresponde puede crear solapamientos, bordes extra o pequeños errores de textura."),
    ("decrease-fillrect-edge", "Decrease Fillrect Edge", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Resta una unidad al borde inferior derecho de los fill rectangles. Corrige sobreextensión y artefactos de relleno en juegos específicos, pero puede dejar huecos o líneas sin cubrir si se fuerza en títulos que usan las coordenadas normales."),
    ("stipple-mode", "Stipple Mode", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Pattern"), ("2", "Rotate")),
     "Selecciona cómo Glide64 emula transparencias basadas en stipple/dithering de alfa. Pattern usa el patrón configurado y Rotate varía el patrón para aproximar el efecto temporal. Un modo incorrecto puede cambiar transparencias, sombras o producir tramado visible."),
    ("clip-zmin", "Clip Zmin", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Activa el recorte contra el plano Z cercano. Puede corregir polígonos que atraviesan la cámara o geometría inválida en juegos concretos. Forzarlo sin necesidad puede recortar objetos demasiado pronto o hacer desaparecer partes de la escena."),
    ("adjust-aspect", "Adjust Aspect", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Permite que Glide64 ajuste la relación de aspecto usando las escalas VI del juego, especialmente en modos panorámicos o títulos con escalado inusual. Desactivarlo puede evitar correcciones no deseadas; forzarlo puede estirar o comprimir la imagen en juegos que esperan su perfil propio."),
    ("correct-viewport", "Correct Viewport", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Aplica la corrección legacy de valores de viewport usada por determinados juegos. Puede arreglar geometría desplazada, recortada o escalada incorrectamente. Activarla en títulos que no la necesitan puede mover o deformar la escena."),
    ("zmode-compare-less", "Zmode Compare Less", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Fuerza la comparación de profundidad LESS para los Z modes 0 y 1. Es un workaround para errores de oclusión y orden de polígonos. Un ajuste incorrecto puede hacer que objetos aparezcan delante o detrás de donde corresponde o que desaparezcan superficies."),
    ("old-style-adither", "Old Style Alpha Dither", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Aplica el método antiguo de alpha dithering incluso cuando el modo normal de dither no lo pediría. Es necesario para la apariencia/compatibilidad de algunos juegos, incluidos ciertos Castlevania. Puede introducir tramado o ruido visible cuando se fuerza innecesariamente."),
    ("n64-z-scale", "N64 Z Scale", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Escala el valor Z de los vértices antes de escribirlo en el depth buffer siguiendo el comportamiento del N64. Puede corregir precisión y conflictos de profundidad en juegos sensibles. Forzarlo donde no corresponde puede generar z-fighting o cambios en la oclusión y activa la tabla Z necesaria para esa ruta."),
    ("pal230", "PAL230", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Usa la escala vertical especial de 230 líneas empleada por algunos juegos PAL. Corrige altura y encuadre cuando el título depende de ese comportamiento; activarla en otros juegos PAL puede comprimir, estirar o desplazar verticalmente la imagen."),
    ("ignore-aux-copy", "Ignore Auxiliary Copy", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Evita copiar framebuffers auxiliares de menor tamaño durante la detección de framebuffer. Puede ahorrar trabajo y solucionar casos donde esas copias no deben tratarse como imágenes reutilizables, pero puede romper efectos que realmente dependen de un buffer auxiliar."),
    ("useless-is-useless", "Useless Is Useless", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Hace que un framebuffer que Glide64 detectó como no utilizado permanezca descartado en lugar de promoverlo a buffer auxiliar. Puede evitar procesamiento innecesario y resolver perfiles concretos; si la detección se equivoca, forzarlo puede hacer desaparecer un efecto que sí necesitaba ese buffer."),
    ("fb-read-always", "Framebuffer Read Always", vals(("-1", "Game default"), ("0", "Disabled"), ("1", "Enabled")),
     "Fuerza la lectura del framebuffer en cada cuadro en lugar de depender de la detección inteligente. Es una ruta de compatibilidad para juegos que reutilizan continuamente imágenes en RDRAM, pero puede ser costosa en hardware modesto. Desactivarla mejora rendimiento cuando no es necesaria y puede perder actualizaciones si el juego depende de ella."),
]

override_targets = [
    ("fog", "settings.fog"),
    ("buff-clear", "settings.buff_clear"),
    ("swapmode", "settings.swapmode"),
    ("lodmode", "settings.lodmode"),
    ("fb-smart", "smart_read"),
    ("fb-render", "depth_render"),
    ("fb-crc-mode", "fb_crc_mode"),
    ("read-back-to-screen", "read_back_to_screen"),
    ("detect-cpu-write", "cpu_write_hack"),
    ("alt-tex-size", "settings.alt_tex_size"),
    ("force-microcheck", "settings.force_microcheck"),
    ("force-quad3d", "settings.force_quad3d"),
    ("optimize-texrect", "optimize_texrect"),
    ("fb-read-alpha", "read_alpha"),
    ("force-calc-sphere", "settings.force_calc_sphere"),
    ("increase-texrect-edge", "settings.increase_texrect_edge"),
    ("decrease-fillrect-edge", "settings.decrease_fillrect_edge"),
    ("stipple-mode", "settings.stipple_mode"),
    ("clip-zmin", "settings.clip_zmin"),
    ("adjust-aspect", "settings.adjust_aspect"),
    ("correct-viewport", "settings.correct_viewport"),
    ("zmode-compare-less", "settings.zmode_compare_less"),
    ("old-style-adither", "settings.old_style_adither"),
    ("n64-z-scale", "settings.n64_z_scale"),
    ("pal230", "settings.pal230"),
    ("ignore-aux-copy", "ignore_aux_copy"),
    ("useless-is-useless", "useless_is_useless"),
    ("fb-read-always", "read_always"),
]


def option_block(suffix, title, values, info):
    value_lines = "\n".join(
        f'            {{ "{value}", "{label}" }},' for value, label in values
    )
    return f'''    {{
        CORE_NAME "-glide64-{suffix}",
        "Glide64: {title}",
        "{title}",
        "{info} Reinicie el contenido después de cambiar esta opción.",
        NULL,
        "glide64",
        {{
{value_lines}
            {{ NULL, NULL }},
        }},
        "-1"
    }},
'''

blocks = "\n".join(option_block(*option) for option in options)
glide_opts_anchor = '''#endif
    {
        CORE_NAME "-gfxplugin-accuracy",'''
base = replace_once(
    base,
    glide_opts_anchor,
    f'''    /* {MARKER}: live Glide64 controls restored from the FZ/mk2 configuration model. */
{blocks}#endif
    {{
        CORE_NAME "-gfxplugin-accuracy",''',
    "Glide64 option definitions",
)

helper_anchor = 'extern void glide_set_filtering(unsigned value);\n'
helper = f'''\n/* {MARKER}. Returns true when RetroArch supplied a numeric value. */\nstatic bool glide64_get_int_option(const char *key, int *value)\n{{\n   struct retro_variable var = {{ key, NULL }};\n   int parsed;\n\n   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) &&\n       var.value && sscanf(var.value, "%d", &parsed) == 1)\n   {{\n      *value = parsed;\n      return true;\n   }}\n\n   return false;\n}}\n'''
glide = replace_once(glide, helper_anchor, helper_anchor + helper, "Glide64 option helper")

override_anchor = '''   if (settings.n64_z_scale)
      ZLUT_init();

   //frame buffer
'''
override_lines = []
for suffix, target in override_targets:
    override_lines.append(
        f'      if (glide64_get_int_option("parallel-n64-glide64-{suffix}", &v) && v >= 0)\n'
        f'         {target} = v;'
    )

override_block = f'''   /* {MARKER}.
    * Built-in game profiles and the accuracy policy are the automatic base.
    * -1 (Game default) leaves that decision untouched; an explicit value is
    * deliberately applied afterwards so the user has final control. */
   {{
      int v;
{chr(10).join(override_lines)}
   }}

   if (settings.n64_z_scale)
      ZLUT_init();

   //frame buffer
'''
glide = replace_once(glide, override_anchor, override_block, "Glide64 post-profile overrides")

BASE_OPTS.write_text(base, encoding="utf-8")
WRAPPER.write_text(wrapper, encoding="utf-8")
MAIN.write_text(main, encoding="utf-8")
GLIDE_INI.write_text(glide, encoding="utf-8-sig")

print(f"Applied {len(options)} permanent Glide64 options with semantic descriptions.")
print("Added System Timing submenu: Count Per Op + SI DMA Duration.")
print("Auto preserves ROM database values; explicit timing values apply at content start.")
print("DelaySI was not added: the post-June-2026 core no longer carries it as an independent runtime setting.")
print("Pokémon Stadium 2's final forced fb_emulation=off compatibility exception is preserved.")
print("Next: git diff --check && make clean && make -j$(nproc)")
