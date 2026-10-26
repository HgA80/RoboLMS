# LMS / Meshmerize fresher selection tasks

ESP32 line / maze follower: PID line following in C++ (Arduino framework), a BOM within Rs 10,000, and the supporting analysis. Tasks are listed with the file that answers each.

## Repository layout

| Folder | Contents |
|---|---|
| `BOM/` | `bom.md` (BOM, prices, links, TB6612 vs L298N), `power_block_diagram.svg`, `power_budget.md`, `wiring_diagram.svg` |
| `docs/` | Notes on control theory, datasheet, wiring and troubleshooting, wireless diagnostics and maze solving, with their diagrams |
| `src/` | `line_follower/` (ESP32 sketch and core logic) and `tests/` (PC test program) |

## Where each task is answered

| Task | Where |
|---|---|
| 1 Basic coding | `src/line_follower/lf_core.h` (`linePosition`, `clampInt`); examples in `src/tests/test_core.cpp`; summary below |
| 2 Datasheet literacy | `docs/datasheet_tb6612fng.md` |
| 3 PID control | `docs/pid_control_and_tuning.md` |
| 4 BOM | `BOM/bom.md`, `BOM/power_block_diagram.svg` |
| 5 Power budget | `BOM/power_budget.md` |
| 6 Electronics and hardware | `BOM/wiring_diagram.svg`, `docs/wiring_and_troubleshooting.md` |
| 7 Wireless diagnostics | `docs/wireless_diagnostics.md`, `docs/wireless_architecture.svg` |
| 8 Maze solving | `docs/maze_solving.md` and its hand-drawn annotated maze diagram (in `docs/`) |
| 9 Implementation | `src/line_follower/` (code); tuning notes in `docs/pid_control_and_tuning.md` |

## How to run

**PC tests (no robot needed).** From the repository root:

```
g++ -std=c++11 -Wall -Wextra -o test_core src/tests/test_core.cpp
./test_core
```

It runs 34 checks against hard-coded sensor arrays and prints a telemetry-style table for a scripted run. It exits with code 0 if every check passes.

**On the ESP32.** Open `src/line_follower/line_follower.ino` in the Arduino IDE (the sketch folder also holds `lf_core.h`), install the Arduino-ESP32 core 3.x (`analogWrite()` is built in from that version), select ESP32 Dev Module and upload. After reset, sweep the sensor array over the line and the background for 3 seconds (calibration), then place the robot on the line. Serial Monitor at 115200 baud shows a CSV stream: time, five calibrated sensor values, position, error, correction, left and right command, state.

## Task 1 summary

- **`linePosition`:** weighted centroid, `sum(w_i * s_i) / sum(s_i)` with weights `[-2, -1, 0, 1, 2]`. If the sum is zero (or below a noise threshold) it returns "not found" instead of dividing by zero. Example: `{0, 600, 900, 0, 0}` gives -0.4 (line slightly left); `{0, 0, 0, 0, 0}` gives not found.
- **`clampInt`:** limits a signed command to `[-max, +max]`, so the sign is kept and only the size is capped. Example with max 200: 250 gives 200, -300 gives -200, 120 stays 120. A negative limit is treated as its magnitude.

## Assumptions

- Sensor readings: higher voltage means darker (line), 12-bit ADC, min-max calibration at startup (Appendix A).
- Junction angles need not be exactly 90 degrees (branches are only classified as left, straight or right); the goal is marked distinctly; both wheels have encoders, giving edge lengths and heading; wheel diameter and track width are measured on the built robot.
- Prices in `BOM/bom.md` are as listed on the supplier pages on 25 Oct 2026, and "Est." items are not verified. Motor currents in `BOM/power_budget.md` are estimates to be replaced by measurements.

## Limitations

- **Not tested on hardware.** The core logic is tested on a PC; the `.ino` was not compiled against a board here. PID gains are starting values, not tuned values.
- **Appendix A pin caveats on a real WROOM-32:** GPIO11 (IN4) is an SPI-flash pin and will not work (remap to GPIO4 by changing the constant in the sketch); GPIO12 is a boot-strapping pin (safe with the TB6612's internal pull-down); GPIO25 is an ADC2 channel, unreadable while Wi-Fi is on, which is why the diagnostics use BLE.
- **Driver choice:** the BOM uses a TB6612FNG instead of Appendix A's L298N; the pin constants are the same for both, so the firmware runs on either driver.
- Encoder reading, soft-start ramp, stall detection, the BLE telemetry link, junction / dead-end / goal detection and the maze exploration are described in the notes but **not implemented** in the code.
- Fault-detection thresholds for the wireless diagnostics are reasoned, not tested on real data.

## References

- Toshiba, TB6612FNG datasheet (absolute maximum ratings, operating range, electrical characteristics) and application note.
- Espressif, ESP32 datasheet and ADC documentation (current consumption figures; ADC2 and Wi-Fi restriction).
- Supplier pages used for the BOM: Zbotic (ESP32 board, TB6612 module, motors, charger, buck converter, wheels, bracket, caster), Robu (LiPo battery), Robocraze (sensor array); Probots listings for price comparison.
