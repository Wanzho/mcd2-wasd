// Mouse dodge. Press and release without dragging = dodge in the direction of
// travel. Press, drag at least DragPx in any direction (at any speed), release =
// roll in the drag direction, measured from where the button went down. If the
// drag comes back inside DragPx before release, the last direction beyond it is
// kept. The game dodges on a right-stick flick, so the result is a flick held
// for FlickMs. Pure logic (no system calls) so it can be unit-tested on the host
// (see dodge_test.c).

typedef struct {
    int down, dragged;
    long ox, oy;                  // cursor where the button went down (screen coords)
    float gx, gy;                 // drag direction once beyond DragPx
    unsigned long long flickUntil;
    float fx, fy;                 // flick direction
} DodgeState;

typedef struct { unsigned dragPx, flickMs; } DodgeCfg;

static float dodgeSqrt(float v) { if (v <= 0) return 0; float r = v > 1 ? v : 1; for (int i = 0; i < 20; i++) r = 0.5f * (r + v / r); return r; }

// held: button state now. moveX/Y: current (or last) direction of travel, stick
// space (y up). cursorOk/cx/cy: mouse position in screen coords (y down).
// Writes the right stick to *sx/*sy (0 when not flicking).
static void dodgeStep(DodgeState *d, const DodgeCfg *c, int held, unsigned long long now,
                      float moveX, float moveY, int cursorOk, long cx, long cy, float *sx, float *sy) {
    if (held && !d->down) { d->down = 1; d->dragged = 0; d->ox = cx; d->oy = cy; if (!cursorOk) d->ox = d->oy = 0x7fffffff; }
    if (d->down && cursorOk && d->ox != 0x7fffffff) {
        float dx = (float)(cx - d->ox), dy = (float)(d->oy - cy); // screen y down -> stick y up
        float l = dodgeSqrt(dx * dx + dy * dy);
        if (l >= (float)c->dragPx && l > 0) { d->dragged = 1; d->gx = dx / l; d->gy = dy / l; }
    }
    if (!held && d->down) {
        d->down = 0;
        if (d->dragged) { d->fx = d->gx; d->fy = d->gy; }
        else { d->fx = moveX; d->fy = moveY; }
        if (d->fx == 0 && d->fy == 0) d->fy = 1;
        d->flickUntil = now + c->flickMs;
    }
    if (now < d->flickUntil) { *sx = d->fx; *sy = d->fy; }
    else { *sx = 0; *sy = 0; }
}
