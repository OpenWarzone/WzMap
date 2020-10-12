/* marker */
#define MAP_C



/* dependencies */
#include "q3map2.h"
#include "inifile.h"
#include "libs/picomodel/proctree/proctree.h"

extern void WzMap_PreloadModel(char *model, int frame, int *numLoadedModels, int allowSimplify, qboolean loadCollision);
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

qboolean		USE_SECONDARY_BSP = qfalse;

qboolean		MAP_REGENERATE_NORMALS = qfalse;
int				MAP_SMOOTH_NORMALS = 1;

qboolean		USE_LODMODEL = qfalse;
qboolean		FORCED_STRUCTURAL = qfalse;
qboolean		FORCED_MODEL_META = qfalse;
qboolean		CAULKIFY_CRAP = qfalse;
qboolean		REMOVE_CAULK = qfalse;
qboolean		USE_FAST_CULLSIDES = qfalse;
int				CULLSIDES_AFTER_MODEL_ADITION = 0;
qboolean		USE_CONVEX_HULL_MODELS = qfalse;
int				USE_BOX_MODELS = 0;
float			MAP_ROAD_SCAN_WIDTH_MULTIPLIER = 1.0;
char			DEFAULT_FALLBACK_SHADER[128] = { { 0 } };

float			NOT_GROUND_MAXSLOPE = 47.0;

qboolean		ADD_CLIFF_FACES = qfalse;
float			CLIFF_FACES_SCALE_XY = 1.0;
float			CLIFF_FACES_SCALE_Z = 1.0;
float			CLIFF_FACES_CULL_MULTIPLIER = 1.0;
qboolean		CLIFF_CHEAP = qfalse;
int				CLIFF_PLANE_SNAP = 0;
char			CLIFF_SHADER[MAX_QPATH] = { 0 };
char			CLIFF_CHECK_SHADER[MAX_QPATH] = { 0 };
int				CLIFF_MODELS_TOTAL = 0;
char			CLIFF_MODEL[MAX_FOREST_MODELS][MAX_QPATH] = { 0 };
qboolean		ADD_LEDGE_FACES = qfalse;
float			LEDGE_FACES_SCALE = 1.0;
qboolean		LEDGE_FACES_SCALE_XY = qfalse;
int				LEDGE_PLANE_SNAP = 0;
float			LEDGE_FACES_CULL_MULTIPLIER = 1.0;
float			LEDGE_MIN_SLOPE = 22.0;
float			LEDGE_MAX_SLOPE = 28.0;
qboolean		LEDGE_CHEAP = qfalse;
char			LEDGE_SHADER[MAX_QPATH] = { 0 };
char			LEDGE_CHECK_SHADER[MAX_QPATH] = { 0 };
float			TREE_SCALE_MULTIPLIER = 2.5;
float			TREE_SCALE_MULTIPLIER_SPECIAL = 1.0;
int				TREE_PERCENTAGE = 100;
int				TREE_PERCENTAGE_SPECIAL = 100;
qboolean		TREE_CLUSTER = qfalse;
float			TREE_CLUSTER_SEED = 0.01;
float			TREE_CLUSTER_MINIMUM = 0.5;
float			TREE_CLIFF_CULL_RADIUS = 1.0;
char			TREE_MODELS[MAX_FOREST_MODELS][128] = { 0 };
float			TREE_OFFSETS[MAX_FOREST_MODELS] = { -4.0 };
float			TREE_SCALES[MAX_FOREST_MODELS] = { 1.0 };
float			TREE_FORCED_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_BUFFER_DISTANCE[MAX_FOREST_MODELS] = { 0.0 };
float			TREE_FORCED_DISTANCE_FROM_SAME[MAX_FOREST_MODELS] = { 0.0 };
char			TREE_FORCED_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };
qboolean		TREE_FORCED_FULLSOLID[MAX_FOREST_MODELS] = { qfalse };
float			TREE_ROADSCAN_MULTIPLIER[MAX_FOREST_MODELS] = { 0.0 };
int				TREE_PLANE_SNAP[MAX_FOREST_MODELS] = { 8 };
qboolean		TREE_USE_ORIGIN_AS_LOWPOINT[MAX_FOREST_MODELS] = { qfalse };
int				TREE_ALLOW_SIMPLIFY[MAX_FOREST_MODELS] = { 0 };
int				PROCEDURAL_TREE_COUNT = 0;
char			PROCEDURAL_TREE_BARKS[MAX_FOREST_MODELS][128] = { 0 };
char			PROCEDURAL_TREE_LEAFS[MAX_FOREST_MODELS][128] = { 0 };
int				treePropertiesSegments[MAX_FOREST_MODELS] = { 6 };
int				treePropertiesLevels[MAX_FOREST_MODELS] = { 5 };
float			treePropertiesVMultiplier[MAX_FOREST_MODELS] = { 0.36f };
int				treePropertiesTwigScale[MAX_FOREST_MODELS] = { 0.39f };
float			treePropertiesInitialBranchLength[MAX_FOREST_MODELS] = { 0.49f };
float			treePropertiesLengthFalloffFactor[MAX_FOREST_MODELS] = { 0.85f };
float			treePropertiesLengthFalloffPower[MAX_FOREST_MODELS] = { 0.99f };
float			treePropertiesClumpMax[MAX_FOREST_MODELS] = { 0.454f };
float			treePropertiesClumpMin[MAX_FOREST_MODELS] = { 0.404f };
float			treePropertiesBranchFactor[MAX_FOREST_MODELS] = { 2.45f };
float			treePropertiesDropAmount[MAX_FOREST_MODELS] = { -0.1f };
float			treePropertiesGrowAmount[MAX_FOREST_MODELS] = { 0.235f };
float			treePropertiesSweepAmount[MAX_FOREST_MODELS] = { 0.01f };
float			treePropertiesMaxRadius[MAX_FOREST_MODELS] = { 0.139f };
float			treePropertiesClimbRate[MAX_FOREST_MODELS] = { 0.371f };
float			treePropertiesTrunkKink[MAX_FOREST_MODELS] = { 0.093f };
int				treePropertiesTreeSteps[MAX_FOREST_MODELS] = { 5 };
float			treePropertiesTaperRate[MAX_FOREST_MODELS] = { 0.947f };
float			treePropertiesRadiusFalloffRate[MAX_FOREST_MODELS] = { 0.73f };
float			treePropertiesTwistRate[MAX_FOREST_MODELS] = { 3.02f };
float			treePropertiesTrunkLength[MAX_FOREST_MODELS] = { 2.4f };
qboolean		ADD_CITY_ROADS = qfalse;
float			CITY_SCALE_MULTIPLIER = 2.5;
float			CITY_CLIFF_CULL_RADIUS = 1.0;
float			CITY_BUFFER = 1024.0;
vec3_t			CITY_LOCATION = { 0 };
float			CITY_RADIUS = 0;
int				CITY_RANDOM_ANGLES = 0;
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
float			CITY_SCALES_XY[MAX_FOREST_MODELS] = { 1.0 };
float			CITY_SCALES_Z[MAX_FOREST_MODELS] = { 1.0 };
int				CITY_ANGLES_RANDOMIZE[MAX_FOREST_MODELS] = { 0 };
qboolean		CITY_CENTRAL_ONCE[MAX_FOREST_MODELS] = { qfalse };
qboolean		CITY_ALLOW_ROAD[MAX_FOREST_MODELS] = { qfalse };
float			CITY_FORCED_MAX_ANGLE[MAX_FOREST_MODELS] = { 0.0 };
float			CITY_FORCED_BUFFER_DISTANCE[MAX_FOREST_MODELS] = { 0.0 };
float			CITY_FORCED_DISTANCE_FROM_SAME[MAX_FOREST_MODELS] = { 0.0 };
char			CITY_FORCED_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };
int				CITY_FORCED_FULLSOLID[MAX_FOREST_MODELS] = { 0 };
int				CITY_PLANE_SNAP[MAX_FOREST_MODELS] = { 0 };
int				CITY_ALLOW_SIMPLIFY[MAX_FOREST_MODELS] = { 0 };

float			CHUNK_MAP_LEVEL = -999999.9;
qboolean		CHUNK_ADD_AT_MAP_EDGES = qtrue;
float			CHUNK_MAP_EDGE_SCALE = 1.0;
float			CHUNK_MAP_EDGE_SCALE_XY = 1.0;
float			CHUNK_FOLIAGE_GENERATOR_SCATTER = 256.0;
float			CHUNK_SCALE = 1.0;
int				CHUNK_PLANE_SNAP = 0;
int				CHUNK_MODELS_TOTAL = 0;
char			CHUNK_MODELS[MAX_FOREST_MODELS][128] = { 0 };
char			CHUNK_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };
int				CHUNK_MAP_EDGE_MODELS_TOTAL = 0;
char			CHUNK_MAP_EDGE_MODELS[MAX_FOREST_MODELS][128] = { 0 };
char			CHUNK_MAP_EDGE_OVERRIDE_SHADER[MAX_FOREST_MODELS][128] = { 0 };

float			CENTER_CHUNK_MAP_LEVEL = -999999.9;
float			CENTER_CHUNK_FOLIAGE_GENERATOR_SCATTER = 256.0;
float			CENTER_CHUNK_SCALE = 1.0;
float			CENTER_CHUNK_ANGLE = 1.0;
int				CENTER_CHUNK_PLANE_SNAP = 0;
char			CENTER_CHUNK_MODEL[128] = { 0 };
char			CENTER_CHUNK_OVERRIDE_SHADER[128] = { 0 };

qboolean		ADD_SKYSCRAPERS = qfalse;
vec3_t			SKYSCRAPERS_CENTER = { 0 };
float			SKYSCRAPERS_RADIUS = 0;
int				SKYSCRAPERS_PLANE_SNAP = 0;

#define STATIC_BUFFER_RANGE 156.0

char			STATIC_MODEL[MAX_STATIC_ENTITY_MODELS][128] = { 0 };
vec3_t			STATIC_ORIGIN[MAX_STATIC_ENTITY_MODELS] = { 0 };
float			STATIC_ANGLE[MAX_STATIC_ENTITY_MODELS] = { 0 };
float			STATIC_SCALE[MAX_STATIC_ENTITY_MODELS] = { 0 };
int				STATIC_ALLOW_SIMPLIFY[MAX_STATIC_ENTITY_MODELS] = { 0 };
int				STATIC_PLANE_SNAP[MAX_STATIC_ENTITY_MODELS] = { 0 };
int				STATIC_FORCED_FULLSOLID[MAX_STATIC_ENTITY_MODELS] = { 0 };
float			STATIC_MODEL_RADIUS[MAX_STATIC_ENTITY_MODELS] = { 0.0 };
char			STATIC_FORCED_OVERRIDE_SHADER[MAX_STATIC_ENTITY_MODELS][128] = { 0 };

#define MAX_REPLACE_SHADERS 128
int				REPLACE_SHADERS_NUM = 0;
char			REPLACE_SHADERS_ORIGINAL[MAX_REPLACE_SHADERS][128] = { 0 };
char			REPLACE_SHADERS_NEW[MAX_REPLACE_SHADERS][128] = { 0 };

#define MAP_INFO_TRACEMAP_SIZE 2048

char currentMapName[256] = { 0 };

// =======================================================================================================================================
//
// Roads Map System...
//
// =======================================================================================================================================
#include "TinyImageLoader/TinyImageLoader.h"
#include <string>

char *R_TIL_TextureFileExistsFull(const char *name)
{
	char texName[512] = { 0 };
	sprintf(texName, "%s.png", name);

	if (FileExists(texName))
	{
		return "png";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.tga", name);

	if (FileExists(texName))
	{
		return "tga";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.jpg", name);

	if (FileExists(texName))
	{
		return "jpg";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.dds", name);

	if (FileExists(texName))
	{
		return "dds";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.gif", name);

	if (FileExists(texName))
	{
		return "gif";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.bmp", name);

	if (FileExists(texName))
	{
		return "bmp";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	memset(&texName, 0, sizeof(char) * 512);
	sprintf(texName, "%s.ico", name);

	if (FileExists(texName))
	{
		return "ico";
	}
	//else Sys_Printf("%s does not exist.\n", texName);

	return NULL;
}

#define PixelCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define srgb_to_linear(c) (((c) <= 0.04045) ? (c) * (1.0 / 12.92) : pow(((c) + 0.055f)*(1.0/1.055), 2.4))

qboolean SampleRoadImage(uint8_t *pixels, int width, int height, float st[2], float color[4])
{
	qboolean texturesRGB = qfalse;
	float	sto[2];
	int		x, y;

	/* clear color first */
	color[0] = color[1] = color[2] = color[3] = 0;

	/* dummy check */
	if (pixels == NULL || width < 1 || height < 1)
		return qfalse;

	/* bias st */
	sto[0] = st[0];
	while (sto[0] < 0.0f)
		sto[0] += 1.0f;
	sto[1] = st[1];
	while (sto[1] < 0.0f)
		sto[1] += 1.0f;

	/* get offsets */
	x = ((float)width * sto[0]);// +0.5f;
	x %= width;
	y = ((float)height * sto[1]);// +0.5f;
	y %= height;

	/* get pixel */
	pixels += (y * width * 4) + (x * 4);
	PixelCopy(pixels, color);
	color[3] = pixels[3];

	return qtrue;
}

qboolean TIL_INITIALIZED = qfalse;

qboolean ROAD_MAP_INITIALIZED = qfalse;
til::Image *ROAD_MAP = NULL;

void StripPath(char *path)
{
	size_t length;

	length = strlen(path) - 1;
	while (length > 0 && path[length] != '/' && path[length] != '\\')
	{
		length--;
	}
	if (length)
	{
		std::string temp = path;
		temp = temp.substr(length + 1, strlen(path) - length);
		sprintf(path, temp.c_str());
	}
}

void LoadRoadMapImage(void)
{
	char fileName[255];
	strcpy(fileName, currentMapName);
	StripPath(fileName);

	if (!ROAD_MAP && !ROAD_MAP_INITIALIZED)
	{
		ROAD_MAP_INITIALIZED = qtrue;

		char name[512] = { 0 };
		strcpy(name, va("%smaps/%s_roads", g_strDirs[0], fileName));
		char *ext = R_TIL_TextureFileExistsFull(name);

		if (ext)
		{
			if (!TIL_INITIALIZED)
			{
				til::TIL_Init();
				TIL_INITIALIZED = qtrue;
			}

			char fullPath[1024] = { 0 };
			sprintf_s(fullPath, "%s.%s", name, ext);
			ROAD_MAP = til::TIL_Load(fullPath/*, TIL_FILE_ADDWORKINGDIR*/);

			if (ROAD_MAP && ROAD_MAP->GetHeight() > 0 && ROAD_MAP->GetWidth() > 0)
			{
				Sys_Printf(va("Road map loaded from %s. Size %ix%i.\n", fullPath, ROAD_MAP->GetWidth(), ROAD_MAP->GetHeight()));
			}
			else
			{
				Sys_Printf(va("Road map failed to load from %s.\n", fullPath));
				til::TIL_Release(ROAD_MAP);
				ROAD_MAP = NULL;
			}
		}
		else
		{
			Sys_Printf(va("Road map not found at %s.\n", name));
		}
	}
}

qboolean ROAD_BOUNDS_INITIALIZED = qfalse;

vec3_t ROADS_MINS;
vec3_t ROADS_MAXS;

void SetupRoadmapBounds(void)
{
	/*
	ClearBounds(ROADS_MINS, ROADS_MAXS);

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		mapDrawSurface_t *ds = &mapDrawSurfs[s];

		for (int i = 0; i < ds->numVerts; i++)
			AddPointToBounds(ds->verts[i].xyz, ROADS_MINS, ROADS_MAXS);
	}
	*/

	VectorCopy(mapPlayableMins, ROADS_MINS);
	VectorCopy(mapPlayableMaxs, ROADS_MAXS);

	ROAD_BOUNDS_INITIALIZED = qtrue;
}

qboolean RoadExistsAtPoint(vec3_t point, int inScanWidth)
{
	if (!ROAD_MAP)
	{
		LoadRoadMapImage();

		if (!ROAD_MAP)
		{
			return qfalse;
		}
	}

	if (!ROAD_BOUNDS_INITIALIZED)
	{
		SetupRoadmapBounds();
	}

	float mapSize[2];
	float pixel[2];

	mapSize[0] = ROADS_MAXS[0] - ROADS_MINS[0];
	mapSize[1] = ROADS_MAXS[1] - ROADS_MINS[1];
	pixel[0] = (point[0] - ROADS_MINS[0]) / mapSize[0];
	pixel[1] = (point[1] - ROADS_MINS[1]) / mapSize[1];

	vec4_t color;
	RadSampleImage((uint8_t *)ROAD_MAP->GetPixels(), ROAD_MAP->GetWidth(), ROAD_MAP->GetHeight(), pixel, color);
	float road = color[2];

	if (road > 0)
	{
		return qtrue;
	}

	int scanWidth = int((float)inScanWidth * MAP_ROAD_SCAN_WIDTH_MULTIPLIER);

#if 1
	// Also scan pixels around this position...
	for (int x = -scanWidth; x <= scanWidth; x++)
	{
		for (int y = -scanWidth; y <= scanWidth; y++)
		{
			if (x == 0 && y == 0) continue; // Already checked this one...

			float pixel2[2];
			pixel2[0] = pixel[0] + (x / (float)ROAD_MAP->GetWidth());
			pixel2[1] = pixel[1] + (y / (float)ROAD_MAP->GetHeight());

			if (pixel2[0] >= 0 && pixel2[0] <= 1.0 && pixel2[1] >= 0 && pixel2[1] <= 1.0)
			{
				RadSampleImage((uint8_t *)ROAD_MAP->GetPixels(), ROAD_MAP->GetWidth(), ROAD_MAP->GetHeight(), pixel2, color);
				float road2 = color[2];

				if (road2 > 0)
				{
					return qtrue;
				}
			}
		}
	}
#endif
	
	return qfalse;
}

extern qboolean StringContainsWord(const char *haystack, const char *needle);

void FindWaterLevel(void)
{
	if (MAP_WATER_LEVEL > -999999.0)
	{// Forced by climate ini file...
		Sys_Printf("Climate specified map water level at %.4f.\n", MAP_WATER_LEVEL);
		return;
	}

	float waterLevel = 999999.9;
	float waterLevel2 = 999999.9; // we actually want the second highest water level...

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
				//Sys_Printf("Water level set to %.4f. 2 set to %.4f.\n", ds->mins[2], waterLevel);
				waterLevel2 = waterLevel;
				waterLevel = ds->mins[2];
				continue;
			}
			
			if (StringContainsWord(si->shader, "water"))
			{
				//Sys_Printf("Water level set to %.4f. 2 set to %.4f.\n", ds->mins[2], waterLevel);
				waterLevel2 = waterLevel;
				waterLevel = ds->mins[2];
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

void CaulkifyStuff(qboolean findBounds);


void PreloadClimateModels(void)
{
	//
	// Pre-load all models, and generate/load convex hull collision meshes...
	//

	int i = 0;
	int numLoadedModels = 0;

	if (CLIFF_MODELS_TOTAL > 0)
	{
		for (i = 0; i < CLIFF_MODELS_TOTAL; i++)
		{
			WzMap_PreloadModel(CLIFF_MODEL[i], 0, &numLoadedModels, 3, qtrue);
		}
	}
	else
	{
		for (i = 1; i <= 5; i++)
		{
			WzMap_PreloadModel(va("models/warzone/rocks/cliffface0%i.md3", i), 0, &numLoadedModels, 3, qtrue);
		}
	}

	for (i = 1; i <= 4; i++)
	{
		WzMap_PreloadModel(va("models/warzone/rocks/ledge0%i.md3", i), 0, &numLoadedModels, 3, qtrue);
	}

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (strlen(TREE_MODELS[i]) > 0)
		{
			WzMap_PreloadModel(TREE_MODELS[i], 0, &numLoadedModels, TREE_ALLOW_SIMPLIFY[i], qtrue);
		}
	}

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		if (strlen(CITY_MODELS[i]) > 0)
		{
			WzMap_PreloadModel(CITY_MODELS[i], 0, &numLoadedModels, CITY_ALLOW_SIMPLIFY[i], qtrue);
		}
	}

	for (i = 0; i < MAX_STATIC_ENTITY_MODELS; i++)
	{
		if (strlen(STATIC_MODEL[i]) > 0)
		{
			WzMap_PreloadModel(STATIC_MODEL[i], 0, &numLoadedModels, STATIC_ALLOW_SIMPLIFY[i], qtrue);
		}
	}

	for (i = 0; i < CHUNK_MODELS_TOTAL; i++)
	{
		WzMap_PreloadModel(CHUNK_MODELS[i], 0, &numLoadedModels, 0, qtrue);
	}

	for (i = 0; i < CHUNK_MAP_EDGE_MODELS_TOTAL; i++)
	{
		WzMap_PreloadModel(CHUNK_MAP_EDGE_MODELS[i], 0, &numLoadedModels, 0, qtrue);
	}

	WzMap_PreloadModel(CENTER_CHUNK_MODEL, 0, &numLoadedModels, 0, qtrue);

#if 0
	if (ADD_CITY_ROADS)
	{
		//WzMap_PreloadModel("models/warzone/roads/road01.md3", 0, &numLoadedModels);
		WzMap_PreloadModel("models/warzone/roads/road02.md3", 0, &numLoadedModels, 0, qtrue);
	}
#endif
}

extern const char *materialNames[MATERIAL_LAST];

#define MAX_ALLOWED_MATERIALS_LIST 64

int GENFOLIAGE_ALLOWED_MATERIALS_NUM = 0;
int GENFOLIAGE_ALLOWED_MATERIALS[MAX_ALLOWED_MATERIALS_LIST] = { 0 };

qboolean SurfaceIsAllowedFoliage(int materialType)
{
	for (int i = 0; i < GENFOLIAGE_ALLOWED_MATERIALS_NUM; i++)
	{
		if (materialType == GENFOLIAGE_ALLOWED_MATERIALS[i]) return qtrue;
	}

	return qfalse;
}

void FOLIAGE_LoadClimateData(char *filename)
{
	int i = 0;

	Sys_PrintHeading("--- LoadClimateData ---\n");

	USE_SECONDARY_BSP = (qboolean)atoi(IniRead(filename, "GENERAL", "useSecondaryBSP", "0"));

	if (USE_SECONDARY_BSP)
	{
		Sys_Printf("Using warzone secondary BSP file.\n");
	}

	MAP_WATER_LEVEL = atof(IniRead(filename, "GENERAL", "forcedWaterLevel", "-999999.9"));

	if (MAP_WATER_LEVEL > -999999.0)
	{
		Sys_Printf("Forcing map water level to %.4f.\n", MAP_WATER_LEVEL);
	}

	MAP_REGENERATE_NORMALS = (qboolean)atoi(IniRead(filename, "GENERAL", "regenerateNormals", "0"));

	if (MAP_REGENERATE_NORMALS)
	{
		Sys_Printf("Normals will be regenerated for this map.\n");
	}
	else
	{
		Sys_Printf("Normals will not be regenerated for this map.\n");
	}

	MAP_SMOOTH_NORMALS = atoi(IniRead(filename, "GENERAL", "smoothNormals", "1"));

	if (MAP_SMOOTH_NORMALS > 1)
	{
		Sys_Printf("Smoothing normals (full map) for this map.\n");
	}
	else if (MAP_SMOOTH_NORMALS)
	{
		Sys_Printf("Smoothing normals for this map.\n");
	}
	else
	{
		Sys_Printf("Not smoothing normals for this map.\n");
	}

	USE_LODMODEL = (qboolean)atoi(IniRead(filename, "GENERAL", "useLodModel", "0"));

	if (USE_LODMODEL)
	{
		Sys_Printf("Using misc_lodmodel instead of misc_model.\n");
	}

	FORCED_STRUCTURAL = (qboolean)atoi(IniRead(filename, "GENERAL", "forcedStructural", "0"));

	if (FORCED_STRUCTURAL)
	{
		Sys_Printf("Forcing all solid surfaces to structural brushwork.\n");
	}

	FORCED_MODEL_META = (qboolean)atoi(IniRead(filename, "GENERAL", "forcedModelMeta", "0"));

	if (FORCED_MODEL_META)
	{
		Sys_Printf("Forcing all models to use meta surfaces.\n");
	}

	CAULKIFY_CRAP = (atoi(IniRead(filename, "GENERAL", "caulkifyCrap", "0")) > 0) ? qtrue : qfalse;

	if (CAULKIFY_CRAP)
	{
		Sys_Printf("Caulkifying some map unneeded surfaces.\n");
	}

	REMOVE_CAULK = (atoi(IniRead(filename, "GENERAL", "removeCaulk", "0")) > 0) ? qtrue : qfalse;

	if (REMOVE_CAULK)
	{
		Sys_Printf("Removing all caulk surfaces from bsp.\n");
	}

	USE_FAST_CULLSIDES = (qboolean)atoi(IniRead(filename, "GENERAL", "fastCullSides", "0"));

	if (USE_FAST_CULLSIDES)
	{
		Sys_Printf("Fast Cullsides will be used.\n");
	}

	CULLSIDES_AFTER_MODEL_ADITION = atoi(IniRead(filename, "GENERAL", "cullSidesAfterModelAddition", "0"));

	if (CULLSIDES_AFTER_MODEL_ADITION == 1)
	{
		Sys_Printf("Cullsides will be run again after adding models in the groundChunk pass.\n");
	}
	else if (CULLSIDES_AFTER_MODEL_ADITION)
	{
		Sys_Printf("Cullsides will be run again after adding models in each pass.\n");
	}

	USE_CONVEX_HULL_MODELS = (qboolean)atoi(IniRead(filename, "GENERAL", "useConvexHullModels", "1"));

	if (USE_CONVEX_HULL_MODELS)
	{
		Sys_Printf("Allowed to use convex hull collision models.\n");
	}
	else
	{
		Sys_Printf("Convex hull collision models will not be used.\n");
	}

	USE_BOX_MODELS = atoi(IniRead(filename, "GENERAL", "useBoxModels", "0"));

	if (USE_BOX_MODELS > 1)
	{
		Sys_Printf("Allowed to use collision box models without rotations (for lower plane counts).\n");
	}
	else if (USE_BOX_MODELS)
	{
		Sys_Printf("Allowed to use collision box models.\n");
	}
	else
	{
		Sys_Printf("Collision box models will not be used.\n");
	}

	MAP_ROAD_SCAN_WIDTH_MULTIPLIER = atof(IniRead(filename, "GENERAL", "roadScanWidthMultiplier", "1.0"));
	Sys_Printf("Roads scan width is %.4f.\n", MAP_ROAD_SCAN_WIDTH_MULTIPLIER);

	strcpy(DEFAULT_FALLBACK_SHADER, IniRead(filename, "GENERAL", "defaultFallbackShader", ""));
	Sys_Printf("Default fallback shader set to %s.\n", DEFAULT_FALLBACK_SHADER[0] ? DEFAULT_FALLBACK_SHADER : "NONE");

	//
	// Allow foliage positions extra materials...
	//
	// Parse any specified extra surface material types to add grasses to...
	for (int m = 0; m < 8; m++)
	{
		char allowMaterial[64] = { 0 };
		strcpy(allowMaterial, IniRead(filename, "POSITIONS", va("proceduralAllowmaterial%i", m), ""));

		if (!allowMaterial || !allowMaterial[0] || allowMaterial[0] == '\0' || strlen(allowMaterial) <= 1) continue;

		for (int i = 0; i < MATERIAL_LAST; i++)
		{
			if (!Q_stricmp(allowMaterial, materialNames[i]))
			{// Got one, add it to the allowed list...
				GENFOLIAGE_ALLOWED_MATERIALS[GENFOLIAGE_ALLOWED_MATERIALS_NUM] = i;
				GENFOLIAGE_ALLOWED_MATERIALS_NUM++;
				break;
			}
		}
	}

	NOT_GROUND_MAXSLOPE = atof(IniRead(filename, "POSITIONS", "maxValidSlope", "47.0"));

	//
	// Replacement shaders...
	//
	//char			REPLACE_SHADERS_ORIGINAL[MAX_REPLACE_SHADERS][128] = { 0 };
	//char			REPLACE_SHADERS_NEW[MAX_REPLACE_SHADERS][128] = { 0 };
	REPLACE_SHADERS_NUM = 0;

	for (int i = 0; i < MAX_REPLACE_SHADERS; i++)
	{// Load any replacement shaders...
		char shaderName[128] = { 0 };
		strcpy(shaderName, IniRead(filename, "SHADER_REPLACE", va("replaceShaderOriginal%i", i), ""));

		if (shaderName[0] != 0 && strlen(shaderName) > 0)
		{
			strcpy(REPLACE_SHADERS_ORIGINAL[REPLACE_SHADERS_NUM], shaderName);
			strcpy(REPLACE_SHADERS_NEW[REPLACE_SHADERS_NUM], IniRead(filename, "SHADER_REPLACE", va("replaceShaderNew%i", i), ""));
			REPLACE_SHADERS_NUM++;
		}
		else
		{
			break;
		}
	}

	//
	// Map Chunks...
	//

	CHUNK_MAP_LEVEL = atof(IniRead(filename, "CHUNKS", "chunkMapLevel", "-999999.9"));

	if (CHUNK_MAP_LEVEL > -999990.0)
	{
		CHUNK_SCALE = atof(IniRead(filename, "CHUNKS", "chunkScale", "1.0"));
		CHUNK_PLANE_SNAP = atoi(IniRead(filename, "CHUNKS", "chunkPlaneSnap", "1"));
		CHUNK_ADD_AT_MAP_EDGES = atoi(IniRead(filename, "CHUNKS", "chunksAtMapEdges", "1")) > 0 ? qtrue : qfalse;
		CHUNK_MAP_EDGE_SCALE = atof(IniRead(filename, "CHUNKS", "chunkMapEdgeScale", "1.0"));
		CHUNK_MAP_EDGE_SCALE_XY = atof(IniRead(filename, "CHUNKS", "chunkMapEdgeScaleXY", "1.0"));
		CHUNK_FOLIAGE_GENERATOR_SCATTER = atof(IniRead(filename, "CHUNKS", "chunkFoliageScatter", "256.0"));

		Sys_Printf("Using map chunks system at map level %f, scaled by %f, with a plane snap of %i.\n", CHUNK_MAP_LEVEL, CHUNK_SCALE, CHUNK_PLANE_SNAP);

		if (CHUNK_ADD_AT_MAP_EDGES)
		{
			Sys_Printf("Adding chunks at map edges with scale %f %f %f.\n", CHUNK_MAP_EDGE_SCALE_XY, CHUNK_MAP_EDGE_SCALE_XY, CHUNK_MAP_EDGE_SCALE);
		}
		else
		{
			Sys_Printf("Not adding chunks at map edges.\n");
		}

		Sys_Printf("Chunk Models:\n");

		for (int i = 0; i < 64; i++)
		{
			char modelName[512] = { 0 };
			strcpy(modelName, IniRead(filename, "CHUNKS", va("model%i", i), ""));

			if (modelName[0])
			{
				strcpy(CHUNK_MODELS[CHUNK_MODELS_TOTAL], modelName);
				strcpy(CHUNK_OVERRIDE_SHADER[CHUNK_MODELS_TOTAL], IniRead(filename, "CHUNKS", va("overrideShader%i", i), ""));

				char fileName[512];
				strcpy(fileName, CHUNK_MODELS[CHUNK_MODELS_TOTAL]);
				StripPath(fileName);

				Sys_Printf("  %s. Override Shader %s.\n", fileName, CHUNK_OVERRIDE_SHADER[CHUNK_MODELS_TOTAL][0] ? CHUNK_OVERRIDE_SHADER[CHUNK_MODELS_TOTAL] : "NONE");
				CHUNK_MODELS_TOTAL++;
			}
		}

		Sys_Printf("Map Edge Chunk Models:\n");

		for (int i = 0; i < 64; i++)
		{
			char modelName[512] = { 0 };
			strcpy(modelName, IniRead(filename, "CHUNKS", va("edgeModel%i", i), ""));

			if (modelName[0])
			{
				strcpy(CHUNK_MAP_EDGE_MODELS[CHUNK_MAP_EDGE_MODELS_TOTAL], modelName);
				strcpy(CHUNK_MAP_EDGE_OVERRIDE_SHADER[CHUNK_MAP_EDGE_MODELS_TOTAL], IniRead(filename, "CHUNKS", va("edgeOverrideShader%i", i), ""));

				Sys_Printf("  %s. Override Shader %s.\n", CHUNK_MAP_EDGE_MODELS[CHUNK_MAP_EDGE_MODELS_TOTAL], CHUNK_MAP_EDGE_OVERRIDE_SHADER[CHUNK_MAP_EDGE_MODELS_TOTAL][0] ? CHUNK_MAP_EDGE_OVERRIDE_SHADER[CHUNK_MAP_EDGE_MODELS_TOTAL] : "NONE");
				CHUNK_MAP_EDGE_MODELS_TOTAL++;
			}
		}

		if (!CHUNK_MAP_EDGE_MODELS_TOTAL)
		{// Copy from standard chunks list...
			for (int i = 0; i < CHUNK_MODELS_TOTAL; i++)
			{
				strcpy(CHUNK_MAP_EDGE_MODELS[i], CHUNK_MODELS[i]);
				strcpy(CHUNK_MAP_EDGE_OVERRIDE_SHADER[i], CHUNK_OVERRIDE_SHADER[i]);

				Sys_Printf("  %s. Override Shader %s.\n", CHUNK_MAP_EDGE_OVERRIDE_SHADER[i], CHUNK_MAP_EDGE_OVERRIDE_SHADER[i]);
			}

			CHUNK_MAP_EDGE_MODELS_TOTAL = CHUNK_MODELS_TOTAL;
		}
	}
	else
	{
		Sys_Printf("Not using map chunks system.\n");
	}

	//
	// Map Center Chunk...
	//

	CENTER_CHUNK_MAP_LEVEL = atof(IniRead(filename, "CENTER_CHUNK", "chunkMapLevel", "-999999.9"));

	if (CENTER_CHUNK_MAP_LEVEL > -999990.0)
	{
		CENTER_CHUNK_SCALE = atof(IniRead(filename, "CENTER_CHUNK", "chunkScale", "1.0"));
		CENTER_CHUNK_ANGLE = atof(IniRead(filename, "CENTER_CHUNK", "chunkAngle", "0.0"));
		CENTER_CHUNK_PLANE_SNAP = atoi(IniRead(filename, "CENTER_CHUNK", "chunkPlaneSnap", "1"));
		CENTER_CHUNK_FOLIAGE_GENERATOR_SCATTER = atof(IniRead(filename, "CENTER_CHUNK", "chunkFoliageScatter", "256.0"));

		Sys_Printf("Using map centerchunk system at map level %f, scaled by %f, angle %f, with a plane snap of %i.\n", CENTER_CHUNK_MAP_LEVEL, CENTER_CHUNK_SCALE, CENTER_CHUNK_ANGLE, CENTER_CHUNK_PLANE_SNAP);

		Sys_Printf("Center Chunk Model:\n");

		char modelName[512] = { 0 };
		strcpy(modelName, IniRead(filename, "CENTER_CHUNK", "model", ""));

		if (modelName[0])
		{
			strcpy(CENTER_CHUNK_MODEL, modelName);
			strcpy(CENTER_CHUNK_OVERRIDE_SHADER, IniRead(filename, "CENTER_CHUNK", "overrideShader", ""));

			Sys_Printf("  %s. Override Shader %s.\n", CENTER_CHUNK_MODEL, CENTER_CHUNK_OVERRIDE_SHADER[0] ? CENTER_CHUNK_OVERRIDE_SHADER : "NONE");
		}
	}
	else
	{
		Sys_Printf("Not using map centerchunk system.\n");
	}

	//
	// Cliffs...
	//

	ADD_CLIFF_FACES = (qboolean)atoi(IniRead(filename, "CLIFFS", "addCliffFaces", "0"));
	CLIFF_FACES_SCALE_XY = atof(IniRead(filename, "CLIFFS", "cliffFacesScaleXY", "1.0"));
	CLIFF_FACES_SCALE_Z = atof(IniRead(filename, "CLIFFS", "cliffFacesScaleZ", "1.0"));
	CLIFF_FACES_CULL_MULTIPLIER = atof(IniRead(filename, "CLIFFS", "cliffFacesCullScale", "1.0"));

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

	CLIFF_CHEAP = (qboolean)atoi(IniRead(filename, "CLIFFS", "cheapCliffs", "0"));

	CLIFF_PLANE_SNAP = atoi(IniRead(filename, "CLIFFS", "cliffPlaneSnap", "16"));

	if (CLIFF_PLANE_SNAP > 0)
	{
		Sys_Printf("Snapping cliff planes to %i units.\n", CLIFF_PLANE_SNAP);
	}

	if (!CLIFF_CHEAP)
	{
		Sys_Printf("Cliff Models:\n");

		for (int i = 0; i < 64; i++)
		{
			char modelName[512] = { 0 };
			strcpy(modelName, IniRead(filename, "CLIFFS", va("cliffModel%i", i), ""));

			if (modelName[0])
			{
				strcpy(CLIFF_MODEL[i], modelName);

				char fileName[512];
				strcpy(fileName, CLIFF_MODEL[i]);
				StripPath(fileName);

				Sys_Printf("  %s (custom)\n", fileName);

				CLIFF_MODELS_TOTAL++;
			}
		}

		if (CLIFF_MODELS_TOTAL <= 0)
		{// Use defaults...
			strcpy(CLIFF_MODEL[0], "models/warzone/rocks/cliffface01.md3");
			strcpy(CLIFF_MODEL[1], "models/warzone/rocks/cliffface02.md3");
			strcpy(CLIFF_MODEL[2], "models/warzone/rocks/cliffface03.md3");
			strcpy(CLIFF_MODEL[3], "models/warzone/rocks/cliffface04.md3");
			strcpy(CLIFF_MODEL[4], "models/warzone/rocks/cliffface05.md3");
			CLIFF_MODELS_TOTAL = 5;

			for (int i = 0; i < CLIFF_MODELS_TOTAL; i++)
			{
				char fileName[512];
				strcpy(fileName, CLIFF_MODEL[i]);
				StripPath(fileName);

				Sys_Printf("  %s (default)\n", fileName);
			}
		}
	}


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

	LEDGE_PLANE_SNAP = atoi(IniRead(filename, "LEDGES", "ledgePlaneSnap", "8"));

	if (LEDGE_PLANE_SNAP > 0)
	{
		Sys_Printf("Snapping ledge planes to %i units.\n", LEDGE_PLANE_SNAP);
	}


	//
	// Trees...
	//

	// Read all the tree info from the new .climate ini files...
	TREE_SCALE_MULTIPLIER = atof(IniRead(filename, "TREES", "treeScaleMultiplier", "1.0"));
	TREE_SCALE_MULTIPLIER_SPECIAL = atof(IniRead(filename, "TREES", "treeScaleMultiplierSpecial", "1.0"));

	TREE_PERCENTAGE = atof(IniRead(filename, "TREES", "treeAssignPercent", "100"));
	TREE_PERCENTAGE_SPECIAL = atof(IniRead(filename, "TREES", "treeAssignPercentSpecial", "100"));

	TREE_CLUSTER = atoi(IniRead(filename, "LEDGES", "treeCluster", "0")) > 0 ? qtrue : qfalse;
	TREE_CLUSTER_MINIMUM = atof(IniRead(filename, "TREES", "treeClusterMinimum", "0.5"));
	TREE_CLUSTER_SEED = atof(IniRead(filename, "TREES", "treeClusterSeed", "0.0"));

	//Sys_Printf("Tree scale for this climate is %.4f.\n", TREE_SCALE_MULTIPLIER);

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
		TREE_ALLOW_SIMPLIFY[i] = atoi(IniRead(filename, "TREES", va("treeAllowSimplify%i", i), "0"));
		TREE_USE_ORIGIN_AS_LOWPOINT[i] = (qboolean)atoi(IniRead(filename, "TREES", va("treeUseOriginAsLowPoint%i", i), "0"));
		strcpy(TREE_FORCED_OVERRIDE_SHADER[i], IniRead(filename, "TREES", va("overrideShader%i", i), ""));
		TREE_PLANE_SNAP[i] = atoi(IniRead(filename, "TREES", va("treePlaneSnap%i", i), "4"));
		TREE_ROADSCAN_MULTIPLIER[i] = atof(IniRead(filename, "TREES", va("treeRoadScanMultiplier%i", i), "1.0"));
		
		if (strcmp(TREE_MODELS[i], ""))
		{
			char fileName[512];
			strcpy(fileName, TREE_MODELS[i]);
			StripPath(fileName);
			Sys_Printf("Tree %i. Model %s. Offset %.4f. Scale %.4f. MaxAngle %i. Buffer %.4f. InstanceDist %.4f. ForceSolid: %s. PlaneSnap: %i. Shader: %s. RoadScale %.4f.\n", i, fileName, TREE_OFFSETS[i], TREE_SCALES[i], TREE_FORCED_MAX_ANGLE[i], TREE_FORCED_BUFFER_DISTANCE[i], TREE_FORCED_DISTANCE_FROM_SAME[i], TREE_FORCED_FULLSOLID[i] ? "true" : "false", TREE_PLANE_SNAP[i], TREE_FORCED_OVERRIDE_SHADER[i][0] != '\0' ? TREE_FORCED_OVERRIDE_SHADER[i] : "Default", TREE_ROADSCAN_MULTIPLIER[i]);
		}
	}

	for (i = 0; i < MAX_FOREST_MODELS; i++)
	{
		strcpy(PROCEDURAL_TREE_BARKS[PROCEDURAL_TREE_COUNT], IniRead(filename, "PROCEDURAL_TREES", va("treeBarkTexture%i", i), ""));

		if (strcmp(PROCEDURAL_TREE_BARKS[PROCEDURAL_TREE_COUNT], ""))
		{
			strcpy(PROCEDURAL_TREE_LEAFS[PROCEDURAL_TREE_COUNT], IniRead(filename, "PROCEDURAL_TREES", va("treeLeafTexture%i", i), ""));

			if (strcmp(PROCEDURAL_TREE_LEAFS[PROCEDURAL_TREE_COUNT], ""))
			{
				char barkFileName[512];
				strcpy(barkFileName, PROCEDURAL_TREE_BARKS[PROCEDURAL_TREE_COUNT]);
				StripPath(barkFileName);

				char leafFileName[512];
				strcpy(leafFileName, PROCEDURAL_TREE_LEAFS[PROCEDURAL_TREE_COUNT]);
				StripPath(leafFileName);

				Sys_Printf("Procedural Tree Type %i. Bark Shader %s. leaf Shader %s.\n", PROCEDURAL_TREE_COUNT, barkFileName, leafFileName);


				treePropertiesSegments[PROCEDURAL_TREE_COUNT] = atoi(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesSegments%i", i), "6"));
				treePropertiesLevels[PROCEDURAL_TREE_COUNT] = atoi(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesLevels%i", i), "5"));
				treePropertiesVMultiplier[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesVMultiplier%i", i), "0.36f"));
				treePropertiesTwigScale[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTwigScale%i", i), "0.39f"));
				treePropertiesInitialBranchLength[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesInitialBranchLength%i", i), "0.49f"));
				treePropertiesLengthFalloffFactor[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesLengthFalloffFactor%i", i), "0.85f"));
				treePropertiesLengthFalloffPower[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesLengthFalloffPower%i", i), "0.99f"));
				treePropertiesClumpMax[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesClumpMax%i", i), "0.454f"));
				treePropertiesClumpMin[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesClumpMin%i", i), "0.404f"));
				treePropertiesBranchFactor[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesBranchFactor%i", i), "2.45f"));
				treePropertiesDropAmount[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesDropAmount%i", i), "-0.1f"));
				treePropertiesGrowAmount[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesGrowAmount%i", i), "0.235f"));
				treePropertiesSweepAmount[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesSweepAmount%i", i), "0.01f"));
				treePropertiesMaxRadius[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesMaxRadius%i", i), "0.139f"));
				treePropertiesClimbRate[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesClimbRate%i", i), "0.371f"));
				treePropertiesTrunkKink[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTrunkKink%i", i), "0.093f"));
				treePropertiesTreeSteps[PROCEDURAL_TREE_COUNT] = atoi(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTreeSteps%i", i), "5"));
				treePropertiesTaperRate[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTaperRate%i", i), "0.947f"));
				treePropertiesRadiusFalloffRate[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesRadiusFalloffRate%i", i), "0.73f"));
				treePropertiesTwistRate[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTwistRate%i", i), "3.02f"));
				treePropertiesTrunkLength[PROCEDURAL_TREE_COUNT] = atof(IniRead(filename, "PROCEDURAL_TREES", va("treePropertiesTrunkLength%i", i), "2.4f"));


				PROCEDURAL_TREE_COUNT++;
			}
			else
			{
				strcpy(PROCEDURAL_TREE_BARKS[PROCEDURAL_TREE_COUNT], "");
				strcpy(PROCEDURAL_TREE_LEAFS[PROCEDURAL_TREE_COUNT], "");
			}
		}
		else
		{
			strcpy(PROCEDURAL_TREE_BARKS[PROCEDURAL_TREE_COUNT], "");
			strcpy(PROCEDURAL_TREE_LEAFS[PROCEDURAL_TREE_COUNT], "");
		}
	}

	// Read all the tree info from the new .climate ini files...
#if 0
	ADD_CITY_ROADS = (qboolean)atoi(IniRead(filename, "CITY", "addCityRoads", "0"));
#endif

	CITY_SCALE_MULTIPLIER = atof(IniRead(filename, "CITY", "cityScaleMultiplier", "1.0"));
	float CITY_LOCATION_X = atof(IniRead(filename, "CITY", "cityLocationX", "0"));
	float CITY_LOCATION_Y = atof(IniRead(filename, "CITY", "cityLocationY", "0"));
	float CITY_LOCATION_Z = atof(IniRead(filename, "CITY", "cityLocationZ", "0"));

	VectorSet(CITY_LOCATION, CITY_LOCATION_X, CITY_LOCATION_Y, CITY_LOCATION_Z);

	//Sys_Printf("Building scale for this climate is %.4f.\n", CITY_SCALE_MULTIPLIER);

	if (VectorLength(CITY_LOCATION) != 0.0)
	{
		CITY_RADIUS = atof(IniRead(filename, "CITY", "cityRadius", "0"));
		CITY_CLIFF_CULL_RADIUS = atof(IniRead(filename, "CITY", "cliffFacesCullScale", "1.0"));
		CITY_RANDOM_ANGLES = atoi(IniRead(filename, "CITY", "cityRandomizeAngles", "0"));
		CITY_BUFFER = atof(IniRead(filename, "CITY", "cityBuffer", "1024.0"));

		for (int i = 0; i < MAX_FOREST_MODELS; i++)
		{// Set default for each city model randomize based on the "do all" option above...
			CITY_ANGLES_RANDOMIZE[i] = CITY_RANDOM_ANGLES ? CITY_RANDOM_ANGLES : 0;
		}

		for (i = 0; i < MAX_FOREST_MODELS; i++)
		{
			strcpy(CITY_MODELS[i], IniRead(filename, "CITY", va("cityModel%i", i), ""));
			CITY_OFFSETS[i] = atof(IniRead(filename, "CITY", va("cityZoffset%i", i), "-4.0"));
			CITY_SCALES[i] = atof(IniRead(filename, "CITY", va("cityScale%i", i), "1.0"));
			CITY_SCALES_XY[i] = atof(IniRead(filename, "CITY", va("cityScaleXY%i", i), "1.0"));
			CITY_SCALES_Z[i] = atof(IniRead(filename, "CITY", va("cityScaleZ%i", i), "1.0"));
			CITY_CENTRAL_ONCE[i] = (qboolean)atoi(IniRead(filename, "CITY", va("cityCentralOneOnly%i", i), "0"));
			CITY_ANGLES_RANDOMIZE[i] = atoi(IniRead(filename, "CITY", va("cityRandomizeAngles%i", i), va("%i", CITY_ANGLES_RANDOMIZE[i])));
			CITY_ALLOW_ROAD[i] = (qboolean)atoi(IniRead(filename, "CITY", va("cityAllowOnRoad%i", i), "0"));
			CITY_FORCED_MAX_ANGLE[i] = atof(IniRead(filename, "CITY", va("cityForcedMaxAngle%i", i), "0.0"));
			CITY_FORCED_BUFFER_DISTANCE[i] = atof(IniRead(filename, "CITY", va("cityForcedBufferDistance%i", i), "0.0"));
			CITY_FORCED_DISTANCE_FROM_SAME[i] = atof(IniRead(filename, "CITY", va("cityForcedDistanceFromSame%i", i), "0.0"));
			CITY_FORCED_FULLSOLID[i] = atoi(IniRead(filename, "CITY", va("cityForcedFullSolid%i", i), "0"));
			CITY_ALLOW_SIMPLIFY[i] = atoi(IniRead(filename, "CITY", va("cityAllowSimplify%i", i), "0"));
			strcpy(CITY_FORCED_OVERRIDE_SHADER[i], IniRead(filename, "CITY", va("overrideShader%i", i), ""));
			CITY_PLANE_SNAP[i] = atoi(IniRead(filename, "CITY", va("cityPlaneSnap%i", i), "0"));

			if (strcmp(CITY_MODELS[i], ""))
			{
				char fileName[512];
				strcpy(fileName, CITY_MODELS[i]);
				StripPath(fileName);

				Sys_Printf("Building %i. Model %s. Offset %.4f. Scale %.4f (XY %.4f Z %.4f). MaxAngle %i. AngRand: %i. Buffer %.4f. InstanceBuffer %.4f. ForceSolid: %s. PlaneSnap: %i. Shader: %s. OneOnly: %s. AllowRoad: %s.\n", i, fileName, CITY_OFFSETS[i], CITY_SCALES[i], CITY_SCALES_XY[i], CITY_SCALES_Z[i], (int)CITY_FORCED_MAX_ANGLE[i], CITY_ANGLES_RANDOMIZE[i], CITY_FORCED_BUFFER_DISTANCE[i], CITY_FORCED_DISTANCE_FROM_SAME[i], CITY_FORCED_FULLSOLID[i] ? "true" : "false", CITY_PLANE_SNAP[i], CITY_FORCED_OVERRIDE_SHADER[i][0] != '\0' ? CITY_FORCED_OVERRIDE_SHADER[i] : "Default", CITY_CENTRAL_ONCE[i] ? "true" : "false", CITY_ALLOW_ROAD[i] ? "true" : "false");
			}
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
		STATIC_ALLOW_SIMPLIFY[i] = atoi(IniRead(filename, "STATIC", va("staticAllowSimplify%i", i), "0"));
		STATIC_PLANE_SNAP[i] = atoi(IniRead(filename, "STATIC", va("staticPlaneSnap%i", i), "4"));
		STATIC_FORCED_FULLSOLID[i] = atoi(IniRead(filename, "STATIC", va("staticForcedFullSolid%i", i), "0"));
		STATIC_MODEL_RADIUS[i] = atof(IniRead(filename, "STATIC", va("staticModelRadius%i", i), "0.0"));
		strcpy(STATIC_FORCED_OVERRIDE_SHADER[i], IniRead(filename, "STATIC", va("overrideShader%i", i), ""));

		if (strcmp(STATIC_MODEL[i], ""))
		{
			char fileName[512];
			strcpy(fileName, STATIC_MODEL[i]);
			StripPath(fileName);

			Sys_Printf("Static %i. Model %s. Origin %i %i %i. Angle %.4f. Scale %.4f. Plane Snap: %i. Forced Solid: %i.\n", i, fileName, (int)STATIC_ORIGIN[i][0], (int)STATIC_ORIGIN[i][1], (int)STATIC_ORIGIN[i][2], (float)STATIC_ANGLE[i], (float)STATIC_SCALE[i], STATIC_PLANE_SNAP[i], STATIC_FORCED_FULLSOLID[i]);
		}
	}

	//
	// Experimental Skyscrapers.
	//

	ADD_SKYSCRAPERS = (qboolean)atoi(IniRead(filename, "SKYSCRAPERS", "addSkyscrapers", "0"));

	float SKYSCRAPERS_CENTER_X = atof(IniRead(filename, "SKYSCRAPERS", "cityCenterX", "0"));
	float SKYSCRAPERS_CENTER_Y = atof(IniRead(filename, "SKYSCRAPERS", "cityCenterY", "0"));

	SKYSCRAPERS_RADIUS = atof(IniRead(filename, "SKYSCRAPERS", "cityRadius", "0"));

	SKYSCRAPERS_PLANE_SNAP = atoi(IniRead(filename, "SKYSCRAPERS", "cityPlaneSnap", "8"));

	VectorSet(SKYSCRAPERS_CENTER, SKYSCRAPERS_CENTER_X, SKYSCRAPERS_CENTER_Y, 0.0);

	//
	// Preload the models needed...
	//

	PreloadClimateModels();
}


void vectoangles(const vec3_t value1, vec3_t angles);
extern qboolean StringContainsWord(const char *haystack, const char *needle);
extern bool NO_BOUNDS_IMPROVEMENT;

#ifdef __SKIPIFY__
void CaulkifyStuff(qboolean findBounds)
{
	if (!CAULKIFY_CRAP) return;

	Sys_PrintHeading("--- CaulkifyJunk ---\n");

	int numCalkified = 0;
	int numNoDrawAdded = 0;
	int numSkipAdded = 0;
	shaderInfo_t *caulkShader = ShaderInfoForShader("textures/system/skip");

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

		if (!StringContainsWord(si->shader, "skyscraper") && StringContainsWord(si->shader, "sky"))
		{// Never on skies...
			continue;
		}

		if (StringContainsWord(si->shader, "system/skip"))
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

	if (findBounds && !NO_BOUNDS_IMPROVEMENT)
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
				&& !(!StringContainsWord(si->shader, "skyscraper") && StringContainsWord(si->shader, "sky"))
				&& !StringContainsWord(si->shader, "system/skip")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (!StringContainsWord(si->shader, "/sand"))
				//Sys_Printf("ds %i [%s] bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", s, si->shader, ds->mins[0], ds->mins[1], ds->mins[2], ds->maxs[0], ds->maxs[1], ds->maxs[2]);
				AddPointToBounds(ds->mins, mapPlayableMins, mapPlayableMaxs);
				AddPointToBounds(ds->maxs, mapPlayableMins, mapPlayableMaxs);
			}
		}

		// Override playable maxs height with the full map version, we only want the lower extent of playable area...
		mapMaxs[2] = mapPlayableMaxs[2];

		Sys_Printf("Old map bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", oldMapPlayableMins[0], oldMapPlayableMins[1], oldMapPlayableMins[2], oldMapPlayableMaxs[0], oldMapPlayableMaxs[1], oldMapPlayableMaxs[2]);
		Sys_Printf("New map bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", mapPlayableMins[0], mapPlayableMins[1], mapPlayableMins[2], mapPlayableMaxs[0], mapPlayableMaxs[1], mapPlayableMaxs[2]);
	}

	Sys_Printf("%d shaders set to nodraw.\n", numNoDrawAdded);
	Sys_Printf("%d shaders set to skip.\n", numSkipAdded);
	Sys_Printf("%d of %d surfaces were caulkified.\n", numCalkified, numMapDrawSurfs);
}
#else //!__SKIPIFY__
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

		if (!StringContainsWord(si->shader, "skyscraper") && StringContainsWord(si->shader, "sky"))
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

	if (findBounds && !NO_BOUNDS_IMPROVEMENT)
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
				&& !(!StringContainsWord(si->shader, "skyscraper") && StringContainsWord(si->shader, "sky"))
				&& !StringContainsWord(si->shader, "caulk")
				&& !StringContainsWord(si->shader, "common/water"))
			{
				//if (!StringContainsWord(si->shader, "/sand"))
				//Sys_Printf("ds %i [%s] bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", s, si->shader, ds->mins[0], ds->mins[1], ds->mins[2], ds->maxs[0], ds->maxs[1], ds->maxs[2]);
				AddPointToBounds(ds->mins, mapPlayableMins, mapPlayableMaxs);
				AddPointToBounds(ds->maxs, mapPlayableMins, mapPlayableMaxs);
			}
		}

		// Override playable maxs height with the full map version, we only want the lower extent of playable area...
		mapMaxs[2] = mapPlayableMaxs[2];

		Sys_Printf("Old map bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", oldMapPlayableMins[0], oldMapPlayableMins[1], oldMapPlayableMins[2], oldMapPlayableMaxs[0], oldMapPlayableMaxs[1], oldMapPlayableMaxs[2]);
		Sys_Printf("New map bounds %.4f %.4f %.4f x %.4f %.4f %.4f.\n", mapPlayableMins[0], mapPlayableMins[1], mapPlayableMins[2], mapPlayableMaxs[0], mapPlayableMaxs[1], mapPlayableMaxs[2]);
	}

	Sys_Printf("%d shaders set to nodraw.\n", numNoDrawAdded);
	Sys_Printf("%d shaders set to skip.\n", numSkipAdded);
	Sys_Printf("%d of %d surfaces were caulkified.\n", numCalkified, numMapDrawSurfs);
}
#endif //__SKIPIFY__

qboolean MapEntityNear(vec3_t origin)
{
	for (int i = 0; i < numEntities; i++)
	{
		entity_t *entity = &entities[i];

		if (!entity) continue;

		/* register class */
		const char *classname = ValueForKey(entity, "classname");

		/* register model */
		/*if (!strcmp(classname, "info_player_start")
		|| !strcmp(classname, "info_player_start_blue")
		|| !strcmp(classname, "info_player_start_red")
		|| !strcmp(classname, "info_player_start")
		|| !strcmp(classname, "info_player_start")
		|| !strcmp(classname, "info_player_start"))
		{

		}*/

		if (Distance(origin, entity->origin) <= 256.0)
		{
			return qtrue;
		}
	}

	return qfalse;
}

qboolean StaticObjectNear(vec3_t origin)
{
	for (int i = 0; i < MAX_STATIC_ENTITY_MODELS; i++)
	{
		if (STATIC_MODEL[i][0] != 0 && strlen(STATIC_MODEL[i]) > 0)
		{
			if (Distance(origin, STATIC_ORIGIN[i]) <= STATIC_MODEL_RADIUS[i] * STATIC_SCALE[i])
			{
				return qtrue;
			}
		}
	}

	return qfalse;
}


#define			FOLIAGE_MAX_FOLIAGES 2097152

int				FOLIAGE_NUM_POSITIONS = 0;
int				FOLIAGE_NOT_GROUND_COUNT = 0;
qboolean		*FOLIAGE_ASSIGNED;
vec3_t			FOLIAGE_POSITIONS[FOLIAGE_MAX_FOLIAGES];
vec3_t			FOLIAGE_NORMALS[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_NOT_GROUND[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_ANGLES[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_TREE_SELECTION[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_SCALE[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_BUFFER[FOLIAGE_MAX_FOLIAGES];

float			FOLIAGE_TREE_PITCH[FOLIAGE_MAX_FOLIAGES];

void FOLIAGE_PrecalculateFoliagePitches(void)
{
	if (FOLIAGE_NUM_POSITIONS > 0)
	{
		//Sys_PrintHeading("--- PrecalculatePitches ---\n");

		int done = 0;

#pragma omp parallel for num_threads(numthreads)
		for (int i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			vec3_t angles;

#pragma omp critical
			{
				printLabelledProgress("PrecalculatePitches", done, FOLIAGE_NUM_POSITIONS);
				done++;
			}

			vectoangles(FOLIAGE_NORMALS[i], angles);

			float pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;

			FOLIAGE_TREE_PITCH[i] = pitch;
		}

		printLabelledProgress("PrecalculatePitches", FOLIAGE_NUM_POSITIONS, FOLIAGE_NUM_POSITIONS);
	}
}

void ProceduralGenFoliage(void)
{// TODO: Skip the flat base surface in the .map file?
	if (FOLIAGE_NUM_POSITIONS > 0) return;

	if (TREE_PERCENTAGE <= 0 && TREE_PERCENTAGE_SPECIAL <= 0) return;
	
	int				FOLIAGE_COUNT = 0;
	float			scanDensity = CHUNK_FOLIAGE_GENERATOR_SCATTER;

	Sys_PrintHeading("--- Finding Map Bounds ---\n");

	vec3_t bounds[2];
	VectorCopy(mapMins, bounds[0]);
	VectorCopy(mapMaxs, bounds[1]);

	Sys_PrintHeading("--- Generating Foliage Points ---\n");

	Sys_Printf("* Map bounds are %f %f %f x %f %f %f.\n", bounds[0][0], bounds[0][1], bounds[0][2], bounds[1][0], bounds[1][1], bounds[1][2]);

	int solidFlags = C_SOLID;
	int numCompleted = 0;
	int workloadTotal = 0;

	for (int i = 0; i < numMapDrawSurfs; i++)
	{
		mapDrawSurface_t *surf = &mapDrawSurfs[i];
		shaderInfo_t *si = surf->shaderInfo;

		if (surf->mapBrush && surf->mapBrush->isMapFileBrush)
		{// Is an original .map brush, skip these...
			continue;
		}

		if (!(si->contentFlags & solidFlags)) {
			continue;
		}

		if (StringContainsWord(si->shader, "caulk"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "system/skip"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "sky"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "skies"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "water"))
		{
			continue;
		}


		extern int MaterialIsValidForFoliage(int materialType);
		int surfaceMaterialValidity = MaterialIsValidForFoliage(si->materialType);

		if (!si || !surfaceMaterialValidity)
		{
			continue;
		}

		workloadTotal += surf->numIndexes / 3;
	}

//#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
	for (int i = 0; i < numMapDrawSurfs; i++)
	{
/*#pragma omp critical (__PROGRESS__)
		{
			printLabelledProgress("GenFoliage", numCompleted, numMapDrawSurfs);
			numCompleted++;
		}*/

		mapDrawSurface_t *surf = &mapDrawSurfs[i];
		shaderInfo_t *si = surf->shaderInfo;

		if (surf->mapBrush && surf->mapBrush->isMapFileBrush)
		{// Is an original .map brush, skip these...
/*
#pragma omp critical (__MAP_BRUSH__)
			{
				Sys_Printf("Surf %i is skipped (original map brush).\n", i);
			}
*/
			continue;
		}
		
		if (!(si->contentFlags & solidFlags)) {
			continue;
		}

		if (StringContainsWord(si->shader, "caulk"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "system/skip"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "sky"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "skies"))
		{
			continue;
		}

		if (StringContainsWord(si->shader, "water"))
		{
			continue;
		}


		extern const char *materialNames[MATERIAL_LAST];
		extern int MaterialIsValidForFoliage(int materialType);
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

		bspDrawVert_t *vs = &surf->verts[0];
		int *idxs = surf->indexes;

#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
		for (int j = 0; j < surf->numIndexes; j += 3)
		{
#pragma omp critical (__PROGRESS__)
			{
				printLabelledProgress("GenFoliage", numCompleted, workloadTotal);
			}
			numCompleted++;

			vec3_t verts[3];

			VectorCopy(vs[idxs[j]].xyz, verts[0]);
			VectorCopy(vs[idxs[j + 1]].xyz, verts[1]);
			VectorCopy(vs[idxs[j + 2]].xyz, verts[2]);

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

			// Optimization for crazy high-poly models... Will sadly slow low-poly ones somewhat...
			qboolean badDensity = qfalse;

			for (int z = 0; z < FOLIAGE_COUNT; z++)
			{
				if (Distance(verts[0], FOLIAGE_POSITIONS[z]) < scanDensity
					|| Distance(verts[1], FOLIAGE_POSITIONS[z]) < scanDensity
					|| Distance(verts[2], FOLIAGE_POSITIONS[z]) < scanDensity)
				{// Already have one here...
					badDensity = qtrue;
					break;
				}
			}

			if (badDensity)
				continue;

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
			int numPossibleFoliages = max((int)(((tringleSize*tringleSize) / 2.0) / scanDensity), 1);

			// Make a temp list of foliage positions, so we can more quickly scan if theres already a point too close...
			const int	THIS_TRI_MAX_FOLIAGES = 8192;
			int			THIS_TRI_FOLIAGE_COUNT = 0;
			vec3_t		THIS_TRI_FOLIAGES[THIS_TRI_MAX_FOLIAGES];

			for (int f = 0; f < numPossibleFoliages; f++)
			{
				if (THIS_TRI_FOLIAGE_COUNT + 1 >= THIS_TRI_MAX_FOLIAGES)
					break;

				extern void randomBarycentricCoordinate(vec3_t v1, vec3_t v2, vec3_t v3, vec3_t &pos);

				vec3_t pos;
				randomBarycentricCoordinate(verts[0], verts[1], verts[2], pos);

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

			/*
			vec3_t			FOLIAGE_POSITIONS[FOLIAGE_MAX_FOLIAGES];
vec3_t			FOLIAGE_NORMALS[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_NOT_GROUND[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_ANGLES[FOLIAGE_MAX_FOLIAGES];
int				FOLIAGE_TREE_SELECTION[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_SCALE[FOLIAGE_MAX_FOLIAGES];
float			FOLIAGE_TREE_BUFFER[FOLIAGE_MAX_FOLIAGES];
			*/

			// Now add the new foliages to the final list...
			for (int f = 0; f < THIS_TRI_FOLIAGE_COUNT; f++)
			{
				if (FOLIAGE_COUNT + 1 >= FOLIAGE_MAX_FOLIAGES)
					break;

#pragma omp critical (__ADD_FOLIAGE__)
				{
					//printf("DEBUG: adding at pos %f %f %f.\n", THIS_TRI_FOLIAGES[f][0], THIS_TRI_FOLIAGES[f][1], THIS_TRI_FOLIAGES[f][2]);
					VectorCopy(THIS_TRI_FOLIAGES[f], FOLIAGE_POSITIONS[FOLIAGE_COUNT]);
					VectorCopy(normal, FOLIAGE_NORMALS[FOLIAGE_COUNT]);
					FOLIAGE_POSITIONS[FOLIAGE_COUNT][2] -= 8.0;

					FOLIAGE_TREE_SELECTION[FOLIAGE_COUNT/*f*/] = 1;
					FOLIAGE_TREE_ANGLES[FOLIAGE_COUNT/*f*/] = 0.0;
					FOLIAGE_TREE_SCALE[FOLIAGE_COUNT/*f*/] = 1.0;
					FOLIAGE_NOT_GROUND[FOLIAGE_COUNT] = qfalse;

					FOLIAGE_COUNT++;
				}
			}
		}
	}

	printLabelledProgress("GenFoliage", workloadTotal, workloadTotal);

	/*for (int i = 0; i < FOLIAGE_COUNT; i++)
	{
	printf("DEBUG: pos %f %f %f.\n", FOLIAGE_POINTS[i][0], FOLIAGE_POINTS[i][1], FOLIAGE_POINTS[i][2]);
	}*/

	Sys_Printf("* Generated %i foliage positions.\n", FOLIAGE_COUNT);


	FOLIAGE_NUM_POSITIONS = FOLIAGE_COUNT;

	if (FOLIAGE_NUM_POSITIONS > 0)
	{
		FOLIAGE_PrecalculateFoliagePitches();
	}

	generateforest = qtrue;
	generatecity = qtrue;
}

qboolean FOLIAGE_LoadFoliagePositions( char *filename )
{
	FILE			*f;
	int				i = 0;
	int				fileCount = 0;
	int				treeCount = 0;

	Sys_PrintHeading("--- LoadFoliagePositions ---\n");

	LoadRoadMapImage();

	f = fopen (filename, "rb");

	if ( !f )
	{
		return qfalse;
	}

	fread( &fileCount, sizeof(int), 1, f );

	qboolean dontUseGroundSystem = qfalse;

	for (i = 0; i < fileCount; i++)
	{
		//vec3_t	unneededVec3;
		//int		unneededInt;
		float	unneededFloat;

		fread( &FOLIAGE_POSITIONS[treeCount], sizeof(vec3_t), 1, f );
		fread( &FOLIAGE_NORMALS[treeCount], sizeof(vec3_t), 1, f );
		//fread( &unneededInt, sizeof(int), 1, f );
		fread( &FOLIAGE_NOT_GROUND[treeCount], sizeof(int), 1, f);
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &unneededFloat, sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SELECTION[treeCount], sizeof(int), 1, f );
		fread( &FOLIAGE_TREE_ANGLES[treeCount], sizeof(float), 1, f );
		fread( &FOLIAGE_TREE_SCALE[treeCount], sizeof(float), 1, f );

		if (FOLIAGE_NOT_GROUND[treeCount] > 2)
		{// Ingame generated points, mark this info as invalid for the whole file...
			dontUseGroundSystem = qtrue;
		}

		//if (FOLIAGE_TREE_SELECTION[treeCount] > 0)
		{// Only keep positions with trees...
			treeCount++;
		}
	}

	FOLIAGE_NUM_POSITIONS = treeCount;

	if (dontUseGroundSystem)
	{// Reset these values to default, since the system is not valid with ingame generated points.
		for (int i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			FOLIAGE_NOT_GROUND[i] = 0;
		}
	}
	else
	{// Using the not-ground system, so change all the values from 1 and 2 to 0 and 1 (on and off).
		for (int i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			if (FOLIAGE_NOT_GROUND[i] == 1)
			{
				FOLIAGE_NOT_GROUND[i] = 0;
			}
			else
			{
				FOLIAGE_NOT_GROUND[i] = 1;
				FOLIAGE_NOT_GROUND_COUNT++;
			}
		}
	}

	fclose(f);

	Sys_Printf("%d positions loaded (%i special surfaces) from %s.\n", FOLIAGE_NUM_POSITIONS, FOLIAGE_NOT_GROUND_COUNT, filename);

	FOLIAGE_PrecalculateFoliagePitches();

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

void GenerateChunks(void)
{
	if (!CHUNK_MODELS_TOTAL) return;

	Sys_PrintHeading("--- GenerateChunks ---\n");

	// Work out where to put these chunks...
	int			numChunkPositions = 0;
	int			numScaledEdgeChunks = 0;
	qboolean	chunkPositionIsEdge[65536] = { { qfalse } };
	vec3_t		chunkPositions[65536] = { { 0 } };

	#define defaultChunkDistance 7680.0//6144.0
	float chunkSize = defaultChunkDistance * CHUNK_SCALE;

	for (int x = mapMins[0]; x <= mapMaxs[0]; x += chunkSize)
	{
		qboolean isMapEdgeX = qfalse;

		if (x <= mapMins[0] + (chunkSize * 0.75))
			isMapEdgeX = qtrue;
		
		if (x >= mapMaxs[0] - (chunkSize * 0.75))
			isMapEdgeX = qtrue;

		if (!CHUNK_ADD_AT_MAP_EDGES && isMapEdgeX)
			continue;

		for (int y = mapMins[1]; y <= mapMaxs[1]; y += chunkSize)
		{
			qboolean isMapEdgeY = qfalse;

			if (y <= mapMins[1] + (chunkSize * 0.75))
				isMapEdgeY = qtrue;

			if (y >= mapMaxs[1] - (chunkSize * 0.75))
				isMapEdgeY = qtrue;

			if (!CHUNK_ADD_AT_MAP_EDGES && isMapEdgeY)
				continue;

			if (isMapEdgeX || isMapEdgeY)
			{
				chunkPositionIsEdge[numChunkPositions] = qtrue;
				numScaledEdgeChunks++;
			}
			else
			{
				chunkPositionIsEdge[numChunkPositions] = qfalse;
			}

			VectorSet(chunkPositions[numChunkPositions], x, y, CHUNK_MAP_LEVEL);
			numChunkPositions++;
		}
	}

	Sys_Printf("Found %i chunk positions. %i will be scaled for map edge.\n", numChunkPositions, numScaledEdgeChunks);

	for (int i = 0; i < numChunkPositions; i++)
	{
		printLabelledProgress("GenerateChunks", i, numChunkPositions);

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

		VectorCopy(chunkPositions[i], mapEnt->origin);

		{
			char str[32];
			sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		float baseScale = 30.0 * CHUNK_SCALE;

		if (chunkPositionIsEdge[i])
		{
			int chunkModelChoice = irand(0, CHUNK_MAP_EDGE_MODELS_TOTAL - 1);

			//Sys_Printf("Chunk at %.4f %.4f %.4f was scaled up for map edge.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			char str[128];
			sprintf(str, "%.4f %.4f %.4f", baseScale * CHUNK_MAP_EDGE_SCALE_XY, baseScale * CHUNK_MAP_EDGE_SCALE_XY, baseScale * CHUNK_MAP_EDGE_SCALE);
			SetKeyValue(mapEnt, "modelscale_vec", str);

			SetKeyValue(mapEnt, "model", CHUNK_MAP_EDGE_MODELS[chunkModelChoice]);

			if (CHUNK_MAP_EDGE_OVERRIDE_SHADER[0] != '\0')
			{
				SetKeyValue(mapEnt, "_overrideShader", CHUNK_MAP_EDGE_OVERRIDE_SHADER[chunkModelChoice]);
			}
		}
		else
		{
			int chunkModelChoice = irand(0, CHUNK_MODELS_TOTAL - 1);

			//Sys_Printf("Chunk at %.4f %.4f %.4f was NOT scaled up for map edge.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			char str[128];
			sprintf(str, "%.4f %.4f %.4f", baseScale, baseScale, baseScale);
			SetKeyValue(mapEnt, "modelscale_vec", str);

			SetKeyValue(mapEnt, "model", CHUNK_MODELS[chunkModelChoice]);

			if (CHUNK_OVERRIDE_SHADER[0] != '\0')
			{
				SetKeyValue(mapEnt, "_overrideShader", CHUNK_OVERRIDE_SHADER[chunkModelChoice]);
			}
		}

		{
			char str[32];
			sprintf(str, "%.4f", irand(0, 360) - 180.0);
			SetKeyValue(mapEnt, "angle", str);
		}

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
			SetKeyValue(mapEnt, "classname", "misc_model");

		classname = ValueForKey(mapEnt, "classname");

		SetKeyValue(mapEnt, "snap", va("%i", CHUNK_PLANE_SNAP));

		SetKeyValue(mapEnt, "_originAsLowPoint", "2");

		//Sys_Printf( "Generated chunk at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );

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

//#if defined(__ADD_PROCEDURALS_EARLY__)
	AddTriangleModels(0, qfalse, qfalse, qtrue, qfalse);
	CaulkifyStuff(qfalse);
	EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
	//MoveBrushesToWorld( mapEnt );
//#endif
}

void GenerateCenterChunk(void)
{
	if (!CENTER_CHUNK_MODEL[0]) return;

	Sys_PrintHeading("--- GenerateCenterChunk ---\n");

	{
		printLabelledProgress("AddCenterChunk", 0, 1);

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

		VectorSet(mapEnt->origin, 0.0, 0.0, CENTER_CHUNK_MAP_LEVEL);

		{
			char str[32];
			sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		float baseScale = CENTER_CHUNK_SCALE;// *20.0;


		char str[128];
		sprintf(str, "%.4f %.4f %.4f", baseScale, baseScale, baseScale);
		SetKeyValue(mapEnt, "modelscale_vec", str);

		SetKeyValue(mapEnt, "model", CENTER_CHUNK_MODEL);

		if (CENTER_CHUNK_OVERRIDE_SHADER[0] != '\0')
		{
			SetKeyValue(mapEnt, "_overrideShader", CENTER_CHUNK_OVERRIDE_SHADER);
		}

		{
			char str[32];
			sprintf(str, "%.4f", CENTER_CHUNK_ANGLE);
			SetKeyValue(mapEnt, "angle", str);
		}

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
			SetKeyValue(mapEnt, "classname", "misc_model");

		classname = ValueForKey(mapEnt, "classname");

		SetKeyValue(mapEnt, "snap", va("%i", CHUNK_PLANE_SNAP));

		SetKeyValue(mapEnt, "_originAsLowPoint", "2");

		//Sys_Printf( "Generated chunk at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );

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

	printLabelledProgress("AddCenterChunk", 1, 1);

	//#if defined(__ADD_PROCEDURALS_EARLY__)
	AddTriangleModels(0, qtrue, qfalse, qfalse, qtrue);
	CaulkifyStuff(qfalse);
	EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
	//MoveBrushesToWorld( mapEnt );
	//#endif
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
				//	Sys_Printf("pitch %.4f.\n", pitch);

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

			if (maxs[2] < MAP_WATER_LEVEL)
			{
				continue;
			}

			if (StaticObjectNear(center))
			{
				continue;
			}

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

				if (dist <= STATIC_BUFFER_RANGE * STATIC_SCALE[k])
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

			//Sys_Printf("cliff found at %.4f %.4f %.4f. angles0 %.4f %.4f %.4f. angles1 %.4f %.4f %.4f. angles2 %.4f %.4f %.4f.\n", center[0], center[1], center[2], angles[0][0], angles[0][1], angles[0][2], angles[1][0], angles[1][1], angles[1][2], angles[2][0], angles[2][1], angles[2][2]);

			cliffScale[numCliffs] = (smallestSize * CLIFF_FACES_SCALE_XY) / 64.0;
			cliffHeight[numCliffs] = (smallestSize * CLIFF_FACES_SCALE_Z) / 64.0;
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

		mapEnt->alreadyAdded = qfalse;
		mapEnt->mapEntityNum = 0;

		VectorCopy(cliffPositions[i], mapEnt->origin);

		{
			char str[32];
			sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		char str[128];
		sprintf(str, "%.4f %.4f %.4f", cliffScale[i], cliffScale[i], cliffHeight[i]);
		SetKeyValue(mapEnt, "modelscale_vec", str);

		{
			char str[32];
			//sprintf(str, "%.4f", irand(0, 360) - 180.0);// cliffAngles[i][1] - 180.0);
			sprintf(str, "%.4f", cliffAngles[i][1] - 180.0);
			SetKeyValue(mapEnt, "angle", str);
		}

		if (CLIFF_SHADER[0] != '\0')
		{
			SetKeyValue(mapEnt, "_overrideShader", CLIFF_SHADER);
		}

		

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
			SetKeyValue(mapEnt, "classname", "misc_model");

		classname = ValueForKey(mapEnt, "classname");

		if (CLIFF_CHEAP)
		{
			SetKeyValue(mapEnt, "model", va("models/warzone/rocks/cliffface0%i.md3", irand(4, 5)));
		}
		else
		{
			if (CLIFF_MODELS_TOTAL > 0)
			{// Using custom list...
				SetKeyValue(mapEnt, "model", CLIFF_MODEL[irand(0, CLIFF_MODELS_TOTAL-1)]);
			}
			else
			{
				SetKeyValue(mapEnt, "model", va("models/warzone/rocks/cliffface0%i.md3", irand(1, 5)));
			}
		}

		if (CLIFF_PLANE_SNAP > 0)
		{
			SetKeyValue(mapEnt, "snap", va("%i", CLIFF_PLANE_SNAP));
		}

		SetKeyValue(mapEnt, "_allowCollisionModelTypes", "3"); // allow convex or simplified collision models only...

		//Sys_Printf( "Generated cliff face at %.4f %.4f %.4f. Angle %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], cliffAngles[i][1] );

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


#if defined(__ADD_PROCEDURALS_EARLY__)
		AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
		EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
		//MoveBrushesToWorld( mapEnt );
		numEntities--;
#endif
	}
}

int			numRoads = 0;
vec3_t		roadPositions[65536];
vec3_t		roadAngles[65536];
float		roadScale[65536];
int			roadIsLowAngle[65536];

void GenerateCityRoads(void)
{
	if (!ADD_CITY_ROADS) return;

#if 0
	Sys_PrintHeading("--- GenerateRoads ---\n");

	numRoads = 0;

	//Sys_Printf("%i map draw surfs.\n", numMapDrawSurfs);

	for (int s = 0; s < numMapDrawSurfs; s++)
	{
		printLabelledProgress("AddRoads", s, numMapDrawSurfs);

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
		{// Don't add roads under water...
			continue;
		}

		/*if (ROAD_CHECK_SHADER[0] != '\0')
		{
		if (strcmp(si->shader, ROAD_CHECK_SHADER))
		{// Not the mapper specified shader to apply ledges to, skip...
		continue;
		}
		}*/

		//Sys_Printf("drawsurf %i. %i indexes. %i verts.\n", s, ds->numIndexes, ds->numVerts);

		if (ds->numIndexes == 0 && ds->numVerts == 3)
		{
			qboolean	isRoad = qfalse;
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
				//	Sys_Printf("pitch %.4f.\n", pitch);

				if (pitch == 180.0 || pitch == -180.0)
				{// Horrible hack to skip the surfaces created under the map by q3map2 code... Why are boxes needed for triangles?
					continue;
				}
				else if (pitch <= 16 && pitch >= -16)
				{
					isRoad = qtrue;
				}
				else
				{
					continue;
				}
			}

			if (!isRoad)
			{
				continue;
			}

			vec3_t center;
			vec3_t size;
			float smallestSize;

			center[0] = (mins[0] + maxs[0]) * 0.5f;
			center[1] = (mins[1] + maxs[1]) * 0.5f;
			center[2] = (mins[2] + maxs[2]) * 0.5f;

			qboolean bad = qtrue;

			if (VectorLength(CITY_LOCATION) != 0.0)
			{
				if (Distance(center, CITY_LOCATION) <= CITY_RADIUS + CITY_BUFFER)
				{
					bad = qfalse;
				}
			}

			if (VectorLength(CITY2_LOCATION) != 0.0)
			{
				if (Distance(center, CITY2_LOCATION) <= CITY2_RADIUS + CITY_BUFFER)
				{
					bad = qfalse;
				}
			}

			if (VectorLength(CITY3_LOCATION) != 0.0)
			{
				if (Distance(center, CITY3_LOCATION) <= CITY3_RADIUS + CITY_BUFFER)
				{
					bad = qfalse;
				}
			}

			if (VectorLength(CITY4_LOCATION) != 0.0)
			{
				if (Distance(center, CITY4_LOCATION) <= CITY4_RADIUS + CITY_BUFFER)
				{
					bad = qfalse;
				}
			}

			if (VectorLength(CITY5_LOCATION) != 0.0)
			{
				if (Distance(center, CITY5_LOCATION) <= CITY5_RADIUS + CITY_BUFFER)
				{
					bad = qfalse;
				}
			}

			if (bad)
			{// Not in range of a city... Skip...
				continue;
			}

			//
			// ok, seems this position is in range of a city area...
			//
			size[0] = (maxs[0] - mins[0]);
			size[1] = (maxs[1] - mins[1]);
			size[2] = (maxs[2] - mins[2]);

			smallestSize = size[0];
			if (size[1] < smallestSize) smallestSize = size[1];
			if (size[2] < smallestSize) smallestSize = size[2];

			//Sys_Printf("road found at %.4f %.4f %.4f. angles0 %.4f %.4f %.4f. angles1 %.4f %.4f %.4f. angles2 %.4f %.4f %.4f.\n", center[0], center[1], center[2], angles[0][0], angles[0][1], angles[0][2], angles[1][0], angles[1][1], angles[1][2], angles[2][0], angles[2][1], angles[2][2]);

			roadScale[numRoads] = 2.0;//(smallestSize * CITY_ROADS_SCALE) / 64.0;
			VectorCopy(center, roadPositions[numRoads]);
			VectorCopy(angles[0], roadAngles[numRoads]);
			roadAngles[numRoads][0] += 90.0;
			numRoads++;

			for (int j = 0; j < 3; j++)
			{
				/* get vertex */
				bspDrawVert_t		*dv = &ds->verts[j];

				roadScale[numRoads] = 2.0;//(smallestSize * CITY_ROADS_SCALE) / 64.0;
				VectorCopy(dv->xyz, roadPositions[numRoads]);
				VectorCopy(angles[0], roadAngles[numRoads]);
				roadAngles[numRoads][0] += 90.0;
				numRoads++;
			}
		}
	}

	Sys_Printf("Found %i road surfaces.\n", numRoads);

	for (int i = 0; i < numRoads; i++)
	{
		printLabelledProgress("GenerateRoads", i, numRoads);

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

		mapEnt->alreadyAdded = qfalse;
		mapEnt->mapEntityNum = 0;

		VectorCopy(roadPositions[i], mapEnt->origin);

		{
			char str[32];
			sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]+4.0);
			SetKeyValue(mapEnt, "origin", str);
		}

		{
			char str[32];
			sprintf(str, "%.4f", roadScale[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}

		{
			//char str[32];
			//sprintf(str, "%.4f", roadAngles[i][1] - 180.0);
			//SetKeyValue(mapEnt, "angle", str);

			char str[32];
			sprintf(str, "%.4f %.4f %.4f", roadAngles[i][0], roadAngles[i][1]/* - 180.0*/, roadAngles[i][2]);
			SetKeyValue(mapEnt, "angles", str);
		}

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
			SetKeyValue(mapEnt, "classname", "misc_model");

		classname = ValueForKey(mapEnt, "classname");

		int choice = irand(1, 2);
		//SetKeyValue(mapEnt, "model", va("models/warzone/roads/road0%i.md3", choice));
		SetKeyValue(mapEnt, "model", "models/warzone/roads/road01.md3");
		//if (ROAD_SHADER[0] != '\0')
		//{// Override the whole model's shader...
		//	SetKeyValue(mapEnt, "_overrideShader", ROAD_SHADER);
		//}

		//Sys_Printf( "Generated cliff face at %.4f %.4f %.4f. Angle %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], roadAngles[i][1] );

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


#if defined(__ADD_PROCEDURALS_EARLY__)
		AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
		EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
		//MoveBrushesToWorld( mapEnt );
		numEntities--;
#endif
	}
#endif
}

int			numSkyscrapers = 0;
vec3_t		skyscraperPositions[65536];
vec3_t		skyscraperScale[65536];
int			skyscraperLevels[65536];
qboolean	skyscraperCBD[65536] = { qfalse };
qboolean	skyscraperCentral[65536] = { qfalse };

float		fLength(float x, float y)
{
	float diff = x - y;
	if (diff < 0) diff *= -1.0;
	return diff;
}

void GenerateSkyscrapers(void)
{
	if (!ADD_SKYSCRAPERS) return;

	Sys_PrintHeading("--- GenerateSkyscrapers ---\n");

	numSkyscrapers = 0;

	//Sys_Printf("%i map draw surfs.\n", numMapDrawSurfs);

	int totalCount = 0;
	int thisCount = 0;

	for (float x = mapPlayableMins[0]; x <= mapPlayableMaxs[0]; x += 2048.0)
	{
		for (float y = mapPlayableMins[1]; y <= mapPlayableMaxs[1]; y += 2048.0)
		{
			vec3_t pos;
			VectorSet(pos, x, y, SKYSCRAPERS_CENTER[2]);
			if (!SKYSCRAPERS_RADIUS || DistanceHorizontal(SKYSCRAPERS_CENTER, pos) + 2048.0 <= SKYSCRAPERS_RADIUS)
				totalCount++;
		}
	}

	for (float x = mapPlayableMins[0]; x <= mapPlayableMaxs[0]; x += 2048.0)
	{
		for (float y = mapPlayableMins[1]; y <= mapPlayableMaxs[1]; y += 2048.0)
		{
			vec3_t pos;
			VectorSet(pos, x, y, SKYSCRAPERS_CENTER[2]);
			if (SKYSCRAPERS_RADIUS || DistanceHorizontal(SKYSCRAPERS_CENTER, pos) + 2048.0 > SKYSCRAPERS_RADIUS)
				continue;

			qboolean isMapEdge = qfalse;

			thisCount++;
			printLabelledProgress("AddSkyscrapers", thisCount, totalCount);

			if (fLength(x, mapPlayableMins[0]) < 128.0 || fLength(x, mapPlayableMaxs[0]) < 128.0 || fLength(y, mapPlayableMins[1]) < 128.0 || fLength(y, mapPlayableMaxs[1]) < 128.0)
			{
				isMapEdge = qtrue;
			}

			int		bestSurface = -1;
			float	bestSurfaceDistance = 9999999.9;
			vec3_t	thisPosition;

			VectorSet(thisPosition, x, y, 0);

			// Find the height at this position...
			for (int s = 0; s < numMapDrawSurfs; s++)
			{
				/* get drawsurf */
				mapDrawSurface_t *ds = &mapDrawSurfs[s];
				shaderInfo_t *si = ds->shaderInfo;

				if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
				{
					//Sys_Printf("Position %.4f %.4f is nodraw!\n", x, y);
					continue;
				}

				if (!(si->compileFlags & C_SOLID))
				{
					//Sys_Printf("Position %.4f %.4f is not solid!\n", x, y);
					continue;
				}

				if (MAP_WATER_LEVEL > -999999.0 && ds->maxs[2] <= MAP_WATER_LEVEL + 256.0)
				{// Don't add skyscrapers under water...
					//Sys_Printf("Position %.4f %.4f %.4f is under water!\n", x, y, ds->maxs[2]);
					continue;
				}

				if (si->compileFlags & C_LIQUID)
				{
					continue;
				}

				//Sys_Printf("drawsurf %i. %i indexes. %i verts.\n", s, ds->numIndexes, ds->numVerts);

				if (ds->numIndexes == 0 && ds->numVerts == 3)
				{
					for (int j = 0; j < 3; j++)
					{
						/* get vertex */
						bspDrawVert_t		*dv = &ds->verts[j];

						if (MAP_WATER_LEVEL > -999999.0 && dv->xyz[2] <= MAP_WATER_LEVEL + 256.0)
						{// Don't add skyscrapers under water...
						 //Sys_Printf("Position %.4f %.4f %.4f is under water!\n", x, y, ds->maxs[2]);
							continue;
						}

						float dist = DistanceHorizontal(dv->xyz, thisPosition);

						if (dist < bestSurfaceDistance)
						{
							bestSurfaceDistance = dist;
							bestSurface = s;
							//Sys_Printf("Position %.4f %.4f best surface set to %i.\n", x, y, s);
						}
					}
				}
			}

			if (bestSurface == -1)
			{// No height found here...
				//Sys_Printf("Position %.4f %.4f did no find a best surface.\n", x, y);
				continue;
			}

			mapDrawSurface_t *ds = &mapDrawSurfs[bestSurface];
			shaderInfo_t *si = ds->shaderInfo;
			vec3_t angles[3];
			qboolean isFlat = qfalse;

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
				//	Sys_Printf("pitch %.4f.\n", pitch);

				//if ((pitch <= 8 && pitch >= -8) || (pitch <= 188.0 && pitch >= 172.0))
				if (pitch == 0 || pitch == 180)
				{
					thisPosition[2] = dv->xyz[2];
					isFlat = qtrue;
					break;
				}
				else
				{
					//Sys_Printf("Pitch is %.4f.\n", pitch);
				}
			}

			if (!isFlat)
			{// This position is not flat...
				//Sys_Printf("Position %.4f %.4f is not flat.\n", x, y);
				continue;
			}
			
			thisPosition[2] -= 256.0;

			if (StaticObjectNear(thisPosition))
			{
				continue;
			}

			float CBD_DISTANCE = max(DistanceHorizontal(SKYSCRAPERS_CENTER, thisPosition) - 6144.0, 0.0);
			float CENTER_DISTANCE = max(DistanceHorizontal(SKYSCRAPERS_CENTER, thisPosition) - 12288.0, 0.0);
			qboolean isCBD = (CBD_DISTANCE <= 0.0) ? qtrue : qfalse;
			qboolean isCentral = (!isCBD && CENTER_DISTANCE <= 0.0) ? qtrue : qfalse;
			
			// Ok, this position looks ok for a skyscraper...
			if (isMapEdge)
			{
				VectorCopy(thisPosition, skyscraperPositions[numSkyscrapers]);
				skyscraperScale[numSkyscrapers][0] = 16.0;
				skyscraperScale[numSkyscrapers][1] = 16.0;

				if (isCBD)
				{
					skyscraperScale[numSkyscrapers][2] = irand(16, 32);
					skyscraperLevels[numSkyscrapers] = irand(0, 2);
					skyscraperCBD[numSkyscrapers] = qtrue;
				}
				else if (isCentral)
				{
					skyscraperScale[numSkyscrapers][2] = irand(8, 16);
					skyscraperLevels[numSkyscrapers] = irand(0, 1);
					skyscraperCentral[numSkyscrapers] = qtrue;
				}
				else
				{
					skyscraperScale[numSkyscrapers][2] = irand(6, 8);
					skyscraperLevels[numSkyscrapers] = (irand(0, 5) == 0) ? irand(0, 1) : 0;
				}
			}
			else
			{
				qboolean bigX = qfalse;
				qboolean bigY = qfalse;
				qboolean mergeY = qfalse;

				if (irand(0, 6) == 6)
				{
					int choice = irand(0, 2);
					
					if (choice == 0)
						mergeY = qtrue;
					else if (choice == 1)
						bigX = qtrue;
					else if (choice == 2)
						bigY = qtrue;
				}

				if (mergeY)
				{
					y += 1024.0;
					VectorCopy(thisPosition, skyscraperPositions[numSkyscrapers]);

					skyscraperScale[numSkyscrapers][0] = irand(10.0, 11.0);
					skyscraperScale[numSkyscrapers][1] = irand(22.0, 32.0);

					if (isCBD)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 32);
						skyscraperLevels[numSkyscrapers] = irand(0, 2);
						skyscraperCBD[numSkyscrapers] = qtrue;
					}
					else if (isCentral)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 16);
						skyscraperLevels[numSkyscrapers] = irand(0, 1);
						skyscraperCentral[numSkyscrapers] = qtrue;
					}
					else
					{
						skyscraperScale[numSkyscrapers][2] = irand(6, 8);
						skyscraperLevels[numSkyscrapers] = (irand(0, 5) == 0) ? irand(0, 1) : 0;
					}

					// Skip one, since we made this one double width...
					y += 1024.0;
					thisCount++;
				}
				else if (bigX)
				{
					VectorCopy(thisPosition, skyscraperPositions[numSkyscrapers]);

					skyscraperScale[numSkyscrapers][0] = 16.0; // Just take up the whole block.
					skyscraperScale[numSkyscrapers][1] = irand(10.0, 11.0);

					if (isCBD)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 32);
						skyscraperLevels[numSkyscrapers] = irand(0, 2);
						skyscraperCBD[numSkyscrapers] = qtrue;
					}
					else if (isCentral)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 16);
						skyscraperLevels[numSkyscrapers] = irand(0, 1);
						skyscraperCentral[numSkyscrapers] = qtrue;
					}
					else
					{
						skyscraperScale[numSkyscrapers][0] = 10.0; // Just take up the whole block.
						skyscraperScale[numSkyscrapers][1] = irand(7.0, 9.0);
						skyscraperScale[numSkyscrapers][2] = irand(6, 8);
						skyscraperLevels[numSkyscrapers] = (irand(0, 5) == 0) ? irand(0, 1) : 0;
					}
				}
				else if (bigY)
				{
					VectorCopy(thisPosition, skyscraperPositions[numSkyscrapers]);

					skyscraperScale[numSkyscrapers][0] = irand(10.0, 11.0);
					skyscraperScale[numSkyscrapers][1] = 16.0; // Just take up the whole block.

					if (isCBD)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 32);
						skyscraperLevels[numSkyscrapers] = irand(0, 2);
						skyscraperCBD[numSkyscrapers] = qtrue;
					}
					else if (isCentral)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 16);
						skyscraperLevels[numSkyscrapers] = irand(0, 1);
						skyscraperCentral[numSkyscrapers] = qtrue;
					}
					else
					{
						skyscraperScale[numSkyscrapers][0] = irand(7.0, 9.0);
						skyscraperScale[numSkyscrapers][1] = 10.0; // Just take up the whole block.
						skyscraperScale[numSkyscrapers][2] = irand(6, 8);
						skyscraperLevels[numSkyscrapers] = (irand(0, 5) == 0) ? irand(0, 1) : 0;
					}
				}
				else
				{
					VectorCopy(thisPosition, skyscraperPositions[numSkyscrapers]);

					skyscraperScale[numSkyscrapers][0] = irand(10.0, 11.0);
					skyscraperScale[numSkyscrapers][1] = irand(10.0, 11.0);

					if (isCBD)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 32);
						skyscraperLevels[numSkyscrapers] = irand(0, 2);
						skyscraperCBD[numSkyscrapers] = qtrue;
					}
					else if (isCentral)
					{
						skyscraperScale[numSkyscrapers][2] = irand(8, 16);
						skyscraperLevels[numSkyscrapers] = irand(0, 1);
						skyscraperCentral[numSkyscrapers] = qtrue;
					}
					else
					{
						skyscraperScale[numSkyscrapers][0] = irand(7.0, 9.0);
						skyscraperScale[numSkyscrapers][1] = irand(7.0, 9.0);
						skyscraperScale[numSkyscrapers][2] = irand(6, 8);
						skyscraperLevels[numSkyscrapers] = (irand(0, 5) == 0) ? irand(0, 1) : 0;
					}
				}
			}

			numSkyscrapers++;
		}
	}

	Sys_Printf("Found %i skyscraper positions.\n", numSkyscrapers);

	int numAddedSections = 0;

	for (int i = 0; i < numSkyscrapers; i++)
	{
		printLabelledProgress("GenerateSkyscrapers", i, numSkyscrapers);

		float currentSkyscraperTop = 0;

		char selectedTexture[256] = { 0 };

		int textureChoice = irand(0, 19);

		if (textureChoice < 10)
			strcpy(selectedTexture, va("textures/city/building0%i", textureChoice));
		else
			strcpy(selectedTexture, va("textures/city/building%i", textureChoice));

		{// Lowest level...
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

			mapEnt->alreadyAdded = qfalse;
			mapEnt->mapEntityNum = 0;

			VectorCopy(skyscraperPositions[i], mapEnt->origin);

			{
				char str[32];
				sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] + 4.0);
				SetKeyValue(mapEnt, "origin", str);
			}

			{
				char str[32];
				sprintf(str, "%.4f %.4f %.4f", skyscraperScale[i][0], skyscraperScale[i][1], skyscraperScale[i][2]);
				SetKeyValue(mapEnt, "modelscale_vec", str);
			}

			{
				char str[32];
				sprintf(str, "%.4f", 0.0 - 180.0);
				SetKeyValue(mapEnt, "angle", str);
			}

			SetKeyValue(mapEnt, "_forcedSolid", "1");

			if (SKYSCRAPERS_PLANE_SNAP > 0)
			{
				SetKeyValue(mapEnt, "snap", va("%i", SKYSCRAPERS_PLANE_SNAP));
			}

			currentSkyscraperTop = (mapEnt->origin[2] + 4.0) + (128.0 * skyscraperScale[i][2]);

			/* ydnar: get classname */
			if (USE_LODMODEL)
				SetKeyValue(mapEnt, "classname", "misc_lodmodel");
			else
				SetKeyValue(mapEnt, "classname", "misc_model");

			classname = ValueForKey(mapEnt, "classname");

			SetKeyValue(mapEnt, "model", "models/warzone/skyscraper/box.md3");

			//Sys_Printf( "Generated skyscraper at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );

			SetKeyValue(mapEnt, "_overrideShader", selectedTexture);

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


#if defined(__ADD_PROCEDURALS_EARLY__)
			AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
			EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
			//MoveBrushesToWorld( mapEnt );
			numEntities--;
#endif

			numAddedSections++;
		}

#if 1
		vec3_t currentSkyscraperScale;
		VectorSet(currentSkyscraperScale, skyscraperScale[i][0], skyscraperScale[i][1], skyscraperScale[i][2]);

		// Add the levels on top...
		for (int level = 0; level < skyscraperLevels[i]; level++)
		{
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
			vec3_t			thisLevelScale;
			float			scaleXY = (float)irand(60, 90) / 100.0;
			float			scaleZ = (skyscraperCBD[i] || skyscraperCentral[i]) ? (float)irand(40, 70) / 100.0 : (float)irand(10, 50) / 100.0;
			VectorSet(thisLevelScale, scaleXY, scaleXY, scaleZ);
			thisLevelScale[0] *= currentSkyscraperScale[0];
			thisLevelScale[1] *= currentSkyscraperScale[1];
			thisLevelScale[2] *= currentSkyscraperScale[2];

			/* setup */
			entitySourceBrushes = 0;
			mapEnt = &entities[numEntities];
			numEntities++;
			memset(mapEnt, 0, sizeof(*mapEnt));

			mapEnt->alreadyAdded = qfalse;
			mapEnt->mapEntityNum = 0;

			VectorCopy(skyscraperPositions[i], mapEnt->origin);
			mapEnt->origin[2] = currentSkyscraperTop;

			{
				char str[32];
				sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
				SetKeyValue(mapEnt, "origin", str);
			}

			{
				char str[32];
				sprintf(str, "%.4f %.4f %.4f", thisLevelScale[0], thisLevelScale[1], thisLevelScale[2]);
				SetKeyValue(mapEnt, "modelscale_vec", str);
			}

			{
				char str[32];
				sprintf(str, "%.4f", 0.0 - 180.0);
				SetKeyValue(mapEnt, "angle", str);
			}

			SetKeyValue(mapEnt, "_forcedSolid", "1");

			currentSkyscraperTop = currentSkyscraperTop + (128.0 * thisLevelScale[2]);
			VectorCopy(thisLevelScale, currentSkyscraperScale);

			/* ydnar: get classname */
			if (USE_LODMODEL)
				SetKeyValue(mapEnt, "classname", "misc_lodmodel");
			else
				SetKeyValue(mapEnt, "classname", "misc_model");

			classname = ValueForKey(mapEnt, "classname");

			SetKeyValue(mapEnt, "model", "models/warzone/skyscraper/box.md3");

			//Sys_Printf( "Generated skyscraper at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );

			SetKeyValue(mapEnt, "_overrideShader", selectedTexture);

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


#if defined(__ADD_PROCEDURALS_EARLY__)
			AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
			EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
			//MoveBrushesToWorld( mapEnt );
			numEntities--;
#endif
		}

		numAddedSections++;
#endif
	}

	Sys_Printf("Created %i total skyscraper sections.\n", numAddedSections);
}


int			numLedges = 0;
int			numLedgesRoadCulled = 0;
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
	numLedgesRoadCulled = 0;
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

			if (RoadExistsAtPoint(ds->verts[0].xyz, 4) || RoadExistsAtPoint(ds->verts[1].xyz, 4) || RoadExistsAtPoint(ds->verts[2].xyz, 4))
			{// There's a road here...
				numLedgesRoadCulled++;
				continue;
			}

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
				//	Sys_Printf("pitch %.4f.\n", pitch);

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

			if (maxs[2] < MAP_WATER_LEVEL)
			{
				continue;
			}

			if (StaticObjectNear(center))
			{
				continue;
			}

			/*if (center[2] + 32.0 <= MAP_WATER_LEVEL)
			{
				continue;
			}*/

			if (RoadExistsAtPoint(center, 4))
			{// There's a road here...
				numLedgesRoadCulled++;
				continue;
			}

			if (!(Distance(center, CITY_LOCATION) > CITY_RADIUS + CITY_BUFFER
				&& Distance(center, CITY2_LOCATION) > CITY2_RADIUS + CITY_BUFFER
				&& Distance(center, CITY3_LOCATION) > CITY3_RADIUS + CITY_BUFFER
				&& Distance(center, CITY4_LOCATION) > CITY4_RADIUS + CITY_BUFFER
				&& Distance(center, CITY5_LOCATION) > CITY5_RADIUS + CITY_BUFFER))
			{// Don't add ledges in towns...
				continue;
			}

			qboolean bad = qfalse;

			for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
			{// Check if this position is too close to a static model location...
				if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
				{// Was not assigned a model... Must be too close to something else...
					continue;
				}

				float dist = Distance(STATIC_ORIGIN[k], center);

				if (dist <= STATIC_BUFFER_RANGE * STATIC_SCALE[k])
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

			//Sys_Printf("cliff found at %.4f %.4f %.4f. angles0 %.4f %.4f %.4f. angles1 %.4f %.4f %.4f. angles2 %.4f %.4f %.4f.\n", center[0], center[1], center[2], angles[0][0], angles[0][1], angles[0][2], angles[1][0], angles[1][1], angles[1][2], angles[2][0], angles[2][1], angles[2][2]);

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
	Sys_Printf("%i ledge objects were culled by closeness to roads.\n", numLedgesRoadCulled);
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

		mapEnt->alreadyAdded = qfalse;
		mapEnt->mapEntityNum = 0;

		VectorCopy(ledgePositions[i], mapEnt->origin);

		{
			char str[32];

			if (ledgeIsLowAngle[i] == 1)
			{// Special case for low angle slopes... Dig it further into the ground more...
				sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]-32.0);
			}
			else if (ledgeIsLowAngle[i] == 2)
			{// Special case for *very* low angle slopes... Dig it further into the ground more...
				sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] - 48.0);
			}
			else
			{
				sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			}
			SetKeyValue(mapEnt, "origin", str);
		}

		if (!LEDGE_FACES_SCALE_XY)
		{// Scale X, Y & Z axis...
			char str[32];
			sprintf(str, "%.4f", ledgeScale[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}
		else
		{// Scale X & Y only...
			char str[128];
			sprintf(str, "%.4f %.4f %.4f", ledgeScale[i], ledgeScale[i], ledgeHeight[i]);
			//Sys_Printf("%s\n", str);
			SetKeyValue(mapEnt, "modelscale_vec", str);
		}

		{
			char str[32];
			sprintf(str, "%.4f", ledgeAngles[i][1] - 180.0);
			SetKeyValue(mapEnt, "angle", str);
		}

		if (LEDGE_PLANE_SNAP > 0)
		{
			SetKeyValue(mapEnt, "snap", va("%i", LEDGE_PLANE_SNAP));
		}

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
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

		SetKeyValue(mapEnt, "_allowCollisionModelTypes", "3"); // allow convex or simplified collision models only...

		// Always use origin as the near low point on ledges... They are at low angles...
		//SetKeyValue(mapEnt, "_originAsLowPoint", "1");

		//Sys_Printf( "Generated cliff face at %.4f %.4f %.4f. Angle %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], ledgeAngles[i][1] );

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


#if defined(__ADD_PROCEDURALS_EARLY__)
		AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
		EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
		//MoveBrushesToWorld( mapEnt );
		numEntities--;
#endif
	}
}

#define __EARLY_PERCENTAGE_CHECK__

typedef vec_t vec2_t[3];

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

float noise(vec2_t st) {
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

bool CheckTreeClusters(vec3_t origin)
{
	if (!TREE_CLUSTER)
		return true;

	vec2_t pos;
	pos[0] = origin[0] * TREE_CLUSTER_SEED;
	pos[1] = origin[1] * TREE_CLUSTER_SEED;

	float seed = noise(pos);

	if (seed < TREE_CLUSTER_MINIMUM)
		return false;

	return true;
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
	int				NUM_CLOSE_ROADS = 0;

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

			if (FOLIAGE_POSITIONS[i][2] - 8.0 <= MAP_WATER_LEVEL)
			{
				continue;
			}

			if (!CheckTreeClusters(FOLIAGE_POSITIONS[i]))
			{
				continue;
			}

#ifdef __EARLY_PERCENTAGE_CHECK__
			if (FOLIAGE_NOT_GROUND[i])
			{
				if (irand(0, 100) > TREE_PERCENTAGE_SPECIAL)
				{
					continue;
				}
			}
			else if (irand(0, 100) > TREE_PERCENTAGE)
			{
				continue;
			}
#endif //__EARLY_PERCENTAGE_CHECK__

			if (!FOLIAGE_NOT_GROUND[i] && StaticObjectNear(FOLIAGE_POSITIONS[i]))
			{
				continue;
			}

			if (!FOLIAGE_NOT_GROUND[i] && MapEntityNear(FOLIAGE_POSITIONS[i]))
			{// Don't spawn stuff near map entities...
				continue;
			}

			if (RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4))
			{// There's a road here...
				NUM_CLOSE_ROADS++;
				continue;
			}

			if (!FOLIAGE_NOT_GROUND[i])
			{
				for (int k = 0; k < MAX_STATIC_ENTITY_MODELS; k++)
				{// Check if this position is too close to a static model location...
					if (STATIC_MODEL[k][0] == 0 || strlen(STATIC_MODEL[k]) <= 0)
					{// Was not assigned a model... Must be too close to something else...
						continue;
					}

					float dist = Distance(STATIC_ORIGIN[k], FOLIAGE_POSITIONS[i]);

					if (dist <= STATIC_BUFFER_RANGE * STATIC_SCALE[k])
					{// Not within this static object's buffer range... OK!
						bad = qtrue;
						break;
					}
				}
			}

			if (bad)
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			if (TREE_ROADSCAN_MULTIPLIER[selected] > 1.0 && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4 * TREE_ROADSCAN_MULTIPLIER[selected]))
			{// There's a road here...
				selected = irand(0, NUM_POSSIBLES - 1);

				if (TREE_ROADSCAN_MULTIPLIER[selected] > 1.0 && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4 * TREE_ROADSCAN_MULTIPLIER[selected]))
				{// There's a road here...
					NUM_CLOSE_ROADS++;
					continue;
				}
			}

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

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= TREE_FORCED_DISTANCE_FROM_SAME[POSSIBLES[selected]])
				{// Not within this object's same type range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY_LOCATION, FOLIAGE_POSITIONS[i]);
				
				if (dist <= CITY_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY2_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY2_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY3_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY3_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY4_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY4_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY5_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY5_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = DistanceHorizontal(SKYSCRAPERS_CENTER, FOLIAGE_POSITIONS[i]);
				
				if (SKYSCRAPERS_RADIUS && dist <= SKYSCRAPERS_RADIUS)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}
			}

			if (bad)
			{
				continue;
			}

			/*vec3_t angles;
			vectoangles( FOLIAGE_NORMALS[i], angles );
			pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;*/
			pitch = FOLIAGE_TREE_PITCH[i];

			if (pitch > POSSIBLES_MAX_ANGLE[selected] || pitch < -POSSIBLES_MAX_ANGLE[selected])
			{// Bad slope...
				//Sys_Printf("Position %i angles too great (%.4f).\n", i, pitch);
				continue;
			}

			if (FOLIAGE_NOT_GROUND[i] && pitch > NOT_GROUND_MAXSLOPE || pitch < -NOT_GROUND_MAXSLOPE)
			{
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

			//Sys_Printf("Position %i angles OK! (%.4f).\n", i, pitch);

#ifndef __EARLY_PERCENTAGE_CHECK__
			if (irand(0, 100) <= TREE_PERCENTAGE)
#endif //__EARLY_PERCENTAGE_CHECK__
			{
				FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
				FOLIAGE_TREE_BUFFER[i] = BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
				SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
				FOLIAGE_ASSIGNED[i] = qtrue;
				NUM_PLACED[POSSIBLES[selected]]++;
			}
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
		int numDone = 0;

#pragma omp parallel for num_threads(numthreads)
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			qboolean bad = qfalse;
			int tries = 0;
			int j;

			numDone++;
#pragma omp critical
			{
				printLabelledProgress("RandomizeModels", numDone, FOLIAGE_NUM_POSITIONS);
			}

			if (FOLIAGE_ASSIGNED[i])
			{
				continue;
			}

			if (FOLIAGE_POSITIONS[i][2] - 8.0 <= MAP_WATER_LEVEL)
			{
				continue;
			}

			if (!CheckTreeClusters(FOLIAGE_POSITIONS[i]))
			{
				continue;
			}

#ifdef __EARLY_PERCENTAGE_CHECK__
			if (FOLIAGE_NOT_GROUND[i])
			{
				if (irand(0, 100) > TREE_PERCENTAGE_SPECIAL)
				{
					continue;
				}
			}
			else if (irand(0, 100) > TREE_PERCENTAGE)
			{
				continue;
			}
#endif //__EARLY_PERCENTAGE_CHECK__

			if (!FOLIAGE_NOT_GROUND[i] && StaticObjectNear(FOLIAGE_POSITIONS[i]))
			{
				continue;
			}

			if (!FOLIAGE_NOT_GROUND[i] && MapEntityNear(FOLIAGE_POSITIONS[i]))
			{// Don't spawn stuff near map entities...
				continue;
			}

			if (RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4))
			{// There's a road here...
				NUM_CLOSE_ROADS++;
				continue;
			}

			if (!FOLIAGE_NOT_GROUND[i])
			{
				float pitch = FOLIAGE_TREE_PITCH[i];

				if (FOLIAGE_NOT_GROUND[i] && pitch > NOT_GROUND_MAXSLOPE || pitch < -NOT_GROUND_MAXSLOPE)
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

					if (dist <= STATIC_BUFFER_RANGE * STATIC_SCALE[k])
					{// Not within this static object's buffer range... OK!
						bad = qtrue;
						break;
					}
				}
			}

			if (bad)
			{
				continue;
			}

			int selected = irand(0,NUM_POSSIBLES-1);

			if (TREE_ROADSCAN_MULTIPLIER[selected] > 1.0 && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4 * TREE_ROADSCAN_MULTIPLIER[selected]))
			{// There's a road here...
				selected = irand(0, NUM_POSSIBLES - 1);

				if (TREE_ROADSCAN_MULTIPLIER[selected] > 1.0 && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4 * TREE_ROADSCAN_MULTIPLIER[selected]))
				{// There's a road here...
					NUM_CLOSE_ROADS++;
					continue;
				}
			}

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

				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= TREE_FORCED_DISTANCE_FROM_SAME[POSSIBLES[selected]])
				{// Not within this object's same type range... OK!
					//selected = irand(0, NUM_POSSIBLES - 1);
					//tries++;
					bad = qtrue;
					break;
				}

				dist = Distance(CITY_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY2_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY2_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY3_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY3_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY4_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY4_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = Distance(CITY5_LOCATION, FOLIAGE_POSITIONS[i]);

				if (dist <= CITY5_RADIUS + CITY_BUFFER)
				{// Not within this city's buffer range... OK!
					bad = qtrue;
					break;
				}

				dist = DistanceHorizontal(SKYSCRAPERS_CENTER, FOLIAGE_POSITIONS[i]);

				if (SKYSCRAPERS_RADIUS && dist <= SKYSCRAPERS_RADIUS)
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
				/*Sys_Printf("cliff %.4f %.4f %.4f. possible %.4f %.4f %.4f. Distance %.4f.\n", cliffPositions[z][0], cliffPositions[z][1], cliffPositions[z][2]
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

#ifndef __EARLY_PERCENTAGE_CHECK__
			if (FOLIAGE_NOT_GROUND[i])
			{
				if (irand(0, 100) <= TREE_PERCENTAGE_SPECIAL)
				{
					FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
					FOLIAGE_TREE_BUFFER[i] = BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
					SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
					FOLIAGE_ASSIGNED[i] = qtrue;
					NUM_PLACED[POSSIBLES[selected]]++;
				}
			}
			else if (irand(0, 100) <= TREE_PERCENTAGE)
#endif //__EARLY_PERCENTAGE_CHECK__
			{
				FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
				FOLIAGE_TREE_BUFFER[i] = BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
				SAME_RANGES[i] = POSSIBLES_SAME_RANGES[selected];
				FOLIAGE_ASSIGNED[i] = qtrue;
				NUM_PLACED[POSSIBLES[selected]]++;
			}
		}

		printLabelledProgress("RandomizeModels", FOLIAGE_NUM_POSITIONS, FOLIAGE_NUM_POSITIONS);
	}

	free(BUFFER_RANGES);
	free(SAME_RANGES);

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

	Sys_Printf("%9d positions ignored due to closeness to a road.\n", NUM_CLOSE_ROADS);
	Sys_Printf("%9d positions ignored due to closeness to a cliff.\n", NUM_CLOSE_CLIFFS);
}

//#define __ADD_PROCEDURALS_EARLY__ // Add trees to map's brush list instanty...

extern void MoveBrushesToWorld( entity_t *ent );
extern qboolean StringContainsWord(const char *haystack, const char *needle);

extern qboolean GENERATING_SECONDARY_BSP;

void GenerateMapForest ( void )
{
	if (generateforest)
	{
		if (FOLIAGE_NUM_POSITIONS > 0 && TREE_PERCENTAGE > 0)
		{// First re-assign model types based on buffer ranges and instance type ranges...
			int i;

			Sys_PrintHeading("--- GenerateMapForest ---\n");

#if defined(__ADD_PROCEDURALS_EARLY__)
			Sys_Printf("Adding %i trees to bsp.\n", FOLIAGE_NUM_POSITIONS);
#endif

			if (!USE_SECONDARY_BSP || !GENERATING_SECONDARY_BSP)
			{// When doing the secondary bsp, we don't want to regen the list, but instead reuse the current one...
				ReassignTreeModels();
			}
			else if (USE_SECONDARY_BSP && GENERATING_SECONDARY_BSP)
			{
				int count = 0;

				for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
				{
					if (FOLIAGE_ASSIGNED[i])
					{
						count++;
					}
				}

				Sys_Printf("%9d of %i positions previously assigned to tree models.\n", count, FOLIAGE_NUM_POSITIONS - 1);
			}

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

				mapEnt->alreadyAdded = qfalse;
				mapEnt->mapEntityNum = 0;

				//mapEnt->mapEntityNum = numMapEntities;
				//numMapEntities++;

				//mapEnt->mapEntityNum = 0 - numMapEntities;

				//mapEnt->forceSubmodel = qtrue;

				VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);

				float offset = TREE_OFFSETS[FOLIAGE_TREE_SELECTION[i]];
				float scale = 2.0*TREE_SCALE_MULTIPLIER*TREE_SCALES[FOLIAGE_TREE_SELECTION[i]];

				if (FOLIAGE_NOT_GROUND[i])
				{// Dig these into the ground a little more. Probably a more varied slope...
					//offset *= 1.5;
					scale *= TREE_SCALE_MULTIPLIER_SPECIAL;
					offset *= TREE_SCALE_MULTIPLIER_SPECIAL;
				}
				
				mapEnt->origin[2] += offset;


				/*{// Offset the model so the lowest bounds are at 0 0 0.
					picoModel_t *picoModel = FindModel(TREE_MODELS[FOLIAGE_TREE_SELECTION[i]], 0);

					if (picoModel)
					{
						float modelOffset = 0.0 - picoModel->mins[2];
						if (modelOffset > 0)
							mapEnt->origin[2] += modelOffset;
					}
				}*/

				{
					char str[32];
					sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
					SetKeyValue(mapEnt, "origin", str);
				}

				{
					char str[32];
					sprintf(str, "%.4f", scale);
					SetKeyValue(mapEnt, "modelscale", str);
				}


				SetKeyValue(mapEnt, "_allowCollisionModelTypes", va("%i", TREE_ALLOW_SIMPLIFY[FOLIAGE_TREE_SELECTION[i]]));


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

				if (TREE_USE_ORIGIN_AS_LOWPOINT[FOLIAGE_TREE_SELECTION[i]])
				{
					SetKeyValue(mapEnt, "_originAsLowPoint", "1");

					char str[32];
					sprintf(str, "%.4f", offset);
					SetKeyValue(mapEnt, "_originOffset", str);
				}
				else
				{
					SetKeyValue(mapEnt, "_originAsLowPoint", "0");
				}

				if (TREE_PLANE_SNAP[FOLIAGE_TREE_SELECTION[i]] > 0)
				{
					SetKeyValue(mapEnt, "snap", va("%i", TREE_PLANE_SNAP[FOLIAGE_TREE_SELECTION[i]]));
				}

				{
					if (!USE_SECONDARY_BSP || !GENERATING_SECONDARY_BSP)
					{
						FOLIAGE_TREE_ANGLES[i] = irand(0, 360) - 180.0;
					}

					char str[32];
					sprintf(str, "%.4f", FOLIAGE_TREE_ANGLES[i]);
					SetKeyValue(mapEnt, "angle", str);
				}


				//Sys_Printf( "Generated tree at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


				/* ydnar: get classname */
				if (USE_LODMODEL)
					SetKeyValue(mapEnt, "classname", "misc_lodmodel");
				else
					SetKeyValue(mapEnt, "classname", "misc_model");

				classname = ValueForKey(mapEnt, "classname");

				SetKeyValue(mapEnt, "model", TREE_MODELS[FOLIAGE_TREE_SELECTION[i]]); // test tree

				//Sys_Printf( "Generated tree at %.4f %.4f %.4f. Model %s.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], TROPICAL_TREES[FOLIAGE_TREE_SELECTION[i]] );

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


#if defined(__ADD_PROCEDURALS_EARLY__)
				AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
				EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
				//MoveBrushesToWorld( mapEnt );
				numEntities--;
#endif
			}

			if (!USE_SECONDARY_BSP)
			{
				free(FOLIAGE_ASSIGNED);
			}
			else if (USE_SECONDARY_BSP)
			{
				if (GENERATING_SECONDARY_BSP)
				{
					free(FOLIAGE_ASSIGNED);
				}
			}

#if defined(__ADD_PROCEDURALS_EARLY__)
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

		mapEnt->alreadyAdded = qfalse;
		mapEnt->mapEntityNum = 0;

		VectorCopy(STATIC_ORIGIN[i], mapEnt->origin);

		{// Offset the model so the lowest bounds are at 0 0 0.
			picoModel_t *picoModel = FindModel(STATIC_MODEL[i], 0);

			if (picoModel)
			{
				float modelOffset = 0.0 - picoModel->mins[2];
				if (modelOffset > 0)
					mapEnt->origin[2] += modelOffset;
			}
		}

		{
			char str[32];
			sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
			SetKeyValue(mapEnt, "origin", str);
		}

		{
			char str[32];
			sprintf(str, "%.4f", STATIC_SCALE[i]);
			SetKeyValue(mapEnt, "modelscale", str);
		}

		{
			char str[32];
			sprintf(str, "%.4f", STATIC_ANGLE[i]);
			SetKeyValue(mapEnt, "angle", str);
		}

		/* ydnar: get classname */
		if (USE_LODMODEL)
			SetKeyValue(mapEnt, "classname", "misc_lodmodel");
		else
			SetKeyValue(mapEnt, "classname", "misc_model");

		classname = ValueForKey(mapEnt, "classname");


		//Sys_Printf( "Generated mapent at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


		SetKeyValue(mapEnt, "model", STATIC_MODEL[i]);

		if (STATIC_PLANE_SNAP[i] > 0)
		{
			SetKeyValue(mapEnt, "snap", va("%i", STATIC_PLANE_SNAP[i]));
		}

		SetKeyValue(mapEnt, "_allowCollisionModelTypes", va("%i", STATIC_ALLOW_SIMPLIFY[i]));

		if (STATIC_FORCED_FULLSOLID[i] >= 2)
		{
			SetKeyValue(mapEnt, "_forcedSolid", "2");
		}
		else if (STATIC_FORCED_FULLSOLID[i])
		{
			SetKeyValue(mapEnt, "_forcedSolid", "1");
		}
		else
		{
			SetKeyValue(mapEnt, "_forcedSolid", "0");
		}

		if (STATIC_FORCED_OVERRIDE_SHADER[i] != '\0')
		{
			SetKeyValue(mapEnt, "_overrideShader", STATIC_FORCED_OVERRIDE_SHADER[i]);
		}


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
float			BUILDING_ANGLES[FOLIAGE_MAX_FOLIAGES] = { 0 };

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
						/*vec3_t angles;
						vectoangles(FOLIAGE_NORMALS[j], angles);
						float pitch = angles[0];
						if (pitch > 180)
							pitch -= 360;

						if (pitch < -180)
							pitch += 360;

						pitch += 90.0f;*/
						float pitch = FOLIAGE_TREE_PITCH[i];

						if (pitch > CITY_FORCED_MAX_ANGLE[i] || pitch < -CITY_FORCED_MAX_ANGLE[i])
						{// Bad slope...
						 //Sys_Printf("Position %i angles too great (%.4f).\n", i, pitch);
							continue;
						}

						if (FOLIAGE_NOT_GROUND[i] && pitch > NOT_GROUND_MAXSLOPE || pitch < -NOT_GROUND_MAXSLOPE)
						{
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

					if (StaticObjectNear(FOLIAGE_POSITIONS[j]))
					{
						continue;
					}

					if (!CITY_ALLOW_ROAD[i] && RoadExistsAtPoint(FOLIAGE_POSITIONS[j], 4))
					{// There's a road here...
						numLedgesRoadCulled++;
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

	//Sys_Printf("Assign angles models from %i possibles.\n", NUM_POSSIBLES);

	// First add any non-steep options...
	if (NUM_POSSIBLES > 0)
	{
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			float pitch;
			qboolean bad = qfalse;
			int j;

			printLabelledProgress("RestrictedAngleModels", i, FOLIAGE_NUM_POSITIONS);

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

			if (StaticObjectNear(FOLIAGE_POSITIONS[i]))
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

				if (dist <= BUILDING_BUFFER_RANGES[j])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					//Sys_Printf("DEBUG: Position %i buffer-range. Dist: %.4f. Buffer: %.4f.\n", i, dist, BUILDING_BUFFER_RANGES[j]);
					break;
				}

				if (dist <= BUILDING_BUFFER_RANGES[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					//Sys_Printf("DEBUG: Position %i buffer-range. Dist: %.4f. Buffer: %.4f.\n", i, dist, BUILDING_BUFFER_RANGES[i]);
					break;
				}

				//if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_BUILDING_SAME_RANGES[selected])
				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= CITY_FORCED_DISTANCE_FROM_SAME[POSSIBLES[selected]])
				{// Not within this object's same type range... OK!
					bad = qtrue;
					//Sys_Printf("DEBUG: Position %i same-range. Dist: %.4f. Range: %.4f.\n", i, dist, POSSIBLES_BUILDING_SAME_RANGES[selected]);
					break;
				}
			}

			if (bad)
			{
				//Sys_Printf("DEBUG: Position %i range.\n", i);
				continue;
			}

			/*vec3_t angles;
			vectoangles(FOLIAGE_NORMALS[i], angles);
			pitch = angles[0];
			if (pitch > 180)
				pitch -= 360;

			if (pitch < -180)
				pitch += 360;

			pitch += 90.0f;*/
			pitch = FOLIAGE_TREE_PITCH[i];

			if (pitch > POSSIBLES_MAX_ANGLE[selected] || pitch < -POSSIBLES_MAX_ANGLE[selected])
			{// Bad slope...
				//printf("DEBUG: Position %i angles too great (%.4f).\n", i, pitch);
				continue;
			}

			if (FOLIAGE_NOT_GROUND[i] && pitch > NOT_GROUND_MAXSLOPE || pitch < -NOT_GROUND_MAXSLOPE)
			{
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				//Sys_Printf("DEBUG: Position %i cliff.\n", i);
				continue;
			}

#if 0
			for (int z = 0; z < numLedges; z++)
			{// Also keep them away from ledge objects...
				if (DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 64.0 /** CITY_CLIFF_CULL_RADIUS*/)
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
#endif

			if (!CITY_ALLOW_ROAD[POSSIBLES[selected]] && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4))
			{// There's a road here...
				//printf("DEBUG: Position %i road.\n", i);
				numLedgesRoadCulled++;
				continue;
			}

			//Sys_Printf("Position %i angles OK! (%.4f).\n", i, pitch);

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
		if (CITY_FORCED_MAX_ANGLE[i] > 0.0)
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
		NUM_POSSIBLES++;
	}

	//Sys_Printf("Assign other models from %i possibles.\n", NUM_POSSIBLES);

	Sys_Printf("Unrestricted angle possibles:\n");
	for (i = 0; i < NUM_POSSIBLES; i++)
	{
		Sys_Printf("%d - %s.\n", i, CITY_MODELS[POSSIBLES[i]]);
	}

	// Now add other options...
	if (NUM_POSSIBLES > 0)
	{
		int numDone = 0;

#pragma omp parallel for num_threads(numthreads)
		for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
		{
			qboolean bad = qfalse;
			int tries = 0;
			int j;

			numDone++;
#pragma omp critical
			{
				printLabelledProgress("RandomizeModels", numDone, FOLIAGE_NUM_POSITIONS);
			}

			if (BUILDING_ASSIGNED[i])
			{
				//printf("blocked by assigned.\n");
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

			if (StaticObjectNear(FOLIAGE_POSITIONS[i]))
			{
				continue;
			}

			int selected = irand(0, NUM_POSSIBLES - 1);

			for (j = 0; j < FOLIAGE_NUM_POSITIONS; j++)
			{
				if (tries > 32)
				{
					//printf("blocked by tries.\n");
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

				if (dist <= BUILDING_BUFFER_RANGES[j])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					//Sys_Printf("DEBUG: Position %i buffer-range. Dist: %.4f. Buffer: %.4f.\n", i, dist, BUILDING_BUFFER_RANGES[j]);
					break;
				}

				if (dist <= BUILDING_BUFFER_RANGES[i])
				{// Not within this object's buffer range... OK!
					bad = qtrue;
					//Sys_Printf("DEBUG: Position %i buffer-range. Dist: %.4f. Buffer: %.4f.\n", i, dist, BUILDING_BUFFER_RANGES[i]);
					break;
				}

				//if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= POSSIBLES_BUILDING_SAME_RANGES[selected])
				if (FOLIAGE_TREE_SELECTION[j] == POSSIBLES[selected] && dist <= CITY_FORCED_DISTANCE_FROM_SAME[POSSIBLES[selected]])
				{// Not within this object's same type range... OK!
					selected = irand(0, NUM_POSSIBLES - 1);
					tries++;
				}
			}

			if (bad)
			{
				//printf("blocked by buffer.\n");
				continue;
			}

			if (!CITY_ALLOW_ROAD[POSSIBLES[selected]] && RoadExistsAtPoint(FOLIAGE_POSITIONS[i], 4))
			{// There's a road here...
				//printf("blocked by road.\n");
				numLedgesRoadCulled++;
				continue;
			}

			for (int z = 0; z < numCliffs; z++)
			{// Also keep them away from cliff objects...
				if (DistanceHorizontal(cliffPositions[z], FOLIAGE_POSITIONS[i]) < cliffScale[z] * 256.0 * CITY_CLIFF_CULL_RADIUS)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				//printf("blocked by cliff.\n");
				continue;
			}

#if 0
			for (int z = 0; z < numLedges; z++)
			{// Also keep them away from ledge objects...
				if (DistanceHorizontal(ledgePositions[z], FOLIAGE_POSITIONS[i]) < ledgeScale[z] * 64.0 /** CITY_CLIFF_CULL_RADIUS*/)
				{
					bad = qtrue;
					NUM_CLOSE_CLIFFS++;
					break;
				}
			}

			if (bad)
			{
				printf("blocked by ledge.\n");
				continue;
			}
#endif

			FOLIAGE_TREE_SELECTION[i] = POSSIBLES[selected];
			FOLIAGE_TREE_BUFFER[i] = BUILDING_BUFFER_RANGES[i] = POSSIBLES_BUFFERS[selected];
			BUILDING_SAME_RANGES[i] = POSSIBLES_BUILDING_SAME_RANGES[selected];
			BUILDING_ASSIGNED[i] = qtrue;
			FOLIAGE_TREE_SCALE[i] = 1.0; // City models always use exact size stated.
			NUM_PLACED[POSSIBLES[selected]]++;
		}

		printLabelledProgress("RandomizeModels", FOLIAGE_NUM_POSITIONS, FOLIAGE_NUM_POSITIONS);
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

#if defined(__ADD_PROCEDURALS_EARLY__)
			Sys_Printf("Adding %i buildings to bsp.\n", FOLIAGE_NUM_POSITIONS);
#endif

			if (!USE_SECONDARY_BSP || !GENERATING_SECONDARY_BSP)
			{// When doing the secondary bsp, we don't want to regen the list, but instead reuse the current one...
				ReassignCityModels();
			}
			else if (USE_SECONDARY_BSP && GENERATING_SECONDARY_BSP)
			{
				int count = 0;

				for (i = 0; i < FOLIAGE_NUM_POSITIONS; i++)
				{
					if (BUILDING_ASSIGNED[i])
					{
						count++;
					}
				}

				Sys_Printf("%9d of %i positions previously assigned to city models.\n", count, FOLIAGE_NUM_POSITIONS - 1);
			}

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

				mapEnt->alreadyAdded = qfalse;
				mapEnt->mapEntityNum = 0;

				//mapEnt->mapEntityNum = numMapEntities;
				//numMapEntities++;

				//mapEnt->mapEntityNum = 0 - numMapEntities;

				//mapEnt->forceSubmodel = qtrue;

				VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
				mapEnt->origin[2] += CITY_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

				/*{// Offset the model so the lowest bounds are at 0 0 0.
					picoModel_t *picoModel = FindModel(CITY_MODELS[FOLIAGE_TREE_SELECTION[i]], 0);

					if (picoModel)
					{
						float modelOffset = 0.0 - picoModel->mins[2];
						if (modelOffset > 0)
							mapEnt->origin[2] += modelOffset;
					}
				}*/

				{
					char str[32];
					sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2]);
					SetKeyValue(mapEnt, "origin", str);
				}

				{
					if (CITY_SCALES_XY[FOLIAGE_TREE_SELECTION[i]] == 1.0 && CITY_SCALES_Z[FOLIAGE_TREE_SELECTION[i]] == 1.0)
					{
						char str[32];
						sprintf(str, "%.4f", FOLIAGE_TREE_SCALE[i] * 2.0*CITY_SCALE_MULTIPLIER*CITY_SCALES[FOLIAGE_TREE_SELECTION[i]]);
						SetKeyValue(mapEnt, "modelscale", str);
					}
					else
					{// Scale X & Y only...
						char str[128];
						float XYscale = FOLIAGE_TREE_SCALE[i] * 2.0*CITY_SCALE_MULTIPLIER*CITY_SCALES[FOLIAGE_TREE_SELECTION[i]] * CITY_SCALES_XY[FOLIAGE_TREE_SELECTION[i]];
						float Zscale = FOLIAGE_TREE_SCALE[i] * 2.0*CITY_SCALE_MULTIPLIER*CITY_SCALES[FOLIAGE_TREE_SELECTION[i]] * CITY_SCALES_Z[FOLIAGE_TREE_SELECTION[i]];
						sprintf(str, "%.4f %.4f %.4f", XYscale, XYscale, Zscale);
						//Sys_Printf("%s\n", str);
						SetKeyValue(mapEnt, "modelscale_vec", str);
					}
				}

				/*
				{
					char str[32];
					sprintf(str, "%.4f", FOLIAGE_TREE_ANGLES[i]);
					SetKeyValue(mapEnt, "angle", str);
				}
				*/
				
				if (USE_SECONDARY_BSP && GENERATING_SECONDARY_BSP)
				{// Reuse original angles...
					SetKeyValue(mapEnt, "angle", va("%i", BUILDING_ANGLES[i]));
				}
				else
				{// Record for any secondary pass...
					if (CITY_ANGLES_RANDOMIZE[FOLIAGE_TREE_SELECTION[i]] == 1)
					{// Full 360 deg randomization...
						BUILDING_ANGLES[i] = float(irand(0, 360));
						SetKeyValue(mapEnt, "angle", va("%i", BUILDING_ANGLES[i]));
					}
					else if (CITY_ANGLES_RANDOMIZE[FOLIAGE_TREE_SELECTION[i]] >= 2)
					{// 90/180/270 deg randomization...
						BUILDING_ANGLES[i] = 90.0 * float(irand(1, 3));
						SetKeyValue(mapEnt, "angle", va("%i", BUILDING_ANGLES[i]));
					}
					else
					{// No random angles on this model...
						BUILDING_ANGLES[i] = 0;
						SetKeyValue(mapEnt, "angle", "0");
					}
				}

				SetKeyValue(mapEnt, "_allowCollisionModelTypes", va("%i", CITY_ALLOW_SIMPLIFY[FOLIAGE_TREE_SELECTION[i]]));

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

				if (CITY_PLANE_SNAP[FOLIAGE_TREE_SELECTION[i]] >= 0)
				{
					SetKeyValue(mapEnt, "snap", va("%i", CITY_PLANE_SNAP[FOLIAGE_TREE_SELECTION[i]]));
				}

				/*{
				char str[32];
				vec3_t angles;
				vectoangles( FOLIAGE_NORMALS[i], angles );
				angles[PITCH] += 90;
				angles[YAW] = 270.0 - FOLIAGE_TREE_ANGLES[i];
				sprintf( str, "%.4f %.4f %.4f", angles[0], angles[1], angles[2] );
				SetKeyValue( mapEnt, "angles", str );
				}*/

				/*{
				char str[32];
				sprintf( str, "%i", 2 );
				SetKeyValue( mapEnt, "spawnflags", str );
				}*/

				//Sys_Printf( "Generated building at %.4f %.4f %.4f.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] );


				/* ydnar: get classname */
				if (USE_LODMODEL)
					SetKeyValue(mapEnt, "classname", "misc_lodmodel");
				else
					SetKeyValue(mapEnt, "classname", "misc_model");

				classname = ValueForKey(mapEnt, "classname");

				SetKeyValue(mapEnt, "model", CITY_MODELS[FOLIAGE_TREE_SELECTION[i]]); // test tree

																				  //Sys_Printf( "Generated building at %.4f %.4f %.4f. Model %s.\n", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2], TROPICAL_TREES[FOLIAGE_TREE_SELECTION[i]] );

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


#if defined(__ADD_PROCEDURALS_EARLY__)
				AddTriangleModels(0, qtrue, qtrue, qfalse, qfalse);
				EmitBrushes(mapEnt->brushes, &mapEnt->firstBrush, &mapEnt->numBrushes);
				//MoveBrushesToWorld( mapEnt );
				numEntities--;
#endif

				if (StringContainsWord(CITY_MODELS[FOLIAGE_TREE_SELECTION[i]], "campfire"))
				{
					//
					// Special case for campfires, also make a fx_runner for flames...
					//
					entitySourceBrushes = 0;
					mapEnt = &entities[numEntities];
					numEntities++;
					memset(mapEnt, 0, sizeof(*mapEnt));

					mapEnt->mapEntityNum = numEntities - 1;

					{
						VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
						mapEnt->origin[2] += CITY_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

						char str[32];
						sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] + 4.0);
						SetKeyValue(mapEnt, "origin", str);
					}

					SetKeyValue(mapEnt, "classname", "fx_runner");
					classname = ValueForKey(mapEnt, "classname");

					//SetKeyValue(mapEnt, "fxFile", "effects/campfire/campfire.efx");
					//SetKeyValue(mapEnt, "fxFile", "effects/campfire/campfire2.efx");
					SetKeyValue(mapEnt, "fxFile", "effects/campfire/campfire3.efx");


					//
					// Also add an ai marker... For future usage...
					//
					entitySourceBrushes = 0;
					mapEnt = &entities[numEntities];
					numEntities++;
					memset(mapEnt, 0, sizeof(*mapEnt));

					mapEnt->mapEntityNum = numEntities - 2;

					{
						VectorCopy(FOLIAGE_POSITIONS[i], mapEnt->origin);
						mapEnt->origin[2] += CITY_OFFSETS[FOLIAGE_TREE_SELECTION[i]];

						char str[32];
						sprintf(str, "%.4f %.4f %.4f", mapEnt->origin[0], mapEnt->origin[1], mapEnt->origin[2] + 4.0);
						SetKeyValue(mapEnt, "origin", str);
					}

					SetKeyValue(mapEnt, "classname", "ai_marker_campfire");
					classname = ValueForKey(mapEnt, "classname");
				}
			}

			if (!USE_SECONDARY_BSP)
			{
				free(BUILDING_ASSIGNED);
			}
			else if (USE_SECONDARY_BSP)
			{
				if (GENERATING_SECONDARY_BSP)
				{
					free(BUILDING_ASSIGNED);
				}
			}

#if defined(__ADD_PROCEDURALS_EARLY__)
			Sys_Printf("Finished adding city.\n");
#endif
		}
	}
}
