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

#include"Base.h"
#include"OOMac.h"
#include"MemoryDebug.h"
#include"Ortho.h"
#include"Word.h"

#include"Color.h"
#include"PConv.h"
#include"ObjectGadgetRamp.h"
#include"Util.h"
#include"Executive.h"
#include"MyPNG.h"
#include"Scene.h"

static int AutoColor[] = { 
  26, 5, 154, 6, 9, 29, 11, 30,
  10, 5262, 12, 36, 5271, 124, 17, 18,
  5270, 20, 5272, 52, 5258, 5274, 5257, 5256,
  15, 5277, 5279, 5276, 53, 5278, 5275,5269,
  22, 5266, 5280, 5267, 5268, 104, 23,  51,
};

static int nAutoColor = 40;

#define cColorExtCutoff (-10)

int ColorGetNext(PyMOLGlobals *G) 
{
  int result;
  int next;
  next = (int)SettingGet(G,cSetting_auto_color_next);

  if(next>=nAutoColor) next = 0;
  result = AutoColor[next];
  next++;
  if(next>=nAutoColor) next = 0;
  SettingSet(G,cSetting_auto_color_next,(float)next);
  return(result);
}

int ColorGetCurrent(PyMOLGlobals *G)
{
  int result;
  int next;
  next = (int)SettingGet(G,cSetting_auto_color_next);
  next--;
  if(next<0)
    next = (nAutoColor-1);
  result = AutoColor[next];
  return(result);
}

int ColorCheckRamped(PyMOLGlobals *G,int index)
{
  return(index<=(cColorExtCutoff));
}

ObjectGadgetRamp* ColorGetRamp(PyMOLGlobals *G,int index)
{
  register CColor *I=G->Color;
  ObjectGadgetRamp *result = NULL;
  if(index<=cColorExtCutoff) {
    index = cColorExtCutoff - index;
    if(index<I->NExt) {
      if(!I->Ext[index].Ptr) {
        if(I->Ext[index].Name)
          I->Ext[index].Ptr = (void*)ExecutiveFindObjectByName(G,I->Ext[index].Name);
      }
      if(I->Ext[index].Ptr) 
        result = (ObjectGadgetRamp*)I->Ext[index].Ptr;
    }
  }
  return result;
}

int ColorGetRamped(PyMOLGlobals *G,int index,float *vertex,float *color)
{
  register CColor *I=G->Color;
  int ok=false;
  if(index<=cColorExtCutoff) {
    index = cColorExtCutoff - index;
    if(index<I->NExt) {
      if(!I->Ext[index].Ptr) {
        if(I->Ext[index].Name)
          I->Ext[index].Ptr = (void*)ExecutiveFindObjectByName(G,I->Ext[index].Name);
      }
      if(I->Ext[index].Ptr) 
        ok = ObjectGadgetRampInterVertex(
                                         (ObjectGadgetRamp*)I->Ext[index].Ptr,
                                         vertex,color);
    }
  
  }
  if(!ok) {
    color[0]=1.0;
    color[1]=1.0;
    color[2]=1.0;
  }
  return(ok);
}

static int ColorFindExtByName(PyMOLGlobals *G,char *name,int null_okay,int *best) 
{
  register CColor *I=G->Color;
  int result = -1;
  int wm;
  int a;
  int mybest;
  if(!best)
    best=&mybest;
  *best = 0;
  for(a=0;a<I->NExt;a++)
    {
      wm = WordMatch(G,name,I->Ext[a].Name,true);
      if(wm<0) {
        if(null_okay||(I->Ext[a].Ptr)) {
          result=a;
          *best=0;
          break;
        }
      } else if ((wm>0)&&((*best)<wm)) {
        if(null_okay||(I->Ext[a].Ptr)) {
          result=a;
          *best=wm;
        }
      }
    }
  return(result);
}

void ColorRegisterExt(PyMOLGlobals *G,char *name,void *ptr,int type)
{
  register CColor *I=G->Color;
  int a;

  a=ColorFindExtByName(G,name,true,NULL);
  if(a<0) {
    VLACheck(I->Ext,ExtRec,I->NExt);
    a = I->NExt;
    I->NExt++;
  }
  if(a>=0) {
    UtilNCopy(I->Ext[a].Name,name,sizeof(ColorName));
    I->Ext[a].Ptr = ptr;
    I->Ext[a].Type = type;
  }
}

void ColorForgetExt(PyMOLGlobals *G,char *name)
{
  register CColor *I=G->Color;
  int a;
  a=ColorFindExtByName(G,name,true,NULL);

  if(a>=0) { /* currently leaks memory TODO fix */
    I->Ext[a].Ptr=NULL;
  }
}

PyObject *ColorExtAsPyList(PyMOLGlobals *G)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  register CColor *I=G->Color;
  PyObject *result,*list;
  ExtRec *ext;
  int a;

  result = PyList_New(I->NExt);
  ext=I->Ext;
  for(a=0;a<I->NExt;a++) {
    list = PyList_New(2);
    PyList_SetItem(list,0,PyString_FromString(ext->Name));
    PyList_SetItem(list,1,PyInt_FromLong(ext->Type));
    PyList_SetItem(result,a,list);
    ext++;
  }
  return(result);
#endif
}

/*========================================================================*/
PyObject *ColorAsPyList(PyMOLGlobals *G)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  register CColor *I=G->Color;
  PyObject *result,*list;
  ColorRec *color;
  int n_custom=0;
  int a,c;
  color=I->Color;
  for(a=0;a<I->NColor;a++) {
    if(color->Custom||color->ClampedFlag)
      n_custom++;
    color++;
  }
  result = PyList_New(n_custom);
  c=0;
  color=I->Color;
  for(a=0;a<I->NColor;a++) {
    if(color->Custom||color->ClampedFlag) {
      list = PyList_New(6);
      PyList_SetItem(list,0,PyString_FromString(color->Name));
      PyList_SetItem(list,1,PyInt_FromLong(a));
      PyList_SetItem(list,2,PConvFloatArrayToPyList(color->Color,3));
      PyList_SetItem(list,3,PyInt_FromLong(color->Custom));
      PyList_SetItem(list,4,PyInt_FromLong(color->ClampedFlag));
      PyList_SetItem(list,5,PConvFloatArrayToPyList(color->Clamped,3));
      PyList_SetItem(result,c,list);
      c++;
    }
    color++;
  }
  return(result);
#endif
}

int ColorExtFromPyList(PyMOLGlobals *G,PyObject *list)
{
#ifdef _PYMOL_NOPY
  return 0;
#else
  int n_ext=0;
  int a;
  int ok=true;
  int ll;
  register CColor *I=G->Color;
  PyObject *rec;
  ExtRec *ext;
  if(ok) ok=(list!=NULL);
  if(ok) ok=PyList_Check(list);

  if(ok) ll = PyList_Size(list);
  /* TO SUPPORT BACKWARDS COMPATIBILITY...
   Always check ll when adding new PyList_GetItem's */

  if(ok) {
    n_ext=PyList_Size(list);
    VLACheck(I->Ext,ExtRec,n_ext); 
    ext=I->Ext;
    for(a=0;a<n_ext;a++) {
      rec=PyList_GetItem(list,a);
      if(ok) ok=(rec!=NULL);
      if(ok) ok=PyList_Check(rec);
      if(ok) ok=PConvPyStrToStr(PyList_GetItem(rec,0),ext->Name,sizeof(ColorName));
      if(ok) ok=PConvPyIntToInt(PyList_GetItem(rec,1),&ext->Type);
      ext++;
    }
    if(ok) I->NExt=n_ext;

  }
  return(ok);
#endif
}

/*========================================================================*/
int ColorFromPyList(PyMOLGlobals *G,PyObject *list)
{
#ifdef _PYMOL_NOPY
  return 0;
#else

  int n_custom=0;
  int a;
  int index=0;
  int ok=true;
  int ll;
  register CColor *I=G->Color;
  PyObject *rec;
  ColorRec *color;
  if(ok) ok=(list!=NULL);
  if(ok) ok=PyList_Check(list);
  if(ok) {
    n_custom=PyList_Size(list);
    for(a=0;a<n_custom;a++) {
      rec=PyList_GetItem(list,a);
      if(ok) ok=(rec!=NULL);
      if(ok) ok=PyList_Check(rec);
      if(ok) ll = PyList_Size(list);
      /* TO SUPPORT BACKWARDS COMPATIBILITY...
         Always check ll when adding new PyList_GetItem's */
      if(ok) ok=PConvPyIntToInt(PyList_GetItem(rec,1),&index);
      if(ok) {
        if(index>=I->NColor) {
          VLACheck(I->Color,ColorRec,index); /* auto-zeros */
          I->NColor=index+1;
        }
        color=I->Color+index;
        if(ok) ok=PConvPyStrToStr(PyList_GetItem(rec,0),color->Name,sizeof(ColorName));
        if(ok) ok=PConvPyListToFloatArrayInPlace(PyList_GetItem(rec,2),color->Color,3);
        if(PyList_Size(rec)>=6) {
          if(ok) ok=PConvPyIntToInt(PyList_GetItem(rec,3),&color->Custom);
          if(ok) ok=PConvPyIntToInt(PyList_GetItem(rec,4),&color->ClampedFlag);
          if(ok) ok=PConvPyListToFloatArrayInPlace(PyList_GetItem(rec,5),color->Clamped,3);
        } else {
          if(ok) {color->Custom=true;}
        }
      }
      if(!ok) break;
    }
  }
  return(ok);
#endif
}

/*========================================================================*/
void ColorDef(PyMOLGlobals *G,char *name,float *v)
{
  register CColor *I=G->Color;
  int color=-1;
  int a;
  int best;
  int wm;

  best = 0;
  for(a=0;a<I->NColor;a++)
	 {
      wm = WordMatch(G,name,I->Color[a].Name,true);
      if(wm<0) {
        color=a;
        break;
      }
	 }
  if(color<0) {
    color=I->NColor;
    VLACheck(I->Color,ColorRec,I->NColor);
    I->NColor++;
  }
  strcpy(I->Color[color].Name,name);
  I->Color[color].Color[0]=v[0];
  I->Color[color].Color[1]=v[1];
  I->Color[color].Color[2]=v[2];
  I->Color[color].Custom=true;
  ColorUpdateClamp(G,color);

  PRINTFB(G,FB_Executive,FB_Actions)
    " Color: \"%s\" defined as [ %3.3f, %3.3f, %3.3f ].\n",name,v[0],v[1],v[2] 
    ENDFB(G);
  PRINTFD(G,FB_Color) 
    " Color: and assigned number %d.\n",color
    ENDFD;
}
/*========================================================================*/
int ColorGetIndex(PyMOLGlobals *G,char *name)
{
  register CColor *I=G->Color;
  int color=-1; /* default for unknown is white */
  int ext_color;
  int a;
  int i;
  int wm,best=0;
  int ext_best=0;
  int is_numeric = 1;

  {
    char *c;
    c=name;
    while(*c) {
      if(!(((*c>='0')&&(*c<='9'))||(*c=='-'))) {
        is_numeric=false;
        break;
      }
      c++;
    }
  }
  
  if(is_numeric&&(((name[0]>='0')&&(name[0]<='9'))||(name[0]=='-')))
    if(sscanf(name,"%d",&i)) 
      if((i<I->NColor)&&(i>=0))
        return(i);
  if(WordMatch(G,name,"default",true))
    return(-1);
  if(WordMatch(G,name,"auto",true))
    return(ColorGetNext(G));
  for(a=0;a<I->NColor;a++)
	 {
      wm = WordMatch(G,name,I->Color[a].Name,true);
      if(wm<0) {
        color=a;
        best=0;
        break;
      } else if ((wm>0)&&(best<wm)) {
        color=a;
        best=wm;
      }
	 }
  if(best||(color<0)) {
    ext_color = ColorFindExtByName(G,name,false,&ext_best);
    if(ext_color>=0) {
      ext_color = -10-ext_color; /* indicates external */
      if((!ext_best)||(ext_best>best)) /* perfect or better match? */
        color = ext_color;
    }
  }
  return(color);
}
/*========================================================================*/
float *ColorGetNamed(PyMOLGlobals *G,char *name)
{
  return(ColorGet(G,ColorGetIndex(G,name)));
}
/*========================================================================*/
char *ColorGetName(PyMOLGlobals *G,int index)
{
  register CColor *I=G->Color;
  if((index>=0)&&(index<I->NColor))
    return(I->Color[index].Name);
  else
    return(NULL);
}
/*========================================================================*/
int ColorGetStatus(PyMOLGlobals *G,int index)
{
  register CColor *I=G->Color; /* return 0 if color is invalid, -1 if hidden; 
                       1 otherwise */
  char *c;
  int result=0;
  if((index>=0)&&(index<I->NColor)) {
    c=I->Color[index].Name;
    result=1;
    while(*c) {
      if(((*c)>='0')&&((*c)<='9')) {
        result=-1;
        break;
      }
      c++;
    }
  }
  return(result);
}
/*========================================================================*/
int ColorGetNColor(PyMOLGlobals *G)
{
  register CColor *I=G->Color;
  return(I->NColor);
}
/*========================================================================*/
void ColorFree(PyMOLGlobals *G)
{
  register CColor *I = G->Color;
  if(I->ColorTable) {
    FreeP(I->ColorTable);
  }
  VLAFreeP(I->Color);
  VLAFreeP(I->Ext);
  FreeP(I);
}

/*========================================================================*/
void ColorReset(PyMOLGlobals *G)
{
/* PyMOL core color names

  1   1   1   white
 .5  .5  .5   grey/gray
  0   0   0   black 

  1   0   0   red
  0   1   0   green
  0   0   1   blue

  1   1   0   yellow
  1   0   1   magenta
  0   1   1   cyan

  1   1  .5   paleyellow  .
  1  .5   1   violet      .
 .5   1   1   aquamarine  .

  1  .5  .5   deepsalmon  .
 .5   1  .5   palegreen   .
 .5  .5   1   slate       .

 .75 .75  0   olive       .
 .75  0  .75  purple      .
  0  .75 .75  teal        .

 .6  .6  .1   deepolive   .
 .6  .1  .6   deeppurple  .
 .1  .6  .6   deepteal    .

  1  .5   0   orange      .
  1   0  .5   hotpink     .
 .5   1   0   chartreuse  .
  0   1  .5   limegreen   .
  0  .5   1   marine      .
 .5   0   1   purpleblue  .

*/

  register CColor *I=G->Color;
  int a;
  int set1;
  float f;
  float spectrumS[13][3] = { 
    { 1.0, 0.0, 1.0 }, /* magenta - 0 */
    { 0.5, 0.0, 1.0 },
    { 0.0, 0.0, 1.0 }, /* blue - 166.66  */
    { 0.0, 0.5, 1.0 },
    { 0.0, 1.0, 1.0 }, /* cyan - 333.33 */

    { 0.0, 1.0, 0.5 },
    { 0.0, 1.0, 0.0 }, /* green - 500 */
    { 0.5, 1.0, 0.0 },
    { 1.0, 1.0, 0.0 }, /* yellow - 666.66 */
    { 1.0, 0.5, 0.0 },

    { 1.0, 0.0, 0.0 }, /* red - 833.33 */
    { 1.0, 0.0, 0.5 },
    { 1.0, 0.0, 1.0 }, /* magenta - 999 */
  };

  float spectrumR[13][3] = { 
    { 1.0, 1.0, 0.0 }, /* yellow - 0 */
    { 0.5, 1.0, 0.0 }, /* chartreuse */
    { 0.0, 1.0, 0.0 }, /* green - 166.66 */
    { 0.0, 1.0, 0.5 }, /* limegreen */
    { 0.0, 1.0, 1.0 }, /* cyan - 333.33 */

    { 0.0, 0.5, 1.0 }, /* marine */
    { 0.0, 0.0, 1.0 }, /* blue - 500 */
    { 0.5, 0.0, 1.0 }, /* purpleblue */
    { 1.0, 0.0, 1.0 }, /* magenta - 666.66 */
    { 1.0, 0.0, 0.5 }, /* hotpink */

    { 1.0, 0.0, 0.0 }, /* red - 833.33 */
    { 1.0, 0.5, 0.0 }, /* orange */
    { 1.0, 1.0, 0.0 }, /* yellow - 999 */
  };

  float spectrumC[][3] = {
    { 1.0, 1.0, 0.0 }, /* yellow - 0 */
    { 0.0, 0.0, 1.0 }, /* blue - 83.333 */
    { 1.0, 0.0, 0.0 }, /* red - 167.67*/
    { 0.0, 1.0, 0.0 }, /* green - 250.00 */
    { 1.0, 0.0, 1.0 }, /* magenta - 333.33 */

    { 0.0, 1.0, 1.0 }, /* cyan - 416.67 */
    { 1.0, 1.0, 0.0 }, /* yellow - 500.00*/    
    { 0.0, 1.0, 0.0 }, /* green - 583.33*/    
    { 0.0, 0.0, 1.0 }, /* blue - 666.67 */    
    { 1.0, 0.0, 1.0 }, /* magenta - 750.00*/

    { 1.0, 1.0, 0.0 }, /* yellow - 833.33*/    
    { 1.0, 0.0, 0.0 }, /* red - 916.67*/    
    { 0.0, 1.0, 1.0 }, /* cyan - 999 */
  };

  float spectrumW[][3] = {
    { 1.0, 1.0, 0.0 }, /* yellow - 0*/
    { 1.0, 1.0, 1.0 }, /* white */
    { 0.0, 0.0, 1.0 }, /* blue  - 83.333 */
    { 1.0, 1.0, 1.0 }, /* white */
    { 1.0, 0.0, 0.0 }, /* red - 166.67 */

    { 1.0, 1.0, 1.0 }, /* white */
    { 0.0, 1.0, 0.0 }, /* green - 250.00 */
    { 1.0, 1.0, 1.0 }, /* white */
    { 1.0, 0.0, 1.0 }, /* magenta - 333.33 */
    { 1.0, 1.0, 1.0 }, /* white */

    { 0.0, 1.0, 1.0 }, /* cyan - 416.67 */
    { 1.0, 1.0, 1.0 }, /* white */
    { 1.0, 1.0, 0.0 }, /* yellow - 500.00*/    
    { 1.0, 1.0, 1.0 }, /* white */
    { 0.0, 1.0, 0.0 }, /* green - 583.33*/    

    { 1.0, 1.0, 1.0 }, /* white */
    { 0.0, 0.0, 1.0 }, /* blue - 666.67 */    
    { 1.0, 1.0, 1.0 }, /* white */
    { 1.0, 0.0, 1.0 }, /* magenta - 750.00*/
    { 1.0, 1.0, 1.0 }, /* white */

    { 1.0, 1.0, 0.0 }, /* yellow - 833.33 */    
    { 1.0, 1.0, 1.0 }, /* white */
    { 1.0, 0.0, 0.0 }, /* red - 916.67*/
    { 1.0, 1.0, 1.0 }, /* white */
    { 0.0, 1.0, 1.0 }, /* cyan - 999 */
  };

  float spectrumO[29][3] = { /* a rainbow with perceptive color balancing and extra blue/red at the ends */
    { 1.0, 0.0,  1.0 }, /* violet */
    { 0.8F,0.0,  1.0 }, 

    { 0.5F,0.0,  1.0 }, /* blend */

    { 0.0, 0.0,  1.0 }, /* blue */
    { 0.0, 0.0,  1.0 }, /* blue */
    { 0.0, 0.2F, 1.0 }, 

    { 0.0, 0.5F, 1.0 }, /* blend */

    { 0.0, 0.8F, 1.0 }, 
    { 0.0, 1.0,  1.0 }, /* cyan */
    { 0.0, 1.0,  0.8F}, 

    { 0.0, 1.0,  0.5F}, /* blend */

    { 0.0, 1.0,  0.2F},  
    { 0.0, 1.0,  0.0 },  /* green */
    { 0.2F,1.0,  0.0 },  

    { 0.5F,1.0,  0.0 },  /* blend */

    { 0.8F,1.0,  0.0 },  
    { 1.0, 1.0,  0.0 },  /* yellow */
    { 1.0, 0.9F, 0.0 },  

    { 1.0, 0.75F,0.0 },  /* blend */

    { 1.0, 0.6F, 0.0 },  
    { 1.0, 0.5F, 0.0 },  /* orange */
    { 1.0, 0.4F, 0.0 },  

    { 1.0, 0.3F,0.0 }, /* blend */

    { 1.0, 0.2F, 0.0 },  
    { 1.0, 0.0,  0.0 },  /* red */
    { 1.0, 0.0,  0.0 },  /* red */

    { 1.0, 0.0,  0.5F},  /* blend */

    { 1.0, 0.0,  0.8F},  /* violet */
    { 1.0, 0.0,  1.0 },  /* violet */
  };

  /* BLUE->VIOLET->RED r546 to r909 */
  /* BLUE->CYAN->GREEN->YELLOW->RED s182 to s909 */
  /* BLUE->WHITE->RED w00 to */

  I->NColor=0;

  strcpy(I->Color[I->NColor].Name,"white");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"black");
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"blue");
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"green");
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"red");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"cyan");
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"yellow");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"dash");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"magenta");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"salmon");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.6F; /* was 0.5 */
  I->Color[I->NColor].Color[2]=0.6F; /* wat 0.5 */
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"lime");
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"slate");
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"hotpink");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"orange");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"yellowgreen"); /* AKA puke green */
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"bluegreen"); /* legacy name -- same as limegreen */
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"blueviolet"); /* legacy name */
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"marine");
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"olive");
  I->Color[I->NColor].Color[0]=0.77F;
  I->Color[I->NColor].Color[1]=0.70F;
  I->Color[I->NColor].Color[2]=0.00F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"purple");
  I->Color[I->NColor].Color[0]=0.75F;
  I->Color[I->NColor].Color[1]=0.00F;
  I->Color[I->NColor].Color[2]=0.75F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"teal");
  I->Color[I->NColor].Color[0]=0.00F;
  I->Color[I->NColor].Color[1]=0.75F;
  I->Color[I->NColor].Color[2]=0.75F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"ruby");
  I->Color[I->NColor].Color[0]=0.6F;
  I->Color[I->NColor].Color[1]=0.2F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"forest");
  I->Color[I->NColor].Color[0]=0.2F;
  I->Color[I->NColor].Color[1]=0.6F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"deepblue"); /* was "deep" */
  I->Color[I->NColor].Color[0]=0.25F;
  I->Color[I->NColor].Color[1]=0.25F;
  I->Color[I->NColor].Color[2]=0.65F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"grey"); /* english spelling */
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"gray"); /* american spelling */
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"carbon");
  I->Color[I->NColor].Color[0]=0.2F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"nitrogen");
  I->Color[I->NColor].Color[0]=0.2F;
  I->Color[I->NColor].Color[1]=0.2F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"oxygen");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.3F;
  I->Color[I->NColor].Color[2]=0.3F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"hydrogen");
  I->Color[I->NColor].Color[0]=0.9F;
  I->Color[I->NColor].Color[1]=0.9F;
  I->Color[I->NColor].Color[2]=0.9F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"brightorange");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.7F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"sulfur");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"tv_red");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.2F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"tv_green");
  I->Color[I->NColor].Color[0]=0.2F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"tv_blue");
  I->Color[I->NColor].Color[0]=0.3F;
  I->Color[I->NColor].Color[1]=0.3F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"tv_yellow");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"yelloworange");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.87F;
  I->Color[I->NColor].Color[2]=0.37F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"tv_orange");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.55F;
  I->Color[I->NColor].Color[2]=0.15F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br0");
  I->Color[I->NColor].Color[0]=0.1F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br1");
  I->Color[I->NColor].Color[0]=0.2F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.9F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br2");
  I->Color[I->NColor].Color[0]=0.3F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.8F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br3");
  I->Color[I->NColor].Color[0]=0.4F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.7F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br4");
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.6F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br5");
  I->Color[I->NColor].Color[0]=0.6F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br6");
  I->Color[I->NColor].Color[0]=0.7F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.4F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br7");
  I->Color[I->NColor].Color[0]=0.8F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.3F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br8");
  I->Color[I->NColor].Color[0]=0.9F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.2F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"br9");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.1F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"pink");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.65F;
  I->Color[I->NColor].Color[2]=0.85F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"firebrick");
  I->Color[I->NColor].Color[0]=0.698F;
  I->Color[I->NColor].Color[1]=0.13F;
  I->Color[I->NColor].Color[2]=0.13F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"chocolate");
  I->Color[I->NColor].Color[0]=0.555F;
  I->Color[I->NColor].Color[1]=0.222F;
  I->Color[I->NColor].Color[2]=0.111F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"brown");
  I->Color[I->NColor].Color[0]=0.65F;
  I->Color[I->NColor].Color[1]=0.32F;
  I->Color[I->NColor].Color[2]=0.17F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"wheat");
  I->Color[I->NColor].Color[0]=0.99F;
  I->Color[I->NColor].Color[1]=0.82F;
  I->Color[I->NColor].Color[2]=0.65F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"violet");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  /* greybow */

  for(a=0;a<100;a=a+1) {
    sprintf(I->Color[I->NColor].Name,"grey%02d",a); /* english spelling */
    I->Color[I->NColor].Color[0]=a/99.0F;
    I->Color[I->NColor].Color[1]=a/99.0F;
    I->Color[I->NColor].Color[2]=a/99.0F;
    I->NColor++;
  }

  strcpy(I->Color[I->NColor].Name,"lightmagenta");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.2F;
  I->Color[I->NColor].Color[2]=0.8F;
  I->NColor++;

  #define A_DIV 83.333333333F

  /* full spectrum (s000-s999) */

  for(a=0;a<1000;a=a+1) {
    set1=(int)(a/A_DIV);
    sprintf(I->Color[I->NColor].Name,"s%03d",a);
    f = 1.0F-(a-(set1*A_DIV))/A_DIV;
    I->Color[I->NColor].Color[0]=f*spectrumS[set1][0]+(1.0F-f)*spectrumS[set1+1][0];
    I->Color[I->NColor].Color[1]=f*spectrumS[set1][1]+(1.0F-f)*spectrumS[set1+1][1];
    I->Color[I->NColor].Color[2]=f*spectrumS[set1][2]+(1.0F-f)*spectrumS[set1+1][2];
    I->NColor++;
  }

  /* offset & reversed full spectrum (r000-r999) */

  for(a=0;a<1000;a=a+1) {
    set1=(int)(a/A_DIV);
    sprintf(I->Color[I->NColor].Name,"r%03d",a);
    f = 1.0F-(a-(set1*A_DIV))/A_DIV;
    I->Color[I->NColor].Color[0]=f*spectrumR[set1][0]+(1.0F-f)*spectrumR[set1+1][0];
    I->Color[I->NColor].Color[1]=f*spectrumR[set1][1]+(1.0F-f)*spectrumR[set1+1][1];
    I->Color[I->NColor].Color[2]=f*spectrumR[set1][2]+(1.0F-f)*spectrumR[set1+1][2];
    I->NColor++;
  }

  /* complementary spectra (c000-c999) */

  for(a=0;a<1000;a=a+1) {
    set1=(int)(a/A_DIV);
    sprintf(I->Color[I->NColor].Name,"c%03d",a);
    f = 1.0F-(a-(set1*A_DIV))/A_DIV;
    I->Color[I->NColor].Color[0]=f*spectrumC[set1][0]+(1.0F-f)*spectrumC[set1+1][0];
    I->Color[I->NColor].Color[1]=f*spectrumC[set1][1]+(1.0F-f)*spectrumC[set1+1][1];
    I->Color[I->NColor].Color[2]=f*spectrumC[set1][2]+(1.0F-f)*spectrumC[set1+1][2];
    I->NColor++;
  }

  #define W_DIV 41.666666667F

  /* complementary spectra separated by white (w000-w999) */

  for(a=0;a<1000;a=a+1) {
    set1=(int)(a/W_DIV);
    sprintf(I->Color[I->NColor].Name,"w%03d",a);
    f = 1.0F-(a-(set1*W_DIV))/W_DIV;
    I->Color[I->NColor].Color[0]=f*spectrumW[set1][0]+(1.0F-f)*spectrumW[set1+1][0];
    I->Color[I->NColor].Color[1]=f*spectrumW[set1][1]+(1.0F-f)*spectrumW[set1+1][1];
    I->Color[I->NColor].Color[2]=f*spectrumW[set1][2]+(1.0F-f)*spectrumW[set1+1][2];
    I->NColor++;
  }

  strcpy(I->Color[I->NColor].Name,"density");
  I->Color[I->NColor].Color[0]=0.1F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.6F;
  I->NColor++;

  for(a=0;a<100;a=a+1) {
    sprintf(I->Color[I->NColor].Name,"gray%02d",a); /* american */
    I->Color[I->NColor].Color[0]=a/99.0F;
    I->Color[I->NColor].Color[1]=a/99.0F;
    I->Color[I->NColor].Color[2]=a/99.0F;
    I->NColor++;
  }

  /* original full spectrum, with extra blue and red at the ends (o000-o999) */

  #define B_DIV 35.7143F

  for(a=0;a<1000;a=a+1) {
    set1=(int)(a/B_DIV);
    sprintf(I->Color[I->NColor].Name,"o%03d",a);
    f = 1.0F-(a-(set1*B_DIV))/B_DIV;
    I->Color[I->NColor].Color[0]=f*spectrumO[set1][0]+(1.0F-f)*spectrumO[set1+1][0];
    I->Color[I->NColor].Color[1]=f*spectrumO[set1][1]+(1.0F-f)*spectrumO[set1+1][1];
    I->Color[I->NColor].Color[2]=f*spectrumO[set1][2]+(1.0F-f)*spectrumO[set1+1][2];
    I->NColor++;
  }

  strcpy(I->Color[I->NColor].Name,"paleyellow");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"aquamarine");
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"deepsalmon");
  I->Color[I->NColor].Color[0]=1.0F;
  I->Color[I->NColor].Color[1]=0.5F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"palegreen");
  I->Color[I->NColor].Color[0]=0.65F;
  I->Color[I->NColor].Color[1]=0.9F;
  I->Color[I->NColor].Color[2]=0.65F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"deepolive");
  I->Color[I->NColor].Color[0]=0.6F;
  I->Color[I->NColor].Color[1]=0.6F;
  I->Color[I->NColor].Color[2]=0.1F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"deeppurple");
  I->Color[I->NColor].Color[0]=0.6F;
  I->Color[I->NColor].Color[1]=0.1F;
  I->Color[I->NColor].Color[2]=0.6F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"deepteal");
  I->Color[I->NColor].Color[0]=0.1F;
  I->Color[I->NColor].Color[1]=0.6F;
  I->Color[I->NColor].Color[2]=0.6F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"chartreuse");
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"limegreen"); /* same as bluegreen */
  I->Color[I->NColor].Color[0]=0.0F;
  I->Color[I->NColor].Color[1]=1.0F;
  I->Color[I->NColor].Color[2]=0.5F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"purpleblue"); 
  I->Color[I->NColor].Color[0]=0.5F;
  I->Color[I->NColor].Color[1]=0.0F;
  I->Color[I->NColor].Color[2]=1.0F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"lightteal"); 
  I->Color[I->NColor].Color[0]=0.4F;
  I->Color[I->NColor].Color[1]=0.7F;
  I->Color[I->NColor].Color[2]=0.7F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"splitpea"); 
  I->Color[I->NColor].Color[0]=0.52F;
  I->Color[I->NColor].Color[1]=0.75F;
  I->Color[I->NColor].Color[2]=0.00F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"raspberry"); 
  I->Color[I->NColor].Color[0]=0.70F;
  I->Color[I->NColor].Color[1]=0.30F;
  I->Color[I->NColor].Color[2]=0.40F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"sand"); 
  I->Color[I->NColor].Color[0]=0.72F;
  I->Color[I->NColor].Color[1]=0.55F;
  I->Color[I->NColor].Color[2]=0.30F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"smudge"); 
  I->Color[I->NColor].Color[0]=0.55F;
  I->Color[I->NColor].Color[1]=0.70F;
  I->Color[I->NColor].Color[2]=0.40F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"violetpurple");
  I->Color[I->NColor].Color[0]=0.55F;
  I->Color[I->NColor].Color[1]=0.25F;
  I->Color[I->NColor].Color[2]=0.60F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"dirtyviolet");
  I->Color[I->NColor].Color[0]=0.70F;
  I->Color[I->NColor].Color[1]=0.50F;
  I->Color[I->NColor].Color[2]=0.50F;
  I->NColor++;


  strcpy(I->Color[I->NColor].Name,"deepsalmon");
  I->Color[I->NColor].Color[0]=1.00F;
  I->Color[I->NColor].Color[1]=0.42F;
  I->Color[I->NColor].Color[2]=0.42F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"lightpink");
  I->Color[I->NColor].Color[0]=1.00F;
  I->Color[I->NColor].Color[1]=0.75F;
  I->Color[I->NColor].Color[2]=0.87F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"greencyan");
  I->Color[I->NColor].Color[0]=0.25F;
  I->Color[I->NColor].Color[1]=1.00F;
  I->Color[I->NColor].Color[2]=0.75F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"limon");
  I->Color[I->NColor].Color[0]=0.75F;
  I->Color[I->NColor].Color[1]=1.00F;
  I->Color[I->NColor].Color[2]=0.25F;
  I->NColor++;


  strcpy(I->Color[I->NColor].Name,"skyblue");
  I->Color[I->NColor].Color[0]=0.20F;
  I->Color[I->NColor].Color[1]=0.50F;
  I->Color[I->NColor].Color[2]=0.80F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"bluewhite");
  I->Color[I->NColor].Color[0]=0.85F;
  I->Color[I->NColor].Color[1]=0.85F;
  I->Color[I->NColor].Color[2]=1.00F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"warmpink");
  I->Color[I->NColor].Color[0]=0.85F;
  I->Color[I->NColor].Color[1]=0.20F;
  I->Color[I->NColor].Color[2]=0.50F;
  I->NColor++;

  strcpy(I->Color[I->NColor].Name,"darksalmon");
  I->Color[I->NColor].Color[0]=0.73F;
  I->Color[I->NColor].Color[1]=0.55F;
  I->Color[I->NColor].Color[2]=0.52F;
  I->NColor++;

  /* if any more colors need to be added, add them here at the end so that existing files won't have their colors changed */


  for(a=0;a<I->NColor;a++) { 
    /* mark all current colors non-custom so that they don't get saved in session files */
    I->Color[a].Custom=false;
  }
  I->NExt = 0;

}

int ColorTableLoad(PyMOLGlobals *G,char *fname,int quiet)
{
  register CColor *I=G->Color; 
  int ok=true;
  int width=512,height=512;
  unsigned int *table = NULL;

  if(!strcmp(fname,"rgb")) {
    FreeP(I->ColorTable);
    PRINTFB(G,FB_Color,FB_Actions)
      " Color: purged table; restoring RGB colors.\n"
      ENDFB(G);
    ColorUpdateClamp(G,-1);    
    
  } else if(!strcmp(fname,"pymol")) {
    
    int x,y;
    unsigned int r=0,g=0,b=0;
    unsigned int *pixel,mask,*p;
    unsigned int rc,bc,gc;
    unsigned int gf,bf,rf;

    float green_max=0.75F;
    float red_max=0.95F;
    float blue_max=0.97F;
    
    float min_factor=0.15F;

    red_max = SettingGet(G,cSetting_pymol_space_max_red);
    green_max = SettingGet(G,cSetting_pymol_space_max_green);
    blue_max = SettingGet(G,cSetting_pymol_space_max_blue);
    min_factor = SettingGet(G,cSetting_pymol_space_min_factor);

    FreeP(I->ColorTable);
    if(I->BigEndian)
      mask = 0x000000FF;
    else
      mask = 0xFF000000;
    
    table = Alloc(unsigned int,512*512);
    
    p=(unsigned int*)table; 
    for(x=0;x<width;x++)
      for(y=0;y<height;y++)
        *(p++)=mask;
    
    for(y=0;y<height;y++) 
      for(x=0;x<width;x++) {
        rc = r;
        gc = g;
        bc = b;

        if((r>=g)&&(r>=b)) {
          if(rc>255*red_max) {
            rc=(unsigned int)(red_max*255);
            bc=bc*rc/r;
            gc=gc*rc/r;
          }
        } else if((g>=b)&&(g>=r)) {
          if(gc>255*green_max) {
            gc=(unsigned int)(green_max*255);
            bc=bc*gc/g;
            rc=rc*gc/g;
          }
        } else if((b>=g)&&(b>=r)) {
          if(bc>255*blue_max) {
            bc=(unsigned int)(blue_max*255);
            gc=gc*bc/b;
            rc=rc*bc/b;
          }
        }

        rf = (int)(min_factor*rc+0.49999F);
        gf = (int)(min_factor*gc+0.49999F);
        bf = (int)(min_factor*bc+0.49999F);
        
        if (rc<gf) rc = (int)gf;
        if (bc<gf) bc = (int)gf;
        
        if (rc<bf) rc = (int)bf;
        if (gc<bf) gc = (int)bf;
        
        if (gc<rf) gc = (int)rf;
        if (bc<rf) bc = (int)rf;
    
        if(rc>255) rc=255;
        if(bc>255) bc=255;
        if(gc>255) gc=255;

        pixel = table+((width)*y)+x;
        if(I->BigEndian) {
          *(pixel)=
            mask|(rc<<24)|(gc<<16)|(bc<<8);
        } else {
          *(pixel)=
            mask|(bc<<16)|(gc<<8)|rc;
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
  
    I->ColorTable = table;
    if(!quiet) {
      PRINTFB(G,FB_Color,FB_Actions)
        " Color: defined table '%s'.\n",fname
        ENDFB(G);
    }
    
    ColorUpdateClamp(G,-1);
    ExecutiveInvalidateRep(G,cKeywordAll,cRepAll,cRepInvColor);
    SceneChanged(G);

  } else {
    if(strlen(fname)) {
      
      unsigned int u_width=(unsigned int)width,u_height = (unsigned int)height;
      unsigned char *u_table = (unsigned char*)table;
      if(MyPNGRead(fname,
                   &u_table,
                   &u_width,
                   &u_height)) {
        table = (unsigned int*)u_table;
        width = (signed int)u_width;
        height = (signed int)u_height;
        if((width==512)&&(height==512)) {
          FreeP(I->ColorTable);
          I->ColorTable = table;
          if(!quiet) {
            PRINTFB(G,FB_Color,FB_Actions)
              " Color: loaded table '%s'.\n",fname
              ENDFB(G);
          }
          
          ColorUpdateClamp(G,-1);

        } else {
          PRINTFB(G,FB_Color,FB_Errors)
            " ColorTableLoad-Error: invalid dimensions w x h  = %d x %d; should be 512 x 512.\n",
            width,height
            ENDFB(G);
          
          ok=false;      
        }
      } else {
        PRINTFB(G,FB_Color,FB_Errors)
          " ColorTableLoad-Error: unable to load '%s'.\n",fname
          ENDFB(G);
        ok=false;
      }
    } else {
      PRINTFB(G,FB_Color,FB_Actions)
        " Color: purged table; colors unchanged.\n"
        ENDFB(G);
      FreeP(I->ColorTable);
    }
  }
  if(!ok) {
    FreeP(table);
  } else {
    ExecutiveInvalidateRep(G,cKeywordAll,cRepAll,cRepInvColor);
    SceneChanged(G);
  }
  return(ok);
}

/*========================================================================*/
void ColorUpdateClamp(PyMOLGlobals *G,int index)
{
  int i;
  int once=false;
  register CColor *I=G->Color; 
  unsigned int *entry;
  float *color,*new_color;
  unsigned int r,g,b,rr,gr,br;
  unsigned int ra,ga,ba;
  unsigned int rc[2][2][2],gc[2][2][2],bc[2][2][2];
  float fr,fg,fb,frm1,fgm1,fbm1,rct,gct,bct;
  int x,y,z;

  i = index;
  if(index>=0) {
    once=true;
  }
  for(i=0;i<I->NColor;i++) {
    if(!once) index=i;
   
    if(index<I->NColor) {
      if(!I->ColorTable) {
        I->Color[index].ClampedFlag = false;
      } else {
        color = I->Color[index].Color;
        r = ((int)(255*color[0]+0.5F))&0xFF;
        g = ((int)(255*color[1]+0.5F))&0xFF;
        b = ((int)(255*color[2]+0.5F))&0xFF;

        rr = r&0x3;
        gr = g&0x3;
        br = b&0x3;

        r = (r>>2);
        g = (g>>2);
        b = (b>>2);

        /* now for a crude little trilinear */

        for(x=0;x<2;x++) {
          ra = r + x;
          if(ra>63) ra=63;
          for(y=0;y<2;y++) {
            ga = g + y;
            if(ga>63) ga=63;
            for(z=0;z<2;z++) {
              ba = b + z;
              if(ba>63) ba=63;
              
              entry = I->ColorTable + (ra<<12) + (ga<<6) + ba;
              
              if(I->BigEndian) {
                rc[x][y][z] = 0xFF&((*entry)>>24);
                gc[x][y][z] = 0xFF&((*entry)>>16);
                bc[x][y][z] = 0xFF&((*entry)>>8);
              } else {
                rc[x][y][z] = 0xFF&((*entry)    );
                gc[x][y][z] = 0xFF&((*entry)>> 8);
                bc[x][y][z] = 0xFF&((*entry)>>16);
              }
            }
          }
        }

        frm1 = rr/4.0F;
        fgm1 = gr/4.0F;
        fbm1 = br/4.0F;
        
        fr = 1.0F - frm1;
        fg = 1.0F - fgm1;
        fb = 1.0F - fbm1;

        rct = 0.4999F + 
          (fr   * fg   * fb   * rc[0][0][0]) + 
          (frm1 * fg   * fb   * rc[1][0][0]) + 
          (fr   * fgm1 * fb   * rc[0][1][0]) + 
          (fr   * fg   * fbm1 * rc[0][0][1]) + 
          (frm1 * fgm1 * fb   * rc[1][1][0]) + 
          (fr   * fgm1 * fbm1 * rc[0][1][1]) + 
          (frm1 * fg   * fbm1 * rc[1][0][1]) + 
          (frm1 * fgm1 * fbm1 * rc[1][1][1]);

        gct = 0.4999F + 
          (fr   * fg   * fb   * gc[0][0][0]) + 
          (frm1 * fg   * fb   * gc[1][0][0]) + 
          (fr   * fgm1 * fb   * gc[0][1][0]) + 
          (fr   * fg   * fbm1 * gc[0][0][1]) + 
          (frm1 * fgm1 * fb   * gc[1][1][0]) + 
          (fr   * fgm1 * fbm1 * gc[0][1][1]) + 
          (frm1 * fg   * fbm1 * gc[1][0][1]) + 
          (frm1 * fgm1 * fbm1 * gc[1][1][1]);

        bct = 0.4999F + 
          (fr   * fg   * fb   * bc[0][0][0]) + 
          (frm1 * fg   * fb   * bc[1][0][0]) + 
          (fr   * fgm1 * fb   * bc[0][1][0]) + 
          (fr   * fg   * fbm1 * bc[0][0][1]) + 
          (frm1 * fgm1 * fb   * bc[1][1][0]) + 
          (fr   * fgm1 * fbm1 * bc[0][1][1]) + 
          (frm1 * fg   * fbm1 * bc[1][0][1]) + 
          (frm1 * fgm1 * fbm1 * bc[1][1][1]);

        if(r>=63) rct+=rr;
        if(g>=63) gct+=gr;
        if(b>=63) bct+=br;

        if(rct<=2.0F) rct=0.0F; /* make sure black is black */
        if(gct<=2.0F) gct=0.0F;
        if(bct<=2.0F) bct=0.0F;

        new_color = I->Color[index].Clamped;
        new_color[0] = rct/255.0F;
        if(new_color[0]>1.0F) new_color[0]=1.0F;
        new_color[1] = gct/255.0F;
        if(new_color[1]>1.0F) new_color[1]=1.0F; 
        new_color[2] = bct/255.0F;
        if(new_color[2]>1.0F) new_color[2]=1.0F;

        PRINTFD(G,FB_Color)
          "%5.3f %5.3f %5.3f -> %5.3f %5.3f %5.3f\n",
               color[0],color[1],color[2],
               new_color[0],new_color[1],new_color[2]
          ENDFD;
      
        I->Color[index].ClampedFlag = true;
      }
    }
      
    if(once) break;
  }
}
/*========================================================================*/
int ColorInit(PyMOLGlobals *G)
{
  CColor *I=NULL;
  
  if( (I=(G->Color=Calloc(CColor,1))) ) {
    unsigned int test;
    unsigned char *testPtr;

    test = 0xFF000000;
    testPtr = (unsigned char*)&test;
    I->BigEndian = (*testPtr)&&1;
    
    I->Color=VLAMalloc(5300,sizeof(ColorRec),5,true);
    I->NColor=0;
    ColorReset(G);
    I->NExt=0;
    I->Ext=VLAMalloc(10,sizeof(ExtRec),5,true);
    I->ColorTable=NULL;
    return 1;
  } else {
    return 0;
  }
}

/*========================================================================*/
float *ColorGet(PyMOLGlobals *G,int index)
{
  register CColor *I=G->Color;
  float *ptr;
  if((index>=0)&&(index<I->NColor)) {
    if(I->Color[index].ClampedFlag&&(int)SettingGet(G,cSetting_clamp_colors))
      ptr = I->Color[index].Clamped;
    else
      ptr = I->Color[index].Color;
    return(ptr);
  } else /* invalid color id, then simply return white */
	 return(I->Color[0].Color);
}

