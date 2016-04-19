
/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* -------------------------------------------------------------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/
#ifndef _H_Ray
#define _H_Ray

#include"Base.h"
#include"Basis.h"
#include"PyMOLGlobals.h"

#define cRayMaxBasis 10

typedef struct _CRayAntiThreadInfo CRayAntiThreadInfo;
typedef struct _CRayHashThreadInfo CRayHashThreadInfo;
typedef struct _CRayThreadInfo CRayThreadInfo;

CRay *RayNew(PyMOLGlobals * G, int antialias);
void RayFree(CRay * I);
void RayPrepare(CRay * I, float v0, float v1, float v2,
                float v3, float v4, float v5,
                float fov, float *pos,
                float *mat, float *rotMat,
                float aspRat, int width, int height,
                float pixel_scale, int ortho, float pixel_ratio,
                float back_ratio, float magnified);
void RayRender(CRay * I, unsigned int *image,
               double timing, float angle, int antialias, unsigned int *return_bg);
void RayRenderPOV(CRay * I, int width, int height, char **headerVLA,
                  char **charVLA, float front, float back, float fov, float angle,
                  int antialias);

void RayRenderIDTF(CRay * I, char **node_vla, char **rsrc_vla);

void RayRenderVRML1(CRay * I, int width, int height,
                    char **vla_ptr, float front, float back,
                    float fov, float angle, float z_corr);
void RayRenderVRML2(CRay * I, int width, int height,
                    char **vla_ptr, float front, float back,
                    float fov, float angle, float z_corr);
void RayRenderCOLLADA(CRay * I, int width, int height,
                    char **vla_ptr, float front, float back, float fov);
void RayRenderObjMtl(CRay * I, int width, int height, char **objVLA_ptr,
                     char **mtlVLA_ptr, float front, float back, float fov,
                     float angle, float z_corr);
void RayRenderTest(CRay * I, int width, int height, float front, float back, float fov);
void RaySetTTT(CRay * I, int flag, float *ttt);
void RayGetTTT(CRay * I, float *ttt);
void RayPushTTT(CRay * I);
void RayPopTTT(CRay * I);
void RaySetContext(CRay * I, int context);
void RayRenderColorTable(CRay * I, int width, int height, int *image);
int RayTraceThread(CRayThreadInfo * T);
int RayGetNPrimitives(CRay * I);
void RayGetScaledAxes(CRay * I, float *xn, float *yn);

int RayHashThread(CRayHashThreadInfo * T);
int RayAntiThread(CRayAntiThreadInfo * T);

// JMS: added so they can be used in COLLADA.c
int RayExpandPrimitives(CRay * I);
int RayTransformFirst(CRay * I, int perspective, int identity);
void RayComputeBox(CRay * I);
int TriangleReverse(CPrimitive * p);


typedef struct {
  int op;
  int x1, y1, z1;
  int x2, y2, z2;
  int x3, y3, z3;
  int c;
  int r;
} G3dPrimitive;

G3dPrimitive *RayRenderG3d(CRay * I, int width, int height, float front,
                           float back, float fov, int quiet);

struct _CRay {

  // methods
  int sphere3fv(float *v, float r);
  int cylinder3fv(float *v1, float *v2, float r, float *c1, float *c2);
  int customCylinder3fv(float *v1, float *v2, float r, float *c1,
			     float *c2, int cap1, int cap2);
  int cone3fv(float *v1, float *v2, float r1, float r2, float *c1,
		   float *c2, int cap1, int cap2);
  int sausage3fv(float *v1, float *v2, float r, float *c1, float *c2);
  void color3fv(float *c);
  int triangle3fv(
		       float *v1, float *v2, float *v3,
		       float *n1, float *n2, float *n3, float *c1, float *c2, float *c3);
  int triangleTrans3fv(
			    float *v1, float *v2, float *v3,
			    float *n1, float *n2, float *n3,
			    float *c1, float *c2, float *c3,
			    float t1, float t2, float t3);
  void wobble(int mode, float *par);
  void transparentf(float t);
  int character(int char_id);
  void interiorColor3fv(float *v, int passive);
  int ellipsoid3fv(float *v, float r, float *n1, float *n2, float *n3);
  int setLastToNoLighting(char no_lighting);

  /* everything below should be private */
  PyMOLGlobals *G;
  CPrimitive *Primitive;
  int NPrimitive;
  CBasis *Basis;
  int NBasis;
  int *Vert2Prim;
  float CurColor[3], IntColor[3];
  float ModelView[16];
  float Rotation[16];
  float Volume[6];
  float Range[3];
  int BigEndian;
  int Wobble;
  float WobbleParam[3];
  float Trans;
  float Random[256];
  int TTTFlag;
  float TTT[16];
  float *TTTStackVLA;
  int TTTStackDepth;
  int Context;
  int CheckInterior;
  float AspRatio;
  int Width, Height;
  float PixelRadius;
  int Ortho;
  float min_box[3];
  float max_box[3];
  int Sampling;
  float PixelRatio;
  float Magnified;              /* ray pixels to screen pixels */
  float FrontBackRatio;
  double PrimSize;
  int PrimSizeCnt;
  float Fov, Pos[3];
  unsigned char *bkgrd_data;
  int bkgrd_width, bkgrd_height;
};

#endif
