/*
===========================================================================
Copyright (C) 2012 Unvanquished Developers
This file is part of Daemon source code.
Daemon source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.
Daemon source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with Daemon source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "q3map2.h"
#include <vector>
#include <queue>
#include <iostream>
#include <cstdlib>
#include "inifile.h"


#define __IGNORE_EXTRA_SURFACES__

//flag for excluding caulk surfaces
static qboolean excludeCaulk = qtrue;

//flag for excluding surfaces with surfaceparm sky from navmesh generation
static qboolean excludeSky = qtrue;

float scanDensity = 64.0;



extern float MAP_WATER_LEVEL;

extern qboolean StringContainsWord(const char *haystack, const char *needle);

qboolean GenFoliageisTreeSolid( char *strippedName )
{
	if (StringContainsWord(strippedName, "bark") || StringContainsWord(strippedName, "trunk") || StringContainsWord(strippedName, "giant_tree") || StringContainsWord(strippedName, "vine01"))
		return qtrue;

	return qfalse;
}

#define MAX_SHADER_DEPRECATION_DEPTH 16

shaderInfo_t *GenFoliageShaderInfoForShader(const char *shaderName)
{
	int				i;
	int				deprecationDepth;
	shaderInfo_t	*si;
	char			shader[MAX_QPATH];

	/* dummy check */
	if (shaderName == NULL || shaderName[0] == '\0')
	{
		Sys_Printf("WARNING: Null or empty shader name\n");
		shaderName = "missing";
	}

	strcpy(shader, shaderName);

	/* search for it */
	deprecationDepth = 0;
	for (i = 0; i < numShaderInfo; i++)
	{
		si = &shaderInfo[i];
		if (!Q_stricmp(shader, si->shader))
		{
			/* check if shader is deprecated */
			if (deprecationDepth < MAX_SHADER_DEPRECATION_DEPTH && si->deprecateShader && si->deprecateShader[0])
			{
				/* override name */
				strcpy(shader, si->deprecateShader);
				StripExtension(shader);
				/* increase deprecation depth */
				deprecationDepth++;
				if (deprecationDepth == MAX_SHADER_DEPRECATION_DEPTH)
					Sys_Warning("MAX_SHADER_DEPRECATION_DEPTH (%i) exceeded on shader '%s'\n", MAX_SHADER_DEPRECATION_DEPTH, shader);
				/* search again from beginning */
				i = -1;
				continue;
			}

			/* load image if necessary */
			if (si->finished == qfalse)
			{
				//LoadShaderImages(si);
				//FinishShader(si);
			}

			/* return it */
			return si;
		}
	}

	/* allocate a default shader */
	extern shaderInfo_t	*AllocShaderInfo(void);

#pragma omp critical (__ALLOC_SI__)
	{
		si = AllocShaderInfo();
	}
	strcpy(si->shader, shader);
	//LoadShaderImages(si);
	//FinishShader(si);

	/* return it */
	return si;
}

void GenFoliageFindWaterLevel(void)
{
	if (MAP_WATER_LEVEL > -999999.0)
	{// Forced by climate ini file...
		Sys_Printf("Climate specified map water level at %.4f.\n", MAP_WATER_LEVEL);
		return;
	}

	float waterLevel = 999999.9;
	float waterLevel2 = 999999.9; // we actually want the second highest water level...

	for (int s = 0; s < numBSPDrawSurfaces; s++)
	{
		printLabelledProgress("FindWaterLevel", s, numBSPDrawSurfaces);

		/* get drawsurf */
		bspDrawSurface_t	*ds = &bspDrawSurfaces[s];
		bspShader_t			*shader = &bspShaders[ds->shaderNum];
		shaderInfo_t		*si = ShaderInfoForShader(shader->shader);

		if ((si->compileFlags & C_SKIP) || (si->compileFlags & C_NODRAW))
		{
			continue;
		}

		if (!(si->compileFlags & C_LIQUID)
			&& si->materialType != MATERIAL_WATER
			&& !StringContainsWord(si->shader, "water"))
		{
			continue;
		}

		for (int j = 0; j < ds->numIndexes; j++)
		{
			float height = bspDrawVerts[bspDrawIndexes[j]].xyz[2];

			if (height < waterLevel)
			{
				waterLevel2 = waterLevel;
				waterLevel = height;
				continue;
			}
		}
	}

	if (waterLevel2 < 999999.0 && waterLevel2 > waterLevel)
	{
		waterLevel = waterLevel2;
	}

	if (waterLevel >= 999999.0)
	{
		waterLevel = -999999.9;// mapPlayableMins[2];
	}

	MAP_WATER_LEVEL = waterLevel;

	Sys_Printf("Detected lowest map water level at %.4f.\n", MAP_WATER_LEVEL);
}


extern qboolean SurfaceIsAllowedFoliage(int materialType);

int MaterialIsValidForFoliage(int materialType)
{
	switch (materialType)
	{
	case MATERIAL_SHORTGRASS:		// 5					// manicured lawn
	case MATERIAL_LONGGRASS:		// 6					// long jungle grass
	case MATERIAL_MUD:				// 17					// wet soil
	case MATERIAL_DIRT:				// 7					// hard mud
	case MATERIAL_SAND:				// 8					// sand
		return 1;
		break;
	default:
		break;
	}

	if (SurfaceIsAllowedFoliage(materialType))
	{
		return 2;
	}

	return 0;
}

float frandom(void)
{
	return (float)rand() / RAND_MAX;
}

#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

typedef vec3_t	vec3pair_t[2], matrix3_t[3];

void AnglesToAxis(vec3_t angles, matrix3_t axis) {
	vec3_t	right;

	// angle vectors returns "right" instead of "y axis"
	AngleVectors(angles, axis[0], right, axis[2]);
	VectorSubtract(vec3_origin, right, axis[1]);
}

void AxisCopy(matrix3_t in, matrix3_t out) {
	VectorCopy(in[0], out[0]);
	VectorCopy(in[1], out[1]);
	VectorCopy(in[2], out[2]);
}

void FOLIAGE_ApplyAxisRotation(vec3_t axis[3], int rotType, float value)
{//apply matrix rotation to this axis.
 //rotType = type of rotation (PITCH, YAW, ROLL)
 //value = size of rotation in degrees, no action if == 0
	vec3_t result[3];  //The resulting axis
	vec3_t rotation[3];  //rotation matrix
	int i, j; //multiplication counters

	if (value == 0)
	{//no rotation, just return.
		return;
	}

	//init rotation matrix
	switch (rotType)
	{
	case ROLL: //R_X
		rotation[0][0] = 1;
		rotation[0][1] = 0;
		rotation[0][2] = 0;

		rotation[1][0] = 0;
		rotation[1][1] = cos(value / 360 * (2 * M_PI));
		rotation[1][2] = -sin(value / 360 * (2 * M_PI));

		rotation[2][0] = 0;
		rotation[2][1] = sin(value / 360 * (2 * M_PI));
		rotation[2][2] = cos(value / 360 * (2 * M_PI));
		break;

	case PITCH: //R_Y
		rotation[0][0] = cos(value / 360 * (2 * M_PI));
		rotation[0][1] = 0;
		rotation[0][2] = sin(value / 360 * (2 * M_PI));

		rotation[1][0] = 0;
		rotation[1][1] = 1;
		rotation[1][2] = 0;

		rotation[2][0] = -sin(value / 360 * (2 * M_PI));
		rotation[2][1] = 0;
		rotation[2][2] = cos(value / 360 * (2 * M_PI));
		break;

	case YAW: //R_Z
		rotation[0][0] = cos(value / 360 * (2 * M_PI));
		rotation[0][1] = -sin(value / 360 * (2 * M_PI));
		rotation[0][2] = 0;

		rotation[1][0] = sin(value / 360 * (2 * M_PI));
		rotation[1][1] = cos(value / 360 * (2 * M_PI));
		rotation[1][2] = 0;

		rotation[2][0] = 0;
		rotation[2][1] = 0;
		rotation[2][2] = 1;
		break;

	default:
		Sys_Printf("Error:  Bad rotType %i given to ApplyAxisRotation\n", rotType);
		break;
	};

	//apply rotation
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			result[i][j] = rotation[i][0] * axis[0][j] + rotation[i][1] * axis[1][j]
				+ rotation[i][2] * axis[2][j];
			/* post apply method
			result[i][j] = axis[i][0]*rotation[0][j] + axis[i][1]*rotation[1][j]
			+ axis[i][2]*rotation[2][j];
			*/
		}
	}

	//copy result
	AxisCopy(result, axis);

}

// Produce a psuedo random point that exists on the current triangle primitive.
void randomBarycentricCoordinate(vec3_t v1, vec3_t v2, vec3_t v3, vec3_t &pos) {
	float R = frandom();
	float S = frandom();

	if (R + S >= 1) {
		R = 1 - R;
		S = 1 - S;
	}

	//return v1 + (R * (v2 - v1)) + (S * (v3 - v1));
	vec3_t p1, p2;
	p1[0] = R * (v2[0] - v1[0]);
	p1[1] = R * (v2[1] - v1[1]);
	p1[2] = R * (v2[2] - v1[2]);

	p2[0] = S * (v3[0] - v1[0]);
	p2[1] = S * (v3[1] - v1[1]);
	p2[2] = S * (v3[2] - v1[2]);

	pos[0] = v1[0] + p1[0] + p2[0];
	pos[1] = v1[1] + p1[1] + p2[1];
	pos[2] = v1[2] + p1[2] + p2[2];
}

static void GenFoliage(char *mapName)
{
	const int		FOLIAGE_MAX_FOLIAGES = 2097152;
	//vec3_t			FOLIAGE_POINTS[FOLIAGE_MAX_FOLIAGES] = { { 0.0 } };
	//vec3_t			FOLIAGE_NORMALS[FOLIAGE_MAX_FOLIAGES] = { { 0.0 } };
	vec3_t			*FOLIAGE_POINTS = (vec3_t *)malloc(sizeof(vec3_t) * FOLIAGE_MAX_FOLIAGES);
	vec3_t			*FOLIAGE_NORMALS = (vec3_t *)malloc(sizeof(vec3_t) * FOLIAGE_MAX_FOLIAGES);
	qboolean		*FOLIAGE_NOT_GROUND = (qboolean *)malloc(sizeof(qboolean) * FOLIAGE_MAX_FOLIAGES);
	int				FOLIAGE_COUNT = 0;

	GenFoliageFindWaterLevel();

	Sys_PrintHeading("--- Finding Map Bounds ---\n");

	vec3_t bounds[2];
	ClearBounds(bounds[0], bounds[1]);

	for (int i = 0; i < numBSPDrawSurfaces; i++)
	{
		bspDrawSurface_t *surf = &bspDrawSurfaces[i];

		bspDrawVert_t *vs = &bspDrawVerts[surf->firstVert];
		int *idxs = &bspDrawIndexes[surf->firstIndex];

		for (int j = 0; j < surf->numIndexes; j += 3)
		{
			AddPointToBounds(vs[idxs[j]].xyz, bounds[0], bounds[1]);
			AddPointToBounds(vs[idxs[j+1]].xyz, bounds[0], bounds[1]);
			AddPointToBounds(vs[idxs[j+2]].xyz, bounds[0], bounds[1]);
		}
	}

	Sys_Printf("* Map bounds are %f %f %f x %f %f %f.\n", bounds[0][0], bounds[0][1], bounds[0][2], bounds[1][0], bounds[1][1], bounds[1][2]);

	Sys_PrintHeading("--- Generating Foliage Points ---\n");

	int solidFlags = C_SOLID;
	int numCompleted = 0;

#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
	for (int i = 0; i < numBSPDrawSurfaces; i++)
	{
#pragma omp critical (__PROGRESS__)
		{
			printLabelledProgress("GenFoliage", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *surf = &bspDrawSurfaces[i];
		bspShader_t *shader = &bspShaders[surf->shaderNum];
		
		if (!(shader->contentFlags & solidFlags)) {
			continue;
		}

#ifdef __IGNORE_EXTRA_SURFACES__
		if (StringContainsWord(shader->shader, "warzone/rock"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "warzone/tree"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "warzone/village"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "warzone/building"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "warzone/campfire"))
		{
			continue;
		}

		if (excludeCaulk && StringContainsWord(shader->shader, "caulk"))
		{
			continue;
		}

		if (excludeCaulk && StringContainsWord(shader->shader, "system/skip"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "sky"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "skies"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "cliff"))
		{
			continue;
		}

		if (StringContainsWord(shader->shader, "ledge"))
		{
			continue;
		}
#endif //__IGNORE_EXTRA_SURFACES__

		extern const char *materialNames[MATERIAL_LAST];
		shaderInfo_t	*si = GenFoliageShaderInfoForShader(shader->shader);
		int surfaceMaterialValidity = MaterialIsValidForFoliage(si->materialType);

		if (!si || !surfaceMaterialValidity)
		{
			//Sys_Printf("Position %i is invalid material %s (%i) on shader %s.\n", i, si ? materialNames[si->materialType] : "UNKNOWN", si ? si->materialType : 0, shader->shader);
			continue;
		}
		/*else if (surfaceMaterialValidity == 2)
		{
			Sys_Printf("Position %i is a non ground material %s (%i) on shader %s.\n", i, materialNames[si->materialType], si->materialType, shader->shader);
		}
		else
		{
			Sys_Printf("Position %i is valid material %s (%i) on shader %s.\n", i, materialNames[si->materialType], si->materialType, shader->shader);
		}*/

		extern float Distance(vec3_t pos1, vec3_t pos2);

		bspDrawVert_t *vs = &bspDrawVerts[surf->firstVert];
		int *idxs = &bspDrawIndexes[surf->firstIndex];

		for (int j = 0; j < surf->numIndexes; j+=3)
		{
			vec3_t verts[3];
			
			VectorCopy(vs[idxs[j]].xyz, verts[0]);
			VectorCopy(vs[idxs[j+1]].xyz, verts[1]);
			VectorCopy(vs[idxs[j+2]].xyz, verts[2]);
			
			if (verts[0][2] <= MAP_WATER_LEVEL
				&& verts[1][2] <= MAP_WATER_LEVEL
				&& verts[2][2] <= MAP_WATER_LEVEL)
			{// If any of the points on this triangle are below the map's water level, then skip this triangle...
				continue;
			}

			float d1 = Distance(verts[0], verts[1]);
			float d2 = Distance(verts[0], verts[2]);
			float d3 = Distance(verts[1], verts[2]);

			float tringleSize = max(d1, max(d2, d3));

			if (tringleSize < scanDensity)
			{// Triangle is smaller then minimum density, we can skip it completely...
				//Sys_Printf("Position %i is too small (%.4f).\n", i, tringleSize);
				continue;
			}

			vec3_t normal;
			VectorCopy(vs[idxs[j]].normal, normal);
			
			extern void vectoangles(const vec3_t value1, vec3_t angles);

			vec3_t angles;
			vectoangles(normal, angles);
			float pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;

			if (pitch > 47.0 || pitch < -47.0)
			{// Bad slope for a foliage...
				//Sys_Printf("Position %i angles too great (%.4f) shader: %s.\n", i, pitch, shader->shader);
				continue;
			}

			//
			// Ok, looks like a valid place to make foliage points, make foliages for this triangle...
			//
			//int numFoliages = max((int)(tringleSize / scanDensity), 1);
			int numPossibleFoliages = max((int)(((tringleSize*tringleSize) / 2.0) / scanDensity), 1);
			//Sys_Printf("Position %i numPossibleFoliages %i (ts: %.4f) (sd: %.4f) shader: %s.\n", i, numPossibleFoliages, tringleSize, scanDensity, (shader && shader->shader[0] != 0) ? shader->shader : "UNKNOWN");
			
			/*printf("DEBUG: v1 %f %f %f. v2 %f %f %f. v3 %f %f %f.\n"
				, verts[0][0], verts[0][1], verts[0][2]
				, verts[1][0], verts[1][1], verts[1][2]
				, verts[2][0], verts[2][1], verts[2][2]);*/

			// Make a temp list of foliage positions, so we can more quickly scan if theres already a point too close...
			const int	THIS_TRI_MAX_FOLIAGES = 8192;
			int			THIS_TRI_FOLIAGE_COUNT = 0;
			vec3_t		THIS_TRI_FOLIAGES[THIS_TRI_MAX_FOLIAGES];

			for (int f = 0; f < numPossibleFoliages; f++)
			{
				if (THIS_TRI_FOLIAGE_COUNT + 1 >= THIS_TRI_MAX_FOLIAGES)
					break;

				vec3_t pos;
				randomBarycentricCoordinate(verts[0], verts[1], verts[2], pos);

				/*printf("DEBUG: v1 %f %f %f. v2 %f %f %f. v3 %f %f %f. pos %f %f %f.\n"
					, verts[0][0], verts[0][1], verts[0][2]
					, verts[1][0], verts[1][1], verts[1][2]
					, verts[2][0], verts[2][1], verts[2][2]
					, pos[0], pos[1], pos[2]);*/

				if (pos[2] <= MAP_WATER_LEVEL + 16.0)
				{// If any of the points on this triangle are below the map's water level, then skip this triangle...
					continue;
				}

				qboolean bad = qfalse;

				for (int z = 0; z < THIS_TRI_FOLIAGE_COUNT; z++)
				{
					if (Distance(pos, THIS_TRI_FOLIAGES[z]) < scanDensity)
					{// Already have one here...
						bad = qtrue;
						break;
					}
				}

				if (bad)
					continue;

				VectorCopy(pos, THIS_TRI_FOLIAGES[THIS_TRI_FOLIAGE_COUNT]);
				THIS_TRI_FOLIAGE_COUNT++;
			}

			// Now add the new foliages to the final list...
			for (int f = 0; f < THIS_TRI_FOLIAGE_COUNT; f++)
			{
				if (FOLIAGE_COUNT + 1 >= FOLIAGE_MAX_FOLIAGES)
					break;

#pragma omp critical (__ADD_FOLIAGE__)
				{
					//printf("DEBUG: adding at pos %f %f %f.\n", THIS_TRI_FOLIAGES[f][0], THIS_TRI_FOLIAGES[f][1], THIS_TRI_FOLIAGES[f][2]);
					VectorCopy(THIS_TRI_FOLIAGES[f], FOLIAGE_POINTS[FOLIAGE_COUNT]);
					VectorCopy(normal, FOLIAGE_NORMALS[FOLIAGE_COUNT]);
					FOLIAGE_POINTS[FOLIAGE_COUNT][2] -= 8.0;
					
					if (surfaceMaterialValidity == 2)
					{// Surface on alt materials, mountains etc... So addfoliage can skip the big model cull range checks...
						FOLIAGE_NOT_GROUND[FOLIAGE_COUNT] = qtrue;
					}
					else
					{
						FOLIAGE_NOT_GROUND[FOLIAGE_COUNT] = qfalse;
					}

					FOLIAGE_COUNT++;
				}
			}
		}
	}

	/*for (int i = 0; i < FOLIAGE_COUNT; i++)
	{
		printf("DEBUG: pos %f %f %f.\n", FOLIAGE_POINTS[i][0], FOLIAGE_POINTS[i][1], FOLIAGE_POINTS[i][2]);
	}*/

	Sys_Printf("* Generated %i foliage positions.\n", FOLIAGE_COUNT);

	int FOLIAGE_NOT_GROUND_COUNT = 0;

	{// Save them...
		FILE			*f = NULL;

		f = fopen(va("%s.foliage", mapName), "wb");

		if (!f)
		{
			Sys_Printf("Failed to open foliage file %s.foliage for save.\n");
			return;
		}

		fwrite(&FOLIAGE_COUNT, sizeof(int), 1, f);

		for (int i = 0; i < FOLIAGE_COUNT; i++)
		{
			int FOLIAGE_PLANT_SELECTION = 1; // Don't have counts in wzmap, so we will have to use replant ingame later...
			int FOLIAGE_PLANT_SELECTION_NOTGROUND = 2; // mark this as a non-normal surface (not grass, etc, eg: mountains)
			float FOLIAGE_SCALE = 1.0;// frandom() * 0.5 + 0.5; // Just randomize the sizes...
			float FOLIAGE_ANGLE = 0;

			{
				extern void vectoangles(const vec3_t value1, vec3_t angles);
				
				vec3_t angles;
				matrix3_t fmat;

				vectoangles(FOLIAGE_NORMALS[i], angles);
				angles[PITCH] += 90;
				AnglesToAxis(angles, fmat);

				FOLIAGE_ApplyAxisRotation(fmat, YAW, FOLIAGE_ANGLE);
			}

			if (FOLIAGE_NOT_GROUND[i])
			{
				FOLIAGE_NOT_GROUND_COUNT++;
			}

			fwrite(&FOLIAGE_POINTS[i], sizeof(vec3_t), 1, f);
			fwrite(&FOLIAGE_NORMALS[i], sizeof(vec3_t), 1, f);
			fwrite(FOLIAGE_NOT_GROUND[i] ? &FOLIAGE_PLANT_SELECTION_NOTGROUND : &FOLIAGE_PLANT_SELECTION, sizeof(int), 1, f);
			fwrite(&FOLIAGE_ANGLE, sizeof(float), 1, f);
			fwrite(&FOLIAGE_SCALE, sizeof(float), 1, f);

			int zero = 0;
			float zerof = 0.0;
			fwrite(&zero, sizeof(int), 1, f); // FOLIAGE_TREE_SELECTION
			fwrite(&zerof, sizeof(float), 1, f); // FOLIAGE_TREE_ANGLE
			fwrite(&zerof, sizeof(float), 1, f); // FOLIAGE_TREE_SCALE
		}

		fclose(f);
	}

	Sys_Printf("* Saved %i foliage positions (%i are special surfaces) to file %s.\n", FOLIAGE_COUNT, FOLIAGE_NOT_GROUND_COUNT, va("%s.foliage", mapName));

	free(FOLIAGE_POINTS);
	free(FOLIAGE_NORMALS);
	free(FOLIAGE_NOT_GROUND);
}

/*
===========
NavMain
===========
*/
int GenFoliageMain(int argc, char **argv)
{
	float temp;
	int i;

	if(argc < 2)
	{
		Sys_Printf("Usage: wzmap -genfoliage [-scanDensity #] [-includecaulk] [-includesky] MAPNAME\n");
		return 0;
	}

	/* note it */
	Sys_PrintHeading("--- GenFoliage ---\n");

	game = GetGame("ja");
	printf("* Game forced to %s (bspIdent %s - bspVersion %i).\n", game->arg, game->bspIdent, game->bspVersion);

	/* process arguments */
	for(i = 1; i < (argc - 1); i++)
	{
		if (!Q_stricmp(argv[i], "-scanDensity")) {
			i++;
			if (i < (argc - 1)) {
				temp = atof(argv[i]);
				if (temp > 0) {
					scanDensity = temp;
				}
			}
		} else if(!Q_stricmp(argv[i], "-includecaulk")) {
			excludeCaulk = qfalse;
		} else if(!Q_stricmp(argv[i], "-includesky")) {
			excludeSky = qfalse;
		} else {
			Sys_Printf("WARNING: Unknown option \"%s\"\n", argv[i]);
		}
	}

	/* load the bsp */
	sprintf(source, "%s%s", inbase, ExpandArg(argv[i]));
	StripExtension(source);

	/*{
		extern void FOLIAGE_LoadClimateData(char *filename);
		char filename2[1024] = { 0 };
		sprintf(filename2, "%s.climate", source);
		FOLIAGE_LoadClimateData(filename2);
	}*/
	{// We need water level...
		char filename2[1024] = { 0 };
		char filenameTemp[1024] = { 0 };
		strcpy(filenameTemp, source);
		StripExtension(filenameTemp);
		sprintf(filename2, "%s.climate", filenameTemp);

		//extern void FOLIAGE_LoadClimateData(char *filename);
		//FOLIAGE_LoadClimateData(filename2);

		MAP_WATER_LEVEL = atof(IniRead(filename2, "GENERAL", "forcedWaterLevel", "-999999.9"));

		if (MAP_WATER_LEVEL > -999999.0)
		{
			Sys_Printf("Forcing map water level to %.4f from climate file %s.\n", MAP_WATER_LEVEL, filename2);
		}
	}

	char mapFilename[1024] = { 0 };
	strcpy(mapFilename, source);

	strcat(source, ".bsp");
	LoadShaderInfo();

	Sys_Printf("Loading %s\n", source);
	
	LoadBSPFile(source);

	ParseEntities();

	GenFoliage(mapFilename);

	return 0;
}
