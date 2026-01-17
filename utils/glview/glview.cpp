// Copyright Valve Corporation, All rights reserved.
//
// Program that displays visleaves.  It has been deprecated in the Orange Box
// (Source 2007) by Hammer's Map > Load Portal File feature.  Despite that,
// glview is still available for games made after Orange Box.
//
// See https://developer.valvesoftware.com/wiki/Glview
//
// YWB:  3/13/98
//
// You run the program like normal with any file.  If you want to read portals
// for the file type, you type:  glview -portal filename.gl0 (or whatever).
// glview will then try to read in the .prt file filename.prt.
//
// The portals are shown as white lines superimposed over your image.
//
// You can toggle the view between showing portals or not by hitting the '2'
// key.
//
// The '1' key toggles world polygons.
// The 'b' key toggles blending modes.
//
// If you don't want to depth buffer the portals, hit 'p'.
//
// The command line parsing is inelegant but functional.
//
// I sped up the KB movement and turn speed, too.

#define NOMINMAX
#include <gl/gl.h>
#include <gl/glu.h>

#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include "tier1/strtools.h"
#include "mathlib/mathlib.h"

#include "cmdlib.h"
#include "cmodel.h"
#include "physdll.h"
#include "phyfile.h"
#include "vphysics_interface.h"
#include "tools_minidump.h"

#include "glos.h"
#include "resource.h"

// Should be last one.
#include "tier0/memdbgon.h"

#define BENCHMARK_PHY 0

namespace {

constexpr inline float MOUSE_SENSITIVITY{0.2f};
constexpr inline float MOUSE_SENSITIVITY_X{MOUSE_SENSITIVITY * 1};
constexpr inline float MOUSE_SENSITIVITY_Y{MOUSE_SENSITIVITY * 1};

HDC camdc;
HGLRC baseRC;
HWND camerawindow;
HANDLE main_instance;

// Vars added by YWB
Vector g_Center;  // Center of all read points, so camera is in a sensible place
int g_nTotalPoints = 0;       // Total points read, for calculating center
int g_UseBlending = 0;        // Toggle to use blending mode or not
BOOL g_bReadPortals = 0;      // Did we read in a portal file?
BOOL g_bNoDepthPortals = 0;   // Do we zbuffer the lines of the portals?
int g_nPortalHighlight = -1;  // The leaf we're viewing
int g_nLeafHighlight = -1;    // The leaf we're viewing
BOOL g_bShowList1 = 1;        // Show regular polygons?
BOOL g_bShowList2 = 1;        // Show portals?
BOOL g_bShowLines = 0;        // Show outlines of faces
BOOL g_Active = TRUE;
BOOL g_Update = TRUE;
BOOL g_bDisp = FALSE;
IPhysicsCollision *physcollision = NULL;
// -----------

static int g_Keys[256];
void AppKeyDown(int key);
void AppKeyUp(int key);

BOOL ReadDisplacementFile(const char *filename);
void DrawDisplacementData();

// For abnormal program terminations
[[noreturn]] void Error(char *error, ...) {
  va_list argptr;
  char text[1024];

  va_start(argptr, error);
  V_vsprintf_safe(text, error, argptr);
  va_end(argptr);

  MessageBox(nullptr, text, "Visleaf Camera View - Error",
             MB_OK | MB_ICONERROR);

  exit(1);
}

Vector origin{32, 32, 48};
float angles[3];
Vector forward;
Vector vup, vpn, vright;
int width = 1024;
int height = 768;

float g_flMovementSpeed = 320.f;        // Units / second (run speed of HL)
constexpr inline float SPEED_TURN{90};  // Degrees / second

void KeyDown(int key) {
  switch (key) {
    case VK_ESCAPE:
      g_Active = FALSE;
      break;

    case VK_F1:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);
      break;

    case 'B':
      g_UseBlending ^= 1;
      if (g_UseBlending)
        glEnable(GL_BLEND);  // YWB TESTING
      else
        glDisable(GL_BLEND);
      break;

    case '1':
      g_bShowList1 ^= 1;
      break;
    case '2':
      g_bShowList2 ^= 1;
      break;
    case 'P':
      g_bNoDepthPortals ^= 1;
      break;
    case 'L':
      g_bShowLines ^= 1;
      break;
  }

  g_Update = TRUE;
}

static BOOL g_Capture = FALSE;

void Cam_MouseMoved() {
  if (g_Capture) {
    RECT rect;
    ::GetWindowRect(camerawindow, &rect);

    rect.top = max(0L, rect.top);
    rect.left = max(0L, rect.left);

    int centerx = (rect.left + rect.right) / 2;
    int centery = (rect.top + rect.bottom) / 2;

    POINT cursorPoint;
    GetCursorPos(&cursorPoint);
    SetCursorPos(centerx, centery);

    float deltax = (cursorPoint.x - centerx) * MOUSE_SENSITIVITY_X;
    float deltay = (cursorPoint.y - centery) * MOUSE_SENSITIVITY_Y;

    angles[1] -= deltax;
    angles[0] -= deltay;

    g_Update = TRUE;
  }
}

int Test_Key(int key) {
  int r = (g_Keys[key] != 0);

  g_Keys[key] &= 0x01;  // clear out debounce bit

  if (r) g_Update = TRUE;

  return r;
}

// UNDONE: Probably should change the controls to match the game - but I don't
// know who relies on them as of now.
void Cam_Update(float frametime) {
  if (Test_Key('W')) {
    VectorMA(origin, g_flMovementSpeed * frametime, vpn, origin);
  }
  if (Test_Key('S')) {
    VectorMA(origin, -g_flMovementSpeed * frametime, vpn, origin);
  }
  if (Test_Key('A')) {
    VectorMA(origin, -g_flMovementSpeed * frametime, vright, origin);
  }
  if (Test_Key('D')) {
    VectorMA(origin, g_flMovementSpeed * frametime, vright, origin);
  }

  if (Test_Key(VK_UP)) {
    VectorMA(origin, g_flMovementSpeed * frametime, forward, origin);
  }
  if (Test_Key(VK_DOWN)) {
    VectorMA(origin, -g_flMovementSpeed * frametime, forward, origin);
  }

  if (Test_Key(VK_LEFT)) {
    angles[1] += SPEED_TURN * frametime;
  }
  if (Test_Key(VK_RIGHT)) {
    angles[1] -= SPEED_TURN * frametime;
  }
  if (Test_Key('F')) {
    origin[2] += g_flMovementSpeed * frametime;
  }
  if (Test_Key('C')) {
    origin[2] -= g_flMovementSpeed * frametime;
  }
  if (Test_Key(VK_INSERT)) {
    angles[0] += SPEED_TURN * frametime;
    if (angles[0] > 85) angles[0] = 85;
  }
  if (Test_Key(VK_DELETE)) {
    angles[0] -= SPEED_TURN * frametime;
    if (angles[0] < -85) angles[0] = -85;
  }
  Cam_MouseMoved();
}

void Cam_BuildMatrix() {
  float matrix[4][4];

  float xa = DEG2RAD(angles[0]);
  float ya = DEG2RAD(angles[1]);

  // the movement matrix is kept 2d ?? do we want this?

  forward[0] = cos(ya);
  forward[1] = sin(ya);

  glGetFloatv(GL_PROJECTION_MATRIX, &matrix[0][0]);

  for (int i = 0; i < 3; i++) {
    vright[i] = matrix[i][0];
    vup[i] = matrix[i][1];
    vpn[i] = matrix[i][2];
  }

  VectorNormalize(vright);
  VectorNormalize(vup);
  VectorNormalize(vpn);
}

void Draw() {
  glClearColor(0.0, 0.0, 0.0, 0);  // Black Clearing YWB
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //
  // set up viewpoint
  //
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  float screenaspect = (float)width / height;
  float yfov = RAD2DEG(2 * atan((float)height / width));
  gluPerspective(yfov, screenaspect, 6, 20000);

  glRotatef(-90, 1, 0, 0);  // put Z going up
  glRotatef(90, 0, 0, 1);   // put Z going up
  glRotatef(angles[0], 0, 1, 0);
  glRotatef(-angles[1], 0, 0, 1);
  glTranslatef(-origin[0], -origin[1], -origin[2]);

  Cam_BuildMatrix();

  //
  // set drawing parms
  //
  glShadeModel(GL_SMOOTH);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glFrontFace(GL_CW);      // YWB   Carmack goes backward
  glCullFace(GL_BACK);     // Cull backfaces (qcsg used to spit out two sides,
                           // doesn't for -glview now)
  glEnable(GL_CULL_FACE);  // Enable face culling, just in case...
  glDisable(GL_TEXTURE_2D);

  // Blending function if enabled..
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (g_UseBlending) {
    glEnable(GL_BLEND);  // YWB TESTING
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // Enable face culling, just in case...
  } else {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
  }
  glDepthFunc(GL_LEQUAL);

  if (g_bDisp) {
    DrawDisplacementData();
  } else {
    //
    // draw the list
    //
    if (g_bShowList1) glCallList(1);

    if (g_bReadPortals) {
      if (g_bNoDepthPortals) glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);  // Disable face culling
      if (g_bShowList2) glCallList(2);
    };

    if (g_bShowLines) glCallList(3);
  }
}

void ReadPolyFileType(const char *name, int nList, BOOL drawLines) {
  int i, j, numverts;
  float v[8];
  int r;

  FILE *f = fopen(name, "rt");
  if (!f) Error("Couldn't open poly file '%s'.\n", name);

  float divisor = g_bReadPortals ? 2.0f : 1.0f;

  int c = 0;
  glNewList(nList, GL_COMPILE);

  for (i = 0; i < 3;
       i++)  // Find the center point so we can put the viewer there by default
    g_Center[i] = 0.0f;

  if (drawLines)  // Slight hilite
    glLineWidth(1.5);

  while (1) {
    r = fscanf(f, "%i\n", &numverts);
    if (!r || r == EOF) break;

    if (c > 65534 * 8) break;

    if (drawLines || numverts == 2)
      glBegin(GL_LINE_LOOP);
    else
      glBegin(GL_POLYGON);

    RunCodeAtScopeExit(glEnd());

    for (i = 0; i < numverts; i++) {
      r = fscanf(f, "%f %f %f %f %f %f\n", &v[0], &v[1], &v[2], &v[3], &v[4],
                 &v[5]);

      if (drawLines)  // YELLOW OUTLINES
        glColor4f(1.0f, 1.0f, 0.0f, 0.5f);
      else {
        if (g_bReadPortals)  // Gray scale it, leave portals blue
        {
          if (fabs(fabs(v[5]) - 1.0f) <
              0.01f)  // Is this a detail brush (color 0,0,1 blue)
          {
            glColor4f(v[3], v[4], v[5], 0.5f);
          } else  // Normal brush, gray scale it...
          {
            v[3] += v[4] + v[5];
            v[3] /= 3.0f;
            glColor4f(v[3] / divisor, v[3] / divisor, v[3] / divisor, 0.6f);
          }
        } else {
          v[3] = pow(v[3], 1.0f / 2.2f);
          v[4] = pow(v[4], 1.0f / 2.2f);
          v[5] = pow(v[5], 1.0f / 2.2f);

          glColor4f(v[3] / divisor, v[4] / divisor, v[5] / divisor,
                    0.6f);  // divisor is one, bright colors
        };
      };
      glVertex3f(v[0], v[1], v[2]);

      for (j = 0; j < 3; j++) {
        g_Center[j] += v[j];
      }

      g_nTotalPoints++;
    }

    c++;
  }

  fclose(f);

  glEndList();

  if (g_nTotalPoints > 0)  // Avoid division by zero
  {
    for (i = 0; i < 3; i++) {
      g_Center[i] = g_Center[i] / (float)g_nTotalPoints;  // Calculate center...
      origin[i] = g_Center[i];
    }
  }
}

#if BENCHMARK_PHY
#define NUM_COLLISION_TESTS 2500
#include "gametrace.h"
#include "fmtstr.h"

struct testlist_t {
  Vector start;
  Vector end;
  Vector normal;
  bool hit;
};

const float baselineTotal = 120.16f;
const float baselineRay = 28.25f;
const float baselineBox = 91.91f;
#define IMPROVEMENT_FACTOR(x, baseline) ((baseline) / (x))
#define IMPROVEMENT_PERCENT(x, baseline) \
  ((((baseline) - (x)) / (baseline)) * 100.0f)

testlist_t g_Traces[NUM_COLLISION_TESTS];
void Benchmark_PHY(const CPhysCollide *pCollide) {
  int i;
  Msg("Testing collision system\n");
  Vector start = vec3_origin;
  static Vector *targets = NULL;
  static bool first = true;
  static float test[2] = {1, 1};
  if (first) {
    float radius = 0;
    float theta = 0;
    float phi = 0;
    for (int i = 0; i < NUM_COLLISION_TESTS; i++) {
      radius += NUM_COLLISION_TESTS * 123.123f;
      radius = fabs(fmod(radius, 128));
      theta += NUM_COLLISION_TESTS * 0.76f;
      theta = fabs(fmod(theta, DEG2RAD(360)));
      phi += NUM_COLLISION_TESTS * 0.16666666f;
      phi = fabs(fmod(phi, DEG2RAD(180)));

      float st, ct, sp, cp;
      SinCos(theta, &st, &ct);
      SinCos(phi, &sp, &cp);
      st = sin(theta);
      ct = cos(theta);
      sp = sin(phi);
      cp = cos(phi);

      g_Traces[i].start.x = radius * ct * sp;
      g_Traces[i].start.y = radius * st * sp;
      g_Traces[i].start.z = radius * cp;
    }
    first = false;
  }

  float duration = 0;
  Vector size[2];
  size[0].Init(0, 0, 0);
  size[1].Init(16, 16, 16);
  unsigned int dots = 0;

#if VPROF_LEVEL > 0
  g_VProfCurrentProfile.Reset();
  g_VProfCurrentProfile.ResetPeaks();
  g_VProfCurrentProfile.Start();
#endif
  unsigned int hitCount = 0;
  double startTime = Plat_FloatTime();
  trace_t tr;
  for (i = 0; i < NUM_COLLISION_TESTS; i++) {
    physcollision->TraceBox(g_Traces[i].start, start, -size[0], size[0],
                            pCollide, vec3_origin, vec3_angle, &tr);
    if (tr.DidHit()) {
      g_Traces[i].end = tr.endpos;
      g_Traces[i].normal = tr.plane.normal;
      g_Traces[i].hit = true;
      hitCount++;
    } else {
      g_Traces[i].hit = false;
    }
  }
  for (i = 0; i < NUM_COLLISION_TESTS; i++) {
    physcollision->TraceBox(g_Traces[i].start, start, -size[1], size[1],
                            pCollide, vec3_origin, vec3_angle, &tr);
  }
  duration = Plat_FloatTime() - startTime;
  {
    unsigned int msSupp = physcollision->ReadStat(100);
    unsigned int msGJK = physcollision->ReadStat(101);
    unsigned int msMesh = physcollision->ReadStat(102);
    CFmtStr str("%d ms total %d ms gjk %d mesh solve\n", msSupp, msGJK, msMesh);
    OutputDebugStr(str.Access());
  }

#if VPROF_LEVEL > 0
  g_VProfCurrentProfile.MarkFrame();
  g_VProfCurrentProfile.Stop();
  g_VProfCurrentProfile.Reset();
  g_VProfCurrentProfile.ResetPeaks();
  g_VProfCurrentProfile.Start();
#endif
  hitCount = 0;
  startTime = Plat_FloatTime();
  for (i = 0; i < NUM_COLLISION_TESTS; i++) {
    physcollision->TraceBox(g_Traces[i].start, start, -size[0], size[0],
                            pCollide, vec3_origin, vec3_angle, &tr);
    if (tr.DidHit()) {
      g_Traces[i].end = tr.endpos;
      g_Traces[i].normal = tr.plane.normal;
      g_Traces[i].hit = true;
      hitCount++;
    } else {
      g_Traces[i].hit = false;
    }
#if VPROF_LEVEL > 0
    g_VProfCurrentProfile.MarkFrame();
#endif
  }
  double midTime = Plat_FloatTime();
  for (i = 0; i < NUM_COLLISION_TESTS; i++) {
    physcollision->TraceBox(g_Traces[i].start, start, -size[1], size[1],
                            pCollide, vec3_origin, vec3_angle, &tr);
#if VPROF_LEVEL > 0
    g_VProfCurrentProfile.MarkFrame();
#endif
  }
  double endTime = Plat_FloatTime();
  duration = endTime - startTime;
  {
    CFmtStr str("%d collisions in %.2f ms [%.2f X] %d hits\n",
                NUM_COLLISION_TESTS, duration * 1000,
                IMPROVEMENT_FACTOR(duration * 1000.0f, baselineTotal),
                hitCount);
    OutputDebugStr(str.Access());
  }
  {
    float rayTime = (midTime - startTime) * 1000.0f;
    float boxTime = (endTime - midTime) * 1000.0f;
    CFmtStr str("%.2f ms rays [%.2f X] %.2f ms boxes [%.2f X]\n", rayTime,
                IMPROVEMENT_FACTOR(rayTime, baselineRay), boxTime,
                IMPROVEMENT_FACTOR(boxTime, baselineBox));
    OutputDebugStr(str.Access());
  }

  {
    unsigned int msSupp = physcollision->ReadStat(100);
    unsigned int msGJK = physcollision->ReadStat(101);
    unsigned int msMesh = physcollision->ReadStat(102);
    CFmtStr str("%d ms total %d ms gjk %d mesh solve\n", msSupp, msGJK, msMesh);
    OutputDebugStr(str.Access());
  }
#if VPROF_LEVEL > 0
  g_VProfCurrentProfile.Stop();
  g_VProfCurrentProfile.OutputReport(VPRT_FULL & ~VPRT_HIERARCHY, NULL);
#endif

  // draw the traces in yellow
  glColor3f(1.0f, 1.0f, 0.0f);

  glBegin(GL_LINES);
  RunCodeAtScopeExit(glEnd());

  for (int i = 0; i < NUM_COLLISION_TESTS; i++) {
    if (!g_Traces[i].hit) continue;
    glVertex3fv(g_Traces[i].end.Base());
    Vector tmp = g_Traces[i].end + g_Traces[i].normal * 10.0f;
    glVertex3fv(tmp.Base());
  }
}
#endif

struct phyviewparams_t {
  Vector mins;
  Vector maxs;
  Vector offset;
  QAngle angles;
  int outputType;

  void Defaults() {
    ClearBounds(mins, maxs);
    offset.Init();
    outputType = GL_POLYGON;
    angles.Init();
  }
};

void AddVCollideToList(phyheader_t &header, vcollide_t &collide,
                       phyviewparams_t &params) {
  matrix3x4_t xform;
  AngleMatrix(params.angles, params.offset, xform);
  ClearBounds(params.mins, params.maxs);

  for (int i = 0; i < header.solidCount; i++) {
    ICollisionQuery *pQuery =
        physcollision->CreateQueryModel(collide.solids[i]);

    for (int j = 0; j < pQuery->ConvexCount(); j++) {
      for (int k = 0; k < pQuery->TriangleCount(j); k++) {
        Vector verts[3];
        pQuery->GetTriangleVerts(j, k, verts);
        Vector v0, v1, v2;
        VectorTransform(verts[0], xform, v0);
        VectorTransform(verts[1], xform, v1);
        VectorTransform(verts[2], xform, v2);
        AddPointToBounds(v0, params.mins, params.maxs);
        AddPointToBounds(v1, params.mins, params.maxs);
        AddPointToBounds(v2, params.mins, params.maxs);

        glBegin(params.outputType);
        RunCodeAtScopeExit(glEnd());

        glColor3ub(255, 0, 0);
        glVertex3fv(v0.Base());
        glColor3ub(0, 255, 0);
        glVertex3fv(v1.Base());
        glColor3ub(0, 0, 255);
        glVertex3fv(v2.Base());
      }
    }
    physcollision->DestroyQueryModel(pQuery);
  }
}

void ReadPHYFile(const char *name, phyviewparams_t &params) {
  FILE *fp = fopen(name, "rb");
  if (!fp) Error("Couldn't open PHY file '%s'.\n", name);

  RunCodeAtScopeExit(fclose(fp));

  phyheader_t header;
  fread(&header, sizeof(header), 1, fp);
  if (header.size != sizeof(header) || header.solidCount <= 0) {
    return;
  }

  int pos = ftell(fp);
  fseek(fp, 0, SEEK_END);
  int fileSize = ftell(fp) - pos;
  fseek(fp, pos, SEEK_SET);

  char *buf = (char *)_alloca(fileSize);
  fread(buf, fileSize, 1, fp);

  vcollide_t collide;
  physcollision->VCollideLoad(&collide, header.solidCount, (const char *)buf,
                              fileSize);
#if BENCHMARK_PHY
  Benchmark_PHY(collide.solids[0]);
#endif
  AddVCollideToList(header, collide, params);
}

void ReadPolyFile(const char *name) {
  char ext[4];
  V_ExtractFileExtension(name, ext);

  bool isPHY = !Q_stricmp(ext, "phy");
  if (isPHY) {
    CreateInterfaceFnT<IPhysicsCollision> physicsFactory = GetPhysicsFactory();
    physcollision =
        physicsFactory(VPHYSICS_COLLISION_INTERFACE_VERSION, nullptr);
    if (physcollision) {
      phyviewparams_t params;
      params.Defaults();
      glNewList(1, GL_COMPILE);
      ReadPHYFile(name, params);
      origin = (params.mins + params.maxs) * 0.5;
      glEndList();
    }
  } else {
    // Read in polys...
    ReadPolyFileType(name, 1, false);

    // Make list 3 just the lines... so we can draw outlines
    ReadPolyFileType(name, 3, true);
  }
}

void ReadPortalFile(char *name) {
  int i, numverts;
  float v[8];
  int c;
  int r;

  // For Portal type reading...
  char szDummy[80];
  int nNumLeafs;
  int nNumPortals;
  int nLeafIndex[2];

  FILE *f = fopen(name, "r");
  if (!f) Error("Couldn't open portal file '%s'.\n", name);

  RunCodeAtScopeExit(fclose(f));

  c = 0;

  glNewList(2, GL_COMPILE);

  // Read in header
  fscanf(f, "%79s\n", szDummy);
  // dimhotepus: Ensure zero-terminated read.
  szDummy[ssize(szDummy) - 1] = '\0';

  fscanf(f, "%i\n", &nNumLeafs);
  fscanf(f, "%i\n", &nNumPortals);

  glLineWidth(1.5);

  while (1) {
    r = fscanf(f, "%i %i %i ", &numverts, &nLeafIndex[0], &nLeafIndex[1]);
    if (r != 3) break;

    glBegin(GL_LINE_LOOP);
    RunCodeAtScopeExit(glEnd());

    for (i = 0; i < numverts; i++) {
      r = fscanf(f, "(%f %f %f )\n", &v[0], &v[1], &v[2]);
      if (r != 3) break;

      if (c == g_nPortalHighlight || nLeafIndex[0] == g_nLeafHighlight ||
          nLeafIndex[1] == g_nLeafHighlight) {
        glColor4f(1.0, 0.0, 0.0, 1.0);
      } else {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);  // WHITE portals
      }
      glVertex3f(v[0], v[1], v[2]);
    }

    c++;
  }

  glEndList();
}

#define MAX_DISP_COUNT 4096
static Vector dispPoints[MAX_DISP_COUNT];
static Vector dispNormals[MAX_DISP_COUNT];
static int dispPointCount = 0;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL ReadDisplacementFile(const char *filename) {
  //
  // open the file
  //
  FILE *pFile = fopen(filename, "r");
  if (!pFile) Error("Couldn't open dsplacement file '%s'.\n", filename);

  RunCodeAtScopeExit(fclose(pFile));

  //
  // read data in file
  //
  while (1) {
    // overflow test
    if (dispPointCount >= ssize(dispPoints)) break;

    int fileCount =
        fscanf(pFile, "%f %f %f %f %f %f", &dispPoints[dispPointCount][0],
               &dispPoints[dispPointCount][1], &dispPoints[dispPointCount][2],
               &dispNormals[dispPointCount][0], &dispNormals[dispPointCount][1],
               &dispNormals[dispPointCount][2]);
    dispPointCount++;

    // end of file check
    if (!fileCount || (fileCount == EOF)) break;
  }

  return TRUE;
}

void DrawDisplacementData() {
  int i, j;

  GLUquadricObj *pObject = gluNewQuadric();

  glEnable(GL_DEPTH_TEST);

  for (i = 0; i < dispPointCount; i++) {
    // draw a sphere where the point is (in red)
    glColor3f(1.0f, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(dispPoints[i][0], dispPoints[i][1], dispPoints[i][2]);
    gluSphere(pObject, 5, 5, 5);
    glPopMatrix();

    // draw the normal (in yellow)
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    RunCodeAtScopeExit(glEnd());

    glVertex3f(dispPoints[i][0], dispPoints[i][1], dispPoints[i][2]);
    glVertex3f(dispPoints[i][0] + (dispNormals[i][0] * 50.0f),
               dispPoints[i][1] + (dispNormals[i][1] * 50.0f),
               dispPoints[i][2] + (dispNormals[i][2] * 50.0f));
  }

  int halfCount = dispPointCount / 2;
  int halfWidth = static_cast<int>(sqrt((float)halfCount));

  glDisable(GL_CULL_FACE);

  glColor3f(0.0f, 0.0f, 1.0f);
  for (i = 0; i < halfWidth - 1; i++) {
    for (j = 0; j < halfWidth - 1; j++) {
      glBegin(GL_POLYGON);
      RunCodeAtScopeExit(glEnd());

      glVertex3f(dispPoints[i * halfWidth + j][0],
                 dispPoints[i * halfWidth + j][1],
                 dispPoints[i * halfWidth + j][2]);
      glVertex3f(dispPoints[(i + 1) * halfWidth + j][0],
                 dispPoints[(i + 1) * halfWidth + j][1],
                 dispPoints[(i + 1) * halfWidth + j][2]);
      glVertex3f(dispPoints[(i + 1) * halfWidth + (j + 1)][0],
                 dispPoints[(i + 1) * halfWidth + (j + 1)][1],
                 dispPoints[(i + 1) * halfWidth + (j + 1)][2]);
      glVertex3f(dispPoints[i * halfWidth + (j + 1)][0],
                 dispPoints[i * halfWidth + (j + 1)][1],
                 dispPoints[i * halfWidth + (j + 1)][2]);
    }
  }

  glColor3f(0.0f, 1.0f, 0.0f);
  for (i = 0; i < halfWidth - 1; i++) {
    for (j = 0; j < halfWidth - 1; j++) {
      glBegin(GL_POLYGON);
      RunCodeAtScopeExit(glEnd());

      glVertex3f(dispPoints[i * halfWidth + j][0] +
                     (dispNormals[i * halfWidth + j][0] * 150.0f),
                 dispPoints[i * halfWidth + j][1] +
                     (dispNormals[i * halfWidth + j][1] * 150.0f),
                 dispPoints[i * halfWidth + j][2] +
                     (dispNormals[i * halfWidth + j][2] * 150.0f));

      glVertex3f(dispPoints[(i + 1) * halfWidth + j][0] +
                     (dispNormals[(i + 1) * halfWidth + j][0] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + j][1] +
                     (dispNormals[(i + 1) * halfWidth + j][1] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + j][2] +
                     (dispNormals[(i + 1) * halfWidth + j][2] * 150.0f));

      glVertex3f(dispPoints[(i + 1) * halfWidth + (j + 1)][0] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][0] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + (j + 1)][1] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][1] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + (j + 1)][2] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][2] * 150.0f));

      glVertex3f(dispPoints[i * halfWidth + (j + 1)][0] +
                     (dispNormals[i * halfWidth + (j + 1)][0] * 150.0f),
                 dispPoints[i * halfWidth + (j + 1)][1] +
                     (dispNormals[i * halfWidth + (j + 1)][1] * 150.0f),
                 dispPoints[i * halfWidth + (j + 1)][2] +
                     (dispNormals[i * halfWidth + (j + 1)][2] * 150.0f));
    }
  }

  glDisable(GL_DEPTH_TEST);

  glColor3f(0.0f, 0.0f, 1.0f);
  for (i = 0; i < halfWidth - 1; i++) {
    for (j = 0; j < halfWidth - 1; j++) {
      glBegin(GL_LINE_LOOP);
      RunCodeAtScopeExit(glEnd());

      glVertex3f(dispPoints[i * halfWidth + j][0] +
                     (dispNormals[i * halfWidth + j][0] * 150.0f),
                 dispPoints[i * halfWidth + j][1] +
                     (dispNormals[i * halfWidth + j][1] * 150.0f),
                 dispPoints[i * halfWidth + j][2] +
                     (dispNormals[i * halfWidth + j][2] * 150.0f));

      glVertex3f(dispPoints[(i + 1) * halfWidth + j][0] +
                     (dispNormals[(i + 1) * halfWidth + j][0] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + j][1] +
                     (dispNormals[(i + 1) * halfWidth + j][1] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + j][2] +
                     (dispNormals[(i + 1) * halfWidth + j][2] * 150.0f));

      glVertex3f(dispPoints[(i + 1) * halfWidth + (j + 1)][0] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][0] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + (j + 1)][1] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][1] * 150.0f),
                 dispPoints[(i + 1) * halfWidth + (j + 1)][2] +
                     (dispNormals[(i + 1) * halfWidth + (j + 1)][2] * 150.0f));

      glVertex3f(dispPoints[i * halfWidth + (j + 1)][0] +
                     (dispNormals[i * halfWidth + (j + 1)][0] * 150.0f),
                 dispPoints[i * halfWidth + (j + 1)][1] +
                     (dispNormals[i * halfWidth + (j + 1)][1] * 150.0f),
                 dispPoints[i * halfWidth + (j + 1)][2] +
                     (dispNormals[i * halfWidth + (j + 1)][2] * 150.0f));
    }
  }

  gluDeleteQuadric(pObject);
}

//=====================================================================

BOOL bSetupPixelFormat(HDC hDC) {
  static PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
      1,                              // version number
      PFD_DRAW_TO_WINDOW |            // support window
          PFD_SUPPORT_OPENGL |        // support OpenGL
          PFD_DOUBLEBUFFER,           // double buffered
      PFD_TYPE_RGBA,                  // RGBA type
      24,                             // 24-bit color depth
      0,
      0,
      0,
      0,
      0,
      0,  // color bits ignored
      0,  // no alpha buffer
      0,  // shift bit ignored
      0,  // no accumulation buffer
      0,
      0,
      0,
      0,               // accum bits ignored
      32,              // 32-bit z-buffer
      0,               // no stencil buffer
      0,               // no auxiliary buffer
      PFD_MAIN_PLANE,  // main layer
      0,               // reserved
      0,
      0,
      0  // layer masks ignored
  };

  int pixelformat = 0;

  if ((pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0) {
    Error("ChoosePixelFormat failed: '%s'.\n",
          std::system_category().message(::GetLastError()).c_str());
  }

  if (!SetPixelFormat(hDC, pixelformat, &pfd)) {
    Error("SetPixelFormat failed: '%s'.\n",
          std::system_category().message(::GetLastError()).c_str());
  }

  return TRUE;
}

/*
============
CameraWndProc
============
*/
LRESULT WINAPI WCam_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  LRESULT lRet = 1;

  RECT rect;
  GetClientRect(hWnd, &rect);

  switch (uMsg) {
    case WM_CREATE: {
      camdc = GetDC(hWnd);
      bSetupPixelFormat(camdc);

      baseRC = ::wglCreateContext(camdc);
      if (!baseRC) {
        Error("wglCreateContext failed: '%s'.\n",
              std::system_category().message(::GetLastError()).c_str());
      }

      if (!::wglMakeCurrent(camdc, baseRC)) {
        Error("wglMakeCurrent failed: '%s'.\n",
              std::system_category().message(::GetLastError()).c_str());
      }

      glCullFace(GL_FRONT);
      glEnable(GL_CULL_FACE);
    } break;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hWnd, &ps);
      RunCodeAtScopeExit(EndPaint(hWnd, &ps));

      if (!::wglMakeCurrent(camdc, baseRC)) {
        Error("wglMakeCurrent failed: '%s'.\n",
              std::system_category().message(::GetLastError()).c_str());
      }

      Draw();
      SwapBuffers(camdc);
    } break;

    case WM_KEYDOWN:
      KeyDown(static_cast<int>(wParam));
      AppKeyDown(static_cast<int>(wParam));
      break;

    case WM_KEYUP:
      AppKeyUp(static_cast<int>(wParam));
      break;

    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDOWN:
      SetCapture(camerawindow);
      ShowCursor(FALSE);
      g_Capture = TRUE;
      break;

    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_LBUTTONUP:
      if (!(wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) {
        g_Capture = FALSE;
        ReleaseCapture();
        ShowCursor(TRUE);
      }
      break;

    case WM_SIZE:
      InvalidateRect(camerawindow, NULL, false);
      break;

    case WM_NCCALCSIZE:  // don't let windows copy pixels
      lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
      return WVR_REDRAW;

    case WM_CLOSE:
      /* call destroy window to cleanup and go away */
      DestroyWindow(hWnd);
      break;

    case WM_DESTROY: {
      /* release and free the device context and rendering context */
      HGLRC hRC = wglGetCurrentContext();
      HDC hDC = wglGetCurrentDC();

      wglMakeCurrent(NULL, NULL);

      if (hRC) wglDeleteContext(hRC);
      if (hDC) ReleaseDC(hWnd, hDC);

      PostQuitMessage(0);
    } break;

    default:
      /* pass all unhandled messages to DefWindowProc */
      lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
      break;
  }

  /* return 1 if handled message, 0 if not */
  return lRet;
}

/*
==============
WCam_Register
==============
*/
void WCam_Register(HINSTANCE hInstance, const char *className) {
  /* Register the camera class */
  WNDCLASS wc = {};
  wc.style = 0;
  wc.lpfnWndProc = WCam_WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(SE_IDI_APP_MAIN));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszMenuName = 0;
  wc.lpszClassName = className;

  if (!RegisterClass(&wc)) {
    Error("Unable to register window class '%s': '%s'.\n",
          std::system_category().message(::GetLastError()).c_str());
  }
}

void WCam_Create(HINSTANCE hInstance) {
  constexpr char className[]{"Valve_GL_View"};

  WCam_Register(hInstance, className);

  int w = ::width;
  int h = ::height;

  int nScx = ::GetSystemMetrics(SM_CXSCREEN);
  int nScy = ::GetSystemMetrics(SM_CYSCREEN);

  // Center it
  int x = (nScx - w) / 2;
  int y = (nScy - h) / 2;

  camerawindow =
      CreateWindow(className, "Visleaf Camera View",
                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                       WS_MAXIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                   x, y, w, h, nullptr, 0, hInstance, 0);

  if (!camerawindow) {
    Error("Can't create Visleaf Camera View window: '%s'.\n",
          std::system_category().message(::GetLastError()).c_str());
  }

  // dimhotepus: Apply dark mode and Mica theme.
  Plat_ApplySystemTitleBarTheme(camerawindow, SystemBackdropType::MainWindow);

  ::ShowWindow(camerawindow, SW_SHOWDEFAULT);
}

void AppKeyDown(int key) {
  key &= 0xFF;

  g_Keys[key] = 0x03;  // add debounce bit
}

void AppKeyUp(int key) {
  key &= 0xFF;

  g_Keys[key] &= 0x02;
}

void AppRender() {
  static double lastTime = 0;

  double time = Plat_FloatTime();
  float frametime = static_cast<float>(time - lastTime);

  // clamp too large frames (like first frame)
  if (frametime > 0.2f) frametime = 0.2f;

  lastTime = time;

  if (!::wglMakeCurrent(camdc, baseRC)) {
    Error("wglMakeCurrent failed: '%s'\n",
          std::system_category().message(::GetLastError()).c_str());
  }

  Cam_Update(frametime);

  if (g_Update) {
    Draw();
    SwapBuffers(camdc);

    g_Update = FALSE;
  } else {
    ThreadSleep(1);
  }
}

SpewRetval_t GlViewSpew(SpewType_t type, const char *pMsg) {
  Plat_DebugString(pMsg);

  if (type == SPEW_ASSERT) return SPEW_DEBUGGER;
  if (type == SPEW_ERROR) return SPEW_ABORT;

  return SPEW_CONTINUE;
}

}  // namespace

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  CommandLine()->CreateCmdLine(Plat_GetCommandLine());

  if (CommandLine()->ParmCount() == 0) Error("Please specify file to view.\n");

  MathLib_Init(2.2f, 2.2f, 0.0f, 2);

  main_instance = hInstance;

  WCam_Create(hInstance);

  // Last argument is the file name
  const char *pFileName =
      CommandLine()->GetParm(CommandLine()->ParmCount() - 1);
  const ScopedFileSystem scopedFileSystem(pFileName);

  if (CommandLine()->CheckParm("-portal")) {
    g_bReadPortals = 1;
    g_nPortalHighlight = CommandLine()->ParmValue("-portalhighlight", -1);
    g_nLeafHighlight = CommandLine()->ParmValue("-leafhighlight", -1);
  }

  g_flMovementSpeed =
      static_cast<float>(CommandLine()->ParmValue("-speed", 320));

  if (CommandLine()->CheckParm("-disp")) {
    ReadDisplacementFile(pFileName);
    g_bDisp = TRUE;
  }

  const ScopedSpewOutputFunc scoped_spew_output(GlViewSpew);

  // Any chunk of original left is the filename.
  if (pFileName && pFileName[0] && !g_bDisp) {
    ReadPolyFile(pFileName);
  }

  if (g_bReadPortals) {
    // Copy file again and this time look for the . from .gl? so we can
    // concatenate .prt and open the portal file.
    char szTempCmd[MAX_PATH];
    V_strcpy_safe(szTempCmd, pFileName);

    char *pTmp = szTempCmd;
    while (*pTmp && *pTmp != '.') {
      pTmp++;
    }

    *pTmp = '\0';
    V_strcat_safe(szTempCmd, ".prt");

    ReadPortalFile(szTempCmd);
  };

  MSG msg = {};
  /* main window message loop */
  while (g_Active) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    AppRender();
  }

  /* return status of application */
  return static_cast<int>(msg.wParam);
}
