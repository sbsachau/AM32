# ADR 0001: Test strategy and layering for the AM32 firmware

Status: accepted (work in progress on branch `feature/unit-tests`)

## Context

AM32 is BLDC ESC firmware with many board targets. The existing build
(`Makefile`, per-MCU `*makefile.mk`) compiles all of `Src/` per target with
`-Werror -Wall -Wundef -Wextra`. Flash is tight on the small MCUs.

Baseline before any change (commit `2a695a8`, target `AM32_SEQURE_4IN1_F051_2.22`):
FLASH 22,892 B, RAM 3,704 B. Any refactoring must not grow these numbers.

Findings while reading the code:

- Logic such as DShot decoding, the command state machine and the helpers in
  `functions.c` is mixed with globals and hardware access.
- Several globals used across files (`EDT_ARMED`, `command_count`,
  `programming_mode`, ...) live in `dshot.c` but are not declared in a header.
- `get_crc8()` and `update_crc8()` are not declared in `functions.h`.
- `map()` has an unreachable branch for reversed input ranges.

## Decision

1. **Tests first, then refactor.** Characterization tests pin down today's
   behaviour (including quirks). Refactoring is only done behind green tests.
2. **Ceedling + Unity** for host tests (CMock later, where interfaces exist).
   The tests live in `test/` and are added next to the existing build. The
   firmware Makefiles and all board targets stay untouched.
3. **Link seams instead of function pointers.** Production code is linked
   unchanged on the host; globals and hardware calls are replaced by test
   doubles in `test/support/`. Function-pointer tables would cost flash and RAM
   on small MCUs, so ports are resolved at link time.
4. **Target layering for `dshot.c`** (after the characterization tests are green):
   - `dshot_codec`: pure frame decoding, CRC, telemetry encoding
   - `dshot_commands`: command state machine, using small ports
     (eeprom, tone, pin)
   - MCU adapters stay where they are
5. **CI runs the tests first** (`.github/workflows/unit-tests.yml`).

## Consequences

- Tests run in seconds on any machine with Ruby; no hardware needed.
- Characterization tests may document behaviour that looks wrong. Such cases
  are fixed in separate, clearly marked commits, never silently during refactoring.
- Each refactoring step is checked against the flash/RAM baseline above.
