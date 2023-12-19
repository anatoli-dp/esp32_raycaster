#include <math.h>
#include <unordered_map>
#include <climits>

#include "../bb.h"

template <typename T>
T swap_endian(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

#define PI  3.1415926535897 // M_PI
#define P2  1.5707963267948 // PI/2
#define Px2 6.2831853071795 // PI*2
#define P3  4.7123889803846 // 3*PI/2
#define DR  0.0174532925199 // PI/180
#define RD  180 / PI

#define SIN(x)   sinL[(int)(x * 572.95779513082)]
#define COS(x)   cosL[(int)(x * 572.95779513082)]
#define TAN(x)   tanL[(int)(x * 572.95779513082)]

#define ATAN(x)  aTanL[(int)(x * 572.95779513082)]
#define SINFL(x) sinFL[(int)(x * 572.95779513082)]
#define COSFL(x) cosFL[(int)(x * 572.95779513082)]

#define COSIL(x) cosIL[(int)(x * 572.95779513082)]

uint16_t convertColor(bb_color color);

uint16_t* displayBuffer;

TFT_eSprite* tileSprite;
uint16_t* tileBuffer;
static int ts;
static int tw;
static int th;
static int _tx;
static int _ty;

static int mapX;
static int mapY;

static int* wallMap;
static int* ceilMap;
static int* floorMap;

static bool useFade = false;
static uint16_t fadeColor = TFT_BLACK;
static float fadeDistance = 750.0;

static int res;

static std::unordered_map<int, float> distL;
static float sinL[3600];
static float cosL[3600];
static float tanL[3600];

static float aTanL[3600];
static float sinFL[3600];
static float cosFL[3600];

static float cosIL[3600];
static float yL[361];

static const int resolution[4] = { 1, 2, 4, 5 };
static const float res_step[4] = {
    ((DR) / ((320 / 1) / 60)),
    ((DR) / ((320 / 2) / 60)),
    ((DR) / ((320 / 4) / 60)),
    ((DR) / ((320 / 5) / 60))
};

static bool drawCeils = true;
static bool drawWalls = true;
static bool drawFloors = true;

inline float dist (float ax, float ay, float bx, float by, float ang) { return sqrt( ((bx - ax) * (bx - ax)) + ((by - ay) * (by - ay)) ); }
inline int round_mul (int n, int m) { return (n >= 0 ? (n / m) * m : ((n - m + 1) / m) * m); }
inline float fixAngle (float a) { float ra = a; if (ra < 0) { ra += Px2; } if (ra > Px2) { ra -= Px2; } return ra; }

inline float inv_fast (float x) {
    union { float f; int i; } v;
    float w, sx;
    int m;

    sx = (x < 0) ? -1:1;
    x = sx * x;
    v.i = (int)(0x7EF127EA - *(uint32_t *)&x);
    w = x * v.f;
    v.f = v.f * (2 - w);
    
    return v.f * sx;
}

void bb_rycstRes (int r)
{
    res = r - 1;
}

void bb_rycstSheet (int sprNum, int tileSize)
{
    tileSprite = bb_sprget(sprNum);
    tileBuffer = (uint16_t*)tileSprite->getPointer();
    tw = tileSprite->width();
    th = tileSprite->height();
    ts = tileSize;
    _tx = tw / ts;
    _ty = tw / ts;
}

void bb_rycstFade (float distance, bb_color color)
{
    if (distance == NULL) {
        useFade = false;
    } else {
        useFade = true;
        fadeDistance = distance;
        if (!(color.r == NULL)) fadeColor = convertColor(color);
    }
}

void bb_rycstMap (int* wm, int* cm, int* fm, int ms, int ts)
{
    mapX = ms;
    mapY = ms;

    free(wallMap);
    free(ceilMap);
    free(floorMap);

    wallMap = (int*)heap_caps_malloc(ms * ms * sizeof(int), MALLOC_CAP_SPIRAM);
    ceilMap = (int*)heap_caps_malloc(ms * ms * sizeof(int), MALLOC_CAP_SPIRAM);
    floorMap = (int*)heap_caps_malloc(ms * ms * sizeof(int), MALLOC_CAP_SPIRAM);

    for (int i = 0; i < ms * ms; i++)
    {
        wallMap[i] = wm[i];
        ceilMap[i] = cm[i];
        floorMap[i] = fm[i];
    }

    float deg = 0.0;
    for (int i = 0; i < 3600; i++)
    {
        float ra = deg * DR;
        int index = (int)round(deg * 10.0);

        sinL[index] = sin(ra);
        cosL[index] = cos(ra);
        tanL[index] = tan(ra);

        aTanL[index] = -1 / tan(ra);
        sinFL[index] = sinL[index] * 120;
        cosFL[index] = cosL[index] * 120;

        cosIL[index] = (1 / cos(ra));

        deg += 0.1;
    }

    for (int i = 0; i < 361; i++)
    {
        yL[i] = (1.0 / ((float)i - 120.0));
    }
}

/*double currentMS = 0;
double lastMS = 0;
double MS = 0;
double FPS = 0;

char fps[50];
char ms[50];*/

void bb_rycst (float px, float py, float pa)
{
    //distance calculations
    int r, mx, my, mp, dof;
    float rx, ry, ra, xo, yo;
    float hx, hy, vx, vy;
    float disH, disV, disT;
    int side = 0;
    int dofMax = max(mapX, mapY);
    int mapSize = mapX * mapY;
    float aTan, nTan;
    //drawing calculations
    float angDiff;
    int r_res;
    int lineH, lineO;
    float ty, tx, ty_step, ty_off;
    int sx, sy;
    int fadeAlpha;

    displayBuffer = (uint16_t*)bb_displaybuffer()->getPointer();

    ra = fixAngle(pa - 0.5235987755982);
    for (r = 0; r < (320 / resolution[res]); r++)
    {
        int vmt = 0; int hmt = 0; //verticle and horizontal map texture number
        //check horizontal lines
        dof = 0;
        disH = __FLT_MAX__; hx = px; hy = py;
        aTan = ATAN(ra); //could remake as -1 / TAN(ra) to save memory ... no real performance gain
        if (ra > PI) { ry = round_mul((int)py, 64) - 0.0001; rx = (py - ry) * aTan + px; yo = -64; xo = -yo * aTan; } //looking down
        if (ra < PI) { ry = round_mul((int)py, 64) +     64; rx = (py - ry) * aTan + px; yo =  64; xo = -yo * aTan; } //looking up
        if (ra == 0 || ra == PI) { rx = px; ry = py; dof = dofMax; } //looking stright left or right
        while (dof < dofMax)
        {
            mx = (int)(rx)>>6; my = (int)(ry)>>6; mp = my * mapX + mx;
            if (mp > 0 && mp < mapSize && wallMap[mp] > 0) { hmt = wallMap[mp] - 1; hx = rx; hy = ry; disH = dist(px, py, hx, hy, ra); dof = dofMax; } //hit wall
            else { rx += xo; ry += yo; dof += 1; }
        }

        //check vertical lines
        dof = 0;
        disV = __FLT_MAX__; vx = px; vy = py;
        nTan = -TAN(ra);
        if (ra > P2 && ra < P3) { rx = round_mul((int)px, 64) - 0.0001; ry = (px - rx) * nTan + py; xo = -64; yo = -xo * nTan; } //looking left
        if (ra < P2 || ra > P3) { rx = round_mul((int)px, 64) +     64; ry = (px - rx) * nTan + py; xo =  64; yo = -xo * nTan; } //looking right
        if (ra == 0 || ra == PI) { rx = px; ry = py; dof = dofMax; } //looking straight up or down
        while (dof < dofMax)
        {
            mx = (int)(rx)>>6; my = (int)(ry)>>6; mp = my * mapX + mx;
            if (mp > 0 && mp < mapSize && wallMap[mp] > 0) { vmt = wallMap[mp] - 1; vx = rx; vy = ry; disV = dist(px, py, vx, vy, ra); dof = dofMax; } //hit wall
            else { rx += xo; ry += yo; dof += 1; }
        }

        if (disV < disH) { hmt = vmt; rx = vx; ry = vy; disT = disV; side = 0; } //verticle wall hit
        if (disH < disV) {            rx = hx; ry = hy; disT = disH; side = 1; } //horizontal wall hit

        angDiff = fixAngle(pa - ra);
        int r_res = r * resolution[res];

        //calculate wall pixel positions
        disT = disT * COS(angDiff); //fix fisheye
        lineH = 15360 / disT;
        ty_step = (float)ts / (float)lineH;
        ty_off = 0;
        if (lineH > 240) { ty_off = (lineH - 240) / 2.0; lineH = 240; } //line height
        lineO = (240 - lineH) / 2;

        ty = ty_off * ty_step;

        if (side == 1) {
            tx = (int)(rx / 2.0) % ts;
            if (ra < PI) tx = (ts - 1) - tx;
        } else {
            tx = (int)(ry / 2.0) % ts;
            if (ra > P2 && ra < 4.7123889803846) tx = (ts - 1) - tx;
        }

        //get x,y loc from tile nuber (hmt)
        sx = (hmt % _tx) * ts;
        sy = (hmt / _tx) * ts;

        if (useFade) {
            if (disT <= fadeDistance)
            {
                fadeAlpha = map((int)min(disT, fadeDistance), 0, fadeDistance, 255, 0);
            

                //draw walls
                for (int y = 0; y < lineH; y++)
                {
                    //bb_cset(tft.alphaBlend(fadeAlpha, tileSprite->readPixel(sx + tx, sy + ty), fadeColor));
                    //bb_px(r_res, y + lineO);
                    displayBuffer[(y + lineO) * 320 + r_res] = swap_endian<uint16_t>(tft.alphaBlend(fadeAlpha, tileBuffer[(sy + (int)ty) * tw + (sx + (int)tx)], fadeColor));
                    ty += ty_step;
                }
            }
        } else {
            //draw walls
            for (int y = 0; y < lineH; y++)
            {
                displayBuffer[(y + lineO) * 320 + r_res] = tileBuffer[(sy + (int)ty) * tw + (sx + (int)tx)];
                ty += ty_step;
            }

            //calculate floor and ceiling tiles pixel positions
            float _px = px / 2; float _angX = COSFL(ra) * ts * COSIL(angDiff);
            float _py = py / 2; float _angY = SINFL(ra) * ts * COSIL(angDiff);
            for (int y = lineO + lineH; y < 240; y ++)
            {
                tx = _px + _angX * yL[y];
                ty = _py + _angY * yL[y];

                //draw floor
                int mp = floorMap[(int)(ty * inv_fast(32.0)) * mapX + (int)(tx * inv_fast(32.0))] - 1;
                sx = (mp % _tx) * ts;
                sy = (mp / _tx) * ts;
                displayBuffer[y * 320 + r_res] = tileBuffer[(((int)(ty)&31) + sy) * tw + (((int)(tx)&31) + sx)];

                //draw ceiling
                mp = ceilMap[(int)(ty * inv_fast(32.0)) * mapX + (int)(tx * inv_fast(32.0))] - 1;
                sx = (mp % _tx) * ts;
                sy = (mp / _tx) * ts;
                displayBuffer[(240 - y) * 320 + r_res] = tileBuffer[(((int)(ty)&31) + sy) * tw + (((int)(tx)&31) + sx)];
            }
        }

        //add angle loop again
        ra = fixAngle(ra + res_step[res]);
    }

    /*bb_textalign(TOP_LEFT);
    bb_textsize(2);
    bb_cset(TFT_RED);
    bb_textln(0, 0);

    currentMS = millis();
    MS = currentMS - lastMS;
    lastMS = currentMS;
    FPS = 1000 / MS;

    sprintf(ms, "MS: %lf", MS);
    sprintf(fps, "FPS: %lf", FPS);

    bb_println(fps);
    bb_println(ms);*/
}