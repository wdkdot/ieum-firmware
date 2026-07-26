# Ieum firmware agent instructions

## Instruction priority

Apply repository instructions in this order:

1. **Ieum-specific rules in this `AGENTS.md`**
2. **Upstream Meshtastic rules in `.github/copilot-instructions.md`**
3. Existing code conventions and nearby implementation patterns

When an upstream convention conflicts with a confirmed Ieum hardware requirement, follow the Ieum rule while keeping the change as narrow and variant-specific as possible. Never override a safety rule, generated-code restriction, or operator-confirmation requirement.

Before any non-trivial change, read this file completely and then read `.github/copilot-instructions.md` completely.

## Upstream Meshtastic context

This repository is a fork of the official Meshtastic firmware, a C++17 embedded project supporting nRF52, ESP32, RP2040, STM32WL, and Portduino devices. The upstream firmware already provides the mesh protocol, SX1262 radio stack, BLE, USB, GNSS framework, telemetry framework, InkHUD, power state handling, and PlatformIO build system.

The original detailed agent guidance remains canonical for shared Meshtastic behavior in `.github/copilot-instructions.md`. In particular, retain its rules for formatting, thread safety, module patterns, test execution, hardware-tool safety, and generated protobuf files. This `AGENTS.md` intentionally summarizes only the upstream rules most relevant to Ieum and gives Ieum development policy higher priority.

Relevant upstream paths:

| Path                               | Purpose                                      |
| ---------------------------------- | -------------------------------------------- |
| `src/`                             | Shared firmware implementation               |
| `src/gps/`                         | GNSS framework and device handling           |
| `src/graphics/niche/InkHUD/`       | E-ink user interface                         |
| `src/graphics/niche/Drivers/EInk/` | InkHUD panel drivers                         |
| `src/modules/Telemetry/Sensor/`    | Environmental sensor drivers                 |
| `src/motion/`                      | Motion sensor support                        |
| `src/power/`                       | Power and PMIC support                       |
| `src/platform/nrf52/`              | Shared nRF52 platform code                   |
| `variants/nrf52840/`               | nRF52840 board definitions                   |
| `src/mesh/generated/`              | Generated protobuf code; never edit directly |

## Version and branch policy

- The validated integration branch is `ieum/main`.
- Its upstream base is official tag `v2.7.26.54e0d8d`, commit `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`.
- `origin` is `wdkdot/ieum-firmware`; `upstream` is `meshtastic/firmware`.
- Do not merge upstream `develop` directly into `ieum/main` and do not use GitHub's **Sync fork** action.
- Create focused development branches from `ieum/main`, for example `ieum/board-support`, `ieum/inkhud`, or `ieum/power-control`.
- Merge only reviewed and hardware-validated work into `ieum/main`.
- Upgrade only to a selected official release tag on a temporary `ieum/upgrade-v2.x.y` branch.
- Before an upgrade, run `git merge-base --is-ancestor <current-tag> <new-tag>`.
- If release histories diverge, preserve `ieum/main`; do not force-push, rewrite shared history, or blindly merge. Create a new tag-based integration branch and port only the Ieum commit set after operator review.
- Tag validated releases with both product and upstream versions, for example `ieum-v0.1.0-mt2.7.26`.

## Confirmed Ieum hardware

| Function             | Device                                                                   | Direction                                                                                |
| -------------------- | ------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| MCU and LoRa         | RAK4630 (`nRF52840` + `SX1262`)                                          | Reuse existing nRF52 and RAK4631 support where electrically compatible                   |
| Display              | Good Display `GDEY0266T90H`, 2.66-inch, 184 x 360, monochrome, `SSD1685` | Add an InkHUD panel driver                                                               |
| Display load switch  | `TPS22919QDCKRQ1`                                                        | Power the panel only during display operations                                           |
| GNSS                 | `ATGM336H-5NR-32`                                                        | Reuse GNSS framework; add Ieum power and UART lifecycle                                  |
| Temperature/humidity | `AHT20-F`                                                                | Reuse the AHT10/AHT20 telemetry driver first                                             |
| Pressure             | `BMP388_TOKMAS`                                                          | Use the dedicated Tokmas/SPA06-compatible telemetry driver; Bosch BMP3XX is incompatible |
| Motion               | `MMA8652FC`                                                              | Add a reusable motion driver                                                             |
| Charger and PMIC     | `BQ25628E`                                                               | Add conservative, datasheet-verified power support                                       |

The final GPIO map, active levels, pull configuration, shared-bus details, and power stabilization delays are not yet authoritative. Never infer them from ESP32 sample code, a similar RAK board, or a generic module datasheet. Verify them against the final Ieum schematic, pin map, component documentation, and real hardware.

The hardware and integration documentation under `ieum-docs/` is the current Ieum design source of truth. Keep it synchronized as decisions become verified. Do not commit `.codex/` session metadata unless the operator explicitly requests it.

## Implementation boundaries

Keep board-specific configuration in:

```text
variants/nrf52840/ieum/
├── platformio.ini
├── variant.h
├── variant.cpp
└── nicheGraphics.h
```

Define the dedicated PlatformIO environment as `ieum`. Reuse the RAK4631/nRF52840 board configuration only where the RAK4630 wiring and memory layout are confirmed compatible.

Expected reusable driver additions:

```text
src/motion/MMA8652FCSensor.*
src/power/BQ25628E.*
src/graphics/niche/Drivers/EInk/GDEY0266T90H.*
```

Prefer existing variant hooks such as `earlyInitVariant()`, `variant_shutdown()`, and `variant_nrf52LoopHook()` over changes to shared startup code.

During initial board support, avoid modifying these areas unless evidence proves a shared integration point is required:

- `src/main.cpp`
- `src/platform/nrf52/main-nrf52.cpp`
- `src/mesh/Router.*`
- `src/mesh/NodeDB.*`
- protobuf definitions and generated bindings

Reuse upstream LoRa, BLE, USB, routing, NodeDB, GNSS, telemetry, and InkHUD behavior. Keep Ieum conditionals near hardware boundaries instead of spreading `#ifdef IEUM` throughout shared logic.

## Power sequencing and backfeed prevention

- Configure GNSS and E-ink power controls to their verified inactive states during early boot, before peripheral initialization.
- Treat power switching as a peripheral lifecycle operation, not a single GPIO write.
- Use bounded waits and explicit failure states. A failed peripheral must not prevent LoRa, BLE, USB, or SWD recovery.

GNSS shutdown sequence:

1. Stop GNSS parsing and new UART transactions.
2. Allow an active transmission to finish when required.
3. Disable or detach the UART peripheral.
4. Put MCU UART RX/TX pins into the board-verified high-impedance, non-backfeeding state with no unverified pulls.
5. Disable GNSS main power.
6. Preserve the separate GNSS VBAT backup domain unless performing an operator-requested cold-start test.

GNSS startup reverses this safely: enable main power, wait the verified stabilization time, restore UART pin mux and configuration, and only then resume communication.

E-ink shutdown sequence:

1. Complete the refresh and wait for BUSY with a timeout.
2. Put SSD1685 into deep sleep.
3. Move SPI and control pins into the board-verified non-backfeeding state.
4. Disable the TPS22919 display rail.

Do not power off the panel immediately after sending a refresh command. Do not continue partial refreshes indefinitely; use periodic full refresh according to measured panel behavior and manufacturer guidance.

## Development order

1. Create the Ieum variant and prove SWD and USB recovery.
2. Verify RAK4630 LoRa transmit/receive and BLE connectivity.
3. Scan I2C and identify every fitted device before enabling higher-level drivers.
4. Reuse and validate AHT20 and BMP388 telemetry.
5. Validate ATGM336H GNSS, fix timeout, main-power switching, UART isolation, and off-state backfeed current.
6. Add MMA8652FC sampling and motion interrupts.
7. Add BQ25628E identification and status reporting, followed by conservative charging controls.
8. Add GDEY0266T90H InkHUD full refresh.
9. Add E-ink deep sleep and load-switch control, then fast or partial refresh if safely supported.
10. Add motion-aware GNSS scheduling only after GNSS and motion sensing are independently stable.
11. Run integrated stability and power measurements for at least 24 hours.

At every stage, verify that LoRa, BLE, USB recovery, reboot, and configuration persistence still work.

## Coding, safety, and verification rules

- Follow existing C++17 conventions and nearby subsystem patterns.
- Use Meshtastic logging macros from `src/DebugConfiguration.h`.
- Keep comments short and explain only non-obvious reasons.
- Use `Throttle` helpers for elapsed-time checks instead of rollover-unsafe raw `millis()` comparisons.
- Use concurrency primitives already established by the subsystem; do not introduce unsynchronized peripheral access.
- Never edit `src/mesh/generated/**`.
- Run `trunk fmt` before proposing a commit.
- Build the board with `pio run -e ieum` once the environment exists.
- Run relevant native tests for shared logic changes.
- Record which real hardware checks were performed; successful compilation alone is not hardware verification.
- Do not flash, erase, reboot, shut down, change regulatory region, factory-reset hardware, or rewrite Git history without explicit operator approval.
- Only one serial or hardware-tool operation may own a device port at a time.
- Do not speculate about root causes. Report `unknown` and state what measurement would disambiguate when evidence is insufficient.

## Quick commands

```bash
pio run -e ieum
pio run -e ieum -t clean
trunk fmt
./bin/run-tests.sh
git fetch upstream --tags
```

Use the test command available in this pinned upstream tag. If repository test infrastructure differs from newer upstream documentation, inspect the checked-out scripts rather than assuming the latest `develop` workflow applies.
