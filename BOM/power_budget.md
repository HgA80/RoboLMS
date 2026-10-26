# Task 5 - Power budget and battery selection

Block diagram: `WiringAndPowerDiagram.jpeg`. Parts: `bom.md`.
Everything marked *assumed* should be replaced with a multimeter measurement once the robot is built.

## 1. Current draw per component

Rails: motors run straight from the battery (11.1 V nominal, 12.6 V full). Everything else runs from the 5 V buck output. The ESP32 board's on-board linear regulator then makes 3.3 V, so the 3.3 V loads draw their current from the 5 V rail too.

| Component | Rail | Idle (mA) | Typical (mA) | Peak / stall (mA) | Source / assumption |
|---|---|---|---|---|---|
| 2x N20 12 V motors | 11.1 V | 0 | 400 (200 each, assumed) | 2,000 (1,000 each) | Stall about 1 A at 12 V from a GA12-N20 12 V listing; typical is assumed at about 20% of stall. Idle means motors off. |
| ESP32 chip | 3.3 V | 60 | 100 | 240 | Espressif datasheet: 30-68 mA with CPU at 240 MHz and radio off; Wi-Fi TX peak 240 mA; BLE TX 130 mA. Typical assumes BLE/Wi-Fi diagnostics with moderate duty. |
| ESP32 board overhead (USB-UART chip, LED, regulator quiescent) | 5 V | 15 | 15 | 15 | Assumed. |
| IR array (5 x TCRT5000) | 3.3 V | 100 | 100 | 100 | Listing figure for a 5-channel array; always on. |
| 2x encoders | 3.3 V | 20 | 20 | 20 | Assumed 10 mA each. |
| TB6612 logic (VCC) | 3.3 V | 2 | 2 | 2 | Datasheet ICC about 1.1-2.2 mA. Standby current is 1 uA or less. |
| MP1584 buck converter | 11.1 V in | loss = 10% of output power | same | same | Assumed 90% efficiency (conservative). |

## 2. Totals, as seen at the battery

Method: add the 3.3 V loads, add the board overhead to get the 5 V-rail current, convert to battery current with `I_batt = (5 V x I_5V) / (0.9 x V_batt)`, then add motor current directly.

| Scenario | 3.3 V loads (mA) | 5 V rail (mA) | Buck input (mA) | Motors (mA) | **Battery total** |
|---|---|---|---|---|---|
| Idle (motors off, radio off), 11.1 V | 182 | 197 | 99 | 0 | **~99 mA** |
| Typical (BLE diagnostics, motors ~200 mA each), 11.1 V | 222 | 237 | 119 | 400 | **~519 mA** |
| Heavy (motors ~500 mA each), 11.1 V | 222 | 237 | 119 | 1,000 | **~1,119 mA** |
| **Worst case** (Wi-Fi TX peak + both motors stalled), 12.6 V full pack | 362 | 377 | 166 | 2,000 | **~2,166 mA (~2.2 A)** |

Checks against component limits:
- Worst case 2.2 A is only 1.7C on a 1300 mAh pack rated 40C (52 A), so the battery has a large margin.
- Each motor channel at stall (about 1.0 A) is at the TB6612's recommended 1.0 A limit. This is the tight spot (see `docs/datasheet_tb6612fng.md`).
- The 3.3 V regulator on the board carries up to 362 mA, within the typical 800 mA capability of its regulator type, and it dissipates about (5 - 3.3) x 0.362 = 0.6 W.
- The MP1584 is rated 3 A; we use under 0.4 A.

## 3. Runtime estimate

Assumptions: battery 1300 mAh at 11.1 V (14.4 Wh); **80% usable** (stop at about 3.3 V per cell, so 20% is kept in reserve to protect the cells and allow for ageing); constant-current approximation; typical load as above.

- Usable capacity = 1300 x 0.8 = **1040 mAh**
- **Typical:** 1040 / 519 = **2.0 h (about 120 min)** 
- Cross-check by energy: 14.4 Wh x 0.8 = 11.5 Wh; power = 0.519 A x 11.1 V = 5.8 W; 11.5 / 5.8 = 2.0 h.
- Idle: 1040 / 99 = about 10.5 h
- Heavy: 1040 / 1119 = about 56 min
- Worst case (stalled): 1040 / 2166 = about 29 min (a floor, not a realistic use case)

Observation: motors account for about 77% of the typical draw (400 of 519 mA), so runtime depends mainly on how hard the robot is driven, not on the electronics.

## 4. Cell chemistry: LiPo vs Li-ion (18650)

| Factor | LiPo pouch (chosen: Orange 3S 1300 mAh 40C) | Li-ion 18650 (3 cells in series, 3S1P) |
|---|---|---|
| Energy density | About 130-140 Wh/kg at pack level, calculated from Robu listing weights (1000 mAh = 85 g, 2200 mAh = 175 g) | About 200-250 Wh/kg at cell level for a typical 3000 mAh cell (typical figures, not from a listing) |
| Discharge capability (C-rating) | 40C continuous = 52 A on 1300 mAh; low voltage sag at motor start | Energy cells about 1-2C (3-6 A); high-drain cells about 10C |
| Weight | About 110 g for 14.4 Wh (scaled from the listings; estimate) | About 140 g for three cells, plus holder and protection board, for about 33 Wh |
| Shape | Flat, fits a low chassis | Cylinders in a holder: taller and bulkier |
| Cost | Rs 1,399 (about Rs 97 per Wh) | Estimate Rs 1,100-1,700 for cells, holder and protection board (about Rs 33-50 per Wh); not verified |
| Safety / handling | Soft pouch: needs a balance charger, a low-voltage alarm, and care against puncture and over-discharge | Hard case, more tolerant of rough handling |

**Choice: LiPo.** Li-ion wins on energy density and cost per Wh, but this robot needs only about 1.3 Ah, and its worst-case current is just 2.2 A, so extra energy is not what limits it. At this size the LiPo is lighter in absolute terms (less inertia for turning and braking), slimmer, comes as a ready 3S pack with XT60 and balance leads that match the B3 charger, delivers motor start-up surges with little voltage sag, and matches Appendix A. Its risks are handled with the balance charger, a low-voltage alarm (about 9.9 V) and storage at about 3.8 V per cell.
