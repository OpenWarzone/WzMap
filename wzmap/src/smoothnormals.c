// UQ1: BSP file normals smoother

/* marker */
#define OPTIMIZE_C

/* dependencies */
#include "q3map2.h"

//
//
//

extern float Distance(vec3_t pos1, vec3_t pos2);

shaderInfo_t *bspShaderInfos[MAX_MAP_MODELS] = { { NULL } };

qboolean	SMOOTHING_WHOLEMAP = qfalse;
qboolean	SMOOTHING_STRICT = qfalse;
int			SMOOTHING_PASSES = 3;

void GenerateNormalsForMeshBSP(bspDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader, int dsNum)
{
	qboolean smoothOnly = qfalse;

	shaderInfo_t *shaderInfo1 = bspShaderInfos[dsNum];

	if (!shaderInfo1)
		return;

	if (shaderInfo1->smoothOnly)
		smoothOnly = qtrue;

	if (shaderInfo1 == caulkShader)
		return;

	if (shaderInfo1 == skipShader)
		return;

	if (!smoothOnly)
	{
		bool broken = false;

		bspDrawVert_t *vs = &bspDrawVerts[ds->firstVert];
		int *idxs = &bspDrawIndexes[ds->firstIndex];

#pragma omp parallel for ordered num_threads(numthreads)
		for (int i = 0; i < ds->numIndexes; i += 3)
		{
			if (broken) continue;

			int tri[3];
			tri[0] = idxs[i];
			tri[1] = idxs[i + 1];
			tri[2] = idxs[i + 2];

			if (tri[0] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[0], cv->numVerts);
				//return;
				broken = true;
				continue;
			}
			if (tri[1] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[1], cv->numVerts);
				//return;
				broken = true;
				continue;
			}
			if (tri[2] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[2], cv->numVerts);
				//return;
				broken = true;
				continue;
			}

			float* a = (float *)vs[tri[0]].xyz;
			float* b = (float *)vs[tri[1]].xyz;
			float* c = (float *)vs[tri[2]].xyz;
			vec3_t ba, ca;
			VectorSubtract(b, a, ba);
			VectorSubtract(c, a, ca);

			vec3_t normal;
			CrossProduct(ca, ba, normal);
			VectorNormalize(normal, normal);

			//ri->Printf(PRINT_WARNING, "OLD: %f %f %f. NEW: %f %f %f.\n", cv->verts[tri[0]].normal[0], cv->verts[tri[0]].normal[1], cv->verts[tri[0]].normal[2], normal[0], normal[1], normal[2]);

#pragma omp critical (__SET_NORMAL__)
			{
				VectorCopy(normal, vs[tri[0]].normal);
				VectorCopy(normal, vs[tri[1]].normal);
				VectorCopy(normal, vs[tri[2]].normal);
			}

			//ForceCrash();
		}
	}
}

//
//
//

extern float Distance(vec3_t pos1, vec3_t pos2);

#define INVERTED_NORMALS_EPSILON 0.8//1.0

void FixInvertedNormalsForBSP(bspDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader, int dsNum)
{
	if (ds->surfaceType == SURFACE_BAD)
		return;

	shaderInfo_t *shaderInfo1 = bspShaderInfos[dsNum];

	if (!shaderInfo1)
		return;

	if (shaderInfo1 == caulkShader)
		return;

	if (shaderInfo1 == skipShader)
		return;

	if ((shaderInfo1->contentFlags & C_TRANSLUCENT)
		|| (shaderInfo1->contentFlags & C_NODRAW))
		return;

	bspDrawVert_t *vs = &bspDrawVerts[ds->firstVert];
	int *idxs = &bspDrawIndexes[ds->firstIndex];

#pragma omp parallel for ordered num_threads(numthreads)
	for (int i = 0; i < ds->numIndexes; i += 3)
	{
		bool badWinding = false;

		int tri[3];
		tri[0] = idxs[i];
		tri[1] = idxs[i + 1];
		tri[2] = idxs[i + 2];

		if (tri[0] >= ds->numVerts)
		{
			continue;
		}
		if (tri[1] >= ds->numVerts)
		{
			continue;
		}
		if (tri[2] >= ds->numVerts)
		{
			continue;
		}

		float dist1 = Distance(vs[tri[0]].normal, vs[tri[1]].normal);
		float dist2 = Distance(vs[tri[0]].normal, vs[tri[2]].normal);
		float dist3 = Distance(vs[tri[1]].normal, vs[tri[2]].normal);

		if (dist1 > INVERTED_NORMALS_EPSILON || dist2 > INVERTED_NORMALS_EPSILON || dist3 > INVERTED_NORMALS_EPSILON)
		{
			badWinding = true;
		}

		vec3 normAvg(0.0, 0.0, 0.0);

		if (badWinding)
		{
			int count = 0;

			for (int j = 0; j < ds->numVerts; j++)
			{
				if (j == tri[0] || j == tri[1] || j == tri[2]) continue;

				if (Distance(vs[tri[0]].xyz, vs[j].xyz) <= 2048.0)
				{
					normAvg = normAvg + vec3(vs[j].normal) * vec3(0.5) + vec3(0.5);
					count++;
				}
			}

			if (count > 0)
			{
				normAvg /= count;
				normAvg = normAvg * vec3(2.0) - vec3(1.0);

				vec3_t n;
				n[0] = normAvg.x;
				n[1] = normAvg.y;
				n[2] = normAvg.z;

				int best = 0;
				vec3 bestNorm(vs[tri[0]].normal);
				float bestDist = Distance(n, vs[tri[0]].normal);

				for (int z = 1; z < 3; z++)
				{
					float dist = Distance(n, vs[tri[z]].normal);

					if (dist < bestDist)
					{
						best = z;
						bestDist = dist;
						bestNorm.x = vs[tri[z]].normal[0];
						bestNorm.y = vs[tri[z]].normal[1];
						bestNorm.z = vs[tri[z]].normal[2];
					}
				}

				if (bestDist <= INVERTED_NORMALS_EPSILON)
				{
					for (int z = 0; z < 3; z++)
					{
						if (z != best)
						{
							vs[tri[z]].normal[0] = bestNorm.x;
							vs[tri[z]].normal[1] = bestNorm.y;
							vs[tri[z]].normal[2] = bestNorm.z;
						}
					}
				}
				else
				{
					for (int z = 0; z < 3; z++)
					{
						vs[tri[z]].normal[0] = normAvg.x;
						vs[tri[z]].normal[1] = normAvg.y;
						vs[tri[z]].normal[2] = normAvg.z;
					}
				}
			}
		}
	}
}

float MAX_SMOOTH_ERROR = 1.0;

bool ValidForSmoothingBSP(vec3_t v1, vec3_t n1, vec3_t v2, vec3_t n2)
{
	if (SMOOTHING_STRICT)
	{
		if (v1[0] == v2[0]
			&& v1[1] == v2[1]
			&& v1[2] == v2[2]
			&& Distance(n1, n2) < MAX_SMOOTH_ERROR)
		{
			return true;
		}
	}
	else
	{
		vec3_t diff;
		diff[0] = (v1[0] - v2[0]);
		diff[1] = (v1[1] - v2[1]);
		diff[2] = (v1[2] - v2[2]);

		if (Distance(v1, v2) <= 2.0
			&& (Distance(n1, n2) < MAX_SMOOTH_ERROR
				|| (diff[0] < 0.333 && diff[0] > -0.333 && diff[1] < 0.333 && diff[1] > -0.333 && diff[2] < 0.333 && diff[2] > -0.333)))
		{
			return true;
		}
	}

	return false;
}


#define __MULTIPASS_SMOOTHING__ // Looks better, but takes a lot longer...
#define MULTIPASS_PASSES SMOOTHING_PASSES

int GetWorkCountForSurfaceBSP(bspDrawSurface_t *ds, shaderInfo_t *caulkShader, int dsNum)
{
	if (ds->surfaceType == SURFACE_BAD)
		return 0;

	shaderInfo_t *shaderInfo1 = bspShaderInfos[dsNum];

	if (!shaderInfo1)
		return 0;

	if (shaderInfo1 == caulkShader)
		return 0;

	if ((shaderInfo1->contentFlags & C_TRANSLUCENT)
		|| (shaderInfo1->contentFlags & C_NODRAW))
		return 0;

	int count = 0;

	if (SMOOTHING_WHOLEMAP
		|| ((shaderInfo1->materialType == MATERIAL_LONGGRASS
			|| shaderInfo1->materialType == MATERIAL_SHORTGRASS
			|| shaderInfo1->materialType == MATERIAL_SAND
			|| shaderInfo1->materialType == MATERIAL_DIRT
			|| shaderInfo1->materialType == MATERIAL_MUD)
			&& !shaderInfo1->isTreeSolid
			&& !shaderInfo1->isMapObjectSolid))
	{// Whole map terrain smoothing...
#ifdef __MULTIPASS_SMOOTHING__
		count = MULTIPASS_PASSES * ds->numVerts * numBSPDrawSurfaces;
#else
		count = ds->numVerts * numBSPDrawSurfaces;
#endif //__MULTIPASS_SMOOTHING__
	}
	else
	{// Local smoothing only... for speed...
#ifdef __MULTIPASS_SMOOTHING__
		count = MULTIPASS_PASSES * ds->numVerts;
#else
		count = ds->numVerts;
#endif //__MULTIPASS_SMOOTHING__
	}

	return count;
}

void GenerateSmoothNormalsForMeshBSP(bspDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader, int dsNum)
{
	if (ds->surfaceType == SURFACE_BAD)
		return;

	shaderInfo_t *shaderInfo1 = bspShaderInfos[dsNum];

	if (!shaderInfo1)
		return;

	if (shaderInfo1 == caulkShader)
		return;

	if (shaderInfo1 == skipShader)
		return;

	if ((shaderInfo1->contentFlags & C_TRANSLUCENT)
		|| (shaderInfo1->contentFlags & C_NODRAW))
		return;

	if (SMOOTHING_WHOLEMAP
		|| ((shaderInfo1->materialType == MATERIAL_LONGGRASS
				|| shaderInfo1->materialType == MATERIAL_SHORTGRASS
				|| shaderInfo1->materialType == MATERIAL_SAND
				|| shaderInfo1->materialType == MATERIAL_DIRT
				|| shaderInfo1->materialType == MATERIAL_MUD)
			&& !shaderInfo1->isTreeSolid
			&& !shaderInfo1->isMapObjectSolid))
	{// Whole map terrain smoothing...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < MULTIPASS_PASSES; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
#pragma omp parallel for ordered num_threads(numthreads)
			for (int i = 0; i < ds->numVerts; i++)
			{
				vec3_t accumNormal, finalNormal;
				VectorCopy(bspDrawVerts[ds->firstVert+i].normal, accumNormal);

				for (int s = 0; s < numBSPDrawSurfaces; s++)
				{
					bspDrawSurface_t *ds2 = &bspDrawSurfaces[s];
					
					if (ds2->surfaceType == SURFACE_BAD)
						continue;

					shaderInfo_t *shaderInfo2 = bspShaderInfos[s];

					if (!shaderInfo2 || shaderInfo1 != shaderInfo2 || shaderInfo2 == caulkShader || shaderInfo2 == skipShader)
						continue;

					if ((shaderInfo2->contentFlags & C_TRANSLUCENT)
						|| (shaderInfo2->contentFlags & C_NODRAW))
						continue;

					for (int i2 = 0; i2 < ds2->numVerts; i2++)
					{
						if (ds2 == ds && ds2->firstVert + i2 == ds->firstVert + i) continue;

						if (ValidForSmoothingBSP(bspDrawVerts[ds->firstVert + i].xyz, bspDrawVerts[ds->firstVert + i].normal, bspDrawVerts[ds2->firstVert + i2].xyz, bspDrawVerts[ds2->firstVert + i2].normal))
						{
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
#ifndef __MULTIPASS_SMOOTHING__
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
#endif //__MULTIPASS_SMOOTHING__
						}
					}
				}

				VectorNormalize(accumNormal, finalNormal);

#pragma omp critical (__SET_SMOOTH_NORMAL__)
				{
					VectorCopy(finalNormal, bspDrawVerts[ds->firstVert + i].normal);
					//printf("Full optimized %s (%i of %i).\n", shaderInfo1->shader, i, ds->numVerts);
				}
			}
		}
	}
	else
	{// Local smoothing only... for speed...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < MULTIPASS_PASSES; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
#pragma omp parallel for ordered num_threads(numthreads)
			for (int i = 0; i < ds->numVerts; i++)
			{
				vec3_t accumNormal, finalNormal;
				VectorCopy(bspDrawVerts[ds->firstVert + i].normal, accumNormal);

				//for (int s = 0; s < numBSPDrawSurfaces; s++)
				{
					//bspDrawSurface_t *ds2 = &bspDrawSurfaces[s];
					bspDrawSurface_t *ds2 = ds;

					if (ds2->surfaceType == SURFACE_BAD)
						continue;
					
					shaderInfo_t *shaderInfo2 = shaderInfo1;

					if (!shaderInfo2 || shaderInfo1 != shaderInfo2 || shaderInfo2 == caulkShader || shaderInfo2 == skipShader)
						continue;

					if ((shaderInfo2->contentFlags & C_TRANSLUCENT)
						|| (shaderInfo2->contentFlags & C_NODRAW))
						continue;

					for (int i2 = 0; i2 < ds2->numVerts; i2++)
					{
						if (ds2 == ds && ds2->firstVert + i2 == ds->firstVert + i) continue;

						if (ValidForSmoothingBSP(bspDrawVerts[ds->firstVert + i].xyz, bspDrawVerts[ds->firstVert + i].normal, bspDrawVerts[ds2->firstVert + i2].xyz, bspDrawVerts[ds2->firstVert + i2].normal))
						{
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
#ifndef __MULTIPASS_SMOOTHING__
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
							VectorAdd(accumNormal, bspDrawVerts[ds2->firstVert + i2].normal, accumNormal);
#endif //__MULTIPASS_SMOOTHING__
						}
					}
				}

				VectorNormalize(accumNormal, finalNormal);

#pragma omp critical (__SET_SMOOTH_NORMAL__)
				{
					VectorCopy(finalNormal, bspDrawVerts[ds->firstVert + i].normal);
					//printf("Fast optimized %s (%i of %i).\n", shaderInfo1->shader, i, ds->numVerts);
				}
			}
		}
	}
}

extern int MAP_SMOOTH_NORMALS;

void GenerateSmoothNormalsBSP(void)
{
	int numCompleted = 0;

	Sys_PrintHeading("--- GenerateNormals ---\n");

	shaderInfo_t *caulkShader = ShaderInfoForShader("textures/system/caulk");
	shaderInfo_t *skipShader = ShaderInfoForShader("textures/system/skip");

	for (int s = 0; s < numBSPDrawSurfaces; s++)
	{
		{
			printLabelledProgress("SetShaderInfos", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *ds = &bspDrawSurfaces[s];
		shaderInfo_t *shaderInfo1 = ShaderInfoForShader(bspShaders[ds->shaderNum].shader);

		bspShaderInfos[s] = shaderInfo1;
	}

	numCompleted = 0;

	for (int s = 0; s < numBSPDrawSurfaces; s++)
	{
		{
			printLabelledProgress("GenerateNormals", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *ds = &bspDrawSurfaces[s];
		shaderInfo_t *shaderInfo1 = bspShaderInfos[s];

		if (shaderInfo1 && shaderInfo1->noSmooth) continue;

		GenerateNormalsForMeshBSP(ds, caulkShader, skipShader, s);
	}

	numCompleted = 0;

	for (int s = 0; s < numBSPDrawSurfaces; s++)
	{
		{
			printLabelledProgress("FixInvertedNormals", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *ds = &bspDrawSurfaces[s];
		shaderInfo_t *shaderInfo1 = bspShaderInfos[s];

		if (shaderInfo1 && shaderInfo1->noSmooth) continue;

		FixInvertedNormalsForBSP(ds, caulkShader, skipShader, s);
	}

	MAP_SMOOTH_NORMALS = 1;

	if (MAP_SMOOTH_NORMALS)
	{
		numCompleted = 0;

		for (int s = 0; s < numBSPDrawSurfaces; s++)
		{
			{
				printLabelledProgress("SmoothNormals", numCompleted, numBSPDrawSurfaces);
				numCompleted++;
			}

			bspDrawSurface_t *ds = &bspDrawSurfaces[s];
			shaderInfo_t *shaderInfo1 = bspShaderInfos[s];

			if (shaderInfo1 && shaderInfo1->noSmooth) continue;

			GenerateSmoothNormalsForMeshBSP(ds, caulkShader, skipShader, s);
		}
	}
}

//
//
//

int SmoothBSPMain(int argc, char **argv)
{
	int i = 1;

	/* arg checking */
	if (argc < 1)
	{
		Sys_Printf("Usage: wzmap -smooth <mapname>\n");
		return 0;
	}

	/* process arguments */
	Sys_PrintHeading("--- CommandLine ---\n");
	strcpy(externalLightmapsPath, "");

	/* process arguments */
	for (i = 1; i < (argc - 1); i++)
	{
		if (!Q_stricmp(argv[i], "-wholemap")) 
		{
			SMOOTHING_WHOLEMAP = qtrue;
			MAP_SMOOTH_NORMALS = 2;
		}
		else if (!Q_stricmp(argv[i], "-strict"))
		{
			SMOOTHING_STRICT = qtrue;
		}
		else if (!Q_stricmp(argv[i], "-passes")) {
			i++;
			if (i<(argc - 1)) {
				int temp = atoi(argv[i]);
				if (temp > 0) 
				{
					SMOOTHING_PASSES = temp;
				}
			}
		}
		else if (!Q_stricmp(argv[i], "-smoothError"))
		{
			i++;
			if (i<(argc - 1)) {
				float temp = atof(argv[i]);
				if (temp > 0)
				{
					MAX_SMOOTH_ERROR = temp;
				}
			}
		}
		else 
		{
			Sys_Printf("WARNING: Unknown option \"%s\"\n", argv[i]);
		}
	}

	/* clean up map name */
	strcpy(source, ExpandArg(argv[i]));
	StripExtension(source);

	DefaultExtension(source, ".bsp");

	/* load shaders */
	LoadShaderInfo();

	/* load map */
	Sys_PrintHeading("--- LoadBSPFile ---\n");
	Sys_Printf("loading %s\n", source);

	/* load surface file */
	LoadBSPFile(source);

	/* smooth normals */
	GenerateSmoothNormalsBSP();

	/* write back */
	Sys_PrintHeading("--- WriteBSPFile ---\n");
	Sys_Printf("Writing %s\n", source);
	WriteBSPFile(source);

	/* return to sender */
	return 0;
}
