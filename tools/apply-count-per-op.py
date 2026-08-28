#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIBRETRO = ROOT / "libretro/libretro.c"
OPTS = ROOT / "libretro/libretro_core_options.h"
MARKER = "ReARMed Count Per Op test"

if not LIBRETRO.exists() or not OPTS.exists():
    raise SystemExit("Run this script from a Mupen64Plus-ReARMed checkout.")

libretro = LIBRETRO.read_text(encoding="utf-8")
opts = OPTS.read_text(encoding="utf-8")

if MARKER in libretro or MARKER in opts:
    print("Count Per Op test patch is already applied.")
    raise SystemExit(0)

# FZ exposes CountPerOp as a per-game advanced option with values 1..3.
# In this libretro core CountPerOp == 0 already means: use ROM_SETTINGS.countperop,
# i.e. preserve the ROM database/default value.  Keep exactly that behaviour.
option_anchor = '''    {
        CORE_NAME "-virefresh",
        "VI Refresh (Overclock)",'''

if option_anchor not in opts:
    raise SystemExit("Count Per Op core-option anchor not found; source layout changed.")

option_block = f'''    /* {MARKER}: expose the core timing control already consumed by main.c. */
    {{
        CORE_NAME "-count-per-op",
        "Count Per Op",
        NULL,
        "N64 CPU timing control used by the Mupen64Plus core. Game default preserves the ROM database CountPerOp value; 1, 2 or 3 explicitly override it for the next content start. Restart content after changing this option.",
        NULL,
        NULL,
        {{
            {{ "0", "Game default" }},
            {{ "1", "1" }},
            {{ "2", "2" }},
            {{ "3", "3" }},
            {{ NULL, NULL }},
        }},
        "0"
    }},
'''

opts = opts.replace(option_anchor, option_block + option_anchor, 1)

parse_anchor = '''   var.key = "parallel-n64-virefresh";
   var.value = NULL;
'''

if parse_anchor not in libretro:
    raise SystemExit("Count Per Op parser anchor not found; source layout changed.")

parse_block = f'''   /* {MARKER}.  main_pre_run() copies this global into count_per_op;
    * zero deliberately falls through to ROM_SETTINGS.countperop in main.c. */
   var.key = CORE_NAME "-count-per-op";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {{
      int value = atoi(var.value);
      if (value >= 0 && value <= 3)
         CountPerOp = (uint32_t)value;
   }}

'''

libretro = libretro.replace(parse_anchor, parse_block + parse_anchor, 1)

LIBRETRO.write_text(libretro, encoding="utf-8")
OPTS.write_text(opts, encoding="utf-8")

print("Applied Count Per Op core option: Game default, 1, 2, 3.")
print("Game default (0) preserves ROM_SETTINGS.countperop.")
print("Restart content after changing Count Per Op.")
print("Next: git diff --check && make clean && make -j$(nproc)")
