/* Microcontroller graphic demo by Pascal Piazzalunga
 * admin@serveurperso.com http://www.serveurperso.com
 * https://www.youtube.com/watch?v=wyuJ-dqS2to
 * Ported to stm32/ChibiOS/glx by Chris Baird.
 * It's spinnin' bubbles, man.
 * --
 * Licencing? Ask Pascal. Let's assume it is freely-distributable and
 * modifiable, provided his name is kept in the source.
 * --
 * Chris Baird,, <cjb@brushtail.apana.org.au> April 2013
 *
 * Modified Andrew Hannam (inmarket) 2013-04-29 New GFX calls
 */

#include <math.h>
#include "gfx.h"


/* ---------------------------------------------------------------------- */

#define N 1024			/* Number of dots */
#define SCALE 8192
#define INCREMENT 512		/* INCREMENT = SCALE / sqrt(N) * 2 */
#define PI2 6.283185307179586476925286766559

#define background RGB2COLOR(0,0,0)

gU16 width, height;
gI16 sine[SCALE + (SCALE / 4)];
gI16 *cosi = &sine[SCALE / 4]; /* cos(x) = sin(x+90d)... */


void initialize(void)
{
    gU16 i;

    /* if you change the SCALE*1.25 back to SCALE, the program will
     * occassionally overrun the cosi array -- however this actually
     * produces some interesting effects as the BUBBLES LOSE CONTROL!!!!
     */
    for (i = 0; i < SCALE + (SCALE / 4); i++)
        //sine[i] = (-SCALE/2) + (int)(sinf(PI2 * i / SCALE) * sinf(PI2 * i / SCALE) * SCALE);
    {
        sine[i] = (int)(sinf(PI2 * i / SCALE) * SCALE);
    }
}


void matrix(gI16 xyz[3][N], gColor col[N])
{
    static gU32 t = 0;
    gI16 x = -SCALE, y = -SCALE;
    gU16 i, s, d;
    gU8 red, grn, blu;

#define RED_COLORS (32)
#define GREEN_COLORS (64)
#define BLUE_COLORS (32)

    for (i = 0; i < N; i++) {
        xyz[0][i] = x;
        xyz[1][i] = y;

        d = sqrt(x * x + y * y);	/* originally a fastsqrt() call */
        s = sine[(t * 30) % SCALE] + SCALE;

        xyz[2][i] = sine[(d + s) % SCALE] * sine[(t * 10) % SCALE] / SCALE / 2;

        red = (cosi[xyz[2][i] + SCALE / 2] + SCALE) *
              (RED_COLORS - 1) / SCALE / 2;
        grn = (cosi[(xyz[2][i] + SCALE / 2 + 2 * SCALE / 3) % SCALE] + SCALE) *
              (GREEN_COLORS - 1) / SCALE / 2;
        blu = (cosi[(xyz[2][i] + SCALE / 2 + SCALE / 3) % SCALE] + SCALE) *
              (BLUE_COLORS - 1) / SCALE / 2;
        col[i] = ((red << 11) + (grn << 5) + blu);

        x += INCREMENT;

        if (x >= SCALE) {
            x = -SCALE, y += INCREMENT;
        }
    }
    t++;
}


void rotate(gI16 xyz[3][N], gU16 angleX, gU16 angleY, gU16 angleZ)
{
    gU16 i;
    gI16 tmpX, tmpY;
    gI16 sinx = sine[angleX], cosx = cosi[angleX];
    gI16 siny = sine[angleY], cosy = cosi[angleY];
    gI16 sinz = sine[angleZ], cosz = cosi[angleZ];

    for (i = 0; i < N; i++) {
        tmpX      = (xyz[0][i] * cosx - xyz[2][i] * sinx) / SCALE;
        xyz[2][i] = (xyz[0][i] * sinx + xyz[2][i] * cosx) / SCALE;
        xyz[0][i] = tmpX;

        tmpY      = (xyz[1][i] * cosy - xyz[2][i] * siny) / SCALE;
        xyz[2][i] = (xyz[1][i] * siny + xyz[2][i] * cosy) / SCALE;
        xyz[1][i] = tmpY;

        tmpX      = (xyz[0][i] * cosz - xyz[1][i] * sinz) / SCALE;
        xyz[1][i] = (xyz[0][i] * sinz + xyz[1][i] * cosz) / SCALE;
        xyz[0][i] = tmpX;
    }
}


void draw(gI16 xyz[3][N], gColor col[N])
{
    static gU16 oldProjX[N] = {0};
    static gU16 oldProjY[N] = {0};
    static gU8 oldDotSize[N] = {0};
    gU16 i, projX, projY, projZ, dotSize;

    for (i = 0; i < N; i++) {
        projZ = SCALE - (xyz[2][i] + SCALE) / 4;
        projX = width / 2 + (xyz[0][i] * projZ / SCALE) / 25;
        projY = height / 2 + (xyz[1][i] * projZ / SCALE) / 25;
        dotSize = 3 - (xyz[2][i] + SCALE) * 2 / SCALE;

        gdispDrawCircle(oldProjX[i], oldProjY[i], oldDotSize[i], background);

        if (projX > dotSize &&
            projY > dotSize &&
            projX < width - dotSize &&
            projY < height - dotSize) {
            gdispDrawCircle(projX, projY, dotSize, col[i]);
            oldProjX[i] = projX;
            oldProjY[i] = projY;
            oldDotSize[i] = dotSize;
        }
    }
}


/* ---------------------------------------------------------------------- */

gI16 angleX = 0, angleY = 0, angleZ = 0;
gI16 speedX = 0, speedY = 0, speedZ = 0;

gI16 xyz[3][N];
gColor col[N];


int main(void)
{
    int pass = 0;

    gfxInit();

    gfxSleepMilliseconds(10);
    gdispClear(background);  /* glitches.. */
    gfxSleepMilliseconds(10);
    gdispClear(background);   /* glitches.. */
    gfxSleepMilliseconds(10);
    gdispClear(background);   /* glitches.. */

    width = (gU16)gdispGetWidth();
    height = (gU16)gdispGetHeight();

    initialize();

    for (;;) {
        matrix(xyz, col);
        rotate(xyz, angleX, angleY, angleZ);
        draw(xyz, col);

        angleX += speedX;
        angleY += speedY;
        angleZ += speedZ;

        if (pass > 400) {
            speedY = 1;
        }
        if (pass > 800) {
            speedX = 1;
        }
        if (pass > 1200) {
            speedZ = 1;
        }
        pass++;

        if (angleX >= SCALE) {
            angleX -= SCALE;
        } else if (angleX < 0) {
            angleX += SCALE;
        }

        if (angleY >= SCALE) {
            angleY -= SCALE;
        } else if (angleY < 0) {
            angleY += SCALE;
        }

        if (angleZ >= SCALE) {
            angleZ -= SCALE;
        } else if (angleZ < 0) {
            angleZ += SCALE;
        }
    }
}

/* ---------------------------------------------------------------------- */
