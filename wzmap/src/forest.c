/* marker */
#define MAP_C



/* dependencies */
#include "q3map2.h"
#include "inifile.h"

extern void SetEntityBounds( entity_t *e );
extern void LoadEntityIndexMap( entity_t *e );
extern void AdjustBrushesForOrigin( entity_t *ent );

extern float Distance(vec3_t pos1, vec3_t pos2);

float DistanceHorizontal(const vec3_t p1, const vec3_t p2) {
	vec3_t	v;

	VectorSubtract(p2, p1, v);
	return sqrt(v[0] * v[0] + v[1] * v[1]); //Leave off the z component
}

float DistanceVertical(const vec3_t p1, const vec3_t p2) {
	vec3_t	v;

	VectorSubtract(p2, p1, v);
	return sqrt(v[2] * v[2]); //Leave off the z component
}

qboolean BoundsWithinPlayableArea ( vec3_t mins, vec3_t maxs )
{
	if (mins[2] < mapPlayableMins[2] && maxs[2] < mapPlayableMins[2])
	{// Outside map bounds, definately cull...
		return qfalse;
	}

	/*if (mins[2] > mapPlayableMaxs[2] && maxs[2] > mapPlayableMaxs[2])
	{// Outside map bounds, definately cull...
		return qfalse;
	}*/

	if (mins[1] < mapPlayableMins[1] && maxs[1] < mapPlayableMins[1])
	{// Outside map bounds, definately cull...
		return qfalse;
	}

	if (mins[1] > mapPlayableMaxs[1] && maxs[1] > mapPlayableMaxs[1])
	{// Outside map bounds, definately cull...
		return qfalse;
	}

	if (mins[0] < mapPlayableMins[0] && maxs[0] < mapPlayableMins[0])
	{// Outside map bounds, definately cull...
		return qfalse;
	}

	if (mins[0] > mapPlayableMaxs[0] && maxs[0] > mapPlayableMaxs[0])
	{// Outside map bounds, definately cull...
		return qfalse;
	}

	return qtrue;
}

//#define QRAND_MAX 32768
static uint32_t	holdrand = 0x89abcdef;

void Rand_Init(int seed)
{
	holdrand = seed;
}

int irand(int min, int max)
{
	int		result;

	//assert((max - min) < QRAND_MAX);

	max++;
	holdrand = (holdrand * 214013L) + 2531011L;
	result = holdrand >> 17;
	result = ((result * (max - min)) >> 15) + min;
	return(result);
}

#define			MAX_FOREST_MODELS				64
#define			MAX_STATIC_ENTITY_MODELS		64

float			MAP_WATER_LEVEL = -999999.9;

qboolean		FORCED_MODEL_META = qfalse;
qboolean		CAULKIFY_CRAP = qfalse;
qboolean		CULLSIDES_AFTER_MODEL_ADITION = qfalse;

qboolean		ADD_CLIFF_FACES = qfalse;
float			CLIFF_FACES_SCALE = 1.0;
qboolean		CLIFF_FACES_SCALE_XY = qfalse;
float			CLIFF_FACES_CULL_MULTIPLIER = 1.0;
qboolean		CLIFF_CHEAP = qfalse;
char			CLIFF_SHADER[MAX_QPATH] = { 0 };
char			CLIFF_CHECK_SHADER[MAX_QPATH] = { 0 };
qboolean		ADD_LEDGE_FACES = qfalse;
float			LEDGE_FACES_SCALE = 1.0;
qboolean		LEDGE_FACES_SCALE_XY = qfalse;
float			LEDGE_FACES_CULL_MULTIPLIER = 1.0;
float			LEDGE_MIN_SLOPE = 22.0;
float			LEDGE_MAX_SLOPE = 28.0;
qboolean		LEDGE_CHEAP = qfalse;
char			LEDGE_SHADER[MAX_QPATH] = { 0 };
char			LEDGE_CHECK_SHADER[MAX_QPATH] = { 0 };
float			TREE_SCALE_MULTIPLIER = 2.5;
int				TREE_PERCENTAGE = 100;
float			TREE_CLIFF_CULL_RADIUS = 1.0;
char			TREE_MODELS[MAX_FOREST_MODELS][128] = { 0 };
float			TREE_OFFSETS[MAX_FOREST_MODELS] = { -4.0 };
float			TREE_SCALES[MAX_FOREST_MODELS] = { 1.0 };
float			TREE_FORCED_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_BUFFER_DISTANCE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_DISTANCE_FROM_SAME[MAX_FOREST_MODELS] = { 0.0 };
char			TREE_FORCED_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };
qboolean		TREE_FORCED_FULLSOLID[MAX_FOREST_MODELS] = { qfalse };
float			CITY_SCALE_MULTIPLIER = 2.5;
float			CITY_CLIFF_CULL_RADIUS = 1.0;
vec3_t			CITY_LOCATION = { 0 };
float			CITY_RADIUS = 0;
qboolean		CITY_RANDOM_ANGLES = qfalse;
vec3_t			CITY2_LOCATION = { 0 };
float			CITY2_RADIUS = 0;
vec3_t			CITY3_LOCATION = { 0 };
float			CITY3_RADIUS = 0;
vec3_t			CITY4_LOCATION = { 0 };
float			CITY4_RADIUS = 0;
vec3_t			CITY5_LOCATION = { 0 };
float			CITY5_RADIUS = 0;
char			CITY_MODELS[MAX_FOREST_MODELS][128] = { 0 };
float			CITY_OFFSETS[MAX_FOREST_MODELS] = { -4.0 };
float			CITY_SCALES[MAX_FOREST_MODELS] = { 1.0 };
qboolean		CITY_CENTRAL_ONCE[MAX_FOREST_MODELS] = { qfalse };
float			CITY_FORCED_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
float			CITY_FORCED_BUFFER_DISTANCE[MAX_FOREST_MODELS] = { 0.0 };
float			CITY_FORCED_DISTANCE_FROM_SAME[MAX_FOREST_MODELS] = { 0.0 };
char			CITY_FORCED_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };
int				CITY_FORCED_FULLSOLID[MAX_FOREST_MODELS] = { 0 };

char			STATIC_MODEL[MAX_STATIC_ENTITY_MODELS][128] = { 0 };
vec3_t			STATIC_ORIGIN[MAX_STATIC_ENTITY_MODELS] = { 0 };
float			STATIC_ANGLE[MAX_STATIC_ENTITY_MODELS] = { 0 };
float			STATIC_SCALE[MAX_STATIC_ENTITY_MODELS] = { 0 };

void FindWaterLevel(void)
{
	float waterLevel = 999999.9;

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		printLabelledProgress("FindWaterLevel", s, numMapDrawSurfs);

		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		shaderInfo_t *si = ds->shaderInfo;

		if ((si->compileFlags & C_SKIP) || (si->compileFlags & C_NODRAW))
		{
			continue;
		}

		if (ds->mins[2] < waterLevel)
		{

			if (si->compileFlags & C_LIQUID)
			{
				waterLevel = ds->mins[2];
				continue;
			}
			/*
			if (StringContainsWord(si->shader, "water"))
			{
			waterLevel = ds->mins[2];
			continue;
			}
			*/
		}
	}

	if (waterLevel == 999999.9)
	{
		waterLevel = -999999.9;// mapPlayableMins[2];
	}

	MAP_WATER_LEVEL = waterLevel;

	Sys_Printf("Detected lowest map water level at %f.\n", MAP_WATER_LEVEL);
}

void CaulkifyStuff(qboolean findBounds);

void FOLIAGE_LoadClimateData( char *filename )
{
	int i = 0;

	Sys_PrintHeading("--- LoadClimateData ---\n");

	FORCED_MODEL_META = (qboolean)atoi(IniRead(filename, "GENERAL", "forcedModelMeta", "0"));

	if (FORCED_MODEL_META)
	{
		Sys_Printf("Forcing all models to use meta surfaces.\n");
	}

	CAULKIFY_CRAP = (qboolean)atoi(IniRead(filename, "GENERAL", "caulkifyCrap", "0"));

	if (CAULKIFY_CRAP)
	{
		Sys_Printf("Caulkifying some map unneeded surfaces.\n");
	}

	CULLSIDES_AFTER_MODEL_ADITION = (qboolean)atoi(IniRead(filename, "GENERAL", "cullSidesAfterModelAddition", "0"));

	if (CULLSIDES_AFTER_MODEL_ADITION)
	{
		Sys_Printf("Cullsides will be run again after adding models.\n");
	}

	//
	// Cliffs...
	//

	ADD_CLIFF_FACES = (qboolean)atoi(IniRead(filename, "CLIFFS", "addCliffFaces", "0"));
	CLIFF_FACES_SCALE = atof(IniRead(filename, "CLIFFS", "cliffFacesScale", "1.0"));
	CLIFF_FACES_CULL_MULTIPLIER = atof(IniRead(filename, "CLIFFS", "cliffFacesCullScale", "1.0"));

	//CLIFF_FACES_CULL_MULTIPLIER *= CLIFF_FACES_SCALE;

	strcpy(CLIFF_SHADER, IniRead(filename, "CLIFFS", "cliffShader", ""));

	if (CLIFF_SHADER[0] != '\0')
	{
		Sys_Printf("Using climate specified cliff face shader: [%s].\n", CLIFF_SHADER);
	}
	else
	{
		strcpy(CLIFF_SHADER, "models/warzone/rocks/mountainpeak01");
		Sys_Printf("Using default cliff face shader: [%s].\n", CLIFF_SHADER);
	}

	strcpy(CLIFF_CHECK_SHADER, IniRead(filename, "CLIFFS", "cliffApplyToShader", ""));

	if (CLIFF_CHECK_SHADER[0] != '\0')
	{
		Sys_Printf("Applying cliffs only to shader: [%s].\n", CLIFF_CHECK_SHADER);
	}
	else
	{
		strcpy(CLIFF_CHECK_SHADER, "");
		Sys_Printf("Applying cliffs to any shader.\n");
	}

	CLIFF_FACES_SCALE_XY = (qboolean)atoi(IniRead(filename, "CLIFFS", "cliffScaleOnlyXY", "0"));
	CLIFF_CHEAP = (qboolean)atoi(IniRead(filename, "CLIFFS", "cheapCliffs", "0"));

	//
	// Ledges...
	//

	ADD_LEDGE_FACES = (qboolean)atoi(IniRead(filename, "LEDGES", "addLedgeFaces", "0"));
	LEDGE_FACES_SCALE = atof(IniRead(filename, "LEDGES", "ledgeFacesScale", "1.0"));
	LEDGE_FACES_CULL_MULTIPLIER = atof(IniRead(filename, "LEDGES", "ledgeFacesCullScale", "1.0"));

	LEDGE_MIN_SLOPE = atof(IniRead(filename, "LEDGES", "ledgeMinSlope", "22.0"));
	LEDGE_MAX_SLOPE = atof(IniRead(filename, "LEDGES", "ledgeMaxSlope", "28.0"));

	strcpy(LEDGE_SHADER, IniRead(filename, "LEDGES", "ledgeShader", ""));

	if (LEDGE_SHADER[0] != '\0')
	{
		Sys_Printf("Using climate specified ledge face shader: [%s].\n", LEDGE_SHADER);
	}
	else
	{
		strcpy(LEDGE_SHADER, "models/warzone/rocks/ledge");
		Sys_Printf("Using default ledge face shader: [%s].\n", LEDGE_SHADER);
	}

	strcpy(LEDGE_CHECK_SHADER, IniRead(filename, "LEDGES", "ledgeApplyToShader", ""));

	if (LEDGE_CHECK_SHADER[0] != '\0')
	{
		Sys_Printf("Applying ledges only to shader: [%s].\n", LEDGE_CHECK_SHADER);
	}
	else
	{
		strcpy(LEDGE_CHECK_SHADER, "");
		Sys_Printf("Applying ledges to any shader.\n");
	}

	LEDGE_FACES_SCALE_XY = (qboolean)atoi(IniRead(filename, "LEDGES", "ledgeScaleOnlyXY", "0"));



	//
	// Trees...
	//

	// Read all the tree info from the new .climate ini files...
	TREE_SCALE_MULTIPLIER = atof(IniRead(filename, "TREES", "treeScaleMultiplier", "1.0"));

	TREE_PERCENTAGE = atof(IniRead(filename, "TREES", "treeAssignPercent", "100"));

	//Sys_Printf("Tree scale for this climate is %f.\n", TREE_SCALE_MULTIPLIER);

	TREE_CLIFF_CULL_RADIUS = atof(IniRead(filename, "TREES", "cliffFacesCullScale", "1.0"));

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		strcpy(TREE_MODELS[i], IniRead(filename, "TREES", va("treeModel%i", i), ""));
		TREE_OFFSETS[i] = atof(IniRead(filename, "TREES", va("treeZoffset%i", i), "-4.0"));
		TREE_SCALES[i] = atof(IniRead(filename, "TREES", va("treeScale%i", i), "1.0"));
		TREE_FORCED_MAX_ANGLE[i] = atof(IniRead(filename, "TREES", va("treeForcedMaxAngle%i", i), "0.0"));
		TREE_FORCED_BUFFER_DISTANCE[i] = atof(IniRead(filename, "TREES", va("treeForcedBufferDistance%i", i), "0.0"));
		TREE_FORCED_DISTANCE_FROM_SAME[i] = atof(IniRead(filename, "TREES", va("treeForcedDistanceFromSame%i", i), "0.0"));
		TREE_FORCED_FULLSOLID[i] = (qboolean)atoi(IniRead(filename, "TREES", va("treeForcedFullSolid%i", i), "0"));
		strcpy(TREE_FORCED_OVERRIDE_SHADER[i], IniRead(filename, "TREES", va("overrideShader%i", i), ""));

		if (strcmp(TREE_MODELS[i], ""))
			Sys_Printf("Tree %i. Model %s. Offset %f. Scale %f. MaxAngle %i. BufferDist %f. InstanceDist %f. ForcedSolid: %s. Shader: %s.\n", i, TREE_MODELS[i], TREE_OFFSETS[i], TREE_SCALES[i], TREE_FORCED_MAX_ANGLE[i], TREE_FORCED_BUFFER_DISTANCE[i], TREE_FORCED_DISTANCE_FROM_SAME[i], TREE_FORCED_FULLSOLID[i] ? "true" : "false", TREE_FORCED_OVERRIDE_SHADER[i][0] != '\0' ? TREE_FORCED_OVERRIDE_SHADER[i] : "Default");
	}

	// Read all the tree info from the new .climate ini files...
	CITY_SCALE_MULTIPLIER = atof(IniRead(filename, "CITY", "cityScaleMultiplier", "1.0"));
	float CITY_LOCATION_X = atof(IniRead(filename, "CITY", "cityLocationX", "0"));
	float CITY_LOCATION_Y = atof(IniRead(filename, "CITY", "cityLocationY", "0"));
	float CITY_LOCATION_Z = atof(IniRead(filename, "CITY", "cityLocationZ", "0"));

	VectorSet(CITY_LOCATION, CITY_LOCATION_X, CITY_LOCATION_Y, CITY_LOCATION_Z);

	//Sys_Printf("Building scale for this climate is %f.\n", CITY_SCALE_MULTIPLIER);

	if (VectorLength(CITY_LOCATION) != 0.0)
	{
		CITY_RADIUS = atof(IniRead(filename, "CITY", "cityRadius", "0"));
		CITY_CLIFF_CULL_RADIUS = atof(IniRead(filename, "CITY", "cliffFacesCullScale", "1.0"));
		CITY_RANDOM_ANGLES = (atoi(IniRead(filename, "CITY", "cityRandomizeAngles", "0")) > 0) ? qtrue : qfalse;

		for (i = 0; i < MAX_FOREST_MODELS; i++)
		{
			strcpy(CITY_MODELS[i], IniRead(filename, "CITY", va("cityModel%i", i), ""));
			CITY_OFFSETS[i] = atof(IniRead(filename, "CITY", va("cityZoffset%i", i), "-4.0"));
			CITY_SCALES[i] = atof(IniRead(filename, "CITY", va("cityScale%i", i), "1.0"));
			CITY_CENTRAL_ONCE[i] = (qboolean)atoi(IniRead(filename, "CITY", va("cityCentralOneOnly%i", i), "0"));
			CITY_FORCED_MAX_ANGLE[i] = atof(IniRead(filename, "CITY", va("cityForcedMaxAngle%i", i), "0.0"));
			CITY_FORCED_BUFFER_DISTANCE[i] = atof(IniRead(filename, "CITY", va("cityForcedBufferDistance%i", i), "0.0"));
			CITY_FORCED_DISTANCE_FROM_SAME[i] = atof(IniRead(filename, "CITY", va("cityForcedDistanceFromSame%i", i), "0.0"));
			CITY_FORCED_FULLSOLID[i] = atoi(IniRead(filename, "CITY", va("cityForcedFullSolid%i", i), "0"));
			strcpy(CITY_FORCED_OVERRIDE_SHADER[i], IniRead(filename, "CITY", va("overrideShader%i", i), ""));

			if (strcmp(CITY_MODELS[i], ""))
				Sys_Printf("Building %i. Model %s. Offset %f. Scale %f. MaxAngle %i. BufferDist %f. InstanceDist %f. ForcedSolid: %s. Shader: %s. OneOnly: %s.\n", i, CITY_MODELS[i], CITY_OFFSETS[i], CITY_SCALES[i], CITY_FORCED_MAX_ANGLE[i], CITY_FORCED_BUFFER_DISTANCE[i], CITY_FORCED_DISTANCE_FROM_SAME[i], CITY_FORCED_FULLSOLID[i] ? "true" : "false", CITY_FORCED_OVERRIDE_SHADER[i][0] != '\0' ? CITY_FORCED_OVERRIDE_SHADER[i] : "Default", CITY_CENTRAL_ONCE[i] ? "true" : "false");
		}
	}

	float CITY2_LOCATION_X = atof(IniRead(filename, "CITY", "city2LocationX", "0"));
	float CITY2_LOCATION_Y = atof(IniRead(filename, "CITY", "city2LocationY", "0"));
	float CITY2_LOCATION_Z = atof(IniRead(filename, "CITY", "city2LocationZ", "0"));

	VectorSet(CITY2_LOCATION, CITY2_LOCATION_X, CITY2_LOCATION_Y, CITY2_LOCATION_Z);

	if (VectorLength(CITY2_LOCATION) != 0.0)
	{
		CITY2_RADIUS = atof(IniRead(filename, "CITY", "city2Radius", "0"));
	}

	float CITY3_LOCATION_X = atof(IniRead(filename, "CITY", "city3LocationX", "0"));
	float CITY3_LOCATION_Y = atof(IniRead(filename, "CITY", "city3LocationY", "0"));
	float CITY3_LOCATION_Z = atof(IniRead(filename, "CITY", "city3LocationZ", "0"));

	VectorSet(CITY3_LOCATION, CITY3_LOCATION_X, CITY3_LOCATION_Y, CITY3_LOCATION_Z);

	if (VectorLength(CITY3_LOCATION) != 0.0)
	{
		CITY3_RADIUS = atof(IniRead(filename, "CITY", "city3Radius", "0"));
	}

	float CITY4_LOCATION_X = atof(IniRead(filename, "CITY", "city4LocationX", "0"));
	float CITY4_LOCATION_Y = atof(IniRead(filename, "CITY", "city4LocationY", "0"));
	float CITY4_LOCATION_Z = atof(IniRead(filename, "CITY", "city4LocationZ", "0"));

	VectorSet(CITY4_LOCATION, CITY4_LOCATION_X, CITY4_LOCATION_Y, CITY4_LOCATION_Z);

	if (VectorLength(CITY4_LOCATION) != 0.0)
	{
		CITY4_RADIUS = atof(IniRead(filename, "CITY", "city4Radius", "0"));
	}

	float CITY5_LOCATION_X = atof(IniRead(filename, "CITY", "city5LocationX", "0"));
	float CITY5_LOCATION_Y = atof(IniRead(filename, "CITY", "city5LocationY", "0"));
	float CITY5_LOCATION_Z = atof(IniRead(filename, "CITY", "city5LocationZ", "0"));

	VectorSet(CITY5_LOCATION, CITY5_LOCATION_X, CITY5_LOCATION_Y, CITY5_LOCATION_Z);

	if (VectorLength(CITY5_LOCATION) != 0.0)
	{
		CITY5_RADIUS = atof(IniRead(filename, "CITY", "city5Radius", "0"));
	}


	for (i = 0; i < MAX_STATIC_ENTITY_MODELS; i++)
	{
		strcpy(STATIC_MODEL[i], IniRead(filename, "STATIC", va("staticModel%i", i), ""));
		STATIC_ORIGIN[i][0] = atof(IniRead(filename, "STATIC", va("staticOriginX%i", i), "0.0"));
		STATIC_ORIGIN[i][1] = atof(IniRead(filename, "STATIC", va("staticOriginY%i", i), "0.0"));
		STATIC_ORIGIN[i][2] = atof(IniRead(filename, "STATIC", va("staticOriginZ%i", i), "0.0"));
		STATIC_SCALE[i] = atof(IniRead(filename, "STATIC", va("staticScale%i", i), "1.0"));
		STATIC_ANGLE[i] = atof(IniRead(filename, "STATIC", va("staticAngle%i", i), "0.0"));

		if (strcmp(STATIC_MODEL[i], ""))
			Sys_Printf("Static %i. Model %s. Origin %i %i %i. Angle %f. Scale %f.\n", i, STATIC_MODEL[i], (int)STATIC_ORIGIN[i][0], (int)STATIC_ORIGIN[i][1], (int)STATIC_ORIGIN[i][2], (float)STATIC_ANGLE[i], (float)STATIC_SCALE[i]);
	}
}


void vectoangles(const vec3_t value1, vec3_t angles);
extern qboolean StringContainsWord(const char *haystack, const char *needle);

void CaulkifyStuff(qboolean findBounds)
{
	if (!CAULKIFY_CRAP) return;

	Sys_PrintHeading("--- CaulkifyJunk ---\n");

	int numCalkified = 0;
	int numNoDrawAdded = 0;
	int numSkipAdded = 0;
	shaderInfo_t *caulkShader = ShaderInfoForShader("textures/system/caulk");

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		printLabelledProgress("CaulkifyJunk", s, numMapDrawSurfs);

		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		shaderInfo_t *si = ds->shaderInfo;

		if ((si->compileFlags & C_SKIP) && !(si->compileFlags & C_NODRAW))
		{
			si->compileFlags |= C_NODRAW;
			numNoDrawAdded++;
		}

		if (!(si->compileFlags & C_SKIP) && (si->compileFlags & C_NODRAW))
		{
			si->compileFlags |= C_SKIP;
			numSkipAdded++;
		}

		if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
		{
			continue;
		}

		if (!(si->compileFlags & C_SOLID))
		{
			continue;
		}

		if (si == caulkShader)
		{
			continue;
		}

		if (ds->skybox)
		{
			continue;
		}

		if (StringContainsWord(si->shader, "sky"))
		{// Never on skies...
			continue;
		}

		if (StringContainsWord(si->shader, "caulk"))
		{// Already caulk...
			continue;
		}

		if (!(si->compileFlags & C_SKY))
		{
			if (ds->numIndexes == 0 && ds->numVerts == 4)
			{// This is under-terrain junk...
				ds->shaderInfo = caulkShader;
				numCalkified++;
			}
		}
	}

	if (findBounds)
	{
		// Now that we have calkified stuff, re-check playableMapBounds, so we can cull most stuff that would be below the map...
		vec3_t oldMapPlayableMins, oldMapPlayableMaxs;
		VectorCopy(mapPlayableMins, oldMapPlayableMins);
		VectorCopy(mapPlayableMaxs, oldMapPlayableMaxs);

		ClearBounds(mapPlayableMins, mapPlayableMaxs);
		for (int s = 0; s < numMapDrawSurfs; s++)
		{
			printLabelledProgress("ImproveMapBounds", s, numMapDrawSurfs);

			/* get drawsurf */
			mapDrawSurface_t *ds = &mapDrawSurfs[s];
			shaderInfo_t *si = ds->shaderInfo;

			ClearBounds(ds->mins, ds->maxs);
			for (int i = 0; i < ds->numVerts; i++)
				AddPointToBounds(ds->verts[i].xyz, ds->mins, ds->maxs);

			// UQ1: Also record actual map playable area mins/maxs...
			if (!(si->compileFlags & C_SKY)
				&& !(si->compileFlags & C_SKIP)
				&& !(si->compileFlags & C_HINT)
				&& !(si->compileFlags & C_NODRAW)
				&& !StringContainsWord(si->shader, "sky")
				&& !StringContainsWord(si->shader, "caulk")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (!StringContainsWord(si->shader, "/sand"))
				//Sys_Printf("ds %i [%s] bounds %f %f %f x %f %f %f.\n", s, si->shader, ds->mins[0], ds->mins[1], ds->mins[2], ds->maxs[0], ds->maxs[1], ds->maxs[2]);
				AddPointToBounds(ds->mins, mapPlayableMins, mapPlayableMaxs);
				AddPointToBounds(ds->maxs, mapPlayableMins, mapPlayableMaxs);
			}
		}

		// Override playable maxs height with the full map version, we only want the lower extent of playable area...
		mapMaxs[2] = mapPlayableMaxs[2];

		Sys_Printf("Old map bounds %f %f %f x %f %f %f.\n", oldMapPlayableMins[0], oldMapPlayableMins[1], oldMapPlayableMins[2], oldMapPlayableMaxs[0], oldMapPlayableMaxs[1], oldMapPlayableMaxs[2]);
		Sys_Printf("New map bounds %f %f %f x %f %f %f.\n", mapPlayableMins[0], mapPlayableMins[1], mapPlayableMins[2], mapPlayableMaxs[0], mapPlayableMaxs[1], mapPlayableMaxs[2]);
	}

	Sys_Printf("%d shaders set to nodraw.\n", numNoDrawAdded);
	Sys_Printf("%d shaders set to skip.\n", numSkipAdded);
	Sys_Printf("%d of %d surfaces were caulkified.\n", numCalkified, numMapDrawSurfs);
}

#define			FOLIAGE_MAX_FOLIAGES 2097152

int				FOLIAGE_NUM_POSITIONS = 0;
qboolean		*FOLIAGE_ASSIGNED;
vec3_t			FOLIAGE_POSITIONS[FOLIAGE_MAX_FOLIAGES];
vec3_t			FOLIAGE_NORMALS[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_ANGLES[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_TREE_SELECTION[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_SCALE[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_BUFFER[FOLIAGE_MAX_FOLIAGES];

qboolean FOLIAGE_LoadFoliagePositions( char *filename )
{
	FILE			*f;
	int				i = 0;
	int				fileCount = 0;
	int				treeCount = 0;

	Sys_PrintHeading("--- LoadFoliagePositions ---\n");

	f = fopen (filename, "rb");

	if ( !f )
	{
		return qfalse;
	}

	fread( &fileCount, sizeof(int), 1, f );

	for (i = 0; i < fileCount; i++)
	{
		//vec3_t	unneededVec3;
		int		unneededInt;
		float	unneededFloat;

		fread( &FOLIAGE_POSITIONS[treeCount], sizeof(vec3_t), 1, f );
		fread( &FOLIAGE_NORMALS[treeCount], sizeof(vec3_t), 1, f );
		fread( &unneededInt, sizeof(int), 1, f );
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SELECTION[treeCount], sizeof(int), 1, f );
		fread( &FOLIAGE_TREE_ANGLES[treeCount], sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SCALE[treeCount], sizeof(float), 1, f );

		if (FOLIAGE_TREE_SELECTION[treeCount] > 0)
		{// Only keep positions with trees...
			treeCount++;
		}
	}

	FOLIAGE_NUM_POSITIONS = treeCount;

	fclose(f);

	return qtrue;
}

#define M_PI		3.14159265358979323846f

void vectoangles( const vec3_t value1, vec3_t angles ) {
	float	forward;
	float	yaw, pitch;

	if ( value1[1] == 0 && value1[0] == 0 ) {
		yaw = 0;
		if ( value1[2] > 0 ) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if ( value1[0] ) {
			yaw = ( atan2 ( value1[1], value1[0] ) * 180 / M_PI );
		}
		else if ( value1[1] > 0 ) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if ( yaw < 0 ) {
			yaw += 360;
		}

		forward = sqrt ( value1[0]*value1[0] + value1[1]*value1[1] );
		pitch = ( atan2(value1[2], forward) * 180 / M_PI );
		if ( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

int		numCliffs = 0;
int		numDistanceCulled = 0;
vec3_t	cliffPositions[65536];
vec3_t	cliffAngles[65536];
float	cliffScale[65536];
float	cliffHeight[65536];

void GenerateCliffFaces(void)
{
	if (!ADD_CLIFF_FACES) return;

	Sys_PrintHeading("--- GenerateCliffs ---\n");

	numCliffs = 0;
	numDistanceCulled = 0;
	
	//Sys_Printf("%i map draw surfs.\n", numMapDrawSurfs);

	for (int s = 0; s < numMapDrawSurfs /*&& numCliffs < 128*/; s++)
	{
		printLabelledProgress("AddCliffFaces", s, numMapDrawSurfs);

		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		shaderInfo_t *si = ds->shaderInfo;

		if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
		{
			continue;
		}
		
		if (!(si->compileFlags & C_SOLID))
		{
			continue;
		}

		if (CLIFF_CHECK_SHADER[0] != '\0')
		{
			if (strcmp(si->shader, CLIFF_CHECK_SHADER))
			{// Not the mapper specified shader to apply cliffs to, skip...
				continue;
			}
		}

		//Sys_Printf("drawsurf %i. %i indexes. %i verts.\n", s, ds->numIndexes, ds->numVerts);
		
		if (ds->numIndexes == 0 && ds->numVerts == 3)
		{
			qboolean	isCliff = qfalse;
			int			highPositionCount = 0;
			vec3_t		mins, maxs;
			vec3_t		angles[3];

			VectorSet(mins, 999999, 999999, 999999);
			VectorSet(maxs, -999999, -999999, -999999);

			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t		*dv = &ds->verts[j];

				if (dv->xyz[0] < mins[0]) mins[0] = dv->xyz[0];
				if (dv->xyz[1] < mins[1]) mins[1] = dv->xyz[1];
				if (dv->xyz[2] < mins[2]) mins[2] = dv->xyz[2];

				if (dv->xyz[0] > maxs[0]) maxs[0] = dv->xyz[0];
				if (dv->xyz[1] > maxs[1]) maxs[1] = dv->xyz[1];
				if (dv->xyz[2] > maxs[2]) maxs[2] = dv->xyz[2];
			}

			if (mins[0] == 999999 || mins[1] == 999999 || mins[2] == 999999 || maxs[0] == -999999 || maxs[1] == -999999 || maxs[2] == -999999)
			{
				continue;
			}

			if (!BoundsWithinPlayableArea(mins, maxs))
			{
				continue;
			}

			float cliffTop = ((maxs[2] - mins[2])) + mins[2];
			float cliffTopCheck = ((maxs[2] - mins[2]) * 0.7) + mins[2];

			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t		*dv = &ds->verts[j];

				vectoangles(dv->normal, angles[j]);

				float pitch = angles[j][0];

				if (pitch > 180)
					pitch -= 360;

				if (pitch < -180)
					pitch += 360;

				pitch += 90.0f;

				//if ((pitch > 80.0 && pitch < 100.0) || (pitch < -80.0 && pitch > -100.0))
				//	Sys_Printf("pitch %f.\n", pitch);

#define MIN_CLIFF_SLOPE 46.0
				if (pitch == 180.0 || pitch == -180.0)
				{// Horrible hack to skip the surfaces created under the map by q3map2 code... Why are boxes needed for triangles?
					continue;
				}
				else if (pitch > MIN_CLIFF_SLOPE || pitch < -MIN_CLIFF_SLOPE)
				{
					isCliff = qtrue;
				}

				if (dv->xyz[2] >= cliffTopCheck)
					highPositionCount++; // only select positions with 2 triangles up high...
			}

			if (!isCliff /*|| highPositionCount < 2*/)
			{
				continue;
			}

			vec3_t center;
			vec3_t size;
			float smallestSize;

			center[0] = (mins[0] + maxs[0]) * 0.5f;
			center[1] = (mins[1] + maxs[1]) * 0.5f;
			center[2] = cliffTop;// (mins[2] + maxs[2]) * 0.5f;


			size[0] = (maxs[0] - mins[0]);
			size[1] = (maxs[1] - mins[1]);
			size[2] = (maxs[2] - mins[2]);

			smallestSize = size[0];
			if (size[1] < smallestSize) smallestSize = size[1];
			if (size[2] < smallestSize) smallestSize = size[2];

			qboolean bad = qfalse;

			for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
			{// Check if this position is too close to a static model location...
				if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				float dist = Distance(STATIC_ORIGIN[k], center);

				if (dist <= 256.0 * STATIC_SCALE[k])
				{// Not within this static object's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			for (int j = 0; j < numCliffs; j++)
			{
				if (Distance(cliffPositions[j], center) < (48.0 * cliffScale[j]) * CLIFF_FACES_CULL_MULTIPLIER)
				//if ((DistanceHorizontal(cliffPositions[j], center) < (64.0 * cliffScale[j]) * CLIFF_FACES_CULL_MULTIPLIER || DistanceVertical(cliffPositions[j], center) < (192.0 * cliffScale[j]) * CLIFF_FACES_CULL_MULTIPLIER)
				//	&& Distance(cliffPositions[j], center) < (192.0 * cliffScale[j]) * CLIFF_FACES_CULL_MULTIPLIER)
				{
					bad = qtrue;
					numDistanceCulled++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			//Sys_Printf("cliff found at %f %f %f. angles0 %f %f %f. angles1 %f %f %f. angles2 %f %f %f.\n", center[0], center[1], center[2], angles[0][0], angles[0][1], angles[0][2], angles[1][0], angles[1][1], angles[1][2], angles[2][0], angles[2][1], angles[2][2]);

			cliffScale[numCliffs] = (smallestSize * CLIFF_FACES_SCALE) / 64.0;
			cliffHeight[numCliffs] = smallestSize / 64.0;
			VectorCopy(center, cliffPositions[numCliffs]);
			VectorCopy(angles[0], cliffAngles[numCliffs]);
			cliffAngles[numCliffs][0] += 90.0;
			numCliffs++;
		}
	}

	Sys_Printf("Found %i cliff surfaces.\n", numCliffs);
	Sys_Printf("%i cliff objects were culled by distance.\n", numDistanceCulled);

	for (int i = 0; i < numCliffs; i++)
	{
		printLabelledProgress("GenerateCliffs", i, numCliffs);

		const char		*classname, *value;
		float			lightmapScale;
		vec3_t          lightmapAxis;
		int			    smoothNormals;
		int				vertTexProj;
		char			shader[MAX_QPATH];
		shaderInfo_t	*celShader = NULL;
		brush_t			*brush;
		parseMesh_t		*patch;
		qboolean		funcGroup;
		char			castShadows, recvShadows;
		qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
		vec3_t          minlight, minvertexlight, ambient, colormod;
		float           patchQuality, patchSubdivision;

		/* setup */
		entitySourceBrushes = 0;
		mapEnt = &entities[numEntities];
		numEntities++;
		memset(mapEnt, 0, sizeof(*mapEnt));

		mapEnt->mapEntityNum = 0;

		VectorCopy(cliffPositions[i], mapEnt->origin);

		{
			char str[32];
			sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		if (!CLIFF_FACES_SCALE_XY)
		{// Scale X, Y & Z axis...
			char str[32];
			sprintf(str, "%f", cliffScale[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}
		else
		{// Scale X & Y only...
			char str[128];
			sprintf(str, "%f %f %f", cliffScale[i], cliffScale[i], cliffHeight[i]);
			//Sys_Printf("%s\n", str);
			SetKeyValue(mapEnt, "modelscale_vec", str);
		}

		{
			char str[32];
			sprintf(str, "%f", cliffAngles[i][1]-180.0);
			SetKeyValue(mapEnt, "angle", str);
		}

		if (CLIFF_SHADER[0] != '\0')
		{
			SetKeyValue(mapEnt, "_overrideShader", CLIFF_SHADER);
		}

		

		/* ydnar: get classname */
		SetKeyValue(mapEnt, "classname", "misc_model");
		classname = ValueForKey(mapEnt, "classname");

		if (CLIFF_CHEAP)
			SetKeyValue(mapEnt, "model", va("models/warzone/rocks/cliffface0%i.md3", irand(4, 5)));
		else
			SetKeyValue(mapEnt, "model", va("models/warzone/rocks/cliffface0%i.md3", irand(1,5)));

		//Sys_Printf( "Generated cliff face at %f %f %f. Angle %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], cliffAngles[i][1] );

		funcGroup = qfalse;

		/* get explicit shadow flags */
		GetEntityShadowFlags(mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse);

		/* vortex: get lightmap scaling value for this entity */
		GetEntityLightmapScale(mapEnt, &lightmapScale, 0);

		/* vortex: get lightmap axis for this entity */
		GetEntityLightmapAxis(mapEnt, lightmapAxis, NULL);

		/* vortex: per-entity normal smoothing */
		GetEntityNormalSmoothing(mapEnt, &smoothNormals, 0);

		/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
		GetEntityMinlightAmbientColor(mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue);
		if (mapEnt == &entities[0])
		{
			/* worldspawn have it empty, since it's keys sets global parms */
			VectorSet(minlight, 0, 0, 0);
			VectorSet(minvertexlight, 0, 0, 0);
			VectorSet(ambient, 0, 0, 0);
			VectorSet(colormod, 1, 1, 1);
		}

		/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
		GetEntityPatchMeta(mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

		/* vortex: vertical texture projection */
		if (strcmp("", ValueForKey(mapEnt, "_vtcproj")) || strcmp("", ValueForKey(mapEnt, "_vp")))
		{
			vertTexProj = IntForKey(mapEnt, "_vtcproj");
			if (vertTexProj <= 0.0f)
				vertTexProj = IntForKey(mapEnt, "_vp");
		}
		else
			vertTexProj = 0;

		/* ydnar: get cel shader :) for this entity */
		value = ValueForKey(mapEnt, "_celshader");

		if (value[0] == '\0')
			value = ValueForKey(&entities[0], "_celshader");

		if (value[0] != '\0')
		{
			sprintf(shader, "textures/%s", value);
			celShader = ShaderInfoForShader(shader);
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
		for (brush = mapEnt->brushes; brush != NULL; brush = brush->next)
		{
			brush->entityNum = mapEnt->mapEntityNum;
			brush->mapEntityNum = mapEnt->mapEntityNum;
			brush->castShadows = castShadows;
			brush->recvShadows = recvShadows;
			brush->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, brush->lightmapAxis); /* vortex */
			brush->smoothNormals = smoothNormals; /* vortex */
			brush->noclip = forceNoClip; /* vortex */
			brush->noTJunc = forceNoTJunc; /* vortex */
			brush->vertTexProj = vertTexProj; /* vortex */
			VectorCopy(minlight, brush->minlight); /* vortex */
			VectorCopy(minvertexlight, brush->minvertexlight); /* vortex */
			VectorCopy(ambient, brush->ambient); /* vortex */
			VectorCopy(colormod, brush->colormod); /* vortex */
			brush->celShader = celShader;
			if (forceNonSolid == qtrue)
			{
				brush->detail = qtrue;
				brush->nonsolid = qtrue;
				brush->noclip = qtrue;
			}
		}

		for (patch = mapEnt->patches; patch != NULL; patch = patch->next)
		{
			patch->entityNum = mapEnt->mapEntityNum;
			patch->mapEntityNum = mapEnt->mapEntityNum;
			patch->castShadows = castShadows;
			patch->recvShadows = recvShadows;
			patch->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, patch->lightmapAxis); /* vortex */
			patch->smoothNormals = smoothNormals; /* vortex */
			patch->vertTexProj = vertTexProj; /* vortex */
			patch->celShader = celShader;
			patch->patchMeta = forceMeta; /* vortex */
			patch->patchQuality = patchQuality; /* vortex */
			patch->patchSubdivisions = patchSubdivision; /* vortex */
			VectorCopy(minlight, patch->minlight); /* vortex */
			VectorCopy(minvertexlight, patch->minvertexlight); /* vortex */
			VectorCopy(ambient, patch->ambient); /* vortex */
			VectorCopy(colormod, patch->colormod); /* vortex */
			patch->nonsolid = forceNonSolid;
		}

		/* vortex: store map entity num */
		{
			char buf[32];
			sprintf(buf, "%i", mapEnt->mapEntityNum);
			SetKeyValue(mapEnt, "_mapEntityNum", buf);
		}

		/* ydnar: gs mods: set entity bounds */
		SetEntityBounds(mapEnt);

		/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
		LoadEntityIndexMap(mapEnt);

		/* get entity origin and adjust brushes */
		GetVectorForKey(mapEnt, "origin", mapEnt->origin);
		if (mapEnt->originbrush_origin[0] || mapEnt->originbrush_origin[1] || mapEnt->originbrush_origin[2])
			AdjustBrushesForOrigin(mapEnt);


#if defined(__ADD_TREES_EARLY__)
		AddTriangleModels(0, qtrue, qtrue);
		EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
		//MoveBrushesToWorld( mapEnt );
		numEntities--;
#endif
	}
}

int			numLedges = 0;
int			numLedgesDistanceCulled = 0;
vec3_t		ledgePositions[65536];
vec3_t		ledgeAngles[65536];
float		ledgeScale[65536];
float		ledgeHeight[65536];
int			ledgeIsLowAngle[65536];

void GenerateLedgeFaces(void)
{
	if (!ADD_LEDGE_FACES) return;

	Sys_PrintHeading("--- GenerateLedges ---\n");

	numLedges = 0;
	numLedgesDistanceCulled = 0;
	
	int numLowAngleLedges = 0;
	int numVeryLowAngleLedges = 0;

	//Sys_Printf("%i map draw surfs.\n", numMapDrawSurfs);

	for (int s = 0; s < numMapDrawSurfs /*&& numLedges < 128*/; s++)
	{
		printLabelledProgress("AddLedgeFaces", s, numMapDrawSurfs);

		/* get drawsurf */
		mapDrawSurface_t *ds = &mapDrawSurfs[s];
		shaderInfo_t *si = ds->shaderInfo;
		int isLowAngle = 0;

		if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
		{
			continue;
		}

		if (!(si->compileFlags & C_SOLID))
		{
			continue;
		}

		if (ds->maxs[2] < MAP_WATER_LEVEL)
		{// Don't add ledges under water to save on brushes and planes...
			continue;
		}

		if (LEDGE_CHECK_SHADER[0] != '\0')
		{
			if (strcmp(si->shader, LEDGE_CHECK_SHADER))
			{// Not the mapper specified shader to apply ledges to, skip...
				continue;
			}
		}

		//Sys_Printf("drawsurf %i. %i indexes. %i verts.\n", s, ds->numIndexes, ds->numVerts);

		if (ds->numIndexes == 0 && ds->numVerts == 3)
		{
			qboolean	isCliff = qfalse;
			int			lowPositionCount = 0;
			vec3_t		mins, maxs;
			vec3_t		angles[3];

			VectorSet(mins, 999999, 999999, 999999);
			VectorSet(maxs, -999999, -999999, -999999);

			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t		*dv = &ds->verts[j];

				if (dv->xyz[0] < mins[0]) mins[0] = dv->xyz[0];
				if (dv->xyz[1] < mins[1]) mins[1] = dv->xyz[1];
				if (dv->xyz[2] < mins[2]) mins[2] = dv->xyz[2];

				if (dv->xyz[0] > maxs[0]) maxs[0] = dv->xyz[0];
				if (dv->xyz[1] > maxs[1]) maxs[1] = dv->xyz[1];
				if (dv->xyz[2] > maxs[2]) maxs[2] = dv->xyz[2];
			}

			if (mins[0] == 999999 || mins[1] == 999999 || mins[2] == 999999 || maxs[0] == -999999 || maxs[1] == -999999 || maxs[2] == -999999)
			{
				continue;
			}

			if (!BoundsWithinPlayableArea(mins, maxs))
			{
				continue;
			}

			float cliffTop = ((maxs[2] - mins[2])) + mins[2];
			float cliffTopCheck = ((maxs[2] - mins[2]) * 0.7) + mins[2];

			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t		*dv = &ds->verts[j];

				vectoangles(dv->normal, angles[j]);

				float pitch = angles[j][0];

				if (pitch > 180)
					pitch -= 360;

				if (pitch < -180)
					pitch += 360;

				pitch += 90.0f;

				//if ((pitch > 80.0 && pitch < 100.0) || (pitch < -80.0 && pitch > -100.0))
				//	Sys_Printf("pitch %f.\n", pitch);

				if (pitch == 180.0 || pitch == -180.0)
				{// Horrible hack to skip the surfaces created under the map by q3map2 code... Why are boxes needed for triangles?
					continue;
				}
				else if ((pitch > LEDGE_MIN_SLOPE && pitch < LEDGE_MAX_SLOPE) || (pitch < -LEDGE_MIN_SLOPE && pitch > -LEDGE_MAX_SLOPE))
				{
					isCliff = qtrue;
				}

				if (isCliff && !(pitch > 20 || pitch < -20))
				{// Mark this as a low angle slope, so we can lower the ledge a bit when adding it. This should allow for some ledges on flattish surfaces...
					if (!(pitch > 16 || pitch < -16))
					{
						isLowAngle = 1;
					}
					else
					{
						isLowAngle = 2;
					}
				}

				if (!(dv->xyz[2] >= cliffTopCheck))
					lowPositionCount++; // only select positions with 2 triangles down low...
			}

			if (!isCliff || lowPositionCount < 2)
			{
				continue;
			}

			vec3_t center;
			vec3_t size;
			float smallestSize;

			center[0] = (mins[0] + maxs[0]) * 0.5f;
			center[1] = (mins[1] + maxs[1]) * 0.5f;
			center[2] = (mins[2] + maxs[2]) * 0.5f;

			qboolean bad = qfalse;

			for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
			{// Check if this position is too close to a static model location...
				if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				float dist = Distance(STATIC_ORIGIN[k], center);

				if (dist <= 256.0 * STATIC_SCALE[k])
				{// Not within this static object's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}


			size[0] = (maxs[0] - mins[0]);
			size[1] = (maxs[1] - mins[1]);
			size[2] = (maxs[2] - mins[2]);

			smallestSize = size[0];
			if (size[1] < smallestSize) smallestSize = size[1];
			if (size[2] < smallestSize) smallestSize = size[2];

			for (int j = 0; j < numLedges; j++)
			{
				if (Distance(ledgePositions[j], center) < (48.0 * ledgeScale[j]) * LEDGE_FACES_CULL_MULTIPLIER)
					//if ((DistanceHorizontal(ledgePositions[j], center) < (64.0 * ledgeScale[j]) * CLIFF_FACES_CULL_MULTIPLIER || DistanceVertical(ledgePositions[j], center) < (192.0 * ledgeScale[j]) * CLIFF_FACES_CULL_MULTIPLIER)
					//	&& Distance(ledgePositions[j], center) < (192.0 * ledgeScale[j]) * CLIFF_FACES_CULL_MULTIPLIER)
				{
					bad = qtrue;
					numLedgesDistanceCulled++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			//Sys_Printf("cliff found at %f %f %f. angles0 %f %f %f. angles1 %f %f %f. angles2 %f %f %f.\n", center[0], center[1], center[2], angles[0][0], angles[0][1], angles[0][2], angles[1][0], angles[1][1], angles[1][2], angles[2][0], angles[2][1], angles[2][2]);

			ledgeScale[numLedges] = (smallestSize * LEDGE_FACES_SCALE) / 64.0;
			ledgeHeight[numLedges] = smallestSize / 64.0;
			VectorCopy(center, ledgePositions[numLedges]);
			VectorCopy(angles[0], ledgeAngles[numLedges]);
			ledgeAngles[numLedges][0] += 90.0;
			ledgeIsLowAngle[numLedges] = isLowAngle;

			if (isLowAngle == 1)
			{
				numLowAngleLedges++;
			}
			else if (isLowAngle == 2)
			{
				numVeryLowAngleLedges++;
			}

			numLedges++;
		}
	}

	Sys_Printf("Found %i ledge surfaces.\n", numLedges);
	Sys_Printf("%i ledge slopes are low angles.\n", numLowAngleLedges);
	Sys_Printf("%i ledge slopes are very low angles.\n", numVeryLowAngleLedges);
	Sys_Printf("%i ledge objects were culled by distance.\n", numLedgesDistanceCulled);

	for (int i = 0; i < numLedges; i++)
	{
		printLabelledProgress("GenerateLedges", i, numLedges);

		const char		*classname, *value;
		float			lightmapScale;
		vec3_t          lightmapAxis;
		int			    smoothNormals;
		int				vertTexProj;
		char			shader[MAX_QPATH];
		shaderInfo_t	*celShader = NULL;
		brush_t			*brush;
		parseMesh_t		*patch;
		qboolean		funcGroup;
		char			castShadows, recvShadows;
		qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
		vec3_t          minlight, minvertexlight, ambient, colormod;
		float           patchQuality, patchSubdivision;

		/* setup */
		entitySourceBrushes = 0;
		mapEnt = &entities[numEntities];
		numEntities++;
		memset(mapEnt, 0, sizeof(*mapEnt));

		mapEnt->mapEntityNum = 0;

		VectorCopy(ledgePositions[i], mapEnt->origin);

		{
			char str[32];

			if (ledgeIsLowAngle[i] == 1)
			{// Special case for low angle slopes... Dig it further into the ground more...
				sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]-32.0);
			}
			else if (ledgeIsLowAngle[i] == 2)
			{// Special case for *very* low angle slopes... Dig it further into the ground more...
				sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] - 48.0);
			}
			else
			{
				sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			}
			SetKeyValue(mapEnt, "origin", str);
		}

		if (!LEDGE_FACES_SCALE_XY)
		{// Scale X, Y & Z axis...
			char str[32];
			sprintf(str, "%f", ledgeScale[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}
		else
		{// Scale X & Y only...
			char str[128];
			sprintf(str, "%f %f %f", ledgeScale[i], ledgeScale[i], ledgeHeight[i]);
			//Sys_Printf("%s\n", str);
			SetKeyValue(mapEnt, "modelscale_vec", str);
		}

		{
			char str[32];
			sprintf(str, "%f", ledgeAngles[i][1] - 180.0);
			SetKeyValue(mapEnt, "angle", str);
		}


		/* ydnar: get classname */
		SetKeyValue(mapEnt, "classname", "misc_model");
		classname = ValueForKey(mapEnt, "classname");

		if (ledgeIsLowAngle[i] > 1)
		{// Can use model 4...
			int choice = irand(1, 4);

			SetKeyValue(mapEnt, "model", va("models/warzone/rocks/ledge0%i.md3", choice));

			if (LEDGE_SHADER[0] != '\0')
			{// Override the whole model's shader...
				SetKeyValue(mapEnt, "_overrideShader", LEDGE_SHADER);
			}
		}
		else
		{// Can't use model 4, it requires a very low angle slope...
			int choice = irand(1, 3);

			SetKeyValue(mapEnt, "model", va("models/warzone/rocks/ledge0%i.md3", choice));

			if (LEDGE_SHADER[0] != '\0')
			{// Override the whole model's shader...
				SetKeyValue(mapEnt, "_overrideShader", LEDGE_SHADER);
			}
		}

		//Sys_Printf( "Generated cliff face at %f %f %f. Angle %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], ledgeAngles[i][1] );

		funcGroup = qfalse;

		/* get explicit shadow flags */
		GetEntityShadowFlags(mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse);

		/* vortex: get lightmap scaling value for this entity */
		GetEntityLightmapScale(mapEnt, &lightmapScale, 0);

		/* vortex: get lightmap axis for this entity */
		GetEntityLightmapAxis(mapEnt, lightmapAxis, NULL);

		/* vortex: per-entity normal smoothing */
		GetEntityNormalSmoothing(mapEnt, &smoothNormals, 0);

		/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
		GetEntityMinlightAmbientColor(mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue);
		if (mapEnt == &entities[0])
		{
			/* worldspawn have it empty, since it's keys sets global parms */
			VectorSet(minlight, 0, 0, 0);
			VectorSet(minvertexlight, 0, 0, 0);
			VectorSet(ambient, 0, 0, 0);
			VectorSet(colormod, 1, 1, 1);
		}

		/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
		GetEntityPatchMeta(mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

		/* vortex: vertical texture projection */
		if (strcmp("", ValueForKey(mapEnt, "_vtcproj")) || strcmp("", ValueForKey(mapEnt, "_vp")))
		{
			vertTexProj = IntForKey(mapEnt, "_vtcproj");
			if (vertTexProj <= 0.0f)
				vertTexProj = IntForKey(mapEnt, "_vp");
		}
		else
			vertTexProj = 0;

		/* ydnar: get cel shader :) for this entity */
		value = ValueForKey(mapEnt, "_celshader");

		if (value[0] == '\0')
			value = ValueForKey(&entities[0], "_celshader");

		if (value[0] != '\0')
		{
			sprintf(shader, "textures/%s", value);
			celShader = ShaderInfoForShader(shader);
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
		for (brush = mapEnt->brushes; brush != NULL; brush = brush->next)
		{
			brush->entityNum = mapEnt->mapEntityNum;
			brush->mapEntityNum = mapEnt->mapEntityNum;
			brush->castShadows = castShadows;
			brush->recvShadows = recvShadows;
			brush->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, brush->lightmapAxis); /* vortex */
			brush->smoothNormals = smoothNormals; /* vortex */
			brush->noclip = forceNoClip; /* vortex */
			brush->noTJunc = forceNoTJunc; /* vortex */
			brush->vertTexProj = vertTexProj; /* vortex */
			VectorCopy(minlight, brush->minlight); /* vortex */
			VectorCopy(minvertexlight, brush->minvertexlight); /* vortex */
			VectorCopy(ambient, brush->ambient); /* vortex */
			VectorCopy(colormod, brush->colormod); /* vortex */
			brush->celShader = celShader;
			if (forceNonSolid == qtrue)
			{
				brush->detail = qtrue;
				brush->nonsolid = qtrue;
				brush->noclip = qtrue;
			}
		}

		for (patch = mapEnt->patches; patch != NULL; patch = patch->next)
		{
			patch->entityNum = mapEnt->mapEntityNum;
			patch->mapEntityNum = mapEnt->mapEntityNum;
			patch->castShadows = castShadows;
			patch->recvShadows = recvShadows;
			patch->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, patch->lightmapAxis); /* vortex */
			patch->smoothNormals = smoothNormals; /* vortex */
			patch->vertTexProj = vertTexProj; /* vortex */
			patch->celShader = celShader;
			patch->patchMeta = forceMeta; /* vortex */
			patch->patchQuality = patchQuality; /* vortex */
			patch->patchSubdivisions = patchSubdivision; /* vortex */
			VectorCopy(minlight, patch->minlight); /* vortex */
			VectorCopy(minvertexlight, patch->minvertexlight); /* vortex */
			VectorCopy(ambient, patch->ambient); /* vortex */
			VectorCopy(colormod, patch->colormod); /* vortex */
			patch->nonsolid = forceNonSolid;
		}

		/* vortex: store map entity num */
		{
			char buf[32];
			sprintf(buf, "%i", mapEnt->mapEntityNum);
			SetKeyValue(mapEnt, "_mapEntityNum", buf);
		}

		/* ydnar: gs mods: set entity bounds */
		SetEntityBounds(mapEnt);

		/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
		LoadEntityIndexMap(mapEnt);

		/* get entity origin and adjust brushes */
		GetVectorForKey(mapEnt, "origin", mapEnt->origin);
		if (mapEnt->originbrush_origin[0] || mapEnt->originbrush_origin[1] || mapEnt->originbrush_origin[2])
			AdjustBrushesForOrigin(mapEnt);


#if defined(__ADD_TREES_EARLY__)
		AddTriangleModels(0, qtrue, qtrue);
		EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
		//MoveBrushesToWorld( mapEnt );
		numEntities--;
#endif
	}
}

void ReassignTreeModels ( void )
{
	int				i;
	int				NUM_PLACED[MAX_FOREST_MODELS] = { 0 };
	float			*BUFFER_RANGES;
	float			*SAME_RANGES;
	int				POSSIBLES[MAX_FOREST_MODELS] = { 0 };
	float			POSSIBLES_BUFFERS[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_SAME_RANGES[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
	int				NUM_POSSIBLES = 0;
	int				NUM_CLOSE_CLIFFS = 0;

	FOLIAGE_ASSIGNED = (qboolean*)malloc(sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	BUFFER_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	SAME_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	memset(FOLIAGE_ASSIGNED, qfalse, sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	memset(BUFFER_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	memset(SAME_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	Rand_Init(FOLIAGE_POSITIONS[0][0]);
	
	//Sys_Printf("Finding angles models.\n");

	// Find non-angle models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (TREE_FORCED_MAX_ANGLE[i] == 0.0)
		{
			continue;
		}

		if (!strcmp(TREE_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = TREE_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_SAME_RANGES[NUM_POSSIBLES] = TREE_FORCED_DISTANCE_FROM_SAME[i];
		POSSIBLES_MAX_ANGLE[NUM_POSSIBLES] = TREE_FORCED_MAX_ANGLE[i];
		NUM_POSSIBLES++;
	}

	Sys_Printf("Restricted angle possibles:\n");
	for (i = 0; i < NUM_POSSIBLES; i++)
	{
		Sys_Printf("%d - %s.\n", i, TREE_MODELS[POSSIBLES[i]]);
	}

	//Sys_Printf("Assign angles models from %i possibles.\n", NUM_POSSIBLES);

	// First add any non-steep options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			float pitch;
			qboolean bad = qfalse;
			int j;

			printLabelledProgress("RandomizeNoAngleModels", i, FOLIAGE_NUM_POSITIONS);

			if (FOLIAGE_ASSIGNED[i])
			{
				continue;
			}

			for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
			{// Check if this position is too close to a static model location...
				if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				float dist = Distance(STATIC_ORIGIN[k], FOLIAGE_POSITIONS[i]);

				if (dist <= 256.0 * STATIC_SCALE[k])
				{// Not within this static object's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (j == i)
				{
					continue;
				}

				if (!FOLIAGE_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUFFER_RANGES[j] || dist <= FOLIAGE_TREE_BUFFER[j] || dist <= BUFFER_RANGES[i] || dist <= FOLIAGE_TREE_BUFFER[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY_LOCATION, FOLIAGE_POSITIONS[i]);
				
				if (dist <= CITY_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY2_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY2_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY3_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY3_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY4_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY4_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY5_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY5_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			vec3_t angles;
			vectoangles( FOLIAGE_NORMALS[i], angles );
			pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;

			if (pitch > POSSIBLES_MAX_ANGLE[selected] || pitch < -POSSIBLES_MAX_ANGLE[selected])
			{// Bad slope...
				//Sys_Printf("Position %i angles too great (%f).\n", i, pitch);
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * TREE_CLIFF_CULL_RADIUS
					/*|| DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 256.0 * TREE_CLIFF_CULL_RADIUS*/)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			//Sys_Printf("Position %i angles OK! (%f).\n", i, pitch);

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			FOLIAGE_TREE_BUFFER[i] = BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
			FOLIAGE_ASSIGNED[i] = qtrue;
			NUM_PLACED[POSSIBLES[selected]]++;
		}
	}

	NUM_POSSIBLES = 0;

	//Sys_Printf("Finding other models.\n");

	// Find other models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (TREE_FORCED_MAX_ANGLE[i] != 0.0)
		{
			continue;
		}

		if (!strcmp(TREE_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = TREE_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_SAME_RANGES[NUM_POSSIBLES] = TREE_FORCED_DISTANCE_FROM_SAME[i];
		NUM_POSSIBLES++;
	}

	//Sys_Printf("Assign other models from %i possibles.\n", NUM_POSSIBLES);

	Sys_Printf("Non restricted angle possibles:\n");
	for (i = 0; i < NUM_POSSIBLES; i++)
	{
		Sys_Printf("%d - %s.\n", i, TREE_MODELS[POSSIBLES[i]]);
	}

	// Now add other options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			qboolean bad = qfalse;
			int tries = 0;
			int j;

			printLabelledProgress("RandomizeModels", i, FOLIAGE_NUM_POSITIONS);

			if (FOLIAGE_ASSIGNED[i])
			{
				continue;
			}

			for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
			{// Check if this position is too close to a static model location...
				if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				float dist = Distance(STATIC_ORIGIN[k], FOLIAGE_POSITIONS[i]);

				if (dist <= 256.0 * STATIC_SCALE[k])
				{// Not within this static object's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (tries > 32)
				{
					bad = qtrue;
					break;
				}

				if (j == i)
				{
					continue;
				}

				if (!FOLIAGE_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUFFER_RANGES[j] || dist <= FOLIAGE_TREE_BUFFER[j] || dist <= BUFFER_RANGES[i] || dist <= FOLIAGE_TREE_BUFFER[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					selected = irand(0, NUM_POSSIBLES - 1);
					tries++;
				}

				dist = Distance(CITY_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY2_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY2_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY3_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY3_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY4_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY4_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY5_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY5_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
				/*Sys_Printf("cliff %f %f %f. possible %f %f %f. Distance %f.\n", cliffPositions[z][0], cliffPositions[z][1], cliffPositions[z][2]
					, FOLIAGE_POSITIONS[POSSIBLES[selected]][0], FOLIAGE_POSITIONS[POSSIBLES[selected]][1], FOLIAGE_POSITIONS[POSSIBLES[selected]][2],
					DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[POSSIBLES[selected]]));
				*/

				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * TREE_CLIFF_CULL_RADIUS
					/*|| DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 256.0 * TREE_CLIFF_CULL_RADIUS*/)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			FOLIAGE_TREE_BUFFER[i] = BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
			FOLIAGE_ASSIGNED[i] = qtrue;
			NUM_PLACED[POSSIBLES[selected]]++;
		}
	}

	free(BUFFER_RANGES);
	free(SAME_RANGES);

	for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
	{// Now check our percentage of how many we should actually use... Disable extras...
		if (FOLIAGE_ASSIGNED[i])
		{
			if (irand(0, 100) > TREE_PERCENTAGE)
			{
				FOLIAGE_ASSIGNED[i] = qfalse;
				FOLIAGE_TREE_SELECTION[i] = 0;
				FOLIAGE_TREE_BUFFER[i] = 0;
			}
		}
	}

	int count = 0;

	for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
	{
		if (FOLIAGE_ASSIGNED[i]) 
		{
			/*if (TREE_FORCED_MAX_ANGLE[FOLIAGE_TREE_SELECTION[i]] != 0.0)
				Sys_Printf("Model %i was reassigned as %s. [MAX_ANGLE_MODEL]\n", i, TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]);
			else
				Sys_Printf("Model %i was reassigned as %s.\n", i, TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]);*/

			count++;
		}
	}

	Sys_Printf("%9d of %i positions successfully reassigned to new models.\n", count, FOLIAGE_NUM_POSITIONS-1);

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (!strcmp(TREE_MODELS[i], ""))
		{
			continue;
		}

		Sys_Printf("%9d placements of model %s.\n", NUM_PLACED[i], TREE_MODELS[i]);
	}

	Sys_Printf("%9d positions ignored due to closeness to a cliff.\n", NUM_CLOSE_CLIFFS);
}

//#define __ADD_TREES_EARLY__ // Add trees to map's brush list instanty...

extern void MoveBrushesToWorld( entity_t *ent );
extern qboolean StringContainsWord(const char *haystack, const char *needle);

void GenerateMapForest ( void )
{
	if (generateforest)
	{
		if (FOLIAGE_NUM_POSITIONS > 0)
		{// First re-assign model types based on buffer ranges and instance type ranges...
			int i;

			Sys_PrintHeading("--- GenerateMapForest ---\n");

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf( "Adding %i trees to bsp.\n", FOLIAGE_NUM_POSITIONS );
#endif

			ReassignTreeModels();

			for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
			{
				printLabelledProgress("GenerateMapForest", i, FOLIAGE_NUM_POSITIONS);

				if (!FOLIAGE_ASSIGNED[i]) 
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

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

				/* setup */
				entitySourceBrushes = 0;
				mapEnt = &entities[ numEntities ];
				numEntities++;
				memset( mapEnt, 0, sizeof( *mapEnt ) );

				mapEnt->mapEntityNum = 0;

				//mapEnt->mapEntityNum = numMapEntities;
				//numMapEntities++;

				//mapEnt->mapEntityNum = 0 - numMapEntities;

				//mapEnt->forceSubmodel = qtrue;

				VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
				mapEnt->origin[2] += TREE_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

				{
					char str[32];
					sprintf( str, "%f %f %f", mapEnt->origin[ 0 ], mapEnt->origin[ 1 ], mapEnt->origin[ 2 ] );
					SetKeyValue( mapEnt, "origin", str );
				}

				{
					char str[32];
					sprintf( str, "%f", FOLIAGE_TREE_SCALE[i]*2.0*TREE_SCALE_MULTIPLIER*TREE_SCALES[FOLIAGE_TREE_SELECTION[i]] );
					SetKeyValue( mapEnt, "modelscale", str );
				}

				{
					char str[32];
					sprintf( str, "%f", FOLIAGE_TREE_ANGLES[i] );
					SetKeyValue( mapEnt, "angle", str );
				}

				if (TREE_FORCED_FULLSOLID[FOLIAGE_TREE_SELECTION[i]])
				{
					SetKeyValue(mapEnt, "_forcedSolid", "1");
				}
				else
				{
					SetKeyValue(mapEnt, "_forcedSolid", "0");
				}

				if (TREE_FORCED_OVERRIDE_SHADER[FOLIAGE_TREE_SELECTION[i]] != '\0')
				{
					SetKeyValue(mapEnt, "_overrideShader", TREE_FORCED_OVERRIDE_SHADER[FOLIAGE_TREE_SELECTION[i]]);
				}

				/*{
				char str[32];
				vec3_t angles;
				vectoangles( FOLIAGE_NORMALS[i], angles );
				angles[PITCH] += 90;
				angles[YAW] = 270.0 - FOLIAGE_TREE_ANGLES[i];
				sprintf( str, "%f %f %f", angles[0], angles[1], angles[2] );
				SetKeyValue( mapEnt, "angles", str );
				}*/

				/*{
				char str[32];
				sprintf( str, "%i", 2 );
				SetKeyValue( mapEnt, "spawnflags", str );
				}*/

				//Sys_Printf( "Generated tree at %f %f %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


				/* ydnar: get classname */
				SetKeyValue( mapEnt, "classname", "misc_model");
				classname = ValueForKey( mapEnt, "classname" );

				SetKeyValue( mapEnt, "model", TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]); // test tree

				//Sys_Printf( "Generated tree at %f %f %f. Model %s.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], TROPICAL_TREES[FOLIAGE_TREE_SELECTION[i]] );

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


#if defined(__ADD_TREES_EARLY__)
				AddTriangleModels( 0, qtrue, qtrue );
				EmitBrushes( mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes );
				//MoveBrushesToWorld( mapEnt );
				numEntities--;
#endif
			}

			free(FOLIAGE_ASSIGNED);

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf( "Finished adding trees.\n" );
#endif
		}
	}
}

void GenerateStaticEntities(void)
{
	int i;

	Sys_PrintHeading("--- GenerateStaticEntities ---\n");

	for (i = 0; i < MAX_STATIC_ENTITY_MODELS; i++)
	{
		printLabelledProgress("GenerateStaticEntities", i, MAX_STATIC_ENTITY_MODELS);

		if (STATIC_MODEL[i][0] == 0 || strlen(STATIC_MODEL[i]) <= 0)
		{// Was not assigned a model... Must be too close to something else...
			continue;
		}

		const char		*classname, *value;
		float			lightmapScale;
		vec3_t          lightmapAxis;
		int			    smoothNormals;
		int				vertTexProj;
		char			shader[MAX_QPATH];
		shaderInfo_t	*celShader = NULL;
		brush_t			*brush;
		parseMesh_t		*patch;
		qboolean		funcGroup;
		char			castShadows, recvShadows;
		qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
		vec3_t          minlight, minvertexlight, ambient, colormod;
		float           patchQuality, patchSubdivision;

		/* setup */
		entitySourceBrushes = 0;
		mapEnt = &entities[numEntities];
		numEntities++;
		memset(mapEnt, 0, sizeof(*mapEnt));

		mapEnt->mapEntityNum = 0;

		VectorCopy(STATIC_ORIGIN[i], mapEnt->origin);

		{
			char str[32];
			sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		{
			char str[32];
			sprintf(str, "%f", STATIC_SCALE[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}

		{
			char str[32];
			sprintf(str, "%f", STATIC_ANGLE[i]);
			SetKeyValue(mapEnt, "angle", str);
		}

		SetKeyValue(mapEnt, "_forcedSolid", "1");
		//SetKeyValue(mapEnt, "_forcedSolid", "0");


		//Sys_Printf( "Generated mapent at %f %f %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


		/* ydnar: get classname */
		SetKeyValue(mapEnt, "classname", "misc_model");
		classname = ValueForKey(mapEnt, "classname");

		SetKeyValue(mapEnt, "model", STATIC_MODEL[i]);

		funcGroup = qfalse;

		/* get explicit shadow flags */
		GetEntityShadowFlags(mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse);

		/* vortex: get lightmap scaling value for this entity */
		GetEntityLightmapScale(mapEnt, &lightmapScale, 0);

		/* vortex: get lightmap axis for this entity */
		GetEntityLightmapAxis(mapEnt, lightmapAxis, NULL);

		/* vortex: per-entity normal smoothing */
		GetEntityNormalSmoothing(mapEnt, &smoothNormals, 0);

		/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
		GetEntityMinlightAmbientColor(mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue);
		if (mapEnt == &entities[0])
		{
			/* worldspawn have it empty, since it's keys sets global parms */
			VectorSet(minlight, 0, 0, 0);
			VectorSet(minvertexlight, 0, 0, 0);
			VectorSet(ambient, 0, 0, 0);
			VectorSet(colormod, 1, 1, 1);
		}

		/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
		GetEntityPatchMeta(mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

		/* vortex: vertical texture projection */
		if (strcmp("", ValueForKey(mapEnt, "_vtcproj")) || strcmp("", ValueForKey(mapEnt, "_vp")))
		{
			vertTexProj = IntForKey(mapEnt, "_vtcproj");
			if (vertTexProj <= 0.0f)
				vertTexProj = IntForKey(mapEnt, "_vp");
		}
		else
			vertTexProj = 0;

		/* ydnar: get cel shader :) for this entity */
		value = ValueForKey(mapEnt, "_celshader");
		if (value[0] == '\0')
			value = ValueForKey(&entities[0], "_celshader");
		if (value[0] != '\0')
		{
			sprintf(shader, "textures/%s", value);
			celShader = ShaderInfoForShader(shader);
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
		for (brush = mapEnt->brushes; brush != NULL; brush = brush->next)
		{
			brush->entityNum = mapEnt->mapEntityNum;
			brush->mapEntityNum = mapEnt->mapEntityNum;
			brush->castShadows = castShadows;
			brush->recvShadows = recvShadows;
			brush->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, brush->lightmapAxis); /* vortex */
			brush->smoothNormals = smoothNormals; /* vortex */
			brush->noclip = forceNoClip; /* vortex */
			brush->noTJunc = forceNoTJunc; /* vortex */
			brush->vertTexProj = vertTexProj; /* vortex */
			VectorCopy(minlight, brush->minlight); /* vortex */
			VectorCopy(minvertexlight, brush->minvertexlight); /* vortex */
			VectorCopy(ambient, brush->ambient); /* vortex */
			VectorCopy(colormod, brush->colormod); /* vortex */
			brush->celShader = celShader;
			if (forceNonSolid == qtrue)
			{
				brush->detail = qtrue;
				brush->nonsolid = qtrue;
				brush->noclip = qtrue;
			}
		}

		for (patch = mapEnt->patches; patch != NULL; patch = patch->next)
		{
			patch->entityNum = mapEnt->mapEntityNum;
			patch->mapEntityNum = mapEnt->mapEntityNum;
			patch->castShadows = castShadows;
			patch->recvShadows = recvShadows;
			patch->lightmapScale = lightmapScale;
			VectorCopy(lightmapAxis, patch->lightmapAxis); /* vortex */
			patch->smoothNormals = smoothNormals; /* vortex */
			patch->vertTexProj = vertTexProj; /* vortex */
			patch->celShader = celShader;
			patch->patchMeta = forceMeta; /* vortex */
			patch->patchQuality = patchQuality; /* vortex */
			patch->patchSubdivisions = patchSubdivision; /* vortex */
			VectorCopy(minlight, patch->minlight); /* vortex */
			VectorCopy(minvertexlight, patch->minvertexlight); /* vortex */
			VectorCopy(ambient, patch->ambient); /* vortex */
			VectorCopy(colormod, patch->colormod); /* vortex */
			patch->nonsolid = forceNonSolid;
		}

		/* vortex: store map entity num */
		{
			char buf[32];
			sprintf(buf, "%i", mapEnt->mapEntityNum);
			SetKeyValue(mapEnt, "_mapEntityNum", buf);
		}

		/* ydnar: gs mods: set entity bounds */
		SetEntityBounds(mapEnt);

		/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
		LoadEntityIndexMap(mapEnt);

		/* get entity origin and adjust brushes */
		GetVectorForKey(mapEnt, "origin", mapEnt->origin);
		if (mapEnt->originbrush_origin[0] || mapEnt->originbrush_origin[1] || mapEnt->originbrush_origin[2])
			AdjustBrushesForOrigin(mapEnt);
	}
}

//
// Cities...
//

qboolean		*BUILDING_ASSIGNED;

void ReassignCityModels(void)
{
	int				i;
	int				NUM_PLACED[MAX_FOREST_MODELS] = { 0 };
	float			*BUILDING_BUFFER_RANGES;
	float			*BUILDING_SAME_RANGES;
	int				POSSIBLES[MAX_FOREST_MODELS] = { 0 };
	float			POSSIBLES_BUFFERS[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_BUILDING_SAME_RANGES[MAX_FOREST_MODELS] = { 0.0 };
	float			POSSIBLES_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
	int				NUM_POSSIBLES = 0;
	int				NUM_CLOSE_CLIFFS = 0;

	BUILDING_ASSIGNED = (qboolean*)malloc(sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	BUILDING_BUFFER_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	BUILDING_SAME_RANGES = (float*)malloc(sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	memset(BUILDING_ASSIGNED, qfalse, sizeof(qboolean)*FOLIAGE_MAX_FOLIAGES);
	memset(BUILDING_BUFFER_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);
	memset(BUILDING_SAME_RANGES, 0.0, sizeof(float)*FOLIAGE_MAX_FOLIAGES);

	Rand_Init(FOLIAGE_POSITIONS[0][0]);

	//Sys_Printf("Finding angles models.\n");

	// Find non-angle models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (CITY_FORCED_MAX_ANGLE[i] == 0.0)
		{
			continue;
		}

		if (CITY_CENTRAL_ONCE[i])
		{
			continue;
		}

		if (!strcmp(CITY_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = CITY_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_BUILDING_SAME_RANGES[NUM_POSSIBLES] = CITY_FORCED_DISTANCE_FROM_SAME[i];
		POSSIBLES_MAX_ANGLE[NUM_POSSIBLES] = CITY_FORCED_MAX_ANGLE[i];
		NUM_POSSIBLES++;
	}

	Sys_Printf("Restricted angle possibles:\n");
	for (i = 0; i < NUM_POSSIBLES; i++)
	{
		Sys_Printf("%d - %s.\n", i, CITY_MODELS[POSSIBLES[i]]);
	}

	// One-Off models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		printLabelledProgress("PlaceOneOffModels", i, MAX_FOREST_MODELS);

		if (!strcmp(CITY_MODELS[i], ""))
		{
			continue;
		}

		if (CITY_CENTRAL_ONCE[i])
		{
			float	bestDist = 99999.9f;
			int		best = -1;

			// Find the best spot...
			for (int j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (BUILDING_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(CITY_LOCATION, FOLIAGE_POSITIONS[j]);

				if (dist < bestDist)
				{
					qboolean bad = qfalse;

					if (CITY_FORCED_MAX_ANGLE[i] != 0.0)
					{// Need to check angles...
						vec3_t angles;
						vectoangles(FOLIAGE_NORMALS[j], angles);
						float pitch = angles[0];
						if (pitch > 180)
							pitch -= 360;

						if (pitch < -180)
							pitch += 360;

						pitch += 90.0f;

						if (pitch > CITY_FORCED_MAX_ANGLE[i] || pitch < -CITY_FORCED_MAX_ANGLE[i])
						{// Bad slope...
						 //Sys_Printf("Position %i angles too great (%f).\n", i, pitch);
							continue;
						}
					}

					for (int z = 0; z < numCliffs; z++)
					{// Also keep them away from cliff objects...
						if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[j]) < cliffScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS
							/*|| DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[j]) < ledgeScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS*/)
						{
							bad = qtrue;
							NUM_CLOSE_CLIFFS++;
							break;
						}
					}

					if (bad)
					{
						continue;
					}

					for (int k = 0; k < FOLIAGE_NUM_POSITIONS; k++)
					{
						if (j == k)
						{
							continue;
						}

						if (!BUILDING_ASSIGNED[k])
						{
							continue;
						}

						float dist2 = Distance(FOLIAGE_POSITIONS[k], FOLIAGE_POSITIONS[j]);

						if (dist2 <= CITY_FORCED_BUFFER_DISTANCE[i] || dist2 <= CITY_FORCED_BUFFER_DISTANCE[FOLIAGE_TREE_SELECTION[k]])
						{// Not within this object's buffer range... OK!
							bad = qtrue;
							break;
						}
					}

					if (bad)
					{
						continue;
					}

					best = j;
					bestDist = dist;
				}
			}

			if (best >= 0)
			{
				FOLIAGE_TREE_SELECTION[best] = i;
				FOLIAGE_TREE_BUFFER[best] = BUILDING_BUFFER_RANGES[best] = CITY_FORCED_BUFFER_DISTANCE[i];
				BUILDING_SAME_RANGES[best] = CITY_FORCED_DISTANCE_FROM_SAME[i];
				BUILDING_ASSIGNED[best] = qtrue;
				FOLIAGE_TREE_SCALE[best] = 1.0; // Once-Off models always use exact size stated.
				NUM_PLACED[i]++;
			}
			else
			{
				Sys_Printf("Warning: Failed to find a place for once off model %i [%s].\n", i, CITY_MODELS[i]);
			}
		}
	}

	//Sys_Printf("Assign angles models from %i possibles.\n", NUM_POSSIBLES);

	// First add any non-steep options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			float pitch;
			qboolean bad = qfalse;
			int j;

			printLabelledProgress("RandomizeNoAngleModels", i, FOLIAGE_NUM_POSITIONS);

			if (BUILDING_ASSIGNED[i])
			{
				continue;
			}

			if (Distance(FOLIAGE_POSITIONS[i], CITY_LOCATION) > CITY_RADIUS 
				&& Distance(FOLIAGE_POSITIONS[i], CITY2_LOCATION) > CITY2_RADIUS
				&& Distance(FOLIAGE_POSITIONS[i], CITY3_LOCATION) > CITY3_RADIUS
				&& Distance(FOLIAGE_POSITIONS[i], CITY4_LOCATION) > CITY4_RADIUS
				&& Distance(FOLIAGE_POSITIONS[i], CITY5_LOCATION) > CITY5_RADIUS)
			{
				continue;
			}

			int selected = irand(0, NUM_POSSIBLES - 1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (j == i)
				{
					continue;
				}

				if (!BUILDING_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUILDING_BUFFER_RANGES[j] || dist <= BUILDING_BUFFER_RANGES[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_BUILDING_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			vec3_t angles;
			vectoangles(FOLIAGE_NORMALS[i], angles);
			pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;

			if (pitch > POSSIBLES_MAX_ANGLE[selected] || pitch < -POSSIBLES_MAX_ANGLE[selected])
			{// Bad slope...
			 //Sys_Printf("Position %i angles too great (%f).\n", i, pitch);
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS
					/*|| DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS*/)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			//Sys_Printf("Position %i angles OK! (%f).\n", i, pitch);

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			FOLIAGE_TREE_BUFFER[i] = BUILDING_BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			BUILDING_SAME_RANGES[i] = POSSIBLES_BUILDING_SAME_RANGES[selected];
			BUILDING_ASSIGNED[i] = qtrue;
			FOLIAGE_TREE_SCALE[i] = 1.0; // City models always use exact size stated.
			NUM_PLACED[POSSIBLES[selected]]++;
		}
	}

	NUM_POSSIBLES = 0;

	//Sys_Printf("Finding other models.\n");

	// Find other models...
	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (CITY_FORCED_MAX_ANGLE[i] != 0.0)
		{
			continue;
		}

		if (CITY_CENTRAL_ONCE[i])
		{
			continue;
		}

		if (!strcmp(CITY_MODELS[i], ""))
		{
			continue;
		}

		POSSIBLES[NUM_POSSIBLES] = i;
		POSSIBLES_BUFFERS[NUM_POSSIBLES] = TREE_FORCED_BUFFER_DISTANCE[i];
		POSSIBLES_BUILDING_SAME_RANGES[NUM_POSSIBLES] = TREE_FORCED_DISTANCE_FROM_SAME[i];
		NUM_POSSIBLES++;
	}

	//Sys_Printf("Assign other models from %i possibles.\n", NUM_POSSIBLES);

	Sys_Printf("Non restricted angle possibles:\n");
	for (i = 0; i < NUM_POSSIBLES; i++)
	{
		Sys_Printf("%d - %s.\n", i, CITY_MODELS[POSSIBLES[i]]);
	}

	// Now add other options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			qboolean bad = qfalse;
			int tries = 0;
			int j;

			printLabelledProgress("RandomizeModels", i, FOLIAGE_NUM_POSITIONS);

			if (BUILDING_ASSIGNED[i])
			{
				continue;
			}

			int selected = irand(0, NUM_POSSIBLES - 1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (tries > 32)
				{
					bad = qtrue;
					break;
				}

				if (j == i)
				{
					continue;
				}

				if (!BUILDING_ASSIGNED[j])
				{
					continue;
				}

				float dist = Distance(FOLIAGE_POSITIONS[i], FOLIAGE_POSITIONS[j]);

				if (dist <= BUILDING_BUFFER_RANGES[j] || dist <= BUILDING_BUFFER_RANGES[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					break;
				}

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_BUILDING_SAME_RANGES[selected])
				{// Not within this object's same type range... OK!
					selected = irand(0, NUM_POSSIBLES - 1);
					tries++;
				}
			}

			if (bad)
			{
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
			 /*Sys_Printf("cliff %f %f %f. possible %f %f %f. Distance %f.\n", cliffPositions[z][0], cliffPositions[z][1], cliffPositions[z][2]
			 , FOLIAGE_POSITIONS[POSSIBLES[selected]][0], FOLIAGE_POSITIONS[POSSIBLES[selected]][1], FOLIAGE_POSITIONS[POSSIBLES[selected]][2],
			 DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[POSSIBLES[selected]]));
			 */

				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS
					/*|| DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS*/)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			FOLIAGE_TREE_BUFFER[i] = BUILDING_BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			BUILDING_SAME_RANGES[i] = POSSIBLES_BUILDING_SAME_RANGES[selected];
			BUILDING_ASSIGNED[i] = qtrue;
			FOLIAGE_TREE_SCALE[i] = 1.0; // City models always use exact size stated.
			NUM_PLACED[POSSIBLES[selected]]++;
		}
	}

	free(BUILDING_BUFFER_RANGES);
	free(BUILDING_SAME_RANGES);

	int count = 0;

	for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
	{
		if (BUILDING_ASSIGNED[i])
		{
			count++;
		}
	}

	Sys_Printf("%9d of %i positions successfully reassigned to new models.\n", count, FOLIAGE_NUM_POSITIONS - 1);

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (!strcmp(CITY_MODELS[i], ""))
		{
			continue;
		}

		Sys_Printf("%9d placements of model %s.\n", NUM_PLACED[i], CITY_MODELS[i]);
	}

	Sys_Printf("%9d positions ignored due to closeness to a cliff.\n", NUM_CLOSE_CLIFFS);
}

void GenerateMapCity(void)
{
	if (generatecity)
	{
		if (FOLIAGE_NUM_POSITIONS > 0)
		{// First re-assign model types based on buffer ranges and instance type ranges...
			int i;

			Sys_PrintHeading("--- GenerateMapCities ---\n");

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf("Adding %i buildings to bsp.\n", FOLIAGE_NUM_POSITIONS);
#endif

			ReassignCityModels();

			for (i = 0; i < FOLIAGE_NUM_POSITIONS /*&& i < 512*/; i++)
			{
				printLabelledProgress("GenerateMapCity", i, FOLIAGE_NUM_POSITIONS);

				if (!BUILDING_ASSIGNED[i])
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				const char		*classname, *value;
				float			lightmapScale;
				vec3_t          lightmapAxis;
				int			    smoothNormals;
				int				vertTexProj;
				char			shader[MAX_QPATH];
				shaderInfo_t	*celShader = NULL;
				brush_t			*brush;
				parseMesh_t		*patch;
				qboolean		funcGroup;
				char			castShadows, recvShadows;
				qboolean		forceNonSolid, forceNoClip, forceNoTJunc, forceMeta;
				vec3_t          minlight, minvertexlight, ambient, colormod;
				float           patchQuality, patchSubdivision;

				/* setup */
				entitySourceBrushes = 0;
				mapEnt = &entities[numEntities];
				numEntities++;
				memset(mapEnt, 0, sizeof(*mapEnt));

				mapEnt->mapEntityNum = 0;

				//mapEnt->mapEntityNum = numMapEntities;
				//numMapEntities++;

				//mapEnt->mapEntityNum = 0 - numMapEntities;

				//mapEnt->forceSubmodel = qtrue;

				VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
				mapEnt->origin[2] += CITY_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

				{
					char str[32];
					sprintf(str, "%f %f %f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
					SetKeyValue(mapEnt, "origin", str);
				}

				{
					char str[32];
					sprintf(str, "%f", FOLIAGE_TREE_SCALE[i] * 2.0*CITY_SCALE_MULTIPLIER*CITY_SCALES[FOLIAGE_TREE_SELECTION[i]]);
					SetKeyValue(mapEnt, "modelscale", str);
				}

				/*
				{
					char str[32];
					sprintf(str, "%f", FOLIAGE_TREE_ANGLES[i]);
					SetKeyValue(mapEnt, "angle", str);
				}
				*/
				

				if (!CITY_RANDOM_ANGLES)
					SetKeyValue(mapEnt, "angle", "0");
				else
					SetKeyValue(mapEnt, "angle", va("%i", irand(0, 360)));


				if (CITY_FORCED_FULLSOLID[FOLIAGE_TREE_SELECTION[i]] >= 2)
				{
					SetKeyValue(mapEnt, "_forcedSolid", "2");
				}
				else if (CITY_FORCED_FULLSOLID[FOLIAGE_TREE_SELECTION[i]])
				{
					SetKeyValue(mapEnt, "_forcedSolid", "1");
				}
				else
				{
					SetKeyValue(mapEnt, "_forcedSolid", "0");
				}

				if (CITY_FORCED_OVERRIDE_SHADER[FOLIAGE_TREE_SELECTION[i]] != '\0')
				{
					SetKeyValue(mapEnt, "_overrideShader", CITY_FORCED_OVERRIDE_SHADER[FOLIAGE_TREE_SELECTION[i]]);
				}

				/*{
				char str[32];
				vec3_t angles;
				vectoangles( FOLIAGE_NORMALS[i], angles );
				angles[PITCH] += 90;
				angles[YAW] = 270.0 - FOLIAGE_TREE_ANGLES[i];
				sprintf( str, "%f %f %f", angles[0], angles[1], angles[2] );
				SetKeyValue( mapEnt, "angles", str );
				}*/

				/*{
				char str[32];
				sprintf( str, "%i", 2 );
				SetKeyValue( mapEnt, "spawnflags", str );
				}*/

				//Sys_Printf( "Generated building at %f %f %f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


				/* ydnar: get classname */
				SetKeyValue(mapEnt, "classname", "misc_model");
				classname = ValueForKey(mapEnt, "classname");

				SetKeyValue(mapEnt, "model", CITY_MODELS[FOLIAGE_TREE_SELECTION[i]]); // test tree

																				  //Sys_Printf( "Generated building at %f %f %f. Model %s.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], TROPICAL_TREES[FOLIAGE_TREE_SELECTION[i]] );

				funcGroup = qfalse;

				/* get explicit shadow flags */
				GetEntityShadowFlags(mapEnt, NULL, &castShadows, &recvShadows, (funcGroup || mapEnt->mapEntityNum == 0) ? qtrue : qfalse);

				/* vortex: get lightmap scaling value for this entity */
				GetEntityLightmapScale(mapEnt, &lightmapScale, 0);

				/* vortex: get lightmap axis for this entity */
				GetEntityLightmapAxis(mapEnt, lightmapAxis, NULL);

				/* vortex: per-entity normal smoothing */
				GetEntityNormalSmoothing(mapEnt, &smoothNormals, 0);

				/* vortex: per-entity _minlight, _ambient, _color, _colormod  */
				GetEntityMinlightAmbientColor(mapEnt, NULL, minlight, minvertexlight, ambient, colormod, qtrue);
				if (mapEnt == &entities[0])
				{
					/* worldspawn have it empty, since it's keys sets global parms */
					VectorSet(minlight, 0, 0, 0);
					VectorSet(minvertexlight, 0, 0, 0);
					VectorSet(ambient, 0, 0, 0);
					VectorSet(colormod, 1, 1, 1);
				}

				/* vortex: _patchMeta, _patchQuality, _patchSubdivide support */
				GetEntityPatchMeta(mapEnt, &forceMeta, &patchQuality, &patchSubdivision, 1.0, patchSubdivisions);

				/* vortex: vertical texture projection */
				if (strcmp("", ValueForKey(mapEnt, "_vtcproj")) || strcmp("", ValueForKey(mapEnt, "_vp")))
				{
					vertTexProj = IntForKey(mapEnt, "_vtcproj");
					if (vertTexProj <= 0.0f)
						vertTexProj = IntForKey(mapEnt, "_vp");
				}
				else
					vertTexProj = 0;

				/* ydnar: get cel shader :) for this entity */
				value = ValueForKey(mapEnt, "_celshader");
				if (value[0] == '\0')
					value = ValueForKey(&entities[0], "_celshader");
				if (value[0] != '\0')
				{
					sprintf(shader, "textures/%s", value);
					celShader = ShaderInfoForShader(shader);
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
				for (brush = mapEnt->brushes; brush != NULL; brush = brush->next)
				{
					brush->entityNum = mapEnt->mapEntityNum;
					brush->mapEntityNum = mapEnt->mapEntityNum;
					brush->castShadows = castShadows;
					brush->recvShadows = recvShadows;
					brush->lightmapScale = lightmapScale;
					VectorCopy(lightmapAxis, brush->lightmapAxis); /* vortex */
					brush->smoothNormals = smoothNormals; /* vortex */
					brush->noclip = forceNoClip; /* vortex */
					brush->noTJunc = forceNoTJunc; /* vortex */
					brush->vertTexProj = vertTexProj; /* vortex */
					VectorCopy(minlight, brush->minlight); /* vortex */
					VectorCopy(minvertexlight, brush->minvertexlight); /* vortex */
					VectorCopy(ambient, brush->ambient); /* vortex */
					VectorCopy(colormod, brush->colormod); /* vortex */
					brush->celShader = celShader;
					if (forceNonSolid == qtrue)
					{
						brush->detail = qtrue;
						brush->nonsolid = qtrue;
						brush->noclip = qtrue;
					}
				}

				for (patch = mapEnt->patches; patch != NULL; patch = patch->next)
				{
					patch->entityNum = mapEnt->mapEntityNum;
					patch->mapEntityNum = mapEnt->mapEntityNum;
					patch->castShadows = castShadows;
					patch->recvShadows = recvShadows;
					patch->lightmapScale = lightmapScale;
					VectorCopy(lightmapAxis, patch->lightmapAxis); /* vortex */
					patch->smoothNormals = smoothNormals; /* vortex */
					patch->vertTexProj = vertTexProj; /* vortex */
					patch->celShader = celShader;
					patch->patchMeta = forceMeta; /* vortex */
					patch->patchQuality = patchQuality; /* vortex */
					patch->patchSubdivisions = patchSubdivision; /* vortex */
					VectorCopy(minlight, patch->minlight); /* vortex */
					VectorCopy(minvertexlight, patch->minvertexlight); /* vortex */
					VectorCopy(ambient, patch->ambient); /* vortex */
					VectorCopy(colormod, patch->colormod); /* vortex */
					patch->nonsolid = forceNonSolid;
				}

				/* vortex: store map entity num */
				{
					char buf[32];
					sprintf(buf, "%i", mapEnt->mapEntityNum);
					SetKeyValue(mapEnt, "_mapEntityNum", buf);
				}

				/* ydnar: gs mods: set entity bounds */
				SetEntityBounds(mapEnt);

				/* ydnar: gs mods: load shader index map (equivalent to old terrain alphamap) */
				LoadEntityIndexMap(mapEnt);

				/* get entity origin and adjust brushes */
				GetVectorForKey(mapEnt, "origin", mapEnt->origin);
				if (mapEnt->originbrush_origin[0] || mapEnt->originbrush_origin[1] || mapEnt->originbrush_origin[2])
					AdjustBrushesForOrigin(mapEnt);


#if defined(__ADD_TREES_EARLY__)
				AddTriangleModels(0, qtrue, qtrue);
				EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
				//MoveBrushesToWorld( mapEnt );
				numEntities--;
#endif
			}

			free(BUILDING_ASSIGNED);

#if defined(__ADD_TREES_EARLY__)
			Sys_Printf("Finished adding city.\n");
#endif
		}
	}
}
