#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

CORE_OPTIONS = ROOT / "libretro/libretro_core_options.h"
RICE_CONFIG = ROOT / "gles2rice/src/RiceConfig.cpp"
RSP_PARSER = ROOT / "gles2rice/src/RSP_Parser.cpp"


def replace_once(path: Path, old: str, new: str, marker: str) -> bool:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        print(f"SKIP: {path.relative_to(ROOT)} ya contiene {marker}")
        return False
    if old not in text:
        raise RuntimeError(
            f"No se encontró el ancla esperada en {path.relative_to(ROOT)}:\n{old[:200]}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"OK:   {path.relative_to(ROOT)}")
    return True


def main() -> int:
    changed = 0

    # 1) Expose a minimal Rice frameskip option in RetroArch Core Options.
    gfx_anchor = '''    {
        CORE_NAME "-gfxplugin",
'''
    rice_option = '''#ifdef HAVE_RICE
    {
        CORE_NAME "-rice-frameskip",
        "(Rice) Frameskip",
        "Frameskip",
        "Skip every other Rice graphics display list before rendering it. This may improve performance on low-power hardware at the cost of visual smoothness.",
        NULL,
        NULL,
        {
            { "disabled", "Disabled" },
            { "1", "1 (Skip every other frame)" },
            { NULL, NULL },
        },
        "disabled"
    },
#endif
    {
        CORE_NAME "-gfxplugin",
'''
    changed += replace_once(
        CORE_OPTIONS,
        gfx_anchor,
        rice_option,
        'CORE_NAME "-rice-frameskip"',
    )

    # 2) Wire the frontend option into Rice's existing options.bSkipFrame.
    config_anchor = '''   options.bSkipFrame = ConfigGetParamBool(l_ConfigVideoRice, "SkipFrame");
'''
    config_replacement = '''   options.bSkipFrame = ConfigGetParamBool(l_ConfigVideoRice, "SkipFrame");

   /* Renetrox frameskip: expose Rice's dormant SkipFrame setting through
    * libretro Core Options. Keep the original Rice config value as fallback. */
   {
      struct retro_variable rice_frameskip_var = { "parallel-n64-rice-frameskip", NULL };
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &rice_frameskip_var) &&
          rice_frameskip_var.value)
         options.bSkipFrame = strcmp(rice_frameskip_var.value, "1") == 0;
   }
'''
    changed += replace_once(
        RICE_CONFIG,
        config_anchor,
        config_replacement,
        "parallel-n64-rice-frameskip",
    )

    # 3) Restore the historical Rice execution path: skip one of every two
    # graphics tasks before display-list parsing/rendering, while still firing
    # DP/SP interrupts so emulation continues.
    parser_anchor = '''    status.bScreenIsDrawn = true;

    if( currentRomOptions.N64RenderToTextureEmuType != TXT_BUF_NONE && defaultRomOptions.bSaveVRAM )
'''
    parser_replacement = '''    status.bScreenIsDrawn = true;

    /* Rice historically supported plugin-level frameskip here. Unlike merely
     * suppressing video_cb(), this avoids parsing/rendering the skipped display
     * list and therefore can reduce actual graphics workload. */
    static unsigned riceSkipFrameCounter = 0;
    if (options.bSkipFrame)
    {
        riceSkipFrameCounter++;
        if (riceSkipFrameCounter & 1u)
        {
            TriggerDPInterrupt();
            TriggerSPInterrupt();
            return;
        }
    }
    else
    {
        riceSkipFrameCounter = 0;
    }

    if( currentRomOptions.N64RenderToTextureEmuType != TXT_BUF_NONE && defaultRomOptions.bSaveVRAM )
'''
    changed += replace_once(
        RSP_PARSER,
        parser_anchor,
        parser_replacement,
        "riceSkipFrameCounter",
    )

    print()
    if changed:
        print("Rice Frameskip Stage 1 aplicado.")
        print("Revisa con: git diff --check && git diff")
    else:
        print("No hubo cambios: el parche ya estaba aplicado.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
