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
#define MODEL_C


/* dependencies */
#include "q3map2.h"

#include "../libs/picomodel/proctree/proctree.h"

extern qboolean USE_LODMODEL;
extern qboolean USE_CONVEX_HULL_MODELS;
extern int USE_BOX_MODELS;
extern qboolean FORCED_STRUCTURAL;

extern qboolean StringContainsWord(const char *haystack, const char *needle);
extern qboolean RemoveDuplicateBrushPlanes(brush_t *b);
extern void CullSides(entity_t *e);
extern void CullSidesStats(void);
extern void ConvexHull(picoModel_t *model, char *fileNameOut);

extern int g_numHiddenFaces, g_numCoinFaces;

extern qboolean FORCED_MODEL_META;

#define ForceCrash() { shaderInfo_t *blah = NULL; blah->lmCustomWidth = 1; }

/*
PicoPrintFunc()
callback for picomodel.lib
*/

void PicoPrintFunc(int level, const char *str)
{
	if (str == NULL)
		return;
	switch (level)
	{
	case PICO_NORMAL:
		Sys_Printf("%s\n", str);
		break;

	case PICO_VERBOSE:
		Sys_FPrintf(SYS_VRB, "%s\n", str);
		break;

	case PICO_WARNING:
		Sys_Printf("WARNING: %s\n", str);
		break;

	case PICO_ERROR:
		Sys_Printf("ERROR: %s\n", str);
		break;

	case PICO_FATAL:
		Error("ERROR: %s\n", str);
		break;
	}
}



/*
PicoLoadFileFunc()
callback for picomodel.lib
*/

void PicoLoadFileFunc(char *name, byte **buffer, int *bufSize)
{
	*bufSize = vfsLoadFile((const char*)name, (void**)buffer, 0);
}

/*
FindModel() - ydnar
finds an existing picoModel and returns a pointer to the picoModel_t struct or NULL if not found
*/

picoModel_t *FindModel(const char *name, int frame)
{
	int	i;

	/* init */
	if (numPicoModels <= 0)
		memset(picoModels, 0, sizeof(picoModels));

	/* dummy check */
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* search list */
	for (i = 0; i < MAX_MODELS; i++)
	if (picoModels[i] != NULL && !strcmp(PicoGetModelName(picoModels[i]), name) && PicoGetModelFrameNum(picoModels[i]) == frame)
		return picoModels[i];

	/* no matching picoModel found */
	return NULL;
}



/*
LoadModel() - ydnar
loads a picoModel and returns a pointer to the picoModel_t struct or NULL if not found
*/

picoModel_t *LoadModel(const char *name, int frame)
{
	int	i;
	picoModel_t	*model, **pm;

	/* init */
	if (numPicoModels <= 0)
		memset(picoModels, 0, sizeof(picoModels));

	/* dummy check */
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* try to find existing picoModel */
	model = FindModel(name, frame);
	if (model != NULL)
		return model;

	/* none found, so find first non-null picoModel */
	pm = NULL;
	for (i = 0; i < MAX_MODELS; i++)
	{
		if (picoModels[i] == NULL)
		{
			pm = &picoModels[i];
			break;
		}
	}

	/* too many picoModels? */
	if (pm == NULL)
		Error("MAX_MODELS (%d) exceeded, there are too many model files referenced by the map.", MAX_MODELS);

	/* attempt to parse model */
	*pm = PicoLoadModel((char*)name, frame);

	/* if loading failed, make a bogus model to silence the rest of the warnings */
	if (*pm == NULL)
	{
		if (StringContainsWord(name, "_collision"))
		{
			return NULL;
		}
		else
		{
			Sys_Printf("LoadModel: failed to load model %s frame %i. Fix your map!", name, frame);
		}

		/* allocate a new model */
		*pm = PicoNewModel();
		if (*pm == NULL)
			return NULL;

		/* set data */
		PicoSetModelName(*pm, (char *)name);
		PicoSetModelFrameNum(*pm, frame);
	}

	/* debug code */
#if 0
	{
		int				numSurfaces, numVertexes;
		picoSurface_t	*ps;


		Sys_Printf( "Model %s\n", name );
		numSurfaces = PicoGetModelNumSurfaces( *pm );
		for( i = 0; i < numSurfaces; i++ )
		{
			ps = PicoGetModelSurface( *pm, i );
			numVertexes = PicoGetSurfaceNumVertexes( ps );
			Sys_Printf( "Surface %d has %d vertexes\n", i, numVertexes );
		}
	}
#endif

	/* set count */
	if (*pm != NULL)
		numPicoModels++;

	/* return the picoModel */
	return *pm;
}

/*
PreloadModel() - vortex
preloads picomodel, returns true once loaded a model
*/

qboolean PreloadModel(const char *name, int frame)
{
	picoModel_t	*model;

	/* try to find model */
	model = FindModel(name, frame);
	if (model != NULL)
		return qfalse;

	/* get model */
	model = LoadModel(name, frame);
	if (model != NULL)
		return qtrue;

	/* fail */
	return qfalse;
}

/*
InsertModel() - ydnar
adds a picomodel into the bsp
*/

float Distance(vec3_t pos1, vec3_t pos2)
{
	vec3_t vLen;
	VectorSubtract(pos1, pos2, vLen);
	return VectorLength(vLen);
}

extern void LoadShaderImages(shaderInfo_t *si);
extern void Decimate(picoModel_t *model, char *fileNameOut);

int numSolidSurfs = 0, numHeightCulledSurfs = 0, numSizeCulledSurfs = 0, numExperimentalCulled = 0, numBoundsCulledSurfs = 0;

int removed_numHiddenFaces = 0;
int removed_numCoinFaces = 0;

qboolean InsertModelSideInBrush(side_t *side, brush_t *b)
{
	int			i, s;
	plane_t		*plane;

	/* ignore sides w/o windings or shaders */
	if (side->winding == NULL || side->shaderInfo == NULL)
		return qtrue;

	/* ignore culled sides and translucent brushes */
	if (side->culled == qtrue || (b->compileFlags & C_TRANSLUCENT))
		return qfalse;

	/* side iterator */
	for (i = 0; i < b->numsides; i++)
	{
		/* fail if any sides are caulk */
		if (b->sides[i].compileFlags & C_NODRAW)
			return qfalse;

		/* check if side's winding is on or behind the plane */
		plane = &mapplanes[b->sides[i].planenum];
		s = WindingOnPlaneSide(side->winding, plane->normal, plane->dist);
		if (s == SIDE_FRONT || s == SIDE_CROSS)
			return qfalse;
	}

	/* don't cull autosprite or polygonoffset surfaces */
	if (side->shaderInfo)
	{
		if (side->shaderInfo->autosprite || side->shaderInfo->polygonOffset)
			return qfalse;
	}

	/* inside */
	side->culled = qtrue;
	removed_numHiddenFaces++;
	return qtrue;
}

int NUM_ORIGINAL_BRUSHES = 0;

void InsertModelCullSides(entity_t *e, brush_t *inBrush)
{
	const float CULL_EPSILON = 0.1f;
	int			numPoints;
	int			i, j, k, l, first, second, dir;
	winding_t	*w1, *w2;
	brush_t		*b1, *b2;
	side_t		*side1, *side2;
	//int			current = 0;// , count = 0;

	removed_numHiddenFaces = 0;
	removed_numCoinFaces = 0;

	//for (b1 = e->brushes; b1; b1 = b1->next)
	//	count++;

	b1 = inBrush;

	/* brush interator 1 */
	//for (b1 = e->brushes; b1; b1 = b1->next)
	{
		int current = 0;

		//printLabelledProgress("CullSides", current, count);
		//current++;

		/* sides check */
		if (b1->numsides < 1)
			//continue;
			return;

		/* brush iterator 2 */
		for (b2 = b1->next; b2; b2 = b2->next)
		{
			current++;

			if (current >= NUM_ORIGINAL_BRUSHES)
				break;

			/* sides check */
			if (b2->numsides < 1)
				continue;

			/* original check */
			if (b1->original == b2->original && b1->original != NULL)
				continue;

			/* bbox check */
			j = 0;
			for (i = 0; i < 3; i++)
				if (b1->mins[i] > b2->maxs[i] || b1->maxs[i] < b2->mins[i])
					j++;
			if (j)
				continue;

#if 1
			/* cull inside sides */
			//#pragma omp parallel for ordered num_threads(numthreads)
			for (i = 0; i < b1->numsides; i++)
				InsertModelSideInBrush(&b1->sides[i], b2);
			//#pragma omp parallel for ordered num_threads(numthreads)
			for (i = 0; i < b2->numsides; i++)
				InsertModelSideInBrush(&b2->sides[i], b1);
#endif

			/* side iterator 1 */
			//#pragma omp parallel for ordered num_threads(numthreads)
			for (i = 0; i < b1->numsides; i++)
			{
				/* winding check */
				side1 = &b1->sides[i];
				w1 = side1->winding;
				if (w1 == NULL)
					continue;
				numPoints = w1->numpoints;
				if (side1->shaderInfo == NULL)
					continue;

				/* side iterator 2 */
				for (j = 0; j < b2->numsides; j++)
				{
					/* winding check */
					side2 = &b2->sides[j];
					w2 = side2->winding;
					if (w2 == NULL)
						continue;
					if (side2->shaderInfo == NULL)
						continue;
					if (w1->numpoints != w2->numpoints)
						continue;
					if (side1->culled == qtrue && side2->culled == qtrue)
						continue;

					/* compare planes */
					if ((side1->planenum & ~0x00000001) != (side2->planenum & ~0x00000001))
						continue;

					/* get autosprite and polygonoffset status */
					if (side1->shaderInfo &&
						(side1->shaderInfo->autosprite || side1->shaderInfo->polygonOffset))
						continue;
					if (side2->shaderInfo &&
						(side2->shaderInfo->autosprite || side2->shaderInfo->polygonOffset))
						continue;

					/* find first common point */
					first = -1;
					for (k = 0; k < numPoints; k++)
					{
						if (VectorCompare(w1->p[0], w2->p[k]))
						{
							first = k;
							k = numPoints;
						}
					}
					if (first == -1)
						continue;

					/* find second common point (regardless of winding order) */
					second = -1;
					dir = 0;
					if ((first + 1) < numPoints)
						second = first + 1;
					else
						second = 0;
					if (VectorCompareExt(w1->p[1], w2->p[second], CULL_EPSILON))
						dir = 1;
					else
					{
						if (first > 0)
							second = first - 1;
						else
							second = numPoints - 1;
						if (VectorCompareExt(w1->p[1], w2->p[second], CULL_EPSILON))
							dir = -1;
					}
					if (dir == 0)
						continue;

					/* compare the rest of the points */
					l = first;
					for (k = 0; k < numPoints; k++)
					{
						if (!VectorCompareExt(w1->p[k], w2->p[l], CULL_EPSILON))
							k = 100000;

						l += dir;
						if (l < 0)
							l = numPoints - 1;
						else if (l >= numPoints)
							l = 0;
					}
					if (k >= 100000)
						continue;

					/* cull face 1 */
					if (!side2->culled && !(side2->compileFlags & C_TRANSLUCENT) && !(side2->compileFlags & C_NODRAW))
					{
						side1->culled = qtrue;
						removed_numCoinFaces++;
					}

					if (side1->planenum == side2->planenum && side1->culled == qtrue)
						continue;

					/* cull face 2 */
					if (!side1->culled && !(side1->compileFlags & C_TRANSLUCENT) && !(side1->compileFlags & C_NODRAW))
					{
						side2->culled = qtrue;
						removed_numCoinFaces++;
					}
				}
			}
		}

		//Sys_Printf(" %9d entityNum\n", entityNum);
		/*if (removed_numHiddenFaces > 0 || removed_numCoinFaces > 0)
		{
			Sys_Printf("%9d hidden faces culled\n", removed_numHiddenFaces);
			Sys_Printf("%9d coincident faces culled\n", removed_numCoinFaces);
		}*/
	}
}

extern brush_t *InsertModelCopyBrush(brush_t *brush);
extern float DistanceHorizontal(const vec3_t p1, const vec3_t p2);
extern qboolean StringContainsWord(const char *haystack, const char *needle);
extern void vectoangles(const vec3_t value1, vec3_t angles);

#define CULL_BY_LOWEST_NEAR_POINT

#ifdef CULL_BY_LOWEST_NEAR_POINT
//#define CULL_BY_LOWEST_NEAR_POINT_DISTANCE_MAX 4096.0

float LowestMapPointNear(vec3_t pos, float radius, const char *modelName)
{// So we can cull surfaces below this height...
	// This is about as good as I think I can do to minimize crap from the additions...
	float nMins = pos[2];
	shaderInfo_t *lsi = NULL;

#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (!ds)
			continue;

		shaderInfo_t *si = ds->shaderInfo;

		if (!si)
			continue;

		if (!(si->compileFlags & C_SKY)
			&& !(si->compileFlags & C_SKIP)
			&& !(si->compileFlags & C_HINT)
			&& !(si->compileFlags & C_NODRAW)
			&& !StringContainsWord(si->shader, "sky")
			&& !StringContainsWord(si->shader, "caulk")
			&& !StringContainsWord(si->shader, "system/skip")
			&& !StringContainsWord(si->shader, "common/water"))
		{
			//if (DistanceHorizontal(pos, ds->mins) <= radius || DistanceHorizontal(pos, ds->maxs) <= radius)
			{
				//if (ds->mins[2] < nMins)
				{
					for (int i = 0; i < ds->numVerts; i++)
					{
						bspDrawVert_t *dv = &ds->verts[i];
						vec3_t angles;

						vectoangles(dv->normal, angles);

						float pitch = angles[0];

						if (pitch > 180)
							pitch -= 360;

						if (pitch < -180)
							pitch += 360;

						pitch += 90.0f;

						if (pitch == 180.0 || pitch == -180.0)
						{// Horrible hack to skip the surfaces created under the map by q3map2 code... Why are boxes needed for triangles?
							continue;
						}

						if (DistanceHorizontal(pos, dv->xyz) <= radius)
						{
							if (dv->xyz[2] < nMins)
							{
#pragma omp critical
								{
									//Sys_Printf("origin %.4f %.4f %.4f. inRangePos %.4f %.4f %.4f.\n", pos[0], pos[1], pos[2], dv->xyz[0], dv->xyz[1], dv->xyz[2]);
									nMins = dv->xyz[2];
									lsi = si;
								}
							}
						}
					}
				}
			}
		}
	}

	if (nMins == pos[2])
	{// Failed to find anything... Try without the angle check..
#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			/* get drawsurf */
			mapDrawSurface_t *ds = &mapDrawSurfs[s];

			if (!ds)
				continue;

			shaderInfo_t *si = ds->shaderInfo;

			if (!si)
				continue;

			if (!(si->compileFlags & C_SKY)
				&& !(si->compileFlags & C_SKIP)
				&& !(si->compileFlags & C_HINT)
				&& !(si->compileFlags & C_NODRAW)
				&& !StringContainsWord(si->shader, "sky")
				&& !StringContainsWord(si->shader, "caulk")
				&& !StringContainsWord(si->shader, "system/skip")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (DistanceHorizontal(pos, ds->mins) <= radius || DistanceHorizontal(pos, ds->maxs) <= radius)
				{
					//if (ds->mins[2] < nMins)
					{
						for (int i = 0; i < ds->numVerts; i++)
						{
							bspDrawVert_t *dv = &ds->verts[i];
							
							if (DistanceHorizontal(pos, dv->xyz) <= radius)
							{
								if (dv->xyz[2] < nMins)
								{
#pragma omp critical
									{
										//Sys_Printf("origin %.4f %.4f %.4f. inRangePos %.4f %.4f %.4f.\n", pos[0], pos[1], pos[2], dv->xyz[0], dv->xyz[1], dv->xyz[2]);
										nMins = dv->xyz[2];
										lsi = si;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (nMins == pos[2])
	{// Failed to find anything... Try double radius...
#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			/* get drawsurf */
			mapDrawSurface_t *ds = &mapDrawSurfs[s];

			if (!ds)
				continue;

			shaderInfo_t *si = ds->shaderInfo;

			if (!si)
				continue;

			if (!(si->compileFlags & C_SKY)
				&& !(si->compileFlags & C_SKIP)
				&& !(si->compileFlags & C_HINT)
				&& !(si->compileFlags & C_NODRAW)
				&& !StringContainsWord(si->shader, "sky")
				&& !StringContainsWord(si->shader, "caulk")
				&& !StringContainsWord(si->shader, "system/skip")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (DistanceHorizontal(pos, ds->mins) <= radius || DistanceHorizontal(pos, ds->maxs) <= radius)
				{
					//if (ds->mins[2] < nMins)
					{
						for (int i = 0; i < ds->numVerts; i++)
						{
							bspDrawVert_t *dv = &ds->verts[i];

							if (DistanceHorizontal(pos, dv->xyz) <= radius * 2.0)
							{
								if (dv->xyz[2] < nMins)
								{
#pragma omp critical
									{
										//Sys_Printf("origin %.4f %.4f %.4f. inRangePos %.4f %.4f %.4f.\n", pos[0], pos[1], pos[2], dv->xyz[0], dv->xyz[1], dv->xyz[2]);
										nMins = dv->xyz[2];
										lsi = si;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (nMins == pos[2])
	{// Failed to find anything... Try tripple radius...
#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			/* get drawsurf */
			mapDrawSurface_t *ds = &mapDrawSurfs[s];

			if (!ds)
				continue;

			shaderInfo_t *si = ds->shaderInfo;

			if (!si)
				continue;

			if (!(si->compileFlags & C_SKY)
				&& !(si->compileFlags & C_SKIP)
				&& !(si->compileFlags & C_HINT)
				&& !(si->compileFlags & C_NODRAW)
				&& !StringContainsWord(si->shader, "sky")
				&& !StringContainsWord(si->shader, "caulk")
				&& !StringContainsWord(si->shader, "system/skip")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (DistanceHorizontal(pos, ds->mins) <= radius || DistanceHorizontal(pos, ds->maxs) <= radius)
				{
					//if (ds->mins[2] < nMins)
					{
						for (int i = 0; i < ds->numVerts; i++)
						{
							bspDrawVert_t *dv = &ds->verts[i];

							if (DistanceHorizontal(pos, dv->xyz) <= radius * 3.0)
							{
								if (dv->xyz[2] < nMins)
								{
#pragma omp critical
									{
										//Sys_Printf("origin %.4f %.4f %.4f. inRangePos %.4f %.4f %.4f.\n", pos[0], pos[1], pos[2], dv->xyz[0], dv->xyz[1], dv->xyz[2]);
										nMins = dv->xyz[2];
										lsi = si;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/*if (nMins >= pos[2] - 128.0)
	{
		nMins = pos[2] - 128.0;
	}*/

	nMins -= 128.0;

	//Sys_Printf("model %s. origin %.4f %.4f %.4f.\n", modelName, pos[0], pos[1], pos[2]);
	//Sys_Printf("LOWEST_NEAR_POINT %.4f. Lowest found surface shader was %s.\n", nMins, (lsi->shader && lsi->shader[0]) ? lsi->shader : "Unknown");

	return nMins;
}
#endif //CULL_BY_LOWEST_NEAR_POINT

brush_t	*ModelBrushFromBounds(float minx, float miny, float minz, float maxx, float maxy, float maxz, shaderInfo_t *si)
{
	brush_t	*b;
	vec3_t mins, maxs;
	vec3_t normal;
	vec_t dist;
	int	i;

	b = AllocBrush(6);
	b->entityNum = mapEntityNum;
	b->original = b;
	b->contentShader = si;
	b->compileFlags = si->compileFlags;
	b->contentFlags = si->contentFlags;
	b->opaque = qtrue;
	b->detail = qfalse;
	b->numsides = 6;
	VectorSet(mins, minx, miny, minz);
	VectorSet(maxs, maxx, maxy, maxz);
	for (i = 0; i<3; i++)
	{
		VectorClear(normal);
		normal[i] = 1;
		dist = maxs[i];
		b->sides[i].planenum = FindFloatPlane(normal, dist, 1, (vec3_t*)&maxs);
		b->sides[i].shaderInfo = si;
		b->sides[i].surfaceFlags = si->surfaceFlags;
		b->sides[i].contentFlags = si->contentFlags;
		b->sides[i].compileFlags = si->compileFlags;
		b->sides[i].value = si->value;

		normal[i] = -1;
		dist = -mins[i];
		b->sides[3 + i].planenum = FindFloatPlane(normal, dist, 1, (vec3_t*)&mins);
		b->sides[3 + i].shaderInfo = si;
		b->sides[3 + i].surfaceFlags = si->surfaceFlags;
		b->sides[3 + i].contentFlags = si->contentFlags;
		b->sides[3 + i].compileFlags = si->compileFlags;
		b->sides[3 + i].value = si->value;
	}

	return b;
}

void RemoveSurface(mapDrawSurface_t *ds)
{
	if (!ds) return;

	if ((ds->shaderInfo->compileFlags & C_SKIP) || (ds->shaderInfo->compileFlags & C_NODRAW) || (ds->shaderInfo->compileFlags & C_HINT) || StringContainsWord(ds->shaderInfo->shader, "caulk") || StringContainsWord(ds->shaderInfo->shader, "skip"))
	{
		ClearSurface(ds);
	}
}

extern qboolean USE_SECONDARY_BSP;
extern qboolean GENERATING_SECONDARY_BSP;

void InsertModel(char *name, int frame, int skin, m4x4_t transform, float uvScale, remap_t *remap, shaderInfo_t *celShader
	, shaderInfo_t *overrideShader, qboolean forcedSolid, qboolean forcedFullSolid, qboolean forcedNoSolid
	, int entityNum, int mapEntityNum, char castShadows, char recvShadows, int spawnFlags, float lightmapScale, vec3_t lightmapAxis
	, vec3_t minlight, vec3_t minvertexlight, vec3_t ambient, vec3_t colormod, float lightmapSampleSize, int shadeAngle
	, int vertTexProj, qboolean noAlphaFix, float pushVertexes, qboolean skybox, int *added_surfaces, int *added_verts
	, int *added_triangles, int *added_brushes, qboolean cullSmallSolids, float LOWEST_NEAR_POINT, qboolean isLodModel, int centerChunkPass)
{
	int					s, numSurfaces;
	m4x4_t				identity, nTransform;
	picoModel_t			*model = NULL;
	float				top = -999999, bottom = 999999;
	bool				ALLOW_CULL_HALF_SIZE = false;
	bool				HAS_COLLISION_INFO = false;
	bool				MODEL_IS_PROCEDURAL = false;

	if (StringContainsWord(name, "forestpine")
		|| StringContainsWord(name, "junglepalm")
		|| StringContainsWord(name, "cypress")
		|| StringContainsWord(name, "cedar")
		|| StringContainsWord(name, "uqoak"))
	{// Special case for high trees, we can cull much more for FPS yay! :)
		ALLOW_CULL_HALF_SIZE = true;
	}

	if (!strncmp(name, "procedural", 10)) {
		MODEL_IS_PROCEDURAL = true;
	}

	/* get model */
	if (MODEL_IS_PROCEDURAL) {
#define			MAX_FOREST_MODELS				64
		extern int				irand(int min, int max);
		extern int				PROCEDURAL_TREE_COUNT;
		extern char				PROCEDURAL_TREE_BARKS[MAX_FOREST_MODELS][128];
		extern char				PROCEDURAL_TREE_LEAFS[MAX_FOREST_MODELS][128];
		extern int				treePropertiesSegments[MAX_FOREST_MODELS];
		extern int				treePropertiesLevels[MAX_FOREST_MODELS];
		extern float			treePropertiesVMultiplier[MAX_FOREST_MODELS];
		extern int				treePropertiesTwigScale[MAX_FOREST_MODELS];
		extern float			treePropertiesInitialBranchLength[MAX_FOREST_MODELS];
		extern float			treePropertiesLengthFalloffFactor[MAX_FOREST_MODELS];
		extern float			treePropertiesLengthFalloffPower[MAX_FOREST_MODELS];
		extern float			treePropertiesClumpMax[MAX_FOREST_MODELS];
		extern float			treePropertiesClumpMin[MAX_FOREST_MODELS];
		extern float			treePropertiesBranchFactor[MAX_FOREST_MODELS];
		extern float			treePropertiesDropAmount[MAX_FOREST_MODELS];
		extern float			treePropertiesGrowAmount[MAX_FOREST_MODELS];
		extern float			treePropertiesSweepAmount[MAX_FOREST_MODELS];
		extern float			treePropertiesMaxRadius[MAX_FOREST_MODELS];
		extern float			treePropertiesClimbRate[MAX_FOREST_MODELS];
		extern float			treePropertiesTrunkKink[MAX_FOREST_MODELS];
		extern int				treePropertiesTreeSteps[MAX_FOREST_MODELS];
		extern float			treePropertiesTaperRate[MAX_FOREST_MODELS];
		extern float			treePropertiesRadiusFalloffRate[MAX_FOREST_MODELS];
		extern float			treePropertiesTwistRate[MAX_FOREST_MODELS];
		extern float			treePropertiesTrunkLength[MAX_FOREST_MODELS];

		Proctree::Properties treeProperties;
		PicoGenerateTreeModelDefaults(&treeProperties);
		treeProperties.mSeed = irand(0, 65536);

#pragma omp critical (__GENERATE_PROCEDURAL__)
		{
			if (PROCEDURAL_TREE_COUNT <= 0)
			{
				model = PicoGenerateTreeModel((char *)name, "models/warzone/trees/treepineforestbarkcomp2.jpg", "models/warzone/trees/mp_valenwood_tree01.png", &treeProperties);
			}
			else
			{// A list was specified...
				int choice = irand(0, PROCEDURAL_TREE_COUNT-1);

				treeProperties.mClumpMax = treePropertiesClumpMax[choice];
				treeProperties.mClumpMin = treePropertiesClumpMin[choice];
				treeProperties.mLengthFalloffFactor = treePropertiesLengthFalloffFactor[choice];
				treeProperties.mLengthFalloffPower = treePropertiesLengthFalloffPower[choice];
				treeProperties.mBranchFactor = treePropertiesBranchFactor[choice];
				treeProperties.mRadiusFalloffRate = treePropertiesRadiusFalloffRate[choice];
				treeProperties.mClimbRate = treePropertiesClimbRate[choice];
				treeProperties.mTrunkKink = treePropertiesTrunkKink[choice];
				treeProperties.mMaxRadius = treePropertiesMaxRadius[choice];
				treeProperties.mTreeSteps = treePropertiesTreeSteps[choice];
				treeProperties.mTaperRate = treePropertiesTaperRate[choice];
				treeProperties.mTwistRate = treePropertiesTwistRate[choice];
				treeProperties.mSegments = treePropertiesSegments[choice];
				treeProperties.mLevels = treePropertiesLevels[choice];
				treeProperties.mSweepAmount = treePropertiesSweepAmount[choice];
				treeProperties.mInitialBranchLength = treePropertiesInitialBranchLength[choice];
				treeProperties.mTrunkLength = treePropertiesTrunkLength[choice];
				treeProperties.mDropAmount = treePropertiesDropAmount[choice];
				treeProperties.mGrowAmount = treePropertiesGrowAmount[choice];
				treeProperties.mVMultiplier = treePropertiesVMultiplier[choice];
				treeProperties.mTwigScale = treePropertiesTwigScale[choice];

				model = PicoGenerateTreeModel((char *)name, PROCEDURAL_TREE_BARKS[choice], PROCEDURAL_TREE_LEAFS[choice], &treeProperties);
			}
		}

		if (model == NULL) {
			Sys_Warning("Failed in creation of a procedural tree. It returned NULL.\n");
			return;
		}
		model->hasCollisionGeometry = qtrue;

#if 0
		/* each surface on the model will become a new map drawsurface */
		numSurfaces = PicoGetModelNumSurfaces(model);

		for (s = 0; s < numSurfaces; s++)
		{
			int					i;
			picoVec_t			*xyz;
			picoSurface_t		*surface;

			/* get surface */
			surface = PicoGetModelSurface(model, s);

			if (surface == NULL)
				continue;

			/* only handle triangle surfaces initially (fixme: support patches) */
			if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
				continue;

			char *picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);

			printf("surface %i - shader %s.\n", s, picoShaderName);
		}
#endif
	}
	else
	{
		model = LoadModel(name, frame);
	}

	if (model == NULL)
		return;

	//printf("DEBUG: Inserting model %s.\n", name);

	qboolean haveLodModel = qfalse;

	HAS_COLLISION_INFO = model->hasCollisionGeometry ? true : false;

	/* handle null matrix */
	if (transform == NULL)
	{
		m4x4_identity(identity);
		transform = identity;
	}

	//Sys_Printf("Model %s. Matrix: %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", name, identity[0], identity[1], identity[2], identity[3], identity[4], identity[5], identity[6], identity[7], identity[8], identity[9], identity[10], identity[11], identity[12], identity[13], identity[14], identity[15]);

	/* hack: Stable-1_2 and trunk have differing row/column major matrix order
	   this transpose is necessary with Stable-1_2
	   uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
	//%	m4x4_transpose( transform );

	/* create transform matrix for normals */
	memcpy(nTransform, transform, sizeof(m4x4_t));
	if (m4x4_invert(nTransform))
		Sys_Warning(mapEntityNum, "Can't invert model transform matrix, using transpose instead");
	//m4x4_transpose(nTransform);

	/* each surface on the model will become a new map drawsurface */
	numSurfaces = PicoGetModelNumSurfaces(model);

	for (s = 0; s < numSurfaces; s++)
	{
		int					i;
		picoVec_t			*xyz;
		picoSurface_t		*surface;

		/* get surface */
		surface = PicoGetModelSurface(model, s);

		if (surface == NULL)
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
			continue;

		/* copy vertexes */
		for (i = 0; i < PicoGetSurfaceNumVertexes(surface); i++)
		{
			vec3_t xyz2;
			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ(surface, i);
			VectorCopy(xyz, xyz2);
			m4x4_transform_point(transform, xyz2);

			if (top < xyz2[2]) top = xyz2[2];
			if (bottom > xyz2[2]) bottom = xyz2[2];
		}

		if (!forcedNoSolid)
		{
			if (overrideShader)
			{
				HAS_COLLISION_INFO = true;
				break;
			}
			else
			{
				char *picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);

				//Sys_Printf("surface name %s.\n", picoShaderName);
				if (StringContainsWord(picoShaderName, "system/nodraw_solid") || StringContainsWord(picoShaderName, "collision"))
				{
					//Sys_Printf("Found collision info in model!\n");
					HAS_COLLISION_INFO = true;
					break;
				}
			}
		}
	}

	if (USE_LODMODEL && isLodModel && !HAS_COLLISION_INFO && !(forcedSolid || forcedFullSolid))
	{
		return;
	}

	//Sys_Printf("top: %.4f. bottom: %.4f.\n", top, bottom);

	//Sys_Printf( "Model %s has %d surfaces\n", name, numSurfaces );

//#pragma omp parallel for ordered num_threads((numSurfaces < numthreads) ? numSurfaces : numthreads)
//#pragma omp parallel for ordered num_threads(numthreads)
	for (s = 0; s < numSurfaces; s++)
	{
		int					i;
		char				*picoShaderName;
		char				shaderName[MAX_QPATH];
		remap_t				*rm, *glob;
		shaderInfo_t		*si;
		mapDrawSurface_t	*ds;
		picoSurface_t		*surface;
		picoIndex_t			*indexes;
		bool				isTriangles = true;

#pragma omp critical
		{
			/* get surface */
			surface = PicoGetModelSurface(model, s);
		}

		if (surface == NULL)
			continue;

#pragma omp critical
		{
			/* only handle triangle surfaces initially (fixme: support patches) */
			if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
				isTriangles = false;
		}

		if (!isTriangles)
			continue;

		if (!(overrideShader && !surface->shader))
		{
			/* get shader name */
			/* vortex: support .skin files */
#pragma omp critical
			{
				picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);
			}
		}
		else
		{
			picoShaderName = overrideShader->shader;
		}

		/* handle shader remapping */
		glob = NULL;
		for (rm = remap; rm != NULL; rm = rm->next)
		{
			if (rm->from[0] == '*' && rm->from[1] == '\0')
				glob = rm;
			else if (!Q_stricmp(picoShaderName, rm->from))
			{
				Sys_FPrintf(SYS_VRB, "Remapping %s to %s\n", picoShaderName, rm->to);
				picoShaderName = rm->to;
				glob = NULL;
				break;
			}
		}

		if (glob != NULL)
		{
			Sys_FPrintf(SYS_VRB, "Globbing %s to %s\n", picoShaderName, glob->to);
			picoShaderName = glob->to;
		}

		bool IS_COLLISION_SURFACE = false;

		if (HAS_COLLISION_INFO && (StringContainsWord(picoShaderName, "system/nodraw_solid") || StringContainsWord(picoShaderName, "collision")))
		{// We have actual solid information for this model, don't generate planes for the other crap...
			IS_COLLISION_SURFACE = true;
		}
		else if (forcedSolid || forcedFullSolid)
		{
			IS_COLLISION_SURFACE = true;
		}

		if (USE_LODMODEL && isLodModel /*&& HAS_COLLISION_INFO*/ && !IS_COLLISION_SURFACE)
		{
			continue;
		}

#pragma omp critical
		{
			if (!forcedNoSolid && HAS_COLLISION_INFO && (StringContainsWord(picoShaderName, "system/nodraw_solid") || StringContainsWord(picoShaderName, "collision")))
			{
				si = ShaderInfoForShader(picoShaderName);
			}
			else
			{
				if (overrideShader)
				{
					si = overrideShader;
				}

				/* shader renaming for sof2 */
				else if (renameModelShaders)
				{
					strcpy(shaderName, picoShaderName);
					StripExtension(shaderName);
					if (spawnFlags & 1)
						strcat(shaderName, "_RMG_BSP");
					else
						strcat(shaderName, "_BSP");
					si = ShaderInfoForShader(shaderName);
				}
				else
					si = ShaderInfoForShader(picoShaderName);
			}

			LoadShaderImages(si);
		}

		if (USE_SECONDARY_BSP)
		{
			if (!GENERATING_SECONDARY_BSP)
			{// When using secondary bsp, but not generating it, skip anything not solid in the main bsp...
				if (!((si->contentFlags & C_SOLID) || (si->contentFlags & C_STRUCTURAL) || IS_COLLISION_SURFACE))
				{
					continue;
				}
			}

			if (GENERATING_SECONDARY_BSP)
			{// When using secondary bsp, and generating it, skip anything solid in the main bsp...
				if ((si->contentFlags & C_SOLID) || (si->contentFlags & C_STRUCTURAL) || IS_COLLISION_SURFACE)
				{
					continue;
				}
			}
		}

		/* warn for missing shader */
		if (si->warnNoShaderImage == qtrue)
		{
			if (mapEntityNum >= 0)
			{
				//Sys_Warning(mapEntityNum, "Failed to load shader image '%s'", si->shader);
				/* external entity, just show single warning */
				Sys_Warning("Failed to load shader image '%s'", si->shader);
				si->warnNoShaderImage = qfalse;
			}
			else
			{
				/* external entity, just show single warning */
				Sys_Warning("Failed to load shader image '%s' for model '%s'", si->shader, PicoGetModelFileName(model));
				si->warnNoShaderImage = qfalse;
			}
		}

		if (FORCED_MODEL_META)
		{
			si->forceMeta = qtrue;
		}

		if (!(spawnFlags & 64) && (shadeAngle <= 0.0f || ds->type != SURFACE_FORCED_META))
		{/* fix the surface's normals (jal: conditioned by shader info) */
			PicoFixSurfaceNormals(surface);
		}

		//
		//
		//

		surfaceType_t	wantedType = SURFACE_TRIANGLES;
		float			wantedSampleSize = 0.0;
		float			wantedLightmapScale = 0.0;
		int				wantedVertTexProj = 0;
		vec3_t			wantedLightMapAxis; VectorClear(wantedLightMapAxis);
		vec3_t			wantedMinLight; VectorClear(wantedMinLight);
		vec3_t			wantedMinvertexLight; VectorClear(wantedMinvertexLight);
		vec3_t			wantedAmbient; VectorClear(wantedAmbient);
		vec3_t			wantedColorMod; VectorClear(wantedColorMod);

		if ((si != NULL && si->forceMeta) || (spawnFlags & 4))	/* 3rd bit */
			wantedType = SURFACE_FORCED_META;

		if (lightmapSampleSize > 0.0f)
			wantedSampleSize = lightmapSampleSize;

		if (lightmapScale > 0.0f)
			wantedLightmapScale = lightmapScale;

		if (lightmapAxis != NULL)
			VectorCopy(lightmapAxis, wantedLightMapAxis);

		if (minlight != NULL)
			VectorCopy(minlight, wantedMinLight);

		if (minvertexlight != NULL)
			VectorCopy(minvertexlight, wantedMinvertexLight);

		if (ambient != NULL)
			VectorCopy(ambient, wantedAmbient);

		if (colormod != NULL)
			VectorCopy(colormod, wantedColorMod);

		/* set vertical texture projection */
		if (vertTexProj > 0)
			wantedVertTexProj = vertTexProj;

		mapDrawSurface_t *dsFound = NULL;

		//if (!si->glow)
		{// Don't merge glow objects, warzone needs them separate for emissive lighting... UQ1: Can do this now. wz code now looks at verts...
			for (int d = 0; d < numMapDrawSurfs; d++)
			{
				mapDrawSurface_t *ds2 = &mapDrawSurfs[d];

#if 0
				if (IS_COLLISION_SURFACE)
				{// Lighting crap is irrelevant... Distance is irrelevant, this surf wont be drawn...
					if (ds2->mapEntityNum == mapEntityNum && ds2->shaderInfo == si)
					{
						dsFound = ds2;
						break;
					}
					/*else if (forcedSolid)
					{
						dsFound = ds2;
						break;
					}
					else if (StringContainsWord(picoShaderName, "system/nodraw_solid") || StringContainsWord(picoShaderName, "collision"))
					{
						dsFound = ds2;
						break;
					}*/
				}
#endif
				
				if (ds2->mapEntityNum == mapEntityNum
					&& ds2->castShadows == castShadows
					&& ds2->recvShadows == recvShadows
					&& ds2->noAlphaFix == noAlphaFix
					&& ds2->skybox == skybox
					&& ds2->shaderInfo == si
					&& ds2->smoothNormals == shadeAngle
					&& ds2->type == wantedType
					&& ds2->sampleSize == wantedSampleSize
					&& ds2->lightmapScale == wantedLightmapScale
					&& ds2->vertTexProj == wantedVertTexProj
					&& ds2->celShader == celShader
					&& (lightmapAxis == NULL || VectorCompare(wantedLightMapAxis, ds2->lightmapAxis))
					&& (minlight == NULL || VectorCompare(wantedMinLight, ds2->minlight))
					&& (minvertexlight == NULL || VectorCompare(wantedMinvertexLight, ds2->minvertexlight))
					&& (ambient == NULL || VectorCompare(wantedAmbient, ds2->ambient))
					&& (colormod == NULL || VectorCompare(wantedColorMod, ds2->colormod)))
				{
					vec3_t xyz;
					VectorCopy(PicoGetSurfaceXYZ(surface, 0), xyz);
					m4x4_transform_point(transform, xyz);

					float dist = Distance(ds2->verts[0].xyz, xyz);
					if (dist < 2048.0)
					{
						dsFound = ds2;
						break;
					}
				}
			}
		}

		if (dsFound != NULL)
		{// Add to the old surface...
			//
			// Add to this new drawSurf...
			//

			//printf("Found draw surf to reuse...\n");

			ds = dsFound;

			ds->currentNumVerts = ds->numVerts;
			ds->currentNumIndexes = ds->numIndexes;

			/* set particulars */
			int nV = PicoGetSurfaceNumVertexes(surface);
			ds->numVerts += nV;
			
#pragma omp critical (__ALLOC__)
			{
				ds->verts = (bspDrawVert_t *)realloc(ds->verts, ds->numVerts * sizeof(ds->verts[0]));
			}

			int nI = PicoGetSurfaceNumIndexes(surface);
			ds->numIndexes += nI;

#pragma omp critical (__ALLOC__)
			{
				ds->indexes = (int *)realloc(ds->indexes, ds->numIndexes * sizeof(ds->indexes[0]));
			}

			if (added_verts != NULL)
				*added_verts += nI;

			if (added_triangles != NULL)
				*added_triangles += (nV / 3);
		}
		else
		{// Create a new surface...
			//printf("Creating new draw surf...\n");

			//ForceCrash();

#pragma omp critical
			{
				/* allocate a surface (ydnar: gs mods) */
				ds = AllocDrawSurface(SURFACE_TRIANGLES);
			}

			ds->entityNum = entityNum;
			ds->mapEntityNum = mapEntityNum;
			ds->castShadows = castShadows;
			ds->recvShadows = recvShadows;
			ds->noAlphaFix = noAlphaFix;
			ds->skybox = skybox;

			if (added_surfaces != NULL)
				*added_surfaces += 1;

			/* set shader */
			ds->shaderInfo = si;

			/* set shading angle */
			ds->smoothNormals = shadeAngle;

			/* force to meta? */
			ds->type = wantedType;

			/* set sample size */
			ds->sampleSize = wantedSampleSize;

			/* set lightmap scale */
			ds->lightmapScale = wantedLightmapScale;

			/* set lightmap axis */
			VectorCopy(lightmapAxis, wantedLightMapAxis);

			/* set minlight/ambient/colormod */
			if (minlight != NULL)
			{
				VectorCopy(wantedMinLight, ds->minlight);
				VectorCopy(wantedMinLight, ds->minvertexlight);
			}

			if (minvertexlight != NULL)
				VectorCopy(wantedMinvertexLight, ds->minvertexlight);

			if (ambient != NULL)
				VectorCopy(wantedAmbient, ds->ambient);

			if (colormod != NULL)
				VectorCopy(wantedColorMod, ds->colormod);

			/* set vertical texture projection */
			ds->vertTexProj = wantedVertTexProj;

			//
			// Set up this new drawSurf...
			//

			/* set particulars */
			ds->numVerts = PicoGetSurfaceNumVertexes(surface);
			ds->currentNumVerts = 0;
			ds->currentNumIndexes = 0;

#pragma omp critical
			{
				ds->verts = (bspDrawVert_t *)safe_malloc(ds->numVerts * sizeof(ds->verts[0]));
				memset(ds->verts, 0, ds->numVerts * sizeof(ds->verts[0]));
			}

			ds->numIndexes = PicoGetSurfaceNumIndexes(surface);
#pragma omp critical
			{
				ds->indexes = (int *)safe_malloc(ds->numIndexes * sizeof(ds->indexes[0]));
				memset(ds->indexes, 0, ds->numIndexes * sizeof(ds->indexes[0]));
			}

			if (added_verts != NULL)
				*added_verts += ds->numVerts;

			if (added_triangles != NULL)
				*added_triangles += (ds->numIndexes / 3);

			/* set cel shader */
			ds->celShader = celShader;
		}

		int totalVertexesToAdd = ds->numVerts - ds->currentNumVerts;
		int totalVertexesAdded = 0;

		/* copy vertexes */
#pragma omp parallel for ordered num_threads((ds->numVerts < numthreads) ? ds->numVerts : numthreads)
		for (i = ds->currentNumVerts; i < ds->numVerts; i++)
		{
			int					j;
			bspDrawVert_t		*dv;
			vec3_t				forceVecs[2];
			picoVec_t			*xyz, *normal, *st;
			picoByte_t			*color;

			if (centerChunkPass == 1)
			{
#pragma omp critical (__PROGRESS_BAR__)
				{
					printLabelledProgress("CenterChunkTriangles", totalVertexesAdded, totalVertexesToAdd);
				}
			}

			totalVertexesAdded++;

			/* get vertex */
			dv = &ds->verts[i];

			/* vortex: create forced tcGen vecs for vertical texture projection */
			if (ds->vertTexProj > 0)
			{
				forceVecs[0][0] = 1.0 / ds->vertTexProj;
				forceVecs[0][1] = 0.0;
				forceVecs[0][2] = 0.0;
				forceVecs[1][0] = 0.0;
				forceVecs[1][1] = 1.0 / ds->vertTexProj;
				forceVecs[1][2] = 0.0;
			}

			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ(surface, i - ds->currentNumVerts);
			VectorCopy(xyz, dv->xyz);
			m4x4_transform_point(transform, dv->xyz);

			normal = PicoGetSurfaceNormal(surface, i - ds->currentNumVerts);
			VectorCopy(normal, dv->normal);
			m4x4_transform_normal(nTransform, dv->normal);
			VectorNormalize(dv->normal, dv->normal);

			/* ydnar: tek-fu celshading support for flat shaded shit */
			if (flat)
			{
				dv->st[0] = si->stFlat[0];
				dv->st[1] = si->stFlat[1];
			}

			/* vortex: entity-set _vp/_vtcproj will force drawsurface to "tcGen ivector ( value 0 0 ) ( 0 value 0 )"  */
			else if (ds->vertTexProj > 0)
			{
				/* project the texture */
				dv->st[0] = DotProduct(forceVecs[0], dv->xyz);
				dv->st[1] = DotProduct(forceVecs[1], dv->xyz);
			}

			/* ydnar: gs mods: added support for explicit shader texcoord generation */
			else if (si->tcGen)
			{
				/* project the texture */
				dv->st[0] = DotProduct(si->vecs[0], dv->xyz);
				dv->st[1] = DotProduct(si->vecs[1], dv->xyz);
			}

			/* normal texture coordinates */
			else
			{
				st = PicoGetSurfaceST(surface, 0, i - ds->currentNumVerts);
				dv->st[0] = st[0];
				dv->st[1] = st[1];
			}

			/* scale UV by external key */
			dv->st[0] *= uvScale;
			dv->st[1] *= uvScale;

			/* set lightmap/color bits */
			color = PicoGetSurfaceColor(surface, 0, i - ds->currentNumVerts);

			for (j = 0; j < MAX_LIGHTMAPS; j++)
			{
				dv->lightmap[j][0] = 0.0f;
				dv->lightmap[j][1] = 0.0f;
				if (spawnFlags & 32) // spawnflag 32: model color -> alpha hack
				{
					dv->color[j][0] = 255.0f;
					dv->color[j][1] = 255.0f;
					dv->color[j][2] = 255.0f;
					dv->color[j][3] = color[0] * 0.3f + color[1] * 0.59f + color[2] * 0.11f;
				}
				else
				{
					dv->color[j][0] = color[0];
					dv->color[j][1] = color[1];
					dv->color[j][2] = color[2];
					dv->color[j][3] = color[3];
				}
			}
		}

		if (centerChunkPass == 1)
		{
#pragma omp critical (__PROGRESS_BAR__)
			{
				printLabelledProgress("CenterChunkTriangles", totalVertexesToAdd, totalVertexesToAdd);
			}
		}

		/* copy indexes */
		indexes = PicoGetSurfaceIndexes(surface, 0);

#ifdef CULL_BY_LOWEST_NEAR_POINT
		int numIndexesCulled = 0;
		int numIndexesFinal = ds->currentNumIndexes;
		int numIndexesOriginal = ds->currentNumIndexes;

		if (LOWEST_NEAR_POINT > -9999999.9)
		{
			int j;

			for (j = ds->currentNumIndexes; j < ds->numIndexes; j += 3)
			{
				// Assume that we can cull them all... Add exceptions as found...
				qboolean shouldLowestPointCull = qtrue;

				numIndexesOriginal += 3;

				for (int k = 0; k < 3; k++)
				{
					bspDrawVert_t		*dv = &ds->verts[indexes[j + k - ds->currentNumIndexes]] + ds->currentNumVerts;

					if (LOWEST_NEAR_POINT == 999999.0f || dv->xyz[2] >= LOWEST_NEAR_POINT)
					{// If any surface is above the found lowest map point, then we need to draw this triangle...
						shouldLowestPointCull = qfalse;
						break;
					}
				}

				if (!shouldLowestPointCull)
				{// If any surface is above the found lowest map point, then we need to draw this triangle...
					ds->indexes[numIndexesFinal] = indexes[j - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
					ds->indexes[numIndexesFinal] = indexes[j + 1 - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
					ds->indexes[numIndexesFinal] = indexes[j + 2 - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
				}
				else
				{
					numIndexesCulled += 3;
				}
			}

			if (numIndexesCulled > 0)
			{
				ds->numIndexes = numIndexesFinal;
				numBoundsCulledSurfs += numIndexesCulled / 3;
				//Sys_Printf("Lowest point culled %i of %i indexes. %.2f percent were culled. %i final indexes.\n", numIndexesCulled, numIndexesOriginal, (float(numIndexesCulled) / float(numIndexesOriginal)) * 100.0, numIndexesFinal);
			}
		}
		else
		{
			int j;

			for (j = ds->currentNumIndexes; j < ds->numIndexes; j += 3)
			{
				numIndexesOriginal += 3;

				{// If any surface is above the found lowest map point, then we need to draw this triangle...
					ds->indexes[numIndexesFinal] = indexes[j - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
					ds->indexes[numIndexesFinal] = indexes[j + 1 - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
					ds->indexes[numIndexesFinal] = indexes[j + 2 - ds->currentNumIndexes] + ds->currentNumVerts;
					numIndexesFinal++;
				}
			}

			if (numIndexesCulled > 0)
			{
				ds->numIndexes = numIndexesFinal;
				numBoundsCulledSurfs += numIndexesCulled / 3;
				//Sys_Printf("Lowest point culled %i of %i indexes. %.2f percent were culled. %i final indexes.\n", numIndexesCulled, numIndexesOriginal, (float(numIndexesCulled) / float(numIndexesOriginal)) * 100.0, numIndexesFinal);
			}
		}

#else //!CULL_BY_LOWEST_NEAR_POINT
		for (i = ds->currentNumIndexes; i < ds->numIndexes; i++)
		{
			ds->indexes[i + ds->currentNumIndexes] = indexes[i];
		}
#endif //CULL_BY_LOWEST_NEAR_POINT

		/* deform vertexes */
		//DeformVertexes(ds, pushVertexes);
		if (pushVertexes)
		{
			/* iterate verts */
			for (int kk = ds->currentNumVerts; kk < ds->numVerts; kk++)
			{
				bspDrawVert_t *vv = &ds->verts[kk];

				/* push verts in direction of their normals (fatboy) */
				VectorMA(vv->xyz, pushVertexes, vv->normal, vv->xyz);
			}
		}

		/* walk triangle list */
		for (i = ds->currentNumIndexes; i < ds->numIndexes; i += 3)
		{
			vec3_t points[4];

			/* make points and back points */
			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t *dv = &ds->verts[ds->indexes[i + j]];

				/* copy xyz */
				VectorCopy(dv->xyz, points[j]);
			}
		}

		/* ydnar: giant hack land: generate clipping brushes for model triangles */
		if (ds->verts && !forcedNoSolid && (!haveLodModel && (si->clipModel || (spawnFlags & 2)) && !noclipmodel) || forcedSolid || IS_COLLISION_SURFACE)	/* 2nd bit */
		{
			if (forcedSolid)
			{
				//Sys_Printf("Adding forced collision planes for %s.\n", overrideShader ? overrideShader->shader : picoShaderName);
			}
			else if (HAS_COLLISION_INFO && !IS_COLLISION_SURFACE)
			{
				continue;
			}
			else if (HAS_COLLISION_INFO && IS_COLLISION_SURFACE)
			{
				//Sys_Printf("Adding collision planes for %s.\n", overrideShader ? overrideShader->shader : picoShaderName);
			}
			else
			{
				if (!forcedSolid && (si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
				{
					continue;
				}
			}

			/* temp hack */
			if (!si->clipModel
				&& !forcedSolid
				&& ((si->compileFlags & C_TRANSLUCENT) || !(si->compileFlags & C_SOLID)))
			{
				//Sys_Printf("DEBUG: Shader %s is translucent or not solid.\n", overrideShader ? overrideShader->shader : picoShaderName);
				continue;
			}

			/* overflow check */
			if ((nummapplanes + 64) >= (MAX_MAP_PLANES >> 1))
			{
				continue;
			}

			int totalIndexesToAdd = (ds->numIndexes - ds->currentNumIndexes) / 3;
			int totalIndexesAdded = 0;

			/* walk triangle list */
#pragma omp parallel for ordered num_threads((ds->numIndexes < numthreads) ? ds->numIndexes : numthreads)
			for( i = ds->currentNumIndexes; i < ds->numIndexes; i += 3 )
			{
				vec3_t				points[4], backs[3];
				vec4_t				plane, reverse, pa, pb, pc;

				if (centerChunkPass == 2)
				{
#pragma omp critical (__PROGRESS_BAR__)
					{
						printLabelledProgress("CenterChunkCollision", totalIndexesAdded, totalIndexesToAdd);
					}
				}

				totalIndexesAdded++;

				/* overflow hack */
				if( (nummapplanes + 64) >= (MAX_MAP_PLANES >> 1) )
				{
					Sys_Warning( mapEntityNum, "MAX_MAP_PLANES (%d) hit generating clip brushes for model %s.", MAX_MAP_PLANES, name );
					break;
				}

				/* make points and back points */
				for( int j = 0; j < 3; j++ )
				{
					bspDrawVert_t		*dv;

					//printf("index %i of %i.\n", i + j, ds->numIndexes);

					/* get vertex */
					dv = &ds->verts[ ds->indexes[ i + j ] ];

					/* copy xyz */
					VectorCopy( dv->xyz, points[ j ] );
					VectorCopy( dv->xyz, backs[ j ] );

					/* find nearest axial to normal and push back points opposite */
					/* note: this doesn't work as well as simply using the plane of the triangle, below */
					for( int k = 0; k < 3; k++ )
					{
						if( fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 1) % 3 ] ) &&
							fabs( dv->normal[ k ] ) >= fabs( dv->normal[ (k + 2) % 3 ] ) )
						{
							backs[ j ][ k ] += dv->normal[ k ] < 0.0f ? 64.0f : -64.0f;
							break;
						}
					}
				}

				VectorCopy( points[0], points[3] ); // for cyclic usage

				/* make plane for triangle */
				// div0: add some extra spawnflags:
				//   0: snap normals to axial planes for extrusion
				//   8: extrude with the original normals
				//  16: extrude only with up/down normals (ideal for terrain)
				//  24: extrude by distance zero (may need engine changes)
				if( PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] ) )
				{
					double				normalEpsilon_save;
					double				distanceEpsilon_save;

					vec3_t bestNormal;
					float backPlaneDistance = 2;

					if(spawnFlags & 8) // use a DOWN normal
					{
						if(spawnFlags & 16)
						{
							// 24: normal as is, and zero width (broken)
							VectorCopy(plane, bestNormal);
						}
						else
						{
							// 8: normal as is
							VectorCopy(plane, bestNormal);
						}
					}
					else
					{
						if(spawnFlags & 16)
						{
							// 16: UP/DOWN normal
							VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
						}
						else
						{
							// 0: axial normal
							if(fabs(plane[0]) > fabs(plane[1])) // x>y
							if(fabs(plane[1]) > fabs(plane[2])) // x>y, y>z
								VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
							else // x>y, z>=y
							if(fabs(plane[0]) > fabs(plane[2])) // x>z, z>=y
								VectorSet(bestNormal, (plane[0] >= 0 ? 1 : -1), 0, 0);
							else // z>=x, x>y
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
							else // y>=x
							if(fabs(plane[1]) > fabs(plane[2])) // y>z, y>=x
								VectorSet(bestNormal, 0, (plane[1] >= 0 ? 1 : -1), 0);
							else // z>=y, y>=x
								VectorSet(bestNormal, 0, 0, (plane[2] >= 0 ? 1 : -1));
						}
					}

					/* regenerate back points */
					for( int j = 0; j < 3; j++ )
					{
						bspDrawVert_t		*dv;

						/* get vertex */
						dv = &ds->verts[ ds->indexes[ i + j ] ];

						// shift by some units
						VectorMA(dv->xyz, -64.0f, bestNormal, backs[j]); // 64 prevents roundoff errors a bit
					}

					/* make back plane */
					VectorScale( plane, -1.0f, reverse );
					reverse[ 3 ] = -plane[ 3 ];
					if((spawnFlags & 24) != 24)
						reverse[3] += DotProduct(bestNormal, plane) * backPlaneDistance;
					// that's at least sqrt(1/3) backPlaneDistance, unless in DOWN mode; in DOWN mode, we are screwed anyway if we encounter a plane that's perpendicular to the xy plane)

					normalEpsilon_save = normalEpsilon;
					distanceEpsilon_save = distanceEpsilon;

					if( PlaneFromPoints( pa, points[ 2 ], points[ 1 ], backs[ 1 ] ) &&
						PlaneFromPoints( pb, points[ 1 ], points[ 0 ], backs[ 0 ] ) &&
						PlaneFromPoints( pc, points[ 0 ], points[ 2 ], backs[ 2 ] ) )
					{
						vec3_t mins, maxs;
						int z;

						VectorSet(mins, 999999, 999999, 999999);
						VectorSet(maxs, -999999, -999999, -999999);

						for (z = 0; z < 4; z++)
						{
							if (points[z][0] < mins[0]) mins[0] = points[z][0];
							if (points[z][1] < mins[1]) mins[1] = points[z][1];
							if (points[z][2] < mins[2]) mins[2] = points[z][2];

							if (points[z][0] > maxs[0]) maxs[0] = points[z][0];
							if (points[z][1] > maxs[1]) maxs[1] = points[z][1];
							if (points[z][2] > maxs[2]) maxs[2] = points[z][2];
						}

						numSolidSurfs++;

						//Sys_Printf("mapmins %.4f %.4f %.4f. mapmaxs %.4f %.4f %.4f.\n", mapPlayableMins[0], mapPlayableMins[1], mapPlayableMins[2], mapPlayableMaxs[0], mapPlayableMaxs[1], mapPlayableMaxs[2]);
						//Sys_Printf("mins %.4f %.4f %.4f. maxs %.4f %.4f %.4f.\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);

						bool playableBoundsInvalid = false;

						if (mapPlayableMins[0] == 99999 && mapPlayableMins[1] == 99999 && mapPlayableMins[2] == 99999 && mapPlayableMaxs[0] == -99999 && mapPlayableMaxs[1] == -99999 && mapPlayableMaxs[2] == -99999)
						{
							playableBoundsInvalid = true;
						}

						if (LOWEST_NEAR_POINT > -9999999.9)
						{
#ifdef CULL_BY_LOWEST_NEAR_POINT
							{
								//Sys_Printf("LOWEST_NEAR_POINT %.4f.\n", LOWEST_NEAR_POINT);

								if (LOWEST_NEAR_POINT != 999999.0f && mins[2] < LOWEST_NEAR_POINT && maxs[2] < LOWEST_NEAR_POINT)
								{// Below map's lowest known near point, definately cull...
									//Sys_Printf("Culled one!\n");
									//Sys_Printf("DEBUG: Lowest point cull %f < %f LOWEST_NEAR_POINT.\n", mins[2], LOWEST_NEAR_POINT);
									numBoundsCulledSurfs++;
									continue;
								}
							}
#endif //CULL_BY_LOWEST_NEAR_POINT

							if (!playableBoundsInvalid && mins[2] < mapPlayableMins[2] && maxs[2] < mapPlayableMins[2])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}

							/* // Hmm don't cull the tops od trees, sticking out of the map, etc...
							if (!playableBoundsInvalid && mins[2] > mapPlayableMaxs[2] && maxs[2] > mapPlayableMaxs[2])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}*/

							if (!playableBoundsInvalid && mins[1] < mapPlayableMins[1] && maxs[1] < mapPlayableMins[1])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}

							if (!playableBoundsInvalid && mins[1] > mapPlayableMaxs[1] && maxs[1] > mapPlayableMaxs[1])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}

							if (!playableBoundsInvalid && mins[0] < mapPlayableMins[0] && maxs[0] < mapPlayableMins[0])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}

							if (!playableBoundsInvalid && mins[0] > mapPlayableMaxs[0] && maxs[0] > mapPlayableMaxs[0])
							{// Outside map bounds, definately cull...
								//Sys_Printf("Culled one!\n");
								numBoundsCulledSurfs++;
								continue;
							}
						}

						//Sys_Printf("top: %.4f. bottom: %.4f.\n", top, bottom);

						if (!HAS_COLLISION_INFO
							&& !forcedFullSolid
							&& !forcedSolid 
							&& ((cullSmallSolids || si->isTreeSolid) && !(si->skipSolidCull || si->isMapObjectSolid)))
						{// Cull small stuff and the tops of trees...
							vec3_t size;
							float sz;

							if (top != -999999 && bottom != -999999)
							{
								float s = top - bottom;
								float newtop = bottom + (s / 2.0);
								//float newtop = bottom + (s / 4.0);

								if (ALLOW_CULL_HALF_SIZE) newtop = bottom + (s / 4.0); // Special case for high pine trees, we can cull much more for FPS yay! :)

								//Sys_Printf("newtop: %.4f. top: %.4f. bottom: %.4f. mins: %.4f. maxs: %.4f.\n", newtop, top, bottom, mins[2], maxs[2]);

								if (mins[2] > newtop || mins[2] > bottom + 768.0/*512.0*/) // 512 is > JKA max jump height...
								{
									//Sys_Printf("CULLED: %.4f > %.4f.\n", maxs[2], newtop);
									numHeightCulledSurfs++;
									continue;
								}
							}

							VectorSubtract(maxs, mins, size);
							//sz = VectorLength(size);
							sz = maxs[0] - mins[0];
							if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
							if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

							if (sz < 36)
							{
								//Sys_Printf("CULLED: %.4f < 30. (%.4f %.4f %.4f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
								numSizeCulledSurfs++;
								continue;
							}
						}
						else if (!HAS_COLLISION_INFO && !forcedFullSolid)//if (cullSmallSolids || si->isTreeSolid)
						{// Only cull stuff too small to fall through...
							vec3_t size;
							float sz;

							VectorSubtract(maxs, mins, size);
							//sz = VectorLength(size);
							sz = maxs[0] - mins[0];
							if (maxs[1] - mins[1] > sz) sz = maxs[1] - mins[1];
							if (maxs[2] - mins[2] > sz) sz = maxs[2] - mins[2];

							if (sz <= 16)//24)//16)
							{
								//Sys_Printf("CULLED: %.4f < 30. (%.4f %.4f %.4f)\n", sz, maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
								numSizeCulledSurfs++;
								continue;
							}
						}

						//#define __FORCE_TREE_META__
//#if defined(__FORCE_TREE_META__)
//						if (meta) si->forceMeta = qtrue; // much slower...
//#endif

//#define __AXIAL_COLLISION_BRUSHES__

#pragma omp ordered
						{
#ifdef __AXIAL_COLLISION_BRUSHES__
#pragma omp critical
							{
								/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
								//buildBrush = AllocBrush(24/*48*/); // UQ1: 48 seems to be more then is used... Wasting memory...
								//buildBrush->entityNum = mapEntityNum;
								//buildBrush->mapEntityNum = mapEntityNum;
								//buildBrush->original = buildBrush;
								//buildBrush->contentShader = si;
								//buildBrush->compileFlags = si->compileFlags;
								//buildBrush->contentFlags = si->contentFlags;

								buildBrush = ModelBrushFromBounds(mins[0], mins[1], mins[2], maxs[0], maxs[0], maxs[0], si);
								buildBrush->entityNum = mapEntityNum;
								buildBrush->mapEntityNum = mapEntityNum;
								buildBrush->original = buildBrush;
								buildBrush->contentShader = si;
								buildBrush->compileFlags = si->compileFlags;
								buildBrush->contentFlags = si->contentFlags;

								if (si && FORCED_STRUCTURAL && (si->compileFlags & C_SOLID) /*&& !(compileFlags & C_NODRAW)*/)
								{// Forced structural option is set, force anything solid to also be structural and not detail...
									si->compileFlags &= ~C_DETAIL;
									si->compileFlags |= C_STRUCTURAL;

									buildBrush->detail = qfalse;

									// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
									if (normalEpsilon > 0)
										normalEpsilon = 0;
									if (distanceEpsilon > 0)
										distanceEpsilon = 0;

									//Sys_Printf("%s was forced structural.\n", si->shader);
								}
								else if (si->isTreeSolid || si->isMapObjectSolid || (si->compileFlags & C_DETAIL))
								{
									buildBrush->detail = qtrue;
								}
								else if (si->compileFlags & C_STRUCTURAL) // allow forced structural brushes here
								{
									buildBrush->detail = qfalse;

									// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
									if (normalEpsilon > 0)
										normalEpsilon = 0;
									if (distanceEpsilon > 0)
										distanceEpsilon = 0;
								}
								else
								{
									buildBrush->detail = qtrue;
								}

								/* set up brush sides */
								//buildBrush->numsides = 5;
								buildBrush->sides[0].shaderInfo = si;

								for (j = 1; j < buildBrush->numsides; j++)
								{
									buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
									buildBrush->sides[j].culled = qtrue;
								}

								//buildBrush->sides[0].planenum = FindFloatPlane(plane, plane[3], 3, points);
								//buildBrush->sides[1].planenum = FindFloatPlane(pa, pa[3], 2, &points[1]); // pa contains points[1] and points[2]
								//buildBrush->sides[2].planenum = FindFloatPlane(pb, pb[3], 2, &points[0]); // pb contains points[0] and points[1]
								//buildBrush->sides[3].planenum = FindFloatPlane(pc, pc[3], 2, &points[2]); // pc contains points[2] and points[0] (copied to points[3]
								//buildBrush->sides[4].planenum = FindFloatPlane(reverse, reverse[3], 3, backs);

								/* add to entity */
								if (CreateBrushWindings(buildBrush))
								{
									int numsides;

									AddBrushBevels();
									//%	EmitBrushes( buildBrush, NULL, NULL );

									numsides = buildBrush->numsides;

									if (!RemoveDuplicateBrushPlanes(buildBrush))
									{// UQ1: Testing - This would create a mirrored plane... free it...
										FreeBrush(buildBrush);
										//Sys_Printf("Removed a mirrored plane\n");
									}
									else
									{
										for (j = 1; j < buildBrush->numsides; j++)
										{
											buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
																					//buildBrush->sides[j].culled = qtrue;
										}

										//InsertModelCullSides(&entities[mapEntityNum], buildBrush);

										buildBrush->next = entities[mapEntityNum].brushes;
										entities[mapEntityNum].brushes = buildBrush;
										entities[mapEntityNum].numBrushes++;
									}
								}
								else
								{
									FreeBrush(buildBrush);
								}
							} // #pragma omp critical
#else //!__AXIAL_COLLISION_BRUSHES__
#pragma omp critical
							{
								/* build a brush */ // -- UQ1: Moved - Why allocate when its not needed...
								buildBrush = AllocBrush( 24/*48*/ ); // UQ1: 48 seems to be more then is used... Wasting memory...
								buildBrush->entityNum = mapEntityNum;
								buildBrush->mapEntityNum = mapEntityNum;
								buildBrush->original = buildBrush;
								buildBrush->contentShader = si;
								buildBrush->compileFlags = si->compileFlags;
								buildBrush->contentFlags = si->contentFlags;

								if (si && FORCED_STRUCTURAL && (si->compileFlags & C_SOLID) /*&& !(compileFlags & C_NODRAW)*/)
								{// Forced structural option is set, force anything solid to also be structural and not detail...
									si->compileFlags &= ~C_DETAIL;
									si->compileFlags |= C_STRUCTURAL;

									buildBrush->detail = qfalse;

									// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
									if (normalEpsilon > 0)
										normalEpsilon = 0;
									if (distanceEpsilon > 0)
										distanceEpsilon = 0;

									//Sys_Printf("%s was forced structural.\n", si->shader);
								}
								else if (si->isTreeSolid || si->isMapObjectSolid || (si->compileFlags & C_DETAIL))
								{
									buildBrush->detail = qtrue;
								}
								else if (si->compileFlags & C_STRUCTURAL) // allow forced structural brushes here
								{
									buildBrush->detail = qfalse;

									// only allow EXACT matches when snapping for these (this is mostly for caulk brushes inside a model)
									if(normalEpsilon > 0)
										normalEpsilon = 0;
									if(distanceEpsilon > 0)
										distanceEpsilon = 0;
								}
								else
								{
									buildBrush->detail = qtrue;
								}

								/* set up brush sides */
								buildBrush->numsides = 5;
								buildBrush->sides[ 0 ].shaderInfo = si;

								for( int j = 1; j < buildBrush->numsides; j++ )
								{
									buildBrush->sides[ j ].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
									buildBrush->sides[ j ].culled = qtrue;
								}

								buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
								buildBrush->sides[ 1 ].planenum = FindFloatPlane( pa, pa[ 3 ], 2, &points[ 1 ] ); // pa contains points[1] and points[2]
								buildBrush->sides[ 2 ].planenum = FindFloatPlane( pb, pb[ 3 ], 2, &points[ 0 ] ); // pb contains points[0] and points[1]
								buildBrush->sides[ 3 ].planenum = FindFloatPlane( pc, pc[ 3 ], 2, &points[ 2 ] ); // pc contains points[2] and points[0] (copied to points[3]
								buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 3, backs );

								/* add to entity */
								if( CreateBrushWindings( buildBrush ) )
								{
									int numsides;

									AddBrushBevels();
									//%	EmitBrushes( buildBrush, NULL, NULL );

									numsides = buildBrush->numsides;

									if (!RemoveDuplicateBrushPlanes( buildBrush )/* && !forcedFullSolid*/)
									{// UQ1: Testing - This would create a mirrored plane... free it...
										FreeBrush(buildBrush);
										//Sys_Printf("Removed a mirrored plane\n");
									}
									else
									{
										for (int j = 1; j < buildBrush->numsides; j++)
										{
											buildBrush->sides[j].shaderInfo = NULL; // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
											//buildBrush->sides[j].culled = qtrue;
										}

										//InsertModelCullSides(&entities[mapEntityNum], buildBrush);

										buildBrush->next = entities[ mapEntityNum ].brushes;
										entities[ mapEntityNum ].brushes = buildBrush;
										entities[ mapEntityNum ].numBrushes++;
									}
								}
								else
								{
									//Sys_Printf("%s failed windings.\n", picoShaderName);
									FreeBrush(buildBrush);
								}
							} // #pragma omp critical
#endif //__AXIAL_COLLISION_BRUSHES__
						} // #pragma omp ordered

						//if (buildBrush && !(*((int*)buildBrush) == 0xFEFEFEFE))
						//	InsertModelCullSides(&entities[mapEntityNum], buildBrush);
					}
					else
					{
						continue;
					}

					normalEpsilon = normalEpsilon_save;
					distanceEpsilon = distanceEpsilon_save;
				}
			}

			if (centerChunkPass == 2)
			{
#pragma omp critical (__PROGRESS_BAR__)
				{
					printLabelledProgress("CenterChunkCollision", totalIndexesToAdd, totalIndexesToAdd);
				}
			}
		}
	}

	if (MODEL_IS_PROCEDURAL) {
		// Free this procerural model, we don't need it any more...
		PicoFreeModel(model);
	}
}

int GetPicoVertCount(picoModel_t *picoModel)
{
	/* each surface on the model will become a new map drawsurface */
	int numSurfaces = PicoGetModelNumSurfaces(picoModel);
	int numVertexes = 0;
	int numIndexes = 0;

	for (int s = 0; s < numSurfaces; s++)
	{
		picoSurface_t		*surface;

		/* get surface */
		surface = PicoGetModelSurface(picoModel, s);

		if (surface == NULL)
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
			continue;

		numVertexes += surface->numVertexes;
		numIndexes += surface->numIndexes;
	}

	return numVertexes;
}

int GetPicoIndexCount(picoModel_t *picoModel)
{
	/* each surface on the model will become a new map drawsurface */
	int numSurfaces = PicoGetModelNumSurfaces(picoModel);
	int numVertexes = 0;
	int numIndexes = 0;

	for (int s = 0; s < numSurfaces; s++)
	{
		picoSurface_t		*surface;

		/* get surface */
		surface = PicoGetModelSurface(picoModel, s);

		if (surface == NULL)
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
			continue;

		numVertexes += surface->numVertexes;
		numIndexes += surface->numIndexes;
	}

	return numIndexes;
}

void WzMap_PreloadModel(char *model, int frame, int *numLoadedModels, int allowSimplify, qboolean loadCollision)
{
	picoModel_t		*picoModel = NULL;
	qboolean		loaded = qfalse;

	if (!strncmp(model, "procedural", 10)) {
		Sys_Printf("model %s is procedural, and therefore does not need to be preloaded.\n", model);
		return;
	}

	/* load the model */
	loaded = PreloadModel((char*)model, frame);

	if (loaded)
	{
		*numLoadedModels++;
	}

	/* warn about missing models */
	picoModel = FindModel((char*)model, frame);

	if (!picoModel || picoModel->numSurfaces == 0)
	{
		Sys_Warning("Failed to load model '%s' frame %i", model, frame);
		return;
	}
	else if (picoModel->isCollisionModel)
	{// Skip the collision model loading, this is a collision model...

	}
	else if (!loadCollision)
	{// Skip the collision model loading, this is a collision model...

	}
	else if (allowSimplify == 4 /*&& !StringContainsWord(model, "warzone/plants") && !StringContainsWord(model, "warzone/foliage")*/)
	{// Generate a collsion box model...
		int baseNumVerts = GetPicoVertCount(picoModel);
		int baseNumIdx = GetPicoIndexCount(picoModel);

		// UQ1: Testing... Simple box for collision planes...
		char			tempCollisionModel[512] = { 0 };
		char			collisionModelObj[512] = { 0 };

		sprintf(tempCollisionModel, "%s", model);
		StripExtension(tempCollisionModel);
		sprintf(collisionModelObj, "%s_collision_box.%s", tempCollisionModel, "obj");

		/* try to find model */
		picoModel_t	*picoModel2 = FindModel(collisionModelObj, frame);
		
		if (picoModel2)
		{
			int numVerts = GetPicoVertCount(picoModel2);
			int numIdx = GetPicoIndexCount(picoModel2);

			Sys_Printf("loaded model %s (%i verts %i indexes). collision box model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
			picoModel2->isCollisionModel = qtrue;
			return;
		}

		/* get model */
		picoModel2 = LoadModel(collisionModelObj, frame);

		if (picoModel2)
		{
			int numVerts = GetPicoVertCount(picoModel2);
			int numIdx = GetPicoIndexCount(picoModel2);

			Sys_Printf("loaded model %s (%i verts %i indexes). collision box model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
			picoModel2->isCollisionModel = qtrue;
			*numLoadedModels++;
			return;
		}
		else
		{
			//ForceCrash();

			// None exists, so go ahead and generate a new collision box...
			Sys_Printf("Generating collision box model for model %s.\n", model);

			extern void CollisionBox(picoModel_t *model, char *fileNameOut);
			CollisionBox(picoModel, collisionModelObj);

			/* try to find model */
			picoModel_t	*picoModel2 = FindModel(collisionModelObj, frame);

			if (picoModel2)
			{
				int numVerts = GetPicoVertCount(picoModel2);
				int numIdx = GetPicoIndexCount(picoModel2);

				Sys_Printf("loaded model %s (%i verts %i indexes). collision box model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
				picoModel2->isCollisionModel = qtrue;
				return;
			}

			/* get model */
			picoModel2 = LoadModel(collisionModelObj, frame);

			if (picoModel2)
			{
				int numVerts = GetPicoVertCount(picoModel2);
				int numIdx = GetPicoIndexCount(picoModel2);

				Sys_Printf("loaded model %s (%i verts %i indexes). collision box model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
				picoModel2->isCollisionModel = qtrue;
				*numLoadedModels++;
				return;
			}
		}

		Sys_Printf("loaded model %s, but a collision box model failed to be generated!??!?!??!\n", model);
	}
	else
	{
		/* debug */
		//if( loaded && picoModel && picoModel->numSurfaces != 0  )
		//	Sys_Printf("loaded %s: %i vertexes %i triangles\n", PicoGetModelFileName( picoModel ), PicoGetModelTotalVertexes( picoModel ), PicoGetModelTotalIndexes( picoModel ) / 3 );

		int baseNumVerts = GetPicoVertCount(picoModel);
		int baseNumIdx = GetPicoIndexCount(picoModel);

		if (USE_CONVEX_HULL_MODELS)
		{// Check if there is a convex hull collision model first...
			char			tempCollisionModel[512] = { 0 };
			char			collisionModel[512] = { 0 };
			char			collisionModelObj[512] = { 0 };
			char			collisionModelMd3[512] = { 0 };
			char			tempCollisionModelExt[32] = { 0 };

			sprintf(tempCollisionModel, "%s", model);
			ExtractFileExtension(tempCollisionModel, tempCollisionModelExt);
			StripExtension(tempCollisionModel);
			sprintf(collisionModel, "%s_collision_convex.%s", tempCollisionModel, tempCollisionModelExt);
			sprintf(collisionModelObj, "%s_collision_convex.%s", tempCollisionModel, "obj");
			sprintf(collisionModelMd3, "%s_collision_convex.%s", tempCollisionModel, "md3");

			qboolean loaded2 = PreloadModel((char*)collisionModel, frame);

			/* warn about missing models */
			picoModel_t *picoModel2 = FindModel((char*)collisionModel, frame);

			if (/*loaded2 &&*/ picoModel2)
			{
				int numVerts = GetPicoVertCount(picoModel2);
				int numIdx = GetPicoIndexCount(picoModel2);

				picoModel2->isCollisionModel = qtrue;
				Sys_Printf("loaded model %s (%i verts %i indexes). convex collision model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModel, numVerts, numIdx);
				*numLoadedModels++;
				return;
			}
			else
			{
				loaded2 = PreloadModel((char*)collisionModelObj, frame);

				/* warn about missing models */
				picoModel2 = FindModel((char*)collisionModelObj, frame);

				if (/*loaded2 &&*/ picoModel2)
				{
					int numVerts = GetPicoVertCount(picoModel2);
					int numIdx = GetPicoIndexCount(picoModel2);

					picoModel2->isCollisionModel = qtrue;
					Sys_Printf("loaded model %s (%i verts %i indexes). convex collision model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
					*numLoadedModels++;
					return;
				}
				else
				{
					loaded2 = PreloadModel((char*)collisionModelMd3, frame);

					/* warn about missing models */
					picoModel2 = FindModel((char*)collisionModelMd3, frame);

					if (/*loaded2 &&*/ picoModel2)
					{
						int numVerts = GetPicoVertCount(picoModel2);
						int numIdx = GetPicoIndexCount(picoModel2);

						picoModel2->isCollisionModel = qtrue;
						Sys_Printf("loaded model %s (%i verts %i indexes). convex collision model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelMd3, numVerts, numIdx);
						*numLoadedModels++;
						return;
					}
				}
			}
		}

		char			tempCollisionModel[512] = { 0 };
		char			collisionModel[512] = { 0 };
		char			collisionModelObj[512] = { 0 };
		char			tempCollisionModelExt[32] = { 0 };

		sprintf(tempCollisionModel, "%s", model);
		ExtractFileExtension(tempCollisionModel, tempCollisionModelExt);
		StripExtension(tempCollisionModel);
		sprintf(collisionModel, "%s_collision.%s", tempCollisionModel, tempCollisionModelExt);
		sprintf(collisionModelObj, "%s_collision.%s", tempCollisionModel, "obj");

		qboolean loaded2 = PreloadModel((char*)collisionModel, frame);

		/* warn about missing models */
		picoModel_t *picoModel2 = FindModel((char*)collisionModel, frame);

		if (/*loaded2 &&*/ picoModel2)
		{
			int numVerts = GetPicoVertCount(picoModel2);
			int numIdx = GetPicoIndexCount(picoModel2);

			picoModel2->isCollisionModel = qtrue;
			Sys_Printf("loaded model %s (%i verts %i indexes). collision model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModel, numVerts, numIdx);
			*numLoadedModels++;
			return;
		}
		/*else if (!loaded2 && picoModel2)
		{
			picoModel2->isCollisionModel = qtrue;
			Sys_Printf("loaded model %s. collision model %s.\n", model, collisionModel);
			return;
		}*/
		else
		{
			//Sys_Printf("loaded model %s. collision model none.\n", model);

			picoModel2 = FindModel((char*)collisionModelObj, frame);

			loaded2 = PreloadModel((char*)collisionModelObj, frame);

			/* warn about missing models */
			picoModel2 = FindModel((char*)collisionModelObj, frame);

			if (/*loaded2 &&*/ picoModel2)
			{
				int numVerts = GetPicoVertCount(picoModel2);
				int numIdx = GetPicoIndexCount(picoModel2);

				picoModel2->isCollisionModel = qtrue;
				Sys_Printf("loaded model %s (%i verts %i indexes). collision model %s (%i verts %i indexes).\n", model, baseNumVerts, baseNumIdx, collisionModelObj, numVerts, numIdx);
				*numLoadedModels++;
			}
			/*else if (!loaded2 && picoModel2)
			{
				picoModel2->isCollisionModel = qtrue;
				Sys_Printf("loaded model %s. collision model %s.\n", model, collisionModelObj);
			}*/
			else
			{
				int		numSurfaces = PicoGetModelNumSurfaces(picoModel);

				for (int s = 0; s < numSurfaces; s++)
				{
					int				skin = 0;

					picoModel->hasCollisionGeometry = qfalse;

					/* get surface */
					picoSurface_t	*surface = PicoGetModelSurface(picoModel, s);

					if (StringContainsWord(surface->name, "collision") || StringContainsWord(surface->name, "system/nodraw_solid")) {
						picoModel->hasCollisionGeometry = qtrue;
						Sys_Printf("loaded model %s (%i verts %i indexes) contains collision geometry (surface name: collision).\n", model, baseNumVerts, baseNumIdx);
						return; // Collision is built into this model...
					}

					if (surface == NULL)
						continue;

					/* only handle triangle surfaces initially (fixme: support patches) */
					if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
						continue;

					char			*picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);

					if (StringContainsWord(picoShaderName, "system/nodraw_solid") || StringContainsWord(picoShaderName, "collision")) {
						picoModel->hasCollisionGeometry = qtrue;
						Sys_Printf("loaded model %s (%i verts %i indexes) contains collision geometry (shader name: system/nodraw_solid).\n", model, baseNumVerts, baseNumIdx);
						return; // Collision is built into this model...
					}
				}

				// allowSimplify : 0 = allow none. 1 = allow convex hull. 2 = allow simplify. 3 = allow both.
#ifdef __MODEL_CONVEX_HULL__
				if (USE_CONVEX_HULL_MODELS && allowSimplify == 1 || allowSimplify == 3)
				{
					// UQ1: Testing... Mesh convex hull for collision planes...
					Sys_Printf("Generating convex hull collision model for model %s.\n", model);

					ConvexHull(picoModel, collisionModelObj); // Falls back to simplify/decimate method on fail if __MODEL_SIMPLIFY__ is also defined...

					loaded2 = PreloadModel((char*)collisionModelObj, 0);

					if (loaded2) {
						//Sys_Printf("Loaded new convex collision model %s.\n", collisionModelObj);
						*numLoadedModels++;
					}

					picoModel2 = FindModel((char*)collisionModelObj, 0);

					if (picoModel2)
					{
						int numVerts = GetPicoVertCount(picoModel2);
						int numIdx = GetPicoIndexCount(picoModel2);

						Sys_Printf("Loaded new convex collision model %s (%i verts %i indexes).\n", collisionModelObj, numVerts, numIdx);
						picoModel2->isCollisionModel = qtrue;
						return;
					}
				}
#endif //__MODEL_CONVEX_HULL__

#ifdef __MODEL_SIMPLIFY__
				if (allowSimplify == 2 || allowSimplify == 3)
				{
					// UQ1: Testing... Mesh simplification for collision planes...
					Sys_Printf("Generating simplified collision model for model %s.\n", model);

					Decimate(picoModel, collisionModelObj);

					loaded2 = PreloadModel((char*)collisionModelObj, 0);

					if (loaded2) {
						//Sys_Printf("Loaded new simplified collision model %s.\n", collisionModelObj);
						*numLoadedModels++;
					}

					picoModel2 = FindModel((char*)collisionModelObj, 0);

					if (picoModel2)
					{
						int numVerts = GetPicoVertCount(picoModel2);
						int numIdx = GetPicoIndexCount(picoModel2);

						Sys_Printf("Loaded new simplified collision model %s (%i verts %i indexes).\n", collisionModelObj, numVerts, numIdx);
						picoModel2->isCollisionModel = qtrue;
						return;
					}
				}
#endif //__MODEL_SIMPLIFY__

				Sys_Printf("loaded model %s (%i verts %i indexes). collision model %s. Suggestion: Create a <modelname>_collision.%s\n", model, baseNumVerts, baseNumIdx, "none", tempCollisionModelExt);
			}
		}
	}
}

/*
LoadTriangleModels()
preload triangle models map is using
*/

void LoadTriangleModels(void)
{
	int num, frame, start, numLoadedModels;

	numLoadedModels = 0;

	/* note it */
	Sys_PrintHeading("--- LoadTriangleModels ---\n");

	/* load */
	start = I_FloatTime();
	
	for (num = 1; num < numEntities; num++)
	{
		//printLabelledProgress("LoadTriangleModels", num, numEntities);

		/* get ent */
		entity_t *e = &entities[num];

		/* convert misc_models into raw geometry  */
		if (Q_stricmp("misc_model", ValueForKey(e, "classname")) && Q_stricmp("misc_gamemodel", ValueForKey(e, "classname")) && Q_stricmp("misc_lodmodel", ValueForKey(e, "classname")))
			continue;

		/* get model name */
		/* vortex: add _model synonim */
		const char *model = ValueForKey(e, "_model");
		if (model[0] == '\0')
			model = ValueForKey(e, "model");
		if (model[0] == '\0')
			continue;

		if (!strncmp(model, "procedural", 10)) {
			continue;
		}

		/* get model frame */
		if (KeyExists(e, "_frame"))
			frame = IntForKey(e, "_frame");
		else if (KeyExists(e, "frame"))
			frame = IntForKey(e, "frame");
		else
			frame = 0;

		if (StringContainsWord(model, "_collision"))
			WzMap_PreloadModel((char *)model, frame, &numLoadedModels, 0, qfalse);
		else
			WzMap_PreloadModel((char *)model, frame, &numLoadedModels, 0, qtrue);
	}

	/* print overall time */
	//Sys_Printf (" (%d)\n", (int) (I_FloatTime() - start) );

	/* emit stats */
	Sys_Printf("%9i unique model/frame combinations\n", numLoadedModels);
}


/*
AddTriangleModels()
adds misc_model surfaces to the bsp
*/

extern int CULLSIDES_AFTER_MODEL_ADITION;

void AddTriangleModels(int entityNum, qboolean quiet, qboolean cullSmallSolids, qboolean chunksPass, qboolean centerChunkPass)
{
	int				added_surfaces = 0, added_triangles = 0, added_verts = 0, added_brushes = 0;
	int				total_added_surfaces = 0, total_added_triangles = 0, total_added_verts = 0, total_added_brushes = 0;
	int				num, frame, skin, spawnFlags;
	char			castShadows, recvShadows;
	entity_t		*e, *e2;
	const char		*targetName;
	const char		*target, *model, *value;
	char			shader[MAX_QPATH];
	shaderInfo_t	*celShader;
	float			temp, baseLightmapScale, lightmapScale, uvScale, pushVertexes;
	int				baseSmoothNormals, smoothNormals;
	int				baseVertTexProj, vertTexProj;
	qboolean        noAlphaFix, skybox;
	vec3_t			origin, scale, angles, baseLightmapAxis, lightmapAxis, baseMinlight, baseMinvertexlight, baseAmbient, baseColormod, minlight, minvertexlight, ambient, colormod;
	m4x4_t			transform, boxTransform;
	epair_t			*ep;
	remap_t			*remap, *remap2;
	char			*split;

	numSolidSurfs = 0;
	numHeightCulledSurfs = 0;
	numSizeCulledSurfs = 0;
	numExperimentalCulled = 0;
	numBoundsCulledSurfs = 0;
	removed_numHiddenFaces = 0;
	removed_numCoinFaces = 0;


	/* note it */
	if (!quiet) Sys_PrintHeadingVerbose("--- AddTriangleModels ---\n");

	/* get current brush entity targetname */
	e = &entities[entityNum];

	NUM_ORIGINAL_BRUSHES = 0;

	for (brush_t *b1 = e->brushes; b1; b1 = b1->next)
		NUM_ORIGINAL_BRUSHES++;

	if (e == entities)
		targetName = "";
	else
	{
		targetName = ValueForKey(e, "targetname");

		/* misc_model entities target non-worldspawn brush model entities */
		if (targetName[0] == '\0')
		{
			//Sys_Printf( "Failed Targetname\n" );
			return;
		}
	}

	/* vortex: get lightmap scaling value for this entity */
	GetEntityLightmapScale(e, &baseLightmapScale, 0);

	/* vortex: get lightmap axis for this entity */
	GetEntityLightmapAxis(e, baseLightmapAxis, NULL);

	/* vortex: per-entity normal smoothing */
	GetEntityNormalSmoothing(e, &baseSmoothNormals, 0);

	/* vortex: per-entity _minlight, _minvertexlight, _ambient, _color, _colormod */
	GetEntityMinlightAmbientColor(e, NULL, baseMinlight, baseMinvertexlight, baseAmbient, baseColormod, qtrue);

	/* vortex: vertical texture projection */
	baseVertTexProj = IntForKey(e, "_vtcproj");
	if (baseVertTexProj <= 0)
		baseVertTexProj = IntForKey(e, "_vp");
	if (baseVertTexProj <= 0)
		baseVertTexProj = 0;

#ifdef CULL_BY_LOWEST_NEAR_POINT
	/* walk the entity list */
	int numDone = 0;

	for (num = 1; num < numEntities; num++)
	{
		shaderInfo_t *overrideShader = NULL;
		
		/* get e3 */
		entity_t *e3 = &entities[num];

		if (e3->alreadyAdded)
		{
			continue;
		}

		numDone++;

		qboolean skip = qfalse;

#pragma omp critical
		{
			if (!quiet) printLabelledProgress("FindLowestPoints", numDone, numEntities);

			/* convert misc_models into raw geometry  */
			if (Q_stricmp("misc_model", ValueForKey(e3, "classname")) && Q_stricmp("misc_lodmodel", ValueForKey(e3, "classname")))
			{
				//Sys_Printf( "Failed Classname\n" );
				//continue;
				skip = qtrue;
			}

			/* ydnar: added support for md3 models on non-worldspawn models */
			if (!skip)
			{
				target = ValueForKey(e3, "target");
				if (strcmp(target, targetName))
				{
					//Sys_Printf( "Failed Target\n" );
					//continue;
					skip = qtrue;
				}
			}

			/* get model name */
			/* vortex: add _model synonim */
			if (!skip)
			{
				model = ValueForKey(e3, "_model");
				if (model[0] == '\0')
					model = ValueForKey(e3, "model");
				if (model[0] == '\0')
				{
					Sys_Warning(e3->mapEntityNum, "misc_model at %i %i %i without a model key", (int)origin[0], (int)origin[1], (int)origin[2]);
					//Sys_Printf( "Failed Model\n" );
					//continue;
					skip = qtrue;
				}
			}

			/* get origin */
			if (!skip)
			{
				GetVectorForKey(e3, "origin", origin);
			}
		}

		if (skip) continue; // cant continue inside omp critical

		VectorSubtract(origin, e->origin, origin);	/* offset by parent */

		int originAsLowpoint = atoi(ValueForKey(e3, "_originAsLowPoint"));

		if (originAsLowpoint >= 2 || !strncmp(model, "procedural", 10))
		{
			e3->lowestPointNear = origin[2] - 64.0;
		}
		else if (originAsLowpoint > 0)
		{// Marked by the generator and mapInfo to use the model origin (but down a little) as the lowest point near for culling...
			float zOffset = FloatForKey(e3, "_originOffset");
			if (zOffset < 0) zOffset = -zOffset;
			if (zOffset < 16.0) zOffset = 16.0;
			e3->lowestPointNear = origin[2] - zOffset; //origin[2]-128.0;
		}
		else
		{
			picoModel_t *m = LoadModel(model, 0);

			if (m == NULL)
			{// Fallback...
				//e3->lowestPointNear = origin[2] - 256.0;
				e3->lowestPointNear = LowestMapPointNear(origin, 4096.0, model);
			}
			else
			{
				/* get scale */
				vec3_t tempScale;
				tempScale[0] = tempScale[1] = tempScale[2] = 1.0f;
				temp = FloatForKey(e3, "modelscale");
				if (temp != 0.0f)
					tempScale[0] = tempScale[1] = tempScale[2] = temp;
				value = ValueForKey(e3, "modelscale_vec");
				if (value[0] != '\0')
					sscanf(value, "%f %f %f", &tempScale[0], &tempScale[1], &tempScale[2]);

				vec3_t size;
				VectorSubtract(m->maxs, m->mins, size);
				size[0] *= tempScale[0];
				size[1] *= tempScale[1];
				size[2] *= tempScale[2];
				//float maxSize = max(size[0], max(size[1], size[2]));
				float maxSize = max(size[0], size[1]); // Don't need Z, thats what we are searching for...
				float radius = maxSize;// / 1.5;

				if (radius < 1024.0) radius = 1024.0;

				//Sys_Printf("%s. mins %.4f %.4f %.4f. maxs %.4f %.4f %.4f. scale %.4f %.4f %.4f. radius %.4f.\n", model, m->mins[0], m->mins[1], m->mins[2], m->maxs[0], m->maxs[1], m->maxs[2], scale[0], scale[1], scale[2], radius);

				e3->lowestPointNear = LowestMapPointNear(origin, radius, model);
			}
		}
	}

	if (!quiet) printLabelledProgress("FindLowestPoints", numEntities, numEntities);
#endif //CULL_BY_LOWEST_NEAR_POINT

	/* walk the entity list */
	for (num = 1; num < numEntities; num++)
	{
		shaderInfo_t *overrideShader = NULL;
		qboolean forcedSolid = qfalse;
		qboolean forcedFullSolid = qfalse;
		bool MODEL_IS_PROCEDURAL = false;

		if (!quiet) printLabelledProgress("AddTriangleModels", num, numEntities);

		/* get e2 */
		e2 = &entities[num];

		if (e2->alreadyAdded) continue;

		/* convert misc_models into raw geometry  */
		if (Q_stricmp("misc_model", ValueForKey(e2, "classname")) && Q_stricmp("misc_lodmodel", ValueForKey(e2, "classname")))
		{
			//Sys_Printf( "Failed Classname\n" );
			continue;
		}

		qboolean isLodModel = (qboolean)!Q_stricmp("misc_lodmodel", ValueForKey(e2, "classname"));

		/* ydnar: added support for md3 models on non-worldspawn models */
		target = ValueForKey(e2, "target");
		if (strcmp(target, targetName))
		{
			//Sys_Printf( "Failed Target\n" );
			continue;
		}

		/* get model name */
		/* vortex: add _model synonim */
		model = ValueForKey(e2, "_model");
		if (model[0] == '\0')
			model = ValueForKey(e2, "model");
		if (model[0] == '\0')
		{
			Sys_Warning(e2->mapEntityNum, "misc_model at %i %i %i without a model key", (int)origin[0], (int)origin[1], (int)origin[2]);
			//Sys_Printf( "Failed Model\n" );
			continue;
		}

		/* get model frame */
		frame = 0;
		if (KeyExists(e2, "frame"))
			frame = IntForKey(e2, "frame");
		if (KeyExists(e2, "_frame"))
			frame = IntForKey(e2, "_frame");

		qboolean HAVE_COLLISION_GEOMETRY = qfalse;

		if (!strncmp(model, "procedural", 10)) {
			MODEL_IS_PROCEDURAL = true;
		}

		if (MODEL_IS_PROCEDURAL) {
			HAVE_COLLISION_GEOMETRY = qtrue;
		} else {
			picoModel_t *picoModel = FindModel((char*)model, frame);

			if (!picoModel)
			{
				picoModel = LoadModel((char*)model, frame);
				picoModel = FindModel((char*)model, frame);
			}

			if (picoModel)
			{
				if (picoModel->hasCollisionGeometry)
				{
					HAVE_COLLISION_GEOMETRY = qtrue;
				}
			}
		}

		char *COLLISION_MODEL = NULL;
		bool collisionIsBox = false;

		int allowCollisionModelTypes = IntForKey(e2, "_allowCollisionModelTypes");

		if (MODEL_IS_PROCEDURAL) {
			//
		}
		else if (USE_BOX_MODELS && allowCollisionModelTypes >= 4)
		{
			char tempCollisionModel[512] = { 0 };
			char collisionModel[512] = { 0 };
			char collisionModelObj[512] = { 0 };
			char collisionModelMD3[512] = { 0 };
			char tempCollisionModelExt[32] = { 0 };

			sprintf(tempCollisionModel, "%s", model);
			ExtractFileExtension(tempCollisionModel, tempCollisionModelExt);
			StripExtension(tempCollisionModel);

			sprintf(collisionModel, "%s_collision_box.%s", tempCollisionModel, tempCollisionModelExt);
			sprintf(collisionModelObj, "%s_collision_box.obj", tempCollisionModel);
			sprintf(collisionModelMD3, "%s_collision_box.md3", tempCollisionModel);

			picoModel_t *picoModel = FindModel((char*)collisionModel, frame);

			if (!picoModel)
			{
				picoModel = LoadModel((char*)collisionModel, frame);
				picoModel = FindModel((char*)collisionModel, frame);
			}

			if (picoModel)
			{
				COLLISION_MODEL = collisionModel;
				collisionIsBox = true;
			}
			else
			{
				picoModel = FindModel((char*)collisionModelObj, frame);

				if (!picoModel)
				{
					picoModel = LoadModel((char*)collisionModelObj, frame);
					picoModel = FindModel((char*)collisionModelObj, frame);
				}

				if (picoModel)
				{
					COLLISION_MODEL = collisionModelObj;
					//Sys_Printf("Found box model %s.\n", COLLISION_MODEL);
					collisionIsBox = true;
				}
				else
				{
					picoModel = FindModel((char*)collisionModelMD3, frame);

					if (!picoModel)
					{
						picoModel = LoadModel((char*)collisionModelMD3, frame);
						picoModel = FindModel((char*)collisionModelMD3, frame);
					}

					if (picoModel)
					{
						COLLISION_MODEL = collisionModelMD3;
						//Sys_Printf("Found box model %s.\n", COLLISION_MODEL);
						collisionIsBox = true;
					}
					else
					{
						//Sys_Printf("Did not find box models %s or %s.\n", collisionModel, collisionModelObj);
					}
				}
			}
		}

		if (MODEL_IS_PROCEDURAL) {
			//
		}
		else if (COLLISION_MODEL == NULL && USE_CONVEX_HULL_MODELS && (allowCollisionModelTypes >= 3 || allowCollisionModelTypes == 1))
		{// Check if there is a convex hull collision model first...
			char tempCollisionModel[512] = { 0 };
			char collisionModel[512] = { 0 };
			char collisionModelObj[512] = { 0 };
			char collisionModelMD3[512] = { 0 };
			char tempCollisionModelExt[32] = { 0 };

			sprintf(tempCollisionModel, "%s", model);
			ExtractFileExtension(tempCollisionModel, tempCollisionModelExt);
			StripExtension(tempCollisionModel);

			sprintf(collisionModel, "%s_collision_convex.%s", tempCollisionModel, tempCollisionModelExt);
			sprintf(collisionModelObj, "%s_collision_convex.obj", tempCollisionModel);
			sprintf(collisionModelMD3, "%s_collision_convex.md3", tempCollisionModel);

			picoModel_t *picoModel = FindModel((char*)collisionModel, frame);

			if (!picoModel)
			{
				picoModel = LoadModel((char*)collisionModel, frame);
				picoModel = FindModel((char*)collisionModel, frame);
			}

			if (picoModel)
			{
				COLLISION_MODEL = collisionModel;
			}
			else
			{
				picoModel = FindModel((char*)collisionModelObj, frame);

				if (!picoModel)
				{
					picoModel = LoadModel((char*)collisionModelObj, frame);
					picoModel = FindModel((char*)collisionModelObj, frame);
				}

				if (picoModel)
				{
					COLLISION_MODEL = collisionModelObj;
					//Sys_Printf("Found convex hull model %s.\n", COLLISION_MODEL);
				}
				else
				{
					picoModel = FindModel((char*)collisionModelMD3, frame);

					if (!picoModel)
					{
						picoModel = LoadModel((char*)collisionModelMD3, frame);
						picoModel = FindModel((char*)collisionModelMD3, frame);
					}

					if (picoModel)
					{
						COLLISION_MODEL = collisionModelMD3;
						//Sys_Printf("Found convex hull model %s.\n", collisionModelMD3);
					}
					else
					{
						//Sys_Printf("Did not find convex hull models %s or %s.\n", collisionModel, collisionModelObj);
					}
				}
			}
		}

		if (MODEL_IS_PROCEDURAL) {

		}
		else if (COLLISION_MODEL == NULL)
		{// Since we didn't find a convex hull collision model, look for a normal collision model...
			char tempCollisionModel[512] = { 0 };
			char collisionModel[512] = { 0 };
			char collisionModelObj[512] = { 0 };
			char collisionModelMD3[512] = { 0 };
			char tempCollisionModelExt[32] = { 0 };

			sprintf(tempCollisionModel, "%s", model);
			ExtractFileExtension(tempCollisionModel, tempCollisionModelExt);
			StripExtension(tempCollisionModel);

			sprintf(collisionModel, "%s_collision.%s", tempCollisionModel, tempCollisionModelExt);
			sprintf(collisionModelObj, "%s_collision.obj", tempCollisionModel);
			sprintf(collisionModelMD3, "%s_collision.md3", tempCollisionModel);

			picoModel_t *picoModel = FindModel((char*)collisionModel, frame);

			if (!picoModel)
			{
				picoModel = LoadModel((char*)collisionModel, frame);
				picoModel = FindModel((char*)collisionModel, frame);
			}

			if (picoModel)
			{
				COLLISION_MODEL = collisionModel;
			}
			else
			{
				picoModel = FindModel((char*)collisionModelMD3, frame);

				if (!picoModel)
				{
					picoModel = LoadModel((char*)collisionModelMD3, frame);
					picoModel = FindModel((char*)collisionModelMD3, frame);
				}

				if (picoModel)
				{
					COLLISION_MODEL = collisionModelMD3;
				}
				else
				{
					picoModel = FindModel((char*)collisionModelObj, frame);

					if (!picoModel)
					{
						picoModel = LoadModel((char*)collisionModelObj, frame);
						picoModel = FindModel((char*)collisionModelObj, frame);
					}

					if (picoModel)
					{
						COLLISION_MODEL = collisionModelObj;
						//Sys_Printf("Found collision model %s.\n", COLLISION_MODEL);
					}
					else
					{
						//Sys_Printf("Did not find collision models %s or %s.\n", collisionModel, collisionModelObj);
					}
				}
			}
		}

		/* get model skin */
		skin = 0;
		if (KeyExists(e2, "skin"))
			skin = IntForKey(e2, "skin");
		if (KeyExists(e2, "_skin"))
			skin = IntForKey(e2, "_skin");

		/* get explicit shadow flags */
		GetEntityShadowFlags(e2, e, &castShadows, &recvShadows, (e == entities) ? qtrue : qfalse);

		/* get spawnflags */
		spawnFlags = IntForKey(e2, "spawnflags");

		/* get origin */
		GetVectorForKey(e2, "origin", origin);

		if (!USE_SECONDARY_BSP || !GENERATING_SECONDARY_BSP)
		{
			VectorSubtract(origin, e->origin, origin);	/* offset by parent */
		}
		else
		{
			extern vec3_t					USE_SECONDARY_FIRST_RUN_ORIGIN;
			VectorSubtract(origin, USE_SECONDARY_FIRST_RUN_ORIGIN, origin);	/* offset by parent */
		}

		/* get scale */
		scale[0] = scale[1] = scale[2] = 1.0f;
		temp = FloatForKey(e2, "modelscale");
		if (temp != 0.0f)
			scale[0] = scale[1] = scale[2] = temp;
		value = ValueForKey(e2, "modelscale_vec");
		if (value[0] != '\0')
			sscanf(value, "%f %f %f", &scale[0], &scale[1], &scale[2]);

		/* VorteX: get UV scale */
		uvScale = 1.0;
		temp = FloatForKey(e2, "uvscale");
		if (temp != 0.0f)
			uvScale = temp;
		else
		{
			temp = FloatForKey(e2, "_uvs");
			if (temp != 0.0f)
				uvScale = temp;
		}

		/* VorteX: UV autoscale */
		if (IntForKey(e2, "uvautoscale") || IntForKey(e2, "_uvas"))
			uvScale = uvScale * (scale[0] + scale[1] + scale[2]) / 3.0f;

		/* get "angle" (yaw) or "angles" (pitch yaw roll) */
		angles[0] = angles[1] = angles[2] = 0.0f;
		angles[2] = FloatForKey(e2, "angle");
		value = ValueForKey(e2, "angles");
		if (value[0] != '\0')
			sscanf(value, "%f %f %f", &angles[1], &angles[2], &angles[0]);

		if (collisionIsBox && USE_BOX_MODELS > 1)
		{
			vec3_t boxAngles;
			VectorSet(boxAngles, 0, 0, 0);
			m4x4_identity(boxTransform);
			m4x4_pivoted_transform_by_vec3(boxTransform, origin, boxAngles, eXYZ, scale, vec3_origin);
		}

		/* set transform matrix (thanks spog) */
		m4x4_identity(transform);
		m4x4_pivoted_transform_by_vec3(transform, origin, angles, eXYZ, scale, vec3_origin);

		/* get shader remappings */
		remap = NULL;
		for (ep = e2->epairs; ep != NULL; ep = ep->next)
		{
			/* look for keys prefixed with "_remap" */
			if (ep->key != NULL && ep->value != NULL &&
				ep->key[0] != '\0' && ep->value[0] != '\0' &&
				!Q_strncasecmp(ep->key, "_remap", 6))
			{
				/* create new remapping */
				remap2 = remap;
				remap = (remap_t *)safe_malloc(sizeof(*remap));
				remap->next = remap2;
				strcpy(remap->from, ep->value);

				/* split the string */
				split = strchr(remap->from, ';');
				if (split == NULL)
				{
					Sys_Warning(e2->mapEntityNum, "Shader _remap key found in misc_model without a ; character");
					free(remap);
					remap = remap2;
					Sys_Printf("Failed Remapping\n");
					continue;
				}

				/* store the split */
				*split = '\0';
				strcpy(remap->to, (split + 1));

				/* note it */
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", remap->from, remap->to );
			}
		}

		/* ydnar: cel shader support */
		value = ValueForKey(e2, "_celshader");
		if (value[0] == '\0')
			value = ValueForKey(&entities[0], "_celshader");
		if (value[0] != '\0')
		{
			sprintf(shader, "textures/%s", value);
			celShader = ShaderInfoForShader(shader);
		}
		else
			celShader = NULL;

		/* ydnar: cel shader support */
		value = ValueForKey(e2, "_overrideShader");

		if (value[0] != '\0')
		{
			sprintf(shader, "%s", value);
			overrideShader = ShaderInfoForShader(shader);
		}
		else
		{
			overrideShader = NULL;
		}

		value = ValueForKey(e2, "_forcedSolid");

		if (atoi(value) >= 2)
		{
			forcedSolid = qtrue;
			forcedFullSolid = qtrue;
		}
		else if (atoi(value) >= 1)
		{
			forcedSolid = qtrue;
		}
		else
		{
			forcedSolid = qfalse;
		}

		/* vortex: get lightmap scaling value for this entity */
		GetEntityLightmapScale(e2, &lightmapScale, baseLightmapScale);

		/* vortex: get lightmap axis for this entity */
		GetEntityLightmapAxis(e2, lightmapAxis, baseLightmapAxis);

		/* vortex: per-entity normal smoothing */
		GetEntityNormalSmoothing(e2, &smoothNormals, baseSmoothNormals);

		/* vortex: per-entity _minlight, _ambient, _color, _colormod */
		VectorCopy(baseMinlight, minlight);
		VectorCopy(baseMinvertexlight, minvertexlight);
		VectorCopy(baseAmbient, ambient);
		VectorCopy(baseColormod, colormod);
		GetEntityMinlightAmbientColor(e2, NULL, minlight, minvertexlight, ambient, colormod, qfalse);

		/* vortex: vertical texture projection */
		vertTexProj = IntForKey(e2, "_vp");
		if (vertTexProj <= 0)
			vertTexProj = baseVertTexProj;

		/* vortex: prevent alpha-fix stage on entity */
		noAlphaFix = (IntForKey(e2, "_noalphafix") > 0) ? qtrue : qfalse;
		skybox = (IntForKey(e2, "_skybox") > 0) ? qtrue : qfalse;

		/* vortex: push vertexes among their normals (fatboy) */
		pushVertexes = FloatForKey(e2, "_pushvertexes");
		if (pushVertexes == 0)
			pushVertexes = FloatForKey(e2, "_pv");
		pushVertexes += FloatForKey(e2, "_pv2"); // vortex: set by decorator

		qboolean forceIntoMapdata = (IntForKey(e2, "forceIntoMapdata") > 0) ? qtrue : qfalse;

		if (forceIntoMapdata)
		{// UQ1: Force any marked with these into map entity surfaces...
			e2->mapEntityNum = 0;
		}

		qboolean noBoundsCull = (IntForKey(e2, "noBoundsCull") > 0) ? qtrue : qfalse;

		if (noBoundsCull)
		{// UQ1: Force any marked with these into map entity surfaces...
			e2->lowestPointNear = -9999999.9;
		}

		/* Model plane snapping... Begin */
		int origSnap = bevelSnap;
		int modelSnap = IntForKey(e2, "snap");
		
		if (modelSnap > 0)
		{
			bevelSnap = modelSnap;
		}
		/* Model plane snapping... End */

		if (StringContainsWord(model, "warzone/plants"))
		{
			forcedSolid = qfalse;
			forcedFullSolid = qfalse;
		}

		/* insert the model */
#ifdef CULL_BY_LOWEST_NEAR_POINT
		if (COLLISION_MODEL && strlen(COLLISION_MODEL) > 0)
		{
			//Sys_Printf("Adding visible model %s surfaces.\n", COLLISION_MODEL);

			// Add the actual model...
			if (!USE_LODMODEL || !isLodModel) // misc_lodmodel doesn't output map surfaces other then the collision objects...
				InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, overrideShader, qfalse, qfalse, qtrue, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids, e2->lowestPointNear, isLodModel, centerChunkPass ? 1 : 0);

			// Add the collision planes...
			overrideShader = ShaderInfoForShader("textures/system/nodraw_solid");

			//Sys_Printf("Adding collision model %s surfaces.\n", COLLISION_MODEL);
			if (collisionIsBox && USE_BOX_MODELS > 1)
				InsertModel((char*)COLLISION_MODEL, frame, NULL, boxTransform, uvScale, NULL, NULL, overrideShader, qtrue, qtrue, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, qfalse, e2->lowestPointNear, isLodModel, centerChunkPass ? 2 : 0);
			else
				InsertModel((char*)COLLISION_MODEL, frame, NULL, transform, uvScale, NULL, NULL, overrideShader, qtrue, qtrue, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, qfalse, e2->lowestPointNear, isLodModel, centerChunkPass ? 2 : 0);
		}
		else
		{
			if (HAVE_COLLISION_GEOMETRY)
				InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, overrideShader, forcedSolid, forcedFullSolid, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids, e2->lowestPointNear, isLodModel, centerChunkPass ? 1 : 0);
			else if (!USE_LODMODEL || !isLodModel) // misc_lodmodel doesn't output map surfaces other then the collision objects...
				InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, overrideShader, forcedSolid, forcedFullSolid, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids, e2->lowestPointNear, isLodModel, centerChunkPass ? 1 : 0);
		}
#else //!CULL_BY_LOWEST_NEAR_POINT
		if (COLLISION_MODEL && strlen(COLLISION_MODEL) > 0)
		{
			// Add the actual model...
			if (!USE_LODMODEL || !isLodModel) // misc_lodmodel doesn't output map surfaces other then the collision objects...
				InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, overrideShader, qfalse, qfalse, qtrue, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids, 999999.0f, isLodModel, centerChunkPass ? 1 : 0);

			// Add the collision planes...
			overrideShader = ShaderInfoForShader("textures/system/nodraw_solid");
			
			InsertModel((char*)COLLISION_MODEL, frame, NULL, transform, uvScale, NULL, NULL, overrideShader, qtrue, qtrue, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, qfalse, 999999.0f, isLodModel, centerChunkPass ? 2 : 0);
		}
		else
		{
			if (!USE_LODMODEL || !isLodModel) // misc_lodmodel doesn't output map surfaces other then the collision objects...
				InsertModel((char*)model, frame, skin, transform, uvScale, remap, celShader, overrideShader, forcedSolid, forcedFullSolid, qfalse, entityNum, e2->mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapAxis, minlight, minvertexlight, ambient, colormod, 0, smoothNormals, vertTexProj, noAlphaFix, pushVertexes, skybox, &added_surfaces, &added_triangles, &added_verts, &added_brushes, cullSmallSolids, 999999.0f, isLodModel, centerChunkPass ? 1 : 0);
		}
#endif //CULL_BY_LOWEST_NEAR_POINT

		/* Model plane snapping... Begin */
		bevelSnap = origSnap;
		/* Model plane snapping... End */

		//Sys_Printf( "insert model: %s. added_surfaces: %i. added_triangles: %i. added_verts: %i. added_brushes: %i.\n", model, added_surfaces, added_triangles, added_verts, added_brushes );

		total_added_surfaces += added_surfaces;
		total_added_triangles += added_triangles;
		total_added_verts += added_verts;
		total_added_brushes += added_brushes;

		added_surfaces = added_triangles = added_verts = added_brushes = 0;

		/* free shader remappings */
		while (remap != NULL)
		{
			remap2 = remap->next;
			free(remap);
			remap = remap2;
		}

		e2->alreadyAdded = qtrue;
	}

	if (!quiet || centerChunkPass)
	{
		//int totalExpCulled = numExperimentalCulled;
		int totalHeightCulled = numHeightCulledSurfs;
		int totalSizeCulled = numSizeCulledSurfs;
		int totalBoundsCulled = numBoundsCulledSurfs;
		int totalTotalCulled = numExperimentalCulled + numHeightCulledSurfs + numSizeCulledSurfs + totalBoundsCulled;
		int percentExpCulled = 0;
		int percentHeightCulled = 0;
		int percentBoundsCulled = 0;
		int percentSizeCulled = 0;
		int percentTotalCulled = 0;

		//if (totalExpCulled > 0 && numSolidSurfs > 0)
		//	percentExpCulled = (int)(((float)totalExpCulled / (float)numSolidSurfs) * 100.0);

		if (totalHeightCulled > 0 && numSolidSurfs > 0)
			percentHeightCulled = (int)(((float)totalHeightCulled / (float)(totalHeightCulled + numSolidSurfs)) * 100.0);

		if (totalBoundsCulled > 0 && numSolidSurfs > 0)
			percentBoundsCulled = (int)(((float)totalBoundsCulled / (float)(totalBoundsCulled + numSolidSurfs)) * 100.0);

		if (totalSizeCulled > 0 && numSolidSurfs > 0)
			percentSizeCulled = (int)(((float)totalSizeCulled / (float)(totalSizeCulled + numSolidSurfs)) * 100.0);

		if (totalTotalCulled > 0 && numSolidSurfs > 0)
			percentTotalCulled = (int)(((float)totalTotalCulled / (float)(totalTotalCulled + numSolidSurfs)) * 100.0);

		/* emit some stats */
		Sys_Printf("%9d surfaces added\n", total_added_surfaces);
		Sys_Printf("%9d triangles added\n", total_added_triangles);
		Sys_Printf("%9d vertexes added\n", total_added_verts);
		Sys_Printf("%9d brushes added\n", total_added_brushes);
		//Sys_Printf("%9i of %i solid surfaces culled by experimental culling (%i percent).\n", totalExpCulled, numSolidSurfs, percentExpCulled);
		Sys_Printf("%9i of %i solid surfaces culled for height (%i percent).\n", totalHeightCulled, totalHeightCulled + numSolidSurfs, percentHeightCulled);
		Sys_Printf("%9i of %i solid surfaces culled for map bounds (%i percent).\n", totalBoundsCulled, totalBoundsCulled + numSolidSurfs, percentBoundsCulled);
		Sys_Printf("%9i of %i solid surfaces culled for tiny size (%i percent).\n", totalSizeCulled, totalSizeCulled + numSolidSurfs, percentSizeCulled);
		Sys_Printf("%9i of %i total solid surfaces culled (%i percent).\n", totalTotalCulled, totalTotalCulled + numSolidSurfs, percentTotalCulled);
	}

	//InsertModelCullSides(&entities[mapEntityNum]);

	//g_numHiddenFaces = 0;
	//g_numCoinFaces = 0;
	//CullSides( &entities[ mapEntityNum ] );
	//CullSidesStats();

	if (CULLSIDES_AFTER_MODEL_ADITION == 1)
	{
		if ((chunksPass || centerChunkPass) && total_added_surfaces > 0)
		{// Do a second cullsides, to remove excess crap (stuff inside other stuff, etc) after everything was added...
			CullSides(&entities[mapEntityNum]);
			CullSidesStats();
		}
	}
	else if (CULLSIDES_AFTER_MODEL_ADITION > 1)
	{
		if (total_added_surfaces > 0)
		{// Do a second cullsides, to remove excess crap (stuff inside other stuff, etc) after everything was added...
			CullSides(&entities[mapEntityNum]);
			CullSidesStats();
		}
	}
}
