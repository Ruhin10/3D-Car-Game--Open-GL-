#include <windows.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

int cameraMode = 0;

// ---------- CONSTANTS ----------
const float PI = 3.1415926f;

const float WORLD_SPEED = 0.55f;

// Wider main road
const float ROAD_LEFT  = -4.0f;
const float ROAD_RIGHT =  4.0f;

// ---------- PARALLAX SPEEDS ----------
const float ROAD_PARALLAX       = 1.35f;
const float SIDE_ROAD_PARALLAX  = 1.20f;
const float TREE_PARALLAX       = 1.05f;
const float BRIDGE_PARALLAX     = 0.90f;
const float BUILDING_PARALLAX   = 0.65f;
const float FAR_PARALLAX        = 0.32f;

// ---------- GLOBALS ----------
float playerX = 0.0f;
float worldScroll = 0.0f;
float animTime = 0.0f;

// ---------- 360 CAMERA ----------
float rotationAngle = 0.0f;
bool isRotating360 = false;

// ---------- DAY / NIGHT SYSTEM ----------
float dayTime = 0.45f;
const float DAY_CYCLE_SPEED = 0.00018f;

#define MAX_ENEMIES 2
#define MAX_BIKES 2

float enemyX[MAX_ENEMIES] = {-2.0f, 0.0f};
float enemyZ[MAX_ENEMIES] = {-35.0f, -65.0f};

float bikeX[MAX_BIKES] = {-2.0f, 2.0f};
float bikeZ[MAX_BIKES] = {-50.0f, -80.0f};

int score = 0;
bool gameOver = false;

#define NUM_OBJECTS 24
float buildingHeights[NUM_OBJECTS];
float buildingColors[NUM_OBJECTS][6];

// ---------- COLOUR PALETTES ----------
static const float WALL_PALETTE[][3] = {
    {0.80f, 0.30f, 0.20f},
    {0.25f, 0.45f, 0.70f},
    {0.90f, 0.75f, 0.30f},
    {0.35f, 0.60f, 0.40f},
    {0.60f, 0.55f, 0.75f},
    {0.70f, 0.45f, 0.25f},
    {0.30f, 0.55f, 0.65f},
    {0.85f, 0.55f, 0.25f},
    {0.50f, 0.50f, 0.55f},
    {0.40f, 0.30f, 0.60f},
};

static const float WIN_PALETTE[][3] = {
    {0.95f, 0.85f, 0.60f},
    {0.70f, 0.90f, 1.00f},
    {1.00f, 0.95f, 0.70f},
    {0.80f, 1.00f, 0.80f},
    {0.90f, 0.80f, 1.00f},
    {1.00f, 0.80f, 0.60f},
    {0.60f, 1.00f, 0.95f},
    {1.00f, 0.90f, 0.50f},
    {0.85f, 0.95f, 1.00f},
    {0.80f, 0.70f, 1.00f},
};

static const float ENEMY_PALETTE[MAX_ENEMIES][6] = {
    {0.95f, 0.15f, 0.15f,  0.60f, 0.00f, 0.00f},
    {0.95f, 0.55f, 0.05f,  0.60f, 0.28f, 0.00f}
};

// Pedestrian color sets: [shirt R,G,B, pants R,G,B, skin R,G,B]
static const float PED_PALETTE[][9] = {
    {0.20f, 0.35f, 0.85f,  0.10f, 0.10f, 0.12f,  0.96f, 0.78f, 0.62f},
    {0.85f, 0.20f, 0.20f,  0.15f, 0.10f, 0.40f,  0.90f, 0.68f, 0.52f},
    {0.15f, 0.65f, 0.25f,  0.20f, 0.15f, 0.10f,  0.80f, 0.58f, 0.42f},
    {0.90f, 0.75f, 0.10f,  0.25f, 0.20f, 0.30f,  0.70f, 0.50f, 0.38f},
    {0.55f, 0.10f, 0.70f,  0.12f, 0.12f, 0.15f,  0.96f, 0.80f, 0.65f},
};

// ---------- HELPERS ----------
float wrapZ(float z, float cycle) {
    while (z > 12.0f)   z -= cycle;
    while (z < -230.0f) z += cycle;
    return z;
}

float getSideRoadZ(int index, bool leftSide) {
    float cycle = 240.0f;
    float baseZ;
    if (leftSide)
        baseZ = -28.0f - index * 65.0f;
    else
        baseZ = -60.0f - index * 65.0f;
    return wrapZ(baseZ + fmodf(worldScroll * SIDE_ROAD_PARALLAX, cycle), cycle);
}

bool isNearSideRoadGap(float z, bool leftSide) {
    for (int i = 0; i < 5; i++) {
        float roadZ = getSideRoadZ(i, leftSide);
        if (fabs(z - roadZ) < 10.0f) return true;
    }
    return false;
}

// ---------- DAY / NIGHT HELPERS ----------
float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float mixValue(float a, float b, float t) {
    return a + (b - a) * clamp01(t);
}

void mixColor(float aR, float aG, float aB,
              float bR, float bG, float bB,
              float t,
              float &r, float &g, float &b)
{
    r = mixValue(aR, bR, t);
    g = mixValue(aG, bG, t);
    b = mixValue(aB, bB, t);
}

const char* getEnvironmentName() {
    if (dayTime < 0.20f || dayTime > 0.86f) return "Night";
    if (dayTime < 0.36f) return "Morning";
    if (dayTime < 0.66f) return "Day";
    return "Evening";
}

void getSkyColors(float &topR, float &topG, float &topB,
                  float &bottomR, float &bottomG, float &bottomB)
{
    float nTopR = 0.02f, nTopG = 0.03f, nTopB = 0.12f;
    float nBotR = 0.05f, nBotG = 0.07f, nBotB = 0.20f;
    float mTopR = 0.95f, mTopG = 0.55f, mTopB = 0.28f;
    float mBotR = 0.98f, mBotG = 0.78f, mBotB = 0.48f;
    float dTopR = 0.18f, dTopG = 0.48f, dTopB = 0.82f;
    float dBotR = 0.72f, dBotG = 0.88f, dBotB = 0.98f;
    float eTopR = 0.15f, eTopG = 0.10f, eTopB = 0.30f;
    float eBotR = 0.95f, eBotG = 0.38f, eBotB = 0.18f;

    if (dayTime < 0.25f) {
        float t = dayTime / 0.25f;
        mixColor(nTopR,nTopG,nTopB, mTopR,mTopG,mTopB, t, topR,topG,topB);
        mixColor(nBotR,nBotG,nBotB, mBotR,mBotG,mBotB, t, bottomR,bottomG,bottomB);
    } else if (dayTime < 0.50f) {
        float t = (dayTime - 0.25f) / 0.25f;
        mixColor(mTopR,mTopG,mTopB, dTopR,dTopG,dTopB, t, topR,topG,topB);
        mixColor(mBotR,mBotG,mBotB, dBotR,dBotG,dBotB, t, bottomR,bottomG,bottomB);
    } else if (dayTime < 0.75f) {
        float t = (dayTime - 0.50f) / 0.25f;
        mixColor(dTopR,dTopG,dTopB, eTopR,eTopG,eTopB, t, topR,topG,topB);
        mixColor(dBotR,dBotG,dBotB, eBotR,eBotG,eBotB, t, bottomR,bottomG,bottomB);
    } else {
        float t = (dayTime - 0.75f) / 0.25f;
        mixColor(eTopR,eTopG,eTopB, nTopR,nTopG,nTopB, t, topR,topG,topB);
        mixColor(eBotR,eBotG,eBotB, nBotR,nBotG,nBotB, t, bottomR,bottomG,bottomB);
    }
}

float getDayBrightness() {
    if (dayTime < 0.25f)       return mixValue(0.25f, 0.75f, dayTime / 0.25f);
    else if (dayTime < 0.50f)  return mixValue(0.75f, 1.00f, (dayTime-0.25f)/0.25f);
    else if (dayTime < 0.75f)  return mixValue(1.00f, 0.55f, (dayTime-0.50f)/0.25f);
    else                       return mixValue(0.55f, 0.25f, (dayTime-0.75f)/0.25f);
}



// ---------- SKY ----------
void drawSky() {
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    float topR, topG, topB, bottomR, bottomG, bottomB;
    getSkyColors(topR, topG, topB, bottomR, bottomG, bottomB);

    glBegin(GL_QUADS);
        glColor3f(bottomR, bottomG, bottomB);
        glVertex3f(-300.0f, 0.0f, -240.0f);
        glVertex3f( 300.0f, 0.0f, -240.0f);
        glColor3f(topR, topG, topB);
        glVertex3f( 300.0f, 200.0f, -240.0f);
        glVertex3f(-300.0f, 200.0f, -240.0f);
    glEnd();

    glBegin(GL_QUADS);
        glColor3f(bottomR, bottomG, bottomB);
        glVertex3f(-300.0f, 0.0f, 20.0f);
        glVertex3f( 300.0f, 0.0f, 20.0f);
        glColor3f(topR, topG, topB);
        glVertex3f( 300.0f, 200.0f, 20.0f);
        glVertex3f(-300.0f, 200.0f, 20.0f);
    glEnd();

    glBegin(GL_QUADS);
        glColor3f(bottomR, bottomG, bottomB);
        glVertex3f(-300.0f, 0.0f, -240.0f);
        glVertex3f(-300.0f, 0.0f, 20.0f);
        glColor3f(topR, topG, topB);
        glVertex3f(-300.0f, 200.0f, 20.0f);
        glVertex3f(-300.0f, 200.0f, -240.0f);
    glEnd();

    glBegin(GL_QUADS);
        glColor3f(bottomR, bottomG, bottomB);
        glVertex3f(300.0f, 0.0f, -240.0f);
        glVertex3f(300.0f, 0.0f, 20.0f);
        glColor3f(topR, topG, topB);
        glVertex3f(300.0f, 200.0f, 20.0f);
        glVertex3f(300.0f, 200.0f, -240.0f);
    glEnd();

    float nightAmount = 0.0f;
    if (dayTime < 0.20f)       nightAmount = 1.0f - dayTime / 0.20f;
    else if (dayTime > 0.78f)  nightAmount = (dayTime - 0.78f) / 0.22f;
    nightAmount = clamp01(nightAmount);


    if (dayTime > 0.18f && dayTime < 0.78f) {
        float sunT = (dayTime - 0.18f) / 0.60f;
        float sunX = mixValue(-65.0f, 65.0f, sunT);
        float sunY = 16.0f + sinf(sunT * PI) * 48.0f;
        glColor3f(1.0f, 0.88f, 0.28f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex3f(sunX, sunY, -216.0f);
            for (int i = 0; i <= 40; i++) {
                float a = 2.0f * PI * i / 40.0f;
                glVertex3f(sunX + cosf(a)*5.2f, sunY + sinf(a)*5.2f, -216.0f);
            }
        glEnd();
    }

    if (nightAmount > 0.05f) {
        float moonX;
        if (dayTime > 0.75f) moonX = mixValue(-55.0f, 15.0f, (dayTime-0.75f)/0.25f);
        else                  moonX = mixValue(15.0f, 55.0f, dayTime/0.25f);
        float moonY = 54.0f;
        glColor3f(0.90f, 0.92f, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex3f(moonX, moonY, -216.0f);
            for (int i = 0; i <= 40; i++) {
                float a = 2.0f * PI * i / 40.0f;
                glVertex3f(moonX + cosf(a)*3.5f, moonY + sinf(a)*3.5f, -216.0f);
            }
        glEnd();
        glColor3f(topR, topG, topB);
        glBegin(GL_TRIANGLE_FAN);
            glVertex3f(moonX+1.2f, moonY+0.4f, -215.0f);
            for (int i = 0; i <= 40; i++) {
                float a = 2.0f * PI * i / 40.0f;
                glVertex3f(moonX+1.2f + cosf(a)*3.2f, moonY+0.4f + sinf(a)*3.2f, -215.0f);
            }
        glEnd();
    }

    struct Cloud { float x, y, z, sx, sy; };
    static const Cloud clouds[] = {
        {-40.0f, 40.0f, -200.0f, 18.0f, 5.0f},
        {-10.0f, 55.0f, -205.0f, 24.0f, 6.5f},
        { 30.0f, 45.0f, -203.0f, 20.0f, 5.5f},
        { 70.0f, 60.0f, -208.0f, 15.0f, 4.5f},
        {-70.0f, 50.0f, -202.0f, 22.0f, 6.0f},
        { 50.0f, 35.0f, -199.0f, 16.0f, 4.0f},
    };
    int numClouds = sizeof(clouds)/sizeof(clouds[0]);
    float cloudLight = mixValue(0.25f, 1.0f, getDayBrightness());

    for (int c = 0; c < numClouds; c++) {
        glColor3f(cloudLight, cloudLight, cloudLight);
        for (int p = 0; p < 3; p++) {
            float cx = clouds[c].x + (p-1)*clouds[c].sx*0.45f;
            float cy = clouds[c].y + (p==1 ? clouds[c].sy*0.25f : 0.0f);
            float cz = clouds[c].z;
            float rx = clouds[c].sx * 0.55f, ry = clouds[c].sy * 0.75f;
            glBegin(GL_TRIANGLE_FAN);
                glVertex3f(cx, cy, cz);
                for (int s = 0; s <= 24; s++) {
                    float a = s * 2.0f * PI / 24;
                    glVertex3f(cx + rx*cosf(a), cy + ry*sinf(a), cz);
                }
            glEnd();
        }
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

// ---------- ENVIRONMENT LIGHTING ----------
void updateEnvironmentLighting() {
    float brightness = getDayBrightness();
    float topR, topG, topB, bottomR, bottomG, bottomB;
    getSkyColors(topR, topG, topB, bottomR, bottomG, bottomB);
    glClearColor(bottomR, bottomG, bottomB, 1.0f);

    float ambient[] = {0.12f+brightness*0.28f, 0.12f+brightness*0.28f, 0.14f+brightness*0.30f, 1.0f};
    float diffuse[] = {0.25f+brightness*0.75f, 0.25f+brightness*0.72f, 0.28f+brightness*0.68f, 1.0f};
    float light_pos[] = {-30.0f+dayTime*80.0f, 18.0f+brightness*25.0f, 18.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    float fogColor[] = {bottomR, bottomG, bottomB, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, brightness < 0.35f ? 0.018f : 0.007f);
}

// ---------- WHEEL ----------
void drawRealisticWheel(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(90, 0, 1, 0);
    GLUquadric* quad = gluNewQuadric();

    glColor3f(0.08f, 0.08f, 0.08f);
    gluCylinder(quad, 0.32f, 0.32f, 0.22f, 24, 3);
    gluDisk(quad, 0.0f, 0.32f, 24, 1);
    glPushMatrix(); glTranslatef(0,0,0.22f); gluDisk(quad, 0.0f, 0.32f, 24, 1); glPopMatrix();

    glColor3f(0.78f, 0.78f, 0.80f);
    glPushMatrix(); glTranslatef(0,0,0.01f); gluCylinder(quad, 0.20f, 0.20f, 0.20f, 24, 2); gluDisk(quad, 0.0f, 0.20f, 24, 1); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0,0.21f); gluDisk(quad, 0.0f, 0.20f, 24, 1); glPopMatrix();

    glColor3f(0.65f, 0.65f, 0.68f);
    for (int sp = 0; sp < 5; sp++) {
        float ang = sp * 72.0f * PI / 180.0f;
        glPushMatrix();
            glTranslatef(cosf(ang)*0.15f, sinf(ang)*0.15f, 0.11f);
            glScalef(0.05f, 0.22f, 0.05f);
            glutSolidCube(1);
        glPopMatrix();
    }

    glColor3f(0.90f, 0.90f, 0.92f);
    glPushMatrix(); glTranslatef(0,0,0.11f); gluDisk(quad, 0.0f, 0.06f, 16, 1); glPopMatrix();

    gluDeleteQuadric(quad);
    glPopMatrix();
}

// ---------- REALISTIC MOTORCYCLE ----------
// FIX: Rotated 180 degrees so bike faces same direction as player (forward = negative Z)
void drawMotorcycle(float x, float z) {
    glPushMatrix();
    glTranslatef(x, 0.38f, z);
    glRotatef(180.0f, 0, 1, 0);  // <-- FACE SAME DIRECTION AS PLAYER

    GLUquadric* q = gluNewQuadric();

    // ===== MAIN FRAME (spine) =====
    glColor3f(0.12f, 0.12f, 0.14f);
    glPushMatrix();
        glTranslatef(0, 0.28f, 0);
        glRotatef(8, 1, 0, 0);
        glScalef(0.10f, 0.10f, 2.20f);
        glutSolidCube(1);
    glPopMatrix();

    // Down tube (front frame)
    glColor3f(0.15f, 0.15f, 0.18f);
    glPushMatrix();
        glTranslatef(0, 0.05f, 0.85f);
        glRotatef(-50, 1, 0, 0);
        glScalef(0.08f, 0.08f, 0.80f);
        glutSolidCube(1);
    glPopMatrix();

    // Rear sub-frame
    glPushMatrix();
        glTranslatef(0, 0.35f, -0.7f);
        glRotatef(-18, 1, 0, 0);
        glScalef(0.07f, 0.07f, 0.80f);
        glutSolidCube(1);
    glPopMatrix();

    // ===== ENGINE BLOCK =====
    glColor3f(0.35f, 0.35f, 0.38f);
    glPushMatrix();
        glTranslatef(0, 0.05f, 0.10f);
        glScalef(0.52f, 0.40f, 0.70f);
        glutSolidCube(1);
    glPopMatrix();

    // Engine fins (cooling)
    glColor3f(0.45f, 0.45f, 0.48f);
    for (int f = 0; f < 5; f++) {
        glPushMatrix();
            glTranslatef(0.28f, 0.05f + f*0.06f - 0.12f, 0.10f);
            glScalef(0.06f, 0.025f, 0.60f);
            glutSolidCube(1);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(-0.28f, 0.05f + f*0.06f - 0.12f, 0.10f);
            glScalef(0.06f, 0.025f, 0.60f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // ===== CYLINDER HEAD =====
    glColor3f(0.30f, 0.30f, 0.33f);
    glPushMatrix();
        glTranslatef(0, 0.28f, 0.20f);
        glRotatef(-70, 1, 0, 0);
        gluCylinder(q, 0.12f, 0.10f, 0.38f, 12, 3);
    glPopMatrix();

    // ===== FUEL TANK (sporty teardrop shape) =====
    glColor3f(0.80f, 0.05f, 0.05f);
    glPushMatrix();
        glTranslatef(0, 0.48f, 0.28f);
        glScalef(0.50f, 0.28f, 0.85f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0, 0.44f, 0.75f);
        glScalef(0.40f, 0.20f, 0.30f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0, 0.42f, -0.15f);
        glScalef(0.42f, 0.18f, 0.30f);
        glutSolidCube(1);
    glPopMatrix();

    // Tank stripe/decal
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
        glTranslatef(0.26f, 0.48f, 0.28f);
        glScalef(0.02f, 0.20f, 0.70f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(-0.26f, 0.48f, 0.28f);
        glScalef(0.02f, 0.20f, 0.70f);
        glutSolidCube(1);
    glPopMatrix();

    // ===== SEAT (contoured) =====
    glColor3f(0.08f, 0.08f, 0.08f);
    glPushMatrix();
        glTranslatef(0, 0.50f, -0.38f);
        glScalef(0.38f, 0.12f, 0.80f);
        glutSolidCube(1);
    glPopMatrix();
    // Seat hump (tail section)
    glColor3f(0.80f, 0.05f, 0.05f);
    glPushMatrix();
        glTranslatef(0, 0.55f, -0.85f);
        glScalef(0.34f, 0.18f, 0.35f);
        glutSolidCube(1);
    glPopMatrix();

    // ===== FAIRING / WINDSCREEN =====
    glColor3f(0.80f, 0.05f, 0.05f);
    glPushMatrix();
        glTranslatef(0, 0.72f, 0.85f);
        glScalef(0.48f, 0.35f, 0.12f);
        glutSolidCube(1);
    glPopMatrix();
    // Windscreen
    glColor3f(0.40f, 0.75f, 1.00f);
    glPushMatrix();
        glTranslatef(0, 0.95f, 0.82f);
        glRotatef(-15, 1, 0, 0);
        glScalef(0.38f, 0.28f, 0.06f);
        glutSolidCube(1);
    glPopMatrix();

    // Headlight
    glColor3f(1.0f, 0.95f, 0.70f);
    glPushMatrix();
        glTranslatef(0, 0.65f, 0.92f);
        glutSolidSphere(0.08f, 10, 10);
    glPopMatrix();
    // Headlight ring
    glColor3f(0.75f, 0.75f, 0.78f);
    glPushMatrix();
        glTranslatef(0, 0.65f, 0.90f);
        glRotatef(90, 0, 1, 0);
        gluCylinder(q, 0.10f, 0.10f, 0.04f, 16, 1);
    glPopMatrix();

    // ===== FRONT FORK (USD - upside-down) =====
    glColor3f(0.82f, 0.82f, 0.85f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
            glTranslatef(s*0.16f, 0.45f, 0.92f);
            glRotatef(-75, 1, 0, 0);
            gluCylinder(q, 0.04f, 0.04f, 0.65f, 10, 2);
        glPopMatrix();
        glColor3f(0.65f, 0.65f, 0.68f);
        glPushMatrix();
            glTranslatef(s*0.16f, 0.10f, 1.10f);
            glRotatef(-75, 1, 0, 0);
            gluCylinder(q, 0.055f, 0.055f, 0.45f, 10, 2);
        glPopMatrix();
    }

    // Fork brace
    glColor3f(0.70f, 0.70f, 0.73f);
    glPushMatrix();
        glTranslatef(0, 0.08f, 1.15f);
        glRotatef(90, 0, 1, 0);
        gluCylinder(q, 0.03f, 0.03f, 0.32f, 8, 1);
    glPopMatrix();

    // ===== HANDLEBAR =====
    glColor3f(0.20f, 0.20f, 0.22f);
    glPushMatrix();
        glTranslatef(0, 0.90f, 0.72f);
        glRotatef(90, 0, 1, 0);
        gluCylinder(q, 0.025f, 0.025f, 0.72f, 10, 2);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0, 0.90f, 0.72f);
        glRotatef(-90, 0, 1, 0);
        gluCylinder(q, 0.025f, 0.025f, 0.36f, 10, 2);
    glPopMatrix();
    glColor3f(0.60f, 0.60f, 0.62f);
    glPushMatrix();
        glTranslatef(0, 0.90f, 0.72f);
        glutSolidSphere(0.06f, 8, 8);
    glPopMatrix();
    glColor3f(0.08f, 0.08f, 0.08f);
    glPushMatrix();
        glTranslatef(0.38f, 0.90f, 0.72f);
        glRotatef(90, 0, 1, 0);
        gluCylinder(q, 0.038f, 0.038f, 0.12f, 10, 2);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(-0.38f, 0.90f, 0.72f);
        glRotatef(-90, 0, 1, 0);
        gluCylinder(q, 0.038f, 0.038f, 0.12f, 10, 2);
    glPopMatrix();

    // ===== EXHAUST SYSTEM =====
    glColor3f(0.72f, 0.72f, 0.74f);
    glPushMatrix();
        glTranslatef(0.24f, 0.02f, 0.35f);
        glRotatef(-25, 1, 0, 0);
        gluCylinder(q, 0.04f, 0.038f, 0.55f, 10, 2);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0.28f, 0.01f, -0.15f);
        glRotatef(90, 1, 0, 0);
        gluCylinder(q, 0.038f, 0.035f, 0.80f, 10, 2);
    glPopMatrix();
    glColor3f(0.55f, 0.55f, 0.58f);
    glPushMatrix();
        glTranslatef(0.30f, 0.38f, -0.90f);
        glRotatef(15, 1, 0, 0);
        gluCylinder(q, 0.055f, 0.045f, 0.55f, 14, 3);
    glPopMatrix();
    glColor3f(0.30f, 0.30f, 0.32f);
    glPushMatrix();
        glTranslatef(0.30f, 0.46f, -1.33f);
        glRotatef(15, 1, 0, 0);
        gluDisk(q, 0.025f, 0.045f, 14, 1);
    glPopMatrix();

    // ===== CHAIN/SWINGARM =====
    glColor3f(0.25f, 0.25f, 0.28f);
    glPushMatrix();
        glTranslatef(0.18f, 0.05f, -0.45f);
        glScalef(0.08f, 0.08f, 0.80f);
        glutSolidCube(1);
    glPopMatrix();

    // ===== FOOTPEGS =====
    glColor3f(0.55f, 0.55f, 0.58f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
            glTranslatef(s*0.30f, 0.02f, -0.10f);
            glRotatef(90, 0, 1, 0);
            gluCylinder(q, 0.025f, 0.025f, 0.20f, 8, 1);
        glPopMatrix();
    }

    // ===== BRAKE CALIPER =====
    glColor3f(0.80f, 0.10f, 0.10f);
    glPushMatrix();
        glTranslatef(-0.22f, -0.10f, 1.10f);
        glScalef(0.08f, 0.12f, 0.10f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(-0.22f, -0.10f, -1.10f);
        glScalef(0.08f, 0.12f, 0.10f);
        glutSolidCube(1);
    glPopMatrix();

    // ===== REALISTIC WHEELS =====
    drawRealisticWheel(0, -0.22f, 1.15f);
    drawRealisticWheel(0, -0.22f, -1.15f);

    // Chain guard
    glColor3f(0.80f, 0.05f, 0.05f);
    glPushMatrix();
        glTranslatef(0.22f, -0.12f, -0.80f);
        glScalef(0.06f, 0.22f, 0.90f);
        glutSolidCube(1);
    glPopMatrix();

    // Tail light
    glColor3f(1.0f, 0.10f, 0.10f);
    glPushMatrix();
        glTranslatef(0, 0.56f, -1.00f);
        glScalef(0.28f, 0.10f, 0.06f);
        glutSolidCube(1);
    glPopMatrix();

    gluDeleteQuadric(q);
    glPopMatrix();
}

// ---------- CAR ----------
// FIX: mirrorFront parameter now used consistently so enemy/player all face same direction.
// Player uses mirrorFront=true (headlights forward = negative Z = toward camera).
// Enemies also use mirrorFront=true so they face the same direction.
void drawRealisticCar(float x, float z,
                      float br, float bg, float bb,
                      float ar, float ag, float ab,
                      bool mirrorFront)
{
    glPushMatrix();
    glTranslatef(x, 0.32f, z);

    glColor3f(br, bg, bb);
    glPushMatrix(); glTranslatef(0,0.20f,0); glScalef(2.20f,0.42f,4.40f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.52f, mirrorFront?-1.30f:1.30f); glScalef(2.00f,0.22f,1.80f); glutSolidCube(1); glPopMatrix();

    glColor3f(br*0.92f,bg*0.92f,bb*0.92f);
    glPushMatrix(); glTranslatef(0,0.52f, mirrorFront?1.30f:-1.30f); glScalef(2.00f,0.18f,1.60f); glutSolidCube(1); glPopMatrix();

    glColor3f(br*0.88f,bg*0.88f,bb*0.88f);
    glPushMatrix(); glTranslatef(0,0.88f,0.10f); glScalef(1.70f,0.60f,2.10f); glutSolidCube(1); glPopMatrix();

    glColor3f(br*0.80f,bg*0.80f,bb*0.80f);
    glPushMatrix(); glTranslatef(0,1.22f,0.10f); glScalef(1.55f,0.22f,1.75f); glutSolidCube(1); glPopMatrix();

    glColor3f(0.15f,0.25f,0.40f);
    glPushMatrix(); glTranslatef(0,0.95f, mirrorFront?-0.92f:0.92f); glScalef(1.50f,0.55f,0.08f); glutSolidCube(1); glPopMatrix();

    glColor3f(0.12f,0.20f,0.35f);
    glPushMatrix(); glTranslatef(0,0.95f, mirrorFront?0.92f:-0.92f); glScalef(1.40f,0.50f,0.08f); glutSolidCube(1); glPopMatrix();

    glColor3f(0.15f,0.25f,0.40f);
    for (int s=-1; s<=1; s+=2) {
        glPushMatrix(); glTranslatef(s*0.86f,0.95f,0.10f); glScalef(0.06f,0.50f,1.60f); glutSolidCube(1); glPopMatrix();
    }

    glColor3f(ar,ag,ab);
    glPushMatrix(); glTranslatef(0,0.18f, mirrorFront?-2.22f:2.22f); glScalef(2.25f,0.28f,0.20f); glutSolidCube(1); glPopMatrix();

    glColor3f(ar*0.80f,ag*0.80f,ab*0.80f);
    glPushMatrix(); glTranslatef(0,0.18f, mirrorFront?2.22f:-2.22f); glScalef(2.25f,0.24f,0.20f); glutSolidCube(1); glPopMatrix();

    // Headlights (bright white/yellow at front)
    glColor3f(1.0f,0.98f,0.82f);
    for (int s=-1; s<=1; s+=2) {
        glPushMatrix(); glTranslatef(s*0.75f,0.50f, mirrorFront?-2.23f:2.23f); glScalef(0.36f,0.18f,0.10f); glutSolidCube(1); glPopMatrix();
    }

    // Tail lights (red at back)
    glColor3f(0.95f,0.10f,0.10f);
    for (int s=-1; s<=1; s+=2) {
        glPushMatrix(); glTranslatef(s*0.75f,0.50f, mirrorFront?2.23f:-2.23f); glScalef(0.34f,0.16f,0.10f); glutSolidCube(1); glPopMatrix();
    }

    glColor3f(ar,ag,ab);
    for (int s=-1; s<=1; s+=2) {
        glPushMatrix(); glTranslatef(s*1.15f,0.92f, mirrorFront?-0.60f:0.60f); glScalef(0.10f,0.12f,0.30f); glutSolidCube(1); glPopMatrix();
    }

    float fz = mirrorFront ? -1.40f :  1.40f;
    float rz = mirrorFront ?  1.40f : -1.40f;
    drawRealisticWheel(-1.12f,-0.12f,fz);
    drawRealisticWheel( 1.12f,-0.12f,fz);
    drawRealisticWheel(-1.12f,-0.12f,rz);
    drawRealisticWheel( 1.12f,-0.12f,rz);

    glPopMatrix();
}

// ---------- TREE ----------
void drawTree(float x, float z) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    GLUquadric* quad = gluNewQuadric();

    glColor3f(0.38f, 0.22f, 0.08f);
    glPushMatrix(); glRotatef(-90,1,0,0); gluCylinder(quad,0.18f,0.14f,1.8f,12,4); glPopMatrix();

    glColor3f(0.07f, 0.45f, 0.10f);
    glPushMatrix(); glTranslatef(0,1.8f,0); glutSolidSphere(0.80f,18,18); glPopMatrix();
    glColor3f(0.09f, 0.58f, 0.14f);
    glPushMatrix(); glTranslatef(-0.30f,2.2f,0.20f); glutSolidSphere(0.65f,16,16); glPopMatrix();
    glPushMatrix(); glTranslatef(0.35f,2.15f,-0.15f); glutSolidSphere(0.70f,16,16); glPopMatrix();
    glPushMatrix(); glTranslatef(0.05f,2.65f,0.10f); glutSolidSphere(0.55f,16,16); glPopMatrix();

    gluDeleteQuadric(quad);
    glPopMatrix();
}

// ---------- BUILDING ----------
void drawBuilding(float x, float z, float height, bool isRightSide, int idx) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);

    int id = idx % NUM_OBJECTS;
    float wr = buildingColors[id][0], wg = buildingColors[id][1], wb = buildingColors[id][2];

    glColor3f(wr, wg, wb);
    glPushMatrix(); glTranslatef(0,height/2.0f,0); glScalef(2.7f,height,2.7f); glutSolidCube(1); glPopMatrix();

    glColor3f(wr*0.60f,wg*0.60f,wb*0.60f);
    glPushMatrix(); glTranslatef(0,height+0.08f,0); glScalef(2.9f,0.18f,2.9f); glutSolidCube(1); glPopMatrix();

    glColor3f(buildingColors[id][3],buildingColors[id][4],buildingColors[id][5]);
    float faceX = 1.36f;
    for (float y=0.6f; y<height; y+=1.0f) {
        for (float zw=-0.9f; zw<=0.9f; zw+=0.9f) {
            glPushMatrix();
            if (isRightSide) glTranslatef(-faceX,y,zw);
            else             glTranslatef( faceX,y,zw);
            glScalef(0.10f,0.42f,0.42f);
            glutSolidCube(1);
            glPopMatrix();
        }
    }
    glPopMatrix();
}

// ---------- COLORED PEDESTRIAN ----------
// FIX: Added per-pedestrian color palette for shirt, pants, and skin tone
void drawPedestrian(float x, float y, float z, float phase, int colorIdx) {
    float armSwing = sinf(phase)*35.0f;
    float legSwing = sinf(phase)*30.0f;

    int ci = colorIdx % 5;
    float sR = PED_PALETTE[ci][0], sG = PED_PALETTE[ci][1], sB = PED_PALETTE[ci][2]; // shirt
    float pR = PED_PALETTE[ci][3], pG = PED_PALETTE[ci][4], pB = PED_PALETTE[ci][5]; // pants
    float skR = PED_PALETTE[ci][6], skG = PED_PALETTE[ci][7], skB = PED_PALETTE[ci][8]; // skin

    glPushMatrix();
    glTranslatef(x,y,z);
    glScalef(0.55f,0.55f,0.55f);

    // Head (skin tone)
    glColor3f(skR, skG, skB);
    glPushMatrix(); glTranslatef(0,1.95f,0); glutSolidSphere(0.22f,16,16); glPopMatrix();

    // Hair
    glColor3f(0.15f, 0.10f, 0.05f);
    glPushMatrix(); glTranslatef(0,2.10f,0); glutSolidSphere(0.14f,12,12); glPopMatrix();

    // Torso (shirt color)
    glColor3f(sR, sG, sB);
    glPushMatrix(); glTranslatef(0,1.15f,0); glScalef(0.40f,0.80f,0.22f); glutSolidCube(1); glPopMatrix();

    // Arms (skin tone)
    glColor3f(skR, skG, skB);
    glPushMatrix(); glTranslatef(-0.28f,1.35f,0); glRotatef(armSwing,1,0,0); glTranslatef(0,-0.32f,0); glScalef(0.10f,0.65f,0.10f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.28f,1.35f,0); glRotatef(-armSwing,1,0,0); glTranslatef(0,-0.32f,0); glScalef(0.10f,0.65f,0.10f); glutSolidCube(1); glPopMatrix();

    // Legs (pants color)
    glColor3f(pR, pG, pB);
    glPushMatrix(); glTranslatef(-0.12f,0.70f,0); glRotatef(-legSwing,1,0,0); glTranslatef(0,-0.45f,0); glScalef(0.12f,0.90f,0.12f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.12f,0.70f,0); glRotatef(legSwing,1,0,0); glTranslatef(0,-0.45f,0); glScalef(0.12f,0.90f,0.12f); glutSolidCube(1); glPopMatrix();

    // Shoes
    glColor3f(0.20f, 0.15f, 0.10f);
    glPushMatrix(); glTranslatef(-0.12f,0.04f,0.06f); glScalef(0.14f,0.10f,0.22f); glutSolidCube(1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.12f,0.04f,0.06f); glScalef(0.14f,0.10f,0.22f); glutSolidCube(1); glPopMatrix();

    glPopMatrix();
}

// ---------- PROPER FOOTOVER BRIDGE ----------

void drawFootBridge(float z) {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, z);

    GLUquadric* q = gluNewQuadric();

    // ===================== COLUMNS =====================
    // 4 main support columns (2 each side)
    glColor3f(0.65f, 0.65f, 0.68f);
    float colPositions[4] = {-8.0f, -6.5f, 6.5f, 8.0f};
    for (int c = 0; c < 4; c++) {
        glPushMatrix();
            glTranslatef(colPositions[c], 0.0f, 0.0f);
            glRotatef(-90, 1, 0, 0);
            gluCylinder(q, 0.28f, 0.28f, 4.0f, 16, 4);
        glPopMatrix();
        // Column base plinth
        glColor3f(0.55f, 0.55f, 0.58f);
        glPushMatrix();
            glTranslatef(colPositions[c], 0.05f, 0.0f);
            glScalef(0.80f, 0.12f, 0.80f);
            glutSolidCube(1);
        glPopMatrix();
        glColor3f(0.65f, 0.65f, 0.68f);
    }

    // ===================== MAIN DECK =====================
    // Concrete deck slab
    glColor3f(0.70f, 0.70f, 0.72f);
    glPushMatrix();
        glTranslatef(0.0f, 3.80f, 0.0f);
        glScalef(17.0f, 0.22f, 2.20f);
        glutSolidCube(1);
    glPopMatrix();

    // Deck underside beam (center longitudinal)
    glColor3f(0.58f, 0.58f, 0.60f);
    glPushMatrix();
        glTranslatef(0.0f, 3.65f, 0.0f);
        glScalef(16.8f, 0.18f, 0.40f);
        glutSolidCube(1);
    glPopMatrix();

    // Cross beams under deck
    glColor3f(0.60f, 0.60f, 0.62f);
    for (int b = -3; b <= 3; b++) {
        glPushMatrix();
            glTranslatef(b * 2.2f, 3.62f, 0.0f);
            glScalef(0.20f, 0.22f, 2.10f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // ===================== RAILINGS =====================
    // Top handrails
    glColor3f(0.88f, 0.88f, 0.90f);
    // Left rail
    glPushMatrix();
        glTranslatef(0.0f, 5.15f, 1.02f);
        glScalef(16.8f, 0.10f, 0.10f);
        glutSolidCube(1);
    glPopMatrix();
    // Right rail
    glPushMatrix();
        glTranslatef(0.0f, 5.15f, -1.02f);
        glScalef(16.8f, 0.10f, 0.10f);
        glutSolidCube(1);
    glPopMatrix();

    // Mid rails
    glColor3f(0.82f, 0.82f, 0.84f);
    glPushMatrix();
        glTranslatef(0.0f, 4.65f, 1.02f);
        glScalef(16.8f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0.0f, 4.65f, -1.02f);
        glScalef(16.8f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();

    // Vertical balusters (posts) along both sides
    glColor3f(0.75f, 0.75f, 0.78f);
    for (float bx = -8.0f; bx <= 8.0f; bx += 0.80f) {
        // Left side
        glPushMatrix();
            glTranslatef(bx, 4.55f, 1.02f);
            glScalef(0.07f, 1.30f, 0.07f);
            glutSolidCube(1);
        glPopMatrix();
        // Right side
        glPushMatrix();
            glTranslatef(bx, 4.55f, -1.02f);
            glScalef(0.07f, 1.30f, 0.07f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // ===================== ROOF / CANOPY =====================
    // Main canopy (covers pedestrians from rain)
    glColor3f(0.40f, 0.55f, 0.75f);
    glPushMatrix();
        glTranslatef(0.0f, 5.40f, 0.0f);
        glScalef(17.0f, 0.12f, 2.60f);
        glutSolidCube(1);
    glPopMatrix();

    // Canopy support posts (at column positions)
    glColor3f(0.55f, 0.60f, 0.70f);
    for (int c = 0; c < 4; c++) {
        glPushMatrix();
            glTranslatef(colPositions[c], 5.05f, 0.0f);
            glScalef(0.12f, 0.55f, 0.12f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // ===================== LEFT STAIRCASE (leads to left side of road) =====================
    // Stair flight: 8 steps going from deck height (3.9) down to ground (0)
    // Steps go from x=-8.0 outward to x=-14.5, descending
    int numSteps = 10;
    float stairStartX = -8.0f;
    float stairEndX   = -14.0f;
    float stairTopY   = 3.80f;
    float stairBotY   = 0.20f;
    float stepW = (stairEndX - stairStartX) / numSteps;
    float stepH = (stairTopY - stairBotY) / numSteps;

    for (int s = 0; s < numSteps; s++) {
        float sx = stairStartX + s * stepW;
        float sy = stairTopY - s * stepH;

        // Step tread (horizontal surface)
        glColor3f(0.68f, 0.68f, 0.70f);
        glPushMatrix();
            glTranslatef(sx + stepW*0.5f, sy - stepH*0.5f + 0.04f, 0.0f);
            glScalef(fabs(stepW), 0.08f, 2.10f);
            glutSolidCube(1);
        glPopMatrix();

        // Step riser (vertical face)
        glColor3f(0.60f, 0.60f, 0.62f);
        glPushMatrix();
            glTranslatef(sx + stepW*0.5f, sy - stepH*0.5f - stepH*0.25f, 0.0f);
            glScalef(fabs(stepW), stepH*0.5f, 2.10f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // Left staircase landing at top
    glColor3f(0.65f, 0.65f, 0.68f);
    glPushMatrix();
        glTranslatef(-8.8f, 3.82f, 0.0f);
        glScalef(1.60f, 0.12f, 2.10f);
        glutSolidCube(1);
    glPopMatrix();

    // Left stair railings
    glColor3f(0.85f, 0.85f, 0.88f);
    // Handrail bar (left stair)
    glPushMatrix();
        glTranslatef((stairStartX+stairEndX)*0.5f, (stairTopY+stairBotY)*0.5f + 0.85f, 1.10f);
        glRotatef( -(float)numSteps * atanf(stepH/fabs(stepW)) * 180.0f / PI, 0, 0, 1);
        glScalef(fabs(stairEndX-stairStartX) * 1.12f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef((stairStartX+stairEndX)*0.5f, (stairTopY+stairBotY)*0.5f + 0.85f, -1.10f);
        glRotatef( -(float)numSteps * atanf(stepH/fabs(stepW)) * 180.0f / PI, 0, 0, 1);
        glScalef(fabs(stairEndX-stairStartX) * 1.12f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();
    // Vertical stair posts (left)
    glColor3f(0.78f, 0.78f, 0.80f);
    for (int s = 0; s <= numSteps; s += 2) {
        float px = stairStartX + s * stepW;
        float py = stairTopY  - s * stepH;
        glPushMatrix(); glTranslatef(px, py + 0.45f, 1.10f); glScalef(0.07f, 0.90f, 0.07f); glutSolidCube(1); glPopMatrix();
        glPushMatrix(); glTranslatef(px, py + 0.45f,-1.10f); glScalef(0.07f, 0.90f, 0.07f); glutSolidCube(1); glPopMatrix();
    }

    // ===================== RIGHT STAIRCASE =====================
    stairStartX = 8.0f;
    stairEndX   = 14.0f;

    for (int s = 0; s < numSteps; s++) {
        float sx = stairStartX - s * stepW;
        float sy = stairBotY - s * stepH;  // ascending order from right

        glColor3f(0.68f, 0.68f, 0.70f);
        glPushMatrix();
            glTranslatef(sx + stepW*0.5f, stairTopY - s * stepH - stepH*0.5f + 0.04f, 0.0f);
            glScalef(fabs(stepW), 0.08f, 2.10f);
            glutSolidCube(1);
        glPopMatrix();

        glColor3f(0.60f, 0.60f, 0.62f);
        glPushMatrix();
            glTranslatef(sx + stepW*0.5f, stairTopY - s * stepH - stepH*0.5f - stepH*0.25f, 0.0f);
            glScalef(fabs(stepW), stepH*0.5f, 2.10f);
            glutSolidCube(1);
        glPopMatrix();
    }

    // Right stair landing at top
    glColor3f(0.65f, 0.65f, 0.68f);
    glPushMatrix();
        glTranslatef(8.8f, 3.82f, 0.0f);
        glScalef(1.60f, 0.12f, 2.10f);
        glutSolidCube(1);
    glPopMatrix();

    // Right stair railings
    glColor3f(0.85f, 0.85f, 0.88f);
    glPushMatrix();
        glTranslatef((stairStartX+stairEndX)*0.5f, (stairTopY+stairBotY)*0.5f + 0.85f, 1.10f);
        glRotatef( (float)numSteps * atanf(stepH/fabs(stepW)) * 180.0f / PI, 0, 0, 1);
        glScalef(fabs(stairEndX-stairStartX) * 1.12f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();
    glPushMatrix();
        glTranslatef((stairStartX+stairEndX)*0.5f, (stairTopY+stairBotY)*0.5f + 0.85f, -1.10f);
        glRotatef( (float)numSteps * atanf(stepH/fabs(stepW)) * 180.0f / PI, 0, 0, 1);
        glScalef(fabs(stairEndX-stairStartX) * 1.12f, 0.08f, 0.08f);
        glutSolidCube(1);
    glPopMatrix();
    glColor3f(0.78f, 0.78f, 0.80f);
    for (int s = 0; s <= numSteps; s += 2) {
        float px = stairStartX - s * stepW;
        float py = stairTopY - s * stepH;
        glPushMatrix(); glTranslatef(px, py + 0.45f, 1.10f); glScalef(0.07f, 0.90f, 0.07f); glutSolidCube(1); glPopMatrix();
        glPushMatrix(); glTranslatef(px, py + 0.45f,-1.10f); glScalef(0.07f, 0.90f, 0.07f); glutSolidCube(1); glPopMatrix();
    }

    // ===================== PEDESTRIANS ON BRIDGE =====================
    for (int i = 0; i < 5; i++) {
        float px = -5.5f + fmodf(animTime*(1.2f+0.15f*i)+i*2.2f, 11.0f);
        float pz = (i%2==0) ? -0.30f : 0.30f;
        drawPedestrian(px, 3.90f, pz, animTime*6.0f+i, i);
    }

    gluDeleteQuadric(q);
    glPopMatrix();
}

// ---------- SIDE ROAD BRANCH ----------
// FIX: Removed footpath/sidewalk strips on the connecting zone near the main road.
// The road asphalt now flows cleanly into the main road without any footpath interruption.
void drawSideRoadBranch(float z, bool leftSide) {
    float z1 = z - 5.5f;
    float z2 = z + 5.5f;

    // Main side road asphalt
    glColor3f(0.18f, 0.18f, 0.18f);
    glBegin(GL_QUADS);
    if (leftSide) {
        glVertex3f(-22.0f,     0.01f, z1);
        glVertex3f(ROAD_LEFT,  0.01f, z1);
        glVertex3f(ROAD_LEFT,  0.01f, z2);
        glVertex3f(-22.0f,     0.01f, z2);
    } else {
        glVertex3f(ROAD_RIGHT, 0.01f, z1);
        glVertex3f(22.0f,      0.01f, z1);
        glVertex3f(22.0f,      0.01f, z2);
        glVertex3f(ROAD_RIGHT, 0.01f, z2);
    }
    glEnd();

    // Side road center dashed line
    glColor3f(1.0f, 1.0f, 0.0f);
    if (leftSide) {
        for (float x=-20.5f; x<ROAD_LEFT-2.5f; x+=4.0f) {
            glBegin(GL_QUADS);
                glVertex3f(x,      0.04f, z-0.15f);
                glVertex3f(x+2.2f, 0.04f, z-0.15f);
                glVertex3f(x+2.2f, 0.04f, z+0.15f);
                glVertex3f(x,      0.04f, z+0.15f);
            glEnd();
        }
    } else {
        for (float x=ROAD_RIGHT+2.5f; x<20.5f; x+=4.0f) {
            glBegin(GL_QUADS);
                glVertex3f(x,      0.04f, z-0.15f);
                glVertex3f(x+2.2f, 0.04f, z-0.15f);
                glVertex3f(x+2.2f, 0.04f, z+0.15f);
                glVertex3f(x,      0.04f, z+0.15f);
            glEnd();
        }
    }

    // Stop line at junction (only on the outer road, not blocking the main road)
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    if (leftSide) {
        glVertex3f(ROAD_LEFT-3.50f, 0.05f, z1+0.5f);
        glVertex3f(ROAD_LEFT-3.20f, 0.05f, z1+0.5f);
        glVertex3f(ROAD_LEFT-3.20f, 0.05f, z2-0.5f);
        glVertex3f(ROAD_LEFT-3.50f, 0.05f, z2-0.5f);
    } else {
        glVertex3f(ROAD_RIGHT+3.20f, 0.05f, z1+0.5f);
        glVertex3f(ROAD_RIGHT+3.50f, 0.05f, z1+0.5f);
        glVertex3f(ROAD_RIGHT+3.50f, 0.05f, z2-0.5f);
        glVertex3f(ROAD_RIGHT+3.20f, 0.05f, z2-0.5f);
    }
    glEnd();

    // ===================== SIDEWALKS ONLY BEYOND THE JUNCTION ZONE =====================
    glColor3f(0.72f, 0.72f, 0.70f);
    if (leftSide) {
        // Sidewalks only start from x=-8 outward (not near main road)
        glBegin(GL_QUADS);
            glVertex3f(-22.0f, 0.06f, z2+0.20f); glVertex3f(-8.5f, 0.06f, z2+0.20f);
            glVertex3f(-8.5f,  0.06f, z2+2.20f); glVertex3f(-22.0f,0.06f, z2+2.20f);
        glEnd();
        glBegin(GL_QUADS);
            glVertex3f(-22.0f, 0.06f, z1-2.20f); glVertex3f(-8.5f, 0.06f, z1-2.20f);
            glVertex3f(-8.5f,  0.06f, z1-0.20f); glVertex3f(-22.0f,0.06f, z1-0.20f);
        glEnd();
    } else {
        glBegin(GL_QUADS);
            glVertex3f(8.5f, 0.06f, z2+0.20f);  glVertex3f(22.0f, 0.06f, z2+0.20f);
            glVertex3f(22.0f,0.06f, z2+2.20f);  glVertex3f(8.5f,  0.06f, z2+2.20f);
        glEnd();
        glBegin(GL_QUADS);
            glVertex3f(8.5f, 0.06f, z1-2.20f);  glVertex3f(22.0f, 0.06f, z1-2.20f);
            glVertex3f(22.0f,0.06f, z1-0.20f);  glVertex3f(8.5f,  0.06f, z1-0.20f);
        glEnd();
    }
}

// ---------- ROAD ----------
void drawRoad() {
    glColor3f(0.20f, 0.20f, 0.20f);
    glBegin(GL_QUADS);
        glVertex3f(ROAD_LEFT,  0.0f,   8.0f);
        glVertex3f(ROAD_RIGHT, 0.0f,   8.0f);
        glVertex3f(ROAD_RIGHT, 0.0f, -230.0f);
        glVertex3f(ROAD_LEFT,  0.0f, -230.0f);
    glEnd();

    glBegin(GL_QUADS);
        glVertex3f(ROAD_LEFT,  0.0f,  50.0f);
        glVertex3f(ROAD_RIGHT, 0.0f,  50.0f);
        glVertex3f(ROAD_RIGHT, 0.0f,   8.0f);
        glVertex3f(ROAD_LEFT,  0.0f,   8.0f);
    glEnd();

    // Center dashes
    glColor3f(1.0f, 1.0f, 0.0f);
    float dashOffset = fmodf(worldScroll * ROAD_PARALLAX, 6.0f);
    for (float z=8.0f+dashOffset; z>-230.0f; z-=6.0f) {
        glBegin(GL_QUADS);
            glVertex3f(-0.07f,0.03f,z); glVertex3f(0.07f,0.03f,z);
            glVertex3f(0.07f,0.03f,z-3.0f); glVertex3f(-0.07f,0.03f,z-3.0f);
        glEnd();
    }

    // Side lines
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
        glVertex3f(ROAD_LEFT+0.05f,0.03f,8.0f); glVertex3f(ROAD_LEFT+0.25f,0.03f,8.0f);
        glVertex3f(ROAD_LEFT+0.25f,0.03f,-230.0f); glVertex3f(ROAD_LEFT+0.05f,0.03f,-230.0f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex3f(ROAD_RIGHT-0.25f,0.03f,8.0f); glVertex3f(ROAD_RIGHT-0.05f,0.03f,8.0f);
        glVertex3f(ROAD_RIGHT-0.05f,0.03f,-230.0f); glVertex3f(ROAD_RIGHT-0.25f,0.03f,-230.0f);
    glEnd();

    for (int i=0; i<5; i++) {
        float z = getSideRoadZ(i, true);
        if (z < 10.0f && z > -220.0f) drawSideRoadBranch(z, true);
    }
    for (int i=0; i<5; i++) {
        float z = getSideRoadZ(i, false);
        if (z < 10.0f && z > -220.0f) drawSideRoadBranch(z, false);
    }
}

// ---------- GROUND ----------
void drawGround() {
    glColor3f(0.12f, 0.55f, 0.12f);
    glBegin(GL_QUADS);
        glVertex3f(-30.0f,0.0f,50.0f); glVertex3f(-4.0f,0.0f,50.0f);
        glVertex3f(-4.0f,0.0f,-230.0f); glVertex3f(-30.0f,0.0f,-230.0f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex3f(4.0f,0.0f,50.0f); glVertex3f(30.0f,0.0f,50.0f);
        glVertex3f(30.0f,0.0f,-230.0f); glVertex3f(4.0f,0.0f,-230.0f);
    glEnd();

    glColor3f(0.72f, 0.72f, 0.70f);
    glBegin(GL_QUADS);
        glVertex3f(-6.0f,0.06f,50.0f); glVertex3f(-4.0f,0.06f,50.0f);
        glVertex3f(-4.0f,0.06f,-230.0f); glVertex3f(-6.0f,0.06f,-230.0f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex3f(4.0f,0.06f,50.0f); glVertex3f(6.0f,0.06f,50.0f);
        glVertex3f(6.0f,0.06f,-230.0f); glVertex3f(4.0f,0.06f,-230.0f);
    glEnd();

    glColor3f(0.58f, 0.58f, 0.56f);
    float lineOffset = fmodf(worldScroll * ROAD_PARALLAX, 2.0f);
    for (float z=8.0f+lineOffset; z>-230.0f; z-=2.0f) {
        glBegin(GL_QUADS);
            glVertex3f(-6.0f,0.075f,z); glVertex3f(-4.0f,0.075f,z);
            glVertex3f(-4.0f,0.075f,z-0.05f); glVertex3f(-6.0f,0.075f,z-0.05f);
        glEnd();
        glBegin(GL_QUADS);
            glVertex3f(4.0f,0.075f,z); glVertex3f(6.0f,0.075f,z);
            glVertex3f(6.0f,0.075f,z-0.05f); glVertex3f(4.0f,0.075f,z-0.05f);
        glEnd();
    }
}

// ---------- ENVIRONMENT WITH PARALLAX ----------
void drawEnvironment() {
    const float spacing = 12.0f;
    const float cycle = NUM_OBJECTS * spacing;

    for (int i=0; i<NUM_OBJECTS; i++) {
        float treeZ = wrapZ(-i*spacing + fmodf(worldScroll*TREE_PARALLAX, cycle), cycle);
        float buildingZ = wrapZ(-i*spacing + fmodf(worldScroll*BUILDING_PARALLAX, cycle), cycle);
        float farZ = wrapZ(-i*spacing + fmodf(worldScroll*FAR_PARALLAX, cycle), cycle);

        if (!isNearSideRoadGap(treeZ, true)) {
            drawTree(-6.6f, treeZ-1.0f);
            if (i%2==0) drawTree(-6.0f, treeZ-5.0f);
        }
        if (!isNearSideRoadGap(treeZ, false)) {
            drawTree(6.6f, treeZ-1.0f);
            if (i%2==0) drawTree(6.0f, treeZ-5.0f);
        }
        if (!isNearSideRoadGap(buildingZ, true))
            drawBuilding(-12.0f, buildingZ, buildingHeights[i], false, i);
        if (!isNearSideRoadGap(buildingZ, false))
            drawBuilding(12.0f, buildingZ, buildingHeights[i], true, i);

        if (i%2==0) {
            glPushMatrix(); glTranslatef(-22.0f,0,farZ); glScalef(0.75f,0.75f,0.75f);
                drawBuilding(0,0,buildingHeights[i]+3.0f,false,i); glPopMatrix();
            glPushMatrix(); glTranslatef(22.0f,0,farZ); glScalef(0.75f,0.75f,0.75f);
                drawBuilding(0,0,buildingHeights[i]+3.0f,true,i); glPopMatrix();
        }
    }

    for (int i=0; i<8; i++) {
        int idx = i % NUM_OBJECTS;
        float backZ = 15.0f + i * 12.0f;
        drawTree(-6.6f, backZ);
        drawTree( 6.6f, backZ);
        drawBuilding(-12.0f, backZ, buildingHeights[idx], false, idx);
        drawBuilding( 12.0f, backZ, buildingHeights[idx], true,  idx);
    }

    for (int i=0; i<3; i++) {
        float z = wrapZ(-45.0f - i*75.0f + fmodf(worldScroll*BRIDGE_PARALLAX, 225.0f), 225.0f);
        if (z < 5.0f && z > -210.0f) drawFootBridge(z);
    }
}

// ---------- TEXT ----------
void drawText(float x, float y, const char* string) {
    glRasterPos2f(x, y);
    for (int i=0; string[i]!='\0'; i++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, string[i]);
}

// ---------- DISPLAY ----------
void display() {
    updateEnvironmentLighting();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (cameraMode == 0) {
        gluLookAt(playerX, 6, 10, playerX, 0, -12, 0, 1, 0);
    }
    else if (cameraMode == 1) {
        gluLookAt(playerX, 6.0f, -8.0f, playerX, 0.5f, 30.0f, 0, 1, 0);
    }
    else if (cameraMode == 2) {
        gluLookAt(playerX+10, 5, 2, playerX, 0, -10, 0, 1, 0);
    }
    else if (cameraMode == 3) {
        float camDist = 12.0f;
        float camHeight = 5.0f;
        float rad = rotationAngle * PI / 180.0f;
        float camX = playerX - sinf(rad) * camDist;
        float camZ = 2.0f + cosf(rad) * camDist;
        gluLookAt(camX, camHeight, camZ, playerX, 1.0f, 2.0f, 0, 1, 0);
    }

    drawSky();
    drawGround();
    drawRoad();
    drawEnvironment();

    // Player car (faces forward, mirrorFront=true: headlights at negative Z)
    drawRealisticCar(playerX, 2.0f, 0.95f,0.95f,0.95f, 0.20f,0.40f,0.90f, true);

    // Enemy cars - FIX: also mirrorFront=true so they face same direction as player
    for (int i=0; i<MAX_ENEMIES; i++) {
        drawRealisticCar(
            enemyX[i], enemyZ[i],
            ENEMY_PALETTE[i][0], ENEMY_PALETTE[i][1], ENEMY_PALETTE[i][2],
            ENEMY_PALETTE[i][3], ENEMY_PALETTE[i][4], ENEMY_PALETTE[i][5],
            true  // <-- SAME facing as player
        );
    }

    // Motorcycles (rotation applied inside drawMotorcycle)
    for (int i=0; i<MAX_BIKES; i++)
        drawMotorcycle(bikeX[i], bikeZ[i]);

    // HUD
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 900, 0, 650);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);

    char text[160];
    sprintf(text, "Score: %d   Environment: %s   Controls: A/D Move, R Restart, 1/2/3/4 Camera",
            score, getEnvironmentName());
    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(10, 625, text);

    const char* camLabel = "";
    if (cameraMode==0) camLabel = "Camera: Back View [1]";
    else if (cameraMode==1) camLabel = "Camera: Front View [2]";
    else if (cameraMode==2) camLabel = "Camera: Side View [3]";
    else if (cameraMode==3) camLabel = "Camera: 360 Rotating View [4]";
    glColor3f(0.9f, 0.9f, 0.2f);
    drawText(10, 600, camLabel);

    if (gameOver) {
        glColor3f(1.0f, 0.2f, 0.2f);
        drawText(390, 330, "GAME OVER");
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText(350, 295, "Press R to Restart");
    }

    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// ---------- COLLISION ----------
bool checkCollision(int i) {
    return (fabs(playerX-enemyX[i]) < 1.8f && fabs(enemyZ[i]-2.0f) < 2.2f);
}

// ---------- UPDATE ----------
void update(int value) {
    dayTime += DAY_CYCLE_SPEED;
    if (dayTime >= 1.0f) dayTime = 0.0f;

    if (cameraMode == 3) {
        rotationAngle += 1.2f;
        if (rotationAngle >= 360.0f) rotationAngle = 0.0f;
    }

    if (!gameOver) {
        worldScroll += WORLD_SPEED;
        animTime += 0.06f;
        if (worldScroll > 10000.0f) worldScroll = 0.0f;

        for (int i=0; i<MAX_ENEMIES; i++) {
            float speed = 0.32f + i*0.07f;
            enemyZ[i] += speed;
            if (enemyZ[i] > 12.0f) {
                enemyZ[i] = -90.0f - (rand()%60);
                enemyX[i] = (rand()%3-1)*2.0f;
                score++;
            }
            if (checkCollision(i)) { gameOver = true; break; }
        }

        for (int i=0; i<MAX_BIKES; i++) {
            bikeZ[i] += 0.28f;
            if (bikeZ[i] > 12.0f) {
                bikeZ[i] = -90.0f - (rand()%60);
                bikeX[i] = (rand()%2==0 ? -2.0f : 2.0f);
                score++;
            }
            if (fabs(playerX-bikeX[i]) < 1.5f && fabs(bikeZ[i]-2.0f) < 2.0f)
                gameOver = true;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// ---------- KEYBOARD ----------
void keyboard(unsigned char key, int x, int y) {
    if (!gameOver) {
        if (key=='a' || key=='A') playerX -= 2.0f;
        if (key=='d' || key=='D') playerX += 2.0f;
        if (playerX < -2.0f) playerX = -2.0f;
        if (playerX >  2.0f) playerX =  2.0f;
    }

    if (key=='r' || key=='R') {
        playerX = 0.0f; worldScroll = 0.0f; animTime = 0.0f;
        score = 0; gameOver = false;
        for (int i=0; i<MAX_BIKES; i++) {
            bikeX[i] = (i==0 ? -2.0f : 2.0f);
            bikeZ[i] = -50.0f - i*30.0f;
        }
        for (int i=0; i<MAX_ENEMIES; i++) {
            enemyX[i] = (i==0 ? -2.0f : 2.0f);
            enemyZ[i] = -35.0f - i*30.0f;
        }
    }

    if (key=='1') { cameraMode = 0; }
    if (key=='2') { cameraMode = 1; }
    if (key=='3') { cameraMode = 2; }
    if (key=='4') { cameraMode = 3; rotationAngle = 0.0f; }
}

// ---------- RESHAPE ----------
void reshape(int w, int h) {
    if (h==0) h=1;
    float ratio = 1.0f*w/h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, ratio, 1.0f, 400.0f);
    glViewport(0,0,w,h);
    glMatrixMode(GL_MODELVIEW);
}

// ---------- INIT ----------
void init() {
    srand((unsigned int)time(NULL));
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.18f, 0.48f, 0.82f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    float light_pos[] = {5.0f,20.0f,10.0f,1.0f};
    float ambient[]   = {0.35f,0.35f,0.35f,1.0f};
    float diffuse[]   = {0.85f,0.85f,0.85f,1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_FOG);

    int numPalette = sizeof(WALL_PALETTE)/sizeof(WALL_PALETTE[0]);
    for (int i=0; i<NUM_OBJECTS; i++) {
        buildingHeights[i] = 3 + rand()%5;
        int wi = i % numPalette;
        buildingColors[i][0] = WALL_PALETTE[wi][0];
        buildingColors[i][1] = WALL_PALETTE[wi][1];
        buildingColors[i][2] = WALL_PALETTE[wi][2];
        buildingColors[i][3] = WIN_PALETTE[wi][0];
        buildingColors[i][4] = WIN_PALETTE[wi][1];
        buildingColors[i][5] = WIN_PALETTE[wi][2];
    }

    for (int i=0; i<MAX_ENEMIES; i++) {
        enemyX[i] = (i-1)*2.0f;
        enemyZ[i] = -35.0f - i*30.0f;
    }
    for (int i=0; i<MAX_BIKES; i++) {
        bikeX[i] = (i==0 ? -2.0f : 2.0f);
        bikeZ[i] = -50.0f - i*30.0f;
    }
}

// ---------- MAIN ----------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(900, 650);
    glutCreateWindow("3D Car Game - Day Night Cycle");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);
    glutTimerFunc(0, update, 0);
    glutMainLoop();
    return 0;
}
