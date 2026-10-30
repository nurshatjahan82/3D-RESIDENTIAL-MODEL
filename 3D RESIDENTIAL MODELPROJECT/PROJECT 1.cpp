/*
    3D RESIDENTIAL AREA - Full Scene
    Course: Computer Graphics Lab Project

    Features:
      - Cross-shaped road network (main road + cross road) with sidewalks
      - Apartment blocks filling all four quadrants, dense, minimal empty space
      - Colorful, varied building facades (10-color palette) with balconies,
        rooftop water tanks / AC units, and front + back windows
      - A pond (with reed bushes, now doubling as the hospital's garden
        pond) and a park (with hedges + playground), both directly beside
        the main road
      - A school building in the open ground behind the park: dark and
        closed at night except for a glowing "SCHOOL" sign
      - A hospital building in the open ground behind the pond, flanked
        by a departmental store and a mosque with twin minarets, all
        linked by a paved civic plaza so there's no bare ground between
        them. Hospital stays mostly lit at night (staffed 24/7), the
        store's storefront sign glows all night, and the mosque's dome
        and minaret tips glow softly after dark.
      - Trees + hedges lining the road network and park boundary
      - Two small parking lots with painted lines and parked cars
      - Multiple cars, all moving, at a realistic slow speed, with glowing
        headlights (front) and taillights (rear) at night
      - Walking people (adults on sidewalks, children in the park)
      - Sun position controlled by the MOUSE CURSOR (move mouse to change time of day;
        night is reached comfortably in the lower ~40% of the window, with a wide dusk band)
      - Street lamps automatically glow when the sun is low / down
      - Building windows randomly light up at night (stable per-window pattern)
      - Cars keep a safe following gap in the same lane so they never stack or look
        like they are about to crash; school bus & ambulance also yield to traffic
      - School bus stays around the school: parks at the driveway, does a short
        local-street loop to pick up students at two stops, then returns; students
        near the stops board when the bus pauses and reappear at the school
      - Fixed: GL_COLOR_MATERIAL is now enabled, so glColor3f() actually
        controls how lit surfaces look (previously everything rendered with
        OpenGL's flat default grey material under lighting)
      - Fixed: the school bus and the ambulance now travel only along the
        roads that are actually drawn (local streets, the main vertical
        road, and the main cross road). The only off-road moments are the
        short driveway spurs right at the destination - the school's front
        door for the bus, the hospital's entrance for the ambulance -
        exactly like a real vehicle pulling off the street. Both vehicles
        also watch for regular traffic sharing their lane and ease off
        instead of driving through a car.

    Controls:
      Left/Right Arrow  -> Rotate camera around scene
      Up/Down Arrow     -> Zoom in / out
      Mouse move        -> Move the sun (left/right = time of day, up/down = height)
      'l' key           -> Toggle overall OpenGL lighting ON/OFF
      'w' key           -> Toggle wireframe/solid mode
      'a' key           -> Toggle car/people animation ON/OFF
      ESC               -> Exit
*/

#include <GL/freeglut.h>
#include <cmath>

#define PI 3.14159265f

// ---------- Camera control ----------
float camAngle = 0.4f;
float camDistance = 100.0f;
bool lightingOn = true;
bool wireframeMode = false;
bool animateScene = true;

// ---------- Mouse-controlled sun ----------
float mouseXNorm = 0.5f;   // 0..1 across window width
float mouseYNorm = 0.25f;  // 0..1 across window height (lower = sun lower)
bool  isNight = false;
float sunX, sunY, sunZ;
float skyColor[3] = { 0.75f, 0.88f, 0.95f };

// ---------- People ----------
struct PersonState {
    float x, z;
    float minZ, maxZ;
    float dir;
    float speed;
    float scale;
    float r, g, b;
};
const int NUM_PEOPLE = 6;
PersonState people[NUM_PEOPLE];

// Students who wait near the school bus stops and board when the bus pauses
struct StudentState {
    float x, z;           // current position
    float waitX, waitZ;   // bus-stop waiting spot
    float roamX, roamZ;   // idle roam centre (near school / park)
    float phase;          // 0..1 walk cycle between roam and stop
    float speed;
    float scale;
    float r, g, b;
    bool boarded;         // true = inside the bus (not drawn)
    float boardTimer;     // time left boarded before they reappear at school
};
const int NUM_STUDENTS = 5;
StudentState students[NUM_STUDENTS];

// ---------- Cars ----------
struct CarState {
    float x, z;
    char axis;       // 'z' = moves along Z (fixed x), 'x' = moves along X (fixed z)
    float minC, maxC;
    float dir;        // +1 or -1, direction of travel (constant, cars loop around)
    float speed;
    float r, g, b;
};
const int NUM_CARS = 8;
CarState cars[NUM_CARS];

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------
// Small helpers: emissive "glow" material + a stable
// pseudo-random value (used to decide which windows are lit)
// ---------------------------------------------------------
void setEmission(float r, float g, float b) {
    GLfloat e[] = { r, g, b, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
}
void clearEmission() {
    GLfloat e[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
}
// Deterministic 0..1 pseudo-random from three integers, so the same
// window/building always gets the same lit/unlit state (no flicker).
float hash01(int a, int b, int c) {
    unsigned int n = (unsigned int)(a * 73856093) ^ (unsigned int)(b * 19349663) ^ (unsigned int)(c * 83492791);
    n = n * 2654435761u;
    n ^= (n >> 13);
    n = n * 2246822519u;
    return (float)(n & 0x00FFFFFFu) / (float)0x01000000u;
}

// ---------------------------------------------------------
// Ground
// ---------------------------------------------------------
void drawGround() {
    glColor3f(0.66f, 0.85f, 0.56f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-70, 0, -70);
    glVertex3f(-70, 0, 70);
    glVertex3f(70, 0, 70);
    glVertex3f(70, 0, -70);
    glEnd();
}

// ---------------------------------------------------------
// Axis-aligned street segment
// ---------------------------------------------------------
void drawStreet(float x1, float z1, float x2, float z2, float halfWidth) {
    glColor3f(0.68f, 0.68f, 0.7f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    if (fabs(x1 - x2) > fabs(z1 - z2)) {
        glVertex3f(x1, 0.01f, z1 - halfWidth);
        glVertex3f(x1, 0.01f, z1 + halfWidth);
        glVertex3f(x2, 0.01f, z2 + halfWidth);
        glVertex3f(x2, 0.01f, z2 - halfWidth);
    }
    else {
        glVertex3f(x1 - halfWidth, 0.01f, z1);
        glVertex3f(x1 + halfWidth, 0.01f, z1);
        glVertex3f(x2 + halfWidth, 0.01f, z2);
        glVertex3f(x2 - halfWidth, 0.01f, z2);
    }
    glEnd();
}

void drawMainRoadMarkings() {
    glColor3f(1, 1, 1);
    for (float z = -63; z < 63; z += 6) {
        glBegin(GL_QUADS);
        glVertex3f(-0.15f, 0.02f, z);
        glVertex3f(-0.15f, 0.02f, z + 3);
        glVertex3f(0.15f, 0.02f, z + 3);
        glVertex3f(0.15f, 0.02f, z);
        glEnd();
    }
    for (float x = -63; x < 63; x += 6) {
        glBegin(GL_QUADS);
        glVertex3f(x, 0.02f, -0.15f);
        glVertex3f(x + 3, 0.02f, -0.15f);
        glVertex3f(x + 3, 0.02f, 0.15f);
        glVertex3f(x, 0.02f, 0.15f);
        glEnd();
    }
}

// ---------------------------------------------------------
// Sidewalks running alongside both roads
// ---------------------------------------------------------
void drawSidewalks() {
    glColor3f(0.8f, 0.8f, 0.78f);
    // Vertical main road, both sides
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-6.5f, 0.015f, -65); glVertex3f(-6.5f, 0.015f, 65);
    glVertex3f(-5.0f, 0.015f, 65); glVertex3f(-5.0f, 0.015f, -65);
    glEnd();
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(5.0f, 0.015f, -65); glVertex3f(5.0f, 0.015f, 65);
    glVertex3f(6.5f, 0.015f, 65); glVertex3f(6.5f, 0.015f, -65);
    glEnd();
    // Horizontal cross road, both sides
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-65, 0.015f, -6.5f); glVertex3f(65, 0.015f, -6.5f);
    glVertex3f(65, 0.015f, -5.0f); glVertex3f(-65, 0.015f, -5.0f);
    glEnd();
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-65, 0.015f, 5.0f); glVertex3f(65, 0.015f, 5.0f);
    glVertex3f(65, 0.015f, 6.5f); glVertex3f(-65, 0.015f, 6.5f);
    glEnd();
}

// ---------------------------------------------------------
// Lamp post - glows automatically at night
// ---------------------------------------------------------
void drawLampPost() {
    glPushMatrix();
    glColor3f(0.3f, 0.3f, 0.3f);
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, 0.1f, 0.1f, 4.0f, 8, 8);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 4.0f, 0);
    if (isNight) {
        glColor3f(1.0f, 0.95f, 0.55f);
        setEmission(0.9f, 0.8f, 0.35f);
        glutSolidSphere(0.28, 10, 10);
        clearEmission();

        // Soft glow halo (translucent, doesn't write depth so it
        // doesn't occlude anything behind it)
        glDepthMask(GL_FALSE);
        glColor4f(1.0f, 0.9f, 0.5f, 0.25f);
        glutSolidSphere(0.65, 10, 10);
        glDepthMask(GL_TRUE);
    }
    else {
        glColor3f(0.75f, 0.75f, 0.65f);
        glutSolidSphere(0.28, 10, 10);
    }
    glPopMatrix();
    glPopMatrix();
}

// ---------------------------------------------------------
// Tree
// ---------------------------------------------------------
void drawTree() {
    glColor3f(0.6f, 0.42f, 0.25f);
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, 0.2f, 0.2f, 1.5f, 8, 8);
    glPopMatrix();

    glColor3f(0.35f, 0.65f, 0.3f);
    glPushMatrix();
    glTranslatef(0, 1.5f, 0);
    glRotatef(-90, 1, 0, 0);
    glutSolidCone(0.9, 1.5, 10, 10);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 2.3f, 0);
    glRotatef(-90, 1, 0, 0);
    glutSolidCone(0.7, 1.3, 10, 10);
    glPopMatrix();
}

// ---------------------------------------------------------
// Bush / hedge unit
// ---------------------------------------------------------
void drawBush() {
    glColor3f(0.3f, 0.55f, 0.25f);
    glPushMatrix();
    glScalef(1.0f, 0.6f, 1.0f);
    glutSolidSphere(0.5, 10, 10);
    glPopMatrix();
}

// ---------------------------------------------------------
// Car (now with headlights + taillights)
// ---------------------------------------------------------
void drawCarModel(float bodyR, float bodyG, float bodyB, bool night) {
    glPushMatrix();
    glColor3f(bodyR, bodyG, bodyB);
    glPushMatrix();
    glScalef(1.2f, 0.5f, 2.2f);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(bodyR * 0.85f + 0.1f, bodyG * 0.85f + 0.1f, bodyB * 0.85f + 0.1f);
    glPushMatrix();
    glTranslatef(0, 0.45f, -0.1f);
    glScalef(0.9f, 0.4f, 1.1f);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(0.2f, 0.2f, 0.2f);
    float wx = 0.65f, wz = 0.75f, wy = -0.25f;
    float positions[4][2] = { {wx, wz}, {-wx, wz}, {wx, -wz}, {-wx, -wz} };
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glTranslatef(positions[i][0], wy, positions[i][1]);
        glRotatef(90, 0, 1, 0);
        GLUquadric* quad = gluNewQuadric();
        gluCylinder(quad, 0.2f, 0.2f, 0.2f, 10, 10);
        glPopMatrix();
    }

    // Headlights - front of car, local +Z
    float headX = 0.45f, headZ = 1.05f, headY = -0.02f;
    if (night) { glColor3f(1.0f, 1.0f, 0.9f); setEmission(1.0f, 0.95f, 0.65f); }
    else glColor3f(0.85f, 0.85f, 0.8f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * headX, headY, headZ);
        glutSolidSphere(0.09, 8, 8);
        glPopMatrix();
    }
    if (night) clearEmission();

    // Headlight glow cone (only at night, translucent, no depth write)
    if (night) {
        glDepthMask(GL_FALSE);
        glColor4f(1.0f, 0.95f, 0.7f, 0.3f);
        for (int s = -1; s <= 1; s += 2) {
            glPushMatrix();
            glTranslatef(s * headX, headY, headZ + 0.35f);
            glScalef(1.0f, 1.0f, 1.6f);
            glutSolidSphere(0.14, 8, 8);
            glPopMatrix();
        }
        glDepthMask(GL_TRUE);
    }

    // Taillights - rear of car, local -Z (dim red always, bright glow at night)
    if (night) { glColor3f(1.0f, 0.15f, 0.1f); setEmission(0.7f, 0.05f, 0.05f); }
    else glColor3f(0.55f, 0.1f, 0.1f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * headX, headY, -headZ);
        glutSolidSphere(0.08, 8, 8);
        glPopMatrix();
    }
    if (night) clearEmission();

    glPopMatrix();
}

// ---------------------------------------------------------
// School bus - classic yellow. Used both for the parked
// drop-off row and for the animated bus that runs its route.
// ---------------------------------------------------------
void drawSchoolBus(bool night) {
    glPushMatrix();

    // Body - classic yellow
    glColor3f(0.95f, 0.75f, 0.15f);
    glPushMatrix();
    glScalef(2.0f, 1.1f, 4.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Roof band
    glColor3f(0.85f, 0.65f, 0.1f);
    glPushMatrix();
    glTranslatef(0, 0.62f, 0);
    glScalef(1.9f, 0.35f, 4.2f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Black side stripes (top and bottom of the window band)
    glColor3f(0.15f, 0.15f, 0.15f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 1.01f, 0.55f, 0);
        glScalef(0.02f, 0.08f, 4.3f);
        glutSolidCube(1.0);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(s * 1.01f, 0.05f, 0);
        glScalef(0.02f, 0.08f, 4.3f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    // Window band along each side
    glColor3f(0.55f, 0.7f, 0.8f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 1.005f, 0.3f, 0);
        glScalef(0.015f, 0.4f, 3.8f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    // Wheels - front and rear axle
    glColor3f(0.15f, 0.15f, 0.15f);
    float wzPositions[2] = { 1.6f, -1.6f };
    for (int i = 0; i < 2; i++) {
        for (int s = -1; s <= 1; s += 2) {
            glPushMatrix();
            glTranslatef(s * 1.05f, -0.55f, wzPositions[i]);
            glRotatef(90, 0, 1, 0);
            GLUquadric* wq = gluNewQuadric();
            gluCylinder(wq, 0.32f, 0.32f, 0.25f, 10, 10);
            glPopMatrix();
        }
    }

    // Headlights (front, +Z) and taillights (rear, -Z)
    float headZ = 2.15f;
    if (night) { glColor3f(1.0f, 1.0f, 0.9f); setEmission(1.0f, 0.95f, 0.65f); }
    else glColor3f(0.85f, 0.85f, 0.8f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 0.7f, -0.2f, headZ);
        glutSolidSphere(0.12, 8, 8);
        glPopMatrix();
    }
    if (night) clearEmission();

    if (night) { glColor3f(1.0f, 0.15f, 0.1f); setEmission(0.7f, 0.05f, 0.05f); }
    else glColor3f(0.55f, 0.1f, 0.1f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 0.7f, -0.2f, -headZ);
        glutSolidSphere(0.1, 8, 8);
        glPopMatrix();
    }
    if (night) clearEmission();

    glPopMatrix();
}

// ---------------------------------------------------------
// Ambulance - reuses the car body, adds a red cross decal and
// a flashing red/blue siren light bar on the roof.
// ---------------------------------------------------------
void drawAmbulance(float sirenPhase) {
    glPushMatrix();
    drawCarModel(0.95f, 0.95f, 0.95f, isNight);

    // Red cross decal on the side
    glColor3f(0.85f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(0.61f, 0.15f, 0.0f);
    glScalef(0.02f, 0.32f, 0.09f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.61f, 0.15f, 0.0f);
    glScalef(0.02f, 0.09f, 0.32f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Flashing siren light bar - alternates red / blue
    bool redOn = sinf(sirenPhase) > 0.0f;
    glPushMatrix();
    glTranslatef(-0.25f, 0.68f, 0.0f);
    if (redOn) { glColor3f(1.0f, 0.1f, 0.1f); setEmission(0.9f, 0.05f, 0.05f); }
    else glColor3f(0.4f, 0.05f, 0.05f);
    glutSolidSphere(0.09, 8, 8);
    if (redOn) clearEmission();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.25f, 0.68f, 0.0f);
    if (!redOn) { glColor3f(0.1f, 0.3f, 1.0f); setEmission(0.05f, 0.2f, 0.9f); }
    else glColor3f(0.05f, 0.1f, 0.4f);
    glutSolidSphere(0.09, 8, 8);
    if (!redOn) clearEmission();
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// Person
// ---------------------------------------------------------
void drawPerson(float scale, float shirtR, float shirtG, float shirtB) {
    glPushMatrix();
    glScalef(scale, scale, scale);

    glColor3f(0.25f, 0.25f, 0.35f);
    glPushMatrix();
    glTranslatef(0, 0.45f, 0);
    glScalef(0.35f, 0.9f, 0.35f);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(shirtR, shirtG, shirtB);
    glPushMatrix();
    glTranslatef(0, 1.1f, 0);
    glScalef(0.45f, 0.6f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(0.94f, 0.8f, 0.65f);
    glPushMatrix();
    glTranslatef(0, 1.6f, 0);
    glutSolidSphere(0.2, 10, 10);
    glPopMatrix();
    glPopMatrix();
}

// ---------------------------------------------------------
// Apartment building - now with balconies, rooftop details,
// front+back windows that light up at night, and a door light
// ---------------------------------------------------------
void drawApartment(float w, float d, int floors, float r, float g, float b, int buildingId) {
    float floorHeight = 1.5f;
    float totalHeight = floors * floorHeight;

    glColor3f(r, g, b);
    glPushMatrix();
    glTranslatef(0, totalHeight / 2.0f, 0);
    glScalef(w, totalHeight, d);
    glutSolidCube(1.0);
    glPopMatrix();

    float rr = r + 0.15f; if (rr > 1) rr = 1;
    float gg = g + 0.15f; if (gg > 1) gg = 1;
    float bb = b + 0.15f; if (bb > 1) bb = 1;
    glColor3f(rr, gg, bb);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.1f, 0);
    glScalef(w + 0.3f, 0.2f, d + 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Rooftop water tank (cylinder + conical cap)
    glColor3f(0.55f, 0.55f, 0.58f);
    glPushMatrix();
    glTranslatef(w * 0.28f, totalHeight + 0.25f, d * 0.28f);
    glRotatef(-90, 1, 0, 0);
    GLUquadric* tankQuad = gluNewQuadric();
    gluCylinder(tankQuad, 0.35f, 0.35f, 0.7f, 10, 10);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(w * 0.28f, totalHeight + 0.25f + 0.7f, d * 0.28f);
    glutSolidCone(0.38, 0.35, 10, 10);
    glPopMatrix();

    // Rooftop AC units
    glColor3f(0.7f, 0.72f, 0.72f);
    for (int u = -1; u <= 1; u += 2) {
        glPushMatrix();
        glTranslatef(-w * 0.3f, totalHeight + 0.28f, u * d * 0.25f);
        glScalef(0.5f, 0.35f, 0.5f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    // Windows on front (+Z) and back (-Z) faces, lit at night in a
    // stable per-window pattern (same windows lit every frame)
    float winW = 0.5f, winH = 0.7f;
    int cols = (int)(w / 1.2f);
    if (cols < 1) cols = 1;
    float faces[2] = { d / 2.0f + 0.01f, -(d / 2.0f + 0.01f) };
    for (int side = 0; side < 2; side++) {
        float nz = (side == 0) ? 1.0f : -1.0f;
        for (int f = 0; f < floors; f++) {
            float wy = f * floorHeight + floorHeight / 2.0f;
            for (int c = 0; c < cols; c++) {
                float wx = -w / 2.0f + (c + 0.5f) * (w / cols);
                bool lit = isNight && (hash01(buildingId + side * 500, f, c) > 0.45f);
                glPushMatrix();
                glTranslatef(wx, wy, faces[side]);
                if (lit) {
                    glColor3f(1.0f, 0.85f, 0.45f);
                    setEmission(0.85f, 0.6f, 0.2f);
                }
                else if (isNight) {
                    glColor3f(0.1f, 0.13f, 0.22f);
                }
                else {
                    glColor3f(0.75f, 0.9f, 1.0f);
                }
                glBegin(GL_QUADS);
                glNormal3f(0, 0, nz);
                if (side == 0) {
                    glVertex3f(-winW / 2, -winH / 2, 0);
                    glVertex3f(winW / 2, -winH / 2, 0);
                    glVertex3f(winW / 2, winH / 2, 0);
                    glVertex3f(-winW / 2, winH / 2, 0);
                }
                else {
                    glVertex3f(winW / 2, -winH / 2, 0);
                    glVertex3f(-winW / 2, -winH / 2, 0);
                    glVertex3f(-winW / 2, winH / 2, 0);
                    glVertex3f(winW / 2, winH / 2, 0);
                }
                glEnd();
                if (lit) clearEmission();
                glPopMatrix();
            }
        }

        // Balconies on every other floor, front face only
        if (side == 0) {
            for (int f = 1; f < floors; f += 2) {
                float wy = f * floorHeight;
                glColor3f(rr * 0.85f, gg * 0.85f, bb * 0.85f);
                glPushMatrix();
                glTranslatef(0, wy, d / 2.0f + 0.35f);
                glScalef(w * 0.85f, 0.08f, 0.6f);
                glutSolidCube(1.0);
                glPopMatrix();
                glColor3f(0.6f, 0.6f, 0.6f);
                glPushMatrix();
                glTranslatef(0, wy + 0.3f, d / 2.0f + 0.65f);
                glScalef(w * 0.85f, 0.5f, 0.05f);
                glutSolidCube(1.0);
                glPopMatrix();
            }
        }
    }

    // Entrance door
    glColor3f(0.55f, 0.4f, 0.3f);
    glPushMatrix();
    glTranslatef(0, 0.6f, d / 2.0f + 0.02f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.6f, 0);
    glVertex3f(0.5f, -0.6f, 0);
    glVertex3f(0.5f, 0.6f, 0);
    glVertex3f(-0.5f, 0.6f, 0);
    glEnd();
    glPopMatrix();

    // Door light - glows at night
    glPushMatrix();
    glTranslatef(0.65f, 1.15f, d / 2.0f + 0.05f);
    if (isNight) {
        glColor3f(1.0f, 0.9f, 0.6f);
        setEmission(0.8f, 0.65f, 0.3f);
        glutSolidSphere(0.08, 8, 8);
        clearEmission();
    }
    else {
        glColor3f(0.6f, 0.55f, 0.45f);
        glutSolidSphere(0.08, 8, 8);
    }
    glPopMatrix();
}

// ---------------------------------------------------------
// Pond (now fringed with reed bushes)
// ---------------------------------------------------------
void drawPond(float cx, float cz, float radius) {
    glPushMatrix();
    glTranslatef(cx, 0.03f, cz);

    glColor3f(0.85f, 0.8f, 0.65f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= 32; i++) {
        float a = i * 2 * PI / 32;
        glVertex3f(cosf(a) * (radius + 1.2f), 0, sinf(a) * (radius + 1.2f));
    }
    glEnd();

    glColor3f(0.35f, 0.65f, 0.85f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0.02f, 0);
    for (int i = 0; i <= 32; i++) {
        float a = i * 2 * PI / 32;
        glVertex3f(cosf(a) * radius, 0.02f, sinf(a) * radius);
    }
    glEnd();

    glColor3f(0.8f, 0.8f, 0.8f);
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, 0.4f, 0.3f, 1.2f, 10, 10);
    glPopMatrix();
    glColor3f(0.9f, 0.95f, 1.0f);
    glPushMatrix();
    glTranslatef(0, 1.3f, 0);
    glutSolidSphere(0.3, 10, 10);
    glPopMatrix();

    // Reed bushes ringing the pond edge
    for (int i = 0; i < 10; i++) {
        float a = i * 2 * PI / 10;
        float bx = cosf(a) * (radius + 1.5f);
        float bz = sinf(a) * (radius + 1.5f);
        glPushMatrix();
        glTranslatef(bx, 0, bz);
        glScalef(0.6f, 0.6f, 0.6f);
        drawBush();
        glPopMatrix();
    }
    glPopMatrix();
}

void drawSwingSet() {
    glColor3f(0.5f, 0.3f, 0.15f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 1.5f, 0, 0);
        glRotatef(-90, 1, 0, 0);
        GLUquadric* quad = gluNewQuadric();
        gluCylinder(quad, 0.08f, 0.08f, 2.2f, 8, 8);
        glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(-1.5f, 2.2f, 0);
    glRotatef(90, 0, 1, 0);
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, 0.08f, 0.08f, 3.0f, 8, 8);
    glPopMatrix();
    glColor3f(0.8f, 0.6f, 0.2f);
    for (int i = -1; i <= 1; i += 2) {
        glPushMatrix();
        glTranslatef(i * 0.6f, 1.3f, 0);
        glScalef(0.4f, 0.06f, 0.25f);
        glutSolidCube(1.0);
        glPopMatrix();
    }
}

void drawSlide() {
    glColor3f(0.9f, 0.5f, 0.2f);
    glPushMatrix();
    glTranslatef(0, 1.5f, -0.8f);
    glScalef(1.0f, 0.1f, 0.8f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 0.8f, 0.6f);
    glRotatef(35, 1, 0, 0);
    glScalef(1.0f, 0.08f, 2.0f);
    glutSolidCube(1.0);
    glPopMatrix();
    glColor3f(0.5f, 0.3f, 0.15f);
    glPushMatrix();
    glTranslatef(-0.45f, 0.75f, -0.8f);
    glScalef(0.1f, 1.5f, 0.1f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.45f, 0.75f, -0.8f);
    glScalef(0.1f, 1.5f, 0.1f);
    glutSolidCube(1.0);
    glPopMatrix();
}

void drawBench() {
    glColor3f(0.45f, 0.28f, 0.15f);
    glPushMatrix();
    glTranslatef(0, 0.45f, 0);
    glScalef(1.4f, 0.1f, 0.5f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, 0.75f, -0.22f);
    glScalef(1.4f, 0.6f, 0.08f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// ---------------------------------------------------------
// Park - now with a hedge perimeter (gap left on the north
// edge as the entrance facing the road)
// ---------------------------------------------------------
void drawPark(float cx, float cz) {
    glColor3f(0.9f, 0.82f, 0.6f);
    glPushMatrix();
    glTranslatef(cx, 0.015f, cz);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-9, 0, -9);
    glVertex3f(-9, 0, 9);
    glVertex3f(9, 0, 9);
    glVertex3f(9, 0, -9);
    glEnd();
    glPopMatrix();

    glPushMatrix(); glTranslatef(cx - 3, 0, cz - 2); drawSwingSet(); glPopMatrix();
    glPushMatrix(); glTranslatef(cx + 3, 0, cz - 3); drawSlide(); glPopMatrix();
    glPushMatrix(); glTranslatef(cx - 3, 0, cz + 4); glRotatef(90, 0, 1, 0); drawBench(); glPopMatrix();
    glPushMatrix(); glTranslatef(cx + 3, 0, cz + 4); glRotatef(90, 0, 1, 0); drawBench(); glPopMatrix();

    float corners[4][2] = { {-8,-8}, {8,-8}, {-8,8}, {8,8} };
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glTranslatef(cx + corners[i][0], 0, cz + corners[i][1]);
        drawTree();
        glPopMatrix();
    }

    // Hedge perimeter, with a 4-unit gap in the middle of the
    // north edge (the side that faces the cross road) as an entrance
    for (float t = -8.5f; t <= 8.5f; t += 2.0f) {
        glPushMatrix(); glTranslatef(cx + t, 0, cz - 9); drawBush(); glPopMatrix(); // south edge
        glPushMatrix(); glTranslatef(cx - 9, 0, cz + t); drawBush(); glPopMatrix(); // west edge
        glPushMatrix(); glTranslatef(cx + 9, 0, cz + t); drawBush(); glPopMatrix(); // east edge
        if (fabs(t) > 2.0f) {
            glPushMatrix(); glTranslatef(cx + t, 0, cz + 9); drawBush(); glPopMatrix(); // north edge (entrance gap)
        }
    }
}

// ---------------------------------------------------------
// School - sits in the open ground behind the park. Closed and
// completely dark at night except for a glowing sign, since a
// school has no reason to be lit up after hours.
// ---------------------------------------------------------
void drawSchool(float cx, float cz, float rotY) {
    glPushMatrix();
    glTranslatef(cx, 0, cz);
    glRotatef(rotY, 0, 1, 0);

    float w = 18.0f, d = 8.0f;
    int floors = 2;
    float floorHeight = 1.6f;
    float totalHeight = floors * floorHeight;

    // Main block - light institutional cream
    glColor3f(0.92f, 0.87f, 0.72f);
    glPushMatrix();
    glTranslatef(0, totalHeight / 2.0f, 0);
    glScalef(w, totalHeight, d);
    glutSolidCube(1.0);
    glPopMatrix();

    // Roof trim
    glColor3f(0.6f, 0.42f, 0.3f);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.1f, 0);
    glScalef(w + 0.4f, 0.2f, d + 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Windows - large, school-style. Lit by day, fully dark at
    // night (no emission at all - the building is simply closed).
    float winW = 0.9f, winH = 0.9f;
    int cols = (int)(w / 1.8f);
    if (cols < 1) cols = 1;
    for (int f = 0; f < floors; f++) {
        float wy = f * floorHeight + floorHeight / 2.0f + 0.2f;
        for (int c = 0; c < cols; c++) {
            float wx = -w / 2.0f + (c + 0.5f) * (w / cols);
            glPushMatrix();
            glTranslatef(wx, wy, d / 2.0f + 0.01f);
            if (isNight) glColor3f(0.08f, 0.1f, 0.16f);
            else glColor3f(0.7f, 0.85f, 0.9f);
            glBegin(GL_QUADS);
            glNormal3f(0, 0, 1);
            glVertex3f(-winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, winH / 2, 0);
            glVertex3f(-winW / 2, winH / 2, 0);
            glEnd();
            glPopMatrix();
        }
    }

    // Entrance steps + door (also dark at night, no door light)
    glColor3f(0.75f, 0.7f, 0.6f);
    glPushMatrix();
    glTranslatef(0, 0.1f, d / 2.0f + 0.5f);
    glScalef(4.0f, 0.2f, 1.0f);
    glutSolidCube(1.0);
    glPopMatrix();
    glColor3f(0.5f, 0.35f, 0.25f);
    glPushMatrix();
    glTranslatef(0, 0.7f, d / 2.0f + 0.02f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.8f, -0.7f, 0);
    glVertex3f(0.8f, -0.7f, 0);
    glVertex3f(0.8f, 0.7f, 0);
    glVertex3f(-0.8f, 0.7f, 0);
    glEnd();
    glPopMatrix();

    // Flagpole
    glColor3f(0.6f, 0.6f, 0.6f);
    glPushMatrix();
    glTranslatef(-w / 2.0f - 1.5f, 0, d / 2.0f + 1.5f);
    glRotatef(-90, 1, 0, 0);
    GLUquadric* poleQuad = gluNewQuadric();
    gluCylinder(poleQuad, 0.06f, 0.06f, 3.5f, 8, 8);
    glPopMatrix();
    glColor3f(0.8f, 0.15f, 0.15f);
    glPushMatrix();
    glTranslatef(-w / 2.0f - 1.3f, 3.1f, d / 2.0f + 1.5f);
    glScalef(0.6f, 0.4f, 0.02f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Small basketball half-court beside the school
    glColor3f(0.75f, 0.45f, 0.3f);
    glPushMatrix();
    glTranslatef(w / 2.0f + 4.0f, 0.012f, 0);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-3.5f, 0, -4.5f);
    glVertex3f(-3.5f, 0, 4.5f);
    glVertex3f(3.5f, 0, 4.5f);
    glVertex3f(3.5f, 0, -4.5f);
    glEnd();
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-3.5f, 0.014f, -0.08f);
    glVertex3f(-3.5f, 0.014f, 0.08f);
    glVertex3f(3.5f, 0.014f, 0.08f);
    glVertex3f(3.5f, 0.014f, -0.08f);
    glEnd();
    glPopMatrix();

    // Illuminated "SCHOOL" sign - the ONLY thing that lights up at
    // night; everything else on the building stays dark.
    glPushMatrix();
    glTranslatef(-2.8f, totalHeight + 0.55f, d / 2.0f + 0.05f);
    glScalef(0.012f, 0.012f, 0.012f);
    if (isNight) { glColor3f(1.0f, 0.95f, 0.5f); setEmission(0.9f, 0.8f, 0.3f); }
    else glColor3f(0.35f, 0.3f, 0.2f);
    const char* schoolLabel = "SCHOOL";
    for (const char* c = schoolLabel; *c != '\0'; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    if (isNight) clearEmission();
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// Hospital - sits in the open ground behind the pond, which now
// doubles as its garden pond. Runs 24/7, so most windows stay
// lit at night and the red cross sign glows at all times.
// ---------------------------------------------------------
void drawHospital(float cx, float cz, float rotY) {
    glPushMatrix();
    glTranslatef(cx, 0, cz);
    glRotatef(rotY, 0, 1, 0);

    float w = 16.0f, d = 9.0f;
    int floors = 4;
    float floorHeight = 1.6f;
    float totalHeight = floors * floorHeight;

    // Main block - clean white / light blue-grey
    glColor3f(0.92f, 0.94f, 0.95f);
    glPushMatrix();
    glTranslatef(0, totalHeight / 2.0f, 0);
    glScalef(w, totalHeight, d);
    glutSolidCube(1.0);
    glPopMatrix();

    // Roof trim
    glColor3f(0.75f, 0.8f, 0.82f);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.1f, 0);
    glScalef(w + 0.4f, 0.2f, d + 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Windows - most are lit at night (patients need doctors at
    // night!), a smaller number lit during the day (offices/labs).
    float winW = 0.7f, winH = 0.8f;
    int cols = (int)(w / 1.4f);
    if (cols < 1) cols = 1;
    for (int f = 0; f < floors; f++) {
        float wy = f * floorHeight + floorHeight / 2.0f + 0.2f;
        for (int c = 0; c < cols; c++) {
            float wx = -w / 2.0f + (c + 0.5f) * (w / cols);
            float rnd = hash01((int)(cx * 3 + cz * 5) + 900, f, c);
            bool lit = isNight ? (rnd > 0.25f) : (rnd > 0.8f);
            glPushMatrix();
            glTranslatef(wx, wy, d / 2.0f + 0.01f);
            if (lit) {
                glColor3f(1.0f, 0.95f, 0.75f);
                if (isNight) setEmission(0.8f, 0.75f, 0.5f);
            }
            else if (isNight) glColor3f(0.12f, 0.15f, 0.2f);
            else glColor3f(0.75f, 0.88f, 0.92f);
            glBegin(GL_QUADS);
            glNormal3f(0, 0, 1);
            glVertex3f(-winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, winH / 2, 0);
            glVertex3f(-winW / 2, winH / 2, 0);
            glEnd();
            if (lit && isNight) clearEmission();
            glPopMatrix();
        }
    }

    // Entrance canopy
    glColor3f(0.6f, 0.65f, 0.68f);
    glPushMatrix();
    glTranslatef(0, 2.2f, d / 2.0f + 1.2f);
    glScalef(5.0f, 0.15f, 2.2f);
    glutSolidCube(1.0);
    glPopMatrix();
    for (int s = -1; s <= 1; s += 2) {
        glColor3f(0.55f, 0.55f, 0.55f);
        glPushMatrix();
        glTranslatef(s * 2.2f, 1.1f, d / 2.0f + 2.2f);
        glRotatef(-90, 1, 0, 0);
        GLUquadric* postQuad = gluNewQuadric();
        gluCylinder(postQuad, 0.08f, 0.08f, 2.2f, 8, 8);
        glPopMatrix();
    }

    // Glowing red cross sign on the roof - always red, glows at night
    glColor3f(0.85f, 0.1f, 0.1f);
    if (isNight) setEmission(0.7f, 0.05f, 0.05f);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.7f, 0);
    glScalef(1.6f, 0.4f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.7f, 0);
    glScalef(0.4f, 1.6f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();
    if (isNight) clearEmission();

    // "HOSPITAL" sign above the entrance
    glPushMatrix();
    glTranslatef(-3.6f, totalHeight * 0.5f + 0.9f, d / 2.0f + 0.05f);
    glScalef(0.012f, 0.012f, 0.012f);
    glColor3f(0.85f, 0.1f, 0.1f);
    if (isNight) setEmission(0.7f, 0.05f, 0.05f);
    const char* hospitalLabel = "HOSPITAL";
    for (const char* c = hospitalLabel; *c != '\0'; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    if (isNight) clearEmission();
    glPopMatrix();

    // Ambulance parked by the entrance
    glPushMatrix();
    glTranslatef(w / 2.0f + 2.5f, 0.45f, d / 2.0f + 0.5f);
    glRotatef(90, 0, 1, 0);
    drawCarModel(0.95f, 0.95f, 0.95f, isNight);
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// College - sits west of the school, sharing the same open
// ground and street frontage. A bit larger and taller, with a
// small clock tower and a columned entrance porch. Like the
// school, it's closed and dark at night except for its sign.
// ---------------------------------------------------------
void drawCollege(float cx, float cz, float rotY) {
    glPushMatrix();
    glTranslatef(cx, 0, cz);
    glRotatef(rotY, 0, 1, 0);

    float w = 15.0f, d = 8.0f;
    int floors = 3;
    float floorHeight = 1.6f;
    float totalHeight = floors * floorHeight;

    // Main block - academic brick red
    glColor3f(0.62f, 0.32f, 0.28f);
    glPushMatrix();
    glTranslatef(0, totalHeight / 2.0f, 0);
    glScalef(w, totalHeight, d);
    glutSolidCube(1.0);
    glPopMatrix();

    // Cream trim band at roofline
    glColor3f(0.9f, 0.85f, 0.7f);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.1f, 0);
    glScalef(w + 0.4f, 0.2f, d + 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Windows - dark at night (closed), warm cream by day
    float winW = 0.8f, winH = 0.85f;
    int cols = (int)(w / 1.6f);
    if (cols < 1) cols = 1;
    for (int f = 0; f < floors; f++) {
        float wy = f * floorHeight + floorHeight / 2.0f + 0.2f;
        for (int c = 0; c < cols; c++) {
            float wx = -w / 2.0f + (c + 0.5f) * (w / cols);
            glPushMatrix();
            glTranslatef(wx, wy, d / 2.0f + 0.01f);
            if (isNight) glColor3f(0.08f, 0.1f, 0.16f);
            else glColor3f(0.85f, 0.85f, 0.7f);
            glBegin(GL_QUADS);
            glNormal3f(0, 0, 1);
            glVertex3f(-winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, -winH / 2, 0);
            glVertex3f(winW / 2, winH / 2, 0);
            glVertex3f(-winW / 2, winH / 2, 0);
            glEnd();
            glPopMatrix();
        }
    }

    // Entrance steps + columned porch
    glColor3f(0.85f, 0.8f, 0.7f);
    glPushMatrix();
    glTranslatef(0, 0.1f, d / 2.0f + 0.6f);
    glScalef(5.0f, 0.2f, 1.2f);
    glutSolidCube(1.0);
    glPopMatrix();
    for (int s = -1; s <= 1; s += 2) {
        glColor3f(0.9f, 0.88f, 0.8f);
        glPushMatrix();
        glTranslatef(s * 1.8f, 1.1f, d / 2.0f + 1.0f);
        glRotatef(-90, 1, 0, 0);
        GLUquadric* colQuad = gluNewQuadric();
        gluCylinder(colQuad, 0.15f, 0.15f, 2.2f, 10, 10);
        glPopMatrix();
    }
    glColor3f(0.5f, 0.3f, 0.25f);
    glPushMatrix();
    glTranslatef(0, 0.75f, d / 2.0f + 0.02f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.9f, -0.75f, 0);
    glVertex3f(0.9f, -0.75f, 0);
    glVertex3f(0.9f, 0.75f, 0);
    glVertex3f(-0.9f, 0.75f, 0);
    glEnd();
    glPopMatrix();

    // Small clock tower on one corner of the roof
    glColor3f(0.62f, 0.32f, 0.28f);
    glPushMatrix();
    glTranslatef(w / 2.0f - 1.2f, totalHeight, 0);
    glScalef(1.8f, 2.0f, 1.8f);
    glutSolidCube(1.0);
    glPopMatrix();
    glColor3f(0.95f, 0.95f, 0.9f);
    glPushMatrix();
    glTranslatef(w / 2.0f - 1.2f, totalHeight + 2.05f, d * 0.2f);
    glutSolidSphere(0.35, 10, 10); // simple clock-face stand-in
    glPopMatrix();
    glColor3f(0.7f, 0.5f, 0.3f);
    glPushMatrix();
    glTranslatef(w / 2.0f - 1.2f, totalHeight + 2.9f, 0);
    glScalef(1.9f, 0.15f, 1.9f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Illuminated "COLLEGE" sign - only thing lit at night
    glPushMatrix();
    glTranslatef(-3.2f, totalHeight + 0.55f, d / 2.0f + 0.05f);
    glScalef(0.011f, 0.011f, 0.011f);
    if (isNight) { glColor3f(1.0f, 0.95f, 0.5f); setEmission(0.9f, 0.8f, 0.3f); }
    else glColor3f(0.35f, 0.3f, 0.2f);
    const char* collegeLabel = "COLLEGE";
    for (const char* c = collegeLabel; *c != '\0'; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    if (isNight) clearEmission();
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// Departmental store - modern retail block with a big glowing
// storefront sign and ground-floor display windows that stay
// softly lit overnight (security/window-shopping); the upper
// office/storage floor goes fully dark at night.
// ---------------------------------------------------------
void drawDepartmentalStore(float cx, float cz, float rotY) {
    glPushMatrix();
    glTranslatef(cx, 0, cz);
    glRotatef(rotY, 0, 1, 0);

    float w = 16.0f, d = 10.0f;
    float groundH = 2.4f, upperH = 1.6f;
    float totalHeight = groundH + upperH;

    // Ground floor - big glass storefront
    glColor3f(0.85f, 0.85f, 0.88f);
    glPushMatrix();
    glTranslatef(0, groundH / 2.0f, 0);
    glScalef(w, groundH, d);
    glutSolidCube(1.0);
    glPopMatrix();

    // Upper floor - office/storage, slightly narrower and set back
    glColor3f(0.65f, 0.55f, 0.45f);
    glPushMatrix();
    glTranslatef(0, groundH + upperH / 2.0f, -0.3f);
    glScalef(w * 0.92f, upperH, d * 0.85f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Roof trim
    glColor3f(0.55f, 0.45f, 0.35f);
    glPushMatrix();
    glTranslatef(0, totalHeight + 0.08f, -0.3f);
    glScalef(w * 0.92f + 0.3f, 0.16f, d * 0.85f + 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Large storefront display windows across the ground floor
    float winW = 1.6f, winH = 1.7f;
    int cols = (int)(w / 2.2f);
    if (cols < 1) cols = 1;
    for (int c = 0; c < cols; c++) {
        float wx = -w / 2.0f + (c + 0.5f) * (w / cols);
        glPushMatrix();
        glTranslatef(wx, groundH * 0.55f, d / 2.0f + 0.01f);
        if (isNight) { glColor3f(1.0f, 0.93f, 0.7f); setEmission(0.55f, 0.5f, 0.35f); }
        else glColor3f(0.6f, 0.75f, 0.85f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glVertex3f(-winW / 2, -winH / 2, 0);
        glVertex3f(winW / 2, -winH / 2, 0);
        glVertex3f(winW / 2, winH / 2, 0);
        glVertex3f(-winW / 2, winH / 2, 0);
        glEnd();
        if (isNight) clearEmission();
        glPopMatrix();
    }

    // Upper floor windows - small, fully dark at night (offices closed)
    int upCols = (int)(w * 0.9f / 2.4f);
    if (upCols < 1) upCols = 1;
    for (int c = 0; c < upCols; c++) {
        float wx = -w * 0.42f + (c + 0.5f) * (w * 0.84f / upCols);
        glPushMatrix();
        glTranslatef(wx, groundH + upperH / 2.0f, d * 0.85f / 2.0f - 0.3f + 0.01f);
        if (isNight) glColor3f(0.08f, 0.1f, 0.16f);
        else glColor3f(0.7f, 0.8f, 0.85f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glVertex3f(-0.5f, -0.4f, 0);
        glVertex3f(0.5f, -0.4f, 0);
        glVertex3f(0.5f, 0.4f, 0);
        glVertex3f(-0.5f, 0.4f, 0);
        glEnd();
        glPopMatrix();
    }

    // Entrance awning
    glColor3f(0.8f, 0.25f, 0.2f);
    glPushMatrix();
    glTranslatef(0, groundH - 0.3f, d / 2.0f + 0.9f);
    glScalef(w * 0.5f, 0.15f, 1.8f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Glowing storefront sign - lit all night for visibility
    glPushMatrix();
    glTranslatef(-5.4f, totalHeight - 0.35f, d / 2.0f + 0.05f);
    glScalef(0.011f, 0.011f, 0.011f);
    if (isNight) { glColor3f(1.0f, 0.6f, 0.2f); setEmission(0.85f, 0.45f, 0.1f); }
    else glColor3f(0.4f, 0.25f, 0.1f);
    const char* storeLabel = "DEPT STORE";
    for (const char* c = storeLabel; *c != '\0'; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    if (isNight) clearEmission();
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// Mosque - domed hall with twin minarets. The dome and minaret
// tips glow softly at night, and the main hall keeps a warm
// light for pre-dawn and night prayers.
// ---------------------------------------------------------
void drawMosque(float cx, float cz, float rotY) {
    glPushMatrix();
    glTranslatef(cx, 0, cz);
    glRotatef(rotY, 0, 1, 0);

    float w = 14.0f, d = 10.0f;
    float wallHeight = 3.4f;

    // Main hall - white with green trim
    glColor3f(0.95f, 0.95f, 0.9f);
    glPushMatrix();
    glTranslatef(0, wallHeight / 2.0f, 0);
    glScalef(w, wallHeight, d);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(0.15f, 0.45f, 0.3f);
    glPushMatrix();
    glTranslatef(0, wallHeight + 0.08f, 0);
    glScalef(w + 0.3f, 0.16f, d + 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Central dome - glows softly through the night
    if (isNight) { glColor3f(0.85f, 0.95f, 0.85f); setEmission(0.25f, 0.4f, 0.3f); }
    else glColor3f(0.2f, 0.55f, 0.4f);
    glPushMatrix();
    glTranslatef(0, wallHeight + 0.2f, 0);
    glScalef(1.0f, 0.75f, 1.0f);
    glutSolidSphere(2.6, 16, 16);
    glPopMatrix();
    if (isNight) clearEmission();

    glColor3f(0.85f, 0.7f, 0.2f);
    glPushMatrix();
    glTranslatef(0, wallHeight + 2.6f, 0);
    glutSolidCone(0.15, 0.6, 8, 8);
    glPopMatrix();

    // Twin minarets at the front corners
    for (int s = -1; s <= 1; s += 2) {
        float mx = s * (w / 2.0f - 0.6f);
        float mz = d / 2.0f - 0.6f;
        glColor3f(0.9f, 0.9f, 0.85f);
        glPushMatrix();
        glTranslatef(mx, 0, mz);
        glRotatef(-90, 1, 0, 0);
        GLUquadric* minQuad = gluNewQuadric();
        gluCylinder(minQuad, 0.35f, 0.3f, 7.5f, 10, 10);
        glPopMatrix();

        glColor3f(0.2f, 0.55f, 0.4f);
        glPushMatrix();
        glTranslatef(mx, 7.5f, mz);
        glutSolidCone(0.4, 1.0, 10, 10);
        glPopMatrix();

        // Minaret light - a familiar glow after dark
        glPushMatrix();
        glTranslatef(mx, 8.6f, mz);
        if (isNight) {
            glColor3f(0.8f, 1.0f, 0.85f);
            setEmission(0.5f, 0.8f, 0.5f);
            glutSolidSphere(0.16, 8, 8);
            clearEmission();
        }
        else {
            glColor3f(0.6f, 0.6f, 0.55f);
            glutSolidSphere(0.16, 8, 8);
        }
        glPopMatrix();
    }

    // Arched entrance, approximated with a doorway + a small
    // half-dome canopy above it
    glColor3f(0.15f, 0.45f, 0.3f);
    glPushMatrix();
    glTranslatef(0, 1.1f, d / 2.0f + 0.02f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-1.0f, -1.1f, 0);
    glVertex3f(1.0f, -1.1f, 0);
    glVertex3f(1.0f, 1.1f, 0);
    glVertex3f(-1.0f, 1.1f, 0);
    glEnd();
    glPopMatrix();
    glColor3f(0.85f, 0.95f, 0.85f);
    glPushMatrix();
    glTranslatef(0, 2.2f, d / 2.0f + 0.02f);
    glScalef(1.0f, 0.4f, 1.0f);
    glutSolidSphere(1.0, 16, 8);
    glPopMatrix();

    // Small "MOSQUE" sign near the entrance, softly lit at night
    glPushMatrix();
    glTranslatef(-1.9f, wallHeight + 0.5f, d / 2.0f + 0.05f);
    glScalef(0.009f, 0.009f, 0.009f);
    if (isNight) { glColor3f(0.8f, 1.0f, 0.85f); setEmission(0.4f, 0.6f, 0.4f); }
    else glColor3f(0.2f, 0.4f, 0.3f);
    const char* mosqueLabel = "MOSQUE";
    for (const char* c = mosqueLabel; *c != '\0'; c++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    if (isNight) clearEmission();
    glPopMatrix();

    glPopMatrix();
}

// ---------------------------------------------------------
// Civic plaza - the paved walkway that links the departmental
// store, hospital and mosque together, with parking, cars,
// trees and lamps filling in what would otherwise be bare
// ground between the three buildings.
// ---------------------------------------------------------
void drawCivicPlaza(float xStart, float xEnd, float zNear, float zFar) {
    glColor3f(0.78f, 0.76f, 0.72f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(xStart, 0.013f, zNear);
    glVertex3f(xStart, 0.013f, zFar);
    glVertex3f(xEnd, 0.013f, zFar);
    glVertex3f(xEnd, 0.013f, zNear);
    glEnd();

    // Parking bay + two cars in front of the departmental store
    glColor3f(1, 1, 1);
    for (int i = 0; i < 2; i++) {
        float lx = xStart + 4.0f + i * 5.0f;
        glBegin(GL_QUADS);
        glVertex3f(lx, 0.015f, zNear - 0.1f);
        glVertex3f(lx, 0.015f, zFar + 0.1f);
        glVertex3f(lx + 0.15f, 0.015f, zFar + 0.1f);
        glVertex3f(lx + 0.15f, 0.015f, zNear - 0.1f);
        glEnd();
    }
    float carColors[2][3] = { {0.3f, 0.45f, 0.75f}, {0.8f, 0.8f, 0.2f} };
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glTranslatef(xStart + 6.5f + i * 5.0f, 0.45f, (zNear + zFar) / 2.0f);
        drawCarModel(carColors[i][0], carColors[i][1], carColors[i][2], isNight);
        glPopMatrix();
    }
}

// ---------------------------------------------------------
// Trees + lamps along the WHOLE road network (main + cross)
// ---------------------------------------------------------
void placeRoadDecor() {
    // Lamp posts, both roads
    for (float z = -60; z <= 60; z += 12) {
        if (fabs(z) < 6) continue;
        glPushMatrix(); glTranslatef(-6.5f, 0, z); drawLampPost(); glPopMatrix();
        glPushMatrix(); glTranslatef(6.5f, 0, z); drawLampPost(); glPopMatrix();
    }
    for (float x = -60; x <= 60; x += 12) {
        if (fabs(x) < 6) continue;
        glPushMatrix(); glTranslatef(x, 0, -6.5f); drawLampPost(); glPopMatrix();
        glPushMatrix(); glTranslatef(x, 0, 6.5f); drawLampPost(); glPopMatrix();
    }

    // Trees, both roads, spaced between the lamp posts
    for (float z = -63; z <= 63; z += 12) {
        if (fabs(z) < 8) continue;
        glPushMatrix(); glTranslatef(-9.0f, 0, z); drawTree(); glPopMatrix();
        glPushMatrix(); glTranslatef(9.0f, 0, z); drawTree(); glPopMatrix();
    }
    for (float x = -63; x <= 63; x += 12) {
        if (fabs(x) < 8) continue;
        glPushMatrix(); glTranslatef(x, 0, -9.0f); drawTree(); glPopMatrix();
        glPushMatrix(); glTranslatef(x, 0, 9.0f); drawTree(); glPopMatrix();
    }
}

// ---------------------------------------------------------
// One apartment row: a local street with buildings beside it
// ---------------------------------------------------------
void placeApartmentRow(float rowFixed, float startCoord, float endCoord,
    float perpOffset, int numBuildings, float rotY,
    int colorSeed) {
    // A richer, more realistic palette - varied hue AND brightness
    float pastel[10][3] = {
        {0.85f, 0.45f, 0.35f}, // terracotta
        {0.55f, 0.72f, 0.85f}, // dusty blue
        {0.93f, 0.78f, 0.35f}, // mustard
        {0.55f, 0.75f, 0.55f}, // sage green
        {0.85f, 0.6f, 0.7f},   // dusty rose
        {0.95f, 0.9f, 0.75f},  // cream
        {0.6f, 0.5f, 0.7f},    // lavender
        {0.8f, 0.5f, 0.3f},    // brick orange
        {0.5f, 0.6f, 0.65f},   // slate blue-grey
        {0.9f, 0.7f, 0.45f}    // sandstone
    };

    drawStreet(startCoord, rowFixed, endCoord, rowFixed, 2.0f);

    float span = endCoord - startCoord;
    float step = span / numBuildings;
    for (int i = 0; i < numBuildings; i++) {
        float coord = startCoord + step * (i + 0.5f);
        int floors = 3 + (i % 3);
        int cIdx = (colorSeed + i) % 10;
        int buildingId = (int)(rowFixed * 4.0f) * 977 + colorSeed * 131 + i * 13 + (int)startCoord;

        glPushMatrix();
        glTranslatef(coord, 0, rowFixed + perpOffset);
        glRotatef(rotY, 0, 1, 0);
        drawApartment(6.0f, 5.0f, floors, pastel[cIdx][0], pastel[cIdx][1], pastel[cIdx][2], buildingId);
        glPopMatrix();
    }
}

// ---------------------------------------------------------
// Small parking lot: painted lines + a couple of parked cars
// ---------------------------------------------------------
void placeParkingLot(float cx, float cz, float rotY) {
    glColor3f(0.75f, 0.75f, 0.73f);
    glPushMatrix();
    glTranslatef(cx, 0.012f, cz);
    glRotatef(rotY, 0, 1, 0);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-5, 0, -3);
    glVertex3f(-5, 0, 3);
    glVertex3f(5, 0, 3);
    glVertex3f(5, 0, -3);
    glEnd();

    glColor3f(1, 1, 1);
    for (int i = -1; i <= 1; i++) {
        glBegin(GL_QUADS);
        glVertex3f(i * 3.0f - 1.4f, 0.014f, -3);
        glVertex3f(i * 3.0f - 1.4f, 0.014f, 3);
        glVertex3f(i * 3.0f - 1.3f, 0.014f, 3);
        glVertex3f(i * 3.0f - 1.3f, 0.014f, -3);
        glEnd();
    }
    glPopMatrix();

    float carColors[2][3] = { {0.75f, 0.15f, 0.15f}, {0.85f, 0.85f, 0.88f} };
    float spotX[2] = { -3.0f, 3.0f };
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glTranslatef(cx, 0.0f, cz);
        glRotatef(rotY, 0, 1, 0);
        glTranslatef(spotX[i], 0.45f, 0);
        glRotatef(90, 0, 1, 0);
        drawCarModel(carColors[i][0], carColors[i][1], carColors[i][2], isNight);
        glPopMatrix();
    }
}

// ---------------------------------------------------------
// A row of parked school buses along a curb, evenly spaced and
// facing along the +X direction (parallel-parked style). Used
// for the drop-off row in front of the college.
// ---------------------------------------------------------
void placeSchoolBusRow(float cx, float cz, int count, float spacing) {
    float startX = cx - spacing * (count - 1) / 2.0f;
    // Curb / painted bay under the buses
    glColor3f(0.7f, 0.7f, 0.68f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(startX - 3.0f, 0.012f, cz - 1.3f);
    glVertex3f(startX - 3.0f, 0.012f, cz + 1.3f);
    glVertex3f(startX + spacing * (count - 1) + 3.0f, 0.012f, cz + 1.3f);
    glVertex3f(startX + spacing * (count - 1) + 3.0f, 0.012f, cz - 1.3f);
    glEnd();

    for (int i = 0; i < count; i++) {
        glPushMatrix();
        glTranslatef(startX + i * spacing, 0.87f, cz);
        glRotatef(90, 0, 1, 0);
        drawSchoolBus(isNight);
        glPopMatrix();
    }
}

// =========================================================
// ANIMATED SCHOOL BUS & AMBULANCE ROUTES
//
// Both vehicles now travel only along the roads that are
// actually drawn (the local streets, the main vertical road,
// and the main cross road). The only places they ever leave a
// paved surface are the short, perpendicular driveway spurs
// right at their destination - the school's front door for the
// bus, the hospital's entrance for the ambulance - exactly like
// a real vehicle pulling off the street into a driveway. Both
// also watch for regular traffic sharing the same lane and ease
// off instead of driving through a car (or through each other).
// =========================================================
struct RoutePoint { float x, z; float pauseTime; };

// A shared "is anything in the way" check used by both special
// vehicles: treats every ordinary car - and the other special
// vehicle - as something not to drive through.
bool positionBlockedByTraffic(float x, float z, float minGap, float extraX, float extraZ) {
    for (int i = 0; i < NUM_CARS; i++) {
        float dx = cars[i].x - x;
        float dz = cars[i].z - z;
        if (dx * dx + dz * dz < minGap * minGap) return true;
    }
    float edx = extraX - x, edz = extraZ - z;
    if (edx * edx + edz * edz < minGap * minGap) return true;
    return false;
}

// ---- School bus: full visible route (like the original) with school as home.
// Parks at the school driveway, picks up students on the local street,
// then drives the main road + cross road (easy to see), and returns to school.
const int BUS_ROUTE_LEN = 12;
RoutePoint schoolBusRoute[BUS_ROUTE_LEN] = {
    { -34.0f, -36.0f, 2.5f },  // school driveway - stands / drops off
    { -34.0f, -46.0f, 0.2f },  // onto local street
    { -48.0f, -46.0f, 1.8f },  // stop A - west of school (pick up)
    { -20.0f, -46.0f, 1.5f },  // stop B - east of school toward main road
    {  -8.0f, -46.0f, 0.0f },  // local street ends
    {  -2.2f, -46.0f, 0.0f },  // onto main road, northbound
    {  -2.2f,  -6.5f, 0.0f },  // driving north (clearly visible)
    {  -2.2f,   2.2f, 0.3f },  // intersection
    {  20.0f,  -2.2f, 0.8f },  // cross road eastbound
    {   2.2f,  -2.2f, 0.0f },  // turn back west
    {   2.2f, -46.0f, 0.0f },  // southbound main road toward school
    { -34.0f, -46.0f, 0.2f }   // local street, approach school again
};
// Start already on the main road so the bus is visible right away
float busRouteX = schoolBusRoute[6].x;
float busRouteZ = schoolBusRoute[6].z;
int busRouteWaypoint = 6;
float busRoutePauseTimer = 0.0f;
float busRouteHeadingDeg = 0.0f;  // heading north along main road
const float BUS_ROUTE_SPEED = 4.0f;  // faster loop so it stays visible more often
const int BUS_PICKUP_A = 2;
const int BUS_PICKUP_B = 3;
const int BUS_SCHOOL_STAND = 0;

// ---- Ambulance: hospital driveway -> civic plaza -> main road
// -> the intersection (the "call on the road") and back, only
// leaving pavement for the short spur into the hospital's own
// driveway (the first/last waypoints below). ----
const int AMB_ROUTE_LEN = 8;
RoutePoint ambulanceRoute[AMB_ROUTE_LEN] = {
    { 44.0f, -34.0f, 1.5f },  // hospital driveway - idles here between calls
    { 10.0f, -34.0f, 0.0f },  // along the civic plaza, toward the main road
    { -2.2f, -34.0f, 0.0f },  // onto the main road, northbound lane
    { -2.2f,  -2.2f, 0.0f },  // approaching the intersection
    {  2.2f,   2.2f, 2.0f },  // reaches the call on the road - loads the patient
    {  2.2f, -34.0f, 0.0f },  // southbound lane, heading back down
    { 10.0f, -34.0f, 0.0f },  // back onto the civic plaza
    { 44.0f, -34.0f, 2.0f }   // arrives back at the hospital - drops the patient off
};
float ambRouteX = ambulanceRoute[0].x;
float ambRouteZ = ambulanceRoute[0].z;
int ambRouteWaypoint = 0;
int ambRouteDir = 1;
float ambRoutePauseTimer = 0.0f;
float ambRouteHeadingDeg = 0.0f;
const float AMB_ROUTE_SPEED = 5.0f;
float ambulanceSirenPhase = 0.0f;

// When the bus is paused at a pickup stop, any nearby un-boarded
// student boards (hides). They reappear near the school after a short ride.
void tryBoardStudentsAtStop() {
    for (int i = 0; i < NUM_STUDENTS; i++) {
        if (students[i].boarded) continue;
        float dx = students[i].x - busRouteX;
        float dz = students[i].z - busRouteZ;
        if (dx * dx + dz * dz < 9.0f) {   // within ~3 units of the bus
            students[i].boarded = true;
            students[i].boardTimer = 6.0f + i * 0.8f; // staggered drop-off
        }
    }
}

void updateSchoolBusRoute(float dt) {
    if (isNight || !animateScene) return; // frozen (and hidden) at night

    if (busRoutePauseTimer > 0.0f) {
        busRoutePauseTimer -= dt;
        // While standing at a pickup / school stop, board nearby students
        if (busRouteWaypoint == BUS_PICKUP_A ||
            busRouteWaypoint == BUS_PICKUP_B ||
            busRouteWaypoint == BUS_SCHOOL_STAND ||
            busRouteWaypoint == BUS_ROUTE_LEN - 1) {
            tryBoardStudentsAtStop();
        }
        return;
    }

    // Continuous loop: after last waypoint, go back to the first
    int targetIdx = busRouteWaypoint + 1;
    if (targetIdx >= BUS_ROUTE_LEN) targetIdx = 0;

    RoutePoint target = schoolBusRoute[targetIdx];
    float dx = target.x - busRouteX;
    float dz = target.z - busRouteZ;
    float dist = sqrtf(dx * dx + dz * dz);

    if (dist < 0.08f) {
        busRouteX = target.x;
        busRouteZ = target.z;
        busRouteWaypoint = targetIdx;
        busRoutePauseTimer = target.pauseTime;
        // Face a sensible direction while parked at school
        if (targetIdx == BUS_SCHOOL_STAND || targetIdx == BUS_ROUTE_LEN - 1)
            busRouteHeadingDeg = 0.0f;   // facing school (+Z)
    }
    else {
        float step = BUS_ROUTE_SPEED * dt;
        if (step > dist) step = dist;
        float nx = busRouteX + dx / dist * step;
        float nz = busRouteZ + dz / dist * step;
        busRouteHeadingDeg = atan2f(dx, dz) * 180.0f / PI;

        // Direction of travel this frame
        float tdx = dx / dist, tdz = dz / dist;

        // Yield only to vehicles that are AHEAD and close (not behind/beside).
        // Cars already yield to the bus, so this avoids running them over
        // without creating a permanent deadlock.
        bool blocked = false;
        const float AHEAD_GAP = 5.5f;
        for (int i = 0; i < NUM_CARS; i++) {
            float cx = cars[i].x - busRouteX;
            float cz = cars[i].z - busRouteZ;
            float along = cx * tdx + cz * tdz;   // positive = ahead
            float side = cx * (-tdz) + cz * tdx;
            if (along > 0.3f && along < AHEAD_GAP && fabsf(side) < 2.5f) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            float ax = ambRouteX - busRouteX;
            float az = ambRouteZ - busRouteZ;
            float along = ax * tdx + az * tdz;
            float side = ax * (-tdz) + az * tdx;
            if (along > 0.3f && along < AHEAD_GAP && fabsf(side) < 2.5f)
                blocked = true;
        }
        if (!blocked) {
            busRouteX = nx;
            busRouteZ = nz;
        }
    }
}

void drawSchoolBusOnRoad() {
    if (isNight) return; // only seen in daytime
    glPushMatrix();
    glTranslatef(busRouteX, 0.87f, busRouteZ);
    glRotatef(busRouteHeadingDeg, 0, 1, 0);
    drawSchoolBus(false);
    glPopMatrix();
}

void updateAmbulanceRoute(float dt) {
    ambulanceSirenPhase += dt * 6.0f; // fast flash, always running
    if (!animateScene) return;

    if (ambRoutePauseTimer > 0.0f) { ambRoutePauseTimer -= dt; return; }

    int targetIdx = ambRouteWaypoint + ambRouteDir;
    if (targetIdx < 0 || targetIdx >= AMB_ROUTE_LEN) {
        // Reached an end of the route - turn around
        ambRouteDir = -ambRouteDir;
        ambRoutePauseTimer = 1.0f;
        return;
    }

    RoutePoint target = ambulanceRoute[targetIdx];
    float dx = target.x - ambRouteX;
    float dz = target.z - ambRouteZ;
    float dist = sqrtf(dx * dx + dz * dz);

    if (dist < 0.08f) {
        ambRouteX = target.x;
        ambRouteZ = target.z;
        ambRouteWaypoint = targetIdx;
        ambRoutePauseTimer = target.pauseTime;
    }
    else {
        float step = AMB_ROUTE_SPEED * dt;
        if (step > dist) step = dist;
        float nx = ambRouteX + dx / dist * step;
        float nz = ambRouteZ + dz / dist * step;
        ambRouteHeadingDeg = atan2f(dx, dz) * 180.0f / PI;
        // Even an ambulance gives way to a car directly in its path.
        if (!positionBlockedByTraffic(nx, nz, 4.0f, busRouteX, busRouteZ)) {
            ambRouteX = nx;
            ambRouteZ = nz;
        }
    }
}

void drawAmbulanceOnRoad() {
    glPushMatrix();
    glTranslatef(ambRouteX, 0.45f, ambRouteZ);
    glRotatef(ambRouteHeadingDeg, 0, 1, 0);
    drawAmbulance(ambulanceSirenPhase);
    glPopMatrix();
}

// ---------------------------------------------------------
// Fill all four quadrants densely.
// NE / NW = three rows of apartments each (dense), plus a
//           small parking lot tucked in the open corner.
// SW = park, plus an extra apartment row further out.
// SE = pond, plus an extra apartment row further out.
// Pond and park sit directly off the main cross road - no
// separate stub roads leading to them.
// ---------------------------------------------------------
void placeQuadrants() {
    // NE quadrant (x>0, z>0)
    placeApartmentRow(14, 8, 60, 4.0f, 4, 180, 0);
    placeApartmentRow(28, 8, 60, 4.0f, 4, 180, 2);
    placeApartmentRow(42, 8, 60, 4.0f, 4, 180, 4);
    placeParkingLot(34, 58, 0);

    // NW quadrant (x<0, z>0)
    placeApartmentRow(14, -60, -8, 4.0f, 4, 180, 1);
    placeApartmentRow(28, -60, -8, 4.0f, 4, 180, 3);
    placeApartmentRow(42, -60, -8, 4.0f, 4, 180, 5);
    placeParkingLot(-34, 58, 0);

    // SW quadrant (x<0, z<0): park sits right beside the cross road
    drawPark(-26, -22);
    // School fills the open ground behind the park - closed and dark
    // at night except for its sign
    drawSchool(-34, -40, 0);
    // College angled 45 degrees to face the corner, like a real corner-lot
    // building - nudged slightly on Z so its wider diagonal footprint
    // still clears the school and the park behind it
    drawCollege(-50, -25.0f, 90);
    // School bus drop-off row, unchanged - still parked in front of the college

    // extra apartment row further out to fill the rest of the quadrant
    placeApartmentRow(-46, -60, -8, -4.0f, 4, 0, 2);

    // SE quadrant (x>0, z<0): pond sits right beside the cross road,
    // now doubling as the hospital's garden pond
    drawPond(26, -22, 8.0f);
    glPushMatrix(); glTranslatef(16, 0, -22); glRotatef(90, 0, 1, 0); drawBench(); glPopMatrix();
    glPushMatrix(); glTranslatef(36, 0, -22); glRotatef(-90, 0, 1, 0); drawBench(); glPopMatrix();
    float treeSpots[4][2] = { {14,-14}, {40,-14}, {14,-32}, {40,-32} };
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glTranslatef(treeSpots[i][0], 0, treeSpots[i][1]);
        drawTree();
        glPopMatrix();
    }
    // Hospital, flanked by a departmental store and a mosque, filling
    // the entire back strip of the quadrant with no gaps between them
    drawDepartmentalStore(16, -40, 0);
    drawHospital(35, -40, 0);
    drawMosque(53, -40, 0);
    // Paved plaza tying all three together, with parking, cars, and
    // trees/lamps in the small gaps between buildings
    drawCivicPlaza(8, 60, -35, -33);
    glPushMatrix(); glTranslatef(25.5f, 0, -34); drawTree(); glPopMatrix();
    glPushMatrix(); glTranslatef(44.5f, 0, -34); drawLampPost(); glPopMatrix();
    // extra apartment row further out to fill the rest of the quadrant
    placeApartmentRow(-46, 8, 60, -4.0f, 4, 0, 3);
}

// ---------------------------------------------------------
// Initialize cars: several per lane, well spaced so they
// never start on top of each other, slightly different speeds
// so relative motion looks natural. All loop endlessly.
// ---------------------------------------------------------
void initCars() {
    // Vertical main road: lane x=-2.2 goes +Z, lane x=+2.2 goes -Z
    // Positions ~half a road apart so they stay clear of each other
    cars[0] = { -2.2f, -55.0f, 'z', -63, 63,  1, 0.75f, 0.85f, 0.3f, 0.3f };
    cars[1] = { -2.2f,   5.0f, 'z', -63, 63,  1, 0.95f, 0.3f,  0.6f, 0.85f };
    cars[2] = { 2.2f,  55.0f, 'z', -63, 63, -1, 0.80f, 0.85f, 0.6f, 0.25f };
    cars[3] = { 2.2f,  -5.0f, 'z', -63, 63, -1, 1.00f, 0.4f,  0.8f, 0.4f };

    // Cross road: lane z=-2.2 goes +X, lane z=+2.2 goes -X
    cars[4] = { -55.0f, -2.2f, 'x', -63, 63,  1, 0.70f, 0.9f, 0.5f, 0.3f };
    cars[5] = { 5.0f, -2.2f, 'x', -63, 63,  1, 0.90f, 0.3f, 0.75f, 0.35f };
    cars[6] = { 55.0f,  2.2f, 'x', -63, 63, -1, 0.85f, 0.7f, 0.3f, 0.7f };
    cars[7] = { -5.0f,  2.2f, 'x', -63, 63, -1, 1.05f, 0.35f, 0.65f, 0.9f };
}

// ---------------------------------------------------------
// True distance along a looping 1-D road segment (accounts for wrap).
// Returns how far ahead "from" is of "to" in the travel direction dir.
// ---------------------------------------------------------
float loopAheadDist(float from, float to, float dir, float minC, float maxC) {
    float span = maxC - minC;
    float d = (to - from) * dir;
    if (d < 0) d += span;   // wrap around the loop
    return d;
}

// ---------------------------------------------------------
// Update + draw all cars. Same-lane cars keep a safe following
// gap so they never stack. Cars also yield to the school bus
// and ambulance so they never drive straight through them.
// ---------------------------------------------------------
void updateAndPlaceCars(float dt) {
    const float SAFE_GAP = 8.0f;      // car-to-car following distance
    const float BUS_GAP = 5.0f;      // car eases off near school bus / ambulance

    for (int i = 0; i < NUM_CARS; i++) {
        if (animateScene) {
            // Propose the next position at full speed
            float nextX = cars[i].x;
            float nextZ = cars[i].z;
            if (cars[i].axis == 'z') {
                nextZ += cars[i].dir * cars[i].speed * dt;
                if (nextZ > cars[i].maxC) nextZ = cars[i].minC;
                if (nextZ < cars[i].minC) nextZ = cars[i].maxC;
            }
            else {
                nextX += cars[i].dir * cars[i].speed * dt;
                if (nextX > cars[i].maxC) nextX = cars[i].minC;
                if (nextX < cars[i].minC) nextX = cars[i].maxC;
            }

            // Look for any other car in the same lane that is ahead and too close.
            bool blocked = false;
            for (int j = 0; j < NUM_CARS; j++) {
                if (j == i) continue;
                if (cars[j].axis != cars[i].axis) continue;
                bool sameLane = (cars[i].axis == 'z')
                    ? (fabsf(cars[i].x - cars[j].x) < 0.5f)
                    : (fabsf(cars[i].z - cars[j].z) < 0.5f);
                if (!sameLane) continue;
                if (cars[i].dir * cars[j].dir <= 0) continue;

                float fromCoord = (cars[i].axis == 'z') ? cars[i].z : cars[i].x;
                float toCoord = (cars[j].axis == 'z') ? cars[j].z : cars[j].x;
                float ahead = loopAheadDist(fromCoord, toCoord, cars[i].dir, cars[i].minC, cars[i].maxC);
                if (ahead < SAFE_GAP) {
                    blocked = true;
                    break;
                }
            }

            // Yield only if the school bus / ambulance is AHEAD in our lane
            // (not behind). That way cars in front clear the path, and cars
            // behind don't freeze the bus into a deadlock.
            if (!blocked && !isNight) {
                float bx = busRouteX - cars[i].x;
                float bz = busRouteZ - cars[i].z;
                float travelX = (cars[i].axis == 'z') ? 0.0f : cars[i].dir;
                float travelZ = (cars[i].axis == 'z') ? cars[i].dir : 0.0f;
                float along = bx * travelX + bz * travelZ;
                float side = bx * (-travelZ) + bz * travelX;
                if (along > 0.2f && along < BUS_GAP && fabsf(side) < 2.5f)
                    blocked = true;
            }
            if (!blocked) {
                float ax = ambRouteX - cars[i].x;
                float az = ambRouteZ - cars[i].z;
                float travelX = (cars[i].axis == 'z') ? 0.0f : cars[i].dir;
                float travelZ = (cars[i].axis == 'z') ? cars[i].dir : 0.0f;
                float along = ax * travelX + az * travelZ;
                float side = ax * (-travelZ) + az * travelX;
                if (along > 0.2f && along < BUS_GAP && fabsf(side) < 2.5f)
                    blocked = true;
            }

            if (!blocked) {
                cars[i].x = nextX;
                cars[i].z = nextZ;
            }
        }

        glPushMatrix();
        glTranslatef(cars[i].x, 0.45f, cars[i].z);
        if (cars[i].axis == 'z') {
            if (cars[i].dir < 0) glRotatef(180, 0, 1, 0);
        }
        else {
            glRotatef(cars[i].dir > 0 ? 90.0f : -90.0f, 0, 1, 0);
        }
        drawCarModel(cars[i].r, cars[i].g, cars[i].b, isNight);
        glPopMatrix();
    }
}

// ---------------------------------------------------------
// Initialize walking people
// ---------------------------------------------------------
void initPeople() {
    people[0] = { 20, 14.6f, 8, 60, 1, 0.6f, 1.0f, 0.8f, 0.2f, 0.2f };
    people[1] = { -20, 28.6f, -60, -8, -1, 0.5f, 1.0f, 0.2f, 0.5f, 0.8f };
    people[2] = { 20, -13.4f, 12, 40, 1, 0.45f, 1.0f, 0.8f, 0.6f, 0.2f };
    people[3] = { -29, -16, -32, -20, 1, 0.4f, 0.6f, 0.9f, 0.3f, 0.3f };
    people[4] = { -23, -26, -30, -18, -1, 0.42f, 0.6f, 0.3f, 0.8f, 0.4f };
    people[5] = { 40, 42.6f, 8, 60, -1, 0.55f, 1.0f, 0.6f, 0.3f, 0.7f };
}

// Students clustered around the school: some wait at the two bus stops
// on the local street, others roam near the school entrance / park edge.
void initStudents() {
    // stop A ~ (-48, -46), stop B ~ (-18, -46), school ~ (-34, -36)
    float waitPts[5][2] = {
        { -48.0f, -44.5f },  // stop A
        { -46.5f, -44.0f },  // stop A cluster
        { -18.0f, -44.5f },  // stop B
        { -16.5f, -44.0f },  // stop B cluster
        { -32.0f, -38.0f }   // near school entrance
    };
    float roamPts[5][2] = {
        { -42.0f, -40.0f },
        { -50.0f, -42.0f },
        { -22.0f, -38.0f },
        { -14.0f, -40.0f },
        { -30.0f, -34.0f }
    };
    float colors[5][3] = {
        { 0.2f, 0.55f, 0.9f },
        { 0.9f, 0.35f, 0.25f },
        { 0.3f, 0.75f, 0.35f },
        { 0.85f, 0.55f, 0.15f },
        { 0.65f, 0.3f, 0.8f }
    };
    for (int i = 0; i < NUM_STUDENTS; i++) {
        students[i].waitX = waitPts[i][0];
        students[i].waitZ = waitPts[i][1];
        students[i].roamX = roamPts[i][0];
        students[i].roamZ = roamPts[i][1];
        students[i].x = roamPts[i][0];
        students[i].z = roamPts[i][1];
        students[i].phase = (float)i / (float)NUM_STUDENTS;
        students[i].speed = 0.25f + 0.05f * i;
        students[i].scale = 0.55f + 0.05f * (i % 3); // child-sized
        students[i].r = colors[i][0];
        students[i].g = colors[i][1];
        students[i].b = colors[i][2];
        students[i].boarded = false;
        students[i].boardTimer = 0.0f;
    }
}

void updateAndPlaceStudents(float dt) {
    for (int i = 0; i < NUM_STUDENTS; i++) {
        if (students[i].boarded) {
            students[i].boardTimer -= dt;
            if (students[i].boardTimer <= 0.0f) {
                // Dropped off near school entrance after the ride
                students[i].boarded = false;
                students[i].x = -32.0f + (i - 2) * 1.2f;
                students[i].z = -37.0f;
                students[i].phase = 0.0f;
            }
            continue; // not drawn while boarded
        }

        if (animateScene) {
            // Walk back and forth between roam area and their bus stop
            students[i].phase += students[i].speed * 0.15f * dt;
            if (students[i].phase > 1.0f) students[i].phase = 0.0f;
            float t = students[i].phase;
            // Smooth ping-pong so they spend time at the stop
            float u = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
            students[i].x = lerp(students[i].roamX, students[i].waitX, u);
            students[i].z = lerp(students[i].roamZ, students[i].waitZ, u);
        }

        glPushMatrix();
        glTranslatef(students[i].x, 0, students[i].z);
        // Face roughly toward the street / bus
        float faceDx = students[i].waitX - students[i].roamX;
        if (faceDx < 0) glRotatef(180, 0, 1, 0);
        drawPerson(students[i].scale, students[i].r, students[i].g, students[i].b);
        glPopMatrix();
    }
}

void updateAndPlacePeople(float dt) {
    for (int i = 0; i < NUM_PEOPLE; i++) {
        if (animateScene) {
            people[i].z += people[i].dir * people[i].speed * dt;
            if (people[i].z > people[i].maxZ) { people[i].z = people[i].maxZ; people[i].dir = -1; }
            if (people[i].z < people[i].minZ) { people[i].z = people[i].minZ; people[i].dir = 1; }
        }
        glPushMatrix();
        glTranslatef(people[i].x, 0, people[i].z);
        if (people[i].dir < 0) glRotatef(180, 0, 1, 0);
        drawPerson(people[i].scale, people[i].r, people[i].g, people[i].b);
        glPopMatrix();
    }
}

// ---------------------------------------------------------
// Sun position + sky color, driven by mouse cursor
// ---------------------------------------------------------
void updateSunAndSky() {
    // Direct, monotonic mapping: moving the mouse left/right moves the
    // sun left/right in a straight line (no circular wraparound).
    // Moving the mouse DOWN lowers the sun smoothly across most of the
    // vertical range so night is easy to reach (not only at the very bottom).
    float xNorm = mouseXNorm;
    if (xNorm < 0) xNorm = 0;
    if (xNorm > 1) xNorm = 1;
    float elevation = 1.0f - mouseYNorm;        // up/down mouse -> sun height
    if (elevation < 0) elevation = 0;
    if (elevation > 1) elevation = 1;

    sunX = lerp(-70.0f, 70.0f, xNorm);          // straight left-right travel
    sunZ = -35.0f;                              // fixed depth, stays in view
    // Map elevation so the sun dips below the horizon more readily
    // (night threshold raised so ~bottom 40% of the screen is night)
    sunY = -12.0f + elevation * 70.0f;

    isNight = (elevation < 0.38f);

    float dayCol[3] = { 0.75f, 0.88f, 0.95f };
    float duskCol[3] = { 0.85f, 0.55f, 0.4f };
    float nightCol[3] = { 0.07f, 0.08f, 0.2f };

    if (elevation > 0.62f) {
        for (int i = 0; i < 3; i++) skyColor[i] = dayCol[i];
    }
    else if (elevation > 0.38f) {
        // Wider, smoother dusk band
        float t = (elevation - 0.38f) / (0.62f - 0.38f);
        for (int i = 0; i < 3; i++) skyColor[i] = lerp(duskCol[i], dayCol[i], t);
    }
    else {
        float t = elevation / 0.38f;
        for (int i = 0; i < 3; i++) skyColor[i] = lerp(nightCol[i], duskCol[i], t);
    }
}

void drawSun() {
    // Hide the sun disk once it is well below the horizon
    if (sunY < -8.0f) return;
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.9f, 0.5f);
    glPushMatrix();
    glTranslatef(sunX, sunY, sunZ);
    glutSolidSphere(4.0, 16, 16);
    glPopMatrix();
    if (lightingOn) glEnable(GL_LIGHTING);
}

void setupLighting() {
    if (lightingOn) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        float elevation = 1.0f - mouseYNorm;
        if (elevation < 0) elevation = 0;
        if (elevation > 1) elevation = 1;
        float brightness = 0.25f + 0.65f * elevation;

        GLfloat lightPos[] = { sunX, sunY, sunZ, 1.0f };
        GLfloat lightAmb[] = { brightness * 0.7f, brightness * 0.7f, brightness * 0.75f, 1.0f };
        GLfloat lightDiff[] = { brightness, brightness, brightness * 0.95f, 1.0f };
        GLfloat lightSpec[] = { 0.3f, 0.3f, 0.3f, 1.0f };

        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpec);
    }
    else {
        glDisable(GL_LIGHTING);
    }
}

// ---------------------------------------------------------
// Main display
// ---------------------------------------------------------
void display() {
    updateSunAndSky();

    glClearColor(skyColor[0], skyColor[1], skyColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);

    setupLighting();

    glLoadIdentity();
    float camX = camDistance * sinf(camAngle);
    float camZ = camDistance * cosf(camAngle);
    gluLookAt(camX, 32.0f, camZ,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f);

    drawSun();

    drawGround();
    drawStreet(-65, 0, 65, 0, 5.0f);
    drawStreet(0, -65, 0, 65, 5.0f);
    drawMainRoadMarkings();
    drawSidewalks();

    placeRoadDecor();
    placeQuadrants();
    updateAndPlaceCars(0.02f);
    updateAndPlacePeople(0.02f);
    updateAndPlaceStudents(0.02f);

    updateSchoolBusRoute(0.02f);
    drawSchoolBusOnRoad();

    updateAmbulanceRoute(0.02f);
    drawAmbulanceOnRoad();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w / h, 1.0, 400.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'l': case 'L': lightingOn = !lightingOn; break;
    case 'w': case 'W': wireframeMode = !wireframeMode; break;
    case 'a': case 'A': animateScene = !animateScene; break;
    case 27: exit(0); break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_LEFT)  camAngle -= 0.05f;
    if (key == GLUT_KEY_RIGHT) camAngle += 0.05f;
    if (key == GLUT_KEY_UP)    camDistance -= 2.0f;
    if (key == GLUT_KEY_DOWN)  camDistance += 2.0f;
    if (camDistance < 20) camDistance = 20;
    if (camDistance > 220) camDistance = 220;
    glutPostRedisplay();
}

// ---------------------------------------------------------
// Mouse movement -> controls the sun (works even with no
// button pressed, thanks to glutPassiveMotionFunc)
// ---------------------------------------------------------
void mouseMotion(int x, int y) {
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    mouseXNorm = (float)x / (float)w;
    mouseYNorm = (float)y / (float)h;
    glutPostRedisplay();
}

void idle() {
    glutPostRedisplay();
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    // IMPORTANT FIX: without this, glColor3f() has no effect while
    // lighting is on, and everything renders in OpenGL's default grey
    // material - this was the reason buildings looked washed out.
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initPeople();
    initStudents();
    initCars();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1200, 800);
    glutCreateWindow("3D Residential Area - Full Scene");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutPassiveMotionFunc(mouseMotion);
    glutMotionFunc(mouseMotion);
    glutIdleFunc(idle);

    glutMainLoop();
    return 0;
}