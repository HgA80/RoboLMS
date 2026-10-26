# Task 4 - Bill of Materials (LMS / Meshmerize robot, budget Rs 10,000)

Prices are INR incl. GST as listed on the supplier pages on 25 Oct 2026 and can change.
"Est." means no listing was verified; price it at checkout.

## Design summary
Differential-drive robot. ESP32-WROOM-32 reads a 5-channel analog IR array and drives two 12 V N20 encoder motors through a TB6612FNG. One 3S LiPo powers everything: the 11.1 V rail feeds the motor driver directly, and an MP1584 buck converter makes 5 V for the ESP32 Vin. The ESP32's 3.3 V rail powers the sensor array, the encoders and the driver logic, so no signal pin ever sees more than 3.3 V.

## A. Priced components

| # | Component (exact model) | Qty | Unit price (Rs) | Line total (Rs) | Supplier / link | Key specs | Why this part, why this quantity |
|---|---|---|---|---|---|---|---|
| 1 | ESP WROOM 32 Wi-Fi/Bluetooth Dev Board, 30-pin (SKU AI0038) | 1 | 410.64 | 410.64 | Zbotic - https://zbotic.in/product/esp-wroom-32-wifi-bluetooth-development-board/ | ESP32-WROOM-32, 4 MB flash, 30 pins, 3.3 V logic | Controller named in Appendix A. One board runs sensing, PID and Wi-Fi/BLE diagnostics. |
| 2 | TB6612FNG Dual Motor Driver Module 1A (SKU AI0999) | 1 | 147.50 | 147.50 | Zbotic - https://zbotic.in/product/tb6612fng-dual-motor-driver-module-1a-for-arduino/ | 1.2 A continuous / 3.2 A peak per channel, logic 2.7-5.5 V, MOSFET H-bridge, STBY pin | Two channels = left and right motor, so one module. MOSFET output loses ~0.5 V vs ~2 V for the L298N, and it takes 3.3 V logic directly. Pin-compatible with Appendix A's ENA/IN1/IN2/ENB/IN3/IN4. |
| 3 | N20 12V 300RPM Micro Metal Gear Motor With Encoder (GA12-N20) | 2 | 514.48 | 1,028.96 | Zbotic - https://zbotic.in/product/n20-12v-300rpm-micro-metal-gear-motor-with-encoder/ | 12 V, 300 RPM, metal gears, 3 PPR encoder, encoder supply 3.3-5 V, 2 signal wires | One per drive wheel. Encoders give distance and speed for the maze-mapping and closed-loop-speed extensions. At ~10.5 V from the driver this is roughly 260 RPM, about 0.46 m/s with a 34 mm wheel. |
| 4 | TCRT5000L 5-Channel Infrared Line Tracking Sensor Module | 1 | 209.00 | 209.00 | Robocraze - https://robocraze.com/products/tcrt5000l-5-channel-infrared-line-tracking-sensor-module | 5 TCRT5000 sensors, 15 mm pitch, supply 3.3-5 V, 5 outputs, mounting holes | One board holds all 5 sensors at fixed, equal spacing, matching the weights [-2..2]. Run it from 3.3 V so outputs stay within the ESP32 ADC range. |
| 5 | Orange 11.1V 1300mAh 40C 3S LiPo (Robu SKU 43002) | 1 | 1,399.00 | 1,399.00 | Robu - search SKU 43002 at https://robu.in/product-category/batteries/battery/lithium-polymer-li-po | 11.1 V (12.6 V full), 1300 mAh, 40C continuous, 74x32x22 mm, XT60 + JST-XH balance | Appendix A specifies 3S. 40C x 1.3 Ah = 52 A capability, far above our ~2 A worst case. One pack runs the whole robot. |
| 6 | iMax B3 LiPo Battery Charger (US plug) | 1 | 335.12 | 335.12 | Zbotic - https://zbotic.in/product/imax-b3-lipo-battery-charger/ | 2S/3S balance charger, JST-XH balance ports, mains input | A LiPo needs a balance charger. Skip this if your club or hostel already has one. |
| 7 | Mini MP1584 DC-DC 3A Adjustable Buck Module (SKU AI5952) | 1 | 52.39 | 52.39 | Zbotic - https://zbotic.in/product/mini-mp1584-dc-dc-3a-adjustable-buck-module/ | Input 4.5-28 V, output 0.8-20 V adjustable, 3 A max | The TB6612 module has no 5 V output, so the ESP32 needs its own 5 V supply. A buck is efficient; a linear regulator would burn (11.1 - 5) V x I as heat. Set to 5.0 V with a multimeter BEFORE connecting the ESP32. |
| 8 | 34mm Mini Car N20 Motor Rubber Small Wheel (SKU AI2376) | 2 | 42.95 | 85.90 | Zbotic - https://zbotic.in/product/34mm-mini-car-n20-motor-rubber-small-wheel/ | 34 mm rubber tyre, D-shaft | One per motor. Small diameter keeps the chassis low and the array close to the line. |
| 9 | N20 Motor - Mounting Bracket (2 brackets + 4 nut/bolts) | 1 pack | 21.71 | 21.71 | Zbotic - https://zbotic.in/product/mounting-bracket-for-n20-micro-gear-motors-2pcs/ | ABS, fits N20 motors | One bracket per motor. |
| 10 | Metal Ball Caster Wheel for Small Robotic Car (SKU AI2870) | 1 | 47.55 | 47.55 | Zbotic - https://zbotic.in/product/metal-ball-caster-wheel-for-small-robotic-car/ | Steel ball, 49x32x21 mm, 38 mm hole pitch | Third support point so the two-wheel robot doesn't tip. |
| | **Subtotal A** | | | **3,737.77** | | | |

## B. Estimated components (price at checkout)

| # | Component | Qty | Est. unit (Rs) | Est. total (Rs) | Why this part, why this quantity |
|---|---|---|---|---|---|
| 11 | Chassis base plate, ~120 x 100 mm, 3 mm acrylic (laser-cut) or 3D-printed PETG, with motor-bracket, caster, driver and array mounting holes | 1 | 300 | 300 | No ready-made N20 line-follower chassis was found, so a custom plate is the plan. Zbotic lists a 3D-print service. One rigid plate carries everything. |
| 12 | XT60 female connector with leads | 1 | 40 | 40 | Mates with the battery's XT60 plug. |
| 13 | SPST rocker/toggle switch | 1 | 20 | 20 | One master power switch on the battery positive. |
| 14 | 470 uF, 25 V electrolytic capacitor | 1 | 10 | 10 | Bulk cap on the motor supply (VM) to absorb spikes. 25 V rating because the pack reaches 12.6 V. |
| 15 | 0.1 uF ceramic capacitor | 2 | 5 | 10 | One across each motor's terminals to reduce brush noise. |
| 16 | Silicone wire 20-22 AWG, red + black | 2 m each | 100 | 100 | Power and ground runs. |
| 17 | Jumper wires (F-F and M-F), 40-pack | 1 | 100 | 100 | Signal connections. |
| 18 | Perfboard (~5x7 cm) + two 15-pin female header strips | 1 + 2 | 60 | 60 | Socket for the ESP32 and a shared power/ground bus. |
| 19 | M3 screw / nut / standoff kit | 1 | 100 | 100 | Mounts boards to the chassis. |
| | **Subtotal B** | | | **~740** | |

Optional (not counted): LiPo low-voltage buzzer alarm, est. Rs 100, protects against over-discharge.

## C. Budget

| Item | Rs |
|---|---|
| Subtotal A (priced) | 3,737.77 |
| Subtotal B (estimated) | ~740 |
| Shipping (est.; several Zbotic pages state free shipping, Robu is free above Rs 499, Robocraze unverified) | ~300 |
| Contingency / spares (spare TB6612 147.50, ESP32 410.64, MP1584 52.39, sensor array 209.00 = 819.53, rounded up for price changes) | 1,000 |
| **Total** | **~5,780** (about 5,880 with the LiPo alarm) |

Headroom of about Rs 4,200 against the Rs 10,000 limit (about Rs 4,100 with the optional alarm).

## D. Things to verify before ordering
1. **TB6612 module voltage rating.** One Indian listing titles the module "4.5V-10V". The IC itself allows 4.5-13.5 V, but the module's onboard filter capacitors may be rated only 10 V. A full 3S pack is 12.6 V. Check the capacitor markings on arrival, and replace them with 25 V parts if needed.
2. **Array outputs are analog.** The product descriptions mention both analog and digital outputs. Measure an output with a multimeter on arrival; analog values should vary smoothly with surface reflectance.
3. **Motor stall current.** The 300 RPM listing doesn't give it. The TB6612 is rated 1.2 A continuous per channel, so the stall current should be at most that.
4. **Stock.** Some N20 encoder variants showed as out of stock on category pages, and the Orange 1300 mAh 30C pack showed a 25-30 day restock. Fallbacks: Robu N20 12V 200RPM with encoder (SKU 475779); the 40C LiPo above is listed in stock.
5. **Bracket fit.** The bracket listing says it is for the "HM N20" series; check that it fits the GA12-N20 motor.

## E. Alternative prices seen (for the later supplier comparison)
TB6612 module: Probots Rs 209, Zbotic Rs 147.50. iMax B3 Pro charger: Probots Rs 499 (EU plug), Zbotic B3 Rs 335.12. 5-channel array: Probots Rs 189 (listed as digital I/O), Robocraze Rs 209. MP1584 buck: Probots Rs 79, Zbotic Rs 52.39.

## F. Why the TB6612FNG instead of the L298N (major component choice)

Appendix A names an L298N, but the sheet is labelled "mock", and for this robot the TB6612FNG is the better driver.

| Factor | L298N module | TB6612FNG module (chosen) | Effect on this robot |
|---|---|---|---|
| Output stage | Bipolar (Darlington) | MOSFET | The L298N wastes power as heat |
| Voltage lost inside the driver | about 1.8-3.2 V | about 0.5 V at 1 A | From a nominal 11.1 V pack the motors get about 8-9 V (L298N) vs about 10.6 V (TB6612) |
| Logic level | Designed for 5 V logic | Works at 2.7-5.5 V | Takes the ESP32's 3.3 V pins directly, no level shifter |
| Current per channel | 2 A | 1.0 A recommended (1.2 A absolute maximum, 3.2 A peak) | Enough for N20 motors (stall about 1 A), not for large motors |
| Motor voltage | up to 46 V | 4.5-13.5 V recommended (15 V absolute maximum) | A 3S pack (12.6 V full) fits, with a small margin; 4S would not |
| Size and weight | Large board with heatsink | A few cm, very light | Less mass to turn and stop in a maze |
| Extras | Onboard 5 V regulator | STBY pin (hardware safe stop) | The L298N regulator is not needed because a separate buck feeds the ESP32 |
| Price | not priced here (typically cheaper) | Rs 147.50 | Similar cost |

**Where the L298N would be the better choice:** high-current motors (more than about 1 A per channel) or a higher supply voltage.

**Compatibility with Appendix A:** the TB6612 signals map one-to-one (ENA to PWMA, IN1/IN2 to AIN1/AIN2, ENB to PWMB, IN3/IN4 to BIN1/BIN2, plus STBY tied to 3V3), so the same firmware pin constants run on either driver.
