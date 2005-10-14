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
#ifndef _H_RepSurface
#define _H_RepSurface

#include"Rep.h"
#include"CoordSet.h"

Rep *RepSurfaceNew(CoordSet *cset);

#define cRepSurface_by_flags     0
#define cRepSurface_all          1
#define cRepSurface_heavy_atoms  2

#endif