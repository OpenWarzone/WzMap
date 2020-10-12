/*
	proctree c++ port test / example
	Copyright (c) 2015 Jari Komppa

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgement in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	NOTE: this license covers this example only, proctree.cpp has different license
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include "proctree.h"
#include <string>

extern "C" {
	/* dependencies */
#include "picointernal.h"
}

extern std::string AssImp_getBasePath(const std::string& path);

void benchmark()
{
	Proctree::Tree tree;
	printf("Start..\n");
	int n = 100000;
	int j;
	for (j = 0; j < 10; j++)
	{
		std::chrono::time_point<std::chrono::system_clock> start, end;
		start = std::chrono::system_clock::now();
		int i;
		for (i = 0; i < n; i++)
		{
			tree.generate();
		}
		end = std::chrono::system_clock::now();
		std::chrono::duration<double> sec = end-start;
		printf("%3.3fs (%3.3f trees per second)\n", sec.count(), n / sec.count());
	}
}

#define VectorSet(v, a, b, c) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])

void Pico_AddQuadStamp2(picoVec3_t quadVerts[4], unsigned int *numIndexes, unsigned int *indexes, unsigned int *numVerts, picoVec3_t *xyz)
{
	int             ndx;

	ndx = *numVerts;

	// triangle indexes for a simple quad
	indexes[*numIndexes] = ndx;
	indexes[*numIndexes + 1] = ndx + 1;
	indexes[*numIndexes + 2] = ndx + 3;

	indexes[*numIndexes + 3] = ndx + 3;
	indexes[*numIndexes + 4] = ndx + 1;
	indexes[*numIndexes + 5] = ndx + 2;

	xyz[ndx + 0][0] = quadVerts[0][0];
	xyz[ndx + 0][1] = quadVerts[0][1];
	xyz[ndx + 0][2] = quadVerts[0][2];

	xyz[ndx + 1][0] = quadVerts[1][0];
	xyz[ndx + 1][1] = quadVerts[1][1];
	xyz[ndx + 1][2] = quadVerts[1][2];

	xyz[ndx + 2][0] = quadVerts[2][0];
	xyz[ndx + 2][1] = quadVerts[2][1];
	xyz[ndx + 2][2] = quadVerts[2][2];

	xyz[ndx + 3][0] = quadVerts[3][0];
	xyz[ndx + 3][1] = quadVerts[3][1];
	xyz[ndx + 3][2] = quadVerts[3][2];

	*numVerts += 4;
	*numIndexes += 6;
}

void Pico_CreateCube(const picoVec3_t mins, const picoVec3_t maxs, unsigned int *numIndexes, unsigned int *indexes, unsigned int *numVerts, picoVec3_t *verts)
{
	picoVec3_t quadVerts[4];

	VectorSet(quadVerts[0], mins[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], mins[0], maxs[1], mins[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], mins[0], mins[1], maxs[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);

	VectorSet(quadVerts[0], maxs[0], mins[1], maxs[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], mins[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);

	VectorSet(quadVerts[0], mins[0], mins[1], maxs[2]);
	VectorSet(quadVerts[1], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], maxs[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);

	VectorSet(quadVerts[0], maxs[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], mins[2]);
	VectorSet(quadVerts[3], mins[0], mins[1], mins[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);

	VectorSet(quadVerts[0], mins[0], mins[1], mins[2]);
	VectorSet(quadVerts[1], mins[0], mins[1], maxs[2]);
	VectorSet(quadVerts[2], maxs[0], mins[1], maxs[2]);
	VectorSet(quadVerts[3], maxs[0], mins[1], mins[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);

	VectorSet(quadVerts[0], maxs[0], maxs[1], mins[2]);
	VectorSet(quadVerts[1], maxs[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[2], mins[0], maxs[1], maxs[2]);
	VectorSet(quadVerts[3], mins[0], maxs[1], mins[2]);
	Pico_AddQuadStamp2(quadVerts, numIndexes, indexes, numVerts, verts);
}

int inline GetIntersection(float fDst1, float fDst2, picoVec3_t P1, picoVec3_t P2, picoVec3_t &Hit) {
	if ((fDst1 * fDst2) >= 0.0f) return 0;
	if (fDst1 == fDst2) return 0;
	Hit[0] = P1[0] + (P2[0] - P1[0]) * (-fDst1 / (fDst2 - fDst1));
	Hit[1] = P1[1] + (P2[1] - P1[1]) * (-fDst1 / (fDst2 - fDst1));
	Hit[2] = P1[2] + (P2[2] - P1[2]) * (-fDst1 / (fDst2 - fDst1));
	return 1;
}

int inline InBox(picoVec3_t Hit, picoVec3_t B1, picoVec3_t B2, const int Axis) {
	if (Axis == 1 && Hit[2] > B1[2] && Hit[2] < B2[2] && Hit[1] > B1[1] && Hit[1] < B2[1]) return 1;
	if (Axis == 2 && Hit[2] > B1[2] && Hit[2] < B2[2] && Hit[0] > B1[0] && Hit[0] < B2[0]) return 1;
	if (Axis == 3 && Hit[0] > B1[0] && Hit[0] < B2[0] && Hit[1] > B1[1] && Hit[1] < B2[1]) return 1;
	return 0;
}

// returns true if line (L1, L2) intersects with the box (B1, B2)
// returns intersection point in Hit
bool Pico_CheckLineBox(picoVec3_t B1, picoVec3_t B2, picoVec3_t L1, picoVec3_t L2, picoVec3_t &Hit)
{
	if (L2[0] < B1[0] && L1[0] < B1[0]) return false;
	if (L2[0] > B2[0] && L1[0] > B2[0]) return false;
	if (L2[1] < B1[1] && L1[1] < B1[1]) return false;
	if (L2[1] > B2[1] && L1[1] > B2[1]) return false;
	if (L2[2] < B1[2] && L1[2] < B1[2]) return false;
	if (L2[2] > B2[2] && L1[2] > B2[2]) return false;
	if (L1[0] > B1[0] && L1[0] < B2[0] &&
		L1[1] > B1[1] && L1[1] < B2[1] &&
		L1[2] > B1[2] && L1[2] < B2[2])
	{
		Hit[0] = L1[0];
		Hit[1] = L1[1];
		Hit[2] = L1[2];
		return true;
	}
	if ((GetIntersection(L1[0] - B1[0], L2[0] - B1[0], L1, L2, Hit) && InBox(Hit, B1, B2, 1))
		|| (GetIntersection(L1[1] - B1[1], L2[1] - B1[1], L1, L2, Hit) && InBox(Hit, B1, B2, 2))
		|| (GetIntersection(L1[2] - B1[2], L2[2] - B1[2], L1, L2, Hit) && InBox(Hit, B1, B2, 3))
		|| (GetIntersection(L1[0] - B2[0], L2[0] - B2[0], L1, L2, Hit) && InBox(Hit, B1, B2, 1))
		|| (GetIntersection(L1[1] - B2[1], L2[1] - B2[1], L1, L2, Hit) && InBox(Hit, B1, B2, 2))
		|| (GetIntersection(L1[2] - B2[2], L2[2] - B2[2], L1, L2, Hit) && InBox(Hit, B1, B2, 3)))
		return true;

	return false;
}

#define INVALID_BOUNDS 99999
#define vec_t float

void Pico_ClearBounds(picoVec3_t mins, picoVec3_t maxs) {
	mins[0] = mins[1] = mins[2] = +INVALID_BOUNDS;
	maxs[0] = maxs[1] = maxs[2] = -INVALID_BOUNDS;
}

void Pico_AddPointToBounds(picoVec3_t v, picoVec3_t mins, picoVec3_t maxs) {
	int i;
	vec_t val;

	if (mins[0] == +INVALID_BOUNDS) {
		if (maxs[0] == -INVALID_BOUNDS) {
			VectorCopy(v, mins);
			VectorCopy(v, maxs);
		}
	}

	for (i = 0; i < 3; i++)
	{
		val = v[i];
		if (val < mins[i]) {
			mins[i] = val;
		}
		if (val > maxs[i]) {
			maxs[i] = val;
		}
	}
}

bool PicoGenerateTreeModel_SetShader(picoModel_t *model, picoSurface_t *surface, char *shaderName)
{
	//printf("Setting shader to %s.\n", shaderName);

	/* setup the shader */
	picoShader_t *shader = PicoFindShader(model, shaderName, 0);

	if (shader == NULL)
	{
		/* create new pico shader */
		shader = PicoNewShader(model);
	}

	if (shader == NULL)
	{
		PicoFreeModel(model);
		return false;
	}

	/* scale shader colors */
	picoColor_t   ambient, diffuse, specular;
	for (int k = 0; k<4; k++)
	{
		ambient[k] = 255;
		diffuse[k] = 255;
		specular[k] = 255;
	}

	/* set shader colors */
	PicoSetShaderAmbientColor(shader, ambient);
	PicoSetShaderDiffuseColor(shader, diffuse);
	PicoSetShaderSpecularColor(shader, specular);

	/* set shader transparency */
	PicoSetShaderTransparency(shader, 1.0);

	/* set shader shininess (0..127) */
	PicoSetShaderShininess(shader, 127.0);

	/* set shader name */
	PicoSetShaderName(shader, shaderName);

	/* set shader texture map name */
	PicoSetShaderMapName(shader, shaderName);

	/* assign shader */
	PicoSetSurfaceShader(surface, shader);

	return true;
}

bool PicoGenerateTreeModel_CollisionBox(picoModel_t *model)
{
	if (model == NULL)
	{
		return false;
	}

	/* each surface on the model will become a new map drawsurface */
	int		numSurfaces = PicoGetModelNumSurfaces(model);

	picoVec3_t	validHitBounds[2];
	validHitBounds[0][0] = -8192;
	validHitBounds[0][1] = -8192;
	validHitBounds[0][2] = 0;

	validHitBounds[1][0] = 8192;
	validHitBounds[1][1] = 8192;
	validHitBounds[1][2] = 2;

	picoVec3_t	modelBoundsAroundBase[2];
	float	heightBounds[2];
	heightBounds[0] = 0.0;
	heightBounds[1] = 0.0;
	Pico_ClearBounds(modelBoundsAroundBase[0], modelBoundsAroundBase[1]);

	int numHits = 0;

	while (numHits == 0.0 && validHitBounds[1][2] <= 128.0)
	{// Make sure that we find at least some valid bounds to use... but <= 128.0 because we need to give up at some point...
		validHitBounds[1][2] *= 2.0;

		for (int s = 0; s < numSurfaces; s++)
		{
			int				skin = 0;

			/* get surface */
			picoSurface_t	*surface = PicoGetModelSurface(model, s);

			if (surface == NULL)
				continue;

			/* only handle triangle surfaces initially (fixme: support patches) */
			if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
			{
				printf("surface %i (%s) is not a triangles surface.", s, surface->name);
				continue;
			}

			char			*picoShaderName = PicoGetSurfaceShaderNameForSkin(surface, skin);

			/* get info */
			picoIndex_t *idx = PicoGetSurfaceIndexes(surface, 0);

			/* walk the triangle list */
			for (int j = 0; j < PicoGetSurfaceNumIndexes(surface); j += 3, idx += 3)
			{
				picoVec_t *xyzA = PicoGetSurfaceXYZ(surface, idx[0]);
				picoVec_t *xyzB = PicoGetSurfaceXYZ(surface, idx[1]);
				picoVec_t *xyzC = PicoGetSurfaceXYZ(surface, idx[2]);

				if (xyzA[2] < heightBounds[0])
					heightBounds[0] = xyzA[2];
				if (xyzA[2] > heightBounds[1])
					heightBounds[1] = xyzA[2];

				if (xyzB[2] < heightBounds[0])
					heightBounds[0] = xyzB[2];
				if (xyzB[2] > heightBounds[1])
					heightBounds[1] = xyzB[2];

				if (xyzC[2] < heightBounds[0])
					heightBounds[0] = xyzC[2];
				if (xyzC[2] > heightBounds[1])
					heightBounds[1] = xyzC[2];

				picoVec3_t hit;

				if (Pico_CheckLineBox(validHitBounds[0], validHitBounds[1], xyzA, xyzB, hit))
				{
					Pico_AddPointToBounds(xyzA, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					Pico_AddPointToBounds(xyzB, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					numHits++;
				}

				if (Pico_CheckLineBox(validHitBounds[0], validHitBounds[1], xyzA, xyzC, hit))
				{
					Pico_AddPointToBounds(xyzA, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					Pico_AddPointToBounds(xyzC, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					numHits++;
				}

				if (Pico_CheckLineBox(validHitBounds[0], validHitBounds[1], xyzB, xyzC, hit))
				{
					Pico_AddPointToBounds(xyzB, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					Pico_AddPointToBounds(xyzC, modelBoundsAroundBase[0], modelBoundsAroundBase[1]);
					numHits++;
				}
			}
		}
	}

	if (numHits <= 0)
	{
		printf("Failed to find valid points to generate a collision box for model %s.", model->fileName);
		return false;
	}

	// Set mix and max height of the cube to the min/max model heights to extend to the top of the tree...
	modelBoundsAroundBase[0][2] = heightBounds[0];
	modelBoundsAroundBase[1][2] = heightBounds[1];

	unsigned int		numVerts = 0;
	unsigned int		numIndexes = 0;
	picoVec3_t			*verts = (picoVec3_t*)malloc(24 * sizeof(picoVec3_t));
	unsigned int		*indexes = (unsigned int*)malloc(36 * sizeof(unsigned int));

	Pico_CreateCube(modelBoundsAroundBase[0], modelBoundsAroundBase[1], &numIndexes, indexes, &numVerts, verts);

	/* plain white */
	picoColor_t white = { 255,255,255,255 };

	if (numVerts > 0 && numIndexes > 0)
	{
		//printf("COLLISION: cube model %s. verts %u. indexes %u.\n", model->fileName, numVerts, numIndexes);

		/* create new pico surface */
		picoSurface_t *surface = PicoNewSurface(model);

		if (surface == NULL)
		{
			return false;
		}

		/* do surface setup */
		PicoSetSurfaceType(surface, PICO_TRIANGLES);
		PicoSetSurfaceName(surface, "collision");

		if (!PicoGenerateTreeModel_SetShader(model, surface, "collision"))
		{
			PicoFreeModel(model);
			return false;
		}

		/* process triangle indices */
		for (unsigned int k = 0; k < numIndexes; k+=3)
		{
			unsigned int tri[3];

			/* get ptr to triangle data */
			tri[0] = indexes[k+0];
			tri[1] = indexes[k+1];
			tri[2] = indexes[k+2];

			/* run through triangle vertices */
			for (int m = 0; m < 3; m++)
			{
				unsigned int	vertIndex = tri[m];
				picoVec2_t		texCoord;
				picoVec3_t		vert, n;

				VectorSet(vert, verts[vertIndex][0], verts[vertIndex][1], verts[vertIndex][2]);

#ifdef __INVERT_ASSIMP_NORMALS__
				VectorSet(n, 0, 0, 0);
#else !//__INVERT_ASSIMP_NORMALS__
				VectorSet(n, 0, 0, 0);
#endif //__INVERT_ASSIMP_NORMALS__

				int num = ((k * 3) + m);

				/* store vertex origin */
				PicoSetSurfaceXYZ(surface, vertIndex, vert);

				/* store vertex color */
				PicoSetSurfaceColor(surface, 0, vertIndex, white);

				/* store vertex normal */
				PicoSetSurfaceNormal(surface, vertIndex, n);

				/* store current face vertex index */
				PicoSetSurfaceIndex(surface, num, (picoIndex_t)tri[m]);

				/* get texture vertex coord */
				texCoord[0] = 0;
				texCoord[1] = 1;	/* flip t */

															/* store texture vertex coord */
				PicoSetSurfaceST(surface, 0, vertIndex, texCoord);
			}
		}
	}

	free(verts);
	free(indexes);

	return true;
}

#define QUAKE3_COORD_SCALE 64.0f

// Generate a procedural tree model based on inputs...
picoModel_t *PicoGenerateTreeModel(char *name, char *barkShader, char *leafShader, Proctree::Properties *treeProperties)
{
	// 1) Create the tree object

	Proctree::Tree tree;

	// 2) Change properties here for different kinds of trees
	/*
	tree.mProperties.mSeed = 7;
	tree.mProperties.mTreeSteps = 7;
	tree.mProperties.mLevels = 3;
	// etc.
	*/

	Proctree::Properties *mProperties = &tree.mProperties;

	mProperties->mSeed = (treeProperties && treeProperties->mSeed != 0) ? treeProperties->mSeed : 262;
	mProperties->mSegments = (treeProperties && treeProperties->mSegments != 0) ? treeProperties->mSegments : 6;
	mProperties->mLevels = (treeProperties && treeProperties->mLevels != 0) ? treeProperties->mLevels : 5;
	mProperties->mVMultiplier = (treeProperties && treeProperties->mVMultiplier != 0) ? treeProperties->mVMultiplier : 0.36f;
	mProperties->mTwigScale = (treeProperties && treeProperties->mTwigScale != 0) ? treeProperties->mTwigScale : 0.39f;
	mProperties->mInitialBranchLength = (treeProperties && treeProperties->mInitialBranchLength != 0) ? treeProperties->mInitialBranchLength : 0.49f;
	mProperties->mLengthFalloffFactor = (treeProperties && treeProperties->mLengthFalloffFactor != 0) ? treeProperties->mLengthFalloffFactor : 0.85f;
	mProperties->mLengthFalloffPower = (treeProperties && treeProperties->mLengthFalloffPower != 0) ? treeProperties->mLengthFalloffPower : 0.99f;
	mProperties->mClumpMax = (treeProperties && treeProperties->mClumpMax != 0) ? treeProperties->mClumpMax : 0.454f;
	mProperties->mClumpMin = (treeProperties && treeProperties->mClumpMin != 0) ? treeProperties->mClumpMin : 0.404f;
	mProperties->mBranchFactor = (treeProperties && treeProperties->mBranchFactor != 0) ? treeProperties->mBranchFactor : 2.45f;
	mProperties->mDropAmount = (treeProperties && treeProperties->mDropAmount != 0) ? treeProperties->mDropAmount : -0.1f;
	mProperties->mGrowAmount = (treeProperties && treeProperties->mGrowAmount != 0) ? treeProperties->mGrowAmount : 0.235f;
	mProperties->mSweepAmount = (treeProperties && treeProperties->mSweepAmount != 0) ? treeProperties->mSweepAmount : 0.01f;
	mProperties->mMaxRadius = (treeProperties && treeProperties->mMaxRadius != 0) ? treeProperties->mMaxRadius : 0.139f;
	mProperties->mClimbRate = (treeProperties && treeProperties->mClimbRate != 0) ? treeProperties->mClimbRate : 0.371f;
	mProperties->mTrunkKink = (treeProperties && treeProperties->mTrunkKink != 0) ? treeProperties->mTrunkKink : 0.093f;
	mProperties->mTreeSteps = (treeProperties && treeProperties->mTreeSteps != 0) ? treeProperties->mTreeSteps : 5;
	mProperties->mTaperRate = (treeProperties && treeProperties->mTaperRate != 0) ? treeProperties->mTaperRate : 0.947f;
	mProperties->mRadiusFalloffRate = (treeProperties && treeProperties->mRadiusFalloffRate != 0) ? treeProperties->mRadiusFalloffRate : 0.73f;
	mProperties->mTwistRate = (treeProperties && treeProperties->mTwistRate != 0) ? treeProperties->mTwistRate : 3.02f;
	mProperties->mTrunkLength = (treeProperties && treeProperties->mTrunkLength != 0) ? treeProperties->mTrunkLength : 2.4f;

	// 3) Call generate
	tree.generate();

	// 4) Use the data

	/* create new pico model */
	picoModel_t *model = PicoNewModel();

	if (model == NULL)
	{
		// No longer need the scene data...
		return NULL;
	}

	/* do model setup */
	PicoSetModelFrameNum(model, 0);
	PicoSetModelName(model, name);
	PicoSetModelFileName(model, name);

	std::string basePath = AssImp_getBasePath(name);

	int numAddedVerts = 0;

	/* plain white */
	picoColor_t white = { 255,255,255,255 };

	/* create new pico surface */
	picoSurface_t *surface = PicoNewSurface(model);

	if (surface == NULL)
	{
		PicoFreeModel(model);
		return NULL;
	}

	/* do surface setup */
	PicoSetSurfaceType(surface, PICO_TRIANGLES);
	PicoSetSurfaceName(surface, barkShader);

	if (!PicoGenerateTreeModel_SetShader(model, surface, barkShader))
	{
		PicoFreeModel(model);
		return NULL;
	}
	
	/* process triangle indices */
	for (int k = 0; k < tree.mFaceCount; k++)
	{
		unsigned int tri[3];

		/* get ptr to triangle data */
		tri[0] = tree.mFace[k].x;
		tri[1] = tree.mFace[k].y;
		tri[2] = tree.mFace[k].z;

		/* run through triangle vertices */
		for (int m = 0; m < 3; m++)
		{
			unsigned int	vertIndex = tri[m];
			picoVec2_t		texCoord;
			picoVec3_t		vert, n;

			VectorSet(vert, tree.mVert[vertIndex].x * QUAKE3_COORD_SCALE, tree.mVert[vertIndex].z * QUAKE3_COORD_SCALE, tree.mVert[vertIndex].y * QUAKE3_COORD_SCALE);

#ifdef __INVERT_ASSIMP_NORMALS__
			VectorSet(n, -tree.mNormal[vertIndex].x, -tree.mNormal[vertIndex].z, -tree.mNormal[vertIndex].y);
#else !//__INVERT_ASSIMP_NORMALS__
			VectorSet(n, tree.mNormal[vertIndex].x, tree.mNormal[vertIndex].z, tree.mNormal[vertIndex].y);
#endif //__INVERT_ASSIMP_NORMALS__

			int num = ((k * 3) + m);

			/* store vertex origin */
			PicoSetSurfaceXYZ(surface, vertIndex, vert);

			/* store vertex color */
			PicoSetSurfaceColor(surface, 0, vertIndex, white);

			/* store vertex normal */
			PicoSetSurfaceNormal(surface, vertIndex, n);

			/* store current face vertex index */
			PicoSetSurfaceIndex(surface, num, (picoIndex_t)tri[m]);

			/* get texture vertex coord */
			texCoord[0] = tree.mUV[vertIndex].u;
			texCoord[1] = tree.mUV[vertIndex].v;	/* flip t */

			/* store texture vertex coord */
			PicoSetSurfaceST(surface, 0, vertIndex, texCoord);

			numAddedVerts++;
		}
	}

	// Add a box for collisions around the trunk...
	if (!PicoGenerateTreeModel_CollisionBox(model))
	{
		//PicoFreeModel(model);
		//return NULL;
	}

	/* create new pico surface */
	surface = PicoNewSurface(model);

	if (surface == NULL)
	{
		PicoFreeModel(model);
		return NULL;
	}

	/* do surface setup */
	PicoSetSurfaceType(surface, PICO_TRIANGLES);
	PicoSetSurfaceName(surface, leafShader);

	if (!PicoGenerateTreeModel_SetShader(model, surface, leafShader))
	{
		PicoFreeModel(model);
		return NULL;
	}
	
	/* process triangle indices */
	for (int k = 0; k < tree.mTwigFaceCount; k++)
	{
		unsigned int tri[3];

		/* get ptr to triangle data */
		tri[0] = tree.mTwigFace[k].x;
		tri[1] = tree.mTwigFace[k].y;
		tri[2] = tree.mTwigFace[k].z;

		/* run through triangle vertices */
		for (int m = 0; m < 3; m++)
		{
			unsigned int	vertIndex = tri[m];
			picoVec2_t		texCoord;
			picoVec3_t		vert, n;

			VectorSet(vert, tree.mTwigVert[vertIndex].x * QUAKE3_COORD_SCALE, tree.mTwigVert[vertIndex].z * QUAKE3_COORD_SCALE, tree.mTwigVert[vertIndex].y * QUAKE3_COORD_SCALE);

#ifdef __INVERT_ASSIMP_NORMALS__
			VectorSet(n, -tree.mTwigNormal[vertIndex].x, -tree.mTwigNormal[vertIndex].z, -tree.mTwigNormal[vertIndex].y);
#else !//__INVERT_ASSIMP_NORMALS__
			VectorSet(n, tree.mTwigNormal[vertIndex].x, tree.mTwigNormal[vertIndex].z, tree.mTwigNormal[vertIndex].y);
#endif //__INVERT_ASSIMP_NORMALS__

			int num = ((k * 3) + m);

			/* store vertex origin */
			PicoSetSurfaceXYZ(surface, vertIndex, vert);

			/* store vertex color */
			PicoSetSurfaceColor(surface, 0, vertIndex, white);

			/* store vertex normal */
			PicoSetSurfaceNormal(surface, vertIndex, n);

			/* store current face vertex index */
			PicoSetSurfaceIndex(surface, num, (picoIndex_t)tri[m]);

			/* get texture vertex coord */
			texCoord[0] = tree.mTwigUV[vertIndex].u;
			texCoord[1] = tree.mTwigUV[vertIndex].v;	/* flip t */

													/* store texture vertex coord */
			PicoSetSurfaceST(surface, 0, vertIndex, texCoord);

			numAddedVerts++;
		}
	}

	// 5) Profit.

	// Note: You can change the properties and call generate to change the data, 
	// no need to delete the tree object in between.

	return model;
}

void PicoGenerateTreeModelDefaults(Proctree::Properties *treeProperties)
{
	treeProperties->mSeed = 262;
	treeProperties->mSegments = 6;
	treeProperties->mLevels = 5;
	treeProperties->mVMultiplier = 0.36f;
	treeProperties->mTwigScale = 0.39f;
	treeProperties->mInitialBranchLength = 0.49f;
	treeProperties->mLengthFalloffFactor = 0.85f;
	treeProperties->mLengthFalloffPower = 0.99f;
	treeProperties->mClumpMax = 0.454f;
	treeProperties->mClumpMin = 0.404f;
	treeProperties->mBranchFactor = 2.45f;
	treeProperties->mDropAmount = -0.1f;
	treeProperties->mGrowAmount = 0.235f;
	treeProperties->mSweepAmount = 0.01f;
	treeProperties->mMaxRadius = 0.139f;
	treeProperties->mClimbRate = 0.371f;
	treeProperties->mTrunkKink = 0.093f;
	treeProperties->mTreeSteps = 5;
	treeProperties->mTaperRate = 0.947f;
	treeProperties->mRadiusFalloffRate = 0.73f;
	treeProperties->mTwistRate = 3.02f;
	treeProperties->mTrunkLength = 2.4f;
}