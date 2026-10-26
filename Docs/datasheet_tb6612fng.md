# Task 2 - Datasheet literacy: TB6612FNG dual motor driver

**Component:** Toshiba TB6612FNG, used on the Zbotic "TB6612FNG Dual Motor Driver Module 1A" breakout (BOM item 2).
**Sources:** Toshiba TB6612FNG datasheet (absolute maximum ratings, operating range and electrical characteristics tables) and the Toshiba application note; supplier page for the module (filter capacitors on both supply lines, no polarity protection).

Values below are quoted from the datasheet tables. Check them against the current revision before the interview, since revisions differ slightly (for example, the lower VM limit is listed as 2.5 V or 4.5 V depending on the revision).

## 1. Three parameters that constrain the design

| Parameter (datasheet) | Value | How it shaped the BOM / wiring |
|---|---|---|
| **Motor supply VM** | Operating 4.5-13.5 V (down to 2.5 V in some revisions); absolute maximum 15 V | A fully charged 3S LiPo is 12.6 V: inside the range, but only 0.9 V under the 13.5 V limit and 2.4 V under the absolute maximum. So the BOM uses **3S, never 4S** (16.8 V would destroy it), a balance charger set to 3S, and a 470 uF capacitor rated **25 V** (not 16 V) at the VM pins. The driver also loses about 0.5 V (output saturation voltage, Vsat 0.5 V typ / 0.7 V max at 1 A), so motors see about 10.6 V from a nominal 11.1 V pack. That is why **12 V** N20 motors were chosen, running at ~88% of rating. |
| **Output current per channel** | 1.2 A average (absolute maximum); operating-range table and application note: **1.0 A or less** for VM >= 4.5 V; peak 2 A (20 ms, duty <= 20%) and 3.2 A (10 ms single pulse) | Suppliers advertise "1.2 A continuous", but the datasheet's recommended operating current is 1.0 A. So the motors must have a **stall current of about 1 A or less**. A GA12-N20 12 V listing shows 1 A stall, which at ~10.6 V is about 0.9 A: close to the limit. Mitigations: one channel per motor (no paralleling needed), a software ramp on speed commands, and encoder-based stall detection (command high but no counts for ~200 ms means stop, or pull STBY low). The ramp and stall detection are planned, not yet in the code. |
| **Logic supply VCC and input thresholds** | VCC 2.7-5.5 V (absolute maximum 6 V); input high VIH >= 0.7 x VCC; input low VIL <= 0.3 x VCC; input pins absolute maximum 6 V | The ESP32 outputs 3.3 V. Powering VCC from the ESP32's 3V3 pin gives VIH min = 0.7 x 3.3 = 2.31 V, which a 3.3 V GPIO satisfies, so **no level shifter** is needed and VCC stays far below the 6 V maximum. STBY must be high for the outputs to work, so it is tied to 3V3 (or to a spare GPIO to allow a hardware safe stop). PWM may run up to 100 kHz, so any ESP32 PWM frequency is acceptable. |

## 2. One condition that would damage the part, and how the design avoids it

**Condition: motor supply above the 15 V absolute maximum, even briefly.** The datasheet states that absolute maximum ratings must not be exceeded "even for a moment". Ways this could happen:

1. A 4S pack (16.8 V) or the wrong charger/pack connected.
2. **Inductive and regenerative voltage spikes** on VM when motors reverse or brake, or when the battery lead disconnects while the motors are spinning.
3. **Reversed battery polarity.** The module has no polarity protection, according to the supplier page.

How the design avoids it:
- **Pack choice:** 3S only; maximum 12.6 V, leaving 2.4 V of margin to 15 V.
- **Keyed XT60 connector** on the battery, so it cannot be plugged in backwards.
- **470 uF, 25 V bulk capacitor at the VM pins** and **0.1 uF across each motor** to absorb spikes and brush noise. The IC's internal regenerative diodes (VF about 1 V at 1 A) clamp the switching spikes, so external flyback diodes are not required.
- **Switch on the battery positive** so the pack is disconnected only when the motors have been stopped in firmware first (stop, then switch off).
- **Verify the module's own filter capacitors:** one Indian listing titles the module "4.5V-10V", so confirm their voltage rating from the markings on arrival and replace them with 25 V parts if they are rated 10 V.

A second condition worth knowing: sustained current above about 1.2 A per channel causes heating, and the thermal shutdown (typically 175 degC, hysteresis 20 degC) stops the outputs. This is a safety net, not a design target.
