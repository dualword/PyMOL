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
#include<math.h>
#include<limits.h>

#ifndef MAXFLOAT
#define MAXFLOAT FLT_MAX
#endif

#include"MemoryDebug.h"
#include"Base.h"
#include"Basis.h"

#ifndef R_SMALL4
#define R_SMALL4 0.0001
#endif

void BasisInit(CBasis *I);
void BasisFinish(CBasis *I);

float ZLineClipPoint(float *base,float *point,float *alongNormalSq,float cutoff);

int ZLineToSphere(float *base,float *point,float *dir,float radius,float maxial,
						float *sphere,float *asum);

static int intersect_triangle(float orig[3], float *pre,float vert0[3], float vert2[3],
										float *u, float *v, float *d);

/*========================================================================*/
int ZLineToSphere(float *base,float *point,float *dir,float radius,float maxial,
						float *sphere,float *asum)
{
  /* Strategy - find an imaginary sphere that lies at the correct point on
	  the line segment, then treat as a sphere reflection */

  float perpAxis[3],intra_p[3];
  float perpDist,radial,axial,axial_sum,dangle,ab_dangle,axial_perp;
  float radialsq,tan_acos_dangle;
  float intra[3],vradial[3],ln;

  ln = sqrt(dir[1]*dir[1]+dir[0]*dir[0]);

  perpAxis[0] = dir[1]/ln; /* was cross_product(MinusZ,dir,perpAxis),normalize */
  perpAxis[1] = -dir[0]/ln;

 /* the perpAxis defines a perp-plane which includes the cyl-axis */
  
  intra[0]=point[0]-base[0];
  intra[1]=point[1]-base[1];

  /* get minimum distance between the lines */

  perpDist = intra[0]*perpAxis[0] + intra[1]*perpAxis[1];
  /* was dot_product3f(intra,perpAxis); */

  if(fabs(perpDist)>radius) {
	 return(0);
  }

  perpAxis[2] = 0.0;
  intra[2]=point[2]-base[2];

  dangle = -dir[2]; /* was dot(MinusZ,dir) */
  ab_dangle=fabs(dangle);
  if(ab_dangle>(1-R_SMALL4))
	 {
		if(dangle>0.0) {
		  sphere[0]=point[0];
		  sphere[1]=point[1];
		  sphere[2]=point[2];
		  return(1);
		} else {
		  sphere[0]=dir[0]*maxial+point[0];
		  sphere[1]=dir[1]*maxial+point[1];
		  sphere[2]=dir[2]*maxial+point[2];
		  return(1);
		  }
	 }

  /*tan_acos_dangle = tan(acos(dangle));*/
  tan_acos_dangle = sqrt(1-dangle*dangle)/dangle;

  /*
  printf("perpDist %8.3f\n",perpDist);
  printf("dir %8.3f %8.3f %8.3f\n",dir[0],dir[1],dir[2]);
  printf("base %8.3f %8.3f %8.3f\n",base[0],base[1],base[2]);
  printf("point %8.3f %8.3f %8.3f\n",point[0],point[1],point[2]);
  printf("intra %8.3f %8.3f %8.3f\n",intra[0],intra[1],intra[2]);
  printf("perpAxis %8.3f %8.3f %8.3f\n",perpAxis[0],perpAxis[1],perpAxis[2]);
  */

  /* now we need to define the triangle in the perp-plane  
	  to figure out where the projected line intersection point is */

  /* first, compute radial distance in the perp-plane between the two starting points */

  remove_component3f(intra,perpAxis,intra_p);
  remove_component3f(intra_p,dir,vradial);

  radialsq = lengthsq3f(vradial);

  /* now figure out the axial distance along the cyl-line that will give us
	  the point of closest approach */

  if(ab_dangle<R_SMALL4)
	 axial_perp=0;
  else
	 axial_perp = sqrt(radialsq)/tan_acos_dangle;
  
  axial = lengthsq3f(intra_p)-radialsq;
  if(axial<0.0) 
	 axial=0.0;
  else
	 axial = sqrt(axial);

  /*
  printf("radial %8.3f\n",radial);
  printf("vradial %8.3f %8.3f %8.3f\n",vradial[0],vradial[1],vradial[2]);
  printf("radial %8.3f\n",radial);
  printf("dangle %8.3f \n",dangle);
  printf("axial_perp %8.3f \n",axial_perp);
  printf("axial1 %8.3f \n",axial);
  printf("%8.3f\n",dot_product3f(intra_p,dir));
  */

  if(dot_product3f(intra_p,dir)>=0.0)
	 axial = axial_perp - axial;
  else
	 axial = axial_perp + axial;

  /*
  printf("axial2 %8.3f\n",axial);
  */
  
  /* now we have to think about where the vector will actually strike the cylinder*/

  /* by definition, it must be perpdist away from the perp-plane becuase the perp-plane
	  is parallel to the line, so we can compute the radial component to this point */

  radial = radius*radius-perpDist*perpDist;
  if(radial<0.0)
	 radial=0.0;
  else
	 radial = sqrt(radial);

  /* now the trick is figuring out how to adjust the axial distance to get the actual
	  position along the cyl line which will give us a representative sphere */

  if(ab_dangle>R_SMALL4)
	 axial_sum = axial - radial/tan_acos_dangle;
  else
	 axial_sum = axial;
  /*
	 printf("radial2 %8.3f \n",radial);*/

  if(axial_sum<0) axial_sum=0.0;
  if(axial_sum>maxial) axial_sum=maxial;
  
  sphere[0]=dir[0]*axial_sum+point[0];
  sphere[1]=dir[1]*axial_sum+point[1];
  sphere[2]=dir[2]*axial_sum+point[2];

  *asum = axial_sum;
  /*  printf("==>%8.3f sphere %8.3f %8.3f %8.3f\n",base[1],sphere[1],axial_perp,axial);*/
  return(1);

}
/*========================================================================*/
float ZLineClipPoint(float *base,float *point,float *alongNormalSq,float cutoff)
{
  float hyp[3];
  float result;
  /* this routine determines whether or not a vector starting at "base"
	  heading in the direction "normal" intersects a sphere located at "point".

	  It returns how far along the vector the intersection with the plane is or
	  MAXFLOAT if there isn't a relevant intersection

	  NOTE: this routine has been optimized for normals along Z
	  Optimizes-out vectors that are more than "cutoff" from "point" in x,y plane 
  */

  hyp[0] = point[0] - base[0];
  if(fabs(hyp[0])>cutoff) return (MAXFLOAT);
  hyp[1] = point[1] - base[1];
  if(fabs(hyp[1])>cutoff) return (MAXFLOAT);
  hyp[2] = point[2] - base[2];
  
  if(hyp[2]<0.0) {
	 (*alongNormalSq) = (hyp[2]*hyp[2]);
	 result=(hyp[0]*hyp[0])+(hyp[1]*hyp[1]);
	 return(result);
  } else {
	 return(MAXFLOAT);
  }
}
/*========================================================================*/
void BasisSetupMatrix(CBasis *I)
{
  float oldZ[3] = { 0.0,0.0,1.0 };
  float newY[3];
  float dotgle,angle;

  cross_product3f(oldZ,I->LightNormal,newY);

  dotgle=dot_product3f(oldZ,I->LightNormal);
  
  if((1.0-fabs(dotgle))<R_SMALL4)
	 {
		dotgle=dotgle/fabs(dotgle);
		newY[0]=0.0;
		newY[1]=1.0;
		newY[2]=0.0;
	 }
  
  normalize3f(newY);

  angle=-acos(dotgle);
  
  /* now all we gotta do is effect a rotation about the new Y axis to line up new Z with Z */
  
  rotation_to_matrix33f(newY,angle,I->Matrix);

  /*
  printf("%8.3f %8.3f %8.3f %8.3f\n",angle*180.0/cPI,newY[0],newY[1],newY[2]);
  
  transform33f3f(I->Matrix,newY,test);
  printf("   %8.3f %8.3f %8.3f\n",test[0],test[1],test[2]);

  printf("   %8.3f %8.3f %8.3f\n",I->LightNormal[0],I->LightNormal[1],I->LightNormal[2]);
  transform33f3f(I->Matrix,I->LightNormal,test);
  printf("   %8.3f %8.3f %8.3f\n",test[0],test[1],test[2]);

  printf(">%8.3f %8.3f %8.3f\n",I->Matrix[0][0],I->Matrix[0][1],I->Matrix[0][2]);
  printf(">%8.3f %8.3f %8.3f\n",I->Matrix[1][0],I->Matrix[1][1],I->Matrix[1][2]);
  printf(">%8.3f %8.3f %8.3f\n",I->Matrix[2][0],I->Matrix[2][1],I->Matrix[2][2]);
  */
}



/*========================================================================*/
void BasisReflectTriangle(CBasis *I,RayInfo *r,int i,float *fc) 
{
  float *n0,w2;
  float vt1[3];
  int ni;

  r->impact[0]=r->base[0]; 
  r->impact[1]=r->base[1]; 
  r->impact[2]=r->base[2]-r->dist;

  ni = I->Vert2Normal[i];
  n0 = I->Normal+3*ni+3; /* skip triangle normal */
  w2 = 1.0-(r->tri1+r->tri2);
  /*  printf("%8.3f %8.3f\n",r->tri[1],r->tri[2]);*/

  fc[0]=(r->prim->c2[0]*r->tri1)+(r->prim->c3[0]*r->tri2)+(r->prim->c1[0]*w2);
  fc[1]=(r->prim->c2[1]*r->tri1)+(r->prim->c3[1]*r->tri2)+(r->prim->c1[1]*w2);
  fc[2]=(r->prim->c2[2]*r->tri1)+(r->prim->c3[2]*r->tri2)+(r->prim->c1[2]*w2);

  scale3f(n0+3,r->tri1,r->surfnormal);
  scale3f(n0+6,r->tri2,vt1);
  add3f(vt1,r->surfnormal,r->surfnormal);

  scale3f(n0,w2,vt1);
  add3f(vt1,r->surfnormal,r->surfnormal);

  normalize3f(r->surfnormal);
  r->dotgle = -r->surfnormal[2]; 
  
  r->reflect[0]= - ( 2 * r->dotgle * r->surfnormal[0] );
  r->reflect[1]= - ( 2 * r->dotgle * r->surfnormal[1] );
  r->reflect[2]= -1.0 - ( 2 * r->dotgle * r->surfnormal[2] );

}
/*========================================================================*/
int BasisHit(CBasis *I,RayInfo *r,int except,
				 int *vert2prim,CPrimitive *prim,
				 int shadow,float front,float back)
{
  float oppSq,dist,sph[3],*vv,vt[3],tri1,tri2;
  int minIndex;
  int a,b,c,h,i,*ip,aa,bb,cc;
  CPrimitive *prm;
  /* assumption: always heading in the negative Z direction with our vector... */
  minIndex = -1;
  vt[0]=r->base[0];
  vt[1]=r->base[1];
  if(except>=0) except=vert2prim[except];
  if(MapInsideXY(I->Map,r->base,&a,&b,&c))
	 {
		r->dist = MAXFLOAT;
		MapCacheReset(I->Map);
		while(1) {
		  h=*MapEStart(I->Map,a,b,c);
		  if(h)
			 {
				ip=I->Map->EList+h;
				i=*(ip++);

				while(i>=0) {
				  if((vert2prim[i]!=except)&&(!MapCached(I->Map,i))) {
					 MapCache(I->Map,i);
					 prm = prim+vert2prim[i];
					 switch(prm->type) {
					 case cPrimTriangle:

						if(intersect_triangle(r->base,I->Precomp+I->Vert2Normal[i]*3,
													 I->Vertex+prm->vert*3,I->Vertex+prm->vert*3+6,
													 &tri1,&tri2,&dist)) 
						  {
							 if(shadow) {
								if((dist>(-R_SMALL4))&&(dist<r->dist))
								  return(1);
							 } else {
								if(dist<r->dist)
								  if((dist>=front)&&(dist<=back)) {
									 minIndex=i;
									 r->tri1=tri1;
									 r->tri2=tri2;
									 r->dist=dist;
								  }
							 }
						  }
						break;
					 case cPrimSphere:
						oppSq = ZLineClipPoint(r->base,I->Vertex+i*3,&dist,I->Radius[i]);
						if(oppSq<=I->Radius2[i])
						  {
							 dist=sqrt(dist)-sqrt((I->Radius2[i]-oppSq));
							 if(shadow) {
								if((dist>(-R_SMALL4))&&(dist<r->dist))
								  return(1);
							 } else {
								if(dist<r->dist)
								  if((dist>=front)&&(dist<=back)) {
									 minIndex=i;
									 r->dist=dist;
								  }
							 }
						  }
						break;
					 case cPrimCylinder:
						  if(ZLineToSphere(r->base,I->Vertex+i*3,
												 I->Normal+I->Vert2Normal[i]*3,I->Radius[i],
												 prm->l1,sph,&tri1))
						  {
							 oppSq = ZLineClipPoint(r->base,sph,&dist,I->Radius[i]);
							 if(oppSq<=I->Radius2[i])
								{
								  dist=sqrt(dist)-sqrt((I->Radius2[i]-oppSq));
								  if(shadow) {
									 if((dist>(-R_SMALL4))&&(dist<r->dist))
										return(1);
								  } else {
									 if(dist<r->dist)
										if((dist>=front)&&(dist<=back)) {
										  r->tri1=tri1;
										  r->sphere[0]=sph[0];
										  r->sphere[1]=sph[1];										
										  r->sphere[2]=sph[2];
										  minIndex=i;
										  r->dist=dist;
										}
								  }
								}
						  }
						break;
					 }
				  }
				  i=*(ip++);
				}
			 }
		  if(minIndex>=0)
			 {
				vt[2]=r->base[2]-r->dist;
				MapLocus(I->Map,vt,&aa,&bb,&cc);
				if(cc>c)
				  break;
			 }
		  c--;
		  if(c<MapBorder)
			 break;
		}
	 }
  if(minIndex>=0) {
	 r->prim = prim+vert2prim[minIndex];
	 switch(r->prim->type) {
	 case cPrimSphere:
		
		vv=I->Vertex+minIndex*3;
		r->sphere[0]=vv[0];
		r->sphere[1]=vv[1];
		r->sphere[2]=vv[2];
		break;
	 case cPrimTriangle:
		break;
	 }
  }
  return(minIndex);
}
/*========================================================================*/
void BasisMakeMap(CBasis *I,int *vert2prim,CPrimitive *prim,float *volume)
{
  float *v,*vv,*d;
  float l;
  CPrimitive *prm;
  int a,b,c,i,n,h,q,x,y,z;
  int extra_vert = 0;
  float p[3],dd[3],*d1,*d2;
  float *tempVertex;
  int *tempRef,*ip,*sp;
  int remapMode=true; /* remap mode means that some objects will span more
                         * than one voxel, so we have to worry about populating
                         * those voxels and also about eliminating duplicates 
                         * when traversing the neighbor lists */
  float min[3],max[3],extent[6];
  float sep;
  float diagonal[3];
  float l1,l2;
  float bh,ch;

  sep = I->MinVoxel;
  if(sep==0.0)
    {
      remapMode = false;
      sep = I->MaxRadius; /* also will imply no remapping of vertices */
    }
  /* we need to get a sense of the actual size in order to avoid sep being too small */
    
  v=I->Vertex;
  for(c=0;c<3;c++)
    {
      min[c] = v[c];
      max[c] = v[c];
    }
  v+=3;
  for(a=1;a<I->NVertex;a++)
    {
      for(c=0;c<3;c++)
        {
          if(min[c]>v[c])
            min[c]=v[c];
          if(max[c]<v[c])
            max[c]=v[c];
        }
      v+=3;
    }
  if(volume) {
    if(min[0]<volume[0])
      min[0]=volume[0];
    if(max[0]>volume[1])
      max[0]=volume[1];
    if(min[1]<volume[2])
      min[1]=volume[2];
    if(max[1]>volume[3])
      max[1]=volume[3];
    if(min[2]<(-volume[5]))
      min[2]=(-volume[5]);
    if(max[2]>(-volume[4]))
		max[2]=(-volume[4]);
  }
  sep = MapGetSeparation(sep,max,min,diagonal); /* this needs to be a minimum 
                                                 * estimate of the actual value */

  /*  printf("sep %f\n",sep);*/
  /* here we have to carry out a complicated work-around in order to
	* efficiently encode our lines into the map in a way that doesn't
   * require expanding the map cutoff to the size of the largest object*/
  if(remapMode) 
    for(a=0;a<I->NVertex;a++)
      {
        prm=prim+vert2prim[a];
		  switch(prm->type) {
		  case cPrimTriangle:
			 if(a==prm->vert) { /* only do this calculation for one of the three vertices */
				l1=length3f(I->Precomp+I->Vert2Normal[a]*3);
				l2=length3f(I->Precomp+I->Vert2Normal[a]*3+3);
				b = ceil(l1/sep)+1;
				c = ceil(l2/sep)+1;
				extra_vert += b*c;
			 }
			 break;
		  case cPrimCylinder:
          extra_vert+= (ceil(prm->l1/sep)+1);
			 break;
		  case cPrimSphere:
          b = 2*floor(prm->r1/sep)+1;
          extra_vert+= (b*b*b);
			 break;
        } 
		}
  if(remapMode) {
	 extra_vert+=I->NVertex;
	 tempVertex = Alloc(float,extra_vert*3);
	 tempRef = Alloc(int,extra_vert); 
	 /* lower indexes->flags, top is ref->lower index*/
	 
	 v=tempVertex;
	 vv=I->Vertex;
	 for(a=0;a<I->NVertex;a++)
		{
		  *(v++)=*(vv++);
		  *(v++)=*(vv++);
		  *(v++)=*(vv++);
		}
	 
	 n=I->NVertex;
	 for(a=0;a<I->NVertex;a++)
		{
		  prm=prim+vert2prim[a];
		  switch(prm->type) {
		  case cPrimTriangle:
			 if(a==prm->vert) { /* only do this calculation for one of the three vertices */
				d1=I->Precomp+I->Vert2Normal[a]*3;
				d2=I->Precomp+I->Vert2Normal[a]*3+3;
				vv=I->Vertex+a*3;
				l1=length3f(d1);
				l2=length3f(d2);
				b = floor(l1/sep)+1;
				c = floor(l2/sep)+1;
				extra_vert += b*c;
				bh=(b/2)+1;
				ch=(c/2)+1;
				
				for(x=0;x<bh;x++)
				  for(y=0;y<ch;y++) 
					 {
						*(v++) = vv[0]+(d1[0]*x)/b+(d2[0]*y)/c;
						*(v++) = vv[1]+(d1[1]*x)/b+(d2[1]*y)/c;
						*(v++) = vv[2]+(d1[2]*x)/b+(d2[2]*y)/c;
						tempRef[n]=a;
						n++;
						
					 }
				for(x=0;x<bh;x++)
				  for(y=0;y<ch;y++) 
					 {
						if(((((float)x)/b)+(((float)y)/c))<0.5) {
						  *(v++) = vv[0]+d1[0]*(0.5+((float)x)/b)+(d2[0]*y)/c;
						  *(v++) = vv[1]+d1[1]*(0.5+((float)x)/b)+(d2[1]*y)/c;
						  *(v++) = vv[2]+d1[2]*(0.5+((float)x)/b)+(d2[2]*y)/c;
						  tempRef[n]=a; 
						  n++;

						  *(v++) = vv[0]+(d1[0]*x)/b+d2[0]*(0.5+((float)y)/c);
						  *(v++) = vv[1]+(d1[1]*x)/b+d2[1]*(0.5+((float)y)/c);
						  *(v++) = vv[2]+(d1[2]*x)/b+d2[2]*(0.5+((float)y)/c);
						  tempRef[n]=a;
						  n++;
						}
					 }
			 }
			 break;
		  case cPrimCylinder:
			 d=I->Normal+3*I->Vert2Normal[a];
			 vv=I->Vertex+a*3;
			 
			 *(v++) = vv[0]+d[0]*prm->l1;
			 *(v++) = vv[1]+d[1]*prm->l1;
			 *(v++) = vv[2]+d[2]*prm->l1;
			 tempRef[n]=a;
			 n++;

			 p[0]=vv[0];
			 p[1]=vv[1];
			 p[2]=vv[2];
			 dd[0]=d[0]*sep;
			 dd[1]=d[1]*sep;
			 dd[2]=d[2]*sep;
			 l=prm->l1;
			 l-=sep;
			 while(l>=0.0) {
				*(v++) = (p[0]+=dd[0]);
				*(v++) = (p[1]+=dd[1]);
				*(v++) = (p[2]+=dd[2]);
				l-=sep;
				tempRef[n]=a; /* store reference to source vertex */
				n++;
			 }
			 break;
		  case cPrimSphere:
          q = floor(prm->r1/sep);
          vv=I->Vertex+a*3;
          
          for(x=-q;x<=q;x++)
            for(y=-q;y<=q;y++)
              for(z=-q;z<=q;z++)
                {
                  *(v++) = vv[0]+x*sep;
                  *(v++) = vv[1]+y*sep;
                  *(v++) = vv[2]+z*sep;
                  tempRef[n]=a;
                  n++;
                }
			 break;
        }
      }
  
	 if(n>extra_vert) {
		exit(1);
	 }
	 
	 if(volume) {
		v=tempVertex;
		for(c=0;c<3;c++)
		  {
			 min[c] = v[c];
			 max[c] = v[c];
		  }
		v+=3;
		for(a=1;a<n;a++)
		  {
			 for(c=0;c<3;c++)
				{
				  if(min[c]>v[c])
					 min[c]=v[c];
				  if(max[c]<v[c])
					 max[c]=v[c];
				}
			 v+=3;
		  }
		if(min[0]<volume[0])
		  min[0]=volume[0];
		if(max[0]>volume[1])
		  max[0]=volume[1];
		if(min[1]<volume[2])
		  min[1]=volume[2];
		if(max[1]>volume[3])
		  max[1]=volume[3];
		/*		printf("%8.3f %8.3f\n",volume[4],volume[5]);*/
		if(min[2]<(-volume[5]))
		  min[2]=(-volume[5]);
		if(max[2]>(-volume[4]))
		max[2]=(-volume[4]);
		extent[0]=min[0];
		extent[1]=max[0];
		extent[2]=min[1];
		extent[3]=max[1];
		extent[4]=min[2];
		extent[5]=max[2];
		/*		printf("%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
				extent[0],extent[1],extent[2],extent[3],extent[4],extent[5]);*/
		I->Map=MapNew(-sep,tempVertex,n,extent);
	 } else {
		I->Map=MapNew(sep,tempVertex,n,NULL);
	 }

	 MapSetupExpressXY(I->Map);
	 
	 /* now do a filter-reassignment pass to remap fake vertices
	  to the original line vertex while deleting duplicate entries */

	 ip=tempRef;
	 for(i=0;i<I->NVertex;i++)
		*(ip++)=0; /* clear flags */
	 for(a=I->Map->iMin[0];a<=I->Map->iMax[0];a++)
		for(b=I->Map->iMin[1];b<=I->Map->iMax[1];b++)
		  for(c=I->Map->iMin[2];c<=I->Map->iMax[2];c++)
			 {
				h=*MapEStart(I->Map,a,b,c);
				if(h)
				  {
					 ip=I->Map->EList+h; 
					 sp=ip;
					 i=*(sp++);
					 while(i>=0) {
						if(i>=I->NVertex) i=tempRef[i];
						if(!tempRef[i]) { /*eliminate duplicates */
						  *(ip++)=i;
						  tempRef[i]=1;
						}
						i=*(sp++);
					 }
					 *(ip)=-1; /* terminate list */
				/* now reset flags efficiently */
					 h=*MapEStart(I->Map,a,b,c); 
					 ip=I->Map->EList+h;
					 i=*(ip++);
					 while(i>=0) {
						tempRef[i]=0;
						i=*(ip++);
					 }

				  }
			 }
	 FreeP(tempVertex);
	 FreeP(tempRef);
  } else {
	 /* simple sphere mode */
	 I->Map=MapNew(-sep,I->Vertex,I->NVertex,NULL);
	 MapSetupExpressXY(I->Map);
  }
  MapCacheInit(I->Map);
}
/*========================================================================*/
void BasisInit(CBasis *I)
{
  I->Vertex = VLAlloc(float,1);
  I->Radius = VLAlloc(float,1);
  I->Radius2 = VLAlloc(float,1);
  I->Normal = VLAlloc(float,1);
  I->Vert2Normal = VLAlloc(int,1);
  I->Precomp = VLAlloc(float,1);
  I->Map=NULL;
  I->NVertex=0;
  I->NNormal=0;
}
/*========================================================================*/
void BasisFinish(CBasis *I)
{
  if(I->Map) 
	 {
		MapFree(I->Map);
		I->Map=NULL;
	 }  
  VLAFreeP(I->Radius2);
  VLAFreeP(I->Radius);
  VLAFreeP(I->Vertex);
  VLAFreeP(I->Vert2Normal);
  VLAFreeP(I->Normal);
  VLAFreeP(I->Precomp);
  I->Vertex=NULL;
}


/*========================================================================*/

#define EPSILON 0.000001

void BasisTrianglePrecompute(float *v0,float *v1,float *v2,float *pre)
{
  float det;

  subtract3f(v1,v0,pre);
  subtract3f(v2,v0,pre+3);
  det = pre[0]*pre[4] - pre[1]*pre[3];
  if(fabs(det)<EPSILON) 
	 *(pre+6)=0.0;
  else {
	 *(pre+6)=1.0;
	 *(pre+7)=1.0/det;
  }
}

#define CROSS(dest,v1,v2) {\
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0];}
#define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])
#define SUB(dest,v1,v2) {dest[0]=v1[0]-v2[0]; dest[1]=v1[1]-v2[1]; dest[2]=v1[2]-v2[2];} 


static int intersect_triangle(float orig[3], float *pre,float vert0[3], float vert2[3],
										float *u, float *v, float *d)
{
  float tvec[3],qvec[3];
  /* float edge1[3], edge2[3],*/
  /* float pvec[3] 
	  float det,inv_det;*/

	/*	printf("%8.3f %8.3f %8.3f origin\n",orig[0],orig[1],orig[2]);
		printf("%8.3f %8.3f %8.3f v0\n",vert0[0],vert0[1],vert0[2]);
		printf("%8.3f %8.3f %8.3f v1\n",vert1[0],vert1[1],vert1[2]);
		printf("%8.3f %8.3f %8.3f v2\n",vert2[0],vert2[1],vert2[2]);*/
   /* find vectors for two edges sharing vert0 */

	/*   SUB(edge1, vert1, vert0); */
	/*   SUB(edge2, vert2, vert0); */

   /* begin calculating determinant - also used to calculate U parameter */
	/*   CROSS(pvec, dir, edge2);*/

   /* if determinant is near zero, ray lies in plane of triangle */
	/*   det = DOT(edge1, pvec);*/
	/*	det = edge1[0]*edge2[1] - edge1[1]*edge2[0];*/

	/*   if (fabs(det)< EPSILON) return 0; */
	/*   inv_det = 1.0 / det; */
	if(!pre[6]) return 0;

   /* calculate distance from vert0 to ray origin */
   SUB(tvec, orig, vert0);

   /* calculate U parameter and test bounds */
	/*   *u = DOT(tvec, pvec) * inv_det;*/
	/*	*u = (tvec[0]*edge2[1] - tvec[1]*edge2[0]) * inv_det;*/
	*u = (tvec[0]*pre[4] - tvec[1]*pre[3]) * pre[7];
   if ((*u < 0.0) || (*u > 1.0)) return 0;

   /* prepare to test V parameter */
	/*   CROSS(qvec, tvec, edge1);*/
   CROSS(qvec, tvec, pre);

   /* calculate V parameter and test bounds */
	/*   *v = DOT(dir, qvec) * inv_det;*/
   *v = -qvec[2] * pre[7];
	
   if ((*v < 0.0) || ((*u + *v) > 1.0)) return 0;

   /* calculate t, ray intersects triangle */
	/*   *t = DOT(edge2, qvec) * inv_det;*/

	/*scale3f(edge1,*u,edge1);
	  scale3f(edge2,*v,edge2);
	  add3f(edge1,vert0,hit);
	  add3f(edge2,hit,hit);
	  subtract3f(hit,orig,pvec);
	  (*d) = length3f(pvec);*/
	
	/*	*d = (orig[2]-((*u)*edge1[2])-((*v)*edge2[2])-vert0[2]);*/
	*d = (orig[2]-((*u)*pre[2])-((*v)*pre[5])-vert0[2]);

   return 1;
}
