# Task 6 - Electronics and hardware

Diagrams: `BOM/wiring_diagram.svg` (pin-level wiring) and `BOM/power_block_diagram.svg` (rails).

## 1. Supply voltages

| Rail | Voltage | Source | Feeds |
|---|---|---|---|
| Battery | 9.9-12.6 V (11.1 V nominal; alarm at about 9.9 V) | 3S LiPo via keyed XT60 and a switch | TB6612 VM (through a 470 uF / 25 V capacitor) and the buck input |
| Motor terminals | about battery voltage minus 0.5 V (about 10.6 V nominal) | TB6612 outputs, PWM-chopped | two 12 V N20 motors |
| 5 V | 5.0 V (set with a multimeter before connecting) | MP1584 buck | ESP32 Vin |
| 3.3 V | 3.3 V | ESP32 board regulator | IR array, encoders, TB6612 VCC and STBY |

No signal pin ever sees more than 3.3 V: the array and encoders are powered at 3.3 V, and the TB6612's logic supply is also 3.3 V.

## 2. Common-ground topology

Everything shares one reference, but the wiring is a **star**, not a daisy chain:

- The **star point is the battery negative**. The TB6612 GND connects to it with a short, thick wire, because it carries the full motor return current (up to about 2 A).
- The ESP32, buck, IR array and encoder grounds each run their own wire to the same star, so motor current never flows through a logic-ground wire.
- A shared, thin ground path would let motor current create a voltage drop (V = I x R) that shifts the ESP32's ground relative to the battery. That corrupts the ADC readings (the sensor values) and can reset the controller.

## 3. Role of the motor driver (TB6612FNG)

The ESP32's pins can supply only a few milliamps and are 3.3 V logic; the motors need about 1 A at about 11 V. The driver is the **power interface**: a pair of H-bridges that switch the battery voltage onto the motors under the control of low-power logic signals.

| AIN1 | AIN2 | PWMA | Motor A |
|---|---|---|---|
| H | L | PWM | forward, speed set by duty cycle |
| L | H | PWM | reverse |
| H | H | - | short brake |
| L | L | - | stop (coast) |

Channel B behaves the same with BIN1, BIN2 and PWMB. In the firmware, the sign of the motor command selects the IN1/IN2 levels and its magnitude sets the PWM duty. STBY must be high for the outputs to work; pulling it low gives a hardware safe stop. The driver also provides internal diodes for motor back-EMF, and thermal shutdown. It also isolates the ESP32 from the motor current and voltage spikes.

## 4. Troubleshooting analysis: the controller resets when the motors engage

**Symptom:** the ESP32 reboots (Serial restarts, telemetry stops) a moment after the motors start, or whenever a wheel is blocked.

### Probable root causes, most likely first

1. **Transient dip on the 5 V / 3.3 V rail.** Motor start-up and stall draw up to about 2 A in a few microseconds (see Task 5). Poor joints, long jumper wires or breadboard contacts add resistance and inductance, so the supply dips briefly. If the 3.3 V rail falls below the ESP32's brown-out threshold (roughly 2.4-2.8 V, depending on configuration), the chip resets. The board's regulator needs about 1 V of headroom, so the 5 V rail must stay above about 4.5 V.
2. **Shared ground impedance (ground bounce).** If motor return current shares a ground path with the ESP32, the logic ground moves with the motor current.
3. **Missing or too-small bulk capacitance** on the motor supply and near the ESP32, so nothing supplies the current step while the wiring and battery respond. For scale, a 470 uF capacitor supplies 2 A for 100 us with only about 0.4 V of droop (dV = I x dt / C).
4. **Brush noise and back-EMF spikes** (no 0.1 uF across the motors, or spikes on VM) coupling into the supply or the EN pin.
5. **Weak source or contact:** a low or ageing battery, a loose XT60 or switch contact, or a wrong buck setting.
6. **Firmware step change:** commanding 0 to full PWM instantly gives maximum inrush; a blocked wheel adds stall current.
7. **A software reset that merely coincides** with motor start (a crash or watchdog).

### First diagnostic check

**Read the reset reason in software**, before touching any hardware. Print `esp_reset_reason()` in `setup()` (the boot log also prints "Brownout detector was triggered"):

- `ESP_RST_BROWNOUT` means the supply dipped: continue with causes 1-5.
- `ESP_RST_PANIC` or a watchdog reset means a software problem (cause 7).
- `ESP_RST_POWERON` means the supply vanished completely (loose connector, switch, or the buck shutting down).

This costs one minute and tells you which half of the list to chase.

### Narrowing it down

| Test | Result | Meaning |
|---|---|---|
| Run the motors with wheels lifted, then with a wheel held (stall) | Resets only on stall or start-up | Current-related supply dip (causes 1-3, 5) |
| Power the ESP32 from USB, motors from the battery | Resets stop | The 5 V rail or ground coupling; the motor side is fine |
| Same test, resets continue | Noise coupling or software (causes 4, 7) | Check motor capacitors, EN pin, firmware |
| Scope or min-capture multimeter on ESP32 Vin and 3V3 during start | Dip below about 4.5 V on 5 V, or 3.1 V on 3.3 V | Confirms the supply dip |

### Fix

1. Add a 220-470 uF low-ESR capacitor on the 5 V rail right at the ESP32 Vin, and a 10-22 uF ceramic on the buck input.
2. Keep the 470 uF at the TB6612 VM pins and the 0.1 uF across each motor.
3. Shorten and thicken the power wiring (20 AWG), solder the motor power joints, and use the star ground from the diagram.
4. Add a soft-start ramp to the firmware (limit how fast the PWM command can increase).
5. Check the battery voltage and the XT60 and switch contacts.

### Confirming the fix

- The 5 V rail stays at or above about 4.5 V, and 3.3 V at or above about 3.1 V, in a captured trace during a worst-case start (both motors from 0 to full, then a 2-second stall).
- No reset across 20 consecutive start and stall cycles.
- The reset reason is never `BROWNOUT` during a 10-minute run on the track.
- The before and after captures are saved for comparison.
