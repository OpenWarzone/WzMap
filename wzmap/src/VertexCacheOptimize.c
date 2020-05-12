#include <vector>
#include "q3map2.h"
#include "meshOptimization/triListOpt.h"

//#define __USE_Q3MAP2_METHOD__
#define __REGENERATE_BSP_NORMALS__

extern qboolean MAP_REGENERATE_NORMALS;
extern int MAP_SMOOTH_NORMALS;

int64_t normalsWorkCountTotal = 0;
int64_t normalsWorkCountDone = 0;

//
// Indexes optimization...
//
unsigned const VBUF_SZ = 32;// 1024;// 32;

typedef struct
{
	unsigned ix;
	unsigned pos;
} vbuf_entry_t;

vbuf_entry_t vbuf[VBUF_SZ];

float calc_acmr(uint32_t numIndexes, uint32_t *indexes) 
{
	vbuf_entry_t vbuf[VBUF_SZ];
	unsigned num_cm(0); // cache misses

	for (unsigned i = 0; i < numIndexes; ++i) {
		bool found(0);
		unsigned best_entry(0), oldest_pos((unsigned)-1);

		for (unsigned n = 0; n < VBUF_SZ; ++n) {
			if (vbuf[n].ix == indexes[i]) { found = 1; break; }
			if (vbuf[n].pos < oldest_pos) { best_entry = n; oldest_pos = vbuf[n].pos; }
		}
		if (found) continue;
		vbuf[best_entry].ix = indexes[i];
		vbuf[best_entry].pos = i;
		++num_cm;
	}
	return float(num_cm) / float(numIndexes);
}

#ifndef __USE_Q3MAP2_METHOD__
void VertexCacheOptimizeMeshIndexes(uint32_t numVerts, uint32_t numIndexes, uint32_t *indexes, float *outStats)
{
	if (numIndexes <= 0)
	{
		outStats[0] = 0;
		outStats[1] = 0;
		outStats[2] = 0;
		outStats[3] = 0;
		return;
	}

	//if (numIndexes < 1.5*numVerts || numVerts < 2 * VBUF_SZ /*|| num_verts < 100000*/) return;
	float const orig_acmr(calc_acmr(numIndexes, indexes)), perfect_acmr(float(numVerts) / float(numIndexes));
	//if (orig_acmr < 1.05*perfect_acmr) return;

	std::vector<unsigned> out_indices(numIndexes);
	TriListOpt::OptimizeTriangleOrdering(numVerts, numIndexes, indexes, &out_indices.front());

	for (int i = 0; i < out_indices.size(); i++)
	{
		indexes[i] = out_indices[i];
	}

	float optimized_acmr = calc_acmr(numIndexes, indexes);
	//float optPercent = (optimized_acmr / orig_acmr) * 100.0;
	float originalPercent = orig_acmr / perfect_acmr;
	float optimizedPercent = optimized_acmr / perfect_acmr;
	float optPercent = (originalPercent / optimizedPercent) * 100.0;
	
	if (optPercent == 0.0 || optPercent == NAN)
	{
		optPercent = 100.0;
	}

	//ri->Printf(PRINT_WARNING, "R_VertexCacheOptimizeMeshIndexes: Original acmr: %f. Optimized acmr: %f. Perfect acmr: %f. Optimization Ratio: %f. Time: %i ms.\n", orig_acmr, optimized_acmr, perfect_acmr, optPercent);
	outStats[0] = orig_acmr;
	outStats[1] = optimized_acmr;
	outStats[2] = perfect_acmr;
	outStats[3] = optPercent;
}
#else //__USE_Q3MAP2_METHOD__
/*
OptimizeTriangleSurface() - ydnar
optimizes the vertex/index data in a triangle surface
*/

std::string StripPath(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	return (std::string::npos == pos) ? "" : path.substr(pos + 1, std::string::npos);
}

#define VERTEX_CACHE_SIZE 1024 /* vortex: was 16 */

static void OptimizeTriangleSurfaceYdnar(mapDrawSurface_t *ds, float *outStats)
{
	if (ds->numIndexes <= VERTEX_CACHE_SIZE || ds->shaderInfo->autosprite)
	{
		outStats[0] = 0;
		outStats[1] = 0;
		outStats[2] = 0;
		outStats[3] = 0;
		return;
	}

	int		vertexCache[VERTEX_CACHE_SIZE + 1];	/* one more for optimizing insert */
	int		*indexes;

	float const orig_acmr(calc_acmr(ds->numIndexes, (uint32_t *)ds->indexes)), perfect_acmr(float(ds->numVerts) / float(ds->numIndexes));

	/* create index scratch pad */
	indexes = (int *)safe_malloc(ds->numIndexes * sizeof(*indexes));
	memcpy(indexes, ds->indexes, ds->numIndexes * sizeof(*indexes));

	/* setup */
	for (int i = 0; i <= VERTEX_CACHE_SIZE && i < ds->numIndexes; i++)
		vertexCache[i] = indexes[i];

	int numCompleted = 0;
	char currentName[128] = { 0 };
	
	strcpy(currentName, (ds->shaderInfo != NULL) ? StripPath(ds->shaderInfo->shader).c_str() : "noshader");

	/* add triangles in a vertex cache-aware order */
//#pragma omp parallel for ordered num_threads(numthreads)
	for (int i = 0; i < ds->numIndexes; i += 3)
	{
		/* find best triangle given the current vertex cache */
		int first = -1;
		int best = -1;
		int bestScore = -1;
		int score = 0;

#pragma omp critical (__PROGRESS_BAR__)
		{
			printLabelledProgress(currentName, numCompleted, ds->numIndexes);
		}

		numCompleted+=3;

		for (int j = 0; j < ds->numIndexes; j += 3)
		{
			/* valid triangle? */
			if (indexes[j] != -1)
			{
				/* set first if necessary */
				if (first < 0)
					first = j;

				/* score the triangle */
				score = 0;
				for (int k = 0; k < VERTEX_CACHE_SIZE; k++)
				{
					if (indexes[j] == vertexCache[k] || indexes[j + 1] == vertexCache[k] || indexes[j + 2] == vertexCache[k])
						score++;
				}

				/* better triangle? */
				if (score > bestScore)
				{
					bestScore = score;
					best = j;
				}

				/* a perfect score of 3 means this triangle's verts are already present in the vertex cache */
				if (score == 3)
					break;
			}
		}

		/* check if no decent triangle was found, and use first available */
		if (best < 0)
			best = first;

		/* valid triangle? */
		if (best >= 0)
		{
			/* add triangle to vertex cache */
			for (int j = 0; j < 3; j++)
			{
				int k = 0;

				for (k = 0; k < VERTEX_CACHE_SIZE; k++)
				{
					if (indexes[best + j] == vertexCache[k])
						break;
				}

				if (k >= VERTEX_CACHE_SIZE)
				{
					/* pop off top of vertex cache */
					for (k = VERTEX_CACHE_SIZE; k > 0; k--)
						vertexCache[k] = vertexCache[k - 1];

					/* add vertex */
					vertexCache[0] = indexes[best + j];
				}
			}

			/* add triangle to surface */
			ds->indexes[i] = indexes[best];
			ds->indexes[i + 1] = indexes[best + 1];
			ds->indexes[i + 2] = indexes[best + 2];

			/* clear from input pool */
			indexes[best] = -1;
			indexes[best + 1] = -1;
			indexes[best + 2] = -1;

			/* sort triangle windings (312 -> 123) */
			while (ds->indexes[i] > ds->indexes[i + 1] || ds->indexes[i] > ds->indexes[i + 2])
			{
				int temp = ds->indexes[i];
				ds->indexes[i] = ds->indexes[i + 1];
				ds->indexes[i + 1] = ds->indexes[i + 2];
				ds->indexes[i + 2] = temp;
			}
		}
	}

	/* clean up */
	free(indexes);

	printLabelledProgress(currentName, ds->numIndexes, ds->numIndexes);

	float optimized_acmr = calc_acmr(ds->numIndexes, (uint32_t *)ds->indexes);
	float originalPercent = orig_acmr / perfect_acmr;
	float optimizedPercent = optimized_acmr / perfect_acmr;
	float optPercent = (originalPercent / optimizedPercent) * 100.0;

	if (optPercent == 0.0 || optPercent == NAN)
	{
		optPercent = 100.0;
	}

	//ri->Printf(PRINT_WARNING, "R_VertexCacheOptimizeMeshIndexes: Original acmr: %f. Optimized acmr: %f. Perfect acmr: %f. Optimization Ratio: %f. Time: %i ms.\n", orig_acmr, optimized_acmr, perfect_acmr, optPercent);
	outStats[0] = orig_acmr;
	outStats[1] = optimized_acmr;
	outStats[2] = perfect_acmr;
	outStats[3] = optPercent;
}
#endif //__USE_Q3MAP2_METHOD__

void OptimizeDrawSurfs( void )
{
#define MIN_VCO_INDEXES 16

	int numCompleted = 0;
	std::vector<vec4_t> out_stats(numMapDrawSurfs);

	Sys_PrintHeading("--- OptimizeDrawSurfs ---\n");

	int numOptimizable = 0;

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->numIndexes > MIN_VCO_INDEXES)
		{
#ifdef __USE_Q3MAP2_METHOD__
			if (ds->numIndexes > MIN_VCO_INDEXES)
			{
				Sys_FPrintf(SYS_STD, "          ent#%i %s - %i tris\n", ds->entityNum, (ds->shaderInfo != NULL) ? ds->shaderInfo->shader : "noshader", ds->numIndexes / 3);
			}
#endif //__USE_Q3MAP2_METHOD__
			numOptimizable++;
		}
	}

	Sys_Printf("%9d total map draw surfs.\n", numMapDrawSurfs);
	Sys_Printf("%9d optimizable map draw surfs.\n", numOptimizable);

#ifndef __USE_Q3MAP2_METHOD__
#pragma omp parallel for ordered num_threads(numthreads)
#endif //__USE_Q3MAP2_METHOD__
	for (int s = 0; s < numMapDrawSurfs; s++)
	{
#ifndef __USE_Q3MAP2_METHOD__
#pragma omp critical (__PROGRESS_BAR__)
		{
			printLabelledProgress("OptimizeDrawSurfs", numCompleted, numMapDrawSurfs);
		}

		numCompleted++;
#endif //__USE_Q3MAP2_METHOD__

		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		
		if (ds->numIndexes > MIN_VCO_INDEXES)
		{
#ifndef __USE_Q3MAP2_METHOD__
			VertexCacheOptimizeMeshIndexes(ds->numVerts, ds->numIndexes, (uint32_t *)ds->indexes, out_stats[s]);
#else //__USE_Q3MAP2_METHOD__
			OptimizeTriangleSurfaceYdnar(ds, out_stats[s]);
#endif //__USE_Q3MAP2_METHOD__
		}
	}

	printLabelledProgress("OptimizeDrawSurfs", numMapDrawSurfs, numMapDrawSurfs);

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		
		if (ds->numIndexes > MIN_VCO_INDEXES)
		{
			Sys_FPrintf(SYS_STD, "          ent#%i %s - %i tris - Original ACMR: %f. Optimized ACMR: %f. Perfect ACMR: %f. Optimization: %f%.\n", ds->entityNum, (ds->shaderInfo != NULL) ? ds->shaderInfo->shader : "noshader", ds->numIndexes / 3, out_stats[s][0], out_stats[s][1], out_stats[s][2], out_stats[s][3]);
		}
	}
}


//
//
//

extern float Distance(vec3_t pos1, vec3_t pos2);

#ifdef __REGENERATE_BSP_NORMALS__
#define INVERTED_NORMALS_EPSILON 0.8//1.0

void FixInvertedNormalsForSurfaceWorkCount(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	if (!ds->shaderInfo || ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

	for (int i = 0; i < ds->numIndexes; i += 3)
	{
		normalsWorkCountTotal++;
	}
}

void FixInvertedNormalsForSurface(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	if (!ds->shaderInfo || ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

#pragma omp parallel for ordered num_threads(numthreads)
	for (int i = 0; i < ds->numIndexes; i += 3)
	{
#pragma omp critical (__PROGRESS_BAR__)
		{
			printLabelledProgress("FixInvertedNormals", normalsWorkCountDone / 32768, normalsWorkCountTotal / 32768);
		}
		normalsWorkCountDone++;

		bool badWinding = false;

		int tri[3];
		tri[0] = ds->indexes[i];
		tri[1] = ds->indexes[i + 1];
		tri[2] = ds->indexes[i + 2];

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

		float dist1 = Distance(ds->verts[tri[0]].normal, ds->verts[tri[1]].normal);
		float dist2 = Distance(ds->verts[tri[0]].normal, ds->verts[tri[2]].normal);
		float dist3 = Distance(ds->verts[tri[1]].normal, ds->verts[tri[2]].normal);

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

				if (Distance(ds->verts[tri[0]].xyz, ds->verts[j].xyz) <= 2048.0)
				{
					normAvg = normAvg + vec3(ds->verts[j].normal) * vec3(0.5) + vec3(0.5);
					count++;
				}
			}

			if (count > 0)
			{
				normAvg /= count;
				normAvg = normAvg * vec3(2.0) - vec3(1.0);

#if 1
				vec3_t n;
				n[0] = normAvg.x;
				n[1] = normAvg.y;
				n[2] = normAvg.z;

				int best = 0;
				vec3 bestNorm(ds->verts[tri[0]].normal);
				float bestDist = Distance(n, ds->verts[tri[0]].normal);

				for (int z = 1; z < 3; z++)
				{
					float dist = Distance(n, ds->verts[tri[z]].normal);

					if (dist < bestDist)
					{
						best = z;
						bestDist = dist;
						bestNorm.x = ds->verts[tri[z]].normal[0];
						bestNorm.y = ds->verts[tri[z]].normal[1];
						bestNorm.z = ds->verts[tri[z]].normal[2];
					}
				}

				if (bestDist <= INVERTED_NORMALS_EPSILON)
				{
					for (int z = 0; z < 3; z++)
					{
						if (z != best)
						{
							ds->verts[tri[z]].normal[0] = bestNorm.x;
							ds->verts[tri[z]].normal[1] = bestNorm.y;
							ds->verts[tri[z]].normal[2] = bestNorm.z;
						}
					}
				}
				else
				{
					for (int z = 0; z < 3; z++)
					{
						ds->verts[tri[z]].normal[0] = normAvg.x;
						ds->verts[tri[z]].normal[1] = normAvg.y;
						ds->verts[tri[z]].normal[2] = normAvg.z;
					}
				}
#else
				for (int z = 0; z < 3; z++)
				{
					cv->verts[tri[z]].normal[0] = normAvg.x;
					cv->verts[tri[z]].normal[1] = normAvg.y;
					cv->verts[tri[z]].normal[2] = normAvg.z;
				}
#endif
			}
		}
	}
}

void GenerateNormalsForMeshWorkCount(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	qboolean smoothOnly = qfalse;

	if (ds->shaderInfo && ds->shaderInfo->smoothOnly)
		smoothOnly = qtrue;

	if (!ds->shaderInfo || ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

	if (!smoothOnly)
	{
		bool broken = false;

		for (int i = 0; i < ds->numIndexes; i += 3)
		{
			normalsWorkCountTotal++;
		}
	}
}

void GenerateNormalsForMesh(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	qboolean smoothOnly = qfalse;

	if (ds->shaderInfo && ds->shaderInfo->smoothOnly)
		smoothOnly = qtrue;

	if (!ds->shaderInfo || ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

	if (!smoothOnly)
	{
		bool broken = false;

#pragma omp parallel for ordered num_threads(numthreads)
		for (int i = 0; i < ds->numIndexes; i += 3)
		{
#pragma omp critical (__PROGRESS_BAR__)
			{
				printLabelledProgress("GenerateNormals", normalsWorkCountDone / 32768, normalsWorkCountTotal / 32768);
			}
			normalsWorkCountDone++;

			if (broken) continue;

			int tri[3];
			tri[0] = ds->indexes[i];
			tri[1] = ds->indexes[i + 1];
			tri[2] = ds->indexes[i + 2];

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

			float* a = (float *)ds->verts[tri[0]].xyz;
			float* b = (float *)ds->verts[tri[1]].xyz;
			float* c = (float *)ds->verts[tri[2]].xyz;
			vec3_t ba, ca;
			VectorSubtract(b, a, ba);
			VectorSubtract(c, a, ca);

			vec3_t normal;
			CrossProduct(ca, ba, normal);
			VectorNormalize(normal, normal);

			//ri->Printf(PRINT_WARNING, "OLD: %f %f %f. NEW: %f %f %f.\n", cv->verts[tri[0]].normal[0], cv->verts[tri[0]].normal[1], cv->verts[tri[0]].normal[2], normal[0], normal[1], normal[2]);

#pragma omp critical (__SET_NORMAL__)
			{
				VectorCopy(normal, ds->verts[tri[0]].normal);
				VectorCopy(normal, ds->verts[tri[1]].normal);
				VectorCopy(normal, ds->verts[tri[2]].normal);
			}

			//ForceCrash();
		}
	}
}

#define MAX_SMOOTH_ERROR 1.0

bool ValidForSmoothing(vec3_t v1, vec3_t n1, vec3_t v2, vec3_t n2)
{
	if (v1[0] == v2[0] 
		&& v1[1] == v2[1]
		&& v1[2] == v2[2]
		&& Distance(n1, n2) < MAX_SMOOTH_ERROR)
	{
		return true;
	}

	return false;
}

#define __MULTIPASS_SMOOTHING__ // Looks better, but takes a lot longer...

void  GenerateSmoothNormalsForMeshWorkCount(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	if (!ds->shaderInfo)
		return;

	if (ds->type == SURFACE_BAD)
		return;

	if (ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

	if ((ds->shaderInfo->contentFlags & C_TRANSLUCENT)
		|| (ds->shaderInfo->contentFlags & C_NODRAW))
		return;

	int count = 0;

	if ((ds->shaderInfo->materialType == MATERIAL_LONGGRASS
		|| ds->shaderInfo->materialType == MATERIAL_SHORTGRASS
		|| ds->shaderInfo->materialType == MATERIAL_SAND
		|| ds->shaderInfo->materialType == MATERIAL_DIRT)
		&& !ds->shaderInfo->isTreeSolid
		&& !ds->shaderInfo->isMapObjectSolid)
	{// Whole map smoothing...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < 3; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
			for (int i = 0; i < ds->numVerts; i++)
			{
				for (int s = 0; s < numMapDrawSurfs; s++)
				{
					normalsWorkCountTotal++;
				}
			}
		}
	}
	else
	{// Local smoothing only... for speed...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < 3; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
			for (int i = 0; i < ds->numVerts; i++)
			{
				normalsWorkCountTotal++;
			}
		}
	}
}


void GenerateSmoothNormalsForMesh(mapDrawSurface_t *ds, shaderInfo_t *caulkShader, shaderInfo_t *skipShader)
{
	if (!ds->shaderInfo)
		return;

	if (ds->type == SURFACE_BAD)
		return;

	if (ds->shaderInfo == caulkShader || ds->shaderInfo == skipShader)
		return;

	if ((ds->shaderInfo->contentFlags & C_TRANSLUCENT)
		|| (ds->shaderInfo->contentFlags & C_NODRAW))
		return;

	if ((ds->shaderInfo->materialType == MATERIAL_LONGGRASS 
		|| ds->shaderInfo->materialType == MATERIAL_SHORTGRASS 
		|| ds->shaderInfo->materialType == MATERIAL_SAND
		|| ds->shaderInfo->materialType == MATERIAL_DIRT)
		&& !ds->shaderInfo->isTreeSolid
		&& !ds->shaderInfo->isMapObjectSolid
		&& MAP_SMOOTH_NORMALS > 1)
	{// Whole map smoothing...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < 3; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
#pragma omp parallel for ordered num_threads(numthreads)
			for (int i = 0; i < ds->numVerts; i++)
			{
				vec3_t accumNormal, finalNormal;
				VectorCopy(ds->verts[i].normal, accumNormal);

				for (int s = 0; s < numMapDrawSurfs; s++)
				{
#pragma omp critical (__PROGRESS_BAR__)
					{
						printLabelledProgress("SmoothNormals", normalsWorkCountDone / 32768, normalsWorkCountTotal / 32768); // / 32768 because of huge numbers and conversion to double...
					}
					normalsWorkCountDone++;

					mapDrawSurface_t *ds2 = &mapDrawSurfs[s];
					//mapDrawSurface_t *ds2 = ds;

					if (ds2->type == SURFACE_BAD)
						continue;

					if (!ds2->shaderInfo || ds->shaderInfo != ds2->shaderInfo || ds2->shaderInfo == caulkShader || ds2->shaderInfo == skipShader)
						continue;

					if ((ds2->shaderInfo->contentFlags & C_TRANSLUCENT)
						|| (ds2->shaderInfo->contentFlags & C_NODRAW))
						continue;

					for (int i2 = 0; i2 < ds2->numVerts; i2++)
					{
						if (ds2 == ds && i2 == i) continue;

						if (ValidForSmoothing(ds->verts[i].xyz, ds->verts[i].normal, ds2->verts[i2].xyz, ds2->verts[i2].normal))
						{
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
#ifndef __MULTIPASS_SMOOTHING__
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
#endif //__MULTIPASS_SMOOTHING__
						}
					}
				}

				VectorNormalize(accumNormal, finalNormal);

#pragma omp critical (__SET_SMOOTH_NORMAL__)
				{
					VectorCopy(finalNormal, ds->verts[i].normal);
				}
			}
		}
	}
	else
	{// Local smoothing only... for speed...
#ifdef __MULTIPASS_SMOOTHING__
		for (int pass = 0; pass < 3; pass++)
#endif //__MULTIPASS_SMOOTHING__
		{
#pragma omp parallel for ordered num_threads(numthreads)
			for (int i = 0; i < ds->numVerts; i++)
			{
#pragma omp critical (__PROGRESS_BAR__)
				{
					printLabelledProgress("SmoothNormals", normalsWorkCountDone / 32768, normalsWorkCountTotal / 32768); // / 32768 because of huge numbers and conversion to double...
				}
				normalsWorkCountDone++;

				vec3_t accumNormal, finalNormal;
				VectorCopy(ds->verts[i].normal, accumNormal);

				//for (int s = 0; s < numMapDrawSurfs; s++)
				{
					//mapDrawSurface_t *ds2 = &mapDrawSurfs[s];
					mapDrawSurface_t *ds2 = ds;

					if (ds2->type == SURFACE_BAD)
						continue;

					if (!ds2->shaderInfo || ds->shaderInfo != ds2->shaderInfo || ds2->shaderInfo == caulkShader || ds2->shaderInfo == skipShader)
						continue;

					if ((ds2->shaderInfo->contentFlags & C_TRANSLUCENT)
						|| (ds2->shaderInfo->contentFlags & C_NODRAW))
						continue;

					for (int i2 = 0; i2 < ds2->numVerts; i2++)
					{
						if (ds2 == ds && i2 == i) continue;

						if (ValidForSmoothing(ds->verts[i].xyz, ds->verts[i].normal, ds2->verts[i2].xyz, ds2->verts[i2].normal))
						{
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
#ifndef __MULTIPASS_SMOOTHING__
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
							VectorAdd(accumNormal, ds2->verts[i2].normal, accumNormal);
#endif //__MULTIPASS_SMOOTHING__
						}
					}
				}

				VectorNormalize(accumNormal, finalNormal);

#pragma omp critical (__SET_SMOOTH_NORMAL__)
				{
					VectorCopy(finalNormal, ds->verts[i].normal);
				}
			}
		}
	}
}

void GenerateSmoothNormals(void)
{
	int numCompleted = 0;

	if (!MAP_REGENERATE_NORMALS)
	{// Disabled...
		return;
	}

	Sys_PrintHeading("--- GenerateNormals ---\n");

	shaderInfo_t *caulkShader = ShaderInfoForShader("textures/system/caulk");
	shaderInfo_t *skipShader = ShaderInfoForShader("textures/system/skip");

	Sys_Printf("NOTE: These progress bars can update in chunks and may be slow to update. Be patient, it is still working!");

	// Get work count...
	normalsWorkCountTotal = 0;
	normalsWorkCountDone = 0;

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

		GenerateNormalsForMeshWorkCount(ds, caulkShader, skipShader);
	}

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

		GenerateNormalsForMesh(ds, caulkShader, skipShader);
	}

	printLabelledProgress("GenerateNormals", normalsWorkCountTotal / 32768, normalsWorkCountTotal / 32768);

	// Get work count...
	normalsWorkCountTotal = 0;
	normalsWorkCountDone = 0;

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

		FixInvertedNormalsForSurfaceWorkCount(ds, caulkShader, skipShader);
	}

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

		FixInvertedNormalsForSurface(ds, caulkShader, skipShader);
	}

	printLabelledProgress("FixInvertedNormals", normalsWorkCountTotal / 32768, normalsWorkCountTotal / 32768);

	if (MAP_SMOOTH_NORMALS)
	{
		// Get work count...
		normalsWorkCountTotal = 0;
		normalsWorkCountDone = 0;

		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			mapDrawSurface_t *ds = &mapDrawSurfs[s];

			if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

			GenerateSmoothNormalsForMeshWorkCount(ds, caulkShader, skipShader);
		}

		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			mapDrawSurface_t *ds = &mapDrawSurfs[s];

			if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

			GenerateSmoothNormalsForMesh(ds, caulkShader, skipShader);
		}

		printLabelledProgress("SmoothNormals", normalsWorkCountTotal / 32768, normalsWorkCountTotal / 32768); // / 32768 because of huge numbers and conversion to double...
	}

	normalsWorkCountTotal = 0;
	normalsWorkCountDone = 0;
}
#else //!__REGENERATE_BSP_NORMALS__
void GenerateSmoothNormals(void)
{
}
#endif //__REGENERATE_BSP_NORMALS__
