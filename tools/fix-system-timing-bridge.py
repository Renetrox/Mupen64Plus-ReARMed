#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "libretro/libretro.c"
PRIV = ROOT / "libretro/libretro_private.h"
MAIN = ROOT / "mupen64plus-core/src/main/main.c"
MARKER = "ReARMed system timing libretro bridge"

for path in (LIB, PRIV, MAIN):
    if not path.exists():
        raise SystemExit(f"Missing expected file: {path}")

lib = LIB.read_text(encoding="utf-8")
priv = PRIV.read_text(encoding="utf-8")
main = MAIN.read_text(encoding="utf-8")

if MARKER in lib:
    print("System timing bridge is already fixed.")
    raise SystemExit(0)


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f"Anchor not found for {label}; source layout changed.")
    return text.replace(old, new, 1)

# Shared startup-only SI DMA override. -1 means preserve ROM database value.
lib_globals = '''uint32_t CountPerOp = 0;
uint32_t CountPerOpDenomPot = 0;
'''
lib_globals_new = '''uint32_t CountPerOp = 0;
uint32_t CountPerOpDenomPot = 0;
int32_t SIDMADurationOverride = -1;
'''
lib = replace_once(lib, lib_globals, lib_globals_new, "libretro timing globals")

priv_anchor = '''uint64_t parallel_n64_get_time_usec(void);
'''
priv_new = '''uint64_t parallel_n64_get_time_usec(void);

/* Startup-only system timing override read by the adopted core. */
extern int32_t SIDMADurationOverride;
'''
priv = replace_once(priv, priv_anchor, priv_new, "private timing bridge declaration")

# RetroArch owns Core Option parsing. Keep timing startup-only because both
# values are consumed while init_device() is being constructed.
update_anchor = '''   /* CPU core selection: pure interpreter (0), cached interpreter (1),
'''
update_block = '''   /* ReARMed system timing libretro bridge.
    * Auto keeps the ROM database/default. Explicit values are consumed only
    * when the emulated device is built, so changing them requires restart. */
   if (startup)
   {
      CountPerOp = 0;
      SIDMADurationOverride = -1;

      var.key = "parallel-n64-count-per-op";
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
          strcmp(var.value, "auto") != 0)
      {
         long value = strtol(var.value, NULL, 0);
         if (value >= 1 && value <= 3)
            CountPerOp = (uint32_t)value;
      }

      var.key = "parallel-n64-si-dma-duration";
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
          strcmp(var.value, "auto") != 0)
      {
         long value = strtol(var.value, NULL, 0);
         if (value >= 0 && value <= 0x10000)
            SIDMADurationOverride = (int32_t)value;
      }
   }

'''
lib = replace_once(lib, update_anchor, update_block + update_anchor, "update_variables timing parsing")

# Remove the frontend API calls accidentally placed inside mupen64plus-core.
bad_main = '''    if (count_per_op <= 0)
        count_per_op = ROM_SETTINGS.countperop;

    /* ReARMed final Glide64 + system timing: explicit core-option values override the ROM database only
     * for this content start. Auto leaves the database/default untouched. */
    {
        struct retro_variable timing_var = { "parallel-n64-count-per-op", NULL };
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &timing_var) &&
            timing_var.value && strcmp(timing_var.value, "auto") != 0)
        {
            long value = strtol(timing_var.value, NULL, 0);
            if (value >= 1 && value <= 3)
                count_per_op = (uint32_t)value;
        }
    }

    if (count_per_op_denom_pot > 20)
        count_per_op_denom_pot = 20;

    si_dma_duration = ROM_SETTINGS.sidmaduration;
    {
        struct retro_variable timing_var = { "parallel-n64-si-dma-duration", NULL };
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &timing_var) &&
            timing_var.value && strcmp(timing_var.value, "auto") != 0)
        {
            long value = strtol(timing_var.value, NULL, 0);
            if (value >= 0 && value <= 0x10000)
                si_dma_duration = (int32_t)value;
        }
    }
'''
fixed_main = '''    if (count_per_op <= 0)
        count_per_op = ROM_SETTINGS.countperop;

    if (count_per_op_denom_pot > 20)
        count_per_op_denom_pot = 20;

    si_dma_duration = ROM_SETTINGS.sidmaduration;
    if (SIDMADurationOverride >= 0)
        si_dma_duration = SIDMADurationOverride;
'''
main = replace_once(main, bad_main, fixed_main, "main.c frontend/API removal")

LIB.write_text(lib, encoding="utf-8")
PRIV.write_text(priv, encoding="utf-8")
MAIN.write_text(main, encoding="utf-8")

print("Moved Count Per Op and SI DMA Core Option parsing to libretro.c.")
print("main.c now consumes only shared values and no longer calls the libretro environment API.")
