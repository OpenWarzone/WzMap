#include <vector>
#include "q3map2.h"
#include "meshOptimization/triListOpt.h"

//#define __USE_Q3MAP2_METHOD__
#define __REGENERATE_BSP_NORMALS__

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
void GenerateSmoothNormalsForMesh(mapDrawSurface_t *ds)
{
	qboolean smoothOnly = qfalse;

	if (ds->shaderInfo && ds->shaderInfo->smoothOnly) 
		smoothOnly = qtrue;

	if (!smoothOnly)
	{
		for (int i = 0; i < ds->numIndexes; i += 3)
		{
			int tri[3];
			tri[0] = ds->indexes[i];
			tri[1] = ds->indexes[i + 1];
			tri[2] = ds->indexes[i + 2];

			if (tri[0] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[0], cv->numVerts);
				return;
			}
			if (tri[1] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[1], cv->numVerts);
				return;
			}
			if (tri[2] >= ds->numVerts)
			{
				//ri->Printf(PRINT_WARNING, "Shitty BSP index data - Bad index in face surface (vert) %i >=  (maxVerts) %i.", tri[2], cv->numVerts);
				return;
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

	//#pragma omp critical
			{
				VectorCopy(normal, ds->verts[tri[0]].normal);
				VectorCopy(normal, ds->verts[tri[1]].normal);
				VectorCopy(normal, ds->verts[tri[2]].normal);
			}

			//ForceCrash();
		}
	}

	// Now the hard part, make smooth normals...
//#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < ds->numVerts; i++)
	{
		int verticesFound[65536];
		int numVerticesFound = 0;

		// Get all vertices that share this one ...
		for (int v = 0; v < ds->numIndexes; v += 3)
		{
			int tri[3];
			tri[0] = ds->indexes[v];
			tri[1] = ds->indexes[v + 1];
			tri[2] = ds->indexes[v + 2];

			if (tri[0] == i && Distance(ds->verts[i].normal, ds->verts[tri[0]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}

			if (tri[1] == i && Distance(ds->verts[i].normal, ds->verts[tri[1]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}

			if (tri[2] == i && Distance(ds->verts[i].normal, ds->verts[tri[2]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}

			// Also merge close normals...
			if (tri[0] != i
				&& Distance(ds->verts[i].xyz, ds->verts[tri[0]].xyz) <= 4.0
				&& Distance(ds->verts[i].normal, ds->verts[tri[0]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}

			if (tri[1] != i
				&& Distance(ds->verts[i].xyz, ds->verts[tri[1]].xyz) <= 4.0
				&& Distance(ds->verts[i].normal, ds->verts[tri[1]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}

			if (tri[2] != i
				&& Distance(ds->verts[i].xyz, ds->verts[tri[2]].xyz) <= 4.0
				&& Distance(ds->verts[i].normal, ds->verts[tri[2]].normal) <= 1.0)
			{
//#pragma omp critical
				{
					verticesFound[numVerticesFound] = tri[0];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[1];
					numVerticesFound++;
					verticesFound[numVerticesFound] = tri[2];
					numVerticesFound++;
				}
				continue;
			}
		}

		vec3_t pcNor;
		VectorSet(pcNor, ds->verts[i].normal[0], ds->verts[i].normal[1], ds->verts[i].normal[2]);
		int numAdded = 1;
		for (unsigned int a = 0; a < numVerticesFound; ++a) {
			vec3_t thisNor;
			VectorSet(thisNor, ds->verts[verticesFound[a]].normal[0], ds->verts[verticesFound[a]].normal[1], ds->verts[verticesFound[a]].normal[2]);
			pcNor[0] += thisNor[0];
			pcNor[1] += thisNor[1];
			pcNor[2] += thisNor[2];
			numAdded++;
		}
		pcNor[0] /= numAdded;
		pcNor[1] /= numAdded;
		pcNor[2] /= numAdded;
		VectorNormalize(pcNor, pcNor);

//#pragma omp critical
		{
			ds->verts[i].normal[0] = pcNor[0];
			ds->verts[i].normal[1] = pcNor[1];
			ds->verts[i].normal[2] = pcNor[2];
		}
	}
}

void GenerateSmoothNormals(void)
{
#define MIN_SMN_INDEXES 6

	int numCompleted = 0;

	Sys_PrintHeading("--- GenerateSmoothNormals ---\n");

#pragma omp parallel for ordered num_threads(numthreads)
	for (int s = 0; s < numMapDrawSurfs; s++)
	{
#pragma omp critical
		{
			printLabelledProgress("GenerateSmoothNormals", numCompleted, numMapDrawSurfs);
			numCompleted++;
		}

		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		if (ds->shaderInfo && ds->shaderInfo->noSmooth) continue;

		GenerateSmoothNormalsForMesh(ds);
	}
}
#else //!__REGENERATE_BSP_NORMALS__
void GenerateSmoothNormals(void)
{
}
#endif //__REGENERATE_BSP_NORMALS__
