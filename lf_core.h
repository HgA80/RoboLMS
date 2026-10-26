// lf_core.h - hardware-independent line-following logic (Tasks 1, 3 and 9)
//
// There are NO Arduino calls in this file, so the exact same code runs on the
// ESP32 (line_follower.ino) and on a PC (tests/test_core.cpp). That is how the
// logic is validated against hard-coded sensor arrays without a robot.
//
// Sign convention used everywhere:
//   position / error  > 0  -> line is to the RIGHT of the robot centre
//   position / error  < 0  -> line is to the LEFT
//   motor command     > 0  -> wheel forward,  < 0 -> wheel reverse
#pragma once
#include <stdint.h>

namespace lf {

static const int    NUM_SENSORS = 5;
static const int8_t WEIGHTS[NUM_SENSORS] = {-2, -1, 0, 1, 2};  // leftmost ... rightmost

// ---------------------------------------------------------------------------
// Tunable parameters (starting values - see docs/pid_control_and_tuning.md)
// ---------------------------------------------------------------------------
struct Config {
  float    kp, ki, kd;           // PID gains (output unit: PWM counts)
  float    i_max_out;            // max |Ki * integral| contribution (anti-windup)
  int      base_speed;           // forward PWM (0-255) when perfectly centred
  int      max_speed;            // |PWM| limit applied to each wheel
  float    min_total;            // sum of normalised readings below which the line counts as LOST
  int      recover_speed;        // PWM used while pivoting to re-find the line
  uint32_t recover_timeout_ms;   // give up and stop after this long without a line

  Config()
      : kp(40.0f), ki(0.0f), kd(1.2f), i_max_out(30.0f),
        base_speed(150), max_speed(255),
        min_total(150.0f), recover_speed(120), recover_timeout_ms(600) {}
};

// ---------------------------------------------------------------------------
// Task 1, part 2: clamp a signed command to [-maxMag, +maxMag], keeping its sign
// ---------------------------------------------------------------------------
inline int clampInt(int v, int maxMag) {
  if (maxMag < 0) maxMag = -maxMag;   // guard against a negative limit
  if (v > maxMag)  return  maxMag;
  if (v < -maxMag) return -maxMag;
  return v;
}

inline int roundToInt(float x) { return (int)(x >= 0.0f ? x + 0.5f : x - 0.5f); }

// ---------------------------------------------------------------------------
// Min-max calibration: map a raw 12-bit reading to 0..1000 (1000 = darkest/line)
// ---------------------------------------------------------------------------
inline float normalise(int raw, int mn, int mx) {
  if (mx <= mn) return 0.0f;                          // calibration failed / no span
  float v = (float)(raw - mn) * 1000.0f / (float)(mx - mn);
  if (v < 0.0f)    v = 0.0f;
  if (v > 1000.0f) v = 1000.0f;
  return v;
}

// ---------------------------------------------------------------------------
// Task 1, part 1: weighted centroid. Returns false if the line is NOT seen.
//   position = sum(w_i * s_i) / sum(s_i)       range -2 .. +2
// The all-zero case (sum == 0 -> divide by zero) is handled explicitly. A
// "too faint" total (< min_total) is also treated as lost, because real
// calibrated sensors are rarely exactly zero.
// ---------------------------------------------------------------------------
inline bool linePosition(const float s[NUM_SENSORS], float min_total, float &pos) {
  float num = 0.0f, den = 0.0f;
  for (int i = 0; i < NUM_SENSORS; i++) {
    num += (float)WEIGHTS[i] * s[i];
    den += s[i];
  }
  if (den <= 0.0f || den < min_total) return false;   // explicit lost-line case
  pos = num / den;
  return true;
}

// ---------------------------------------------------------------------------
// PID with anti-windup and no "derivative kick" on the first sample
// ---------------------------------------------------------------------------
struct Pid {
  float kp, ki, kd, i_max_out;
  float integral, last_error;
  bool  primed;

  Pid() : kp(0), ki(0), kd(0), i_max_out(0), integral(0), last_error(0), primed(false) {}

  void reset() { integral = 0.0f; last_error = 0.0f; primed = false; }

  float update(float error, float dt) {
    if (dt < 1e-4f) dt = 1e-4f;                        // avoid divide-by-zero
    // I term: accumulate, then clamp so |ki*integral| <= i_max_out
    integral += error * dt;
    if (ki > 0.0f) {
      float lim = i_max_out / ki;
      if (integral >  lim) integral =  lim;
      if (integral < -lim) integral = -lim;
    }
    // D term: rate of change of error (0 on the very first sample)
    float deriv = primed ? (error - last_error) / dt : 0.0f;
    last_error = error;
    primed = true;
    return kp * error + ki * integral + kd * deriv;
  }
};

// ---------------------------------------------------------------------------
// One control step: sensors in -> motor commands out
// ---------------------------------------------------------------------------
enum State : uint8_t { FOLLOWING = 0, RECOVERING = 1, STOPPED = 2 };

struct Output {
  int   left, right;     // signed motor commands, already clamped
  State state;
  bool  found;           // was the line seen this step?
  float position;        // centroid (valid if found)
  float error;           // = position (setpoint is 0)
  float correction;      // PID output
};

class LineFollower {
 public:
  explicit LineFollower(const Config &c = Config()) : cfg_(c), state_(FOLLOWING),
                                                      last_dir_(1), lost_since_(0) {
    pid_.kp = cfg_.kp; pid_.ki = cfg_.ki; pid_.kd = cfg_.kd; pid_.i_max_out = cfg_.i_max_out;
  }

  State state() const { return state_; }

  // s: calibrated readings 0..1000, higher = darker (line).
  // dt: seconds since the previous call.  now_ms: monotonic milliseconds.
  Output step(const float s[NUM_SENSORS], float dt, uint32_t now_ms) {
    Output o;
    o.left = 0; o.right = 0; o.found = false;
    o.position = 0; o.error = 0; o.correction = 0; o.state = state_;

    if (state_ == STOPPED) return o;                   // safe stop is latched until reset

    float pos = 0.0f;
    bool found = linePosition(s, cfg_.min_total, pos);
    o.found = found;

    if (found) {
      if (state_ == RECOVERING) {                      // line re-acquired
        state_ = FOLLOWING;
        pid_.reset();                                  // drop stale integral / derivative
      }
      float err  = pos;                                // setpoint = 0 (line under centre sensor)
      float corr = pid_.update(err, dt);

      // remember which side the line was last on (ignore tiny errors near centre)
      if (err >  0.05f) last_dir_ =  1;
      if (err < -0.05f) last_dir_ = -1;

      // differential steering: line on the right (err>0) -> left wheel faster
      int c = roundToInt(corr);
      o.left  = clampInt(cfg_.base_speed + c, cfg_.max_speed);
      o.right = clampInt(cfg_.base_speed - c, cfg_.max_speed);
      o.position = pos; o.error = err; o.correction = corr;
    } else {
      if (state_ == FOLLOWING) {                       // just lost it
        state_ = RECOVERING;
        lost_since_ = now_ms;
        pid_.reset();
      }
      if ((uint32_t)(now_ms - lost_since_) >= cfg_.recover_timeout_ms) {
        state_ = STOPPED;                              // give up: safe stop
      } else {
        // pivot in place toward the side where the line was last seen
        o.left  = clampInt( last_dir_ * cfg_.recover_speed, cfg_.max_speed);
        o.right = clampInt(-last_dir_ * cfg_.recover_speed, cfg_.max_speed);
      }
    }
    o.state = state_;
    return o;
  }

 private:
  Config   cfg_;
  Pid      pid_;
  State    state_;
  int      last_dir_;     // +1 = line last seen on right, -1 = on left
  uint32_t lost_since_;
};

}  // namespace lf
