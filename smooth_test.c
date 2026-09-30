// Host unit test for smooth.inc: cc smooth_test.c -o build/smooth_test && build/smooth_test
#include <math.h>
#include <stdio.h>
#include "smooth.inc"

static int fail;
static void check(const char *what, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fail = 1; }

// Runs frames at 60 fps and returns the time (ms) until the predicate holds.
static float run(Smooth *s, const SmoothCfg *c, float tx, float ty, int (*until)(float, float), float *x, float *y) {
    for (int f = 1; f <= 120; f++) { smoothStep(s, c, tx, ty, 16.667f, x, y); if (until(*x, *y)) return f * 16.667f; }
    return -1;
}
static int fullUp(float x, float y) { return y > 0.999f; }
static int stopped(float x, float y) { return fabsf(x) < 0.001f && fabsf(y) < 0.001f; }
static int fullRight(float x, float y) { return x > 0.999f; }
static int fullDown(float x, float y) { return y < -0.999f; }

int main(void) {
    SmoothCfg c = {45, 35, 110};
    Smooth s = {0}; float x, y, t;
    t = run(&s, &c, 0, 1, fullUp, &x, &y);
    printf("  start W: full speed after %.0f ms\n", t); check("starts within 60 ms", t > 0 && t <= 60);
    check("starts straight up (no sideways drift)", fabsf(x) < 0.001f);
    t = run(&s, &c, 1, 0, fullRight, &x, &y);
    printf("  W -> D (90 deg): facing right after %.0f ms\n", t); check("90 deg turn within 75 ms", t > 0 && t <= 75);
    float peakDip = 1; Smooth s2 = {0, 1, 1}; // check speed stays full while turning
    for (int f = 0; f < 6; f++) { smoothStep(&s2, &c, 1, 0, 16.667f, &x, &y); float m = sqrtf(x * x + y * y); if (m < peakDip) peakDip = m; }
    check("keeps full speed through the turn", peakDip > 0.99f);
    Smooth s3 = {0, 1, 1};
    t = run(&s3, &c, 0, -1, fullDown, &x, &y);
    printf("  W -> S (180 deg): facing down after %.0f ms\n", t); check("reversal within 130 ms", t > 0 && t <= 130);
    Smooth s4 = {0, 1, 1};
    t = run(&s4, &c, 0, 0, stopped, &x, &y);
    printf("  release: stopped after %.0f ms\n", t); check("stops within 3 frames (~50 ms)", t > 0 && t <= 51);
    Smooth s5 = {0, 1, 1};
    smoothStep(&s5, &c, 0.7071f, 0.7071f, 16.667f, &x, &y);
    smoothStep(&s5, &c, 0.7071f, 0.7071f, 16.667f, &x, &y);
    check("45 deg change done within 2 frames (~33 ms)", x > 0.70f && y > 0.70f);
    SmoothCfg off = {0, 0, 0}; Smooth s6 = {0};
    smoothStep(&s6, &off, 1, 0, 16.667f, &x, &y); check("0 settings = old instant behaviour", x > 0.999f);
    printf(fail ? "RESULT FAIL\n" : "RESULT PASS\n");
    return fail;
}
