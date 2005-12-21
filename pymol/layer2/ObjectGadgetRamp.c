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

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"

#include"OOMac.h"
#include"ObjectGadgetRamp.h"
#include"GadgetSet.h"
#include"Base.h"
#include"MemoryDebug.h"
#include"CGO.h"
#include"Scene.h"
#include"Setting.h"
#include"PConv.h"
#include"main.h"
#include"Color.h"
#include"VFont.h"
#include"ObjectMolecule.h"
#include"Executive.h"
#include"Util.h"

#include"P.h"

void ObjectGadgetRampFree(ObjectGadgetRamp *I) {
  ColorForgetExt(I->Gadget.Obj.G,I->Gadget.Obj.Name);
  VLAFreeP(I->Level);
  VLAFreeP(I->Color);
  ObjectGadgetPurge(&I->Gadget);
  OOFreeP(I);
}

#define ShapeVertex(cgo,a,b) CGOVertex(cgo,(float)a,(float)b,0.0F)
#define ShapeFVertex(cgo,a,b) CGOFontVertex(cgo,(float)a,(float)b,0.0F)
#define ABS 0.0F
#define REL 1.0F
#define OFF 2.0F

#define ShapeNormal(cgo,a,b) CGONormal(cgo,(float)a,(float)b,0.0F)
#define ShapeColor(cgo,a,b) CGONormal(cgo,(float)a,(float)b,0.0F)
#define LKP 2.0F


PyObject *ObjectGadgetRampAsPyList(ObjectGadgetRamp *I)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else

  PyObject *result = NULL;

  result = PyList_New(9);

  PyList_SetItem(result,0,ObjectGadgetPlainAsPyList(&I->Gadget));
  PyList_SetItem(result,1,PyInt_FromLong(I->RampType));
  PyList_SetItem(result,2,PyInt_FromLong(I->NLevel));
  if(I->Level&&I->NLevel) {
    PyList_SetItem(result,3,PConvFloatVLAToPyList(I->Level));
  } else {
    PyList_SetItem(result,3,PConvAutoNone(NULL));
  }
  if(I->Color&&I->NLevel) {
    PyList_SetItem(result,4,PConvFloatVLAToPyList(I->Color));
  } else {
    PyList_SetItem(result,4,PConvAutoNone(NULL));
  }
  PyList_SetItem(result,5,PyInt_FromLong(I->var_index));
  PyList_SetItem(result,6,PyString_FromString(I->SrcName));
  PyList_SetItem(result,7,PyInt_FromLong(I->SrcState));
  PyList_SetItem(result,8,PyInt_FromLong(I->CalcMode));

  return(PConvAutoNone(result));  
#endif
}

int ObjectGadgetRampNewFromPyList(PyMOLGlobals *G,PyObject *list,ObjectGadgetRamp **result,int version)
{
#ifdef _PYMOL_NOPY
  return 0;
#else

  ObjectGadgetRamp *I = NULL;
  int ok = true;
  int ll = 0;

  if(ok) I=ObjectGadgetRampNew(G);
  if(ok) ok = (I!=NULL);
  if(ok) ok = (list!=NULL);
  if(ok) ok = PyList_Check(list);
  if(ok) ll = PyList_Size(list);
  /* TO SUPPORT BACKWARDS COMPATIBILITY...
   Always check ll when adding new PyList_GetItem's */

  if(ok) ok = ObjectGadgetInitFromPyList(G,PyList_GetItem(list,0),&I->Gadget,version);
  if(ok) ok = PConvPyIntToInt(PyList_GetItem(list,1),&I->RampType);
  if(ok) ok = PConvPyIntToInt(PyList_GetItem(list,2),&I->NLevel);
  if(ok&&I->NLevel) ok = PConvPyListToFloatVLA(PyList_GetItem(list,3),&I->Level);
  if(ok&&I->NLevel) {
    PyObject *item = PyList_GetItem(list,4);
    if(item!=Py_None) {
      ok = PConvPyListToFloatVLA(item,&I->Color);
    }
  }
  if(ok) ok = PConvPyIntToInt(PyList_GetItem(list,5),&I->var_index);
  if(ok) ok = PConvPyStrToStr(PyList_GetItem(list,6),I->SrcName,ObjNameMax);
  if(ok) ok = PConvPyIntToInt(PyList_GetItem(list,7),&I->SrcState);
  if(ok&&(ll>8)) ok=PConvPyIntToInt(PyList_GetItem(list,8),&I->CalcMode);
  /*  if(ok) ObjectGadgetRampBuild(I);
      if(ok) ObjectGadgetRampUpdate(I);*/
  if(ok) ObjectGadgetUpdateStates(&I->Gadget);
  if(ok) ObjectGadgetUpdateExtents(&I->Gadget);
  if(ok) (*result)=I;
  return(ok);
#endif
}

int ObjectGadgetRampInterVertex(ObjectGadgetRamp *I,float *pos,float *color)
{
  float level;
  int ok=true;
  switch(I->RampType) {
  case cRampMap:
    if(!I->Map)
      I->Map = ExecutiveFindObjectMapByName(I->Gadget.Obj.G,I->SrcName);
    if(!ExecutiveValidateObjectPtr(I->Gadget.Obj.G,(CObject*)I->Map,cObjectMap))
      ok=false;
    else {
      if(ok) ok = (I->Map!=NULL);
      if(ok) ok = ObjectMapInterpolate(I->Map,I->SrcState,pos,&level,NULL,1);
      if(ok) ok = ObjectGadgetRampInterpolate(I,level,color);
    }
    break;
  case cRampMol:
    if(!I->Mol)
      I->Mol = ExecutiveFindObjectMoleculeByName(I->Gadget.Obj.G,I->SrcName);
    
    if(!ExecutiveValidateObjectPtr(I->Gadget.Obj.G,(CObject*)I->Mol,cObjectMolecule))
      ok=false;
    else  {
      float cutoff = 1.0F;
      int state = SceneGetState(I->Gadget.Obj.G);
      if(I->Level&&I->NLevel)
        cutoff = I->Level[I->NLevel-1];
      if(ok) ok = (I->Mol!=NULL);      
      if(ok) {
        int index = ObjectMoleculeGetNearestAtomIndex(I->Mol, pos, cutoff, state);
        if(index>=0) {
          float *rgb =  ColorGet(I->Gadget.Obj.G,I->Mol->AtomInfo[index].color);
          copy3f(rgb,color);
        } else {
          if(I->Color) { 
            copy3f(I->Color,color);
          }
        }
      }
    }
    break;
  default:
    ok=false;
    break;
  }
  return(ok);
}


static void ObjectGadgetRampCalculate(ObjectGadgetRamp *I, float v,float *result)
{
  int i;
  const float _1 = 1.0F;
  const float _0 = 0.0F;
  /* from Filipe Maia */
  
  /* All of this functions are taken right of the gnuplot manual  */
  if(v>_1)
    v=_1;
  else if(v<_0)
    v=_0;

  switch(I->CalcMode){
  case cRAMP_TRADITIONAL:
    result[0] = (float)sqrt(v);
    result[1] = v*v*v;
    result[2] = (float)sin(v*2*cPI);
    break;
  case cRAMP_SLUDGE:
    result[0] = v;
    result[1] = (float)fabs(v-0.5F);
    result[2] = v*v*v*v;
    break;
  case cRAMP_OCEAN:
    result[0] = 3*v-2;
    result[1] = (float)fabs((3*v-1)/2);
    result[2] = v;
    break;
  case cRAMP_HOT:
    result[0] = 3*v;
    result[1] = 3*v-1;
    result[2] = 3*v-2;
    break;
  case cRAMP_GRAYABLE:
    result[0] = v/0.32F-0.78125F; 
    result[1] = 2*v-0.84F;
    result[2] = v/0.08F-11.5F; /* I'm not so sure about this one */
    break;
  case cRAMP_RAINBOW:
    result[0] = (float)fabs(2*v - 0.5F);
    result[1] = (float)sin(v*cPI);
    result[2] = (float)cos(v*cPI/2.0F);
    break;
  case cRAMP_AFMHOT:
    result[0] = 2*v;
    result[1] = 2*v-0.5F;
    result[2] = 2*v-1.0F;
    break;
  case cRAMP_GRAYSCALE:
    result[0] = v;
    result[1] = v;
    result[2] = v;
    break;
  default: /* default is simply white */
    result[0] = 1.0F;
    result[1] = 1.0F;
    result[2] = 1.0F;
    break;
  }  
  for(i = 0;i<3;i++) {
    if(result[i] > 1.0F){
      result[i] = 1.0F;
    } else if(result[i]<0.0F){
      result[i] = 0.0F;
    }
  }
}

int ObjectGadgetRampInterpolate(ObjectGadgetRamp *I,float level,float *color)
{
  int i=0;
  int ok=true;
  int below=0;
  int above=0;
  float d,x0,x1;
  int a;
  const float _0 = 0.0F;
  const float _1 = 1.0F;

  if(I->Level&&I->Color) {
    while(i<I->NLevel) {
      if(I->Level[i]>level) {
        above=i;
        break;
      } else {
        below=i;
        above=i;
      }
      i++;
    }
    if(above!=below) {
      d=I->Level[above]-I->Level[below];
      if(fabs(d)>R_SMALL8) {
        x0=(level-I->Level[below])/d;
        x1=1.0F-x0;
        for(a=0;a<3;a++) {
          color[a]=x0*I->Color[3*above+a]+x1*I->Color[3*below+a];
        }
        clamp3f(color);
      } else {
        copy3f(I->Color+3*above,color);
        clamp3f(color);
      }
    } else {
      copy3f(I->Color+3*above,color);
     clamp3f(color);
    }
  } else {
    float base,range;
    if(I->NLevel&&I->Level) {
      base=I->Level[0];
      range=I->Level[I->NLevel-1]-base;
      if(fabs(range)<R_SMALL8)
        range=_1;
    } else {
      base = _0;
      range = _1;
    }
    level = (level-base)/range;
    ObjectGadgetRampCalculate(I,level,color);
  }
  return(ok);
}

static void ObjectGadgetRampUpdateCGO(ObjectGadgetRamp *I,GadgetSet *gs)
{
  CGO *cgo;
  int n_extra;
  int a,c=0;
  float *p;
  char buffer[255];

  cgo = CGONewSized(I->Gadget.Obj.G,100);

  /* behind text */

  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  CGOColor(cgo,0.05F,0.05F,0.05F);
  ShapeNormal(cgo,LKP,2);
  ShapeVertex(cgo,REL,9);
  ShapeVertex(cgo,REL,10);
  ShapeVertex(cgo,REL,7);
  ShapeVertex(cgo,REL,8);
  CGOEnd(cgo);

  CGOColor(cgo,1.0F,1.0F,1.0F);
  CGOFontScale(cgo,I->text_scale_h,I->text_scale_v);


  if(I->Level&&I->NLevel) {
    sprintf(buffer,"%0.3f",I->Level[0]);
    ShapeFVertex(cgo,REL,11);
    CGOWrite(cgo,buffer);
    sprintf(buffer,"%0.3f",I->Level[I->NLevel-1]);
    ShapeFVertex(cgo,REL,12);
    CGOWriteLeft(cgo,buffer);
  }


  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeNormal(cgo,LKP,2);

  if(I->Color) {

    n_extra = 3*I->NLevel;
    
    if(I->NLevel<2) {
      /* TO DO */
    } else {
      VLACheck(gs->Coord,float,(I->var_index+n_extra)*3);      
      c = I->var_index;
      p=gs->Coord+3*c;
      for(a=0;a<I->NLevel;a++) {
        
        CGOColorv(cgo,I->Color+3*a);
        
        *(p++) = I->border + (I->width * a)/(I->NLevel-1);
        *(p++) = -I->border;
        *(p++) = I->border;
        ShapeVertex(cgo,REL,c);
        c++;
        
        *(p++) = I->border + (I->width * a)/(I->NLevel-1);
        *(p++) = -(I->border+I->bar_height);
        *(p++) = I->border;
        ShapeVertex(cgo,REL,c);
        c++;
        
        *(p++) = I->border + (I->width * a)/(I->NLevel-1);
        *(p++) = -(I->border+I->height+I->height);
        *(p++) = I->border;
        c++;
      }
    }
  } else {
    int samples=20;
    float fxn;
    float color[3];
    n_extra = 3*samples;
    VLACheck(gs->Coord,float,(I->var_index+n_extra)*3);      
    c = I->var_index;
    p=gs->Coord+3*c;

    for(a=0;a<samples;a++) {
      fxn = a/(samples-1.0F);

      ObjectGadgetRampCalculate(I,fxn,color);
      CGOColorv(cgo,color);
      
      *(p++) = I->border + (I->width * fxn);
      *(p++) = -I->border;
      *(p++) = I->border;
      ShapeVertex(cgo,REL,c);
      c++;
        
      *(p++) = I->border + (I->width * fxn);
      *(p++) = -(I->border+I->bar_height);
      *(p++) = I->border;
      ShapeVertex(cgo,REL,c);
      c++;
        
      *(p++) = I->border + (I->width * fxn);
      *(p++) = -(I->border+I->height+I->height);
      *(p++) = I->border;
      c++;
      
    }
  }
  gs->NCoord = c;
  /* center */
  CGOEnd(cgo);


  CGOColor(cgo,0.5F,0.5F,0.5F);

  /* top */
  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeNormal(cgo,LKP,2);
  ShapeVertex(cgo,REL,5);
  ShapeVertex(cgo,REL,6);
  ShapeNormal(cgo,LKP,1);
  ShapeVertex(cgo,REL,1);
  ShapeVertex(cgo,REL,2);
  CGOEnd(cgo);

  /* bottom */

  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeNormal(cgo,LKP,4);
  ShapeVertex(cgo,REL,3);
  ShapeVertex(cgo,REL,4);
  ShapeNormal(cgo,LKP,2);
  ShapeVertex(cgo,REL,7);
  ShapeVertex(cgo,REL,8);
  CGOEnd(cgo);

  /* left */

  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeNormal(cgo,LKP,3);
  ShapeVertex(cgo,REL,1);
  ShapeVertex(cgo,REL,3);
  ShapeNormal(cgo,LKP,2);
  ShapeVertex(cgo,REL,5);
  ShapeVertex(cgo,REL,7);
  CGOEnd(cgo);

  /* right */
  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeNormal(cgo,LKP,2);
  ShapeVertex(cgo,REL,6);
  ShapeVertex(cgo,REL,8);
  ShapeNormal(cgo,LKP,0);
  ShapeVertex(cgo,REL,2);
  ShapeVertex(cgo,REL,4);
  CGOEnd(cgo);


  CGOStop(cgo);

  CGOFree(gs->ShapeCGO);
  gs->ShapeCGO = cgo;

  CGOPreloadFonts(gs->ShapeCGO);
  
  cgo = CGONewSized(I->Gadget.Obj.G,100);
  CGODotwidth(cgo,5);
  CGOPickColor(cgo,0,cPickableGadget);

  /* top */
  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeVertex(cgo,REL,1);
  ShapeVertex(cgo,REL,2);
  ShapeVertex(cgo,REL,5);
  ShapeVertex(cgo,REL,6);

  CGOEnd(cgo);

  /* bottom */

  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeVertex(cgo,REL,3);
  ShapeVertex(cgo,REL,4);
  ShapeVertex(cgo,REL,7);
  ShapeVertex(cgo,REL,8);
  CGOEnd(cgo);

  /* left */

  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeVertex(cgo,REL,1);
  ShapeVertex(cgo,REL,3);
  ShapeVertex(cgo,REL,5);
  ShapeVertex(cgo,REL,7);
  CGOEnd(cgo);

  /* right */
  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeVertex(cgo,REL,6);
  ShapeVertex(cgo,REL,8);
  ShapeVertex(cgo,REL,2);
  ShapeVertex(cgo,REL,4);
  CGOEnd(cgo);

  /* band */

  CGOPickColor(cgo,13,cPickableGadget);
  CGOBegin(cgo,GL_TRIANGLE_STRIP);
  ShapeVertex(cgo,REL,5);
  ShapeVertex(cgo,REL,6);
  ShapeVertex(cgo,REL,7);
  ShapeVertex(cgo,REL,8);
  CGOEnd(cgo);
  
  CGOStop(cgo);

  CGOFree(gs->PickShapeCGO);
  gs->PickShapeCGO = cgo;
}

#ifndef _PYMOL_NOPY
static void ObjectGadgetRampBuild(ObjectGadgetRamp *I)
{
  GadgetSet *gs = NULL;
  ObjectGadget *og;
  int a;

  float coord[100];
  int ix=0;

  float normal[] = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0,
   -1.0, 0.0, 0.0,
    0.0,-1.0, 0.0,
  };
#define VV(a,b,c) {coord[ix++]=a;coord[ix++]=b;coord[ix++]=c;};

VV(    I->x ,  I->y , 0.3F );

    /* outer points */

VV(    0.0 ,  0.0 , 0.0 );
    VV(I->width+I->border*2 ,  0.0 , 0.0 );
VV(    0.0 , -(I->height+I->border*2) , 0.0 );
VV(    I->width+I->border*2 , -(I->height+I->border*2) , 0.0 );

VV(    I->border, -I->border, I->border);
VV(    I->width+I->border,-I->border, I->border);
VV(    I->border,  -(I->height+I->border), I->border);
VV(    I->width+I->border, -(I->height+I->border), I->border);

VV(    I->border, -(I->border+I->bar_height), I->border);
VV(    I->width+I->border,-(I->border+I->bar_height), I->border);
    
VV(    I->border+I->text_border, I->text_border-(I->border+I->height), I->border+I->text_raise);
VV(    I->width+I->border,I->text_border-(I->border+I->height), I->border+I->text_raise);

 VV(   0.0,0.0,0.0);
#undef VV

  OrthoBusyPrime(I->Gadget.Obj.G);

  og = &I->Gadget;
  gs = GadgetSetNew(I->Gadget.Obj.G);

  gs->NCoord = 14;
  I->var_index = gs->NCoord;
  gs->Coord = VLAlloc(float,gs->NCoord*3);
  for(a=0;a<gs->NCoord*3;a++) {
    gs->Coord[a]=coord[a];
  }

  gs->NNormal = 5;
  gs->Normal = VLAlloc(float,gs->NNormal*3);
  for(a=0;a<gs->NNormal*3;a++) {
    gs->Normal[a]=normal[a];
  }

  og->GSet[0] = gs;
  og->NGSet = 1;
  og->Obj.Context=1; /* unit window */
  gs->Obj = (ObjectGadget*)I;
  gs->State = 0;

  ObjectGadgetRampUpdateCGO(I,gs);
  gs->fUpdate(gs);
  
}
#endif

/*========================================================================*/
void ObjectGadgetRampUpdate(ObjectGadgetRamp *I)
{
  float scale;

  if(I->Gadget.Changed) {
    scale = (1.0F+5*I->Gadget.GSet[0]->Coord[13*3]);
    
    I->Gadget.GSet[0]->Coord[13*3] = 0.0;
    if(I->NLevel==2) {
      float mean = (I->Level[0]+I->Level[1])/2.0F;
      I->Level[0]=(I->Level[0]-mean)*scale+mean;
      I->Level[2]=(I->Level[1]-mean)*scale+mean;
      ExecutiveInvalidateRep(I->Gadget.Obj.G,cKeywordAll,cRepAll,cRepInvColor);
    } else if(I->NLevel==3) {
      I->Level[0]=(I->Level[0]-I->Level[1])*scale+I->Level[1];
      I->Level[2]=(I->Level[2]-I->Level[1])*scale+I->Level[1];
      ExecutiveInvalidateRep(I->Gadget.Obj.G,cKeywordAll,cRepAll,cRepInvColor);
    }
    if(I->Gadget.NGSet)
      if(I->Gadget.GSet[0]) {
        ObjectGadgetRampUpdateCGO(I,I->Gadget.GSet[0]);
        ObjectGadgetUpdateStates(&I->Gadget);
      }
    ObjectGadgetUpdateExtents(&I->Gadget);
    I->Gadget.Changed=false;
    SceneChanged(I->Gadget.Obj.G);
  }
}


/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampMapNewAsDefined(PyMOLGlobals *G,
                                                  ObjectMap *map,
                                                  PyObject *level,
                                                  PyObject *color,int map_state,
                                                  float *vert_vla,float beyond,float within,
                                                  float sigma,int zero)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else

  ObjectGadgetRamp *I;
  int ok = true;

  I = ObjectGadgetRampNew(G);
  I->RampType = cRampMap;

  PBlock();
  if(ok) {
    if(PyList_Check(color))
      ok = PConvPyList3ToFloatVLA(color,&I->Color);
    else if(PyInt_Check(color)) {
      ok = PConvPyIntToInt(color,&I->CalcMode);      
    }
  }
  if(ok) {     
    ObjectMapState *ms;
    float tmp_level[3];
    if(vert_vla && 
       (ms = ObjectMapGetState(map,map_state)) &&
       ObjectMapStateGetExcludedStats(G,ms,vert_vla,beyond,within,tmp_level)) {
      tmp_level[0]=tmp_level[1]+(tmp_level[0]-tmp_level[1])*sigma;
      tmp_level[2]=tmp_level[1]+(tmp_level[2]-tmp_level[1])*sigma;
      if(zero) {
        if(tmp_level[1]<0.0F) {
          tmp_level[1]=0.0F;
          tmp_level[2]=-tmp_level[0];
        } else if(tmp_level[1]>0.0F) {
          tmp_level[1]=0.0F;
          tmp_level[0]=-tmp_level[2];
        }
      }
      I->Level = VLAlloc(float,3);
      copy3f(tmp_level,I->Level);
    } else  {
      ok = PConvPyListToFloatVLA(level,&I->Level);
    }
  }
  if(ok) I->NLevel=VLAGetSize(I->Level);
  ObjectGadgetRampBuild(I);
  UtilNCopy(I->SrcName,map->Obj.Name,ObjNameMax);
  I->SrcState=map_state;
  
  /* test interpolate 
     { 
    float test[3];

    ObjectGadgetRampInterpolate(I,-2.0,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,-1.0,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,-0.9,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,-0.5,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,0.0,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,0.5,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,1.0,test);
    dump3f(test,"test color");
    ObjectGadgetRampInterpolate(I,2.0,test);
    dump3f(test,"test color");
  }
  */

  PUnblock();
  return(I);
#endif
}

/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampMolNewAsDefined(PyMOLGlobals *G,ObjectMolecule *mol,
                                                  PyObject *level,
                                                  PyObject *color,
                                                  int mol_state)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else

  ObjectGadgetRamp *I;
  int ok = true;

  I = ObjectGadgetRampNew(G);
  I->RampType = cRampMol;
  PBlock();
  if(ok) {
    if(PyList_Check(color))
      ok = PConvPyList3ToFloatVLA(color,&I->Color);
    else if(PyInt_Check(color)) {
      ok = PConvPyIntToInt(color,&I->CalcMode);      
    }
  }
  if(ok) {     
    ok = PConvPyListToFloatVLA(level,&I->Level);
  }
  if(ok) I->NLevel=VLAGetSize(I->Level);

  ObjectGadgetRampBuild(I);
  UtilNCopy(I->SrcName,mol->Obj.Name,ObjNameMax);
  I->SrcState=mol_state;
  
  PUnblock();
  return(I);
#endif
}


static void ObjectGadgetRampInvalidate(ObjectGadgetRamp *I,int rep,int level,int state)
{
  I->Gadget.Changed=true;
}

/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampNew(PyMOLGlobals *G)
{
  OOAlloc(G,ObjectGadgetRamp);

  ObjectGadgetInit(G,&I->Gadget);
  I->Gadget.GadgetType = cGadgetRamp;
  I->RampType = 0;
  I->NLevel = 0;
  I->Level = NULL;
  I->Color = NULL;
  I->SrcName[0] = 0;

  I->Gadget.Obj.fUpdate =(void (*)(struct CObject *)) ObjectGadgetRampUpdate;
  I->Gadget.Obj.fFree =(void (*)(struct CObject *)) ObjectGadgetRampFree;
  I->Gadget.Obj.fInvalidate = (void (*)(struct CObject *,int,int,int)) ObjectGadgetRampInvalidate;

  I->Mol = NULL;
  I->Map = NULL;
  I->width = 0.9F;
  I->height = 0.06F;
  I->bar_height = 0.03F;
  I->text_raise = 0.003F;
  I->text_border = 0.004F;
  I->text_scale_h = 0.04F;
  I->text_scale_v = 0.02F;
  I->border = 0.018F;
  I->var_index = 0;
  I->x = (1.0F-(I->width+2*I->border))/2.0F;
  I->y = 0.12F;
  I->Map = NULL;
  I->CalcMode = 0;
  return(I);
}
