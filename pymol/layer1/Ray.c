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
-*   Larry Coopet (various optimizations)
-* 
-*
Z* -------------------------------------------------------------------
*/
#include"os_predef.h"
#include"os_std.h"

#include"Base.h"
#include"MemoryDebug.h"
#include"Err.h"
#include"Vector.h"
#include"OOMac.h"
#include"Setting.h"
#include"Ortho.h"
#include"Util.h"
#include"Ray.h"
#include"Triangle.h" 
#include"Color.h"
#include"Matrix.h"
#include"P.h"
#include"MemoryCache.h"
#include"Character.h"
#include"Text.h"
#include"PyMOL.h"

#ifdef _PYMOL_INLINE
#undef _PYMOL_INLINE
#include"Basis.c"
#define _PYMOL_INLINE
#endif

#ifndef RAY_SMALL
#define RAY_SMALL 0.00001
#endif

/* BASES 
   0 contains untransformed vertices (vector size = 3)
	1 contains transformed vertices (vector size = 3)
	2 contains transformed vertices for shadowing 
*/

#define MAX_RAY_THREADS 12

typedef float float3[3];
typedef float float4[4];

struct _CRayThreadInfo {
  CRay *ray;
  int width,height;
  unsigned int *image;
  float front,back;
  unsigned int fore_mask;
  float *bkrd;
  unsigned int background;
  int border;
  int phase, n_thread;
  float spec_vector[3];
  int x_start,x_stop;
  int y_start,y_stop;
  unsigned int *edging;
  unsigned int edging_cutoff;
  int perspective;
  float fov,pos[3];
};

 struct _CRayHashThreadInfo {
   CBasis *basis;
   int *vert2prim;
   CPrimitive *prim;
   float *clipBox;
   unsigned int *image;
   unsigned int background;
   unsigned int bytes;
   int perspective;
   float front;
   int phase;
   CRay *ray;
 };

 struct _CRayAntiThreadInfo {
  unsigned int *image;
  unsigned int *image_copy;
  unsigned int width,height;
  int mag;
  int phase,n_thread;
  CRay *ray;
};

void RayRelease(CRay *I);
void RayWobble(CRay *I,int mode,float *v);
void RayTransparentf(CRay *I,float v);

void RaySetup(CRay *I);
void RayColor3fv(CRay *I,float *v);
void RaySphere3fv(CRay *I,float *v,float r);
void RayCharacter(CRay *I,int char_id, float xorig, float yorig, float advance);
void RayCylinder3fv(CRay *I,float *v1,float *v2,float r,float *c1,float *c2);
void RaySausage3fv(CRay *I,float *v1,float *v2,float r,float *c1,float *c2);

void RayTriangle3fv(CRay *I,
						  float *v1,float *v2,float *v3,
						  float *n1,float *n2,float *n3,
						  float *c1,float *c2,float *c3);

void RayApplyMatrix33( unsigned int n, float3 *q, const float m[16],
							float3 *p );
void RayApplyMatrixInverse33( unsigned int n, float3 *q, const float m[16],
                              float3 *p );

void RayExpandPrimitives(CRay *I);
void RayTransformBasis(CRay *I,CBasis *B,int group_id);

int PrimitiveSphereHit(CRay *I,float *v,float *n,float *minDist,int except);

void RayTransformNormals33( unsigned int n, float3 *q, const float m[16],float3 *p );
void RayTransformInverseNormals33( unsigned int n, float3 *q, const float m[16],float3 *p );
void RayProjectTriangle(CRay *I,RayInfo *r,float *light,float *v0,float *n0,float scale);
void RayCustomCylinder3fv(CRay *I,float *v1,float *v2,float r,
                          float *c1,float *c2,int cap1,int cap2);
void RaySetContext(CRay *I,int context)
{
  I->Context=context;
}
void RayApplyContextToNormal(CRay *I,float *v);
void RayApplyContextToVertex(CRay *I,float *v);

void RayApplyContextToVertex(CRay *I,float *v)
{
  switch(I->Context) {
  case 1:
    {
      float tw;
      float th;
      
      if(I->AspRatio>1.0F) {
        tw = I->AspRatio;
        th = 1.0F;
      } else {
        th = 1.0F/I->AspRatio;
        tw = 1.0F;
      }
      v[0]+=(tw-1.0F)/2;
      v[1]+=(th-1.0F)/2;
      v[0]=v[0]*(I->Range[0]/tw)+I->Volume[0];
      v[1]=v[1]*(I->Range[1]/th)+I->Volume[2];
      v[2]=v[2]*I->Range[2]-(I->Volume[4]+I->Volume[5])/2.0F;
      RayApplyMatrixInverse33(1,(float3*)v,I->ModelView,(float3*)v);    

      /* TO DO: factor out the perspective division */

    }
    break;
  }
}
void RayApplyContextToNormal(CRay *I,float *v)
{
  switch(I->Context) {
  case 1:
    RayTransformInverseNormals33(1,(float3*)v,I->ModelView,(float3*)v);    
    break;
  }
}
int RayGetNPrimitives(CRay *I)    
{
  return(I->NPrimitive);
}
/*========================================================================*/
#ifdef _PYMOL_INLINE
__inline__
#endif
static void RayGetSphereNormal(CRay *I,RayInfo *r)
{
  
  r->impact[0]=r->base[0]; 
  r->impact[1]=r->base[1]; 
  r->impact[2]=r->base[2]-r->dist;
  
  r->surfnormal[0]=r->impact[0]-r->sphere[0];
  r->surfnormal[1]=r->impact[1]-r->sphere[1];
  r->surfnormal[2]=r->impact[2]-r->sphere[2];
  
  normalize3f(r->surfnormal);
}

#ifdef _PYMOL_INLINE
__inline__
#endif
static void RayGetSphereNormalPerspective(CRay *I,RayInfo *r)
{
  
  r->impact[0]=r->base[0] + r->dist*r->dir[0];
  r->impact[1]=r->base[1] + r->dist*r->dir[1];
  r->impact[2]=r->base[2] + r->dist*r->dir[2];
  
  r->surfnormal[0]=r->impact[0]-r->sphere[0];
  r->surfnormal[1]=r->impact[1]-r->sphere[1];
  r->surfnormal[2]=r->impact[2]-r->sphere[2];
  
  normalize3f(r->surfnormal);
}

static void fill(unsigned int *buffer, unsigned int value,unsigned int cnt)
{
  while(cnt&0xFFFFFF80) {
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    cnt-=0x20;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
    *(buffer++) = value;
  }
  while(cnt--) {
    *(buffer++) = value;
  }
}
/*========================================================================*/
#ifdef _PYMOL_INLINE
__inline__
#endif
static void RayReflectAndTexture(CRay *I,RayInfo *r,int perspective)
{

  if(r->prim->wobble)
    switch(r->prim->wobble) {
    case 1:
      scatter3f(r->surfnormal,I->WobbleParam[0]);
      break;
    case 2:
      wiggle3f(r->surfnormal,r->impact,I->WobbleParam);
      break;
    case 3: 
      {
        float3 v;
        float3 n;
        copy3f(r->impact,v);
        RayApplyMatrixInverse33(1,&v,I->ModelView,&v);
        n[0]=(float)cos((v[0]+v[1]+v[2])*I->WobbleParam[1]);
        n[1]=(float)cos((v[0]-v[1]+v[2])*I->WobbleParam[1]);
        n[2]=(float)cos((v[0]+v[1]-v[2])*I->WobbleParam[1]);
        RayTransformNormals33(1,&n,I->ModelView,&n);
        scale3f(n,I->WobbleParam[0],n);
        add3f(n,r->surfnormal,r->surfnormal);
        normalize3f(r->surfnormal);
      }
    case 4: 
      {
        float3 v;
        float3 n;
        float *tp = I->WobbleParam;
        copy3f(r->impact,v);
        RayApplyMatrixInverse33(1,&v,I->ModelView,&v);
        n[0]=I->Random[0xFF&(int)((cos((v[0])*tp[1])*256*tp[2]))];
        n[1]=I->Random[0xFF&(int)((cos((v[1])*tp[1])*256*tp[2]+96))];
        n[2]=I->Random[0xFF&(int)((cos((v[2])*tp[1])*256*tp[2]+148))];
        RayTransformNormals33(1,&n,I->ModelView,&n);
        scale3f(n,tp[0],n);
        add3f(n,r->surfnormal,r->surfnormal);
        normalize3f(r->surfnormal);
      }
      break;
    case 5: 
      {
        float3 v;
        float3 n;
        float *tp = I->WobbleParam;
        copy3f(r->impact,v);
        RayApplyMatrixInverse33(1,&v,I->ModelView,&v);
        n[0]=I->Random[0xFF&(int)((v[0]*tp[1])+0)]+
          I->Random[0xFF&(int)((v[1]*tp[1])+20)]+
          I->Random[0xFF&(int)((v[2]*tp[1])+40)];
        n[1]=I->Random[0xFF&(int)((-v[0]*tp[1])+90)]+
          I->Random[0xFF&(int)((v[1]*tp[1])+100)]+
          I->Random[0xFF&(int)((-v[2]*tp[1])+120)];
        n[2]=I->Random[0xFF&(int)((v[0]*tp[1])+200)]+
          I->Random[0xFF&(int)((-v[1]*tp[1])+70)]+
          I->Random[0xFF&(int)((v[2]*tp[1])+30)];
        
        n[0]+=
          I->Random[0xFF&((int)((v[0]-v[1])*tp[1])+0)] +
          I->Random[0xFF&((int)((v[1]-v[2])*tp[1])+20)] +
          I->Random[0xFF&((int)((v[2]-v[0])*tp[1])+40)];
        n[1]+=
          I->Random[0xFF&((int)((v[0]+v[1])*tp[1])+10)]+
          I->Random[0xFF&((int)((v[1]+v[2])*tp[1])+90)]+
          I->Random[0xFF&((int)((v[2]+v[0])*tp[1])+30)];
        n[2]+=
          I->Random[0xFF&((int)((-v[0]+v[1])*tp[1])+220)]+
          I->Random[0xFF&((int)((-v[1]+v[2])*tp[1])+20)]+
          I->Random[0xFF&((int)((-v[2]+v[0])*tp[1])+50)];
        
        n[0]+=
          I->Random[0xFF&((int)((v[0]+v[1]+v[2])*tp[1])+5)]+
          I->Random[0xFF&((int)((v[0]+v[1]+v[2])*tp[1])+25)]+
          I->Random[0xFF&((int)((v[0]+v[1]+v[2])*tp[1])+46)];
        n[1]+=
          I->Random[0xFF&((int)((-v[0]-v[1]+v[2])*tp[1])+90)]+
          I->Random[0xFF&((int)((-v[0]-v[1]+v[2])*tp[1])+45)]+
          I->Random[0xFF&((int)((-v[0]-v[1]+v[2])*tp[1])+176)];
        n[2]+=
          I->Random[0xFF&((int)((v[0]+v[1]-v[2])*tp[1])+192)]+
          I->Random[0xFF&((int)((v[0]+v[1]-v[2])*tp[1])+223)]+
          I->Random[0xFF&((int)((v[0]+v[1]-v[2])*tp[1])+250)];

        RayTransformNormals33(1,&n,I->ModelView,&n);
        scale3f(n,tp[0],n);
        add3f(n,r->surfnormal,r->surfnormal);
        normalize3f(r->surfnormal);
      }
      break;
    }
  if(perspective) {
    r->dotgle = dot_product3f(r->dir, r->surfnormal);
    r->flat_dotgle = -r->dotgle;
  
    r->reflect[0]= r->dir[0] - ( 2 * r->dotgle * r->surfnormal[0] );
    r->reflect[1]= r->dir[1] - ( 2 * r->dotgle * r->surfnormal[1] );
    r->reflect[2]= r->dir[2] - ( 2 * r->dotgle * r->surfnormal[2] );
  } else {
    r->dotgle = -r->surfnormal[2]; 
    r->flat_dotgle = r->surfnormal[2];
    
    r->reflect[0]= - ( 2 * r->dotgle * r->surfnormal[0] );
    r->reflect[1]= - ( 2 * r->dotgle * r->surfnormal[1] );
    r->reflect[2]= -1.0F - ( 2 * r->dotgle * r->surfnormal[2] );
  }
}
/*========================================================================*/
void RayExpandPrimitives(CRay *I)
{
  int a;
  float *v0,*v1,*n0,*n1;
  CBasis *basis;
  int nVert, nNorm;
  float voxel_floor;

  nVert=0;
  nNorm=0;
  for(a=0;a<I->NPrimitive;a++) {
	 switch(I->Primitive[a].type) {
	 case cPrimSphere:
		nVert++;
		break;
	 case cPrimCylinder:
    case cPrimSausage:
		nVert++;
		nNorm++;
		break;
	 case cPrimTriangle:
	 case cPrimCharacter:
		nVert+=3;
		nNorm+=4;
		break;
	 }
  }

  basis = I->Basis;
  
  VLACacheSize(I->G,basis->Vertex,float,3*nVert,0,cCache_basis_vertex);
  VLACacheSize(I->G,basis->Radius,float,nVert,0,cCache_basis_radius);
  VLACacheSize(I->G,basis->Radius2,float,nVert,0,cCache_basis_radius2);
  VLACacheSize(I->G,basis->Vert2Normal,int,nVert,0,cCache_basis_vert2normal);
  VLACacheSize(I->G,basis->Normal,float,3*nNorm,0,cCache_basis_normal);

  VLACacheSize(I->G,I->Vert2Prim,int,nVert,0,cCache_ray_vert2prim);

  voxel_floor=I->PixelRadius/2.0F;

  basis->MaxRadius = 0.0F;
  basis->MinVoxel = 0.0F;
  basis->NVertex=nVert;
  basis->NNormal=nNorm;

  nVert=0;
  nNorm=0;
  v0=basis->Vertex;
  n0=basis->Normal;
  for(a=0;a<I->NPrimitive;a++) {
	 switch(I->Primitive[a].type) {
	 case cPrimTriangle:
	 case cPrimCharacter:

		I->Primitive[a].vert=nVert;
		I->Vert2Prim[nVert]=a;
		I->Vert2Prim[nVert+1]=a;
		I->Vert2Prim[nVert+2]=a;
		basis->Radius[nVert]=I->Primitive[a].r1;
		basis->Radius2[nVert]=I->Primitive[a].r1*I->Primitive[a].r1; /*necessary??*/
		/*		if(basis->Radius[nVert]>basis->MinVoxel)
				basis->MinVoxel=basis->Radius[nVert];*/
		if(basis->MinVoxel<voxel_floor)
		  basis->MinVoxel=voxel_floor;
		basis->Vert2Normal[nVert]=nNorm;
		basis->Vert2Normal[nVert+1]=nNorm;
		basis->Vert2Normal[nVert+2]=nNorm;
		n1=I->Primitive[a].n0;
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		n1=I->Primitive[a].n1;
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		n1=I->Primitive[a].n2;
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		n1=I->Primitive[a].n3;
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		(*n0++)=(*n1++);
		nNorm+=4;
		v1=I->Primitive[a].v1;
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		v1=I->Primitive[a].v2;
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		v1=I->Primitive[a].v3;
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		nVert+=3;
		break;
	 case cPrimSphere:
		I->Primitive[a].vert=nVert;
		I->Vert2Prim[nVert]=a;
		v1=I->Primitive[a].v1;
		basis->Radius[nVert]=I->Primitive[a].r1;
		basis->Radius2[nVert]=I->Primitive[a].r1*I->Primitive[a].r1; /*precompute*/
		if(basis->Radius[nVert]>basis->MaxRadius)
		  basis->MaxRadius=basis->Radius[nVert];
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		nVert++;
		break;
	 case cPrimCylinder:
	 case cPrimSausage:
		I->Primitive[a].vert=nVert;
		I->Vert2Prim[nVert]=a;
		basis->Radius[nVert]=I->Primitive[a].r1;
		basis->Radius2[nVert]=I->Primitive[a].r1*I->Primitive[a].r1; /*precompute*/
		if(basis->MinVoxel<voxel_floor)
        basis->MinVoxel=voxel_floor;
		subtract3f(I->Primitive[a].v2,I->Primitive[a].v1,n0);
		I->Primitive[a].l1=(float)length3f(n0);
		normalize3f(n0);
		n0+=3;
		basis->Vert2Normal[nVert]=nNorm;
		nNorm++;
		v1=I->Primitive[a].v1;
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		(*v0++)=(*v1++);
		nVert++;
		break;
	 }
  }
  if(nVert>basis->NVertex) {
    fprintf(stderr,"Error: basis->NVertex exceeded\n");
  }
  PRINTFB(I->G,FB_Ray,FB_Blather)
    " Ray: minvoxel  %8.3f\n Ray: NPrimit  %d nvert %d\n",basis->MinVoxel,I->NPrimitive,nVert
    ENDFB(I->G);
}
/*========================================================================*/
static void RayComputeBox(CRay *I)
{

#define minmax(v,r) { \
  xp = v[0] + r;\
  xm = v[0] - r;\
  yp = v[1] + r;\
  ym = v[1] - r;\
  zp = v[2] + r;\
  zm = v[2] - r;\
  if(xmin>xm) xmin = xm;\
  if(xmax<xp) xmax = xp;\
  if(ymin>ym) ymin = ym;\
  if(ymax<yp) ymax = yp;\
  if(zmin>zm) zmin = zm;\
  if(zmax<zp) zmax = zp;\
}

  CPrimitive *prm;
  CBasis *basis1;

  float xmin=0.0F,ymin=0.0F,xmax=0.0F,ymax=0.0F,zmin=0.0F,zmax=0.0F;
  float xp,xm,yp,ym,zp,zm;

  float *v,r;
  float vt[3];
  const float _0 = 0.0F;
  int a;

  basis1 = I->Basis+1;
  if(basis1->NVertex) {
    xmin = xmax = basis1->Vertex[0];
    ymin = ymax = basis1->Vertex[1];
    zmin = zmax = basis1->Vertex[2];

    for(a=0;a<I->NPrimitive;a++) {
      prm=I->Primitive+a;
      
      switch(prm->type) 
      {
      case cPrimTriangle:
      case cPrimCharacter:

        r = _0;
        v = basis1->Vertex + prm->vert*3;
        minmax(v,r);
        v = basis1->Vertex + prm->vert*3+3;
        minmax(v,r);
        v = basis1->Vertex + prm->vert*3+6;
        minmax(v,r);
        break;
      case cPrimSphere:
        r = prm->r1;
        v = basis1->Vertex + prm->vert*3;
        minmax(v,r);
        break;
      case cPrimCylinder:
      case cPrimSausage:
        r = prm->r1;
        v = basis1->Vertex + prm->vert*3;
        minmax(v,r);
        v = basis1->Normal+basis1->Vert2Normal[prm->vert]*3;
        scale3f(v,prm->l1,vt);
        v = basis1->Vertex + prm->vert*3;
        add3f(v,vt,vt);
        minmax(vt,r);
        break;
      }	/* end of switch */
	 }
  }
  I->min_box[0] = xmin;
  I->min_box[1] = ymin;
  I->min_box[2] = zmin;
  I->max_box[0] = xmax;
  I->max_box[1] = ymax;
  I->max_box[2] = zmax;
}

static void RayTransformFirst(CRay *I,int perspective)
{
  CBasis *basis0,*basis1;
  CPrimitive *prm;
  int a;
  float *v0;
  int backface_cull;

  backface_cull = (int)SettingGet(I->G,cSetting_backface_cull);
  
  if((SettingGet(I->G,cSetting_two_sided_lighting)||
      (SettingGet(I->G,cSetting_transparency_mode)==1)||
      SettingGet(I->G,cSetting_ray_interior_color)>=0))
    backface_cull=0;

  basis0 = I->Basis;
  basis1 = I->Basis+1;
  
  VLACacheSize(I->G,basis1->Vertex,float,3*basis0->NVertex,1,cCache_basis_vertex);
  VLACacheSize(I->G,basis1->Normal,float,3*basis0->NNormal,1,cCache_basis_normal);
  VLACacheSize(I->G,basis1->Precomp,float,3*basis0->NNormal,1,cCache_basis_precomp);
  VLACacheSize(I->G,basis1->Vert2Normal,int,basis0->NVertex,1,cCache_basis_vert2normal);
  VLACacheSize(I->G,basis1->Radius,float,basis0->NVertex,1,cCache_basis_radius);
  VLACacheSize(I->G,basis1->Radius2,float,basis0->NVertex,1,cCache_basis_radius2);
  
  RayApplyMatrix33(basis0->NVertex,(float3*)basis1->Vertex,
					  I->ModelView,(float3*)basis0->Vertex);

  for(a=0;a<basis0->NVertex;a++)
	 {
		basis1->Radius[a]=basis0->Radius[a];
		basis1->Radius2[a]=basis0->Radius2[a];
		basis1->Vert2Normal[a]=basis0->Vert2Normal[a];
	 }
  basis1->MaxRadius=basis0->MaxRadius;
  basis1->MinVoxel=basis0->MinVoxel;
  basis1->NVertex=basis0->NVertex;

  RayTransformNormals33(basis0->NNormal,(float3*)basis1->Normal,
					  I->ModelView,(float3*)basis0->Normal);
  
  basis1->NNormal=basis0->NNormal;

  if(perspective) {
    for(a=0;a<I->NPrimitive;a++) {
      prm=I->Primitive+a;
      
      prm=I->Primitive+a;
      switch(prm->type) {
      case cPrimTriangle:
      case cPrimCharacter:
        BasisTrianglePrecomputePerspective(
                                           basis1->Vertex+prm->vert*3,
                                           basis1->Vertex+prm->vert*3+3,
                                           basis1->Vertex+prm->vert*3+6,
                                           basis1->Precomp+basis1->Vert2Normal[prm->vert]*3);
        break;
      }
    }
  } else {
    for(a=0;a<I->NPrimitive;a++) {
      prm=I->Primitive+a;
      switch(prm->type) {
      case cPrimTriangle:
      case cPrimCharacter:
        
        BasisTrianglePrecompute(basis1->Vertex+prm->vert*3,
                                basis1->Vertex+prm->vert*3+3,
                                basis1->Vertex+prm->vert*3+6,
                                basis1->Precomp+basis1->Vert2Normal[prm->vert]*3);
        v0 = basis1->Normal + (basis1->Vert2Normal[prm->vert]*3 + 3);
        prm->cull = backface_cull&&((v0[2]<0.0F)&&(v0[5]<0.0F)&&(v0[8]<0.0F));
        break;
      case cPrimSausage:
      case cPrimCylinder:
        BasisCylinderSausagePrecompute(basis1->Normal + basis1->Vert2Normal[prm->vert]*3,
                                       basis1->Precomp + basis1->Vert2Normal[prm->vert]*3);
        break;
        
      }
    }
  }
}
/*========================================================================*/
void RayTransformBasis(CRay *I,CBasis *basis1,int group_id)
{
  CBasis *basis0;
  int a;
  float *v0,*v1;
  CPrimitive *prm;

  basis0 = I->Basis+1;

  VLACacheSize(I->G,basis1->Vertex,float,3*basis0->NVertex,group_id,cCache_basis_vertex);
  VLACacheSize(I->G,basis1->Normal,float,3*basis0->NNormal,group_id,cCache_basis_normal);
  VLACacheSize(I->G,basis1->Precomp,float,3*basis0->NNormal,group_id,cCache_basis_precomp);
  VLACacheSize(I->G,basis1->Vert2Normal,int,basis0->NVertex,group_id,cCache_basis_vert2normal);
  VLACacheSize(I->G,basis1->Radius,float,basis0->NVertex,group_id,cCache_basis_radius);
  VLACacheSize(I->G,basis1->Radius2,float,basis0->NVertex,group_id,cCache_basis_radius2);
  v0=basis0->Vertex;
  v1=basis1->Vertex;
  for(a=0;a<basis0->NVertex;a++)
	 {
		matrix_transform33f3f(basis1->Matrix,v0,v1);
		v0+=3;
		v1+=3;
		basis1->Radius[a]=basis0->Radius[a];
		basis1->Radius2[a]=basis0->Radius2[a];
		basis1->Vert2Normal[a]=basis0->Vert2Normal[a];
	 }
  v0=basis0->Normal;
  v1=basis1->Normal;
  for(a=0;a<basis0->NNormal;a++)
	 {
		matrix_transform33f3f(basis1->Matrix,v0,v1);
		v0+=3;
		v1+=3;
	 }
  basis1->MaxRadius=basis0->MaxRadius;
  basis1->MinVoxel=basis0->MinVoxel;
  basis1->NVertex=basis0->NVertex;
  basis1->NNormal=basis0->NNormal;


  for(a=0;a<I->NPrimitive;a++) {
	 prm=I->Primitive+a;
    switch(prm->type) {
    case cPrimTriangle:
	 case cPrimCharacter:

        BasisTrianglePrecompute(basis1->Vertex+prm->vert*3,
                                basis1->Vertex+prm->vert*3+3,
                                basis1->Vertex+prm->vert*3+6,
                                basis1->Precomp+basis1->Vert2Normal[prm->vert]*3);
        break;
    case cPrimSausage:
    case cPrimCylinder:
      BasisCylinderSausagePrecompute(basis1->Normal + basis1->Vert2Normal[prm->vert]*3,
                                     basis1->Precomp + basis1->Vert2Normal[prm->vert]*3);
      break;
      
	 }
  }
}

/*========================================================================*/
void RayRenderTest(CRay *I,int width,int height,float front,float back,float fov)
{

  PRINTFB(I->G,FB_Ray,FB_Details)
    " RayRenderTest: obtained %i graphics primitives.\n",I->NPrimitive 
    ENDFB(I->G);
}
/*========================================================================*/

G3dPrimitive *RayRenderG3d(CRay *I,int width, int height,
                           float front, float back, float fov,int quiet)
{
  /* generate a rendering stream for Miguel's G3d java rendering engine */

  register float scale_x,scale_y,scale_z;
  int shift_x,shift_y;
  float *d;
  CBasis *base;
  CPrimitive *prim;
  float *vert;
  float vert2[3];
  int a;
  G3dPrimitive *jprim = VLAlloc(G3dPrimitive,10000),*jp;
  int n_jp = 0;

#define convert_r(r) 2*(int)(r*scale_x);
#define convert_x(x) shift_x + (int)(x*scale_x);
#define convert_y(y) height - (shift_y + (int)(y*scale_y));
#define convert_z(z) -(int)((z+front)*scale_x);
#define convert_col(c) (0xFF000000 | (((int)(c[0]*255.0))<<16) | (((int)(c[1]*255.0))<<8) | (((int)(c[2]*255.0))))

  RayExpandPrimitives(I);
  RayTransformFirst(I,0);

  if(!quiet) {
    PRINTFB(I->G,FB_Ray,FB_Details)
      " RayRenderG3d: processed %i graphics primitives.\n",I->NPrimitive 
      ENDFB(I->G);
  }
  base = I->Basis+1;

  /* always orthoscopic */
  
  /* front should give a zero Z, 
     -I->Range[0] should be off the right hand size
     I->Range[1] should be off the top */
  scale_x = width/I->Range[0];
  scale_y = height/I->Range[1];
  scale_z = -4096.0F/(back-front);
  shift_x = width/2;
  shift_y = height/2;

  for(a=0;a<I->NPrimitive;a++) {
    prim = I->Primitive+a;
    vert = base->Vertex+3*(prim->vert);
    switch(prim->type) {
	 case cPrimSphere:
      VLACheck(jprim,G3dPrimitive,n_jp);
      jp = jprim + n_jp;
      jp->op = 1;
      jp->r = convert_r(prim->r1);
      jp->x1 = convert_x(vert[0]);
      jp->y1 = convert_y(vert[1]);
      jp->z1 = convert_z(vert[2]);
      jp->c = convert_col(prim->c1);
      n_jp++;
		break;
    case cPrimSausage:
      VLACheck(jprim,G3dPrimitive,n_jp);
      d=base->Normal+3*base->Vert2Normal[prim->vert];
      scale3f(d,prim->l1,vert2);
      add3f(vert,vert2,vert2);

      jp = jprim + n_jp;
      jp->op = 3;
      jp->r = convert_r(prim->r1);
      jp->x1 = convert_x(vert[0]);
      jp->y1 = convert_y(vert[1]);
      jp->z1 = convert_z(vert[2]);
      jp->x2 = convert_x(vert2[0]);
      jp->y2 = convert_y(vert2[1]);
      jp->z2 = convert_z(vert2[2]);
      jp->c = convert_col(prim->c1);
      n_jp++;
		break;
	 case cPrimTriangle:
      VLACheck(jprim,G3dPrimitive,n_jp);
      jp = jprim + n_jp;
      jp->op = 2;
      jp->x1 = convert_x(vert[0]);
      jp->y1 = convert_y(vert[1]);
      jp->z1 = convert_z(vert[2]);
      jp->x2 = convert_x(vert[3]);
      jp->y2 = convert_y(vert[4]);
      jp->z2 = convert_z(vert[5]);
      jp->x3 = convert_x(vert[6]);
      jp->y3 = convert_y(vert[7]);
      jp->z3 = convert_z(vert[8]);
      jp->c = convert_col(prim->c1);
      n_jp++;
		break;
    }
  }
  VLASize(jprim,G3dPrimitive,n_jp);
  return jprim;
}
/*========================================================================*/
void RayRenderPOV(CRay *I,int width,int height,char **headerVLA_ptr,
                  char **charVLA_ptr,float front,float back,float fov,
                  float angle)
{
  int antialias;
  int fogFlag=false;
  int fogRangeFlag=false;
  float fog;
  float *bkrd;
  float fog_start=0.0F;
  float gamma;
  float *d;
  CBasis *base;
  CPrimitive *prim;
  OrthoLineType buffer;
  float *vert,*norm;
  float vert2[3];
  float light[3],*lightv;
  int cc,hc;
  int a;
  int smooth_color_triangle;
  int mesh_obj = false;
  char *charVLA,*headerVLA;
  char transmit[64];

  charVLA=*charVLA_ptr;
  headerVLA=*headerVLA_ptr;
  smooth_color_triangle=(int)SettingGet(I->G,cSetting_smooth_color_triangle);
  PRINTFB(I->G,FB_Ray,FB_Details)
    " RayRenderPOV: w %d h %d f %8.3f b %8.3f\n",width,height,front,back
    ENDFB(I->G);
  if(Feedback(I->G,FB_Ray,FB_Details)) {
    dump3f(I->Volume," RayRenderPOV: vol");
    dump3f(I->Volume+3," RayRenderPOV: vol");
  }
  cc=0;
  hc=0;
  gamma = SettingGet(I->G,cSetting_gamma);
  if(gamma>R_SMALL4)
    gamma=1.0F/gamma;
  else
    gamma=1.0F;

  fog = SettingGet(I->G,cSetting_ray_trace_fog);
  if(fog<0.0F)
    fog = SettingGet(I->G,cSetting_depth_cue);
  if(fog!=0.0F) {
    if(fog>1.0F) fog=1.0F;
    fogFlag=true;
    fog_start = SettingGet(I->G,cSetting_ray_trace_fog_start);
    if(fog_start<0.0F)
      fog_start = SettingGet(I->G,cSetting_fog_start) + 0.05F;
    if(fog_start>1.0F)
      fog_start=1.0F;
    if(fog_start<0.0F)
      fog_start=0.0F;
    if(fog_start>R_SMALL4) {
      fogRangeFlag=true;
      if(fabs(fog_start-1.0F)<R_SMALL4) /* prevent div/0 */
        fogFlag=false;
    }
  }

  /* SETUP */
  
  antialias = (int)SettingGet(I->G,cSetting_antialias);
  bkrd=SettingGetfv(I->G,cSetting_bg_rgb);

  RayExpandPrimitives(I);
  RayTransformFirst(I,0);

  PRINTFB(I->G,FB_Ray,FB_Details)
    " RayRenderPovRay: processed %i graphics primitives.\n",I->NPrimitive 
    ENDFB(I->G);
  base = I->Basis+1;

  if(!SettingGet(I->G,cSetting_ortho)) {
    sprintf(buffer,"camera {direction<0.0,0.0,%8.3f>\n location <0.0 , 0.0 , 0.0>\n right %12.10f*x up y \n }\n",
            -57.3F*cos(fov*cPI/(180*2.4))/fov,/* by trial and error */
            I->Range[0]/I->Range[1]);
  } else {
    sprintf(buffer,"camera {orthographic location <0.0 , 0.0 , %12.10f>\nlook_at  <0.0 , 0.0 , -1.0> right %12.10f*x up %12.10f*y}\n",
            front,-I->Range[0],I->Range[1]);
  }
  UtilConcatVLA(&headerVLA,&hc,buffer);

  {
    float ambient =           SettingGet(I->G,cSetting_ambient) + SettingGet(I->G,cSetting_direct);
    float reflect =           SettingGet(I->G,cSetting_reflect);

    if(ambient>0.5) ambient=0.5;

    reflect = 1.2F - 1.5F*ambient;

    sprintf(buffer,"#default { finish{phong %8.3f ambient %8.3f diffuse %8.3f phong_size %8.6f}}\n",
            SettingGet(I->G,cSetting_spec_reflect),
            ambient,
            reflect,
            SettingGet(I->G,cSetting_spec_power)/4.0F);
    UtilConcatVLA(&headerVLA,&hc,buffer);
  }
  lightv = SettingGet_3fv(I->G,NULL,NULL,cSetting_light);
  copy3f(lightv,light);
  if(angle) {
    float temp[16];
    identity44f(temp);
    MatrixRotateC44f(temp,(float)-PI*angle/180,0.0F,1.0F,0.0F);
    MatrixTransformC44fAs33f3f(temp,light,light);
  }
  sprintf(buffer,"light_source{<%6.4f,%6.4f,%6.4f>  rgb<1.0,1.0,1.0>}\n",
          -light[0]*10000.0F,
          -light[1]*10000.0F,
          -light[2]*10000.0F-front
          );
  UtilConcatVLA(&headerVLA,&hc,buffer);

  {
    int opaque_back = SettingGetGlobal_i(I->G,cSetting_ray_opaque_background);
    if(opaque_back<0)
      opaque_back			= SettingGetGlobal_i(I->G,cSetting_opaque_background);      
    
    if(opaque_back) { /* drop a plane into the background for the background color */
      sprintf(buffer,"plane{z , %6.4f \n pigment{color rgb<%6.4f,%6.4f,%6.4f>}\n finish{phong 0 specular 0 diffuse 0 ambient 1.0}}\n",-back,bkrd[0],bkrd[1],bkrd[2]);
      UtilConcatVLA(&headerVLA,&hc,buffer);
    } 
  }
  
  for(a=0;a<I->NPrimitive;a++) {
    prim = I->Primitive+a;
    vert = base->Vertex+3*(prim->vert);
    if(prim->type==cPrimTriangle) {
      if(smooth_color_triangle)
        if(!mesh_obj) {
          sprintf(buffer,"mesh {\n");
          UtilConcatVLA(&charVLA,&cc,buffer);        
          mesh_obj=true;
        }
    } else if(mesh_obj) {
      sprintf(buffer," pigment{color rgb <1,1,1>}}");
      UtilConcatVLA(&charVLA,&cc,buffer);     
      mesh_obj=false;
    }
    switch(prim->type) {
	 case cPrimSphere:
      sprintf(buffer,"sphere{<%12.10f,%12.10f,%12.10f>, %12.10f\n",
             vert[0],vert[1],vert[2],prim->r1);
      UtilConcatVLA(&charVLA,&cc,buffer);      
      sprintf(buffer,"pigment{color rgb<%6.4f,%6.4f,%6.4f>}}\n",
              prim->c1[0],prim->c1[1],prim->c1[2]);
      UtilConcatVLA(&charVLA,&cc,buffer);
		break;
	 case cPrimCylinder:
      d=base->Normal+3*base->Vert2Normal[prim->vert];
      scale3f(d,prim->l1,vert2);
      add3f(vert,vert2,vert2);
      sprintf(buffer,"cylinder{<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n %12.10f\n",
              vert[0],vert[1],vert[2],
              vert2[0],vert2[1],vert2[2],
              prim->r1);
      UtilConcatVLA(&charVLA,&cc,buffer);
      sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f>}}\n",
              (prim->c1[0]+prim->c2[0])/2,
              (prim->c1[1]+prim->c2[1])/2,
              (prim->c1[2]+prim->c2[2])/2);
      UtilConcatVLA(&charVLA,&cc,buffer);
		break;
    case cPrimSausage:
      d=base->Normal+3*base->Vert2Normal[prim->vert];
      scale3f(d,prim->l1,vert2);
      add3f(vert,vert2,vert2);
      sprintf(buffer,"cylinder{<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n %12.10f\nopen\n",
              vert[0],vert[1],vert[2],
              vert2[0],vert2[1],vert2[2],
              prim->r1);
      UtilConcatVLA(&charVLA,&cc,buffer);
      sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f>}}\n",
              (prim->c1[0]+prim->c2[0])/2,
              (prim->c1[1]+prim->c2[1])/2,
              (prim->c1[2]+prim->c2[2])/2);
      UtilConcatVLA(&charVLA,&cc,buffer);

      sprintf(buffer,"sphere{<%12.10f,%12.10f,%12.10f>, %12.10f\n",
             vert[0],vert[1],vert[2],prim->r1);
      UtilConcatVLA(&charVLA,&cc,buffer);      
      sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f>}}\n",
              prim->c1[0],prim->c1[1],prim->c1[2]);
      UtilConcatVLA(&charVLA,&cc,buffer);

      sprintf(buffer,"sphere{<%12.10f,%12.10f,%12.10f>, %12.10f\n",
             vert2[0],vert2[1],vert2[2],prim->r1);
      UtilConcatVLA(&charVLA,&cc,buffer);      
      sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f>}}\n",
              prim->c2[0],prim->c2[1],prim->c2[2]);
      UtilConcatVLA(&charVLA,&cc,buffer);

      
		break;
	 case cPrimTriangle:
      norm=base->Normal+3*base->Vert2Normal[prim->vert]+3;/* first normal is the average */      


      if(!TriangleDegenerate(vert,norm,vert+3,norm+3,vert+6,norm+6)) {

        if(smooth_color_triangle) {
          sprintf(buffer,"smooth_color_triangle{<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%6.4f1,%6.4f,%6.4f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%6.4f1,%6.4f,%6.4f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%6.4f1,%6.4f,%6.4f> }\n",
                  vert[0],vert[1],vert[2],
                  norm[0],norm[1],norm[2],
                  prim->c1[0],prim->c1[1],prim->c1[2],
                  vert[3],vert[4],vert[5],
                  norm[3],norm[4],norm[5],
                  prim->c2[0],prim->c2[1],prim->c2[2],
                  vert[6],vert[7],vert[8],
                  norm[6],norm[7],norm[8],
                  prim->c3[0],prim->c3[1],prim->c3[2]
                  );
          UtilConcatVLA(&charVLA,&cc,buffer);
        } else {
#if 0
          
          sprintf(buffer,"smooth_triangle{<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>\n",
                  vert[0],vert[1],vert[2],
                  norm[0],norm[1],norm[2],
                  vert[3],vert[4],vert[5],
                  norm[3],norm[4],norm[5],
                  vert[6],vert[7],vert[8],
                  norm[6],norm[7],norm[8]
                  );
          UtilConcatVLA(&charVLA,&cc,buffer);
          if(prim->trans>R_SMALL4) 
            sprintf(transmit,"transmit %4.6f",prim->trans);
          else
            transmit[0]=0;
          if(equal3f(prim->c1,prim->c2)||equal3f(prim->c1,prim->c3)) {
            sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}}\n",
                    prim->c1[0],prim->c1[1],prim->c1[2],transmit);
          } else if(equal3f(prim->c2,prim->c3)) {
            sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}}\n",
                    prim->c2[0],prim->c2[1],prim->c2[2],transmit);
          } else {
            sprintf(buffer,"pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}}\n",
                    (prim->c1[0]+prim->c2[0]+prim->c3[0])/3,
                  (prim->c1[1]+prim->c2[1]+prim->c3[1])/3,
                    (prim->c1[2]+prim->c2[2]+prim->c3[2])/3,transmit);
          }
        UtilConcatVLA(&charVLA,&cc,buffer);
#else
        /* nowadays we use mesh2 to generate smooth_color_triangles */

        UtilConcatVLA(&charVLA,&cc,"mesh2 { ");
        sprintf(buffer,"vertex_vectors { 3, <%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>}\n normal_vectors { 3,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>,\n<%12.10f,%12.10f,%12.10f>}\n",
                vert[0],vert[1],vert[2],
                vert[3],vert[4],vert[5],
                vert[6],vert[7],vert[8],
                norm[0],norm[1],norm[2],
                norm[3],norm[4],norm[5],
                norm[6],norm[7],norm[8]
                );
        UtilConcatVLA(&charVLA,&cc,buffer);

        if(prim->trans>R_SMALL4) 
          sprintf(transmit,"transmit %4.6f",prim->trans);
        else
          transmit[0]=0;

        sprintf(buffer,"texture_list { 3, ");
        UtilConcatVLA(&charVLA,&cc,buffer);

        sprintf(buffer, "texture { pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}}\n",
                prim->c1[0],prim->c1[1],prim->c1[2],transmit);
        UtilConcatVLA(&charVLA,&cc,buffer);

        sprintf(buffer, ",texture { pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}}\n",
                prim->c2[0],prim->c2[1],prim->c2[2],transmit);
        UtilConcatVLA(&charVLA,&cc,buffer);

        sprintf(buffer, ",texture { pigment{color rgb<%6.4f1,%6.4f,%6.4f> %s}} }\n",
                prim->c3[0],prim->c3[1],prim->c3[2],transmit);
        UtilConcatVLA(&charVLA,&cc,buffer);

        sprintf(buffer, "face_indices { 1, <0,1,2>, 0, 1, 2 } }\n");
        UtilConcatVLA(&charVLA,&cc,buffer);        
#endif

        }
      }
		break;
    }
  }
  
  if(mesh_obj) {
    sprintf(buffer," pigment{color rgb <1,1,1>}}");
    UtilConcatVLA(&charVLA,&cc,buffer);     
    mesh_obj=false;
  }
  *charVLA_ptr=charVLA;
  *headerVLA_ptr=headerVLA;
}

/*========================================================================*/
void RayProjectTriangle(CRay *I,RayInfo *r,float *light,float *v0,float *n0,float scale)
{
  register float w2;
  float d1[3],d2[3],d3[3];
  float p1[3],p2[3],p3[3];
  register int c=0;
  register const float _0 = 0.0F;
  register float *impact = r->impact;

  if(dot_product3f(light,n0-3)>=_0) c++;  
  else if(dot_product3f(light,n0)>=_0) c++;
  else if(dot_product3f(light,n0+3)>=_0) c++;
  else if(dot_product3f(light,n0+6)>=_0) c++;
  
  if(c) {

    w2 = 1.0F-(r->tri1+r->tri2);
    
    subtract3f(v0,impact,d1);
    subtract3f(v0+3,impact,d2);
    subtract3f(v0+6,impact,d3);
    project3f(d1,n0,p1);
    project3f(d2,n0+3,p2);
    project3f(d3,n0+6,p3);
    scale3f(p1,w2,d1);
    scale3f(p2,r->tri1,d2);
    scale3f(p3,r->tri2,d3);
    add3f(d1,d2,d2);
    add3f(d2,d3,d3);
    scale3f(d3,scale,d3);
    if(dot_product3f(r->surfnormal,d3)>=_0)
      add3f(d3,impact,impact);
  }
}
#ifndef _PYMOL_NOPY
static void RayHashSpawn(CRayHashThreadInfo *Thread,int n_thread)
{
  int blocked;
  PyObject *info_list;
  int a;
  CRay *I = Thread->ray;

  blocked = PAutoBlock();

  PRINTFB(I->G,FB_Ray,FB_Blather)
    " Ray: filling voxels with %d threads...\n",n_thread
  ENDFB(I->G);
  info_list = PyList_New(n_thread);
  for(a=0;a<n_thread;a++) {
    PyList_SetItem(info_list,a,PyCObject_FromVoidPtr(Thread+a,NULL));
  }
  PyObject_CallMethod(P_cmd,"_ray_hash_spawn","O",info_list);
  Py_DECREF(info_list);
  PAutoUnblock(blocked);
}
#endif

#ifndef _PYMOL_NOPY
static void RayAntiSpawn(CRayAntiThreadInfo *Thread,int n_thread)
{
  int blocked;
  PyObject *info_list;
  int a;
  CRay *I = Thread->ray;

  blocked = PAutoBlock();


  PRINTFB(I->G,FB_Ray,FB_Blather)
    " Ray: antialiasing with %d threads...\n",n_thread
  ENDFB(I->G);
  info_list = PyList_New(n_thread);
  for(a=0;a<n_thread;a++) {
    PyList_SetItem(info_list,a,PyCObject_FromVoidPtr(Thread+a,NULL));
  }
  PyObject_CallMethod(P_cmd,"_ray_anti_spawn","O",info_list);
  Py_DECREF(info_list);
  PAutoUnblock(blocked);
}
#endif

int RayHashThread(CRayHashThreadInfo *T)
{
  BasisMakeMap(T->basis,T->vert2prim,T->prim,T->clipBox,T->phase,
               cCache_ray_map,T->perspective,T->front);

  /* utilize a little extra wasted CPU time in thread 0 which computes the smaller map... */

  if(!T->phase) { 
    fill(T->image,T->background,T->bytes);
    RayComputeBox(T->ray);
  }
  return 1;
}
#ifndef _PYMOL_NOPY
static void RayTraceSpawn(CRayThreadInfo *Thread,int n_thread)
{
  int blocked;
  PyObject *info_list;
  int a;
  CRay *I=Thread->ray;
  blocked = PAutoBlock();

  PRINTFB(I->G,FB_Ray,FB_Blather)
    " Ray: rendering with %d threads...\n",n_thread
  ENDFB(I->G);
  info_list = PyList_New(n_thread);
  for(a=0;a<n_thread;a++) {
    PyList_SetItem(info_list,a,PyCObject_FromVoidPtr(Thread+a,NULL));
  }
  PyObject_CallMethod(P_cmd,"_ray_spawn","O",info_list);
  Py_DECREF(info_list);
  PAutoUnblock(blocked);
  
}
#endif

static int find_edge(unsigned int *ptr,unsigned int width,int threshold)
{
  unsigned int shift = 0;
  int compare[9];
  int sum[9] = {0,0,0,0,0,0,0,0};
  int current;
  int a;

  compare[0] = (signed int)*(ptr);
  compare[1] = (signed int)*(ptr-1);
  compare[2] = (signed int)*(ptr+1);
  compare[3] = (signed int)*(ptr-width);
  compare[4] = (signed int)*(ptr+width);
  compare[5] = (signed int)*(ptr-width-1);
  compare[6] = (signed int)*(ptr+width-1);
  compare[7] = (signed int)*(ptr-width+1);
  compare[8] = (signed int)*(ptr+width+1);
  
  for(a=0;a<4;a++) {
    current = ((compare[0]>>shift)&0xFF);
    sum[1] += abs(current - ((compare[1]>>shift)&0xFF));
    sum[2] += abs(current - ((compare[2]>>shift)&0xFF));
    if(sum[1]>=threshold) return 1;
    sum[3] += abs(current - ((compare[3]>>shift)&0xFF));
    if(sum[2]>=threshold) return 1;
    sum[4] += abs(current - ((compare[4]>>shift)&0xFF));
    if(sum[3]>=threshold) return 1;
    sum[5] += abs(current - ((compare[5]>>shift)&0xFF));
    if(sum[4]>=threshold) return 1;
    sum[6] += abs(current - ((compare[6]>>shift)&0xFF));
    if(sum[5]>=threshold) return 1;
    sum[7] += abs(current - ((compare[7]>>shift)&0xFF));
    if(sum[6]>=threshold) return 1;
    sum[8] += abs(current - ((compare[8]>>shift)&0xFF));
    if(sum[7]>=threshold) return 1;
    if(sum[8]>=threshold) return 1;
    shift+=8;
  }
  return 0;
}

int RayTraceThread(CRayThreadInfo *T)
{
	CRay *I=NULL;
	int x,y,yy;
	float excess=0.0F;
	float dotgle;
	float bright,direct_cmp,reflect_cmp,fc[4];
	float ambient,direct,lreflect,ft,ffact=0.0F,ffact1m;
	unsigned int cc0,cc1,cc2,cc3;
	int i;
	RayInfo r1,r2;
	int fogFlag=false;
	int fogRangeFlag=false;
	int opaque_back=0;
	int n_hit=0;
	int two_sided_lighting;
	float fog;
	float *inter=NULL;
	float fog_start=0.0F;
   /*	float gamma,inp,sig=1.0F;*/
	float persist,persist_inv;
	float new_front;
	int pass;
	unsigned int last_pixel=0,*pixel;
	int exclude;
	float lit;
	int backface_cull;
	float project_triangle;
	float excl_trans;
	int shadows;
	int trans_shadows;
   int trans_mode;
	float first_excess;
	int pixel_flag;
	float ray_trans_spec;
	float shadow_fudge;
	int interior_color;
	int interior_flag;
	int interior_shadows;
	int interior_wobble;
   float interior_reflect;
	int wobble_save;
	float		settingPower, settingReflectPower,settingSpecPower,settingSpecReflect;
	float		invHgt, invFrontMinusBack, inv1minusFogStart,invWdth,invHgtRange;
	register float       invWdthRange,vol0;
	float       vol2;
	CBasis      *bp1,*bp2;
	int render_height;
	int offset=0;
   BasisCallRec SceneCall,ShadeCall;
   float border_offset;
   int edge_sampling = false;
   unsigned int edge_avg[4],edge_alpha_avg[4];
   int edge_cnt=0;
   float edge_base[2];
   float interior_normal[3];
   float edge_width = 0.35356F;
   float edge_height = 0.35356F;
   float trans_spec_cut,trans_spec_scale;
   float direct_shade;
   float red_blend=0.0F;
   float blue_blend=0.0F;
   float green_blend=0.0F;
   float trans_cont;
   float pixel_base[3];
   float inv_trans_cont = 1.0F;

   int trans_cont_flag = false;
   int blend_colors;
   int max_pass;
   float BasisFudge0,BasisFudge1;
   int perspective = T->perspective;
   float eye[3];
   float half_height, front_ratio;
   float start[3],nudge[3];
	const float _0		= 0.0F;
	const float _1		= 1.0F;
	const float _p5		= 0.5F;
	const float _255	= 255.0F;
   const float _p499 = 0.499F;
	const float _persistLimit	= 0.0001F;
   
	I = T->ray;

   {
     float fudge = SettingGet(I->G,cSetting_ray_triangle_fudge);
   
     BasisFudge0 = 0.0F-fudge;
     BasisFudge1 = 1.0F+fudge;
   }
  	
	/* SETUP */
	
	/*  if(T->n_thread>1)
	printf(" Ray: Thread %d: Spawned.\n",T->phase+1);
	*/
	
	interior_shadows	= SettingGetGlobal_i(I->G,cSetting_ray_interior_shadows);
	interior_wobble	= SettingGetGlobal_i(I->G,cSetting_ray_interior_texture);
	interior_color		= SettingGetGlobal_i(I->G,cSetting_ray_interior_color);
   interior_reflect  = 1.0F - SettingGet(I->G,cSetting_ray_interior_reflect);

	project_triangle	= SettingGet(I->G,cSetting_ray_improve_shadows);
	shadows				= SettingGetGlobal_i(I->G,cSetting_ray_shadows);
	trans_shadows		= SettingGetGlobal_i(I->G,cSetting_ray_transparency_shadows);
	backface_cull		= SettingGetGlobal_i(I->G,cSetting_backface_cull);
	opaque_back			= SettingGetGlobal_i(I->G,cSetting_ray_opaque_background);
    if(opaque_back<0)
      opaque_back			= SettingGetGlobal_i(I->G,cSetting_opaque_background);      
	two_sided_lighting	= SettingGetGlobal_i(I->G,cSetting_two_sided_lighting);
	ray_trans_spec		= SettingGet(I->G,cSetting_ray_transparency_specular);
   trans_cont        = SettingGetGlobal_f(I->G,cSetting_ray_transparency_contrast);
   trans_mode        = SettingGetGlobal_i(I->G,cSetting_transparency_mode);
   if(trans_mode==1) two_sided_lighting = true;
   if(trans_cont>1.0F) {
     trans_cont_flag = true;
     inv_trans_cont = 1.0F/trans_cont;
   }
	ambient				= SettingGet(I->G,cSetting_ambient);
	lreflect			= SettingGet(I->G,cSetting_reflect);
	direct				= SettingGet(I->G,cSetting_direct);
	direct_shade	= SettingGet(I->G,cSetting_ray_direct_shade);
   trans_spec_cut = SettingGet(I->G,cSetting_ray_transparency_spec_cut);
   blend_colors    = SettingGetGlobal_i(I->G,cSetting_ray_blend_colors);
   max_pass = SettingGetGlobal_i(I->G,cSetting_ray_max_passes);
   if(blend_colors) {
     red_blend = SettingGet(I->G,cSetting_ray_blend_red);
     green_blend = SettingGet(I->G,cSetting_ray_blend_green);
     blue_blend = SettingGet(I->G,cSetting_ray_blend_blue);
   }

   if(trans_spec_cut<_1)
     trans_spec_scale = _1/(_1-trans_spec_cut);
   else
     trans_spec_scale = _0;

	/* COOP */
	settingPower		= SettingGet(I->G,cSetting_power);
	settingReflectPower	= SettingGet(I->G,cSetting_reflect_power);
	settingSpecPower	= SettingGet(I->G,cSetting_spec_power);

	settingSpecReflect	= SettingGet(I->G,cSetting_spec_reflect);
   if(settingSpecReflect>1.0F) settingSpecReflect = 1.0F;
	if(SettingGet(I->G,cSetting_specular)<R_SMALL4) {
     settingSpecReflect = 0.0F;
   }
    
	if((interior_color>=0)||(two_sided_lighting)||(trans_mode==1))
		backface_cull	= 0;

	shadow_fudge = SettingGet(I->G,cSetting_ray_shadow_fudge);
	
   /*	gamma = SettingGet(I->G,cSetting_gamma);
	if(gamma > R_SMALL4)
		gamma	= _1/gamma;
	else
		gamma	= _1;
   */

	inv1minusFogStart	= _1;
	
	fog = SettingGet(I->G,cSetting_ray_trace_fog);
   if(fog<0.0F) {
     if(SettingGet(I->G,cSetting_depth_cue)) {
       fog = SettingGet(I->G,cSetting_fog);
     } else 
       fog = _0;
   }
   
	if(fog != _0) 
	{
     if(fog>1.0F) fog=1.0F;
     fogFlag	= true;
     fog_start = SettingGet(I->G,cSetting_ray_trace_fog_start);
     if(fog_start<0.0F)
       fog_start = SettingGet(I->G,cSetting_fog_start);
     if(fog_start>1.0F)
       fog_start=1.0F;
     if(fog_start<0.0F)
       fog_start=0.0F;
     if(fog_start>R_SMALL4) {
       fogRangeFlag=true;
       if(fabs(fog_start-1.0F)<R_SMALL4) /* prevent div/0 */
         fogFlag=false;
     }
     if(fog_start>R_SMALL4) 
       {
			fogRangeFlag=true;
			if(fabs(fog_start - _1) < R_SMALL4) /* prevent div/0 */
           fogFlag = false;
       }
     inv1minusFogStart	= _1 / (_1 - fog_start);
	}
   

	/* ray-trace */
	
   if(T->border) {
     invHgt				= _1 / (float) (T->height-(3.0F+T->border));
     invWdth             = _1 / (float) (T->width-(3.0F+T->border));
   } else {

     invHgt				= _1 / (float) (T->height);
     invWdth             = _1 / (float) (T->width);
   }

   if(perspective) {
     float height_range, width_range;

     zero3f(eye);
     half_height = -T->pos[2] * (float)tan((T->fov/2.0F)*PI/180.0F);
     front_ratio = -T->front/T->pos[2];
     height_range = front_ratio*2*half_height;
     width_range = height_range*(I->Range[0]/I->Range[1]);
     invWdthRange        = invWdth * width_range;
     invHgtRange         = invHgt * height_range;
     vol0 = eye[0] - width_range/2.0F;
     vol2 = eye[1] - height_range/2.0F;
   } else {
     invWdthRange        = invWdth * I->Range[0];
     invHgtRange         = invHgt * I->Range[1];
     vol0 = I->Volume[0];
     vol2 = I->Volume[2];
   }
	invFrontMinusBack	= _1 / (T->front - T->back);

   edge_width *= invWdthRange;
   edge_height *= invHgtRange;

	bp1  = I->Basis + 1;
	bp2  = I->Basis + 2;
	
   render_height = T->y_stop - T->y_start;

   if(render_height) {
     offset = (T->phase * render_height/T->n_thread);
     offset = offset - (offset % T->n_thread) + T->phase;
   }

	if(interior_color>=0) {
		inter = ColorGet(I->G,interior_color);
      interior_normal[0] = interior_reflect*bp2->LightNormal[0];
      interior_normal[1] = interior_reflect*bp2->LightNormal[1];
      interior_normal[2] = 1.0F+interior_reflect*bp2->LightNormal[2];
      normalize3f(interior_normal);
   }
     
	r1.base[2]	= _0;

   SceneCall.Basis = I->Basis + 1;
   SceneCall.rr = &r1;
   SceneCall.vert2prim = I->Vert2Prim;
   SceneCall.prim = I->Primitive;
   SceneCall.shadow = false;
   SceneCall.back = T->back;
   SceneCall.trans_shadows = trans_shadows;
   SceneCall.check_interior = (interior_color >= 0);
   SceneCall.fudge0 = BasisFudge0;
   SceneCall.fudge1 = BasisFudge1;

	MapCacheInit(&SceneCall.cache,I->Basis[1].Map,T->phase,cCache_map_scene_cache);

   if(shadows&&(I->NBasis>1)) {
     ShadeCall.Basis = I->Basis + 2;
     ShadeCall.rr = &r2;
     ShadeCall.vert2prim = I->Vert2Prim;
     ShadeCall.prim = I->Primitive;
     ShadeCall.shadow = true;
     ShadeCall.front = _0;
     ShadeCall.back = _0;
     ShadeCall.excl_trans = _0;
     ShadeCall.trans_shadows = trans_shadows;
     ShadeCall.check_interior = false;
     ShadeCall.fudge0 = BasisFudge0;
     ShadeCall.fudge1 = BasisFudge1;
     MapCacheInit(&ShadeCall.cache,I->Basis[2].Map,T->phase,cCache_map_shadow_cache);     
   }

   if(T->border) {
     border_offset = -1.50F+T->border/2.0F;
   } else {
     border_offset = 0.0F;
   }
	for(yy = T->y_start; (yy < T->y_stop); yy++)
	{
     if(PyMOL_GetInterrupt(I->G->PyMOL,false))
       break;

      y = T->y_start + ((yy-T->y_start) + offset) % ( render_height); /* make sure threads write to different pages */

		if((!T->phase)&&!(yy & 0xF)) { /* don't slow down rendering too much */
        if(T->edging_cutoff) {
          if(T->edging) {
            OrthoBusyFast(I->G,(int)(2.5F*T->height/3 + 0.5F*y),4*T->height/3); 
          } else {
            OrthoBusyFast(I->G,(int)(T->height/3 + 0.5F*y),4*T->height/3); 
          }
        } else {
			OrthoBusyFast(I->G,T->height/3 + y,4*T->height/3); 
        }
      }
		pixel = T->image + (T->width * y) + T->x_start;
	
		if((y % T->n_thread) == T->phase)	/* this is my scan line */
		{	
        pixel_base[1]	= ((y+0.5F+border_offset) * invHgtRange) + vol2;

			for(x = T->x_start; (x < T->x_stop); x++)
			{
				
           pixel_base[0]	= (((x+0.5F+border_offset)) * invWdthRange)  + vol0;

            while(1) {
              if(T->edging) {
                if(!edge_sampling) {
                  if(x&&y&&(x<(T->width-1))&&(y<(T->height-1))) { /* not on the edge... */
                    if(find_edge(T->edging + (pixel - T->image),
                                 T->width, T->edging_cutoff)) {
                      register unsigned char *pixel_c = (unsigned char*)pixel;
                      register unsigned int c1,c2,c3,c4; 

                      edge_cnt = 1;
                      edge_sampling = true;

                      edge_avg[0] = (c1 = pixel_c[0]);
                      edge_avg[1] = (c2 = pixel_c[1]);
                      edge_avg[2] = (c3 = pixel_c[2]);
                      edge_avg[3] = (c4 = pixel_c[3]);
                      
                      edge_alpha_avg[0] = c1*c4;
                      edge_alpha_avg[1] = c2*c4;
                      edge_alpha_avg[2] = c3*c4;
                      edge_alpha_avg[3] = c4;

                      edge_base[0]=pixel_base[0];
                      edge_base[1]=pixel_base[1];
                    }
                  }
                }
                if(edge_sampling) {
                  if(edge_cnt==5) {
                    /* done with edging, so store averaged value */

                    register unsigned char *pixel_c = (unsigned char*)pixel;
                    register unsigned int c1,c2,c3,c4; 

                    edge_sampling=false;
                    /* done with edging, so store averaged value */
                    
                    if(edge_alpha_avg[3]) {
                      c4 = edge_alpha_avg[3];
                      c1 = edge_alpha_avg[0] / c4;
                      c2 = edge_alpha_avg[1] / c4;
                      c3 = edge_alpha_avg[2] / c4;
                      c4 /= edge_cnt;
                    } else {
                      c1 = edge_avg[0]/edge_cnt;
                      c2 = edge_avg[1]/edge_cnt;
                      c3 = edge_avg[2]/edge_cnt;
                      c4 = edge_avg[3]/edge_cnt;
                    }
                    pixel_c[0] = c1;
                    pixel_c[1] = c2;
                    pixel_c[2] = c3;
                    pixel_c[3] = c4;

                    /* restore X,Y coordinates */
                    r1.base[0]=pixel_base[0];
                    r1.base[1]=pixel_base[1];

                  } else {
                    *pixel = T->background;
                    switch(edge_cnt) {
                    case 1:
                      r1.base[0] = edge_base[0]+edge_width;
                      r1.base[1] = edge_base[1]+edge_height;
                      break;
                    case 2:
                      r1.base[0] = edge_base[0]+edge_width;
                      r1.base[1] = edge_base[1]-edge_height;
                      break;
                    case 3:
                      r1.base[0] = edge_base[0]-edge_width;
                      r1.base[1] = edge_base[1]+edge_height;
                      break;
                    case 4:
                      r1.base[0] = edge_base[0]-edge_width;
                      r1.base[1] = edge_base[1]-edge_height;
                      break;
                    }
                  }
                }
                if(!edge_sampling) /* not oversampling this edge or already done... */
                  break;
              } else {
                r1.base[0] = pixel_base[0];
                r1.base[1] = pixel_base[1];
              }
              
              exclude		= -1;
              persist			= _1;
              first_excess	= _0;
              excl_trans		= _0;
              pass			= 0;
              new_front		= T->front;

              if(perspective) {
                r1.base[2] = -T->front;
                r1.dir[0] = (r1.base[0] - eye[0]);
                r1.dir[1] = (r1.base[1] - eye[1]);
                r1.dir[2] = (r1.base[2] - eye[2]);
                if(interior_color>=0) {
                  start[0] = r1.base[0];
                  start[1] = r1.base[1];
                  start[2] = r1.base[2];
                }
                normalize3f(r1.dir);
                {
                  register float scale = I->max_box[2]/r1.base[2];
                  
                  r1.skip[0] = r1.base[0]*scale;
                  r1.skip[1] = r1.base[1]*scale;
                  r1.skip[2] = I->max_box[2];
                }

              }

              while((persist > _persistLimit) && (pass <= max_pass))
                {
                  pixel_flag		= false;
                  SceneCall.except = exclude;
                  SceneCall.front = new_front;
                  SceneCall.excl_trans = excl_trans;
                  SceneCall.interior_flag = false;


                  if(perspective) {
                    SceneCall.pass = pass;
                    if(pass) {
                      add3f(nudge,r1.base,r1.base);
                      copy3f(r1.base,r1.skip);
                    }
                    SceneCall.back_dist = -(T->back+r1.base[2])/r1.dir[2];
                    i = BasisHitPerspective( &SceneCall );
                  } else {
                    i = BasisHitNoShadow( &SceneCall );
                  }
                  interior_flag = SceneCall.interior_flag;
                  
                  if(((i >= 0) || interior_flag) && (pass < max_pass))
                    {
                      pixel_flag		= true;
                      n_hit++;
                      if( ((r1.trans = r1.prim->trans) != _0 ) &&
                          trans_cont_flag ) {
                        r1.trans = (float)pow(r1.trans,inv_trans_cont);
                      }
                      if(interior_flag)
                        {
                          copy3f(interior_normal,r1.surfnormal);
                          if(perspective) {
                            copy3f(start,r1.impact);
                            r1.dist = _0;
                          } else {
                            copy3f(r1.base,r1.impact);
                            r1.dist = T->front;
                            r1.impact[2]	-= T->front; 
                          }
                          
                          if(interior_wobble >= 0) 
                            {
                              wobble_save		= r1.prim->wobble; /* This is a no-no for multithreading! */
                              r1.prim->wobble	= interior_wobble;
                              
                              RayReflectAndTexture(I,&r1,perspective);
                              
                              r1.prim->wobble	= wobble_save;
                            }
                          else
                            RayReflectAndTexture(I,&r1,perspective);
                          
                          dotgle = -r1.dotgle;
                          copy3f(inter,fc);
                        }
                      else
                        {
                          if(!perspective) 
                            new_front	= r1.dist;
                          
                          if(r1.prim->type==cPrimTriangle)
                            {
                              BasisGetTriangleNormal(bp1,&r1,i,fc,perspective);
                              
                              RayProjectTriangle(I, &r1, bp2->LightNormal,
                                                 bp1->Vertex+i*3,
                                                 bp1->Normal+bp1->Vert2Normal[i]*3+3,
                                                 project_triangle);
                              
                              RayReflectAndTexture(I,&r1,perspective);
                              if(perspective) {
                                BasisGetTriangleFlatDotglePerspective(bp1,&r1,i);
                              } else {
                                BasisGetTriangleFlatDotgle(bp1,&r1,i);
                              }
                            }
                          else if(r1.prim->type==cPrimCharacter) {
                            BasisGetTriangleNormal(bp1,&r1,i,fc,perspective);
                            
                            r1.trans = CharacterInterpolate(I->G,r1.prim->char_id,fc);
                            
                            RayReflectAndTexture(I,&r1,perspective);
                            BasisGetTriangleFlatDotgle(bp1,&r1,i);
                            
                          } else { /* must be a sphere */
                            
                            if(perspective) {
                              RayGetSphereNormalPerspective(I,&r1);
                              RayReflectAndTexture(I,&r1,perspective);
                            } else {
                              RayGetSphereNormal(I,&r1);
                              RayReflectAndTexture(I,&r1,perspective);
                            }
                            
                            if((r1.prim->type==cPrimCylinder) || (r1.prim->type==cPrimSausage)) 
                              {
                                ft = r1.tri1;
                                fc[0]=(r1.prim->c1[0]*(_1-ft))+(r1.prim->c2[0]*ft);
                                fc[1]=(r1.prim->c1[1]*(_1-ft))+(r1.prim->c2[1]*ft);
                                fc[2]=(r1.prim->c1[2]*(_1-ft))+(r1.prim->c2[2]*ft);
                              }
                            else 
                              {
                                fc[0]=r1.prim->c1[0];
                                fc[1]=r1.prim->c1[1];
                                fc[2]=r1.prim->c1[2];
                              }
                          }
                          dotgle=-r1.dotgle;
                          
                          if(r1.flat_dotgle < _0)
                            {
                              if((!two_sided_lighting) && (interior_color>=0)) 
                                {
                                  interior_flag		= true;
                                  copy3f(interior_normal,r1.surfnormal);
                                  if(perspective) {
                                    copy3f(start,r1.impact);                                    
                                    r1.dist = _0;
                                  } else {
                                    copy3f(r1.base,r1.impact);
                                    r1.impact[2]		-= T->front; 
                                    r1.dist				= T->front;
                                  }
                                  
                                  if(interior_wobble >= 0)
                                    {
                                      wobble_save		= r1.prim->wobble;
                                      r1.prim->wobble	= interior_wobble;
                                      RayReflectAndTexture(I,&r1,perspective);
                                      r1.prim->wobble	= wobble_save;
                                    }
                                  else
                                    RayReflectAndTexture(I,&r1,perspective);
                                  
                                  dotgle	= -r1.dotgle;
                                  copy3f(inter,fc);
                                }
                            }
                          
                          if((dotgle < _0) && (!interior_flag))
                            {
                              if(two_sided_lighting) 
                                {
                                  dotgle	= -dotgle;
                                  invert3f(r1.surfnormal);
                                }
                              else 
                                dotgle	= _0;
                            }
                        }
                    
                      direct_cmp = (float) ( (dotgle + (pow(dotgle, settingPower))) * _p5 );
                      
                      lit = _1;
                      
                      if(shadows && ((!interior_flag)||(interior_shadows))) 
                        {
                          matrix_transform33f3f(bp2->Matrix,r1.impact,r2.base);
                          r2.base[2]-=shadow_fudge;
                          ShadeCall.except = i;
                          if(BasisHitShadow(&ShadeCall) > -1)
                            lit	= (float) pow(r2.trans, _p5);
                        }
                      
                      if(lit>_0) {
                        dotgle	= -dot_product3f(r1.surfnormal,bp2->LightNormal);
                        if(dotgle < _0) dotgle = _0;
                        
                        reflect_cmp	=(float)(lit * (dotgle + (pow(dotgle, settingReflectPower))) * _p5 );
                        dotgle	= -dot_product3f(r1.surfnormal,T->spec_vector);
                        if(dotgle < _0) dotgle=_0;
                        excess	= (float)( pow(dotgle, settingSpecPower) * settingSpecReflect * lit);
                      } else {
                        excess		= _0;
                        reflect_cmp	= _0;
                      }

                      bright	= ambient + (_1-ambient) * 
                        (((_1-direct_shade)+direct_shade*lit) * direct*direct_cmp +
                         (_1-direct)*direct_cmp*lreflect*reflect_cmp);

                      if(bright > _1) bright = _1;
                      else if(bright < _0) bright = _0;
                      
                      fc[0] = (bright*fc[0]+excess);
                      fc[1] = (bright*fc[1]+excess);
                      fc[2] = (bright*fc[2]+excess);
                      
                      if(fogFlag) 
                        {
                          if(perspective) {
                            ffact = (T->front + r1.impact[2]) * invFrontMinusBack;
                          } else {
                            ffact = (T->front - r1.dist) * invFrontMinusBack;
                          }
                          if(fogRangeFlag)
                            ffact = (ffact - fog_start) * inv1minusFogStart;
                          
						  ffact*=fog;
                          
						  if(ffact<_0)	ffact = _0;
                          if(ffact>_1)	ffact = _0;
                          
                          ffact1m	= _1-ffact;
                          
                          if(opaque_back) {
                              fc[0]	= ffact*T->bkrd[0]+fc[0]*ffact1m;
                              fc[1]	= ffact*T->bkrd[1]+fc[1]*ffact1m;
                              fc[2]	= ffact*T->bkrd[2]+fc[2]*ffact1m;
                            } else {
                              fc[3] = ffact1m*(_1 - r1.trans);
                            }
                          
                          if(!pass) {
                            if(r1.trans<trans_spec_cut) {
                              first_excess = excess*ffact1m*ray_trans_spec;
                            } else {
                              first_excess = excess*ffact1m*ray_trans_spec*
                                trans_spec_scale*(_1 - r1.trans);
                            }
                          } else {
                              fc[0]+=first_excess;
                              fc[1]+=first_excess;
                              fc[2]+=first_excess;
                            }
                        }
                      else 
                        {
                          if(!pass) {
                            if(r1.trans<trans_spec_cut) {
                              first_excess = excess*ray_trans_spec;
                            } else {
                              first_excess = excess*ray_trans_spec*
                                trans_spec_scale*(_1 - r1.trans);
                            }
                          } else {
                              fc[0]	+= first_excess;
                              fc[1]	+= first_excess;
                              fc[2]	+= first_excess;
                            }
                          if(opaque_back) {
                            fc[3]	= _1;
                          } else {
                            fc[3] = _1 - r1.trans;
                          }
                        }
                    }
                  else if(pass) 
                    {
                      /* hit nothing, and we're on on second or greater pass,
                         or we're on the last pass of a dead-end loop */
                      i=-1;

                      fc[0] = first_excess+T->bkrd[0];
                      fc[1] = first_excess+T->bkrd[1];
                      fc[2] = first_excess+T->bkrd[2];
                      if(opaque_back) {
                        fc[3] = _1;
                      } else {
                        fc[3] = _0;
                      }
                      
                      ffact = 1.0F;
                      ffact1m = 0.0F;
                      
                      pixel_flag	= true;
                      if(trans_cont_flag)
                        persist = (float)pow(persist,trans_cont);
                      
                    }

                  if(pixel_flag)
                    {
                      /*
                      inp	= (fc[0]+fc[1]+fc[2]) * _inv3;
                      if(inp < R_SMALL4) 
                        sig = _1;
                      else
                        sig = (float)(pow(inp,gamma) / inp);
                      
                      cc0 = (uint)(sig * fc[0] * _255);
                      cc1 = (uint)(sig * fc[1] * _255);
                      cc2 = (uint)(sig * fc[2] * _255);
                      */

                      cc0 = (uint)(fc[0] * _255);
                      cc1 = (uint)(fc[1] * _255);
                      cc2 = (uint)(fc[2] * _255);

                      if(cc0 > 255) cc0 = 255;
                      if(cc1 > 255) cc1 = 255;
                      if(cc2 > 255) cc2 = 255;
                      
                      if(opaque_back) 
                        { 
                          if(I->BigEndian) 
                            *pixel = T->fore_mask|(cc0<<24)|(cc1<<16)|(cc2<<8);
                          else
                            *pixel = T->fore_mask|(cc2<<16)|(cc1<<8)|cc0;
                        }
                      else	/* use alpha channel for fog with transparent backgrounds */
                        {
                          cc3	= (uint)(fc[3] * _255);
                          if(cc3 > 255) cc3 = 255;
                          
                          if(I->BigEndian)
                            *pixel = (cc0<<24)|(cc1<<16)|(cc2<<8)|cc3;
                          else
                            *pixel = (cc3<<24)|(cc2<<16)|(cc1<<8)|cc0;
                        }
                    }
                  
                  if(pass)	/* average all four channels */
                    {	
                      float mix_in;
                      if(i>=0) {
                        if(fogFlag) {
                          if(trans_cont_flag&&(ffact>_p5)) {
                            mix_in = 2*(persist*(_1-ffact)+((float)pow(persist,trans_cont)*(ffact-_p5)))
                              * (_1 - r1.trans*ffact);                            
                          } else {
                            mix_in = persist * (_1 - r1.trans*ffact);
                          }
                        } else {
                          mix_in = persist * (_1 - r1.trans);
                        }
                      } else {
                        mix_in = persist;
                      }

                      persist_inv = _1-mix_in;

                      if(!opaque_back) {
                        if(i<0) { /* hit nothing -- so don't blend */
                          fc[0] = (float)(0xFF&(last_pixel>>24));
                          fc[1] = (float)(0xFF&(last_pixel>>16));
                          fc[2] = (float)(0xFF&(last_pixel>>8));
                          fc[3] = (float)(0xFF&(last_pixel));
                          if(trans_cont_flag) { /* unless we are increasing contrast */
                            float m;
                            if(I->BigEndian) {
                              m = _1 - (float)(0xFF&(last_pixel))/_255;
                            } else {
                              m = _1 - (float)(0xFF&(last_pixel>>24))/_255;
                            }
                            m = _1 - (float)pow(m,trans_cont);
                            if(I->BigEndian) {
                              fc[3]	= m*_255 + _p499;
                            } else {
                              fc[0]	= m*_255 + _p499;
                            }
                          }
                        } else { /* hit something -- so keep blend and compute cumulative alpha*/
                          
                          fc[0]	= (0xFF&((*pixel)>>24)) * mix_in + (0xFF&(last_pixel>>24))*persist_inv;
                          fc[1]	= (0xFF&((*pixel)>>16)) * mix_in + (0xFF&(last_pixel>>16))*persist_inv;
                          fc[2]	= (0xFF&((*pixel)>>8))  * mix_in + (0xFF&(last_pixel>>8))*persist_inv;
                          fc[3]	= (0xFF&((*pixel)))     * mix_in + (0xFF&(last_pixel))*persist_inv;
                        
                          if(i>=0) { /* make sure opaque objects get opaque alpha*/
                            float o1,o2;
                            float m;
                            
                            if(I->BigEndian) {
                              o1 = (float)(0xFF&(last_pixel))/_255;
                              o2 = (float)(0xFF&(*pixel))/_255;
                            } else {
                              o1 = (float)(0xFF&(last_pixel>>24))/_255;
                              o2 = (float)(0xFF&((*pixel)>>24))/_255;
                            }
                            
                            if(o1<o2) { /* make sure o1 is largest opacity*/
                              m = o1;
                              o1 = o2;
                              o2 = m;
                            }
                            m = o1 + (1.0F - o1) * o2;
                            if(I->BigEndian) {
                            fc[3]	= m*_255 + _p499;
                            } else {
                              fc[0]	= m*_255 + _p499;
                            }
                          }
                        }
                      } else { /* opaque background, so just blend */
                        fc[0]	= (0xFF&((*pixel)>>24)) * mix_in + (0xFF&(last_pixel>>24))*persist_inv;
                        fc[1]	= (0xFF&((*pixel)>>16)) * mix_in + (0xFF&(last_pixel>>16))*persist_inv;
                        fc[2]	= (0xFF&((*pixel)>>8))  * mix_in + (0xFF&(last_pixel>>8))*persist_inv;
                        fc[3]	= (0xFF&((*pixel)))     * mix_in + (0xFF&(last_pixel))*persist_inv;
                      }
                      
                      cc0		= (uint)(fc[0]);
                      cc1		= (uint)(fc[1]);
                      cc2		= (uint)(fc[2]);
                      cc3		= (uint)(fc[3]);
                      
                      if(cc0 > 255) cc0	= 255;
                      if(cc1 > 255) cc1	= 255;
                      if(cc2 > 255) cc2	= 255;
                      if(cc3 > 255) cc3	= 255;
                      
                      *pixel = (cc0<<24)|(cc1<<16)|(cc2<<8)|cc3;
                      
                    }
                  
                  if(i >= 0)
                    {
                      if(r1.prim->type == cPrimSausage) {	/* carry ray through the stick */
                        if(perspective) 
                          excl_trans = (2*r1.surfnormal[2]*r1.prim->r1/r1.dir[2]);                          
                        else
                          excl_trans = new_front+(2*r1.surfnormal[2]*r1.prim->r1);
                      }

                      if((!backface_cull)&&(trans_mode!=2))
                        persist	= persist * r1.trans;
                      else 
                        {
                          if((persist < 0.9999) && (r1.trans))	{
                            /* don't combine transparent surfaces */ 
                            *pixel	= last_pixel;
                          } else {
                            persist	= persist * r1.trans;
                          }
                        }
                      
                    }
                  
                  if( i < 0 )	/* nothing hit */
                    {
                      break;
                    }
                  else 
                    {
                      if(perspective) {
                        float extend = r1.dist + 0.00001F;
                        scale3f(r1.dir, extend , nudge);
                      }
                      last_pixel	= *pixel;
                      exclude		= i;
                      pass++;
                    }
                  
                } /* end of ray while */

              if(blend_colors) {
                
                float red_min = _0;
                float green_min = _0;
                float blue_min = _0;
                float red_part;
                float green_part;
                float blue_part;

                if(I->BigEndian) {
                  fc[0] = (float)(0xFF&(*pixel>>24));
                  fc[1] = (float)(0xFF&(*pixel>>16));
                  fc[2] = (float)(0xFF&(*pixel>>8));
                  cc3   =        (0xFF&(*pixel));
                } else {
                  cc3   =        (0xFF&(*pixel>>24));
                  fc[2] = (float)(0xFF&(*pixel>>16));
                  fc[1] = (float)(0xFF&(*pixel>>8));
                  fc[0] = (float)(0xFF&(*pixel));
                }

                red_part = red_blend * fc[0];
                green_part = green_blend * fc[1];
                blue_part = blue_blend * fc[2];
                
                red_min = (green_part>blue_part) ? green_part : blue_part;
                green_min = (red_part>blue_part) ? red_part : blue_part;
                blue_min = (green_part>red_part) ? green_part : red_part;
                
                if(fc[0]<red_min) fc[0] = red_min;
                if(fc[1]<green_min) fc[1] = green_min;
                if(fc[2]<blue_min) fc[2] = blue_min;

                cc0 = (uint)(fc[0]);
                cc1 = (uint)(fc[1]);
                cc2 = (uint)(fc[2]);
                
                if(cc0 > 255) cc0 = 255;
                if(cc1 > 255) cc1 = 255;
                if(cc2 > 255) cc2 = 255;
                
                if(I->BigEndian) 
                  *pixel = (cc0<<24)|(cc1<<16)|(cc2<<8)|cc3;
                else
                  *pixel = (cc3<<24)|(cc2<<16)|(cc1<<8)|cc0;
              }
            
              if(!T->edging) break;
              /* if here, then we're edging...
                 so accumulate averages */
              { 
                
                register unsigned char *pixel_c = (unsigned char*)pixel;
                register unsigned int c1,c2,c3,c4; 
                
                edge_avg[0] += (c1 = pixel_c[0]);
                edge_avg[1] += (c2 = pixel_c[1]);
                edge_avg[2] += (c3 = pixel_c[2]);
                edge_avg[3] += (c4 = pixel_c[3]);
                
                edge_alpha_avg[0] += c1*c4;
                edge_alpha_avg[1] += c2*c4;
                edge_alpha_avg[2] += c3*c4;
                edge_alpha_avg[3] += c4;

                edge_cnt++;
              }
              
            } /* end of edging while */
            pixel++;
         }	/* end of for */
         
		}	/* end of if */
		
	}	/* end of for */
	
	/*  if(T->n_thread>1) 
	  printf(" Ray: Thread %d: Complete.\n",T->phase+1);*/
	MapCacheFree(&SceneCall.cache,T->phase,cCache_map_scene_cache);
	
	if(shadows&&(I->NBasis>1))
		MapCacheFree(&ShadeCall.cache,T->phase,cCache_map_shadow_cache);
	
	return (n_hit);
}

/* this is both an antialias and a slight blur */

/* for whatever reason, greatly GCC perfers a linear sequence of
   accumulates over a single large expression -- the difference is
   huge: over 10% !!! */


#define combine4by4(var,src,mask) { \
  var =  ((src)[0 ] & mask)   ; \
  var += ((src)[1 ] & mask)   ; \
  var += ((src)[2 ] & mask)   ; \
  var += ((src)[3 ] & mask)   ; \
  var += ((src)[4 ] & mask)   ; \
  var +=(((src)[5 ] & mask)*13) ; \
  var +=(((src)[6 ] & mask)*13) ; \
  var += ((src)[7 ] & mask)   ; \
  var += ((src)[8 ] & mask)    ; \
  var +=(((src)[9 ] & mask)*13) ; \
  var +=(((src)[10] & mask)*13) ; \
  var += ((src)[11] & mask)   ; \
  var += ((src)[12] & mask)   ; \
  var += ((src)[13] & mask)   ; \
  var += ((src)[14] & mask)   ; \
  var += ((src)[15] & mask)   ; \
  var = (var >> 6) & mask; \
}

#define combine5by5(var,src,mask) { \
  var =  ((src)[0 ] & mask)   ; \
  var += ((src)[1 ] & mask)   ; \
  var += ((src)[2 ] & mask)   ; \
  var += ((src)[3 ] & mask)   ; \
  var += ((src)[4 ] & mask)   ; \
  var += ((src)[5 ] & mask)   ; \
  var +=(((src)[6 ] & mask)*5); \
  var +=(((src)[7 ] & mask)*5); \
  var +=(((src)[8 ] & mask)*5); \
  var += ((src)[9 ] & mask)   ; \
  var += ((src)[10] & mask)   ; \
  var +=(((src)[11] & mask)*5); \
  var +=(((src)[12] & mask)*8); \
  var +=(((src)[13] & mask)*5); \
  var += ((src)[14] & mask)   ; \
  var += ((src)[15] & mask)   ; \
  var +=(((src)[16] & mask)*5); \
  var +=(((src)[17] & mask)*5); \
  var +=(((src)[18] & mask)*5); \
  var += ((src)[19] & mask)   ; \
  var += ((src)[20] & mask)   ; \
  var += ((src)[21] & mask)   ; \
  var += ((src)[22] & mask)   ; \
  var += ((src)[23] & mask)   ; \
  var += ((src)[24] & mask)   ; \
  var = (var >> 6) & mask; \
 }

#define combine6by6(var,src,mask) { \
  var =  ((src)[0 ] & mask)   ; \
  var += ((src)[1 ] & mask)   ; \
  var += ((src)[2 ] & mask)   ; \
  var += ((src)[3 ] & mask)   ; \
  var += ((src)[4 ] & mask)   ; \
  var += ((src)[5 ] & mask)   ; \
  var += ((src)[6 ] & mask)   ; \
  var +=(((src)[7 ] & mask)*5); \
  var +=(((src)[8 ] & mask)*7); \
  var +=(((src)[9 ] & mask)*7); \
  var +=(((src)[10] & mask)*5); \
  var += ((src)[11] & mask)   ; \
  var += ((src)[12] & mask)   ; \
  var +=(((src)[13] & mask)*7); \
  var +=(((src)[14] & mask)*8); \
  var +=(((src)[15] & mask)*8); \
  var +=(((src)[16] & mask)*7); \
  var += ((src)[17] & mask)   ; \
  var += ((src)[18] & mask)   ; \
  var +=(((src)[19] & mask)*7); \
  var +=(((src)[20] & mask)*8); \
  var +=(((src)[21] & mask)*8); \
  var +=(((src)[22] & mask)*7); \
  var += ((src)[23] & mask)   ; \
  var += ((src)[24] & mask)   ; \
  var +=(((src)[25] & mask)*5); \
  var +=(((src)[26] & mask)*7); \
  var +=(((src)[27] & mask)*7); \
  var +=(((src)[28] & mask)*5); \
  var += ((src)[29] & mask)   ; \
  var += ((src)[30] & mask)   ; \
  var += ((src)[31] & mask)   ; \
  var += ((src)[32] & mask)   ; \
  var += ((src)[33] & mask)   ; \
  var += ((src)[34] & mask)   ; \
  var += ((src)[35] & mask)   ; \
  var = (var >> 7) & mask; \
 }

#define m00FF 0x00FF
#define mFF00 0xFF00
#define mFFFF 0xFFFF

int RayAntiThread(CRayAntiThreadInfo *T)
{
	int		src_row_pixels;
	
	unsigned int *pSrc;
	unsigned int *pDst;
   /*   unsigned int m00FF=0x00FF,mFF00=0xFF00,mFFFF=0xFFFF;*/
	int width;
	int height;
	int x,y,yy;
	unsigned int *p;
	int offset = 0;
	CRay *I = T->ray;

	OrthoBusyFast(I->G,9,10);
	width	= (T->width/T->mag) - 2;
	height = (T->height/T->mag) - 2;
	
	src_row_pixels	= T->width;

	offset = (T->phase * height)/T->n_thread;
	offset = offset - (offset % T->n_thread) + T->phase;

	for(yy = 0; yy< height; yy++ ) {
      y = (yy + offset) % height; /* make sure threads write to different pages */
      
      if((y % T->n_thread) == T->phase)	{ /* this is my scan line */
        register unsigned long c1,c2,c3,c4,a;
        register unsigned char *c;
        
        pSrc	= T->image + src_row_pixels * (y*T->mag);
        pDst	= T->image_copy + width * y ;	
        switch(T->mag) {
        case 2: 
          {
            for(x = 0; x < width; x++) {
              
              c = (unsigned char*)( p = pSrc + (x * T->mag));
              c1 = c2 = c3 = c4 = a = 0;

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);
                
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*13); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*13); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*13); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*13); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;

              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;

              if(c4) {
                c1 /= c4;
                c2 /= c4;
                c3 /= c4;
              } else { /* compute straight RGB average */
                
                c = (unsigned char*)( p = pSrc + (x * T->mag));
                c1 = c2 = c3 = 0;

                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=13*c[0]; c2+=13*c[1]; c3+=13*c[2]; c+=4;
                c1+=13*c[0]; c2+=13*c[1]; c3+=13*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=13*c[0]; c2+=13*c[1]; c3+=13*c[2]; c+=4;
                c1+=13*c[0]; c2+=13*c[1]; c3+=13*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c1 = c1>>6;
                c2 = c2>>6;
                c3 = c3>>6;
              }
              
              c = (unsigned char*)(pDst++);
              
              *(c++) = c1;
              *(c++) = c2;
              *(c++) = c3;
              *(c++) = c4>>6;
            }
          }
          break;
        case 3:
          {
            for(x = 0; x < width; x++) {
              
              c = (unsigned char*)( p = pSrc + (x * T->mag));
              c1 = c2 = c3 = c4 = a = 0;
              
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);
                
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*8); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;

              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;

              if(c4) {
                c1 /= c4;
                c2 /= c4;
                c3 /= c4;
              } else { /* compute straight RGB average */
                
                c = (unsigned char*)( p = pSrc + (x * T->mag));
                c1 = c2 = c3 = 0;
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=8*c[0]; c2+=8*c[1]; c3+=8*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;

                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c1 = c1>>6;
                c2 = c2>>6;
                c3 = c3>>6;
              }
              
              c = (unsigned char*)(pDst++);
              
              *(c++) = c1;
              *(c++) = c2;
              *(c++) = c3;
              *(c++) = c4>>6;
            }
          }
          break;
        case 4:
          {
            for(x = 0; x < width; x++) {
              
              c = (unsigned char*)( p = pSrc + (x * T->mag));
              c1 = c2 = c3 = c4 = a = 0;
              
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);
                
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*8); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*8); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*8); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*8); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*7); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]*5); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              
              c = (unsigned char*)(p += src_row_pixels);

              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;
              c4+=(a=c[3]); c1+=c[0]*a; c2+=c[1]*a; c3+=c[2]*a; c+=4;

              if(c4) {
                c1 /= c4;
                c2 /= c4;
                c3 /= c4;
              } else { /* compute straight RGB average */
                
                c = (unsigned char*)( p = pSrc + (x * T->mag));
                c1 = c2 = c3 = 0;
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;


                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=8*c[0]; c2+=8*c[1]; c3+=8*c[2]; c+=4;
                c1+=8*c[0]; c2+=8*c[1]; c3+=8*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;

                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=8*c[0]; c2+=8*c[1]; c3+=8*c[2]; c+=4;
                c1+=8*c[0]; c2+=8*c[1]; c3+=8*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;


                c = (unsigned char*)(p += src_row_pixels);
                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=7*c[0]; c2+=7*c[1]; c3+=7*c[2]; c+=4;
                c1+=5*c[0]; c2+=5*c[1]; c3+=5*c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;

                c = (unsigned char*)(p += src_row_pixels);

                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;                
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                c1+=c[0]; c2+=c[1]; c3+=c[2]; c+=4;
                
                c1 = c1>>7;
                c2 = c2>>7;
                c3 = c3>>7;
              }
              
              c = (unsigned char*)(pDst++);
              
              *(c++) = c1;
              *(c++) = c2;
              *(c++) = c3;
              *(c++) = c4>>7;
            }
          }
          break;

        }
      }
    }
    return 1;
}

#ifdef PROFILE_BASIS
extern int n_cells;
extern int n_prims;
extern int n_triangles;
extern int n_spheres;
extern int n_cylinders;
extern int n_sausages;
extern int n_skipped;
#endif

/*========================================================================*/
void RayRender(CRay *I,int width,int height,unsigned int *image,
               float front,float back,double timing,float angle,
               float fov,float *pos)
{
  int a;
  float *v,light[3];
  unsigned int *image_copy = NULL;
  unsigned int back_mask,fore_mask=0;
  unsigned int background,buffer_size;
  int antialias;
int opaque_back=0;
  int n_hit=0;
  float *bkrd_ptr,bkrd[3];
  double now;
  int shadows;
  float spec_vector[3];
  int n_thread;
  int mag=1;
  int oversample_cutoff;
  int perspective = SettingGetGlobal_i(I->G,cSetting_ray_orthoscopic);
  const float _0 = 0.0F, _p499 = 0.499F;
  if(perspective<0)
    perspective = SettingGetGlobal_b(I->G,cSetting_ortho);
  perspective = !perspective;
      
#ifdef PROFILE_BASIS
  n_cells = 0;
  n_prims = 0;
  n_triangles = 0;
  n_spheres = 0;
  n_cylinders = 0;
  n_sausages = 0;
  n_skipped = 0;
#endif

  n_thread  = SettingGetGlobal_i(I->G,cSetting_max_threads);
  if(n_thread<1)
    n_thread=1;
  if(n_thread>MAX_RAY_THREADS)
    n_thread = MAX_RAY_THREADS;
  opaque_back = SettingGetGlobal_i(I->G,cSetting_ray_opaque_background);
  if(opaque_back<0)
    opaque_back			= SettingGetGlobal_i(I->G,cSetting_opaque_background);      

  shadows = SettingGetGlobal_i(I->G,cSetting_ray_shadows);
  antialias = SettingGetGlobal_i(I->G,cSetting_antialias);
  if(antialias<0) antialias=0;
  if(antialias>4) antialias=4;
  mag = antialias;
  if(mag<1) mag=1;

  if(antialias>1) {
    width=(width+2)*mag;
    height=(height+2)*mag;
    image_copy = image;
    buffer_size = mag*mag*width*height;
    image = CacheAlloc(I->G,unsigned int,buffer_size,0,cCache_ray_antialias_buffer);
    ErrChkPtr(I->G,image);
  } else {
    buffer_size = width*height;
  }
  bkrd_ptr=SettingGetfv(I->G,cSetting_bg_rgb);
  copy3f(bkrd_ptr,bkrd);
  { /* adjust bkrd to offset the effect of gamma correction */
    float gamma = SettingGet(I->G,cSetting_gamma);
    register float inp;
    register float sig;

    inp = (bkrd[0]+bkrd[1]+bkrd[2])/3.0F;
    if(inp < R_SMALL4) 
      sig = 1.0F;
    else
      sig = (float)(pow(inp,gamma))/inp;
    bkrd[0] *= sig;
    bkrd[1] *= sig;
    bkrd[2] *= sig;
    if(bkrd[0]>1.0F) bkrd[0] = 1.0F;
    if(bkrd[1]>1.0F) bkrd[1] = 1.0F;
    if(bkrd[2]>1.0F) bkrd[2] = 1.0F;
  }

  if(opaque_back) {
    if(I->BigEndian)
      back_mask = 0x000000FF;
    else
      back_mask = 0xFF000000;
    fore_mask = back_mask;
  } else {
    if(I->BigEndian) {
      back_mask = 0x00000000;
    } else {
      back_mask = 0x00000000;
    }
  }
  if(I->BigEndian) {
     background = back_mask|
      ((0xFF& ((unsigned int)(bkrd[0]*255+_p499))) <<24)|
      ((0xFF& ((unsigned int)(bkrd[1]*255+_p499))) <<16)|
      ((0xFF& ((unsigned int)(bkrd[2]*255+_p499))) <<8 );
  } else {
    background = back_mask|
      ((0xFF& ((unsigned int)(bkrd[2]*255+_p499))) <<16)|
      ((0xFF& ((unsigned int)(bkrd[1]*255+_p499))) <<8)|
      ((0xFF& ((unsigned int)(bkrd[0]*255+_p499))) );
  }

  OrthoBusyFast(I->G,2,20);

  PRINTFB(I->G,FB_Ray,FB_Blather) 
    " RayNew: Background = %x %d %d %d\n",background,(int)(bkrd[0]*255),
    (int)(bkrd[1]*255),(int)(bkrd[2]*255)
    ENDFB(I->G);

  if(!I->NPrimitive) { /* nothing to render! */
    fill(image,background,width * (unsigned int)height);
  } else {
    
    RayExpandPrimitives(I);
    RayTransformFirst(I,perspective);

    OrthoBusyFast(I->G,3,20);

    now = UtilGetSeconds(I->G)-timing;

	 PRINTFB(I->G,FB_Ray,FB_Blather)
      " Ray: processed %i graphics primitives in %4.2f sec.\n",I->NPrimitive,now
      ENDFB(I->G);

    I->NBasis=3; /* light source */
    BasisInit(I->G,I->Basis+2,2);
    
    { /* setup light & rotate if necessary  */
      v=SettingGetfv(I->G,cSetting_light);
      copy3f(v,light);
      
      if(angle) {
        float temp[16];
        identity44f(temp);
        MatrixRotateC44f(temp,(float)-PI*angle/180,0.0F,1.0F,0.0F);
        MatrixTransformC44fAs33f3f(temp,light,light);
      }
      
      I->Basis[2].LightNormal[0]=light[0];
      I->Basis[2].LightNormal[1]=light[1];
      I->Basis[2].LightNormal[2]=light[2];
      normalize3f(I->Basis[2].LightNormal);
      
      copy3f(I->Basis[2].LightNormal,spec_vector);
      spec_vector[2]--; /* HUH? */
      normalize3f(spec_vector);
      
    }

    if(shadows) { /* don't waste time on shadows unless needed */
      BasisSetupMatrix(I->Basis+2);
      RayTransformBasis(I,I->Basis+2,2);
    }

    OrthoBusyFast(I->G,4,20);
#ifndef _PYMOL_NOPY
    if(shadows&&(n_thread>1)) { /* parallel execution */

      CRayHashThreadInfo thread_info[2];
      
      /* rendering map */

      thread_info[0].basis = I->Basis+1;
      thread_info[0].vert2prim = I->Vert2Prim;
      thread_info[0].prim = I->Primitive;
      thread_info[0].clipBox = I->Volume;
      thread_info[0].image = image;
      thread_info[0].background = background;
      thread_info[0].bytes = width * (unsigned int)height;
      thread_info[0].phase = 0;
      thread_info[0].ray = I; /* for compute box */
      thread_info[0].perspective = perspective;
      thread_info[0].front = front;
      /* shadow map */

      thread_info[1].basis = I->Basis+2;
      thread_info[1].vert2prim = I->Vert2Prim;
      thread_info[1].prim = I->Primitive;
      thread_info[1].clipBox = NULL;
      thread_info[1].phase = 1;
      thread_info[1].perspective = false; 
      RayHashSpawn(thread_info,2);
      
    } else
#endif
      { /* serial execution */
        BasisMakeMap(I->Basis+1,I->Vert2Prim,I->Primitive,
                     I->Volume,0,cCache_ray_map,
                     perspective,front);
        if(shadows) {
          BasisMakeMap(I->Basis+2,I->Vert2Prim,I->Primitive,
                       NULL,1,cCache_ray_map,false,_0);
        }

      /* serial tasks which RayHashThread does in parallel mode */

      fill(image,background,width * (unsigned int)height);
      RayComputeBox(I);
       
    }

    OrthoBusyFast(I->G,5,20);
    now = UtilGetSeconds(I->G)-timing;

#ifdef _MemoryDebug_ON
    if(Feedback(I->G,FB_Ray,FB_Debugging)) {
      MemoryDebugDump();
    }
    if(shadows) {
      PRINTFB(I->G,FB_Ray,FB_Blather)
        " Ray: voxels: [%4.2f:%dx%dx%d], [%4.2f:%dx%dx%d], %d MB, %4.2f sec.\n",
        I->Basis[1].Map->Div,   I->Basis[1].Map->Dim[0],
        I->Basis[1].Map->Dim[1],I->Basis[1].Map->Dim[2],
        I->Basis[2].Map->Div,   I->Basis[2].Map->Dim[0],
        I->Basis[2].Map->Dim[2],I->Basis[2].Map->Dim[2],
        (int)(MemoryDebugUsage()/(1024.0*1024)),
        now
        ENDFB(I->G);
    } else {
      PRINTFB(I->G,FB_Ray,FB_Blather)
        " Ray: voxels: [%4.2f:%dx%dx%d], %d MB, %4.2f sec.\n",
        I->Basis[1].Map->Div,   I->Basis[1].Map->Dim[0],
        I->Basis[1].Map->Dim[1],I->Basis[1].Map->Dim[2],
        (int)(MemoryDebugUsage()/(1024.0*1024)),
        now
        ENDFB(I->G);
    }
#else
    if(shadows) {
      PRINTFB(I->G,FB_Ray,FB_Blather)
        " Ray: voxels: [%4.2f:%dx%dx%d], [%4.2f:%dx%dx%d], %4.2f sec.\n",
        I->Basis[1].Map->Div,   I->Basis[1].Map->Dim[0],
        I->Basis[1].Map->Dim[1],I->Basis[1].Map->Dim[2],
        I->Basis[2].Map->Div,   I->Basis[2].Map->Dim[0],
        I->Basis[2].Map->Dim[2],I->Basis[2].Map->Dim[2],
        now
        ENDFB(I->G);
    } else {
      PRINTFB(I->G,FB_Ray,FB_Blather)
        " Ray: voxels: [%4.2f:%dx%dx%d], %4.2f sec.\n",
        I->Basis[1].Map->Div,   I->Basis[1].Map->Dim[0],
        I->Basis[1].Map->Dim[1],I->Basis[1].Map->Dim[2],
        now
        ENDFB(I->G);
    }

#endif
    /* IMAGING */
        
    {
		/* now spawn threads as needed */
		CRayThreadInfo rt[MAX_RAY_THREADS];
      
      int x_start=0,y_start=0;
      int x_stop=0,y_stop=0;
      float x_test=_0, y_test=_0;
      int x_pixel, y_pixel;

      if(perspective) {
        int c;

        if(I->min_box[2]>-front)
          I->min_box[2] = -front;
        if(I->max_box[2]>-front)
          I->max_box[2] = -front;

        for(c=0;c<4;c++) {
          switch(c) {
          case 0:
            x_test = -I->min_box[0]/I->min_box[2];
            y_test = -I->min_box[1]/I->min_box[2];
            break;
          case 1:
            x_test = -I->min_box[0]/I->max_box[2];
            y_test = -I->min_box[1]/I->max_box[2];
            break;
          case 2:
            x_test = -I->max_box[0]/I->min_box[2];
            y_test = -I->max_box[1]/I->min_box[2];
            break;
          case 3:
            x_test = -I->max_box[0]/I->max_box[2];
            y_test = -I->max_box[1]/I->max_box[2];
            break;
          }

          /* project onto back to get the effective range */

          x_pixel = (int)(width * (((x_test*I->Volume[5])-I->Volume[0])/I->Range[0])); 
          y_pixel = (int)(height * (((y_test*I->Volume[5])-I->Volume[2])/I->Range[1]));

          if(!c) {
            x_start = x_pixel;
            x_stop = x_pixel;
            y_start = y_pixel;
            y_stop = y_pixel;
          } else {
            if(x_start>x_pixel) x_start = x_pixel;
            if(x_stop<x_pixel) x_stop = x_pixel;
            if(y_start>y_pixel) y_start = y_pixel;
            if(y_stop<y_pixel) y_stop = y_pixel;
          }
        }
        x_start -=2;
        x_stop +=2;
        y_start -=2;
        y_stop +=2;

        /*
          x_start = 0; 
          y_start = 0;
          x_stop = width;
          y_stop = height;*/

      } else {
        x_start = (int)((width * (I->min_box[0] - I->Volume[0]))/I->Range[0]) - 2;
        x_stop  = (int)((width * (I->max_box[0] - I->Volume[0]))/I->Range[0]) + 2;
        
        y_stop = (int)((height * (I->max_box[1] - I->Volume[2]))/I->Range[1]) + 2;
        y_start  = (int)((height * (I->min_box[1] - I->Volume[2]))/I->Range[1]) - 2;
        
      }
      if(x_start<0) x_start = 0;
      if(y_start<0) y_start = 0;
      if(x_stop>width) x_stop = width;
      if(y_stop>height) y_stop = height;
      
      oversample_cutoff = SettingGetGlobal_i(I->G,cSetting_ray_oversample_cutoff);

      if(!antialias)
        oversample_cutoff = 0;

		for(a=0;a<n_thread;a++) {
			rt[a].ray = I;
			rt[a].width = width;
			rt[a].height = height;
         rt[a].x_start = x_start;
         rt[a].x_stop = x_stop;
         rt[a].y_start = y_start;
         rt[a].y_stop = y_stop;
			rt[a].image = image;
			rt[a].border = mag-1;
			rt[a].front = front;
			rt[a].back = back;
			rt[a].fore_mask = fore_mask;
			rt[a].bkrd = bkrd;
			rt[a].background = background;
			rt[a].phase = a;
			rt[a].n_thread = n_thread;
         rt[a].edging = NULL;
         rt[a].edging_cutoff = oversample_cutoff; /* info needed for busy indicator */
         rt[a].perspective = perspective;
         rt[a].fov = fov;
         rt[a].pos[2] = pos[2];
			copy3f(spec_vector,rt[a].spec_vector);
			}
		
#ifndef _PYMOL_NOPY
		if(n_thread>1)
        RayTraceSpawn(rt,n_thread);
      else
#endif
        RayTraceThread(rt);

      if(oversample_cutoff) { /* perform edge oversampling, if requested */
        unsigned int *edging;

        edging = CacheAlloc(I->G,unsigned int,buffer_size,0,cCache_ray_edging_buffer);
        
        memcpy(edging,image,buffer_size*sizeof(unsigned int));

        for(a=0;a<n_thread;a++) {
          rt[a].edging = edging;
        }

#ifndef _PYMOL_NOPY
        if(n_thread>1)
          RayTraceSpawn(rt,n_thread);
        else 
#endif
          RayTraceThread(rt);

        CacheFreeP(I->G,edging,0,cCache_ray_edging_buffer,false);
      }
    }
  }
  
  if(antialias>1) {
    {
		/* now spawn threads as needed */
		CRayAntiThreadInfo rt[MAX_RAY_THREADS];

		for(a=0;a<n_thread;a++) 
		{
			rt[a].width = width;
			rt[a].height = height;
         rt[a].image = image;
         rt[a].image_copy = image_copy;
         rt[a].phase = a;
         rt[a].mag = mag; /* fold magnification */
         rt[a].n_thread = n_thread;
         rt[a].ray = I;
      }
		
#ifndef _PYMOL_NOPY
		if(n_thread>1)
        RayAntiSpawn(rt,n_thread);
		else 
#endif
        RayAntiThread(rt);

    }
    CacheFreeP(I->G,image,0,cCache_ray_antialias_buffer,false);
    image = image_copy;
  }

  PRINTFD(I->G,FB_Ray)
    " RayRender: n_hit %d\n",n_hit
    ENDFD;
#ifdef PROFILE_BASIS

  printf("int n_cells = %d;\nint n_prims = %d;\nint n_triangles = %8.3f;\nint n_spheres = %8.3f;\nint n_cylinders = %8.3f;\nint n_sausages = %8.3f;\nint n_skipped = %8.3f;\n",
         n_cells,
         n_prims,
         n_triangles/((float)n_cells),
         n_spheres/((float)n_cells),
         n_cylinders/((float)n_cells),
         n_sausages/((float)n_cells),
         n_skipped/((float)n_cells));
#endif

}

void RayRenderColorTable(CRay *I,int width,int height,int *image)
{
  int x,y;
  unsigned int r=0,g=0,b=0;
  unsigned int *pixel,mask,*p;

  if(I->BigEndian)
    mask = 0x000000FF;
  else
    mask = 0xFF000000;

  p=(unsigned int*)image; 
  for(x=0;x<width;x++)
    for(y=0;y<height;y++)
      *(p++)=mask;
  
  if((width>=512)&&(height>=512)) {
    
    
    for(y=0;y<512;y++) 
      for(x=0;x<512;x++)        
        {
          pixel = (unsigned int*) (image+((width)*y)+x);
          if(I->BigEndian) {
            *(pixel)=
              mask|(r<<24)|(g<<16)|(b<<8);
          } else {
            *(pixel)=
              mask|(b<<16)|(g<<8)|r;
          }
          b = b + 4;
          if(!(0xFF&b)) { 
            b=0;
            g=g+4;
            if(!(0xFF&g)) {           
              g=0;
              r=r+4;
            }
          }
        }
  }
}
/*========================================================================*/
void RayWobble(CRay *I,int mode,float *v)
{
  I->Wobble=mode;
  if(v) 
    copy3f(v,I->WobbleParam);
}
/*========================================================================*/
void RayTransparentf(CRay *I,float v)
{
  I->Trans=v;
}
/*========================================================================*/
void RayColor3fv(CRay *I,float *v)
{
  I->CurColor[0]=(*v++);
  I->CurColor[1]=(*v++);
  I->CurColor[2]=(*v++);
}
/*========================================================================*/
void RaySphere3fv(CRay *I,float *v,float r)
{
  CPrimitive *p;

  float *vv;

  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimSphere;
  p->r1=r;
  p->trans=I->Trans;
  p->wobble=I->Wobble;
  /* 
    copy3f(I->WobbleParam,p->wobble_param);*/
  vv=p->v1;
  (*vv++)=(*v++);
  (*vv++)=(*v++);
  (*vv++)=(*v++);


  vv=p->c1;
  v=I->CurColor;
  (*vv++)=(*v++);
  (*vv++)=(*v++);
  (*vv++)=(*v++);

  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
  }

  I->NPrimitive++;
}

/*========================================================================*/
void RayCharacter(CRay *I,int char_id, float xorig, float yorig, float advance)
{
  CPrimitive *p;
  float *v;
  float vt[3];
  float *vv;
  float width,height;

  v = TextGetPos(I->G);
  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive+1,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimCharacter;
  p->trans=I->Trans;
  p->char_id = char_id;
  p->wobble=I->Wobble;
  /*
    copy3f(I->WobbleParam,p->wobble_param);*/

  vv=p->v1;
  (*vv++)=v[0];
  (*vv++)=v[1];
  (*vv++)=v[2];

  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
  }

  {
    float xn[3] = {1.0F,0.0F,0.0F};
    float yn[3] = {0.0F,1.0F,0.0F};
    float zn[3] = {0.0F,0.0F,1.0F};
    float sc[3];
    float scale;
    CPrimitive *pp = p+1;

    RayApplyMatrixInverse33(1,(float3*)xn,I->Rotation,(float3*)xn);    
    RayApplyMatrixInverse33(1,(float3*)yn,I->Rotation,(float3*)yn);    
    RayApplyMatrixInverse33(1,(float3*)zn,I->Rotation,(float3*)zn);    

    scale = I->PixelRadius * advance;
    scale3f(xn,scale,vt); /* advance raster position in 3-space */
    add3f(v,vt,vt);
    TextSetPos(I->G,vt);

    /* position the pixmap relative to raster position */

    scale = ((-xorig)-0.5F)*I->PixelRadius;
    scale3f(xn,scale,sc);
    add3f(sc,p->v1,p->v1);
         
    scale = ((-yorig)-0.5F)*I->PixelRadius;
    scale3f(yn,scale,sc);
    add3f(sc,p->v1,p->v1);
    
    width = (float)CharacterGetWidth(I->G,char_id);
    height = (float)CharacterGetHeight(I->G,char_id);

    scale = I->PixelRadius*width;
    scale3f(xn,scale,xn);
    scale = I->PixelRadius*height;
    scale3f(yn,scale,yn);

    copy3f(zn,p->n0);
    copy3f(zn,p->n1);
    copy3f(zn,p->n2);
    copy3f(zn,p->n3);


    *(pp)=(*p);

    /* define coordinates of first triangle */

    add3f(p->v1,xn,p->v2);
    add3f(p->v1,yn,p->v3);
    
    /* encode characters coordinates in the colors  */

    zero3f(p->c1);
    set3f(p->c2,width,0.0F,0.0F);
    set3f(p->c3,0.0F,height,0.0F);

    /* define coordinates of second triangle */

    add3f(yn,xn,pp->v1);
    add3f(p->v1,pp->v1,pp->v1);
    add3f(p->v1,yn,pp->v2);
    add3f(p->v1,xn,pp->v3);

    /* encode integral character coordinates into the vertex colors  */

    set3f(pp->c1,width,height,0.0F);
    set3f(pp->c2,0.0F,height,0.0F);
    set3f(pp->c3,width,0.0F,0.0F);

  }
  I->NPrimitive+=2;
}
/*========================================================================*/
void RayCylinder3fv(CRay *I,float *v1,float *v2,float r,float *c1,float *c2)
{
  CPrimitive *p;

  float *vv;

  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimCylinder;
  p->r1=r;
  p->trans=I->Trans;
  p->cap1=cCylCapFlat;
  p->cap2=cCylCapFlat;
  p->wobble=I->Wobble;
  /* 
 copy3f(I->WobbleParam,p->wobble_param);*/

  vv=p->v1;
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  vv=p->v2;
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);

  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
    transformTTT44f3f(I->TTT,p->v2,p->v2);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
    RayApplyContextToVertex(I,p->v2);
  }

  vv=p->c1;
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  vv=p->c2;
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);

  I->NPrimitive++;
}
/*========================================================================*/
void RayCustomCylinder3fv(CRay *I,float *v1,float *v2,float r,
                          float *c1,float *c2,int cap1,int cap2)
{
  CPrimitive *p;

  float *vv;

  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimCylinder;
  p->r1=r;
  p->trans=I->Trans;
  p->cap1=cap1;
  p->cap2=cap2;
  p->wobble=I->Wobble;
  /*
  copy3f(I->WobbleParam,p->wobble_param);*/

  vv=p->v1;
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  vv=p->v2;
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);

  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
    transformTTT44f3f(I->TTT,p->v2,p->v2);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
    RayApplyContextToVertex(I,p->v2);
  }

  vv=p->c1;
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  vv=p->c2;
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);

  I->NPrimitive++;
}
/*========================================================================*/
void RaySausage3fv(CRay *I,float *v1,float *v2,float r,float *c1,float *c2)
{
  CPrimitive *p;

  float *vv;

  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimSausage;
  p->r1=r;
  p->trans=I->Trans;
  p->wobble=I->Wobble;
  /*  
    copy3f(I->WobbleParam,p->wobble_param);*/

  vv=p->v1;
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  vv=p->v2;
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);

  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
    transformTTT44f3f(I->TTT,p->v2,p->v2);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
    RayApplyContextToVertex(I,p->v2);
  }

  vv=p->c1;
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  vv=p->c2;
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);

  I->NPrimitive++;
}
/*========================================================================*/
void RayTriangle3fv(CRay *I,
						  float *v1,float *v2,float *v3,
						  float *n1,float *n2,float *n3,
						  float *c1,float *c2,float *c3)
{
  CPrimitive *p;

  float *vv;
  float n0[3],nx[3],s1[3],s2[3],s3[3];
  float l1,l2,l3;

  /*  dump3f(v1," v1");
  dump3f(v2," v2");
  dump3f(v3," v3");
  dump3f(n1," n1");
  dump3f(n2," n2");
  dump3f(n3," n3");
  dump3f(c1," c1");
  dump3f(c2," c2");
  dump3f(c3," c3");*/
  VLACacheCheck(I->G,I->Primitive,CPrimitive,I->NPrimitive,0,cCache_ray_primitive);
  p = I->Primitive+I->NPrimitive;

  p->type = cPrimTriangle;
  p->trans=I->Trans;
  p->wobble=I->Wobble;
  /*
    copy3f(I->WobbleParam,p->wobble_param);*/

  /* determine exact triangle normal */
  add3f(n1,n2,nx);
  add3f(n3,nx,nx);
  subtract3f(v1,v2,s1);
  subtract3f(v3,v2,s2);
  subtract3f(v1,v3,s3);
  cross_product3f(s1,s2,n0);
  if((fabs(n0[0])<RAY_SMALL)&&
	  (fabs(n0[1])<RAY_SMALL)&&
	  (fabs(n0[2])<RAY_SMALL))
	 {copy3f(nx,n0);} /* fall-back */
  else if(dot_product3f(n0,nx)<0)
	 invert3f(n0);
  normalize3f(n0);

  vv=p->n0;
  (*vv++)=n0[0];
  (*vv++)=n0[1];
  (*vv++)=n0[2];

  /* determine maximum distance from vertex to point */
  l1=(float)length3f(s1);
  l2=(float)length3f(s2);
  l3=(float)length3f(s3);
  if(l2>l1) { if(l3>l2)	l1=l3; else	l1=l2;  }
  /* store cutoff distance */

  p->r1=l1*0.6F;

  /*  if(l1>20) {
		printf("%8.3f\n",l1);
		printf("%8.3f %8.3f %8.3f\n",s1[0],s1[1],s1[2]);
		printf("%8.3f %8.3f %8.3f\n",s2[0],s2[1],s2[2]);
		printf("%8.3f %8.3f %8.3f\n",s3[0],s3[1],s3[2]);
		}*/

  vv=p->v1;
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  (*vv++)=(*v1++);
  vv=p->v2;
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);
  (*vv++)=(*v2++);
  vv=p->v3;
  (*vv++)=(*v3++);
  (*vv++)=(*v3++);
  (*vv++)=(*v3++);

  vv=p->c1;
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  (*vv++)=(*c1++);
  vv=p->c2;
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);
  (*vv++)=(*c2++);
  vv=p->c3;
  (*vv++)=(*c3++);
  (*vv++)=(*c3++);
  (*vv++)=(*c3++);

  vv=p->n1;
  (*vv++)=(*n1++);
  (*vv++)=(*n1++);
  (*vv++)=(*n1++);
  vv=p->n2;
  (*vv++)=(*n2++);
  (*vv++)=(*n2++);
  (*vv++)=(*n2++);
  vv=p->n3;
  (*vv++)=(*n3++);
  (*vv++)=(*n3++);
  (*vv++)=(*n3++);


  if(I->TTTFlag) {
    transformTTT44f3f(I->TTT,p->v1,p->v1);
    transformTTT44f3f(I->TTT,p->v2,p->v2);
    transformTTT44f3f(I->TTT,p->v3,p->v3);
    transform_normalTTT44f3f(I->TTT,p->n0,p->n0);
    transform_normalTTT44f3f(I->TTT,p->n1,p->n1);
    transform_normalTTT44f3f(I->TTT,p->n2,p->n2);
    transform_normalTTT44f3f(I->TTT,p->n3,p->n3);
  }

  if(I->Context) {
    RayApplyContextToVertex(I,p->v1);
    RayApplyContextToVertex(I,p->v2);
    RayApplyContextToVertex(I,p->v3);
    RayApplyContextToNormal(I,p->n0);
    RayApplyContextToNormal(I,p->n1);
    RayApplyContextToNormal(I,p->n2);
    RayApplyContextToNormal(I,p->n3);
  }

  I->NPrimitive++;

}
/*========================================================================*/
CRay *RayNew(PyMOLGlobals *G)
{
  unsigned int test;
  unsigned char *testPtr;
  int a;

  OOAlloc(I->G,CRay);
  I->G = G;
  test = 0xFF000000;
  testPtr = (unsigned char*)&test;
  I->BigEndian = (*testPtr)&&1;
  I->Trans=0.0F;
  I->Wobble=0;
  I->TTTFlag=false;
  zero3f(I->WobbleParam);
  PRINTFB(I->G,FB_Ray,FB_Blather) 
    " RayNew: BigEndian = %d\n",I->BigEndian
    ENDFB(I->G);

  I->Basis=CacheAlloc(I->G,CBasis,3,0,cCache_ray_basis);
  BasisInit(I->G,I->Basis,0);
  BasisInit(I->G,I->Basis+1,1);
  I->Vert2Prim=VLACacheAlloc(I->G,int,1,0,cCache_ray_vert2prim);
  I->NBasis=2;
  I->Primitive=NULL;
  I->NPrimitive=0;
  I->fColor3fv=RayColor3fv;
  I->fSphere3fv=RaySphere3fv;
  I->fCylinder3fv=RayCylinder3fv;
  I->fCustomCylinder3fv=RayCustomCylinder3fv;
  I->fSausage3fv=RaySausage3fv;
  I->fTriangle3fv=RayTriangle3fv;
  I->fCharacter=RayCharacter;
  I->fWobble=RayWobble;
  I->fTransparentf=RayTransparentf;
  for(a=0;a<256;a++) {
    I->Random[a]=(float)((rand()/(1.0+RAND_MAX))-0.5);
  }

  I->Wobble = SettingGet_i(I->G,NULL,NULL,cSetting_ray_texture);
  {
    float *v = SettingGet_3fv(I->G,NULL,NULL,cSetting_ray_texture_settings);
    copy3f(v,I->WobbleParam);
  }

  return(I);
}
/*========================================================================*/
void RayPrepare(CRay *I,float v0,float v1,float v2,
                float v3,float v4,float v5,
                float *mat,float *rotMat,float aspRat,
                int width, float pixel_scale,int ortho,float pixel_ratio)
	  /*prepare for vertex calls */
{
  int a;
  if(!I->Primitive) 
	 I->Primitive=VLACacheAlloc(I->G,CPrimitive,10000,3,cCache_ray_primitive);  
  if(!I->Vert2Prim) 
	 I->Vert2Prim=VLACacheAlloc(I->G,int,10000,3,cCache_ray_vert2prim);
  I->Volume[0]=v0;
  I->Volume[1]=v1;
  I->Volume[2]=v2;
  I->Volume[3]=v3;
  I->Volume[4]=v4;
  I->Volume[5]=v5;
  I->Range[0]=I->Volume[1]-I->Volume[0];
  I->Range[1]=I->Volume[3]-I->Volume[2];
  I->Range[2]=I->Volume[5]-I->Volume[4];
  I->AspRatio=aspRat;
  CharacterSetRetention(I->G,true);

  if(mat)  
    for(a=0;a<16;a++)
      I->ModelView[a]=mat[a];
  else {
    for(a=0;a<16;a++)
      I->ModelView[a]=0.0F;
    for(a=0;a<3;a++)
      I->ModelView[a*5]=1.0F;
  }
  if(rotMat)  
    for(a=0;a<16;a++)
      I->Rotation[a]=rotMat[a];
  if(ortho) {
    I->PixelRadius = (((float)I->Range[0])/width)*pixel_scale;
  } else {
    I->PixelRadius = (((float)I->Range[0])/width)*pixel_scale*pixel_ratio;
  }
}
/*========================================================================*/

void RaySetTTT(CRay *I,int flag,float *ttt)
{
  I->TTTFlag=flag;
  if(flag) {
    UtilCopyMem(I->TTT,ttt,sizeof(float)*16);
  }
}

/*========================================================================*/
void RayRelease(CRay *I)
{
  int a;

  for(a=0;a<I->NBasis;a++) {
	 BasisFinish(&I->Basis[a],a);
  }
  I->NBasis=0;
  VLACacheFreeP(I->G,I->Primitive,0,cCache_ray_primitive,false);
  VLACacheFreeP(I->G,I->Vert2Prim,0,cCache_ray_vert2prim,false);
}
/*========================================================================*/
void RayFree(CRay *I)
{
  RayRelease(I);
  CharacterSetRetention(I->G,false);
  CacheFreeP(I->G,I->Basis,0,cCache_ray_basis,false);
  VLACacheFreeP(I->G,I->Vert2Prim,0,cCache_ray_vert2prim,false);
  OOFreeP(I);
}
/*========================================================================*/

void RayApplyMatrix33( unsigned int n, float3 *q, const float m[16],
                          float3 *p )
{
   {
      unsigned int i;
      float m0 = m[0],  m4 = m[4],  m8 = m[8],  m12 = m[12];
      float m1 = m[1],  m5 = m[5],  m9 = m[9],  m13 = m[13];
      float m2 = m[2],  m6 = m[6],  m10 = m[10],  m14 = m[14];
      for (i=0;i<n;i++) {
         float p0 = p[i][0], p1 = p[i][1], p2 = p[i][2];
         q[i][0] = m0 * p0 + m4  * p1 + m8 * p2 + m12;
         q[i][1] = m1 * p0 + m5  * p1 + m9 * p2 + m13;
         q[i][2] = m2 * p0 + m6 * p1 + m10 * p2 + m14;
      }
   }
}

void RayApplyMatrixInverse33( unsigned int n, float3 *q, const float m[16],
                          float3 *p )
{
   {
      unsigned int i;
      float m0 = m[0],  m4 = m[4],  m8 = m[8],  m12 = m[12];
      float m1 = m[1],  m5 = m[5],  m9 = m[9],  m13 = m[13];
      float m2 = m[2],  m6 = m[6],  m10 = m[10],  m14 = m[14];
      for (i=0;i<n;i++) {
         float p0 = p[i][0]-m12, p1 = p[i][1]-m13, p2 = p[i][2]-m14;
         q[i][0] = m0 * p0 + m1  * p1 + m2 * p2;
         q[i][1] = m4 * p0 + m5  * p1 + m6 * p2;
         q[i][2] = m8 * p0 + m9 * p1 + m10 * p2;
      }
   }
}

void RayTransformNormals33( unsigned int n, float3 *q, const float m[16],float3 *p )
{
  unsigned int i;
  float m0 = m[0],  m4 = m[4],  m8 = m[8];
  float m1 = m[1],  m5 = m[5],  m9 = m[9];
  float m2 = m[2],  m6 = m[6],  m10 = m[10];
  for (i=0;i<n;i++) {
    float p0 = p[i][0], p1 = p[i][1], p2 = p[i][2];
    q[i][0] = m0 * p0 + m4  * p1 + m8 * p2;
    q[i][1] = m1 * p0 + m5  * p1 + m9 * p2;
    q[i][2] = m2 * p0 + m6 * p1 + m10 * p2;
  }
  for (i=0;i<n;i++) { /* renormalize - can we do this to the matrix instead? */
    normalize3f(q[i]);
  }
}

void RayTransformInverseNormals33( unsigned int n, float3 *q, const float m[16],float3 *p )
{
  unsigned int i;
  float m0 = m[0],  m4 = m[4],  m8 = m[8];
  float m1 = m[1],  m5 = m[5],  m9 = m[9];
  float m2 = m[2],  m6 = m[6],  m10 = m[10];
  for (i=0;i<n;i++) {
    float p0 = p[i][0], p1 = p[i][1], p2 = p[i][2];
    q[i][0] = m0 * p0 + m1  * p1 + m2 * p2;
    q[i][1] = m4 * p0 + m5  * p1 + m6 * p2;
    q[i][2] = m8 * p0 + m9 * p1 + m10 * p2;
  }
  for (i=0;i<n;i++) { /* renormalize - can we do this to the matrix instead? */
    normalize3f(q[i]);
  }
}


