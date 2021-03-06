/* -------------------------------------------------------------------------------

Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define MAP_C



/* dependencies */
#include "q3map2.h"



/* FIXME: remove these vars */

/* undefine to make plane finding use linear sort (note: really slow) */
#define	USE_HASHING
#define	PLANE_HASHES	8192

plane_t					*planehash[ PLANE_HASHES ];

int						c_boxbevels;
int						c_edgebevels;
int						c_areaportals;
int						c_detail;
int						c_structural;



/*
PlaneEqual()
ydnar: replaced with variable epsilon for djbob
*/

#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.01

qboolean PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	float	ne, de;
	
	
	/* get local copies */
	ne = normalEpsilon;
	de = distanceEpsilon;
	
	/* compare */
	if( fabs( p->dist - dist ) <= de &&
		fabs( p->normal[ 0 ] - normal[ 0 ] ) <= ne &&
		fabs( p->normal[ 1 ] - normal[ 1 ] ) <= ne &&
		fabs( p->normal[ 2 ] - normal[ 2 ] ) <= ne )
		return qtrue;
	
	/* different */
	return qfalse;
}



/*
AddPlaneToHash()
*/

void AddPlaneToHash( plane_t *p )
{
	int		hash;

	
	hash = (PLANE_HASHES - 1) & (int) fabs( p->dist );

	p->hash_chain = planehash[hash];
	planehash[hash] = p;
}

/*
================
CreateNewFloatPlane
================
*/
int CreateNewFloatPlane (vec3_t normal, vec_t dist)
{
	plane_t	*p, temp;

	if (VectorLength(normal) < 0.5)
	{
		Sys_Printf( "FloatPlane: bad normal\n");
		return -1;
	}

	// create a new plane
	if (nummapplanes+2 > MAX_MAP_PLANES)
		Error ("MAX_MAP_PLANES");

	p = &mapplanes[nummapplanes];
	VectorCopy (normal, p->normal);
	p->dist = dist;
	p->type = (p+1)->type = PlaneTypeForNormal (p->normal);

	VectorSubtract (vec3_origin, normal, (p+1)->normal);
	(p+1)->dist = -dist;

	nummapplanes += 2;

	// allways put axial planes facing positive first
	if (p->type < 3)
	{
		if (p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0)
		{
			// flip order
			temp = *p;
			*p = *(p+1);
			*(p+1) = temp;

			AddPlaneToHash (p);
			AddPlaneToHash (p+1);
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash (p);
	AddPlaneToHash (p+1);
	return nummapplanes - 2;
}

#define Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX 1
#define Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX 1

/*
SnapNormal()
snaps a near-axial normal vector
*/

qboolean SnapNormal(vec3_t normal) {
#if Q3MAP2_EXPERIMENTAL_SNAP_NORMAL_FIX
	int i;
	qboolean adjusted = qfalse;

	// A change from the original SnapNormal() is that we snap each
	// component that's close to 0.  So for example if a normal is
	// (0.707, 0.707, 0.0000001), it will get snapped to lie perfectly in the
	// XY plane (its Z component will be set to 0 and its length will be
	// normalized).  The original SnapNormal() didn't snap such vectors - it
	// only snapped vectors that were near a perfect axis.

	for (i = 0; i < 3; i++)
	{
		if (normal[i] != 0.0 && -normalEpsilon < normal[i] && normal[i] < normalEpsilon) {
			normal[i] = 0.0;
			adjusted = qtrue;
		}
	}

	if (adjusted) {
		VectorNormalize(normal, normal);
		return qtrue;
	}
	return qfalse;
#else
	int i;

	// I would suggest that you uncomment the following code and look at the
	// results:

	/*
	Sys_Printf("normalEpsilon is %f\n", normalEpsilon);
	for (i = 0;; i++)
	{
	normal[0] = 1.0;
	normal[1] = 0.0;
	normal[2] = i * 0.000001;
	VectorNormalize(normal, normal);
	if (1.0 - normal[0] >= normalEpsilon) {
	Sys_Printf("(%f %f %f)\n", normal[0], normal[1], normal[2]);
	Error("SnapNormal: test completed");
	}
	}
	*/

	// When the normalEpsilon is 0.00001, the loop will break out when normal is
	// (0.999990 0.000000 0.004469).  In other words, this is the vector closest
	// to axial that will NOT be snapped.  Anything closer will be snaped.  Now,
	// 0.004469 is close to 1/225.  The length of a circular quarter-arc of radius
	// 1 is PI/2, or about 1.57.  And 0.004469/1.57 is about 0.0028, or about
	// 1/350.  Expressed a different way, 1/350 is also about 0.26/90.
	// This means is that a normal with an angle that is within 1/4 of a degree
	// from axial will be "snapped".  My belief is that the person who wrote the
	// code below did not intend it this way.  I think the person intended that
	// the epsilon be measured against the vector components close to 0, not 1.0.
	// I think the logic should be: if 2 of the normal components are within
	// epsilon of 0, then the vector can be snapped to be perfectly axial.
	// We may consider adjusting the epsilon to a larger value when we make this
	// code fix.

	for (i = 0; i < 3; i++)
	{
		if (fabs(normal[i] - 1) < normalEpsilon) {
			VectorClear(normal);
			normal[i] = 1;
			return qtrue;
		}
		if (fabs(normal[i] - -1) < normalEpsilon) {
			VectorClear(normal);
			normal[i] = -1;
			return qtrue;
		}
	}
	return qfalse;
#endif
}

/*
SnapPlane()
snaps a plane to normal/distance epsilons
// SnapPlane disabled by LordHavoc because it often messes up collision
// brushes made from triangles of embedded models, and it has little effect
// on anything else (axial planes are usually derived from snapped points)
// SnapPlane reenabled by namespace because of multiple reports of
// q3map2-crashes which were triggered by this patch.
*/

void SnapPlane( vec3_t normal, vec_t *dist )
{
	SnapNormal( normal );
	if( fabs( *dist - Q_rint( *dist ) ) < distanceEpsilon )
		*dist = Q_rint( *dist );
}

/*
SnapPlaneImproved()
snaps a plane to normal/distance epsilons, improved code
*/
void SnapPlaneImproved(vec3_t normal, vec_t *dist, int numPoints, const vec3_t *points) {
	int i;
	vec3_t center;
	vec_t distNearestInt;

	if (SnapNormal(normal)) {
		if (numPoints > 0) {
			// Adjust the dist so that the provided points don't drift away.
			VectorClear(center);
			for (i = 0; i < numPoints; i++)
			{
				VectorAdd(center, points[i], center);
			}
			for (i = 0; i < 3; i++) { center[i] = center[i] / numPoints; }
			*dist = DotProduct(normal, center);
		}
	}

	if (VectorIsOnAxis(normal)) {
		// Only snap distance if the normal is an axis.  Otherwise there
		// is nothing "natural" about snapping the distance to an integer.
		distNearestInt = Q_rint(*dist);
		if (-distanceEpsilon < *dist - distNearestInt && *dist - distNearestInt < distanceEpsilon) {
			*dist = distNearestInt;
		}
	}
}

/*
FindFloatPlane()
ydnar: changed to allow a number of test points to be supplied that
must be within an epsilon distance of the plane
*/

int FindFloatPlane(vec3_t normal, vec_t dist, int numPoints, vec3_t *points)

#ifdef USE_HASHING

{
	int i, j, hash, h;
	plane_t *p;
	vec_t d;


#if Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX
	SnapPlaneImproved(normal, &dist, numPoints, (const vec3_t *)points);
#else
	SnapPlane(normal, &dist);
#endif
	/* hash the plane */
	hash = (PLANE_HASHES - 1) & (int)fabs(dist);

	/* search the border bins as well */
	for (i = -1; i <= 1; i++)
	{
		h = (hash + i) & (PLANE_HASHES - 1);
		for (p = planehash[h]; p != NULL; p = p->hash_chain)
		{
			/* do standard plane compare */
			if (!PlaneEqual(p, normal, dist)) {
				continue;
			}

			/* ydnar: uncomment the following line for old-style plane finding */
			//%	return p - mapplanes;

			/* ydnar: test supplied points against this plane */
			for (j = 0; j < numPoints; j++)
			{
				// NOTE: When dist approaches 2^16, the resolution of 32 bit floating
				// point number is greatly decreased.  The distanceEpsilon cannot be
				// very small when world coordinates extend to 2^16.  Making the
				// dot product here in 64 bit land will not really help the situation
				// because the error will already be carried in dist.
				d = DotProduct(points[j], normal) - dist;
				d = fabs(d);
				if (d != 0.0 && d >= distanceEpsilon) {
					break; // Point is too far from plane.
				}
			}

			/* found a matching plane */
			if (j >= numPoints) {
				return p - mapplanes;
			}
			}
		}

	/* none found, so create a new one */
	return CreateNewFloatPlane(normal, dist);
	}

#else

{
	int i;
	plane_t *p;

#if Q3MAP2_EXPERIMENTAL_SNAP_PLANE_FIX
	SnapPlaneImproved(normal, &dist, numPoints, (const vec3_t *)points);
#else
	SnapPlane(normal, &dist);
#endif
	for (i = 0, p = mapplanes; i < nummapplanes; i++, p++)
	{
		if (PlaneEqual(p, normal, dist)) {
			return i;
		}
		// TODO: Note that the non-USE_HASHING code does not compute epsilons
		// for the provided points.  It should do that.  I think this code
		// is unmaintained because nobody sets USE_HASHING to off.
	}

	return CreateNewFloatPlane(normal, dist);
}

#endif

/*
MapPlaneFromPoints()
takes 3 points and finds the plane they lie in
*/

int MapPlaneFromPoints( vec3_t *p )
{
	dvec3_t t1, t2, normal;
	vec3_t snormal;
	dvec_t dist;
	vec_t sdist;

	/* calc plane normal */
	VectorSubtract( p[ 0 ], p[ 1 ], t1 );
	VectorSubtract( p[ 2 ], p[ 1 ], t2 );
	CrossProduct( t1, t2, normal );
	DVectorNormalize( normal, normal );
	
	/* calc plane distance */
	dist = DotProduct( p[ 0 ], normal );

	/* convert to single precision */
	DVector3ToSingle( normal, snormal );
	DVector1ToSingle( dist, sdist );

	/* store the plane */
	return FindFloatPlane( snormal, sdist, 3, p );
}

/*
SetBrushContents()
the content flags and compile flags on all sides of a brush should be the same
*/
extern qboolean FORCED_STRUCTURAL;

void SetBrushContents( brush_t *b )
{
	int			contentFlags, compileFlags;
	side_t		*s;
	int			i;
	qboolean	mixed, nobsp;
	
	/* get initial compile flags from first side */
	s = &b->sides[ 0 ];
	contentFlags = s->contentFlags;
	compileFlags = s->compileFlags;
	b->contentShader = s->shaderInfo;
	mixed = qfalse;
	
	/* get the content/compile flags for every side in the brush */
	nobsp = qtrue;
	for( i = 1; i < b->numsides; i++, s++ )
	{
		s = &b->sides[ i ];
		if( s->shaderInfo == NULL )
		{
			nobsp = qfalse;
			continue;
		}
		if( s->contentFlags != contentFlags || s->compileFlags != compileFlags )
			mixed = qtrue;
		if( s->shaderInfo->noBSP == qfalse )
			nobsp = qfalse;
	}

	if (s && s->shaderInfo && FORCED_STRUCTURAL && (compileFlags & C_SOLID) /*&& !(compileFlags & C_NODRAW)*/)
	{// Forced structural option is set, force anything solid to also be structural and not detail...
		compileFlags &= ~C_DETAIL;
		compileFlags |= C_STRUCTURAL;
		//Sys_Printf("%s was forced structural.\n", s->shaderInfo->shader);
	}

	/* ydnar: getting rid of this stupid warning */
	//%	if( mixed )
	//%		Sys_FPrintf( SYS_VRB,"Entity %i, Brush %i: mixed face contentFlags\n", b->entitynum, b->brushnum );

	/* check for detail & structural */
	if( (compileFlags & C_DETAIL) && (compileFlags & C_STRUCTURAL) )
	{
		Sys_Warning( mapEnt->mapEntityNum, entitySourceBrushes, "Mixed detail and structural (defaulting to structural)" );
		compileFlags &= ~C_DETAIL;
	}
	
	/* the fulldetail flag will cause detail brushes to be treated like normal brushes */
	if( fulldetail )
		compileFlags &= ~C_DETAIL;

	/* VorteX: for non solid detail all pure detail brushes will be non solid */
	b->nonsolid = nobsp;
	if( (compileFlags & C_DETAIL) && nodetailcollision == qtrue)
		b->nonsolid = qtrue;

	/* all translucent brushes that aren't specifically made structural will be detail */
	if( (compileFlags & C_TRANSLUCENT) && !(compileFlags & C_STRUCTURAL) )
		compileFlags |= C_DETAIL;
	
	/* detail? */
	if( compileFlags & C_DETAIL )
	{
		c_detail++;
		b->detail = qtrue;
	}
	else
	{
		c_structural++;
		b->detail = qfalse;
	}
	
	/* opaque? */
	if( compileFlags & C_TRANSLUCENT )
		b->opaque = qfalse;
	else
		b->opaque = qtrue;
	
	/* areaportal? */
	if( compileFlags & C_AREAPORTAL )
		c_areaportals++;
	
	/* set brush flags */
	b->contentFlags = contentFlags;
	b->compileFlags = compileFlags;
}



/*
AddBrushBevels()
adds any additional planes necessary to allow the brush being
built to be expanded against axial bounding boxes
ydnar 2003-01-20: added mrelusive fixes
*/

void AddBrushBevels( void )
{
	int			axis, dir;
	int			i, j, k, l, order;
	side_t		sidetemp;
	side_t		*s, *s2;
	winding_t	*w, *w2;
	vec3_t		normal;
	vec_t		dist;
	vec3_t		vec, vec2;
	float		d, minBack;

	//
	// add the axial planes
	//
	order = 0;
	for ( axis = 0; axis < 3; axis++ ) {
		for ( dir = -1; dir <= 1; dir += 2, order++ ) {
			// see if the plane is allready present
			for ( i = 0, s = buildBrush->sides; i < buildBrush->numsides; i++, s++ )
			{
				/* ydnar: testing disabling of mre code */
				#if 0
					if ( dir > 0 ) {
						if ( mapplanes[s->planenum].normal[axis] >= 0.9999f ) {
							break;
						}
					}
					else {
						if ( mapplanes[s->planenum].normal[axis] <= -0.9999f ) {
							break;
						}
					}
				#else
					if( (dir > 0 && mapplanes[ s->planenum ].normal[ axis ] == 1.0f ) ||
						(dir < 0 && mapplanes[ s->planenum ].normal[ axis ] == -1.0f) )
						break;
				#endif
			}

			if ( i == buildBrush->numsides )
			{
				// add a new side
				if (buildBrush->numsides == MAX_BUILD_SIDES )
					Sys_Error(buildBrush->entityNum, buildBrush->brushNum, "MAX_BUILD_SIDES (%d)", MAX_BUILD_SIDES );
				memset( s, 0, sizeof( *s ) );
				buildBrush->numsides++;
				VectorClear (normal);
				normal[axis] = dir;

				if( dir == 1 )
				{
					/* ydnar: adding bevel plane snapping for fewer bsp planes */
					if( bevelSnap > 0 )
						dist = floor(buildBrush->maxs[ axis ] / bevelSnap ) * bevelSnap;
					else
						dist = buildBrush->maxs[ axis ];
				}
				else
				{
					/* ydnar: adding bevel plane snapping for fewer bsp planes */
					if( bevelSnap > 0 )
						dist = -ceil(buildBrush->mins[ axis ] / bevelSnap ) * bevelSnap;
					else
						dist = -buildBrush->mins[ axis ];
				}

				s->planenum = FindFloatPlane( normal, dist, 0, NULL );
				s->contentFlags = buildBrush->sides[ 0 ].contentFlags;
				s->bevel = qtrue;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if ( i != order ) 
			{
				sidetemp = buildBrush->sides[order];
				buildBrush->sides[order] = buildBrush->sides[i];
				buildBrush->sides[i] = sidetemp;
			}
		}
	}

	//
	// add the edge bevels
	//
	if (buildBrush->numsides == 6 )
	{
		return;		// pure axial
	}

	// test the non-axial plane edges
	for ( i = 6; i < buildBrush->numsides; i++ ) {
		s = buildBrush->sides + i;
		if (s->culled || !s->shaderInfo) continue;
		w = s->winding;
		if ( !w ) {
			continue;
		}
		for ( j = 0; j < w->numpoints; j++) {
			k = (j+1)%w->numpoints;
			VectorSubtract( w->p[j], w->p[k], vec );
			if ( VectorNormalize( vec, vec ) < 0.5f ) {
				continue;
			}
			SnapNormal( vec );
			for ( k = 0; k < 3; k++ ) {
				if ( vec[k] == -1.0f || vec[k] == 1.0f || (vec[k] == 0.0f && vec[(k+1)%3] == 0.0f) ) {
					break;	// axial
				}
			}
			if ( k != 3 ) {
				continue;	// only test non-axial edges
			}

			/* debug code */
			//%	Sys_Printf( "-------------\n" );

			// try the six possible slanted axials from this edge
			for ( axis = 0; axis < 3; axis++ ) {
				for ( dir = -1; dir <= 1; dir += 2 ) {
					// construct a plane
					VectorClear( vec2 );
					vec2[axis] = dir;
					CrossProduct( vec, vec2, normal );
					if ( VectorNormalize( normal, normal ) < 0.5f ) {
						continue;
					}
					dist = DotProduct( w->p[j], normal );
					
					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for ( k = 0; k < buildBrush->numsides; k++ ) {

						// if this plane has allready been used, skip it
						if ( PlaneEqual( &mapplanes[buildBrush->sides[k].planenum], normal, dist ) ) {
							break;
						}

						w2 = buildBrush->sides[k].winding;
						if ( !w2 ) {
							continue;
						}
						minBack = 0.0f;
						for ( l = 0; l < w2->numpoints; l++ ) {
							d = DotProduct( w2->p[l], normal ) - dist;
							if ( d > 0.1f ) {
								break;	// point in front
							}
							if ( d < minBack ) {
								minBack = d;
							}
						}
						// if some point was at the front
						if ( l != w2->numpoints ) {
							break;
						}

						// if no points at the back then the winding is on the bevel plane
						if ( minBack > -0.1f ) {
							//%	Sys_Printf( "On bevel plane\n" );
							break;
						}
					}

					if ( k != buildBrush->numsides ) {
						continue;	// wasn't part of the outer hull
					}
					
					/* debug code */
					//%	Sys_Printf( "n = %f %f %f\n", normal[ 0 ], normal[ 1 ], normal[ 2 ] );
					
					// add this plane
					if(buildBrush->numsides == MAX_BUILD_SIDES )
						Sys_Error(buildBrush->entityNum, buildBrush->brushNum, "MAX_BUILD_SIDES (%d)", MAX_BUILD_SIDES );
					
					s2 = &buildBrush->sides[buildBrush->numsides];
					buildBrush->numsides++;
					memset( s2, 0, sizeof( *s2 ) );

					s2->planenum = FindFloatPlane( normal, dist, 1, &w->p[ j ] );
					s2->contentFlags = buildBrush->sides[0].contentFlags;
					s2->bevel = qtrue;
					c_edgebevels++;
				}
			}
		}
	}
}



/*
FinishBrush()
produces a final brush based on the buildBrush->sides array
and links it to the current entity
*/

brush_t *FinishBrush( void )
{
	brush_t		*b;
	
	
	/* create windings for sides and bounds for brush */
	if ( !CreateBrushWindings( buildBrush ) )
		return NULL;

	/* origin brushes are removed, but they set the rotation origin for the rest of the brushes in the entity.
	   after the entire entity is parsed, the planenums and texinfos will be adjusted for the origin brush */
	if ( buildBrush->compileFlags & C_ORIGIN ) 
	{
		vec3_t origin, adjustment;
		char string[128];

		if ( numEntities == 1 ) 
		{
			Sys_Warning( mapEnt->mapEntityNum, entitySourceBrushes, "Origin brushes not allowed in world" );
			return NULL;
		}
		VectorAdd( buildBrush->mins, buildBrush->maxs, origin );
		VectorScale( origin, 0.5, origin );

		/* merge with entity-key-set origin */
		GetVectorForKey( mapEnt, "origin", mapEnt->origin );
		VectorMA( origin, -1, mapEnt->originbrush_origin, adjustment );
		VectorAdd( adjustment, mapEnt->origin, mapEnt->origin );
		VectorCopy( origin, mapEnt->originbrush_origin );
		sprintf( string, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );
		SetKeyValue( mapEnt, "origin", string );

		/* don't keep this brush */
		return NULL;
	}
	
	/* determine if the brush is an area portal */
	if( buildBrush->compileFlags & C_AREAPORTAL )
	{
		if( numEntities != 1 )
		{
			Sys_Printf ("Entity %i, Brush %i: areaportals only allowed in world\n", numEntities - 1, entitySourceBrushes );
			return NULL;
		}
	}

	/* determine if the brush is an region brush */
	if( buildBrush->compileFlags & C_REGION )
	{
		if ( mapRegionBrushes == qtrue )
		{
			if ( mapRegion == qfalse )
				ClearBounds( mapRegionMins, mapRegionMaxs );
			AddPointToBounds(buildBrush->mins, mapRegionMins, mapRegionMaxs);
			AddPointToBounds(buildBrush->maxs, mapRegionMins, mapRegionMaxs);
			mapRegion = qtrue;
		}

		/* don't keep this brush */
		return NULL;
	}
	
	/* add bevel planes */
	AddBrushBevels();
	
	/* keep it */
	b = CopyBrush( buildBrush );
	
	/* set map entity and brush numbering */
	b->entityNum = mapEnt->mapEntityNum;
	b->brushNum = entitySourceBrushes;
	
	/* set original */
	b->original = b;
	
	/* link opaque brushes to head of list, translucent brushes to end */
	if( b->opaque || mapEnt->lastBrush == NULL )
	{
		b->next = mapEnt->brushes;
		mapEnt->brushes = b;
		if( mapEnt->lastBrush == NULL )
			mapEnt->lastBrush = b;
	}
	else
	{
		b->next = NULL;
		mapEnt->lastBrush->next = b;
		mapEnt->lastBrush = b;
	}

	
	/* link colorMod volume brushes to the entity directly */
	if( b->contentShader != NULL &&
		b->contentShader->colorMod != NULL &&
		b->contentShader->colorMod->type == CM_VOLUME )
	{
		b->nextColorModBrush = mapEnt->colorModBrushes;
		mapEnt->colorModBrushes = b;
	}
	
	/* return to sender */
	return b;
}



/*
TextureAxisFromPlane()
determines best orthagonal axis to project a texture onto a wall
(must be identical in radiant!)
*/

vec3_t	baseaxis[18] =
{
	{0,0,1}, {1,0,0}, {0,-1,0},			// floor
	{0,0,-1}, {1,0,0}, {0,-1,0},		// ceiling
	{1,0,0}, {0,1,0}, {0,0,-1},			// west wall
	{-1,0,0}, {0,1,0}, {0,0,-1},		// east wall
	{0,1,0}, {1,0,0}, {0,0,-1},			// south wall
	{0,-1,0}, {1,0,0}, {0,0,-1}			// north wall
};

void TextureAxisFromPlane( plane_t *pln, vec3_t xv, vec3_t yv )
{
	int		bestaxis;
	vec_t	dot,best;
	int		i;
	
	best = 0;
	bestaxis = 0;
	
	for (i=0 ; i<6 ; i++)
	{
		dot = DotProduct (pln->normal, baseaxis[i*3]);
		if( dot > best + 0.0001f )	/* ydnar: bug 637 fix, suggested by jmonroe */
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	VectorCopy (baseaxis[bestaxis*3+1], xv);
	VectorCopy (baseaxis[bestaxis*3+2], yv);
}



/*
QuakeTextureVecs()
creates world-to-texture mapping vecs for crappy quake plane arrangements
*/

void QuakeTextureVecs( plane_t *plane, vec_t shift[ 2 ], vec_t rotate, vec_t scale[ 2 ], vec_t mappingVecs[ 2 ][ 4 ] )
{
	vec3_t	vecs[2];
	int		sv, tv;
	vec_t	ang, sinv, cosv;
	vec_t	ns, nt;
	int		i, j;
	
	
	TextureAxisFromPlane(plane, vecs[0], vecs[1]);
	
	if (!scale[0])
		scale[0] = 1;
	if (!scale[1])
		scale[1] = 1;

	// rotate axis
	if (rotate == 0)
		{ sinv = 0 ; cosv = 1; }
	else if (rotate == 90)
		{ sinv = 1 ; cosv = 0; }
	else if (rotate == 180)
		{ sinv = 0 ; cosv = -1; }
	else if (rotate == 270)
		{ sinv = -1 ; cosv = 0; }
	else
	{	
		ang = rotate / 180 * Q_PI;
		sinv = sin(ang);
		cosv = cos(ang);
	}

	if (vecs[0][0])
		sv = 0;
	else if (vecs[0][1])
		sv = 1;
	else
		sv = 2;
				
	if (vecs[1][0])
		tv = 0;
	else if (vecs[1][1])
		tv = 1;
	else
		tv = 2;
					
	for (i=0 ; i<2 ; i++) {
		ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
		nt = sinv * vecs[i][sv] +  cosv * vecs[i][tv];
		vecs[i][sv] = ns;
		vecs[i][tv] = nt;
	}

	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<3 ; j++)
			mappingVecs[i][j] = vecs[i][j] / scale[i];

	mappingVecs[0][3] = shift[0];
	mappingVecs[1][3] = shift[1];
}

//#define __RANOMIZATION__

#ifdef __RANOMIZATION__
typedef float					vec2_t[2];

#define HASHSCALE1 .1031

float fract(float x)
{
	return x - floor(x);
}

float trandom(vec2_t p)
{
	vec3_t p3, p4;
	p3[0] = fract(p[0] * HASHSCALE1);
	p3[1] = fract(p[1] * HASHSCALE1);
	p3[2] = fract(p[0] * HASHSCALE1);

	p4[0] = p3[1] + 19.19;
	p4[1] = p3[2] + 19.19;
	p4[2] = p3[0] + 19.19;

	float dt = DotProduct(p3, p4);
	p3[0] += dt;
	p3[1] += dt;
	p3[2] += dt;
	return fract((p3[0] + p3[1]) * p3[2]);
}

/*float mix(float x, float y, float a)
{
	return x * (1.0 - a) + y * a;
}*/

float rnoise(vec2_t st) {
	vec2_t i;
	i[0] = floor(st[0]);
	i[1] = floor(st[1]);
	vec2_t f;
	f[0] = fract(st[0]);
	f[1] = fract(st[1]);

	// Four corners in 2D of a tile
	float a = trandom(i);
	vec2_t v;
	v[0] = 1.0 + i[0];
	v[1] = 0.0 + i[1];
	float b = trandom(v);
	v[0] = 0.0 + i[0];
	v[1] = 1.0 + i[1];
	float c = trandom(v);
	v[0] = 1.0 + i[0];
	v[1] = 1.0 + i[1];
	float d = trandom(v);

	// Smooth Interpolation

	// Cubic Hermine Curve.  Same as SmoothStep()
	vec2_t u;
	u[0] = f[0] * f[0] * (3.0 - 2.0*f[0]);
	u[1] = f[1] * f[1] * (3.0 - 2.0*f[1]);
	u[2] = f[2] * f[2] * (3.0 - 2.0*f[2]);
	// u = smoothstep(0.,1.,f);

	// Mix 4 coorners percentages

	return mix(a, b, u[0]) +
		(c - a)* u[1] * (1.0 - u[0]) +
		(d - b) * u[0] * u[1];
}
#endif //__RANDOMIZATION__

/*
ParseRawBrush()
parses the sides into buildBrush->sides[], nothing else.
no validation, back plane removal, etc.

Timo - 08/26/99
added brush epairs parsing ( ignoring actually )
Timo - 08/04/99
added exclusive brush primitive parsing
Timo - 08/08/99
support for old brush format back in
NOTE: it would be "cleaner" to have seperate functions to parse between old and new brushes
*/

static void ParseRawBrush( )
{
	side_t			*side;
	vec3_t			planePoints[ 3 ];
	int				planenum;
	shaderInfo_t	*si;
	vec_t			shift[ 2 ];
	vec_t			rotate;
	vec_t			scale[ 2 ];
	char			name[ MAX_QPATH ];
	char			shader[ MAX_QPATH ];
	int				flags;
	
	/* initial setup */
	buildBrush->numsides = 0;
	buildBrush->detail = qfalse;
	
	/* bp */
	if( g_bBrushPrimit == BPRIMIT_NEWBRUSHES )
		MatchToken( "{" );
	
	/* parse sides */
	while( 1 )
	{
		if( !GetToken( qtrue ) )
			break;
		if( !strcmp( token, "}" ) )
			break;
		 
		/* ttimo : bp: here we may have to jump over brush epairs (only used in editor) */
		if( g_bBrushPrimit == BPRIMIT_NEWBRUSHES )
		{
			while( 1 )
			{
				if( strcmp( token, "(" ) )
					GetToken( qfalse );
				else
					break;
				GetToken( qtrue );
			}
		}
		UnGetToken();
		
		/* test side count */
		if( buildBrush->numsides >= MAX_BUILD_SIDES )
			Sys_Error( buildBrush->entityNum, buildBrush->brushNum, "MAX_BUILD_SIDES (%d)", MAX_BUILD_SIDES );
		
		/* add side */
		side = &buildBrush->sides[ buildBrush->numsides ];
		memset( side, 0, sizeof( *side ) );
		buildBrush->numsides++;
		
		/* read the three point plane definition */
		Parse1DMatrix( 3, planePoints[ 0 ] );
		Parse1DMatrix( 3, planePoints[ 1 ] );
		Parse1DMatrix( 3, planePoints[ 2 ] );

		/* bp: read the texture matrix */
		if( g_bBrushPrimit == BPRIMIT_NEWBRUSHES )
			Parse2DMatrix( 2, 3, (float*) side->texMat );

/*#ifdef __RANOMIZATION__
		if (randomizeBSP)
		{
			vec2_t st, st2, st3;
			st[0] = planePoints[0][0] * 0.00875;
			st[1] = planePoints[0][1] * 0.00875;

			st2[0] = planePoints[0][0] * 0.003875;
			st2[1] = planePoints[0][1] * 0.003875;

			st3[0] = planePoints[0][0] * 0.000875;
			st3[1] = planePoints[0][1] * 0.000875;

			float offset1 = rnoise(st);
			float offset2 = rnoise(st2);
			float offset3 = rnoise(st3);

			float offset = (offset1 + offset2 + offset3) / 3.0;
			offset *= 1024.0;

			planePoints[0][2] += offset;
			planePoints[1][2] += offset;
			planePoints[2][2] += offset;
		}
#endif //__RANOMIZATION__*/
		
		/* read shader name */
		GetToken( qfalse );
		strcpy( name, token );
		
#ifdef __SUPPORT_VALVE220__
		/* BP or 220? */
		{
			GetToken(qfalse);
			if (!strcmp(token, "[")) {
				side->brushType = BPRIMIT_VALVE220;
			}
			else {
				side->brushType = BPRIMIT_QUAKE;
			}
		}

		if (side->brushType == BPRIMIT_VALVE220) 
		{
			for (int comp = 0; comp < 4; ++comp) {
				side->texMat[0][comp] = 0.0;

				GetToken(qfalse);
				side->vecs[0][comp] = atof(token);
			}

			MatchToken("]");

			MatchToken("[");

			for (int comp = 0; comp < 4; ++comp) {
				side->texMat[1][comp] = 0.0;

				GetToken(qfalse);
				side->vecs[1][comp] = atof(token);
			}

			MatchToken("]");

			Sys_FPrintf(SYS_VRB, "DEBUGVALVE: Initial tokens: [ %f %f %f %f ] [ %f %f %f %f ].\n", side->vecs[0][0], side->vecs[0][1], side->vecs[0][2], side->vecs[0][3], side->vecs[1][0], side->vecs[1][1], side->vecs[1][2], side->vecs[1][3]);


			GetToken(qfalse);
			rotate = atof(token);
			GetToken(qfalse);
			scale[0] = atof(token);
			GetToken(qfalse);
			scale[1] = atof(token);

			Sys_FPrintf(SYS_VRB, "DEBUGVALVE: Rotate: %f. Scales %f %f.\n", rotate, scale[0], scale[1]);

			shift[0] = 0;
			shift[1] = 0;

			if (!scale[0]) scale[0] = 1.0f;
			if (!scale[1]) scale[1] = 1.0f;

			for (int axis = 0; axis < 2; ++axis)
				for (int comp = 0; comp < 3; ++comp)
					side->vecs[axis][comp] /= scale[axis];

			Sys_FPrintf(SYS_VRB, "DEBUGVALVE: Scaled vectors: [ %f %f %f %f ] [ %f %f %f %f ].\n", side->vecs[0][0], side->vecs[0][1], side->vecs[0][2], side->vecs[0][3], side->vecs[1][0], side->vecs[1][1], side->vecs[1][2], side->vecs[1][3]);
		}
		else 
#endif //__SUPPORT_VALVE220__
		/* bp */
		if (g_bBrushPrimit == BPRIMIT_OLDBRUSHES)
		{
			shift[0] = atof(token);
			GetToken(qfalse);
			shift[1] = atof(token);
			GetToken(qfalse);
			rotate = atof(token);
			GetToken(qfalse);
			scale[0] = atof(token);
			GetToken(qfalse);
			scale[1] = atof(token);
		}
		
		/* set default flags and values */
		sprintf( shader, "textures/%s", name );
		si = ShaderInfoForShader( shader );
		side->shaderInfo = si;
		side->surfaceFlags = si->surfaceFlags;
		side->contentFlags = si->contentFlags;
		side->compileFlags = si->compileFlags;
		side->value = si->value;

#ifdef __RANOMIZATION__
		extern qboolean StringContainsWord(const char *haystack, const char *needle);
		if (randomizeBSP && !(si->compileFlags & C_SKY) && !StringContainsWord(si->shader, "skies/"))
		{
			vec2_t st, st2, st3;
			st[0] = planePoints[0][0] * 0.00875;
			st[1] = planePoints[0][1] * 0.00875;

			st2[0] = planePoints[0][0] * 0.003875;
			st2[1] = planePoints[0][1] * 0.003875;

			st3[0] = planePoints[0][0] * 0.000875;
			st3[1] = planePoints[0][1] * 0.000875;

			float offset1 = rnoise(st);
			float offset2 = rnoise(st2);
			float offset3 = rnoise(st3);

			float offset = (offset1 + offset2 + offset3) / 3.0;
			offset *= 1024.0;

			planePoints[0][2] += offset;
			planePoints[1][2] += offset;
			planePoints[2][2] += offset;
		}
#endif //__RANOMIZATION__

		/* ydnar: gs mods: bias texture shift */
		if( si->globalTexture == qfalse )
		{
			shift[ 0 ] -= (floor( shift[ 0 ] / si->shaderWidth ) * si->shaderWidth);
			shift[ 1 ] -= (floor( shift[ 1 ] / si->shaderHeight ) * si->shaderHeight);
		}
		
		/*
			historically, there are 3 integer values at the end of a brushside line in a .map file.
			in quake 3, the only thing that mattered was the first of these three values, which
			was previously the content flags. and only then did a single bit matter, the detail
			bit. because every game has its own special flags for specifying detail, the
			traditionally game-specified CONTENTS_DETAIL flag was overridden for Q3Map 2.3.0
			by C_DETAIL, defined in q3map2.h. the value is exactly as it was before, but
			is stored in compileFlags, as opposed to contentFlags, for multiple-game
			portability. :sigh:
		*/
		
		if( TokenAvailable() )
		{
			/* get detail bit from map content flags */
			GetToken( qfalse );
			flags = atoi( token );
			if( flags & C_DETAIL )
				side->compileFlags |= C_DETAIL;
			
			/* historical */
			GetToken( qfalse );
			//% td.flags = atoi( token );
			GetToken( qfalse );
			//% td.value = atoi( token );
		}
		
		/* find the plane number */
		planenum = MapPlaneFromPoints( planePoints );
		side->planenum = planenum;
		
		/* bp: get the texture mapping for this texturedef / plane combination */
#ifdef __SUPPORT_VALVE220__
		if( g_bBrushPrimit == BPRIMIT_OLDBRUSHES || side->brushType == BPRIMIT_VALVE220) // UQ1: hmm, valve here as well?
#else //!__SUPPORT_VALVE220__
		if (g_bBrushPrimit == BPRIMIT_OLDBRUSHES)
#endif //__SUPPORT_VALVE220__
			QuakeTextureVecs( &mapplanes[ planenum ], shift, rotate, scale, side->vecs );
	}
	
	/* bp */
	if( g_bBrushPrimit == BPRIMIT_NEWBRUSHES )
	{
		UnGetToken();
		MatchToken( "}" );
		MatchToken( "}" );
	}
}



/*
RemoveDuplicateBrushPlanes
returns false if the brush has a mirrored set of planes,
meaning it encloses no volume.
also removes planes without any normal
*/

qboolean RemoveDuplicateBrushPlanes( brush_t *b )
{
	int			i, j, k;
	side_t		*sides;

	sides = b->sides;

	for ( i = 1 ; i < b->numsides ; i++ ) {

		// check for a degenerate plane
		if ( sides[i].planenum == -1) 
		{
			//Sys_Warning( b->entityNum, b->brushNum, "Degenerate plane" );
			// remove it
			for ( k = i + 1 ; k < b->numsides ; k++ ) {
				sides[k-1] = sides[k];
			}
			b->numsides--;
			i--;
			continue;
		}

		// check for duplication and mirroring
		for ( j = 0 ; j < i ; j++ ) 
		{
			if ( sides[i].planenum == sides[j].planenum ) 
			{
				//Sys_Warning( b->entityNum, b->brushNum, "Duplicate plane" );
				// remove the second duplicate
				for ( k = i + 1 ; k < b->numsides ; k++ ) {
					sides[k-1] = sides[k];
				}
				b->numsides--;
				i--;
				break;
			}

			if ( sides[i].planenum == (sides[j].planenum ^ 1) )
			{
				// mirror plane, brush is invalid
				//Sys_Warning( b->entityNum, b->brushNum, "Mirrored plane" );
				return qfalse;
			}
		}
	}
	return qtrue;
}



/*
ParseBrush()
parses a brush out of a map file and sets it up
*/

static void ParseBrush( qboolean onlyLights, qboolean onlyLightgridBrushes, qboolean onlyFoliage )
{
	brush_t	*b;

	/* parse the brush out of the map */
	ParseRawBrush( );
	
	/* only go this far? */
	if( onlyFoliage )
		return;
	
	/* set some defaults */
	buildBrush->portalareas[ 0 ] = -1;
	buildBrush->portalareas[ 1 ] = -1;
	buildBrush->entityNum = numMapEntities - 1;
	buildBrush->mapEntityNum = mapEntityNum;
	buildBrush->brushNum = entitySourceBrushes;
	
	/* if there are mirrored planes, the entire brush is invalid */
	if( !RemoveDuplicateBrushPlanes( buildBrush ) )
		return;
	
	/* get the content for the entire brush */
	SetBrushContents( buildBrush );

	/* only go this far? */
	if( onlyLights || onlyLightgridBrushes )
	{
		/* register lightgrid brush */
		if( buildBrush->compileFlags & C_LIGHTGRID )
			if( CreateBrushWindings( buildBrush ) )
				AllocateGridArea( buildBrush->mins, buildBrush->maxs );
		return;
	}
	
	/* allow detail brushes to be removed */
	if( nodetail && (buildBrush->compileFlags & C_DETAIL) )
	{
		//%	FreeBrush( buildBrush );
		return;
	}
	
	/* allow liquid brushes to be removed */
	if( nowater && (buildBrush->compileFlags & C_LIQUID ) )
	{
		//%	FreeBrush( buildBrush );
		return;
	}
	
	/* ydnar: allow hint brushes to be removed */
	if( noHint && (buildBrush->compileFlags & C_HINT) )
	{
		//%	FreeBrush( buildBrush );
		return;
	}
	
	/* finish the brush */
	b = FinishBrush();
}



/*
MoveBrushesToWorld()
takes all of the brushes from the current entity and
adds them to the world's brush list
(used by func_group)
*/
void AdjustBrushesForOrigin( entity_t *ent );
void MoveBrushesToWorld( entity_t *ent )
{
	brush_t     *b, *next;
	parseMesh_t *pm;

	/* we need to undo the common/origin adjustment, and instead shift them by the entity key origin */
	VectorScale( ent->origin, -1, ent->originbrush_origin );
	AdjustBrushesForOrigin( ent );
	VectorClear( ent->originbrush_origin );

	/* move brushes */
	for( b = ent->brushes; b != NULL; b = next )
	{
		/* get next brush */
		next = b->next;
		
		/* link opaque brushes to head of list, translucent brushes to end */
		b->entityNum = 0;
		if( b->opaque || entities[ 0 ].lastBrush == NULL )
		{
			b->next = entities[ 0 ].brushes;
			entities[ 0 ].brushes = b;
			if( entities[ 0 ].lastBrush == NULL )
				entities[ 0 ].lastBrush = b;
		}
		else
		{
			b->next = NULL;
			entities[ 0 ].lastBrush->next = b;
			entities[ 0 ].lastBrush = b;
		}
	}
	ent->brushes = NULL;
	
	/* ydnar: move colormod brushes */
	if( ent->colorModBrushes != NULL )
	{
		for( b = ent->colorModBrushes; b != NULL; b = b->nextColorModBrush )
			b->entityNum = 0;

		for( b = ent->colorModBrushes; b->nextColorModBrush != NULL; b = b->nextColorModBrush );

		b->nextColorModBrush = entities[ 0 ].colorModBrushes;
		entities[ 0 ].colorModBrushes = ent->colorModBrushes;
		
		ent->colorModBrushes = NULL;
	}
	
	/* move patches */
	if( ent->patches != NULL )
	{
		for( pm = ent->patches; pm != NULL; pm = pm->next )
			pm->entityNum = 0;

		for( pm = ent->patches; pm->next != NULL; pm = pm->next );
		
		pm->next = entities[ 0 ].patches;
		entities[ 0 ].patches = ent->patches;
		
		ent->patches = NULL;
	}
}



/*
AdjustBrushesForOrigin()
*/

void AdjustBrushesForOrigin( entity_t *ent )
{
	int i;
	side_t      *s;
	vec_t newdist;
	brush_t     *b;
	parseMesh_t *p;

	/* walk brush list */
	for ( b = ent->brushes; b != NULL; b = b->next )
	{
		/* offset brush planes */
		for ( i = 0; i < b->numsides; i++ )
		{
			/* get brush side */
			s = &b->sides[ i ];

			/* offset side plane */
			newdist = mapplanes[ s->planenum ].dist - DotProduct( mapplanes[ s->planenum ].normal, ent->originbrush_origin );

			/* find a new plane */
			s->planenum = FindFloatPlane( mapplanes[ s->planenum ].normal, newdist, 0, NULL );
		}

		/* rebuild brush windings (ydnar: just offsetting the winding above should be fine) */
		CreateBrushWindings( b );
	}

	/* walk patch list */
	for ( p = ent->patches; p != NULL; p = p->next )
	{
		for ( i = 0; i < ( p->mesh.width * p->mesh.height ); i++ )
			VectorSubtract( p->mesh.verts[ i ].xyz, ent->originbrush_origin, p->mesh.verts[ i ].xyz );
	}
}



/*
SetEntityBounds() - ydnar
finds the bounds of an entity's brushes (necessary for terrain-style generic metashaders)
*/

void SetEntityBounds( entity_t *e )
{
	int			i;
	brush_t	*b;
	parseMesh_t	*p;
	vec3_t		mins, maxs;
	const char	*value;

	/* walk the entity's brushes/patches and determine bounds */
	ClearBounds( mins, maxs );
	for( b = e->brushes; b; b = b->next )
	{
		AddPointToBounds( b->mins, mins, maxs );
		AddPointToBounds( b->maxs, mins, maxs );
	}
	for( p = e->patches; p; p = p->next )
	{
		for( i = 0; i < (p->mesh.width * p->mesh.height); i++ )
			AddPointToBounds( p->mesh.verts[ i ].xyz, mins, maxs );
	}
	
	/* try to find explicit min/max key */
	value = ValueForKey( e, "min" ); 
	if( value[ 0 ] != '\0' )
		GetVectorForKey( e, "min", mins );
	value = ValueForKey( e, "max" ); 
	if( value[ 0 ] != '\0' )
		GetVectorForKey( e, "max", maxs );
	
	/* store the bounds */
	for( b = e->brushes; b; b = b->next )
	{
		VectorCopy( mins, b->eMins );
		VectorCopy( maxs, b->eMaxs );
	}
	for( p = e->patches; p; p = p->next )
	{
		VectorCopy( mins, p->eMins );
		VectorCopy( maxs, p->eMaxs );
	}
}



/*
LoadEntityIndexMap() - ydnar
based on LoadAlphaMap() from terrain.c, a little more generic
*/

void LoadEntityIndexMap( entity_t *e )
{
	int				i, size, numLayers, w, h;
	const char		*value, *indexMapFilename, *shader;
	char			ext[ MAX_QPATH ], offset[ 4096 ], *search, *space;
	byte			*pixels;
	unsigned int	*pixels32;
	indexMap_t		*im;
	brush_t			*b;
	parseMesh_t		*p;
	
	
	/* this only works with bmodel ents */
	if( e->brushes == NULL && e->patches == NULL )
		return;
	
	/* determine if there is an index map (support legacy "alphamap" key as well) */
	value = ValueForKey( e, "_indexmap" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( e, "alphamap" );
	if( value[ 0 ] == '\0' )
		return;
	indexMapFilename = value;
	
	/* get number of layers (support legacy "layers" key as well) */
	value = ValueForKey( e, "_layers" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( e, "layers" );
	if( value[ 0 ] == '\0' )
	{
		Sys_Warning( e->mapEntityNum, "Entity with index/alpha map \"%s\" has missing \"_layers\" or \"layers\" key. Entity will not be textured properly. Check your keys/values.", indexMapFilename );
		return;
	}
	numLayers = atoi( value );
	if( numLayers < 1 )
	{
		Sys_Warning( e->mapEntityNum, "Entity with index/alpha map \"%s\" has < 1 layer (%d). Entity will not be textured properly. Check your keys/values.", indexMapFilename, numLayers );
		return;
	}
	
	/* get base shader name (support legacy "shader" key as well) */
	value = ValueForKey( mapEnt, "_shader" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( e, "shader" );
	if( value[ 0 ] == '\0' )
	{
		Sys_Warning( e->mapEntityNum, "Entity with index/alpha map \"%s\" has missing \"_shader\" or \"shader\" key. Entity will not be textured properly. Check your keys/values.", indexMapFilename );
		return;
	}
	shader = value;
	
	/* note it */
	Sys_FPrintf( SYS_VRB, "Entity %d (%s) has shader index map \"%s\"\n",  mapEnt->mapEntityNum, ValueForKey( e, "classname" ), indexMapFilename );
	
	/* get index map file extension */
	ExtractFileExtension( indexMapFilename, ext );
	
	/* handle tga image */
	if( !Q_stricmp( ext, "tga" ) )
	{
		/* load it */
		Load32BitImage( indexMapFilename, &pixels32, &w, &h );
		
		/* convert to bytes */
		size = w * h;
		pixels = (byte *)safe_malloc( size );
		for( i = 0; i < size; i++ )
		{
			pixels[ i ] = ((pixels32[ i ] & 0xFF) * numLayers) / 256;
			if( pixels[ i ] >= numLayers )
				pixels[ i ] = numLayers - 1;
		}
		
		/* free the 32 bit image */
		free( pixels32 );
	}
	else
	{
		/* load it */
		Load256Image( indexMapFilename, &pixels, NULL, &w, &h );
		
		/* debug code */
		//%	Sys_Printf( "-------------------------------" );
		
		/* fix up out-of-range values */
		size = w * h;
		for( i = 0; i < size; i++ )
		{
			if( pixels[ i ] >= numLayers )
				pixels[ i ] = numLayers - 1;
			
			/* debug code */
			//%	if( (i % w) == 0 )
			//%		Sys_Printf( "\n" );
			//%	Sys_Printf( "%c", pixels[ i ] + '0' );
		}
		
		/* debug code */
		//%	Sys_Printf( "\n-------------------------------\n" );
	}
	
	/* the index map must be at least 2x2 pixels */
	if( w < 2 || h < 2 )
	{
		Sys_Warning( e->mapEntityNum, "Entity with index/alpha map \"%s\" is smaller than 2x2 pixels. Entity will not be textured properly. Check your keys/values.", indexMapFilename );
		free( pixels );
		return;
	}

	/* create a new index map */
	im = (indexMap_t *)safe_malloc( sizeof( *im ) );
	memset( im, 0, sizeof( *im ) );
	
	/* set it up */
	im->w = w;
	im->h = h;
	im->numLayers = numLayers;
	strcpy( im->name, indexMapFilename );
	strcpy( im->shader, shader );
	im->pixels = pixels;
	
	/* get height offsets */
	value = ValueForKey( mapEnt, "_offsets" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( e, "offsets" );
	if( value[ 0 ] != '\0' )
	{
		/* value is a space-seperated set of numbers */
		strcpy( offset, value );
		search = offset;
		
		/* get each value */
		for( i = 0; i < 256 && *search != '\0'; i++ )
		{
			space = strstr( search, " " );
			if( space != NULL )
				*space = '\0';
			im->offsets[ i ] = atof( search );
			if( space == NULL )
				break;
			search = space + 1;
		}
	}
	
	/* store the index map in every brush/patch in the entity */
	for( b = e->brushes; b != NULL; b = b->next )
		b->im = im;
	for( p = e->patches; p != NULL; p = p->next )
		p->im = im;
}

/*
ParseMapEntity()
parses a single entity out of a map file
*/

static qboolean ParseMapEntity( qboolean onlyLights, qboolean onlyLightgridBrushes, qboolean onlyFoliage, qboolean externalFile )
{
	epair_t			*ep;
	const char		*classname, *value;
	float			lightmapScale;
	vec3_t          lightmapAxis;
	int			    smoothNormals;
	int				vertTexProj;
	char			shader[ MAX_QPATH ];
	shaderInfo_t	*celShader = NULL;
	brush_t			*brush;
	parseMesh_t		*patch;
	qboolean		funcGroup;
	char			castShadows, recvShadows;
	qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
	vec3_t          minlight, minvertexlight, ambient, colormod;
	float           patchQuality, patchSubdivision;
	
	/* eof check */
	if( !GetToken( qtrue ) )
		return qfalse;
	
	/* conformance check */
	if( strcmp( token, "{" ) )
	{
		Sys_Warning( numMapEntities, "ParseEntity: { not found, found %s on line %d - last entity was at: <%4.2f, %4.2f, %4.2f>. Continuing to process map, but resulting BSP may be invalid.", token, scriptline, entities[ numEntities ].origin[ 0 ], entities[ numEntities ].origin[ 1 ], entities[ numEntities ].origin[ 2 ] );
		return qfalse;
	}
	
	/* range check */
	if( numEntities >= MAX_MAP_ENTITIES )
		Sys_Error( "MAX_MAP_ENTITIES (%d) exceeded", MAX_MAP_ENTITIES );
	
	/* setup */
	entitySourceBrushes = 0;
	mapEnt = &entities[ numEntities ];
	numEntities++;
	memset( mapEnt, 0, sizeof( *mapEnt ) );
	
	/* ydnar: true entity numbering */
	if( externalFile )
		mapEnt->mapEntityNum = 0 - numEntities;
	else
	{
		mapEnt->mapEntityNum = numMapEntities;
		numMapEntities++;
	}

	/* loop */
	while( 1 )
	{
		/* get initial token */
		if( !GetToken( qtrue ) )
		{
			Sys_Warning( numMapEntities, "ParseEntity: EOF without closing brace. Continuing to process map, but resulting BSP may be invalid." );
			return qfalse;
		}
		
		if( !strcmp( token, "}" ) )
			break;
		
		if( !strcmp( token, "{" ) )
		{
			/* parse a brush or patch */
			if( !GetToken( qtrue ) )
				break;
			
			/* check */
			if( !strcmp( token, "patchDef2" ) )
			{
				numMapPatches++;
				ParsePatch( onlyLights );
			}
			else if( !strcmp( token, "terrainDef" ) )
			{
				//% ParseTerrain();
				Sys_Error( numMapEntities, 0, "Terrain entity parsing not supported in this build." );	/* ydnar */
			}
			else if( !strcmp( token, "brushDef" ) )
			{
				if( g_bBrushPrimit == BPRIMIT_OLDBRUSHES )
					Sys_Error( "Old brush format not allowed in new brush format map" );
				g_bBrushPrimit = BPRIMIT_NEWBRUSHES;
				
				/* parse brush primitive */
				ParseBrush( onlyLights, onlyLightgridBrushes, onlyFoliage );
			}
			else
			{
				if( g_bBrushPrimit == BPRIMIT_NEWBRUSHES )
					Sys_Error( "New brush format not allowed in old brush format map" );
				g_bBrushPrimit = BPRIMIT_OLDBRUSHES;
				
				/* parse old brush format */
				UnGetToken();
				ParseBrush( onlyLights, onlyLightgridBrushes, onlyFoliage );
			}
			entitySourceBrushes++;
		}
		else
		{
			/* parse a key / value pair */
			ep = ParseEPair();
			
			/* ydnar: 2002-07-06 fixed wolf bug with empty epairs */
			if( ep->key[ 0 ] != '\0' && ep->value[ 0 ] != '\0' )
			{
				ep->next = mapEnt->epairs;
				mapEnt->epairs = ep;
			}
		}
	}
	
	/* ydnar: get classname */
	classname = ValueForKey( mapEnt, "classname" );
	
	/* ydnar: only lights? */
	if( onlyLights && Q_strncasecmp( classname, "light", 5 ) )
	{
		numEntities--;
		return qtrue;
	}

	/* vortex: only foliage? */
	if( onlyFoliage && Q_strncasecmp( classname, "misc_model", 10 ))
	{
		numEntities--;
		return qtrue;
	}

	/* vortex: only light brushes */
	if( onlyLightgridBrushes )
	{	
		if( !onlyLights || Q_strncasecmp( classname, "light", 5 ) )
		{
			numEntities--;
			return qtrue;
		}
	}
	
	/* ydnar: determine if this is a func_group */
	if( !Q_stricmp( "func_group", classname ) )
		funcGroup = qtrue;
	else
		funcGroup = qfalse;
	
	/* get explicit shadow flags */
	GetEntityShadowFlags( mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse );
	
	/* vortex: get lightmap scaling value for this entity */
	GetEntityLightmapScale( mapEnt, &lightmapScale, 0);

	/* vortex: get lightmap axis for this entity */
	GetEntityLightmapAxis( mapEnt, lightmapAxis, NULL );

	/* vortex: per-entity normal smoothing */
	GetEntityNormalSmoothing( mapEnt, &smoothNormals, 0);

	/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
	GetEntityMinlightAmbientColor( mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue );
	if( mapEnt == &entities[ 0 ] )
	{
		/* worldspawn have it empty, since it's keys sets global parms */
		VectorSet( minlight, 0, 0, 0 );
		VectorSet( minvertexlight, 0, 0, 0 );
		VectorSet( ambient, 0, 0, 0 );
		VectorSet( colormod, 1, 1, 1 );
	}

	/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
	GetEntityPatchMeta( mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

	/* vortex: vertical texture projection */
	if( strcmp( "", ValueForKey( mapEnt, "_vtcproj" ) ) || strcmp( "", ValueForKey( mapEnt, "_vp" ) ) )
	{
		vertTexProj = IntForKey(mapEnt, "_vtcproj");
		if (vertTexProj <= 0.0f)
			vertTexProj = IntForKey(mapEnt, "_vp");
	}
	else
		vertTexProj = 0;

	/* ydnar: get cel shader :) for this entity */
	value = ValueForKey( mapEnt, "_celshader" );
	if( value[ 0 ] == '\0' )	
		value = ValueForKey( &entities[ 0 ], "_celshader" );
	if( value[ 0 ] != '\0' )
	{
		sprintf( shader, "textures/%s", value );
		celShader = ShaderInfoForShader( shader );
		//Sys_FPrintf (SYS_VRB, "Entity %d (%s) has cel shader %s\n", mapEnt->mapEntityNum, classname, celShader->shader );
	}
	else
		celShader = NULL;

	/* vortex: _nonsolid forces detail non-solid brush */
	forceNonSolid = ((IntForKey(mapEnt, "_nonsolid") > 0) || (IntForKey(mapEnt, "_ns") > 0)) ? qtrue : qfalse;

	/* vortex: preserve original face winding, don't clip by bsp tree */
	forceNoClip = ((IntForKey(mapEnt, "_noclip") > 0) || (IntForKey(mapEnt, "_nc") > 0)) ? qtrue : qfalse;

	/* vortex: do not apply t-junction fixing (preserve original face winding) */
	forceNoTJunc = ((IntForKey(mapEnt, "_notjunc") > 0) || (IntForKey(mapEnt, "_ntj") > 0)) ? qtrue : qfalse;

	/* attach stuff to everything in the entity */
	for( brush = mapEnt->brushes; brush != NULL; brush = brush->next )
	{
		brush->entityNum = mapEnt->mapEntityNum;
		brush->mapEntityNum = mapEnt->mapEntityNum;
		brush->castShadows = castShadows;
		brush->recvShadows = recvShadows;
		brush->lightmapScale = lightmapScale;
		VectorCopy( lightmapAxis, brush->lightmapAxis ); /* vortex */
		brush->smoothNormals = smoothNormals; /* vortex */
		brush->noclip = forceNoClip; /* vortex */
		brush->noTJunc = forceNoTJunc; /* vortex */
		brush->vertTexProj = vertTexProj; /* vortex */
		VectorCopy( minlight, brush->minlight ); /* vortex */
		VectorCopy( minvertexlight, brush->minvertexlight ); /* vortex */
		VectorCopy( ambient, brush->ambient ); /* vortex */
		VectorCopy( colormod, brush->colormod ); /* vortex */
		brush->celShader = celShader;
		if (forceNonSolid == qtrue)
		{
			brush->detail = qtrue;
			brush->nonsolid = qtrue;
			brush->noclip = qtrue;
		}
	}
	
	for( patch = mapEnt->patches; patch != NULL; patch = patch->next )
	{
		patch->entityNum = mapEnt->mapEntityNum;
		patch->mapEntityNum = mapEnt->mapEntityNum;
		patch->castShadows = castShadows;
		patch->recvShadows = recvShadows;
		patch->lightmapScale = lightmapScale;
		VectorCopy( lightmapAxis, patch->lightmapAxis ); /* vortex */
		patch->smoothNormals = smoothNormals; /* vortex */
		patch->vertTexProj = vertTexProj; /* vortex */
		patch->celShader = celShader;
		patch->patchMeta = forceMeta; /* vortex */
		patch->patchQuality = patchQuality; /* vortex */
		patch->patchSubdivisions = patchSubdivision; /* vortex */
		VectorCopy( minlight, patch->minlight ); /* vortex */
		VectorCopy( minvertexlight, patch->minvertexlight ); /* vortex */
		VectorCopy( ambient, patch->ambient ); /* vortex */
		VectorCopy( colormod, patch->colormod ); /* vortex */
		patch->nonsolid = forceNonSolid;
	}

	/* vortex: store map entity num */
	{
		char buf[32];
		sprintf( buf, "%i", mapEnt->mapEntityNum ); 
		SetKeyValue( mapEnt, "_mapEntityNum", buf );
	}
	
	/* ydnar: gs mods: set entity bounds */
	SetEntityBounds( mapEnt );
	
	/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
	LoadEntityIndexMap( mapEnt );
	
	/* get entity origin and adjust brushes */
	GetVectorForKey( mapEnt, "origin", mapEnt->origin );
	if ( mapEnt->originbrush_origin[ 0 ] || mapEnt->originbrush_origin[ 1 ] || mapEnt->originbrush_origin[ 2 ] )
		AdjustBrushesForOrigin( mapEnt );

	/* group_info entities are just for editor grouping (fixme: leak!) */
	if( !Q_stricmp( "group_info", classname ) )
	{
		numEntities--;
		return qtrue;
	}
	
	/* group entities are just for editor convenience, toss all brushes into worldspawn */
	if( funcGroup )
	{
		MoveBrushesToWorld( mapEnt );
		numEntities--;
		return qtrue;
	}
	
	/* done */
	return qtrue;
}


/*
LoadMapFile()
loads a map file into a list of entities
*/

bool NO_BOUNDS_IMPROVEMENT = false;

void LoadMapFile( char *filename, qboolean onlyLights, qboolean onlyLightgridBrushes, qboolean onlyFoliage, qboolean externalFile )
{		
	FILE		*file;
	brush_t		*b;
	int			oldNumEntities, numMapBrushes;

	/* note it */
	if( onlyFoliage == qfalse )
		Sys_PrintHeading ( "--- LoadMapFile ---\n" );

	/* hack */
	file = SafeOpenRead( filename );
	fclose( file );
	
	/* load the map file */
	LoadScriptFile( filename, -1 );
	
	/* setup */
	if( onlyLights || onlyFoliage || onlyLightgridBrushes )
		oldNumEntities = numEntities;
	else
		numEntities = 0;
	
	/* initial setup */
	if ( onlyFoliage == qfalse )
	{
		numMapDrawSurfs = 0;
		c_detail = 0;
		g_bBrushPrimit = BPRIMIT_UNDEFINED;

		/* allocate a very large temporary brush for building the brushes as they are loaded */
		buildBrush = AllocBrush( MAX_BUILD_SIDES );
	}

	/* parse the map file */
	while( ParseMapEntity( onlyLights, onlyLightgridBrushes, onlyFoliage, externalFile ) );

	/* light loading */
	if ( onlyFoliage )
	{
		/* emit some statistics */
		Sys_Printf( "%9d foliage models\n", numEntities - oldNumEntities );
	}
	else if( onlyLights )
	{
		/* emit some statistics */
		Sys_Printf( "%9d light entities\n", numEntities - oldNumEntities );
	}
	else if( onlyLightgridBrushes )
	{
		/* emit some statistics */
		Sys_Printf( "%9d lightgrid brushes\n", numGridAreas );
	}
	else
	{
		/* set map bounds */
		ClearBounds( mapMins, mapMaxs );
		ClearBounds(mapPlayableMins, mapPlayableMaxs);
		for( b = entities[ 0 ].brushes; b; b = b->next )
		{
			b->isOriginalMapSurface = qtrue; // UQ1: marking this for foliage generator usage...

			AddPointToBounds( b->mins, mapMins, mapMaxs );
			AddPointToBounds( b->maxs, mapMins, mapMaxs );
			
			// UQ1: Also record actual map playable area mins/maxs...
			if (!(b->compileFlags & C_SKY) 
				&& !(b->compileFlags & C_SKIP) 
				&& !(b->compileFlags & C_HINT) 
				&& !(b->compileFlags & C_NODRAW))
			{
				for (int s = 0; s < b->numsides; s++)
				{
					if (!(b->sides[s].compileFlags & C_SKY) 
						&& !(b->sides[s].compileFlags & C_SKIP) 
						&& !(b->sides[s].compileFlags & C_HINT) 
						&& !(b->sides[s].compileFlags & C_NODRAW))
					{
						AddPointToBounds(b->mins, mapPlayableMins, mapPlayableMaxs);
						AddPointToBounds(b->maxs, mapPlayableMins, mapPlayableMaxs);
					}
				}
			}
		}

		if (mapPlayableMins[0] == 99999 && mapPlayableMins[1] == 99999 && mapPlayableMins[2] == 99999 && mapPlayableMaxs[0] == -99999 && mapPlayableMaxs[1] == -99999 && mapPlayableMaxs[2] == -99999)
		{// Failed to find any surfaces, must be an empty map (space, etc), allow sky...
			Sys_Printf("* Empty map found (space, etc), calculating new valid bounds.\n");

			NO_BOUNDS_IMPROVEMENT = true;

			if (mapMins[0] == 99999 && mapMins[1] == 99999 && mapMins[2] == 99999 && mapMaxs[0] == -99999 && mapMaxs[1] == -99999 && mapMaxs[2] == -99999)
			{
				ClearBounds(mapPlayableMins, mapPlayableMaxs);
				for (b = entities[0].brushes; b; b = b->next)
				{
					if (!(b->compileFlags & C_SKIP)
						&& !(b->compileFlags & C_HINT)
						&& !(b->compileFlags & C_NODRAW))
					{
						for (int s = 0; s < b->numsides; s++)
						{
							if (!(b->sides[s].compileFlags & C_SKIP)
								&& !(b->sides[s].compileFlags & C_HINT)
								&& !(b->sides[s].compileFlags & C_NODRAW))
							{
								AddPointToBounds(b->mins, mapPlayableMins, mapPlayableMaxs);
								AddPointToBounds(b->maxs, mapPlayableMins, mapPlayableMaxs);
							}
						}
					}
				}
			}
			else
			{
				VectorCopy(mapMins, mapPlayableMins);
				VectorCopy(mapMaxs, mapPlayableMaxs);
			}
		}
		else if (mapPlayableMaxs[2] - mapPlayableMins[2] <= 256)
		{
			Sys_Printf("* Bad playable bounds found (probably space, completely flat, etc), using full map bounds.\n");

			NO_BOUNDS_IMPROVEMENT = true;
			VectorCopy(mapMins, mapPlayableMins);
			VectorCopy(mapMaxs, mapPlayableMaxs);
		}

		// Override playable maxs height with the full map version, we only want the lower extent of playable area...
		mapMaxs[2] = mapPlayableMaxs[2];

		/* region seal */
		if ( mapRegion == qtrue )
		{
			/* vortex 03.01.2015: just ignore leaks instead */
			SetKeyValue( &entities[ 0 ], "_ignoreleaks", "1" );
			if ( mapRegionBrushes == qfalse )
			{
				VectorCopy( mapMins, mapRegionMins );
				VectorCopy( mapMaxs, mapRegionMaxs );
			}
			/*
			brush_t *b[6];

			if ( mapRegionBrushes == qfalse )
			{
				VectorCopy( mapMins, mapRegionMins );
				VectorCopy( mapMaxs, mapRegionMaxs );
			}

			shaderInfo_t *si = ShaderInfoForShader("textures/common/caulk");
			b[0] = BrushFromBounds(mapRegionMins[0] - 8, mapRegionMins[1] - 8, mapRegionMins[2] - 8, mapRegionMins[0]    , mapRegionMaxs[1] + 8, mapRegionMaxs[2] + 8, si);
			b[1] = BrushFromBounds(mapRegionMaxs[0]    , mapRegionMins[1] - 8, mapRegionMins[2] - 8, mapRegionMaxs[0] + 8, mapRegionMaxs[1] + 8, mapRegionMaxs[2] + 8, si);
			b[2] = BrushFromBounds(mapRegionMins[0] - 8, mapRegionMins[1] - 8, mapRegionMins[2] - 8, mapRegionMaxs[0] + 8, mapRegionMins[1]    , mapRegionMaxs[2] + 8, si);
			b[3] = BrushFromBounds(mapRegionMins[0] - 8, mapRegionMaxs[1]    , mapRegionMins[2] - 8, mapRegionMaxs[0] + 8, mapRegionMaxs[1] + 8, mapRegionMaxs[2] + 8, si);
			b[4] = BrushFromBounds(mapRegionMins[0] - 8, mapRegionMins[1] - 8, mapRegionMins[2] - 8, mapRegionMaxs[0] + 8, mapRegionMaxs[1] + 8, mapRegionMins[2]    , si);
			b[5] = BrushFromBounds(mapRegionMins[0] - 8, mapRegionMins[1] - 8, mapRegionMaxs[2]    , mapRegionMaxs[0] + 8, mapRegionMaxs[1] + 8, mapRegionMaxs[2] + 8, si);
			b[0]->next = b[1];
			b[1]->next = b[2];
			b[2]->next = b[3];
			b[3]->next = b[4];
			b[4]->next = b[5];
			b[5]->next = entities[ 0 ].brushes;
			entities[ 0 ].brushes = b[0];
			entities[ 0 ].numBrushes += 6;
			*/
		}

		/* get brush counts */
		numMapBrushes = CountBrushList( entities[ 0 ].brushes );
		if( (float) c_detail / (float) numMapBrushes < 0.10f && numMapBrushes > 500 )
			Sys_Warning( "Over 90 percent structural map detected. Compile time may be adversely affected." );

		// UQ1: Mark all BSP brushes as being from the original .map file, so the chunks system can skip the original brushes for trees etc...
		brush_t *brushes = entities[0].brushes;
		for (brushes; brushes != NULL; brushes = brushes->next)
			brushes->isMapFileBrush = qtrue;

		/* emit some statistics */
		Sys_Printf(  "%9d total world brushes\n", numMapBrushes );
		numMapBrushes = CountBrushList( entities[ 0 ].brushes );
		//Sys_Printf(  "%9d total world brushes with trees added\n", numMapBrushes );

		Sys_Printf( "%9d detail brushes\n", c_detail );
		Sys_Printf( "%9d patches\n", numMapPatches);
		Sys_Printf( "%9d boxbevels\n", c_boxbevels);
		Sys_Printf( "%9d edgebevels\n", c_edgebevels);
		Sys_Printf( "%9d entities\n", numEntities );
		Sys_Printf( "%9d planes\n", nummapplanes);
		Sys_Printf( "%9d areaportals\n", c_areaportals);
		Sys_Printf( "Size: { %.0f %.0f %.0f } { %.0f %.0f %.0f }\n", mapMins[ 0 ], mapMins[ 1 ], mapMins[ 2 ], mapMaxs[ 0 ], mapMaxs[ 1 ], mapMaxs[ 2 ]);
		Sys_Printf( "Playable Size: { %.0f %.0f %.0f } { %.0f %.0f %.0f }\n", mapPlayableMins[0], mapPlayableMins[1], mapPlayableMins[2], mapPlayableMaxs[0], mapPlayableMaxs[1], mapPlayableMaxs[2]);
		
		/* region stats */
		if ( mapRegion == qtrue )
			Sys_Printf( "Region: { %.0f %.0f %.0f } { %.0f %.0f %.0f }\n", mapRegionMins[ 0 ], mapRegionMins[ 1 ], mapRegionMins[ 2 ], mapRegionMaxs[ 0 ], mapRegionMaxs[ 1 ], mapRegionMaxs[ 2 ]);

		/* write bogus map */
		if( fakemap )
			WriteBSPBrushMap( "fakemap.map", entities[ 0 ].brushes );

		//mapplanes = (plane_t*)realloc(mapplanes, sizeof(plane_t)*nummapplanes); // UQ1: Doh, can't do this here... the generation process needs an unknown number of free slots, and dynamic alloc would be slow and highly fragmented for this many instances

	}
}

/*
CheckMapForErrors()
check map for possible errors
*/

int CheckMapForErrors( void )
{
	int entityNum, numErrors = 0, i;
	qboolean foundDuplicate;
	const char *classname, *saveid, *saveid2;
	shaderInfo_t *si;
	brush_t *b;
	parseMesh_t *pm;
	entity_t *e;

	for( entityNum = 0; entityNum < numEntities; entityNum++ )
	{
		e = &entities[ entityNum ];
		classname = ValueForKey( e, "classname" );

		/* check brushes for missing textures */
		for( b = e->brushes; b; b = b->next )
		{
			for( i = 0; i < b->numsides; i++ )
			{
				si = b->sides[ i ].shaderInfo;
				if( si == NULL )
					continue;
				if( si->warnNoShaderImage == qtrue )
				{
					if( e->mapEntityNum >= 0 )
						Sys_Warning( b->entityNum, b->brushNum, "Failed to load shader image '%s'", si->shader );
					else
					{
						/* external entity, just show single warning */
						Sys_Warning( "Failed to load shader image '%s' for entity '%s'", si->shader, classname );
						si->warnNoShaderImage = qfalse;
					}
				}
			}
		}
		
		/* check patches for missing textures */
		for( pm = e->patches; pm; pm = pm->next )
		{
			si = pm->shaderInfo;
			if( si == NULL )
				continue;
			if( si->warnNoShaderImage == qtrue )
			{
				if( e->mapEntityNum >= 0 )
					Sys_Warning( pm->entityNum, pm->brushNum, "Failed to load shader image '%s'", si->shader );
				else
				{
					/* external entity, just show single warning */
					Sys_Warning( "Failed to load shader image '%s' for entity '%s'", si->shader, classname );
					si->warnNoShaderImage = qfalse;
				}
			}
		}

		/* skip the rest of checks for worldspawn and external entities */
		if( entityNum == 0 || e->mapEntityNum < 0 )
			continue;

		/* check for func_ entities with no geometry added */
		if( !Q_strncasecmp(classname, "func_", 5) || !Q_stricmp(classname, "effect_glares") || !Q_stricmp(classname, "trigger_changelevel") || !Q_stricmp(classname, "trigger_hurt") ||
			!Q_stricmp(classname, "trigger_environment") || !Q_stricmp(classname, "trigger_event") || !Q_stricmp(classname, "trigger_exit") || !Q_stricmp(classname, "trigger_multi") ||
			!Q_stricmp(classname, "trigger_once") || !Q_stricmp(classname, "trigger_savegame") || !Q_stricmp(classname, "trigger_speech") || !Q_stricmp(classname, "trigger_teleport") )
		{
			if( !e->brushes && !e->patches )
				Sys_Warning( e->mapEntityNum, "Group entity %s with no brushes/patches attached", classname );
		}

		/* check for duplicate or missing entity save id */
		if ( useEntitySaveId == qtrue )
		{
			/* exclude entities to be merged to worldspawn */
			if (Q_stricmp(classname, "func_group") && Q_stricmp(classname, "misc_model") && Q_stricmp(classname, "_skybox") && Q_strncasecmp(classname, "light", 5))
			{
				/* get saveid */
				saveid = ValueForKey( e, "sid" ); 
				if( saveid[ 0 ] == '\0' )
				{
					Sys_Warning( e->mapEntityNum, "Missing SaveID" );
					numErrors++;
				}
				else
				{
					/* get entity with duplicate saveid */
					foundDuplicate = qfalse;
					for( i = entityNum + 1; i < numEntities; i++ )
					{
						saveid2 = ValueForKey( &entities[ i ], "sid" ); 
						if( !Q_stricmp( saveid, saveid2) )
						{
							if ( foundDuplicate == qfalse )
								Sys_Warning( e->mapEntityNum, "Duplicate SaveID (%s)", saveid );
							Sys_Warning( entities[ i ].mapEntityNum, "Duplicate SaveID (%s)", saveid );
							numErrors++;
							foundDuplicate = qtrue;
						}
					}
				}
			}
		}
	}

	return numErrors;
}