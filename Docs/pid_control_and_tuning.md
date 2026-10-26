# PID control for line following (Task 3) and tuning notes (Task 9)

Code: `src/line_follower/lf_core.h` (logic), `line_follower.ino` (ESP32 I/O), `src/tests/test_core.cpp` (PC tests).

## 1. From sensors to motor commands

1. **Calibrate** (min-max, at startup): each sensor's raw 12-bit reading is mapped to 0-1000, where 1000 is the darkest surface (the line), per Appendix A.
2. **Centroid:** `position = sum(w_i * s_i) / sum(s_i)` with weights `[-2, -1, 0, 1, 2]`. Result is in `[-2, +2]`; 0 means the line is under the centre sensor, positive means it is to the right.
3. **Error:** the setpoint is 0 (line centred), so `error = position`.
4. **PID output:** `correction = Kp*error + Ki*integral(error) + Kd*d(error)/dt`.
5. **Differential steering:** `left = base + correction`, `right = base - correction`. A line on the right (error > 0) makes the left wheel faster, so the robot curves right. Both commands are clamped to `[-255, 255]` with the sign preserved; the inner wheel may reverse for tight turns.

## 2. Role of each term

| Term | What it does | On this robot |
|---|---|---|
| **P** | Steers in proportion to how far off the line is. | Main driver. Too high -> oscillation; too low -> slow, cuts corners or loses the line. |
| **D** | Reacts to how fast the error is changing; counter-steers early. | Damps wobble and overshoot, helps on turns. |
| **I** | Accumulates past error to remove a constant bias. | Small or zero. Slow, so it can wind up and add oscillation. |

**Chosen configuration: PD, with a very small optional Ki.** The code implements all three terms (with anti-windup: `|Ki * integral|` is capped at 30 PWM counts, and the state is reset on lost-line), but `Ki` starts at 0. It is raised only if the robot consistently rides to one side of the line.

### Steady-state error with P alone
A pure P controller needs a non-zero error to produce any steady correction (`correction = Kp * error`), so the robot rides off-centre whenever something demands a constant turn:
- **Motor mismatch:** if one motor is slightly weaker and needs a constant `b` PWM counts of trim, the steady offset is `e_ss = b / Kp`. With `Kp = 40` and `b = 8`, the robot runs `0.2` sensor spacings off the line.
- **A constant-radius curve:** the wheel-speed difference needed to hold the curve must come from a persistent error, again `e_ss = required_correction / Kp`.

Raising Kp shrinks this offset but cannot remove it (and invites oscillation). A small Ki removes it by integrating the persistent error until the correction cancels the bias. On a straight line with matched motors, the steady-state error is already zero.

### Behaviour while turning
- On a bend the line slides toward an outer sensor, so `|error|` grows to as much as 2. P produces a large differential: the outer wheel speeds up, the inner wheel slows or reverses.
- D sees the error changing quickly at the start of the bend and adds extra push in the same direction, which speeds up the entry. As the robot swings back toward the line the error shrinks, D turns negative and eases the steering, which prevents overshooting the centre.
- I is the risk during a turn: a large sustained error would wind up the integral and cause overshoot afterwards. That is why Ki is small, the integral is clamped, and it is reset when the line is lost or re-acquired.
- The correction is limited by the array: error saturates at +/-2, so the maximum differential is `Kp * 2 = 80`. Turns sharper than that are handled by the lost-line recovery below.

## 3. Oscillation and lost-line handling

**If the robot oscillates**, adjust in this order: (1) lower Kp, (2) raise Kd, (3) lower the base speed, (4) make sure Ki is 0.

**Lost line** (sum of calibrated readings below 150 of a possible 5000, which includes the exact all-zero case from Task 1):
1. Remember which side the line was last seen on (`last_dir`, ignoring tiny errors near centre).
2. Pivot in place toward that side at 120 PWM (one wheel forward, the other back), since the line most likely left the array on that side.
3. If the line is found again, resume PID with the integral and derivative state reset (no derivative kick).
4. If it is not found within 600 ms, stop both motors. The stop is latched until reset, which is the safe default. A 180 degree turn is deliberately *not* used here: it is the right move for a true dead end in the maze (Task 8), not for an ordinary lost line.

## 4. Tuning choices

| Parameter | Start value | Reason |
|---|---|---|
| `base_speed` | 150 / 255 | About 60% duty leaves headroom for the faster outer wheel; raise it only after the gains are stable. |
| `Kp` | 40 | At error 2 the correction is 80, so wheels go to about 230 and 70: strong steering without immediately saturating. |
| `Kd` | 1.2 | Units are PWM per (error/second). About 30 ms of "lead" relative to Kp. Realistic error rates of a few units/s give a 5-10 count damping term. |
| `Ki` | 0 (max 30 counts) | See Section 2. Add a small value only for a persistent one-sided offset. |
| Loop rate | 100 Hz, fixed | A stable `dt` keeps the D term meaningful, and 100 lines/s of telemetry fits within 115200 baud. |
| Lost threshold / timeout / pivot speed | 150 / 600 ms / 120 | Above sensor noise, short enough to stop before the robot drives far off course, slow enough to re-find the line without overshooting it. |

**Tuning procedure on the robot:** set Ki = Kd = 0 and a low base speed; raise Kp until the robot follows with a slight wobble; raise Kd until the wobble disappears; then raise the base speed and repeat. Use the CSV telemetry (error and motor commands per step) to see oscillation directly.

## 5. Validation and limitations
- `test_core.cpp` runs 34 checks on hard-coded sensor arrays: the all-zero case, centroid values, clamping, PID terms and anti-windup, steering direction, saturation, and the full lost-line sequence. It also prints a scripted telemetry table.
- **Not validated on hardware.** Gains are starting values, not tuned values, and the `.ino` was not compiled against a board here.
- `analogWrite()` needs Arduino-ESP32 core 3.x.
- Appendix A pin caveats on a real WROOM-32: GPIO11 is a flash pin, GPIO12 is a boot-strapping pin, and GPIO25 is an ADC2 channel (conflicts with Wi-Fi). Pins are constants at the top of the sketch for easy remapping.
