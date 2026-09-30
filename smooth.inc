// Stick smoothing for keyboard movement. Pure math, no system calls, so it can
// be unit-tested on the host (see smooth_test.c).
//
// Keys give an 8-way target direction. Instead of snapping, the stick:
//  - ramps its magnitude up over AccelMs and down over DecelMs, and
//  - rotates its direction toward the target at 180 degrees per TurnMs
//    (a 45 degree change takes TurnMs/4), starting from the target when at rest.

typedef struct { float dx, dy, mag; } Smooth;
typedef struct { float accelMs, decelMs, turnMs; } SmoothCfg;

static float sqrtf_(float v) {
    if (v <= 0) return 0;
    float r = v > 1 ? v : 1; // Newton's method; plenty for stick values
    for (int i = 0; i < 20; i++) r = 0.5f * (r + v / r);
    return r;
}

// Advances the smoothed stick by dtMs toward target (tx, ty), where the target
// is a unit vector or (0, 0) for "no key held". Writes the stick in [-1, 1].
static void smoothStep(Smooth *s, const SmoothCfg *c, float tx, float ty, float dtMs, float *outX, float *outY) {
    int moving = tx != 0 || ty != 0;
    if (dtMs > 100) dtMs = 100; // after a hitch, don't jump
    if (moving) {
        if (s->mag <= 0.001f || (s->dx == 0 && s->dy == 0)) { s->dx = tx; s->dy = ty; }
        else {
            float maxTurn = c->turnMs > 0 ? 3.14159265f * dtMs / c->turnMs : 10; // radians this step
            float dot = s->dx * tx + s->dy * ty, cross = s->dx * ty - s->dy * tx;
            float cosT = 1 - maxTurn * maxTurn / 2 + maxTurn * maxTurn * maxTurn * maxTurn / 24;
            if (maxTurn >= 3.14159265f || dot >= cosT) { s->dx = tx; s->dy = ty; }
            else {
                float sinT = maxTurn - maxTurn * maxTurn * maxTurn / 6 + maxTurn * maxTurn * maxTurn * maxTurn * maxTurn / 120;
                if (cross < 0) sinT = -sinT; // turn the short way (left when exactly opposite)
                float nx = s->dx * cosT - s->dy * sinT, ny = s->dx * sinT + s->dy * cosT;
                float l = sqrtf_(nx * nx + ny * ny);
                s->dx = nx / l; s->dy = ny / l;
            }
        }
        s->mag += c->accelMs > 0 ? dtMs / c->accelMs : 1;
        if (s->mag > 1) s->mag = 1;
    } else {
        s->mag -= c->decelMs > 0 ? dtMs / c->decelMs : 1;
        if (s->mag < 0) s->mag = 0;
    }
    *outX = s->dx * s->mag; *outY = s->dy * s->mag;
}
