// test_core.cpp - validates lf_core.h against hard-coded sensor arrays (no robot needed)
//
// Build & run (from the repo root):
//   g++ -std=c++11 -Wall -Wextra -o test_core src/tests/test_core.cpp && ./test_core
//
// Exit code is 0 if every check passes, 1 otherwise.
#include <stdio.h>
#include <math.h>
#include "lf_core.h"

static int g_fail = 0, g_pass = 0;

#define CHECK(cond, msg)                                              \
  do {                                                                \
    if (cond) { g_pass++; }                                           \
    else { g_fail++; printf("  FAIL: %s  (line %d)\n", msg, __LINE__); } \
  } while (0)

static bool near(float a, float b, float eps = 1e-3f) { return fabsf(a - b) <= eps; }

static const char *stateName(lf::State s) {
  return s == lf::FOLLOWING ? "FOLLOW " : (s == lf::RECOVERING ? "RECOVER" : "STOPPED");
}

// ---------------------------------------------------------------- Task 1
static void test_line_position() {
  printf("[linePosition]\n");
  float pos = 123.0f;
  float zeros[5]  = {0, 0, 0, 0, 0};
  float left[5]   = {0, 600, 900, 0, 0};
  float half[5]   = {0, 100, 100, 0, 0};
  float right[5]  = {0, 0, 0, 0, 200};
  float faint[5]  = {10, 10, 10, 10, 10};   // total 50 < min_total 150 -> treated as lost
  float sym[5]    = {500, 0, 0, 0, 500};    // symmetric -> centroid 0

  CHECK(!lf::linePosition(zeros, 150.0f, pos), "all-zero must report 'not found'");
  CHECK(!lf::linePosition(zeros, 0.0f, pos),   "all-zero must not divide by zero even if min_total = 0");
  CHECK(pos == 123.0f, "pos must be untouched when line not found");
  CHECK(lf::linePosition(left, 150.0f, pos) && near(pos, -0.4f), "{0,600,900,0,0} -> -0.4");
  CHECK(lf::linePosition(half, 150.0f, pos) && near(pos, -0.5f), "{0,100,100,0,0} -> -0.5");
  CHECK(lf::linePosition(right, 150.0f, pos) && near(pos, 2.0f), "{0,0,0,0,200} -> 2.0");
  CHECK(!lf::linePosition(faint, 150.0f, pos), "faint noise below min_total -> lost");
  CHECK(lf::linePosition(sym, 150.0f, pos) && near(pos, 0.0f), "symmetric -> 0.0");
}

static void test_clamp() {
  printf("[clampInt]\n");
  CHECK(lf::clampInt(250, 200)  ==  200, "250 -> 200");
  CHECK(lf::clampInt(-300, 200) == -200, "-300 -> -200");
  CHECK(lf::clampInt(120, 200)  ==  120, "120 unchanged");
  CHECK(lf::clampInt(-50, 200)  ==  -50, "-50 unchanged");
  CHECK(lf::clampInt(250, -200) ==  200, "negative limit is treated as its magnitude");
  CHECK(lf::clampInt(0, 200)    ==    0, "0 unchanged");
}

// ---------------------------------------------------------------- calibration
static void test_normalise() {
  printf("[normalise]\n");
  CHECK(near(lf::normalise(2000, 500, 3500), 500.0f), "mid-range -> 500");
  CHECK(near(lf::normalise(100, 500, 3500), 0.0f),    "below min clamps to 0");
  CHECK(near(lf::normalise(4000, 500, 3500), 1000.0f),"above max clamps to 1000");
  CHECK(near(lf::normalise(2000, 3000, 3000), 0.0f),  "no span (max<=min) -> 0");
}

// ---------------------------------------------------------------- PID terms
static void test_pid() {
  printf("[Pid]\n");
  lf::Pid p; p.kp = 40; p.ki = 0; p.kd = 0; p.i_max_out = 30;
  CHECK(near(p.update(0.5f, 0.01f), 20.0f), "P only: 40 * 0.5 = 20");

  lf::Pid d; d.kp = 0; d.ki = 0; d.kd = 1.0f; d.i_max_out = 30;
  CHECK(near(d.update(0.0f, 0.01f), 0.0f),  "D: first sample has no derivative kick");
  CHECK(near(d.update(0.1f, 0.01f), 10.0f), "D: (0.1-0)/0.01 * 1.0 = 10");

  lf::Pid i; i.kp = 0; i.ki = 10.0f; i.kd = 0; i.i_max_out = 30;
  float out = 0;
  for (int k = 0; k < 100; k++) out = i.update(2.0f, 0.1f);  // constant error 2.0 for 10 s
  CHECK(near(out, 30.0f), "I: output capped at i_max_out (anti-windup)");
  i.reset();
  CHECK(near(i.update(0.0f, 0.01f), 0.0f), "reset() clears the integral");
}

// ---------------------------------------------------------------- full control step
static void test_controller() {
  printf("[LineFollower - steering]\n");
  lf::Config c;                                  // kp=40 kd=1.2 ki=0 base=150
  float centred[5]  = {0, 0, 1000, 0, 0};
  float slightR[5]  = {0, 0, 500, 500, 0};       // position +0.5
  float farLeft[5]  = {1000, 0, 0, 0, 0};        // position -2.0

  { lf::LineFollower r(c); lf::Output o = r.step(centred, 0.01f, 0);
    CHECK(o.left == 150 && o.right == 150, "centred -> both wheels at base speed"); }
  { lf::LineFollower r(c); lf::Output o = r.step(slightR, 0.01f, 0);
    CHECK(o.left == 170 && o.right == 130, "line right -> left faster, right slower"); }
  { lf::LineFollower r(c); lf::Output o = r.step(farLeft, 0.01f, 0);
    CHECK(o.left == 70 && o.right == 230, "line far left -> left slower, right faster"); }

  // Saturation: huge gain must be clamped, sign preserved (reverse allowed on inner wheel)
  { lf::Config big = c; big.kp = 200;
    lf::LineFollower r(big); float far_right[5] = {0, 0, 0, 0, 1000};
    lf::Output o = r.step(far_right, 0.01f, 0);
    CHECK(o.left == 255 && o.right == -250, "saturation: 550 -> 255, -250 stays -250"); }

  // D term damps: error shrinking from -0.50 to -0.45 -> correction smaller than P alone
  { lf::LineFollower r(c);
    float a[5] = {0, 500, 500, 0, 0};            // -0.50
    float b[5] = {0, 450, 550, 0, 0};            // -0.45
    r.step(a, 0.01f, 0);
    lf::Output o = r.step(b, 0.01f, 10);
    CHECK(near(o.correction, -12.0f, 0.01f), "P(-18) + D(+6) = -12 (less aggressive than P alone)"); }
}

// ---------------------------------------------------------------- lost line
static void test_lost_line() {
  printf("[LineFollower - lost line]\n");
  lf::Config c;                                  // recover_speed 120, timeout 600 ms
  float onRight[5] = {0, 0, 0, 500, 500};        // +1.5 -> last seen on the right
  float onLeft[5]  = {500, 500, 0, 0, 0};        // -1.5 -> last seen on the left
  float none[5]    = {0, 0, 0, 0, 0};

  { lf::LineFollower r(c); uint32_t t = 0;
    r.step(onRight, 0.01f, t);
    t += 10; lf::Output o = r.step(none, 0.01f, t);
    CHECK(o.state == lf::RECOVERING && o.left == 120 && o.right == -120,
          "lost after right side -> pivot right (left fwd, right back)");
    // still inside the timeout window
    t += 500; o = r.step(none, 0.01f, t);
    CHECK(o.state == lf::RECOVERING, "still recovering before the timeout");
    // timeout reached -> safe stop
    t += 100; o = r.step(none, 0.01f, t);
    CHECK(o.state == lf::STOPPED && o.left == 0 && o.right == 0, "timeout -> safe stop");
    // latched: seeing the line again does not restart it
    o = r.step(onRight, 0.01f, t + 10);
    CHECK(o.state == lf::STOPPED && o.left == 0 && o.right == 0, "STOPPED is latched until reset"); }

  { lf::LineFollower r(c); uint32_t t = 0;
    r.step(onLeft, 0.01f, t);
    t += 10; lf::Output o = r.step(none, 0.01f, t);
    CHECK(o.left == -120 && o.right == 120, "lost after left side -> pivot left"); }

  { lf::LineFollower r(c); uint32_t t = 0;
    r.step(onRight, 0.01f, t);
    t += 10; r.step(none, 0.01f, t);
    t += 100; lf::Output o = r.step(onRight, 0.01f, t);
    CHECK(o.state == lf::FOLLOWING && o.found, "line re-found inside the timeout -> back to FOLLOWING"); }
}

// ---------------------------------------------------------------- scripted demo
// Prints a telemetry table like the Serial stream the ESP32 produces.
static void demo_run() {
  printf("\n[scripted demo run]  (kp=40 ki=0 kd=1.2 base=150, 10 ms per step)\n");
  printf(" t_ms | sensors (0..1000)        |   pos |  corr |   L |    R | state\n");
  printf("------+--------------------------+-------+-------+-----+------+--------\n");

  // Positions chosen so the line moves gradually, like a real robot at ~100 Hz.
  const float seq[][5] = {
    {   0,   0, 1000,    0,    0},   // pos 0.0  centred
    {   0,   0,  900,  100,    0},   // pos 0.1  starts drifting right
    {   0,   0,  750,  250,    0},   // pos 0.25
    {   0,   0,  500,  500,    0},   // pos 0.5
    {   0,   0,  200,  800,    0},   // pos 0.8
    {   0,   0,    0,  900,  100},   // pos 1.1  curve to the right
    {   0,   0,    0,  600,  400},   // pos 1.4
    {   0,   0,    0,  300,  700},   // pos 1.7
    {   0,   0,    0,    0, 1000},   // pos 2.0  line at the edge of the array
    {   0,   0,    0,    0,    0},   // LOST (line left the array on the right)
    {   0,   0,    0,    0,    0},   // still lost -> pivoting right
    {   0,   0,    0,  400,  600},   // pos 1.6  re-found while pivoting
    {   0,   0,    0, 1000,    0},   // pos 1.0
    {   0,   0,  500,  500,    0},   // pos 0.5
    {   0,   0,  900,  100,    0},   // pos 0.1
    {   0,   0, 1000,    0,    0},   // pos 0.0  recentred
  };
  const int N = (int)(sizeof(seq) / sizeof(seq[0]));

  lf::LineFollower robot;                         // default Config
  uint32_t t = 0;
  for (int k = 0; k < N; k++) {
    lf::Output o = robot.step(seq[k], 0.01f, t);
    printf(" %4u | %4.0f %4.0f %4.0f %4.0f %4.0f | %5.2f | %5.1f | %3d | %4d | %s\n",
           (unsigned)t, seq[k][0], seq[k][1], seq[k][2], seq[k][3], seq[k][4],
           o.found ? o.position : 0.0f, o.correction, o.left, o.right, stateName(o.state));
    t += 10;
  }
}

int main() {
  test_line_position();
  test_clamp();
  test_normalise();
  test_pid();
  test_controller();
  test_lost_line();
  demo_run();
  printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
