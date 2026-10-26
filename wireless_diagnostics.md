# Task 7 - Wireless sensor diagnostics

Block diagram: `docs/wireless_architecture.svg`.

**Scenario:** the maze surface has small bumps, and one or more IR sensors give intermittent readings. We must find the faulty sensor(s) in real time, without stopping the robot or touching the hardware.

## 1. Protocol selection: BLE

| | Wi-Fi (WebSocket) | Classic Bluetooth (SPP) | **BLE (GATT notifications)** |
|---|---|---|---|
| Latency (typical) | about 5-30 ms, with jitter | about 10-50 ms | about 7.5-30 ms (set by the connection interval; 15 ms minimum on iPhone) |
| Throughput | Mbit/s | hundreds of kbit/s | tens to hundreds of kbit/s, platform-dependent |
| Radio current (ESP32 datasheet, as in Task 5) | TX peaks up to 240 mA | peaks about 130 mA, but a continuous stream averages higher | TX peaks about 130 mA, with the lowest average draw |
| Conflict with our hardware | **ADC2 is unusable while Wi-Fi is on, and sensor S5 is on GPIO25 (ADC2)** | none | none |
| Ease | access point or IP setup, server code | easiest: a serial-terminal app | moderate: one GATT service; phone side needs no app build (Web Bluetooth or nRF Connect) |
| Phone support | any browser | Android and PC only, not iOS | Android, PC and iOS (via nRF Connect or Bluefy) |

**Choice: BLE**, for four reasons.

1. **Wi-Fi would corrupt the very measurement we are diagnosing.** On the ESP32 the Wi-Fi driver takes over ADC2, so `analogRead()` on GPIO25 returns unreliable values while Wi-Fi is active. We would see "faulty sensor 5" caused by our own diagnostic link. (Appendix A labels GPIO25 as ADC1; on a real WROOM-32 it is ADC2.) BLE and Classic Bluetooth do not use ADC2.
2. **Lower power and fewer brown-outs.** Wi-Fi's 240 mA TX bursts land on the same small supply that Task 6 shows is sensitive to current spikes. BLE bursts are lower and the average draw is smaller.
3. **Latency is good enough, because the design does not depend on it.** Frames are time-stamped on the ESP32, and fault detection runs on-board, so link jitter only delays the display and never changes the data. A 7.5-15 ms connection interval at 100 Hz is ample for a person watching a live plot.
4. **Bandwidth is sufficient.** 16 bytes at 100 Hz is 1.6 kB/s (about 13 kbit/s), far below what BLE can carry.

Classic Bluetooth SPP is a close second (simplest, serial-terminal style), but it is not available on iPhones, draws more power continuously, and uses more flash. If S5 were not on ADC2 and power were not tight, Wi-Fi WebSocket would give the nicest dashboard.

## 2. System architecture (stage by stage)

1. **IR sensors.** Five analog outputs, read as raw 12-bit values. Raw values are sent, not calibrated ones, because calibration (min-max scaling) hides "stuck" and "out of range" faults.
2. **Sample.** Done in the same 100 Hz loop as the PID, on core 1. Each frame gets a millisecond timestamp.
3. **Health check (on the ESP32).** Cheap per-sensor rules over a short history (see Section 3). The result is a fault bit per sensor.
4. **Pack and queue.** A 16-byte binary frame: 4 B time, 5 x 2 B raw readings, 1 B fault mask, 1 B flags (line lost, bump event). It goes into a non-blocking queue that drops the oldest frame when full, so telemetry can never stall the control loop.
5. **BLE notify (core 0).** One GATT service with one notify characteristic. The BLE stack runs on the other core by default, so the control loop keeps its timing. The characteristic is read-only for the phone: diagnostics only observe; they never recalibrate or stop anything.
6. **Client (phone or PC).** A Web Bluetooth page in Chrome, or a small Python script using `bleak` on a PC. It decodes frames and uses the sequence/time stamps to spot dropped frames.
7. **Display and log.** A live plot of the five channels with a fault indicator each, plus a CSV file (time, five raw values, mask, flags) for later analysis.

If the link drops, the robot keeps driving. If a frame is lost, the gap in the timestamps shows it.

## 3. How the faulty sensor is identified

The bumps make this tricky. A bump lifts the whole sensor bar, so **all five readings change together** (common-mode); a real line edge moves **one sensor and its neighbour in opposite directions** and persists; a faulty sensor jumps **on its own**. The rules use that difference. All thresholds are starting values to tune on real data.

| Pattern | Rule | Meaning |
|---|---|---|
| Bump | 4 or more sensors change in the same direction by more than about 400 counts in one frame | Mark a "bump event"; not counted against any sensor |
| Isolated spike or dropout | One sensor changes by more than about 400 counts while both neighbours change by less than about 150, and it returns within about 3 frames; flagged if it happens in 3 of the last 10 frames | Intermittent connection or sensor: **faulty** |
| Stuck | Reading unchanged (within about 5 counts) for more than 0.5 s while at least two other sensors move by more than about 300 counts | Dead or stuck: **faulty** |
| Out of range or floating | Reading below the calibrated minimum or above the maximum by a margin (for example below about 50 counts while neighbours read high) | Disconnected wire (GPIO34 and 35 have no internal pull-down): **faulty** |

The sensor whose fault bit stays on, and whose changes are not shared by its neighbours or by the bump flag, is the faulty one. The CSV log lets you confirm this afterwards by comparing each channel against the others.

## 4. Limitations

- The phone or PC must stay within roughly 5-10 m of the robot.
- Web Bluetooth does not work in iOS Safari; use nRF Connect or Bluefy there.
- BLE throughput varies by phone, so the 16-byte frame is kept small.
- The rule thresholds have not been tested on real data, only reasoned from the behaviour described above.
