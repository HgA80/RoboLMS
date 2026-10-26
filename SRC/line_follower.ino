// line_follower.ino - ESP32-WROOM-32 PID line follower (Tasks 3 and 9)
//
// Hardware: Appendix A pin map. All control maths lives in lf_core.h (also unit-tested
// on a PC); this file only does hardware I/O: ADC reads, calibration, motor pins, Serial.
//
// Requires Arduino-ESP32 core 3.x (analogWrite() is built in from 3.0).
// Motor driver: written for the Appendix L298N signals (ENA/IN1/IN2, ENB/IN3/IN4). The same
// signals map 1:1 onto a TB6612FNG (PWMA/AIN1/AIN2, PWMB/BIN1/BIN2; tie STBY to 3V3).
#include "lf_core.h"

// ------------------------------- Pin map (Appendix A) -------------------------------
// Sensors, leftmost -> rightmost, weights [-2 -1 0 1 2]
static const int IR_PINS[lf::NUM_SENSORS] = {34, 35, 32, 33, 25};

static const int ENA = 14, IN1 = 27, IN2 = 26;   // left  motor: PWM, dir A, dir B
static const int ENB = 13, IN3 = 12, IN4 = 11;   // right motor: PWM, dir A, dir B
// NOTE (real-hardware caveats, see README): on a genuine WROOM-32, GPIO6-11 are wired to the
// SPI flash (GPIO11 / IN4 will NOT work), GPIO12 is a boot-strapping pin (keep low at reset),
// and GPIO25 is an ADC2 channel (conflicts with Wi-Fi). Pins are constants so they are easy to remap.

// ------------------------------- Tuning / timing ------------------------------------
static const uint32_t CAL_MS    = 3000;     // calibration window: sweep sensors over line + background
static const int      MIN_SPAN  = 400;      // a sensor whose (max-min) is below this failed calibration
static const uint32_t LOOP_US   = 10000;    // 10 ms -> 100 Hz control loop (also keeps Serial within 115200 baud)

// ------------------------------- State ----------------------------------------------
static int  cal_min[lf::NUM_SENSORS];
static int  cal_max[lf::NUM_SENSORS];
static bool cal_ok = false;

static lf::LineFollower robot;              // default lf::Config (edit gains in lf_core.h)
static uint32_t last_us = 0;

// ------------------------------- Motor helper ---------------------------------------
// speed > 0 forward, < 0 reverse, 0 stop (coast). |speed| is the PWM duty (0-255).
// If a wheel spins the wrong way, swap that motor's two IN pins (or its two wires).
static void setMotor(int en, int inA, int inB, int speed) {
  if (speed > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW);  }
  else if (speed < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else                { digitalWrite(inA, LOW);  digitalWrite(inB, LOW);  }
  analogWrite(en, abs(speed));
}

static void driveMotors(int left, int right) {
  setMotor(ENA, IN1, IN2, left);
  setMotor(ENB, IN3, IN4, right);
}

// ------------------------------- Calibration ----------------------------------------
// Min-max calibration: while this runs, move the robot by hand so every sensor sees both
// the black line and the white surface. Higher voltage = darker (line), per Appendix A.
static void calibrate() {
  for (int i = 0; i < lf::NUM_SENSORS; i++) { cal_min[i] = 4095; cal_max[i] = 0; }

  Serial.println("Calibrating: sweep all sensors across the line and the background...");
  uint32_t start = millis();
  while (millis() - start < CAL_MS) {
    for (int i = 0; i < lf::NUM_SENSORS; i++) {
      int v = analogRead(IR_PINS[i]);
      if (v < cal_min[i]) cal_min[i] = v;
      if (v > cal_max[i]) cal_max[i] = v;
    }
    delay(2);
  }

  cal_ok = true;
  for (int i = 0; i < lf::NUM_SENSORS; i++) {
    Serial.printf("  S%d  min=%d  max=%d\n", i + 1, cal_min[i], cal_max[i]);
    if (cal_max[i] - cal_min[i] < MIN_SPAN) {
      Serial.printf("  WARNING: S%d span too small - check wiring/height and recalibrate\n", i + 1);
      cal_ok = false;                              // refuse to drive on bad calibration
    }
  }
}

// Read all five sensors and convert to calibrated 0..1000 values
static void readSensors(float s[lf::NUM_SENSORS]) {
  for (int i = 0; i < lf::NUM_SENSORS; i++) {
    s[i] = lf::normalise(analogRead(IR_PINS[i]), cal_min[i], cal_max[i]);
  }
}

// ------------------------------- Arduino entry points -------------------------------
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);                        // 0..4095

  const int outs[] = {ENA, IN1, IN2, ENB, IN3, IN4};
  for (int i = 0; i < 6; i++) { pinMode(outs[i], OUTPUT); digitalWrite(outs[i], LOW); }
  for (int i = 0; i < lf::NUM_SENSORS; i++) pinMode(IR_PINS[i], INPUT);

  driveMotors(0, 0);                               // motors off during calibration
  calibrate();
  Serial.println("ms,s1,s2,s3,s4,s5,pos,error,corr,left,right,state");   // telemetry header
  last_us = micros();
}

void loop() {
  uint32_t now_us = micros();
  if ((uint32_t)(now_us - last_us) < LOOP_US) return;       // fixed-rate loop (dt is stable for the D term)
  float dt = (float)(uint32_t)(now_us - last_us) * 1e-6f;
  last_us = now_us;

  if (!cal_ok) { driveMotors(0, 0); return; }               // never drive on a failed calibration

  float s[lf::NUM_SENSORS];
  readSensors(s);

  lf::Output o = robot.step(s, dt, millis());               // centroid -> PID -> differential speeds
  driveMotors(o.left, o.right);

  // Telemetry every iteration: sensors, computed error, motor commands (CSV)
  Serial.printf("%lu,%d,%d,%d,%d,%d,%.3f,%.3f,%.1f,%d,%d,%d\n",
                (unsigned long)millis(),
                (int)s[0], (int)s[1], (int)s[2], (int)s[3], (int)s[4],
                o.position, o.error, o.correction, o.left, o.right, (int)o.state);
}
