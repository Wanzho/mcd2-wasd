// Host unit test for dodge.inc: cc dodge_test.c -o build/dodge_test && build/dodge_test
#include <stdio.h>
#include "dodge.inc"

static int fail;
static void check(const char *what, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fail = 1; }
static int near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

int main(void) {
    DodgeCfg c = {40, 80};
    float x, y;

    // Click without dragging: dodge toward travel (right) on release.
    DodgeState d = {0};
    dodgeStep(&d, &c, 1, 1000, 1, 0, 1, 500, 500, &x, &y);
    check("no flick while the button is down", x == 0 && y == 0);
    dodgeStep(&d, &c, 1, 1400, 1, 0, 1, 510, 495, &x, &y); // small wobble, long hold
    dodgeStep(&d, &c, 0, 1410, 1, 0, 1, 510, 495, &x, &y);
    check("no drag (even after a long hold) = dodge toward travel", x == 1 && y == 0);
    dodgeStep(&d, &c, 0, 1485, 1, 0, 1, 510, 495, &x, &y);
    check("flick held for 80 ms", x == 1);
    dodgeStep(&d, &c, 0, 1495, 1, 0, 1, 510, 495, &x, &y);
    check("flick ends after 80 ms", x == 0 && y == 0);

    // Fast flick upward: press, 60 px up within one poll, release immediately.
    DodgeState f = {0};
    dodgeStep(&f, &c, 1, 2000, 1, 0, 1, 500, 500, &x, &y);
    dodgeStep(&f, &c, 1, 2016, 1, 0, 1, 500, 440, &x, &y);
    dodgeStep(&f, &c, 0, 2020, 1, 0, 1, 500, 440, &x, &y);
    check("fast drag up = roll up, whatever the timing", near(x, 0) && near(y, 1));

    // Drag detected on the release frame itself.
    DodgeState r = {0};
    dodgeStep(&r, &c, 1, 3000, 1, 0, 1, 500, 500, &x, &y);
    dodgeStep(&r, &c, 0, 3010, 1, 0, 1, 450, 500, &x, &y);
    check("drag seen only at release still counts (roll left)", near(x, -1) && near(y, 0));

    // Drag out diagonally, drift back inside 40 px before release: keeps the diagonal.
    DodgeState b = {0};
    dodgeStep(&b, &c, 1, 4000, 1, 0, 1, 500, 500, &x, &y);
    dodgeStep(&b, &c, 1, 4016, 1, 0, 1, 540, 460, &x, &y);
    dodgeStep(&b, &c, 1, 4032, 1, 0, 1, 510, 490, &x, &y);
    dodgeStep(&b, &c, 0, 4040, 1, 0, 1, 510, 490, &x, &y);
    check("last direction beyond 40 px is kept (up-right)", near(x, 0.7071f) && near(y, 0.7071f));

    // Just under the threshold does not count as a drag.
    DodgeState u = {0};
    dodgeStep(&u, &c, 1, 5000, -1, 0, 1, 500, 500, &x, &y);
    dodgeStep(&u, &c, 0, 5010, -1, 0, 1, 539, 500, &x, &y);
    check("39 px is not a drag (dodge toward travel)", x == -1 && y == 0);

    // No cursor information: always toward travel; never a zero direction.
    DodgeState n = {0};
    dodgeStep(&n, &c, 1, 6000, 0, 0, 0, 0, 0, &x, &y);
    dodgeStep(&n, &c, 0, 6010, 0, 0, 0, 0, 0, &x, &y);
    check("no cursor and no travel = roll forward, not nowhere", x == 0 && y == 1);

    printf(fail ? "RESULT FAIL\n" : "RESULT PASS\n");
    return fail;
}
