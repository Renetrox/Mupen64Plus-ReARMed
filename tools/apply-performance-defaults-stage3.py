#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CORE_OPTIONS = ROOT / "libretro/libretro_core_options.h"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        print(f"SKIP: {label} ya aplicado")
        return text
    if old not in text:
        raise RuntimeError(f"No se encontro el ancla para {label}")
    print(f"OK:   {label}")
    return text.replace(old, new, 1)


def main() -> int:
    text = CORE_OPTIONS.read_text(encoding="utf-8")

    # Hide renderer categories that are not actually compiled in.
    old = '''   {
      "gliden64",
      "GLideN64",
      "Configure GLideN64 Options."
   },
   {
      "glide64",
      "Glide64",
      "Configure Glide64 Options."
   },
'''
    new = '''#ifdef HAVE_GLIDEN64
   {
      "gliden64",
      "GLideN64",
      "Configure GLideN64 Options."
   },
#endif
#ifdef HAVE_GLIDE64
   {
      "glide64",
      "Glide64",
      "Configure Glide64 Options."
   },
#endif
'''
    text = replace_once(text, old, new, "categorias GFX segun build")

    # This fork is performance-oriented: Glide64 automatic frameskip by default.
    old = '''        "disabled"
    },
#endif
    {
        CORE_NAME "-gfxplugin-accuracy",
'''
    new = '''        "auto"
    },
#endif
    {
        CORE_NAME "-gfxplugin-accuracy",
'''
    text = replace_once(text, old, new, "Glide64 frameskip default = Automatic")

    # Rice only has the historical 1-in-2 mode; make that the fork default.
    old = '''        "disabled"
    },
#endif
    {
        CORE_NAME "-gfxplugin",
'''
    new = '''        "1"
    },
#endif
    {
        CORE_NAME "-gfxplugin",
'''
    text = replace_once(text, old, new, "Rice frameskip default = 1")

    # Only advertise renderers that exist in this build. On GLES2 targets the
    # Makefile disables GLideN64, so it disappears from the menu instead of
    # leaving a dead/unavailable choice. Prefer Glide64 as this fork's default.
    old = '''        {
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
            { "gliden64", NULL },
            { "glide64", NULL },
            { "gln64", NULL },
            { "rice", NULL },
#endif
            { "angrylion", NULL },
#ifdef HAVE_PARALLEL
            { "parallel", NULL },
#endif
            { NULL, NULL },
        },
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
        "gliden64"
#else
        "angrylion"
#endif
'''
    new = '''        {
#ifdef HAVE_GLIDE64
            { "glide64", NULL },
#endif
#ifdef HAVE_RICE
            { "rice", NULL },
#endif
#ifdef HAVE_GLN64
            { "gln64", NULL },
#endif
#ifdef HAVE_GLIDEN64
            { "gliden64", NULL },
#endif
#ifdef HAVE_THR_AL
            { "angrylion", NULL },
#endif
#ifdef HAVE_PARALLEL
            { "parallel", NULL },
#endif
            { NULL, NULL },
        },
#ifdef HAVE_GLIDE64
        "glide64"
#elif defined(HAVE_RICE)
        "rice"
#elif defined(HAVE_GLN64)
        "gln64"
#elif defined(HAVE_GLIDEN64)
        "gliden64"
#else
        "angrylion"
#endif
'''
    text = replace_once(text, old, new, "Glide64 default + ocultar renderers no compilados")

    CORE_OPTIONS.write_text(text, encoding="utf-8")
    print("\nStage 3 aplicado.")
    print("Defaults: GFX=Glide64, Glide64 Frameskip=Automatic, Rice Frameskip=1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
