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
-* Thomas Malik (matmul derived)
-* Whoever wrote EISPACK
Z* -------------------------------------------------------------------
*/

#include<math.h>
#include"Base.h"
#include"Vector.h"
#include"Matrix.h"
#include"MemoryDebug.h"
#include"Ortho.h"

/* WARNING - MAJOR GOTCHA!  

   A state of confusion has arisen because of mixed row-major and
   column-major ordering in this module.  THIS NEEDS TO BE
   STRAIGHTENED OUT before any further use is made of the following
   routines.

   The problem is that OpenGL uses one matrix storage format, while C
   and Python follow the other convention.  The situation
   arose because some of the routines below are used to emulate OpenGL
   transformations...(and are in fact based on Mesa code).
   
   Proposed Solution: 

   (1) Assume the C-PYTHON convention of fast-column (row-major?)
   ordering - and specifically rename routines which don't conform as
   fast row (column-major?) "CM44f" matrices.  Clean up and resolve
   problems with existing code that call these routines.
   
   (2) Move all of the 4x4 transformation code into Vector.h and rename this
   module Algebra.h/.c (for linear algebra and fitting).

*/

/*========================================================================*/
void MatrixDump44f(float *m,char *prefix)
{
  if(prefix) {
    PRINTF "%s %12.5f %12.5f %12.5f %12.5f\n",prefix,m[ 0],m[ 1],m[ 2],m[ 3] ENDF;
    PRINTF "%s %12.5f %12.5f %12.5f %12.5f\n",prefix,m[ 4],m[ 5],m[ 6],m[ 7] ENDF;
    PRINTF "%s %12.5f %12.5f %12.5f %12.5f\n",prefix,m[ 8],m[ 9],m[10],m[11] ENDF;
    PRINTF "%s %12.5f %12.5f %12.5f %12.5f\n",prefix,m[12],m[13],m[14],m[15] ENDF;
  } else {
    PRINTF "%12.5f %12.5f %12.5f %12.5f\n",m[ 0],m[ 1],m[ 2],m[ 3] ENDF;
    PRINTF "%12.5f %12.5f %12.5f %12.5f\n",m[ 4],m[ 5],m[ 6],m[ 7] ENDF;
    PRINTF "%12.5f %12.5f %12.5f %12.5f\n",m[ 8],m[ 9],m[10],m[11] ENDF;
    PRINTF "%12.5f %12.5f %12.5f %12.5f\n",m[12],m[13],m[14],m[15] ENDF;
  }
}
/*========================================================================*/
void MatrixApplyTTTfn3f( unsigned int n, float *q, const float m[16], float *p )
/* NOTE: THIS DOESN'T EXPECT A NORMAL 4x4 TRANFORMATION MATRIX...
 * instead, it expects a 4x4 matrix with a pre-translation stored in column 3(0-3) (rows 0-2)
 * and a post-translation stored in row 3(0-3) (cols 0-2), as well as the normal 3x3 */
{
  float m0 = m[0],  m4 = m[4],  m8 = m[8],  m12 = m[12];
  float m1 = m[1],  m5 = m[5],  m9 = m[9],  m13 = m[13];
  float m2 = m[2],  m6 = m[6],  m10 = m[10],  m14 = m[14];
  float m3 = m[3],  m7 = m[7],  m11 = m[11];
  float p0,p1,p2;
  while(n--) {
	p0=*(p++) + m3;
	p1=*(p++) + m7;
	p2=*(p++) + m11;
	*(q++) = m0 * p0 + m4  * p1 + m8 * p2 + m12;
	*(q++) = m1 * p0 + m5  * p1 + m9 * p2 + m13;
	*(q++) = m2 * p0 + m6 * p1 + m10 * p2 + m14;
  }
}
/*========================================================================*/
void MatrixLoadIdentity44f(float *m)
{
   m[0]=1.0;
   m[1]=0.0;
   m[2]=0.0;
   m[3]=0.0;
   
   m[4]=0.0;   
   m[5]=1.0;
   m[6]=0.0;
   m[7]=0.0;
   
   m[8]=0.0;
   m[9]=0.0;
   m[10]=1.0;
   m[11]=0.0;
   
   m[12]=0.0;
   m[13]=0.0;
   m[14]=0.0;
   m[15]=1.0;
}
/*========================================================================*/
void MatrixRotate44f3f( float *m, const float angle, const float x,const float y,const float z)
{
  float m33[9];
  float m44[16];
  rotation_matrix3f(angle,x,y,z,m33);
  m44[0]=m33[0];
  m44[1]=m33[1];
  m44[2]=m33[2];
  m44[3]=0.0;
  m44[4]=m33[3];
  m44[5]=m33[4];
  m44[6]=m33[5];
  m44[7]=0.0;
  m44[8]=m33[6];
  m44[9]=m33[7];
  m44[10]=m33[8];
  m44[11]=0.0;
  m44[12]=0.0;
  m44[13]=0.0;
  m44[14]=0.0;
  m44[15]=1.0;
  MatrixMultiply44f(m44,m);
}

/*========================================================================*/
void MatrixTranslate44f3f( float *m, const float x,const float y,const float z)
{
   m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
   m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
   m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
   m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];                                                                    
}

/*========================================================================*/
void MatrixMultiply44f( const float *b, float *m )
{
  
   int i;
 
#define A(row,col)  m[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define P(row,col)  m[(col<<2)+row]
 
   /* i-te Zeile */
   for (i = 0; i < 4; i++) {
      float ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
      P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
      P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
      P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
      P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
   }
 
#undef A
#undef B
#undef P
}
                                                                                                                             
/*========================================================================*/
void MatrixInvTransform3f(float *p, float *m, float *q)
{
  register float p1  = *p    , p2  = *(p+1), p3  = *(p+2);

  *(q++) = p1*(* m   ) + p2*(*(m+1)) + p3*(*(m+2));
  *(q++) = p1*(*(m+4)) + p2*(*(m+5)) + p3*(*(m+6));
  *(q  ) = p1*(*(m+8)) + p2*(*(m+9)) + p3*(*(m+10));

}
/*========================================================================*/
void MatrixTransform3f(float *p, float *m, float *q)
{
  register float p1  = *p    , p2  = *(p+1), p3  = *(p+2);

  *(q++) = p1*(* m   ) + p2*(*(m+4)) + p3*(*(m+8));
  *(q++) = p1*(*(m+1)) + p2*(*(m+5)) + p3*(*(m+9));
  *(q  ) = p1*(*(m+2)) + p2*(*(m+6)) + p3*(*(m+10));

}

/*========================================================================*/
float MatrixGetRMS(int n,float *v1,float *v2,float *wt)
{
  /* Just Compute RMS given current coordinates */

  float *vv1,*vv2;
  float  err, etmp, tmp;
  int a, c;
  float sumwt = 0.0;

  if(wt) {
    for(c=0;c<n;c++)
      if (wt[c]!=0.0) {
        sumwt = sumwt + wt[c];
      } else {
        sumwt = sumwt + 1.0; /* WHAT IS THIS? */
      }
  } else {
    for(c=0;c<n;c++)
      sumwt+=1.0;
  }
  err = 0.0;
  vv1=v1;
  vv2=v2;
  for(c=0;c<n;c++) {
    etmp = 0.0;
    for(a=0;a<3;a++) {
      tmp = (vv2[a]-vv1[a]);
      etmp += tmp*tmp;
    }
    if(wt)
      err += wt[c] * etmp;
    else 
      err += etmp;
    vv1+=3;
    vv2+=3;
  }
  err=err/sumwt;
  err=sqrt(err);

  if(fabs(err)<R_SMALL4)
    err=0.0;

  return(err);
}

/*========================================================================*/
float MatrixFitRMS(int n,float *v1,float *v2,float *wt,float *ttt)
{
  /*
	Subroutine to do the actual RMS fitting of two sets of vector coordinates
	This routine does not rotate the actual coordinates, but instead returns 
	the RMS fitting value, along with the center-of-mass translation vectors 
	T1 and T2 and the rotation vector M, which rotates the translated 
	coordinates of molecule 2 onto the translated coordinates of molecule 1.
  */

  float *vv1,*vv2;
  float m[3][3],aa[3][3],x[3],xx[3];
  float sumwt, tol, sig, gam;
  float sg, bb, cc, err, etmp, tmp;
  int a, b, c, maxiter, iters, ix, iy, iz;
  float t1[3],t2[3];

  /* Initialize arrays. */

  for(a=0;a<3;a++) {
	for(b=0;b<3;b++) {
	  m[a][b] = 0.0;
	  aa[a][b] = 0.0;
	}
	m[a][a] = 1.0;
	t1[a]=0.0;
	t2[a]=0.0;
  }

  sumwt = 0.0;
  tol = 0.001;
  maxiter = 1000;

  /* Calculate center-of-mass vectors */

  vv1=v1;
  vv2=v2;

  if(wt) {
	for(c=0;c<n;c++)
	  {
		for(a=0;a<3;a++) {
		  t1[a] += wt[c]*vv1[a];
		  t2[a] += wt[c]*vv2[a];
		}
		if (wt[c]!=0.0) {
		  sumwt = sumwt + wt[c];
		} else {
		  sumwt = sumwt + 1.0; /* WHAT IS THIS? */
		}
		vv1+=3;
		vv2+=3;
	  }
  } else {
	for(c=0;c<n;c++)
	  {
		for(a=0;a<3;a++) {
		  t1[a] += vv1[a];
		  t2[a] += vv2[a];
		}
		sumwt+=1.0;
		vv1+=3;
		vv2+=3;
	  }
  }
  if(sumwt==0.0) sumwt = 1.0;
  for(a=0;a<3;a++) {
	t1[a] /= sumwt;
	t2[a] /= sumwt;
  }
  /* Calculate correlation matrix */
  vv1=v1;
  vv2=v2;
  for(c=0;c<n;c++)
	{
	  if(wt) {
		for(a=0;a<3;a++) {
		  x[a] = wt[c]*vv1[a] - t1[a];
		  xx[a] = wt[c]*vv2[a] - t2[a];
		}
	  } else {
		for(a=0;a<3;a++) {
		  x[a] = vv1[a] - t1[a];
		  xx[a] = vv2[a] - t2[a];
		}
	  }
	  for(a=0;a<3;a++)
		for(b=0;b<3;b++)
		  aa[a][b] = aa[a][b] + xx[a]*x[b];
	  vv1+=3;
	  vv2+=3;
	}
  /* Primary iteration scheme to determine rotation matrix for molecule 2 */
  iters = 0;
  while(1) {
	/*	for(a=0;a<3;a++)
		{
		for(b=0;b<3;b++) 
		printf("%8.3f ",m[a][b]);
		printf("\n");
		}
		for(a=0;a<3;a++)
		{
		for(b=0;b<3;b++) 
		printf("%8.3f ",aa[a][b]);
		printf("\n");
		}
		printf("\n");
	*/
	if(iters>=maxiter) 
	  {
		printf("Error in RMS fitting : maximum iterations exceeded.\n");
		break;
	  }
	/* IX, IY, and IZ rotate 1-2-3, 2-3-1, 3-1-2, etc.*/
	iz = (iters+1) % 3;
	iy = (iz+1) % 3;
	ix = (iy+1) % 3;
	sig = aa[iz][iy] - aa[iy][iz];
	gam = aa[iy][iy] + aa[iz][iz];
	/* Determine size of off-diagonal element.  If off-diagonals exceed the
	  diagonal elements * tolerance, perform Jacobi rotation. */
	tmp = sig*sig + gam*gam;
	sg = sqrt(tmp);
	if((sg!=0.0) &&(fabs(sig)>(tol*fabs(gam)))) {
	  sg = 1.0 / sg;
	  for(a=0;a<3;a++)
		{
		  bb = gam*aa[iy][a] + sig*aa[iz][a];
		  cc = gam*aa[iz][a] - sig*aa[iy][a];
		  aa[iy][a] = bb*sg;
		  aa[iz][a] = cc*sg;
		  
		  bb = gam*m[iy][a] + sig*m[iz][a];
		  cc = gam*m[iz][a] - sig*m[iy][a];
		  m[iy][a] = bb*sg;
		  m[iz][a] = cc*sg;
		}
	} else {
	  break;
	}
  iters++;
  }
  /* At this point, we should have a converged rotation matrix (M).  Calculate
	 the weighted RMS error. */
  err = 0.0;
  vv1=v1;
  vv2=v2;
  for(c=0;c<n;c++) {
	etmp = 0.0;
	for(a=0;a<3;a++) {
	  tmp = m[a][0]*(vv2[0]-t2[0])
		+ m[a][1]*(vv2[1]-t2[1])
		+ m[a][2]*(vv2[2]-t2[2]);
	  tmp = (vv1[a]-t1[a])-tmp;
	  etmp += tmp*tmp;
	}
	if(wt)
	  err += wt[c] * etmp;
	else 
	  err += etmp;
	vv1+=3;
	vv2+=3;
  }

  err=err/sumwt;
  err=sqrt(err);

  ttt[0]=m[0][0];
  ttt[1]=m[0][1];
  ttt[2]=m[0][2];
  ttt[3]=-t1[0];
  ttt[4]=m[1][0];
  ttt[5]=m[1][1];
  ttt[6]=m[1][2];
  ttt[7]=-t1[1];
  ttt[8]=m[2][0];
  ttt[9]=m[2][1];
  ttt[10]=m[2][2];
  ttt[11]=-t1[2];
  ttt[12]=t2[0];
  ttt[13]=t2[1];
  ttt[14]=t2[2];
  ttt[15]=1.0; /* for compatibility with normal 4x4 matrices */

  if(fabs(err)<R_SMALL4)
    err=0.0;

  return(err);
}

/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/* Only a partial translation of eispack -
 * just the real general diagonialization and support routines */

/* eispack.f -- translated by f2c (version 19970805).*/

#ifndef F2C_INCLUDE
#define F2C_INCLUDE

typedef long int integer;
typedef unsigned long uinteger;
typedef char *address;
typedef short int shortint;
typedef float real;
typedef double doublereal;
typedef struct { real r, i; } complex;
typedef struct { doublereal r, i; } doublecomplex;
typedef long int logical;
typedef short int shortlogical;
typedef char logical1;
typedef char integer1;
#if 0	/* Adjust for integer*8. */
typedef long long longint;		/* system-dependent */
typedef unsigned long long ulongint;	/* system-dependent */
#define qbit_clear(a,b)	((a) & ~((ulongint)1 << (b)))
#define qbit_set(a,b)	((a) |  ((ulongint)1 << (b)))
#endif

#define TRUE_ (1)
#define FALSE_ (0)

/* Extern is for use with -E */
#ifndef Extern
#define Extern extern
#endif

/* I/O stuff */

#ifdef f2c_i2
/* for -i2 */
typedef short flag;
typedef short ftnlen;
typedef short ftnint;
#else
typedef long int flag;
typedef long int ftnlen;
typedef long int ftnint;
#endif

/*external read, write*/
typedef struct
{	flag cierr;
	ftnint ciunit;
	flag ciend;
	char *cifmt;
	ftnint cirec;
} cilist;

/*internal read, write*/
typedef struct
{	flag icierr;
	char *iciunit;
	flag iciend;
	char *icifmt;
	ftnint icirlen;
	ftnint icirnum;
} icilist;

/*open*/
typedef struct
{	flag oerr;
	ftnint ounit;
	char *ofnm;
	ftnlen ofnmlen;
	char *osta;
	char *oacc;
	char *ofm;
	ftnint orl;
	char *oblnk;
} olist;

/*close*/
typedef struct
{	flag cerr;
	ftnint cunit;
	char *csta;
} cllist;

/*rewind, backspace, endfile*/
typedef struct
{	flag aerr;
	ftnint aunit;
} alist;

/* inquire */
typedef struct
{	flag inerr;
	ftnint inunit;
	char *infile;
	ftnlen infilen;
	ftnint	*inex;	/*parameters in standard's order*/
	ftnint	*inopen;
	ftnint	*innum;
	ftnint	*innamed;
	char	*inname;
	ftnlen	innamlen;
	char	*inacc;
	ftnlen	inacclen;
	char	*inseq;
	ftnlen	inseqlen;
	char 	*indir;
	ftnlen	indirlen;
	char	*infmt;
	ftnlen	infmtlen;
	char	*inform;
	ftnint	informlen;
	char	*inunf;
	ftnlen	inunflen;
	ftnint	*inrecl;
	ftnint	*innrec;
	char	*inblank;
	ftnlen	inblanklen;
} inlist;

#define VOID void

union Multitype {	/* for multiple entry points */
	integer1 g;
	shortint h;
	integer i;
	/* longint j; */
	real r;
	doublereal d;
	complex c;
	doublecomplex z;
	};

typedef union Multitype Multitype;

/*typedef long int Long;*/	/* No longer used; formerly in Namelist */

struct Vardesc {	/* for Namelist */
	char *name;
	char *addr;
	ftnlen *dims;
	int  type;
	};
typedef struct Vardesc Vardesc;

struct Namelist {
	char *name;
	Vardesc **vars;
	int nvars;
	};
typedef struct Namelist Namelist;

#define abs(x) ((x) >= 0 ? (x) : -(x))
#define dabs(x) (doublereal)abs(x)
#define mymin(a,b) ((a) <= (b) ? (a) : (b))
#define mymax(a,b) ((a) >= (b) ? (a) : (b))
#define dmymin(a,b) (doublereal)mymin(a,b)
#define dmymax(a,b) (doublereal)mymax(a,b)
#define bit_test(a,b)	((a) >> (b) & 1)
#define bit_clear(a,b)	((a) & ~((uinteger)1 << (b)))
#define bit_set(a,b)	((a) |  ((uinteger)1 << (b)))

/* procedure parameter types for -A and -C++ */

#define F2C_proc_par_types 1
#ifdef __cplusplus
typedef int /* Unknown procedure type */ (*U_fp)(...);
typedef shortint (*J_fp)(...);
typedef integer (*I_fp)(...);
typedef real (*R_fp)(...);
typedef doublereal (*D_fp)(...), (*E_fp)(...);
typedef /* Complex */ VOID (*C_fp)(...);
typedef /* Double Complex */ VOID (*Z_fp)(...);
typedef logical (*L_fp)(...);
typedef shortlogical (*K_fp)(...);
typedef /* Character */ VOID (*H_fp)(...);
typedef /* Subroutine */ int (*S_fp)(...);
#else
typedef int /* Unknown procedure type */ (*U_fp)();
typedef shortint (*J_fp)();
typedef integer (*I_fp)();
typedef real (*R_fp)();
typedef doublereal (*D_fp)(), (*E_fp)();
typedef /* Complex */ VOID (*C_fp)();
typedef /* Double Complex */ VOID (*Z_fp)();
typedef logical (*L_fp)();
typedef shortlogical (*K_fp)();
typedef /* Character */ VOID (*H_fp)();
typedef /* Subroutine */ int (*S_fp)();
#endif
/* E_fp is for real functions when -R is not specified */
typedef VOID C_f;	/* complex function */
typedef VOID H_f;	/* character function */
typedef VOID Z_f;	/* double complex function */
typedef doublereal E_f;	/* real function with -R not specified */

/* undef any lower-case symbols that your C compiler predefines, e.g.: */

#ifndef Skip_f2c_Undefs
#undef cray
#undef gcos
#undef mc68010
#undef mc68020
#undef mips
#undef pdp11
#undef sgi
#undef sparc
#undef sun
#undef sun2
#undef sun3
#undef sun4
#undef u370
#undef u3b
#undef u3b2
#undef u3b5
#undef unix
#undef vax
#endif
#endif



static int rg_(
integer *nm, integer *n,
doublereal *a, doublereal *wr, doublereal *wi,
integer *matz,
doublereal *z__,
integer *iv1,
doublereal *fv1,
integer *ierr);

/*========================================================================*/

int MatrixEigensolve33d(double *a, double *wr, double *wi, double *v)
{
  integer n,nm;
  integer iv1[3];
  integer matz;
  double fv1[9];
  double at[9];
  integer ierr;
  int x;

  nm = 3;
  n = 3;
  matz = 1;

  for(x=0;x<9;x++) /* make a copy cause eispack trashes the matrix */
	 at[x]=a[x];

  rg_(&nm,&n,at,wr,wi,&matz,v,iv1,fv1,&ierr);

  /* NOTE: the returned eigenvectors are stored one per row which is
	  is actually the inverse of the normal eigenvalue matrix -
     ----
     IS that because we're actually solving the transpose?
  */

  printf(" Eigensolve: eigenvectors\n%8.3f %8.3f %8.3f\n",v[0],v[1],v[2]);
  printf(" Eigensolve: %8.3f %8.3f %8.3f\n",v[3],v[4],v[5]);
  printf(" Eigensolve: %8.3f %8.3f %8.3f\n",v[6],v[7],v[8]);
  
  printf(" Eigensolve: eigenvalues\n%8.3f %8.3f %8.3f\n",wr[0],wr[1],wr[2]);
  printf(" Eigensolve: %8.3f %8.3f %8.3f\n",wi[0],wi[1],wi[2]);
  return(ierr);
}



/*========================================================================*/
/* DON'T GO BELOW HERE - JUST DON'T DO IT - */
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/*========================================================================*/
/* A MIND IS A TERRIBLE THING TO WASTE (on linear algebra) */
/*========================================================================*/

static double d_sign(doublereal *a, doublereal *b);

static int balanc_(integer *nm, integer *n, doublereal *a, integer *low, integer *igh, doublereal *scale);

static int balbak_(
integer *nm, integer *n, integer *low, integer *igh,
doublereal *scale,
integer *m,
doublereal *z__
);

static  int cdiv_(
doublereal *ar, doublereal *ai, doublereal *br, doublereal *bi, doublereal *cr, doublereal *ci);


static  int elmhes_(
integer *nm, integer *n, integer *low, integer *igh,
doublereal *a,
integer *int__);

static  int eltran_(
integer *nm, integer *n, integer *low, integer *igh,
doublereal *a,
integer *int__,
doublereal *z__);

static  int hqr2_(
integer *nm, integer *n,integer *low,integer *igh,
doublereal *h__,doublereal *wr,doublereal *wi,doublereal *z__,
integer *ierr);

static  int hqr_(
integer *nm, integer *n, integer *low, integer *igh,
doublereal *h__, doublereal *wr, doublereal *wi,
integer *ierr);






double d_sign(doublereal *a, doublereal *b)
{
double x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}

/* Table of constant values */

static doublereal c_b126 = 0.;

static int balanc_(nm, n, a, low, igh, scale)
integer *nm, *n;
doublereal *a;
integer *low, *igh;
doublereal *scale;
{
/* System generated locals */
integer a_dim1, a_offset, i__1, i__2;
doublereal d__1;

/* Local variables */
static integer iexc;
static doublereal c__, f, g;
static integer i__, j, k, l, m;
static doublereal r__, s, radix, b2;
static integer jj;
static logical noconv;



/*     this subroutine is a translation of the algol procedure balance, */
/*     num. math. 13, 293-304(1969) by parlett and reinsch. */
/*     handbook for auto. comp., vol.ii-linear algebra, 315-326(1971). */

/*     this subroutine balances a real matrix and isolates */
/*     eigenvalues whenever possible. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        a contains the input matrix to be balanced. */

/*     on output */

/*        a contains the balanced matrix. */

/*        low and igh are two integers such that a(i,j) */
/*          is equal to zero if */
/*           (1) i is greater than j and */
/*           (2) j=1,...,low-1 or i=igh+1,...,n. */

/*        scale contains information determining the */
/*           permutations and scaling factors used. */

/*     suppose that the principal submatrix in rows low through igh */
/*     has been balanced, that p(j) denotes the index interchanged */
/*     with j during the permutation step, and that the elements */
/*     of the diagonal matrix used are denoted by d(i,j).  then */
/*        scale(j) = p(j),    for j = 1,...,low-1 */
/*                 = d(j,j),      j = low,...,igh */
/*                 = p(j)         j = igh+1,...,n. */
/*     the order in which the interchanges are made is n to igh+1, */
/*     then 1 to low-1. */

/*     note that 1 is returned for igh if igh is zero formally. */

/*     the algol procedure exc contained in balance appears in */
/*     balanc  in line.  (note that the algol roles of identifiers */
/*     k,l have been reversed.) */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

/* Parameter adjustments */
--scale;
a_dim1 = *nm;
a_offset = a_dim1 + 1;
a -= a_offset;

/* Function Body */
radix = 16.;

b2 = radix * radix;
k = 1;
l = *n;
goto L100;
/*     .......... in-line procedure for row and */
/*                column exchange .......... */
L20:
scale[m] = (doublereal) j;
if (j == m) {
goto L50;
}

i__1 = l;
for (i__ = 1; i__ <= i__1; ++i__) {
f = a[i__ + j * a_dim1];
a[i__ + j * a_dim1] = a[i__ + m * a_dim1];
a[i__ + m * a_dim1] = f;
/* L30: */
}

i__1 = *n;
for (i__ = k; i__ <= i__1; ++i__) {
f = a[j + i__ * a_dim1];
a[j + i__ * a_dim1] = a[m + i__ * a_dim1];
a[m + i__ * a_dim1] = f;
/* L40: */
}

L50:
switch ((int)iexc) {
case 1:  goto L80;
case 2:  goto L130;
}
/*     .......... search for rows isolating an eigenvalue */
/*                and push them down .......... */
L80:
if (l == 1) {
goto L280;
}
--l;
/*     .......... for j=l step -1 until 1 do -- .......... */
L100:
i__1 = l;
for (jj = 1; jj <= i__1; ++jj) {
j = l + 1 - jj;

i__2 = l;
for (i__ = 1; i__ <= i__2; ++i__) {
    if (i__ == j) {
	goto L110;
    }
    if (a[j + i__ * a_dim1] != 0.) {
	goto L120;
    }
L110:
    ;
}

m = l;
iexc = 1;
goto L20;
L120:
;
}

goto L140;
/*     .......... search for columns isolating an eigenvalue */
/*                and push them left .......... */
L130:
++k;

L140:
i__1 = l;
for (j = k; j <= i__1; ++j) {

i__2 = l;
for (i__ = k; i__ <= i__2; ++i__) {
    if (i__ == j) {
	goto L150;
    }
    if (a[i__ + j * a_dim1] != 0.) {
	goto L170;
    }
L150:
    ;
}

m = k;
iexc = 2;
goto L20;
L170:
;
}
/*     .......... now balance the submatrix in rows k to l .......... */
i__1 = l;
for (i__ = k; i__ <= i__1; ++i__) {
/* L180: */
scale[i__] = 1.;
}
/*     .......... iterative loop for norm reduction .......... */
L190:
noconv = FALSE_;

i__1 = l;
for (i__ = k; i__ <= i__1; ++i__) {
c__ = 0.;
r__ = 0.;

i__2 = l;
for (j = k; j <= i__2; ++j) {
    if (j == i__) {
	goto L200;
    }
    c__ += (d__1 = a[j + i__ * a_dim1], abs(d__1));
    r__ += (d__1 = a[i__ + j * a_dim1], abs(d__1));
L200:
    ;
}
/*     .......... guard against zero c or r due to underflow .........
. */
if (c__ == 0. || r__ == 0.) {
    goto L270;
}
g = r__ / radix;
f = 1.;
s = c__ + r__;
L210:
if (c__ >= g) {
    goto L220;
}
f *= radix;
c__ *= b2;
goto L210;
L220:
g = r__ * radix;
L230:
if (c__ < g) {
    goto L240;
}
f /= radix;
c__ /= b2;
goto L230;
/*     .......... now balance .......... */
L240:
if ((c__ + r__) / f >= s * .95) {
    goto L270;
}
g = 1. / f;
scale[i__] *= f;
noconv = TRUE_;

i__2 = *n;
for (j = k; j <= i__2; ++j) {
/* L250: */
    a[i__ + j * a_dim1] *= g;
}

i__2 = l;
for (j = 1; j <= i__2; ++j) {
/* L260: */
    a[j + i__ * a_dim1] *= f;
}

L270:
;
}

if (noconv) {
goto L190;
}

L280:
*low = k;
*igh = l;
return 0;
} /* balanc_ */

static int balbak_(nm, n, low, igh, scale, m, z__)
integer *nm, *n, *low, *igh;
doublereal *scale;
integer *m;
doublereal *z__;
{
/* System generated locals */
integer z_dim1, z_offset, i__1, i__2;

/* Local variables */
static integer i__, j, k;
static doublereal s;
static integer ii;



/*     this subroutine is a translation of the algol procedure balbak, */
/*     num. math. 13, 293-304(1969) by parlett and reinsch. */
/*     handbook for auto. comp., vol.ii-linear algebra, 315-326(1971). */

/*     this subroutine forms the eigenvectors of a real general */
/*     matrix by back transforming those of the corresponding */
/*     balanced matrix determined by  balanc. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        low and igh are integers determined by  balanc. */

/*        scale contains information determining the permutations */
/*          and scaling factors used by  balanc. */

/*        m is the number of columns of z to be back transformed. */

/*        z contains the real and imaginary parts of the eigen- */
/*          vectors to be back transformed in its first m columns. */

/*     on output */

/*        z contains the real and imaginary parts of the */
/*          transformed eigenvectors in its first m columns. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

/* Parameter adjustments */
--scale;
z_dim1 = *nm;
z_offset = z_dim1 + 1;
z__ -= z_offset;

/* Function Body */
if (*m == 0) {
goto L200;
}
if (*igh == *low) {
goto L120;
}

i__1 = *igh;
for (i__ = *low; i__ <= i__1; ++i__) {
s = scale[i__];
/*     .......... left hand eigenvectors are back transformed */
/*                if the foregoing statement is replaced by */
/*                s=1.0d0/scale(i). .......... */
i__2 = *m;
for (j = 1; j <= i__2; ++j) {
/* L100: */
    z__[i__ + j * z_dim1] *= s;
}

/* L110: */
}
/*     ......... for i=low-1 step -1 until 1, */
/*               igh+1 step 1 until n do -- .......... */
L120:
i__1 = *n;
for (ii = 1; ii <= i__1; ++ii) {
i__ = ii;
if (i__ >= *low && i__ <= *igh) {
    goto L140;
}
if (i__ < *low) {
    i__ = *low - ii;
}
k = (integer) scale[i__];
if (k == i__) {
    goto L140;
}

i__2 = *m;
for (j = 1; j <= i__2; ++j) {
    s = z__[i__ + j * z_dim1];
    z__[i__ + j * z_dim1] = z__[k + j * z_dim1];
    z__[k + j * z_dim1] = s;
/* L130: */
}

L140:
;
}

L200:
return 0;
} /* balbak_ */

static int cdiv_(ar, ai, br, bi, cr, ci)
doublereal *ar, *ai, *br, *bi, *cr, *ci;
{
/* System generated locals */
doublereal d__1, d__2;

/* Local variables */
static doublereal s, ais, bis, ars, brs;


/*     complex division, (cr,ci) = (ar,ai)/(br,bi) */

s = abs(*br) + abs(*bi);
ars = *ar / s;
ais = *ai / s;
brs = *br / s;
bis = *bi / s;
/* Computing 2nd power */
d__1 = brs;
/* Computing 2nd power */
d__2 = bis;
s = d__1 * d__1 + d__2 * d__2;
*cr = (ars * brs + ais * bis) / s;
*ci = (ais * brs - ars * bis) / s;
return 0;
} /* cdiv_ */

static int elmhes_(nm, n, low, igh, a, int__)
integer *nm, *n, *low, *igh;
doublereal *a;
integer *int__;
{
/* System generated locals */
integer a_dim1, a_offset, i__1, i__2, i__3;
doublereal d__1;

/* Local variables */
static integer i__, j, m;
static doublereal x, y;
static integer la, mm1, kp1, mp1;



/*     this subroutine is a translation of the algol procedure elmhes, */
/*     num. math. 12, 349-368(1968) by martin and wilkinson. */
/*     handbook for auto. comp., vol.ii-linear algebra, 339-358(1971). */

/*     given a real general matrix, this subroutine */
/*     reduces a submatrix situated in rows and columns */
/*     low through igh to upper hessenberg form by */
/*     stabilized elementary similarity transformations. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        low and igh are integers determined by the balancing */
/*          subroutine  balanc.  if  balanc  has not been used, */
/*          set low=1, igh=n. */

/*        a contains the input matrix. */

/*     on output */

/*        a contains the hessenberg matrix.  the multipliers */
/*          which were used in the reduction are stored in the */
/*          remaining triangle under the hessenberg matrix. */

/*        int contains information on the rows and columns */
/*          interchanged in the reduction. */
/*          only elements low through igh are used. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

/* Parameter adjustments */
a_dim1 = *nm;
a_offset = a_dim1 + 1;
a -= a_offset;
--int__;

/* Function Body */
la = *igh - 1;
kp1 = *low + 1;
    if (la < kp1) {
	goto L200;
    }

    i__1 = la;
    for (m = kp1; m <= i__1; ++m) {
	mm1 = m - 1;
	x = 0.;
	i__ = m;

	i__2 = *igh;
	for (j = m; j <= i__2; ++j) {
	    if ((d__1 = a[j + mm1 * a_dim1], abs(d__1)) <= abs(x)) {
		goto L100;
	    }
	    x = a[j + mm1 * a_dim1];
	    i__ = j;
L100:
	    ;
	}

	int__[m] = i__;
	if (i__ == m) {
	    goto L130;
	}
/*     .......... interchange rows and columns of a .......... */
	i__2 = *n;
	for (j = mm1; j <= i__2; ++j) {
	    y = a[i__ + j * a_dim1];
	    a[i__ + j * a_dim1] = a[m + j * a_dim1];
	    a[m + j * a_dim1] = y;
/* L110: */
	}

	i__2 = *igh;
	for (j = 1; j <= i__2; ++j) {
	    y = a[j + i__ * a_dim1];
	    a[j + i__ * a_dim1] = a[j + m * a_dim1];
	    a[j + m * a_dim1] = y;
/* L120: */
	}
/*     .......... end interchange .......... */
L130:
	if (x == 0.) {
	    goto L180;
	}
	mp1 = m + 1;

	i__2 = *igh;
	for (i__ = mp1; i__ <= i__2; ++i__) {
	    y = a[i__ + mm1 * a_dim1];
	    if (y == 0.) {
		goto L160;
	    }
	    y /= x;
	    a[i__ + mm1 * a_dim1] = y;

	    i__3 = *n;
	    for (j = m; j <= i__3; ++j) {
/* L140: */
		a[i__ + j * a_dim1] -= y * a[m + j * a_dim1];
	    }

	    i__3 = *igh;
	    for (j = 1; j <= i__3; ++j) {
/* L150: */
		a[j + m * a_dim1] += y * a[j + i__ * a_dim1];
	    }

L160:
	    ;
	}

L180:
	;
    }

L200:
    return 0;
} /* elmhes_ */

static  int eltran_(nm, n, low, igh, a, int__, z__)
integer *nm, *n, *low, *igh;
doublereal *a;
integer *int__;
doublereal *z__;
{
    /* System generated locals */
    integer a_dim1, a_offset, z_dim1, z_offset, i__1, i__2;

    /* Local variables */
    static integer i__, j, kl, mm, mp, mp1;



/*     this subroutine is a translation of the algol procedure elmtrans, 
*/
/*     num. math. 16, 181-204(1970) by peters and wilkinson. */
/*     handbook for auto. comp., vol.ii-linear algebra, 372-395(1971). */

/*     this subroutine accumulates the stabilized elementary */
/*     similarity transformations used in the reduction of a */
/*     real general matrix to upper hessenberg form by  elmhes. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        low and igh are integers determined by the balancing */
/*          subroutine  balanc.  if  balanc  has not been used, */
/*          set low=1, igh=n. */

/*        a contains the multipliers which were used in the */
/*          reduction by  elmhes  in its lower triangle */
/*          below the subdiagonal. */

/*        int contains information on the rows and columns */
/*          interchanged in the reduction by  elmhes. */
/*          only elements low through igh are used. */

/*     on output */

/*        z contains the transformation matrix produced in the */
/*          reduction by  elmhes. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

/*     .......... initialize z to identity matrix .......... */
    /* Parameter adjustments */
    z_dim1 = *nm;
    z_offset = z_dim1 + 1;
    z__ -= z_offset;
    --int__;
    a_dim1 = *nm;
    a_offset = a_dim1 + 1;
    a -= a_offset;

    /* Function Body */
    i__1 = *n;
    for (j = 1; j <= i__1; ++j) {

	i__2 = *n;
	for (i__ = 1; i__ <= i__2; ++i__) {
/* L60: */
	    z__[i__ + j * z_dim1] = 0.;
	}

	z__[j + j * z_dim1] = 1.;
/* L80: */
    }

    kl = *igh - *low - 1;
    if (kl < 1) {
	goto L200;
    }
/*     .......... for mp=igh-1 step -1 until low+1 do -- .......... */
    i__1 = kl;
    for (mm = 1; mm <= i__1; ++mm) {
	mp = *igh - mm;
	mp1 = mp + 1;

	i__2 = *igh;
	for (i__ = mp1; i__ <= i__2; ++i__) {
/* L100: */
	    z__[i__ + mp * z_dim1] = a[i__ + (mp - 1) * a_dim1];
	}

	i__ = int__[mp];
	if (i__ == mp) {
	    goto L140;
	}

	i__2 = *igh;
	for (j = mp; j <= i__2; ++j) {
	    z__[mp + j * z_dim1] = z__[i__ + j * z_dim1];
	    z__[i__ + j * z_dim1] = 0.;
/* L130: */
	}

	z__[i__ + mp * z_dim1] = 1.;
L140:
	;
    }

L200:
    return 0;
} /* eltran_ */

static int hqr_(nm, n, low, igh, h__, wr, wi, ierr)
integer *nm, *n, *low, *igh;
doublereal *h__, *wr, *wi;
integer *ierr;
{
    /* System generated locals */
    integer h_dim1, h_offset, i__1, i__2, i__3;
    doublereal d__1, d__2;

    /* Local variables */
    static doublereal norm;
    static integer i__, j, k, l, m;
    static doublereal p, q, r__, s, t, w, x, y;
    static integer na, en, ll, mm;
    static doublereal zz;
    static logical notlas;
    static integer mp2, itn, its, enm2;
    static doublereal tst1, tst2;

/*  RESTORED CORRECT INDICES OF LOOPS (200,210,230,240). (9/29/89 BSG) */


/*     this subroutine is a translation of the algol procedure hqr, */
/*     num. math. 14, 219-231(1970) by martin, peters, and wilkinson. */
/*     handbook for auto. comp., vol.ii-linear algebra, 359-371(1971). */

/*     this subroutine finds the eigenvalues of a real */
/*     upper hessenberg matrix by the qr method. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        low and igh are integers determined by the balancing */
/*          subroutine  balanc.  if  balanc  has not been used, */
/*          set low=1, igh=n. */

/*        h contains the upper hessenberg matrix.  information about */
/*          the transformations used in the reduction to hessenberg */
/*          form by  elmhes  or  orthes, if performed, is stored */
/*          in the remaining triangle under the hessenberg matrix. */

/*     on output */

/*        h has been destroyed.  therefore, it must be saved */
/*          before calling  hqr  if subsequent calculation and */
/*          back transformation of eigenvectors is to be performed. */

/*        wr and wi contain the real and imaginary parts, */
/*          respectively, of the eigenvalues.  the eigenvalues */
/*          are unordered except that complex conjugate pairs */
/*          of values appear consecutively with the eigenvalue */
/*          having the positive imaginary part first.  if an */
/*          error exit is made, the eigenvalues should be correct */
/*          for indices ierr+1,...,n. */

/*        ierr is set to */
/*          zero       for normal return, */
/*          j          if the limit of 30*n iterations is exhausted */
/*                     while the j-th eigenvalue is being sought. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated september 1989. */

/*     ------------------------------------------------------------------ 
*/

    /* Parameter adjustments */
    --wi;
    --wr;
    h_dim1 = *nm;
    h_offset = h_dim1 + 1;
    h__ -= h_offset;

    /* Function Body */
    *ierr = 0;
    norm = 0.;
    k = 1;
/*     .......... store roots isolated by balanc */
/*                and compute matrix norm .......... */
    i__1 = *n;
    for (i__ = 1; i__ <= i__1; ++i__) {

	i__2 = *n;
	for (j = k; j <= i__2; ++j) {
/* L40: */
	    norm += (d__1 = h__[i__ + j * h_dim1], abs(d__1));
	}

	k = i__;
	if (i__ >= *low && i__ <= *igh) {
	    goto L50;
	}
	wr[i__] = h__[i__ + i__ * h_dim1];
	wi[i__] = 0.;
L50:
	;
    }

    en = *igh;
    t = 0.;
    itn = *n * 30;
/*     .......... search for next eigenvalues .......... */
L60:
    if (en < *low) {
	goto L1001;
    }
    its = 0;
    na = en - 1;
    enm2 = na - 1;
/*     .......... look for single small sub-diagonal element */
/*                for l=en step -1 until low do -- .......... */
L70:
    i__1 = en;
    for (ll = *low; ll <= i__1; ++ll) {
	l = en + *low - ll;
	if (l == *low) {
	    goto L100;
	}
	s = (d__1 = h__[l - 1 + (l - 1) * h_dim1], abs(d__1)) + (d__2 = h__[l 
		+ l * h_dim1], abs(d__2));
	if (s == 0.) {
	    s = norm;
	}
	tst1 = s;
	tst2 = tst1 + (d__1 = h__[l + (l - 1) * h_dim1], abs(d__1));
	if (tst2 == tst1) {
	    goto L100;
	}
/* L80: */
    }
/*     .......... form shift .......... */
L100:
    x = h__[en + en * h_dim1];
    if (l == en) {
	goto L270;
    }
    y = h__[na + na * h_dim1];
    w = h__[en + na * h_dim1] * h__[na + en * h_dim1];
    if (l == na) {
	goto L280;
    }
    if (itn == 0) {
	goto L1000;
    }
    if (its != 10 && its != 20) {
	goto L130;
    }
/*     .......... form exceptional shift .......... */
    t += x;

    i__1 = en;
    for (i__ = *low; i__ <= i__1; ++i__) {
/* L120: */
	h__[i__ + i__ * h_dim1] -= x;
    }

    s = (d__1 = h__[en + na * h_dim1], abs(d__1)) + (d__2 = h__[na + enm2 * 
	    h_dim1], abs(d__2));
    x = s * .75;
    y = x;
    w = s * -.4375 * s;
L130:
    ++its;
    --itn;
/*     .......... look for two consecutive small */
/*                sub-diagonal elements. */
/*                for m=en-2 step -1 until l do -- .......... */
    i__1 = enm2;
    for (mm = l; mm <= i__1; ++mm) {
	m = enm2 + l - mm;
	zz = h__[m + m * h_dim1];
	r__ = x - zz;
	s = y - zz;
	p = (r__ * s - w) / h__[m + 1 + m * h_dim1] + h__[m + (m + 1) * 
		h_dim1];
	q = h__[m + 1 + (m + 1) * h_dim1] - zz - r__ - s;
	r__ = h__[m + 2 + (m + 1) * h_dim1];
	s = abs(p) + abs(q) + abs(r__);
	p /= s;
	q /= s;
	r__ /= s;
	if (m == l) {
	    goto L150;
	}
	tst1 = abs(p) * ((d__1 = h__[m - 1 + (m - 1) * h_dim1], abs(d__1)) + 
		abs(zz) + (d__2 = h__[m + 1 + (m + 1) * h_dim1], abs(d__2)));
	tst2 = tst1 + (d__1 = h__[m + (m - 1) * h_dim1], abs(d__1)) * (abs(q) 
		+ abs(r__));
	if (tst2 == tst1) {
	    goto L150;
	}
/* L140: */
    }

L150:
    mp2 = m + 2;

    i__1 = en;
    for (i__ = mp2; i__ <= i__1; ++i__) {
	h__[i__ + (i__ - 2) * h_dim1] = 0.;
	if (i__ == mp2) {
	    goto L160;
	}
	h__[i__ + (i__ - 3) * h_dim1] = 0.;
L160:
	;
    }
/*     .......... double qr step involving rows l to en and */
/*                columns m to en .......... */
    i__1 = na;
    for (k = m; k <= i__1; ++k) {
	notlas = k != na;
	if (k == m) {
	    goto L170;
	}
	p = h__[k + (k - 1) * h_dim1];
	q = h__[k + 1 + (k - 1) * h_dim1];
	r__ = 0.;
	if (notlas) {
	    r__ = h__[k + 2 + (k - 1) * h_dim1];
	}
	x = abs(p) + abs(q) + abs(r__);
	if (x == 0.) {
	    goto L260;
	}
	p /= x;
	q /= x;
	r__ /= x;
L170:
	d__1 = sqrt(p * p + q * q + r__ * r__);
	s = d_sign(&d__1, &p);
	if (k == m) {
	    goto L180;
	}
	h__[k + (k - 1) * h_dim1] = -s * x;
	goto L190;
L180:
	if (l != m) {
	    h__[k + (k - 1) * h_dim1] = -h__[k + (k - 1) * h_dim1];
	}
L190:
	p += s;
	x = p / s;
	y = q / s;
	zz = r__ / s;
	q /= p;
	r__ /= p;
	if (notlas) {
	    goto L225;
	}
/*     .......... row modification .......... */
	i__2 = en;
	for (j = k; j <= i__2; ++j) {
	    p = h__[k + j * h_dim1] + q * h__[k + 1 + j * h_dim1];
	    h__[k + j * h_dim1] -= p * x;
	    h__[k + 1 + j * h_dim1] -= p * y;
/* L200: */
	}

/* Computing MIN */
	i__2 = en, i__3 = k + 3;
	j = mymin(i__2,i__3);
/*     .......... column modification .......... */
	i__2 = j;
	for (i__ = l; i__ <= i__2; ++i__) {
	    p = x * h__[i__ + k * h_dim1] + y * h__[i__ + (k + 1) * h_dim1];
	    h__[i__ + k * h_dim1] -= p;
	    h__[i__ + (k + 1) * h_dim1] -= p * q;
/* L210: */
	}
	goto L255;
L225:
/*     .......... row modification .......... */
	i__2 = en;
	for (j = k; j <= i__2; ++j) {
	    p = h__[k + j * h_dim1] + q * h__[k + 1 + j * h_dim1] + r__ * h__[
		    k + 2 + j * h_dim1];
	    h__[k + j * h_dim1] -= p * x;
	    h__[k + 1 + j * h_dim1] -= p * y;
	    h__[k + 2 + j * h_dim1] -= p * zz;
/* L230: */
	}

/* Computing MIN */
	i__2 = en, i__3 = k + 3;
	j = mymin(i__2,i__3);
/*     .......... column modification .......... */
	i__2 = j;
	for (i__ = l; i__ <= i__2; ++i__) {
	    p = x * h__[i__ + k * h_dim1] + y * h__[i__ + (k + 1) * h_dim1] + 
		    zz * h__[i__ + (k + 2) * h_dim1];
	    h__[i__ + k * h_dim1] -= p;
	    h__[i__ + (k + 1) * h_dim1] -= p * q;
	    h__[i__ + (k + 2) * h_dim1] -= p * r__;
/* L240: */
	}
L255:

L260:
	;
    }

    goto L70;
/*     .......... one root found .......... */
L270:
    wr[en] = x + t;
    wi[en] = 0.;
    en = na;
    goto L60;
/*     .......... two roots found .......... */
L280:
    p = (y - x) / 2.;
    q = p * p + w;
    zz = sqrt((abs(q)));
    x += t;
    if (q < 0.) {
	goto L320;
    }
/*     .......... real pair .......... */
    zz = p + d_sign(&zz, &p);
    wr[na] = x + zz;
    wr[en] = wr[na];
    if (zz != 0.) {
	wr[en] = x - w / zz;
    }
    wi[na] = 0.;
    wi[en] = 0.;
    goto L330;
/*     .......... complex pair .......... */
L320:
    wr[na] = x + p;
    wr[en] = x + p;
    wi[na] = zz;
    wi[en] = -zz;
L330:
    en = enm2;
    goto L60;
/*     .......... set error -- all eigenvalues have not */
/*                converged after 30*n iterations .......... */
L1000:
    *ierr = en;
L1001:
    return 0;
} /* hqr_ */

static int hqr2_(nm, n, low, igh, h__, wr, wi, z__, ierr)
integer *nm, *n, *low, *igh;
doublereal *h__, *wr, *wi, *z__;
integer *ierr;
{
    /* System generated locals */
    integer h_dim1, h_offset, z_dim1, z_offset, i__1, i__2, i__3;
    doublereal d__1, d__2, d__3, d__4;


    /* Local variables */
    static doublereal norm;
    static integer i__, j, k, l, m;
    static doublereal p, q, r__, s, t, w, x, y;
    static integer na, ii, en, jj;
    static doublereal ra, sa;
    static integer ll, mm, nn;
    static doublereal vi, vr, zz;
    static logical notlas;
    static integer mp2, itn, its, enm2;
    static doublereal tst1, tst2;



/*     this subroutine is a translation of the algol procedure hqr2, */
/*     num. math. 16, 181-204(1970) by peters and wilkinson. */
/*     handbook for auto. comp., vol.ii-linear algebra, 372-395(1971). */

/*     this subroutine finds the eigenvalues and eigenvectors */
/*     of a real upper hessenberg matrix by the qr method.  the */
/*     eigenvectors of a real general matrix can also be found */
/*     if  elmhes  and  eltran  or  orthes  and  ortran  have */
/*     been used to reduce this general matrix to hessenberg form */
/*     and to accumulate the similarity transformations. */

/*     on input */

/*        nm must be set to the row dimension of two-dimensional */
/*          array parameters as declared in the calling program */
/*          dimension statement. */

/*        n is the order of the matrix. */

/*        low and igh are integers determined by the balancing */
/*          subroutine  balanc.  if  balanc  has not been used, */
/*          set low=1, igh=n. */

/*        h contains the upper hessenberg matrix. */

/*        z contains the transformation matrix produced by  eltran */
/*          after the reduction by  elmhes, or by  ortran  after the */
/*          reduction by  orthes, if performed.  if the eigenvectors */
/*          of the hessenberg matrix are desired, z must contain the */
/*          identity matrix. */

/*     on output */

/*        h has been destroyed. */

/*        wr and wi contain the real and imaginary parts, */
/*          respectively, of the eigenvalues.  the eigenvalues */
/*          are unordered except that complex conjugate pairs */
/*          of values appear consecutively with the eigenvalue */
/*          having the positive imaginary part first.  if an */
/*          error exit is made, the eigenvalues should be correct */
/*          for indices ierr+1,...,n. */

/*        z contains the real and imaginary parts of the eigenvectors. */
/*          if the i-th eigenvalue is real, the i-th column of z */
/*          contains its eigenvector.  if the i-th eigenvalue is complex 
*/
/*          with positive imaginary part, the i-th and (i+1)-th */
/*          columns of z contain the real and imaginary parts of its */
/*          eigenvector.  the eigenvectors are unnormalized.  if an */
/*          error exit is made, none of the eigenvectors has been found. 
*/

/*        ierr is set to */
/*          zero       for normal return, */
/*          j          if the limit of 30*n iterations is exhausted */
/*                     while the j-th eigenvalue is being sought. */

/*     calls cdiv for complex division. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

    /* Parameter adjustments */
    z_dim1 = *nm;
    z_offset = z_dim1 + 1;
    z__ -= z_offset;
    --wi;
    --wr;
    h_dim1 = *nm;
    h_offset = h_dim1 + 1;
    h__ -= h_offset;

    /* Function Body */
    *ierr = 0;
    norm = 0.;
    k = 1;
/*     .......... store roots isolated by balanc */
/*                and compute matrix norm .......... */
    i__1 = *n;
    for (i__ = 1; i__ <= i__1; ++i__) {

	i__2 = *n;
	for (j = k; j <= i__2; ++j) {
/* L40: */
	    norm += (d__1 = h__[i__ + j * h_dim1], abs(d__1));
	}

	k = i__;
	if (i__ >= *low && i__ <= *igh) {
	    goto L50;
	}
	wr[i__] = h__[i__ + i__ * h_dim1];
	wi[i__] = 0.;
L50:
	;
    }

    en = *igh;
    t = 0.;
    itn = *n * 30;
/*     .......... search for next eigenvalues .......... */
L60:
    if (en < *low) {
	goto L340;
    }
    its = 0;
    na = en - 1;
    enm2 = na - 1;
/*     .......... look for single small sub-diagonal element */
/*                for l=en step -1 until low do -- .......... */
L70:
    i__1 = en;
    for (ll = *low; ll <= i__1; ++ll) {
	l = en + *low - ll;
	if (l == *low) {
	    goto L100;
	}
	s = (d__1 = h__[l - 1 + (l - 1) * h_dim1], abs(d__1)) + (d__2 = h__[l 
		+ l * h_dim1], abs(d__2));
	if (s == 0.) {
	    s = norm;
	}
	tst1 = s;
	tst2 = tst1 + (d__1 = h__[l + (l - 1) * h_dim1], abs(d__1));
	if (tst2 == tst1) {
	    goto L100;
	}
/* L80: */
    }
/*     .......... form shift .......... */
L100:
    x = h__[en + en * h_dim1];
    if (l == en) {
	goto L270;
    }
    y = h__[na + na * h_dim1];
    w = h__[en + na * h_dim1] * h__[na + en * h_dim1];
    if (l == na) {
	goto L280;
    }
    if (itn == 0) {
	goto L1000;
    }
    if (its != 10 && its != 20) {
	goto L130;
    }
/*     .......... form exceptional shift .......... */
    t += x;

    i__1 = en;
    for (i__ = *low; i__ <= i__1; ++i__) {
/* L120: */
	h__[i__ + i__ * h_dim1] -= x;
    }

    s = (d__1 = h__[en + na * h_dim1], abs(d__1)) + (d__2 = h__[na + enm2 * 
	    h_dim1], abs(d__2));
    x = s * .75;
    y = x;
    w = s * -.4375 * s;
L130:
    ++its;
    --itn;
/*     .......... look for two consecutive small */
/*                sub-diagonal elements. */
/*                for m=en-2 step -1 until l do -- .......... */
    i__1 = enm2;
    for (mm = l; mm <= i__1; ++mm) {
	m = enm2 + l - mm;
	zz = h__[m + m * h_dim1];
	r__ = x - zz;
	s = y - zz;
	p = (r__ * s - w) / h__[m + 1 + m * h_dim1] + h__[m + (m + 1) * 
		h_dim1];
	q = h__[m + 1 + (m + 1) * h_dim1] - zz - r__ - s;
	r__ = h__[m + 2 + (m + 1) * h_dim1];
	s = abs(p) + abs(q) + abs(r__);
	p /= s;
	q /= s;
	r__ /= s;
	if (m == l) {
	    goto L150;
	}
	tst1 = abs(p) * ((d__1 = h__[m - 1 + (m - 1) * h_dim1], abs(d__1)) + 
		abs(zz) + (d__2 = h__[m + 1 + (m + 1) * h_dim1], abs(d__2)));
	tst2 = tst1 + (d__1 = h__[m + (m - 1) * h_dim1], abs(d__1)) * (abs(q) 
		+ abs(r__));
	if (tst2 == tst1) {
	    goto L150;
	}
/* L140: */
    }

L150:
    mp2 = m + 2;

    i__1 = en;
    for (i__ = mp2; i__ <= i__1; ++i__) {
	h__[i__ + (i__ - 2) * h_dim1] = 0.;
	if (i__ == mp2) {
	    goto L160;
	}
	h__[i__ + (i__ - 3) * h_dim1] = 0.;
L160:
	;
    }
/*     .......... double qr step involving rows l to en and */
/*                columns m to en .......... */
    i__1 = na;
    for (k = m; k <= i__1; ++k) {
	notlas = k != na;
	if (k == m) {
	    goto L170;
	}
	p = h__[k + (k - 1) * h_dim1];
	q = h__[k + 1 + (k - 1) * h_dim1];
	r__ = 0.;
	if (notlas) {
	    r__ = h__[k + 2 + (k - 1) * h_dim1];
	}
	x = abs(p) + abs(q) + abs(r__);
	if (x == 0.) {
	    goto L260;
	}
	p /= x;
	q /= x;
	r__ /= x;
L170:
	d__1 = sqrt(p * p + q * q + r__ * r__);
	s = d_sign(&d__1, &p);
	if (k == m) {
	    goto L180;
	}
	h__[k + (k - 1) * h_dim1] = -s * x;
	goto L190;
L180:
	if (l != m) {
	    h__[k + (k - 1) * h_dim1] = -h__[k + (k - 1) * h_dim1];
	}
L190:
	p += s;
	x = p / s;
	y = q / s;
	zz = r__ / s;
	q /= p;
	r__ /= p;
	if (notlas) {
	    goto L225;
	}
/*     .......... row modification .......... */
	i__2 = *n;
	for (j = k; j <= i__2; ++j) {
	    p = h__[k + j * h_dim1] + q * h__[k + 1 + j * h_dim1];
	    h__[k + j * h_dim1] -= p * x;
	    h__[k + 1 + j * h_dim1] -= p * y;
/* L200: */
	}

/* Computing MIN */
	i__2 = en, i__3 = k + 3;
	j = mymin(i__2,i__3);
/*     .......... column modification .......... */
	i__2 = j;
	for (i__ = 1; i__ <= i__2; ++i__) {
	    p = x * h__[i__ + k * h_dim1] + y * h__[i__ + (k + 1) * h_dim1];
	    h__[i__ + k * h_dim1] -= p;
	    h__[i__ + (k + 1) * h_dim1] -= p * q;
/* L210: */
	}
/*     .......... accumulate transformations .......... */
	i__2 = *igh;
	for (i__ = *low; i__ <= i__2; ++i__) {
	    p = x * z__[i__ + k * z_dim1] + y * z__[i__ + (k + 1) * z_dim1];
	    z__[i__ + k * z_dim1] -= p;
	    z__[i__ + (k + 1) * z_dim1] -= p * q;
/* L220: */
	}
	goto L255;
L225:
/*     .......... row modification .......... */
	i__2 = *n;
	for (j = k; j <= i__2; ++j) {
	    p = h__[k + j * h_dim1] + q * h__[k + 1 + j * h_dim1] + r__ * h__[
		    k + 2 + j * h_dim1];
	    h__[k + j * h_dim1] -= p * x;
	    h__[k + 1 + j * h_dim1] -= p * y;
	    h__[k + 2 + j * h_dim1] -= p * zz;
/* L230: */
	}

/* Computing MIN */
	i__2 = en, i__3 = k + 3;
	j = mymin(i__2,i__3);
/*     .......... column modification .......... */
	i__2 = j;
	for (i__ = 1; i__ <= i__2; ++i__) {
	    p = x * h__[i__ + k * h_dim1] + y * h__[i__ + (k + 1) * h_dim1] + 
		    zz * h__[i__ + (k + 2) * h_dim1];
	    h__[i__ + k * h_dim1] -= p;
	    h__[i__ + (k + 1) * h_dim1] -= p * q;
	    h__[i__ + (k + 2) * h_dim1] -= p * r__;
/* L240: */
	}
/*     .......... accumulate transformations .......... */
	i__2 = *igh;
	for (i__ = *low; i__ <= i__2; ++i__) {
	    p = x * z__[i__ + k * z_dim1] + y * z__[i__ + (k + 1) * z_dim1] + 
		    zz * z__[i__ + (k + 2) * z_dim1];
	    z__[i__ + k * z_dim1] -= p;
	    z__[i__ + (k + 1) * z_dim1] -= p * q;
	    z__[i__ + (k + 2) * z_dim1] -= p * r__;
/* L250: */
	}
L255:

L260:
	;
    }

    goto L70;
/*     .......... one root found .......... */
L270:
    h__[en + en * h_dim1] = x + t;
    wr[en] = h__[en + en * h_dim1];
    wi[en] = 0.;
    en = na;
    goto L60;
/*     .......... two roots found .......... */
L280:
    p = (y - x) / 2.;
    q = p * p + w;
    zz = sqrt((abs(q)));
    h__[en + en * h_dim1] = x + t;
    x = h__[en + en * h_dim1];
    h__[na + na * h_dim1] = y + t;
    if (q < 0.) {
	goto L320;
    }
/*     .......... real pair .......... */
    zz = p + d_sign(&zz, &p);
    wr[na] = x + zz;
    wr[en] = wr[na];
    if (zz != 0.) {
	wr[en] = x - w / zz;
    }
    wi[na] = 0.;
    wi[en] = 0.;
    x = h__[en + na * h_dim1];
    s = abs(x) + abs(zz);
    p = x / s;
    q = zz / s;
    r__ = sqrt(p * p + q * q);
    p /= r__;
    q /= r__;
/*     .......... row modification .......... */
    i__1 = *n;
    for (j = na; j <= i__1; ++j) {
	zz = h__[na + j * h_dim1];
	h__[na + j * h_dim1] = q * zz + p * h__[en + j * h_dim1];
	h__[en + j * h_dim1] = q * h__[en + j * h_dim1] - p * zz;
/* L290: */
    }
/*     .......... column modification .......... */
    i__1 = en;
    for (i__ = 1; i__ <= i__1; ++i__) {
	zz = h__[i__ + na * h_dim1];
	h__[i__ + na * h_dim1] = q * zz + p * h__[i__ + en * h_dim1];
	h__[i__ + en * h_dim1] = q * h__[i__ + en * h_dim1] - p * zz;
/* L300: */
    }
/*     .......... accumulate transformations .......... */
    i__1 = *igh;
    for (i__ = *low; i__ <= i__1; ++i__) {
	zz = z__[i__ + na * z_dim1];
	z__[i__ + na * z_dim1] = q * zz + p * z__[i__ + en * z_dim1];
	z__[i__ + en * z_dim1] = q * z__[i__ + en * z_dim1] - p * zz;
/* L310: */
    }

    goto L330;
/*     .......... complex pair .......... */
L320:
    wr[na] = x + p;
    wr[en] = x + p;
    wi[na] = zz;
    wi[en] = -zz;
L330:
    en = enm2;
    goto L60;
/*     .......... all roots found.  backsubstitute to find */
/*                vectors of upper triangular form .......... */
L340:
    if (norm == 0.) {
	goto L1001;
    }
/*     .......... for en=n step -1 until 1 do -- .......... */
    i__1 = *n;
    for (nn = 1; nn <= i__1; ++nn) {
	en = *n + 1 - nn;
	p = wr[en];
	q = wi[en];
	na = en - 1;
	if (q < 0.) {
	    goto L710;
	} else if (q == 0) {
	    goto L600;
	} else {
	    goto L800;
	}
/*     .......... real vector .......... */
L600:
	m = en;
	h__[en + en * h_dim1] = 1.;
	if (na == 0) {
	    goto L800;
	}
/*     .......... for i=en-1 step -1 until 1 do -- .......... */
	i__2 = na;
	for (ii = 1; ii <= i__2; ++ii) {
	    i__ = en - ii;
	    w = h__[i__ + i__ * h_dim1] - p;
	    r__ = 0.;

	    i__3 = en;
	    for (j = m; j <= i__3; ++j) {
/* L610: */
		r__ += h__[i__ + j * h_dim1] * h__[j + en * h_dim1];
	    }

	    if (wi[i__] >= 0.) {
		goto L630;
	    }
	    zz = w;
	    s = r__;
	    goto L700;
L630:
	    m = i__;
	    if (wi[i__] != 0.) {
		goto L640;
	    }
	    t = w;
	    if (t != 0.) {
		goto L635;
	    }
	    tst1 = norm;
	    t = tst1;
L632:
	    t *= .01;
	    tst2 = norm + t;
	    if (tst2 > tst1) {
		goto L632;
	    }
L635:
	    h__[i__ + en * h_dim1] = -r__ / t;
	    goto L680;
/*     .......... solve real equations .......... */
L640:
	    x = h__[i__ + (i__ + 1) * h_dim1];
	    y = h__[i__ + 1 + i__ * h_dim1];
	    q = (wr[i__] - p) * (wr[i__] - p) + wi[i__] * wi[i__];
	    t = (x * s - zz * r__) / q;
	    h__[i__ + en * h_dim1] = t;
	    if (abs(x) <= abs(zz)) {
		goto L650;
	    }
	    h__[i__ + 1 + en * h_dim1] = (-r__ - w * t) / x;
	    goto L680;
L650:
	    h__[i__ + 1 + en * h_dim1] = (-s - y * t) / zz;

/*     .......... overflow control .......... */
L680:
	    t = (d__1 = h__[i__ + en * h_dim1], abs(d__1));
	    if (t == 0.) {
		goto L700;
	    }
	    tst1 = t;
	    tst2 = tst1 + 1. / tst1;
	    if (tst2 > tst1) {
		goto L700;
	    }
	    i__3 = en;
	    for (j = i__; j <= i__3; ++j) {
		h__[j + en * h_dim1] /= t;
/* L690: */
	    }

L700:
	    ;
	}
/*     .......... end real vector .......... */
	goto L800;
/*     .......... complex vector .......... */
L710:
	m = na;
/*     .......... last vector component chosen imaginary so that */
/*                eigenvector matrix is triangular .......... */
	if ((d__1 = h__[en + na * h_dim1], abs(d__1)) <= (d__2 = h__[na + en *
		 h_dim1], abs(d__2))) {
	    goto L720;
	}
	h__[na + na * h_dim1] = q / h__[en + na * h_dim1];
	h__[na + en * h_dim1] = -(h__[en + en * h_dim1] - p) / h__[en + na * 
		h_dim1];
	goto L730;
L720:
	d__1 = -h__[na + en * h_dim1];
	d__2 = h__[na + na * h_dim1] - p;
	cdiv_(&c_b126, &d__1, &d__2, &q, &h__[na + na * h_dim1], &h__[na + en 
		* h_dim1]);
L730:
	h__[en + na * h_dim1] = 0.;
	h__[en + en * h_dim1] = 1.;
	enm2 = na - 1;
	if (enm2 == 0) {
	    goto L800;
	}
/*     .......... for i=en-2 step -1 until 1 do -- .......... */
	i__2 = enm2;
	for (ii = 1; ii <= i__2; ++ii) {
	    i__ = na - ii;
	    w = h__[i__ + i__ * h_dim1] - p;
	    ra = 0.;
	    sa = 0.;

	    i__3 = en;
	    for (j = m; j <= i__3; ++j) {
		ra += h__[i__ + j * h_dim1] * h__[j + na * h_dim1];
		sa += h__[i__ + j * h_dim1] * h__[j + en * h_dim1];
/* L760: */
	    }

	    if (wi[i__] >= 0.) {
		goto L770;
	    }
	    zz = w;
	    r__ = ra;
	    s = sa;
	    goto L795;
L770:
	    m = i__;
	    if (wi[i__] != 0.) {
		goto L780;
	    }
	    d__1 = -ra;
	    d__2 = -sa;
	    cdiv_(&d__1, &d__2, &w, &q, &h__[i__ + na * h_dim1], &h__[i__ + 
		    en * h_dim1]);
	    goto L790;
/*     .......... solve complex equations .......... */
L780:
	    x = h__[i__ + (i__ + 1) * h_dim1];
	    y = h__[i__ + 1 + i__ * h_dim1];
	    vr = (wr[i__] - p) * (wr[i__] - p) + wi[i__] * wi[i__] - q * q;
	    vi = (wr[i__] - p) * 2. * q;
	    if (vr != 0. || vi != 0.) {
		goto L784;
	    }
	    tst1 = norm * (abs(w) + abs(q) + abs(x) + abs(y) + abs(zz));
	    vr = tst1;
L783:
	    vr *= .01;
	    tst2 = tst1 + vr;
	    if (tst2 > tst1) {
		goto L783;
	    }
L784:
	    d__1 = x * r__ - zz * ra + q * sa;
	    d__2 = x * s - zz * sa - q * ra;
	    cdiv_(&d__1, &d__2, &vr, &vi, &h__[i__ + na * h_dim1], &h__[i__ + 
		    en * h_dim1]);
	    if (abs(x) <= abs(zz) + abs(q)) {
		goto L785;
	    }
	    h__[i__ + 1 + na * h_dim1] = (-ra - w * h__[i__ + na * h_dim1] + 
		    q * h__[i__ + en * h_dim1]) / x;
	    h__[i__ + 1 + en * h_dim1] = (-sa - w * h__[i__ + en * h_dim1] - 
		    q * h__[i__ + na * h_dim1]) / x;
	    goto L790;
L785:
	    d__1 = -r__ - y * h__[i__ + na * h_dim1];
	    d__2 = -s - y * h__[i__ + en * h_dim1];
	    cdiv_(&d__1, &d__2, &zz, &q, &h__[i__ + 1 + na * h_dim1], &h__[
		    i__ + 1 + en * h_dim1]);

/*     .......... overflow control .......... */
L790:
/* Computing MAX */
	    d__3 = (d__1 = h__[i__ + na * h_dim1], abs(d__1)), d__4 = (d__2 = 
		    h__[i__ + en * h_dim1], abs(d__2));
	    t = mymax(d__3,d__4);
	    if (t == 0.) {
		goto L795;
	    }
	    tst1 = t;
	    tst2 = tst1 + 1. / tst1;
	    if (tst2 > tst1) {
		goto L795;
	    }
	    i__3 = en;
	    for (j = i__; j <= i__3; ++j) {
		h__[j + na * h_dim1] /= t;
		h__[j + en * h_dim1] /= t;
/* L792: */
	    }

L795:
	    ;
	}
/*     .......... end complex vector .......... */
L800:
	;
    }
/*     .......... end back substitution. */
/*                vectors of isolated roots .......... */
    i__1 = *n;
    for (i__ = 1; i__ <= i__1; ++i__) {
	if (i__ >= *low && i__ <= *igh) {
	    goto L840;
	}

	i__2 = *n;
	for (j = i__; j <= i__2; ++j) {
/* L820: */
	    z__[i__ + j * z_dim1] = h__[i__ + j * h_dim1];
	}

L840:
	;
    }
/*     .......... multiply by transformation matrix to give */
/*                vectors of original full matrix. */
/*                for j=n step -1 until low do -- .......... */
    i__1 = *n;
    for (jj = *low; jj <= i__1; ++jj) {
	j = *n + *low - jj;
	m = mymin(j,*igh);

	i__2 = *igh;
	for (i__ = *low; i__ <= i__2; ++i__) {
	    zz = 0.;

	    i__3 = m;
	    for (k = *low; k <= i__3; ++k) {
/* L860: */
		zz += z__[i__ + k * z_dim1] * h__[k + j * h_dim1];
	    }

	    z__[i__ + j * z_dim1] = zz;
/* L880: */
	}
    }

    goto L1001;
/*     .......... set error -- all eigenvalues have not */
/*                converged after 30*n iterations .......... */
L1000:
    *ierr = en;
L1001:
    return 0;
} /* hqr2_ */

static int rg_(nm, n, a, wr, wi, matz, z__, iv1, fv1, ierr)
integer *nm, *n;
doublereal *a, *wr, *wi;
integer *matz;
doublereal *z__;
integer *iv1;
doublereal *fv1;
integer *ierr;
{
    /* System generated locals */
    integer a_dim1, a_offset, z_dim1, z_offset;

    /* Local variables */
    static integer is1, is2;

/*     this subroutine calls the recommended sequence of */
/*     subroutines from the eigensystem subroutine package (eispack) */
/*     to find the eigenvalues and eigenvectors (if desired) */
/*     of a real general matrix. */

/*     on input */

/*        nm  must be set to the row dimension of the two-dimensional */
/*        array parameters as declared in the calling program */
/*        dimension statement. */

/*        n  is the order of the matrix  a. */

/*        a  contains the real general matrix. */

/*        matz  is an integer variable set equal to zero if */
/*        only eigenvalues are desired.  otherwise it is set to */
/*        any non-zero integer for both eigenvalues and eigenvectors. */

/*     on output */

/*        wr  and  wi  contain the real and imaginary parts, */
/*        respectively, of the eigenvalues.  complex conjugate */
/*        pairs of eigenvalues appear consecutively with the */
/*        eigenvalue having the positive imaginary part first. */

/*        z  contains the real and imaginary parts of the eigenvectors */
/*        if matz is not zero.  if the j-th eigenvalue is real, the */
/*        j-th column of  z  contains its eigenvector.  if the j-th */
/*        eigenvalue is complex with positive imaginary part, the */
/*        j-th and (j+1)-th columns of  z  contain the real and */
/*        imaginary parts of its eigenvector.  the conjugate of this */
/*        vector is the eigenvector for the conjugate eigenvalue. */

/*        ierr  is an integer output variable set equal to an error */
/*           completion code described in the documentation for hqr */
/*           and hqr2.  the normal completion code is zero. */

/*        iv1  and  fv1  are temporary storage arrays. */

/*     questions and comments should be directed to burton s. garbow, */
/*     mathematics and computer science div, argonne national laboratory 
*/

/*     this version dated august 1983. */

/*     ------------------------------------------------------------------ 
*/

    /* Parameter adjustments */
    --fv1;
    --iv1;
    z_dim1 = *nm;
    z_offset = z_dim1 + 1;
    z__ -= z_offset;
    --wi;
    --wr;
    a_dim1 = *nm;
    a_offset = a_dim1 + 1;
    a -= a_offset;

    /* Function Body */
    if (*n <= *nm) {
	goto L10;
    }
    *ierr = *n * 10;
    goto L50;

L10:
    balanc_(nm, n, &a[a_offset], &is1, &is2, &fv1[1]);
    elmhes_(nm, n, &is1, &is2, &a[a_offset], &iv1[1]);
    if (*matz != 0) {
	goto L20;
    }
/*     .......... find eigenvalues only .......... */
    hqr_(nm, n, &is1, &is2, &a[a_offset], &wr[1], &wi[1], ierr);
    goto L50;
/*     .......... find both eigenvalues and eigenvectors .......... */
L20:
    eltran_(nm, n, &is1, &is2, &a[a_offset], &iv1[1], &z__[z_offset]);
    hqr2_(nm, n, &is1, &is2, &a[a_offset], &wr[1], &wi[1], &z__[z_offset], 
	    ierr);
    if (*ierr != 0) {
	goto L50;
    }
    balbak_(nm, n, &is1, &is2, &fv1[1], n, &z__[z_offset]);
L50:
    return 0;
} /* rg_ */


