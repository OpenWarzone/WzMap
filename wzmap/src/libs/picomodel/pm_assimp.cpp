/* -----------------------------------------------------------------------------

PicoModel Library

Copyright (c) 2002, Randy Reddig & seaw0lf
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the names of the copyright holders nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------------- */



/* marker */
#define PM_ASSIMP_C

// assimp include files. These three are usually needed.
#include "assimp/Importer.hpp"	//OO version Header!
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/DefaultLogger.hpp"
#include "assimp/LogStream.hpp"

/* disable warnings */
#ifdef WIN32
#pragma warning( disable:4100 )		/* unref param */
#endif

extern "C" {
	/* dependencies */
	#include "picointernal.h"
}

/* remarks:
 * - loader seems stable
 * todo:
 * - fix uv coordinate problem
 * - check for buffer overflows ('bufptr' accesses)
 */
/* uncomment when debugging this module */
//#define DEBUG_PM_ASSIMP
//#define DEBUG_PM_ASSIMP_EX

#define ASSIMP_MODEL_SCALE		64.0
#define	MD3_XYZ_SCALE			(1.0/64)

/* plain white */
static picoColor_t white = { 255,255,255,255 };

/* assimp limits */
#define ASSIMP_MAX_VERTS		32768
#define ASSIMP_MAX_TRIS			65536
#define ASSIMP_MAX_GROUPS		128
#define ASSIMP_MAX_MATERIALS	128
#define ASSIMP_MAX_JOINTS		128
#define ASSIMP_MAX_KEYFRAMES	216

/*
void ClearBounds(picoVec3_t mins, picoVec3_t maxs) {
	mins[0] = mins[1] = mins[2] = 999999;
	maxs[0] = maxs[1] = maxs[2] = -999999;
}

void AddPointToBounds(const picoVec3_t v, picoVec3_t mins, picoVec3_t maxs) {
	if (v[0] < mins[0]) {
		mins[0] = v[0];
	}
	if (v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if (v[1] < mins[1]) {
		mins[1] = v[1];
	}
	if (v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if (v[2] < mins[2]) {
		mins[2] = v[2];
	}
	if (v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}
*/

void VectorSet(picoVec3_t vec, float x, float y, float z) {
	vec[0] = x; vec[1] = y; vec[2] = z;
}

std::string AssImp_getBasePath(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	return (std::string::npos == pos) ? "" : path.substr(0, pos + 1);
}

std::string AssImp_getTextureName(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	return (std::string::npos == pos) ? "" : path.substr(pos + 1, std::string::npos);
}

bool StringContainsWord(const char *haystack, const char *needle)
{
	if (!*needle)
	{
		return false;
	}
	for (; *haystack; ++haystack)
	{
		if (toupper(*haystack) == toupper(*needle))
		{
			/*
			* Matched starting char -- loop through remaining chars.
			*/
			const char *h, *n;
			for (h = haystack, n = needle; *h && *n; ++h, ++n)
			{
				if (toupper(*h) != toupper(*n))
				{
					break;
				}
			}
			if (!*n) /* matched all of 'needle' to null termination */
			{
				return true; /* return the start of the match */
			}
		}
	}
	return false;
}

void COM_StripExtension(const char *in, char *out, int destsize)
{
	const char *dot = strrchr(in, '.'), *slash;
	if (dot && (!(slash = strrchr(in, '/')) || slash < dot))
		destsize = (destsize < dot - in + 1 ? destsize : dot - in + 1);

	if (in == out && destsize > 1)
		out[destsize - 1] = '\0';
	else
		strncpy(out, in, destsize);
}

std::string StripPath(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	return (std::string::npos == pos) ? path.substr(0, pos) : path.substr(pos + 1, std::string::npos);
}

std::string StripExtension(const std::string& path)
{
	size_t pos = path.find_last_of(".");
	return path.substr(0, pos);
}

std::string replace_all(
	const std::string & str,   // where to work
	const std::string & find,  // substitute 'find'
	const std::string & replace //      by 'replace'
) {
	using namespace std;
	string result;
	size_t find_len = find.size();
	size_t pos, from = 0;
	while (string::npos != (pos = str.find(find, from))) {
		result.append(str, from, pos - from);
		result.append(replace);
		from = pos + find_len;
	}
	result.append(str, from, string::npos);
	return result;
}

bool FS_FileExists(const char *testpath)
{
	struct stat buffer;
	return (bool)(stat(testpath, &buffer) == 0);
}

char *textureExtNames[] = {
	"png",
	"tga",
	"jpg",
	"dds",
	"gif",
	"bmp",
	"ico"
};

char texName[512] = { 0 }; // not thread safe, but we are not threading anywhere here...

char *FS_TextureFileExists(const char *name)
{
	if (!name || !name[0] || name[0] == '\0' || strlen(name) < 1) return NULL;

	for (int i = 0; i < 7; i++)
	{
		memset(&texName, 0, sizeof(char) * 512);
		COM_StripExtension(name, texName, sizeof(texName));
		sprintf(texName, "%s.%s", name, textureExtNames[i]);

		if (FS_FileExists(texName))
		{
			return textureExtNames[i];
		}
	}

	return NULL;
}

// Create an instance of the Importer class
Assimp::Importer assImpImporter;

/*
#define aiProcessPreset_TargetRealtime_MaxQuality_Fix ( \
	aiProcessPreset_TargetRealtime_Quality	|  \
	aiProcess_FlipWindingOrder			|  \
    aiProcess_FixInfacingNormals			|  \
    aiProcess_FindInstances                  |  \
    aiProcess_ValidateDataStructure          |  \
	aiProcess_OptimizeMeshes                 |  \
    0 )
*/

//aiProcess_ImproveCacheLocality	|\

#if 1
#define aiProcessPreset_TargetRealtime_MaxQuality_Fix (\
	aiProcess_JoinIdenticalVertices	|\
	aiProcess_GenNormals	|\
	aiProcess_ValidateDataStructure	|\
	aiProcess_FindInvalidData	|\
	aiProcess_FindDegenerates	|\
	aiProcess_GenUVCoords	|\
	aiProcess_TransformUVCoords	|\
	aiProcess_Triangulate	|\
	aiProcess_SortByPType	|\
	aiProcess_FindInstances                  |\
    aiProcess_ValidateDataStructure          |\
	aiProcess_OptimizeMeshes                 |\
	aiProcess_GenSmoothNormals              |\
	aiProcess_RemoveRedundantMaterials		|\
	aiProcess_OptimizeGraph					|\
    0 )
#else
#define aiProcessPreset_TargetRealtime_MaxQuality_Fix (\
	aiProcess_JoinIdenticalVertices	|\
	aiProcess_GenNormals	|\
	aiProcess_ValidateDataStructure	|\
	aiProcess_FindInvalidData	|\
	aiProcess_FindDegenerates	|\
	aiProcess_GenUVCoords	|\
	aiProcess_TransformUVCoords	|\
	aiProcess_Triangulate	|\
    aiProcess_ValidateDataStructure          |\
	aiProcess_RemoveRedundantMaterials		|\
    0 )
#endif

/* _assimp_canload:
 *	validates a assimp model file.
 */
/*static*/ int CPP_assimp_canload( PM_PARAMS_CANLOAD )
{
	const aiScene* scene = assImpImporter.ReadFileFromMemory(buffer, bufSize, aiProcessPreset_TargetRealtime_MaxQuality_Fix/*, ext*/);

	if (!scene)
	{
		printf("_assimp_load: %s could not load. Error: %s\n", fileName, assImpImporter.GetErrorString());
		return PICO_PMV_ERROR;
	}

	// No longer need the scene data...
	assImpImporter.FreeScene();

	/* file seems to be a valid model */
	return PICO_PMV_OK;
}

/* _assimp_load:
 *	loads a milkshape3d model file.
*/
/*static*/ picoModel_t *CPP_assimp_load( PM_PARAMS_LOAD )
{
	picoModel_t	   *model;
	unsigned char  *bufptr;
	int				shaderRefs[ ASSIMP_MAX_GROUPS ];
	int				numGroups;
	int				numMaterials;
	unsigned int	i,k,m;


	const aiScene* scene = assImpImporter.ReadFileFromMemory(buffer, bufSize, aiProcessPreset_TargetRealtime_MaxQuality_Fix/*, ext*/);

	if (!scene)
	{
		printf("_assimp_load: %s could not load. Error: %s\n", fileName, assImpImporter.GetErrorString());
		return NULL;
	}

	/* create new pico model */
	model = PicoNewModel();
	
	if (model == NULL)
	{
		// No longer need the scene data...
		assImpImporter.FreeScene();

		return NULL;
	}

	/* do model setup */
	PicoSetModelFrameNum( model, frameNum );
	PicoSetModelName( model, fileName );
	PicoSetModelFileName( model, fileName );

/*
	// Calculate the bounds/radius info...
	picoVec3_t bounds[2];
	ClearBounds(bounds[0], bounds[1]);

	for (i = 0; i < scene->mNumMeshes; i++)
	{
		for (k = 0; k < scene->mMeshes[i]->mNumVertices; k++)
		{
			picoVec3_t origin;
			VectorSet(origin, scene->mMeshes[i]->mVertices[k].x * MD3_XYZ_SCALE * ASSIMP_MODEL_SCALE, scene->mMeshes[i]->mVertices[k].y * MD3_XYZ_SCALE * ASSIMP_MODEL_SCALE, scene->mMeshes[i]->mVertices[k].z * MD3_XYZ_SCALE * ASSIMP_MODEL_SCALE);
			AddPointToBounds(origin, bounds[0], bounds[1]);
		}
	}
*/

	std::string basePath = AssImp_getBasePath(fileName);

	/* get number of groups */
	numGroups = scene->mNumMeshes;

#ifdef DEBUG_PM_ASSIMP
	printf("NumGroups: %d\n",numGroups);
#endif
	/* run through all groups in model */
	for (i = 0; i < numGroups && i < ASSIMP_MAX_GROUPS; i++)
	{
		aiMesh			*aiSurf = scene->mMeshes[i];
		picoSurface_t	*surface;

		/* create new pico surface */
		surface = PicoNewSurface( model );

		if (surface == NULL)
		{
			PicoFreeModel( model );
			
			// No longer need the scene data...
			assImpImporter.FreeScene();

			return NULL;
		}

		aiString		shaderPath;	// filename
		scene->mMaterials[aiSurf->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &shaderPath);
		
		// Clean up the shaderpath string, make sure it's in textures/whatever/whatever format, not crap like "..\textures\whatever\whatever.jpg"...
		shaderPath = StripExtension(shaderPath.C_Str()).c_str();
		std::string sp = replace_all(replace_all(shaderPath.C_Str(), "\\", "/"), "../", "");
		shaderPath = sp.c_str();

		// Now make sure we have a nice clean surface name (using the shader name if needed)...
		if (StringContainsWord(fileName, "_collision"))
		{
			aiSurf->mName = "collision";
			shaderPath = "collision";
		}
		else if (aiSurf->mName.length <= 0)
		{
			aiSurf->mName = StripPath(shaderPath.C_Str()).c_str();
		}
		else if (!strncmp(aiSurf->mName.C_Str(), ".", 1) && shaderPath.length > 0 && !StringContainsWord(shaderPath.C_Str(), "collision"))
		{// ASE models... they are nutty...
			aiSurf->mName = StripPath(shaderPath.C_Str()).c_str();

			if (aiSurf->mName.length <= 0)
			{
				aiSurf->mName = shaderPath.C_Str();
			}
		}

		/* do surface setup */
		PicoSetSurfaceType( surface, PICO_TRIANGLES );
		PicoSetSurfaceName( surface, (char *)aiSurf->mName.C_Str() );
		
		/* process triangle indices */
		for (k = 0; k < aiSurf->mNumFaces; k++)
		{
			unsigned int tri[3];

			/* get ptr to triangle data */
			tri[0] = aiSurf->mFaces[k].mIndices[0];
			tri[1] = aiSurf->mFaces[k].mIndices[1];
			tri[2] = aiSurf->mFaces[k].mIndices[2];

			/* run through triangle vertices */
			//if (aiSurf->mFaces[k].mNumIndices >= 3)
			{
				for (m = 0; m < 3 && m < aiSurf->mFaces[k].mNumIndices; m++)
				{
					unsigned int	vertIndex = tri[m];
					picoVec2_t		texCoord;
					picoVec3_t		vert, n;

					aiVector3D xyz = aiSurf->mVertices[vertIndex];
					VectorSet(vert, xyz.x, xyz.y, xyz.z);

					aiVector3D norm = aiSurf->mNormals[vertIndex];
					VectorSet(n, norm.x, norm.y, norm.z);

					/* store vertex origin */
					PicoSetSurfaceXYZ(surface, tri[m], vert);

					/* store vertex color */
					PicoSetSurfaceColor(surface, 0, vertIndex, white);

					/* store vertex normal */
					PicoSetSurfaceNormal(surface, vertIndex, n);

					/* store current face vertex index */
					PicoSetSurfaceIndex(surface, (k * 3 + (2 - m)), (picoIndex_t)vertIndex);

					/* get texture vertex coord */
					if (aiSurf->mNormals != NULL && aiSurf->HasTextureCoords(0))
					{
						texCoord[0] = aiSurf->mTextureCoords[0][vertIndex].x;
						texCoord[1] = -aiSurf->mTextureCoords[0][vertIndex].y;	/* flip t */
						//texCoord[ 1 ] = 1 - aiSurf->mTextureCoords[0][vertIndex].y;	/* flip t */
					}
					else
					{
						texCoord[0] = 0.0;
						texCoord[1] = 1.0;
					}

					/* store texture vertex coord */
					PicoSetSurfaceST(surface, 0, vertIndex, texCoord);
				}
			}
		}

		/* store material */
		shaderRefs[i] = aiSurf->mMaterialIndex;

#ifdef DEBUG_PM_ASSIMP
		printf("Group %d: '%s' (%d tris)\n", i, aiSurf->mName.C_Str(), aiSurf->mNumFaces);
#endif

		picoShader_t *shader;
		picoColor_t   ambient,diffuse,specular;


		aiString		origShaderPath;	// filename
		origShaderPath = shaderPath;

		bool foundName = false;

		if (!StringContainsWord(shaderPath.C_Str(), "/") && !StringContainsWord(shaderPath.C_Str(), "\\") && !StringContainsWord(shaderPath.C_Str(), "collision"))
		{// Missing any folder info, use model's base path...
			shaderPath = basePath + shaderPath.C_Str();
			origShaderPath = shaderPath;
		}


		//printf("DEBUG: fileName: %s. aiSurf->mName: %s. shaderPath %s. basePath %s.\n", fileName, aiSurf->mName.C_Str(), shaderPath.C_Str(), basePath.c_str());


		if (aiSurf->mName.length > 0 && FS_FileExists(aiSurf->mName.C_Str()))
		{// Original file+path exists... Use it...
			shaderPath = aiSurf->mName;
			foundName = true;
		}

		if (shaderPath.length > 0 && FS_FileExists(shaderPath.C_Str()))
		{// Original file+path exists... Use it...
			foundName = true;
		}

		if (aiSurf->mName.length > 0 && FS_TextureFileExists(aiSurf->mName.C_Str()))
		{// Original file+path exists... Use it...
			char *nExt = FS_TextureFileExists(aiSurf->mName.C_Str());

			if (nExt && nExt[0])
			{
				shaderPath = nExt;
				foundName = true;
			}
			foundName = true;
		}

		if (shaderPath.length > 0 && FS_TextureFileExists(shaderPath.C_Str()))
		{// Original file+path exists... Use it...
			char *nExt = FS_TextureFileExists(shaderPath.C_Str());

			if (nExt && nExt[0])
			{
				shaderPath = nExt;
				foundName = true;
			}
			foundName = true;
		}

		if (!foundName)
		{// Grab the filename from the original path.
			std::string textureName = AssImp_getTextureName(shaderPath.C_Str());


			//printf("DEBUG: shaderPath is %s.\n", shaderPath.C_Str());


			// Free the shaderPath so that we can replace it with what we find...
			shaderPath.Clear();


			//printf("DEBUG: textureName is %s.\n", textureName.c_str());


			if (textureName.length() > 0 && !foundName)
			{// Check if the file exists in the current directory...
				if (FS_FileExists(textureName.c_str()))
				{
					shaderPath = textureName.c_str();
					foundName = true;
				}
				//else
				//{
				//	Q_Printf("DEBUG: %s does not exist.\n", textureName.c_str());
				//}
			}

			if (textureName.length() > 0 && !foundName)
			{// Check if filename.ext exists in the current directory...
				char *nExt = FS_TextureFileExists(textureName.c_str());

				if (nExt && nExt[0])
				{
					shaderPath = nExt;
					foundName = true;
				}
				//else
				//{
				//	Q_Printf("DEBUG: %s does not exist.\n", nExt);
				//}
			}

			if (textureName.length() > 0 && !foundName)
			{// Search for the file... Make final dir/filename.ext
				char out[256] = { 0 };
				COM_StripExtension(textureName.c_str(), out, sizeof(out));
				textureName = out;

				char shaderRealPath[256] = { 0 };
				sprintf(shaderRealPath, "%s%s", basePath.c_str(), textureName.c_str());

				char *nExt = FS_TextureFileExists(shaderRealPath);

				if (nExt && nExt[0])
				{
					shaderPath = nExt;
					foundName = true;
				}
				//else
				//{
				//	Q_Printf("DEBUG: %s does not exist.\n", nExt);
				//}
			}

			if (shaderPath.length == 0)
			{// We failed... Set name do "unknown".
				shaderPath = origShaderPath;// "unknown";
			}
		}



		/* create new pico shader */
		shader = PicoNewShader( model );
		if (shader == NULL)
		{
			PicoFreeModel( model );

			// No longer need the scene data...
			assImpImporter.FreeScene();

			return NULL;
		}
		/* scale shader colors */
		for (k=0; k<4; k++)
		{
			ambient [ k ] = 255;
			diffuse [ k ] = 255;
			specular[ k ] = 255;
		}
		/* set shader colors */
		PicoSetShaderAmbientColor( shader, ambient );
		PicoSetShaderDiffuseColor( shader, diffuse );
		PicoSetShaderSpecularColor( shader, specular );

		/* set shader transparency */
		PicoSetShaderTransparency( shader, 1.0 );

		/* set shader shininess (0..127) */
		PicoSetShaderShininess( shader, 127.0 );

		/* set shader name */
		PicoSetShaderName( shader, (char *)shaderPath.C_Str());

		/* set shader texture map name */
		PicoSetShaderMapName( shader, (char *)shaderPath.C_Str());


#ifdef DEBUG_PM_ASSIMP
		printf("Material %d: '%s' ('%s')\n", i, aiSurf->mName.C_Str(), shaderPath.C_Str());
#endif
	}

	/* assign shaders to surfaces */
	for (i = 0; i < numGroups && i < ASSIMP_MAX_GROUPS; i++)
	{
		picoSurface_t *surface;
		picoShader_t  *shader;

		/* sanity check */
		if (shaderRefs[ i ] >= ASSIMP_MAX_MATERIALS || shaderRefs[ i ] < 0)
			continue;

		/* get surface */
		surface = PicoGetModelSurface( model, i );
		if (surface == NULL) continue;

		/* get shader */
		shader = PicoGetModelShader( model, i/*shaderRefs[ i ]*/ );
		if (shader == NULL) continue;

		/* assign shader */
		PicoSetSurfaceShader( surface, shader );

#ifdef DEBUG_PM_ASSIMP
		printf("Mapped: %d ('%s') to %d (%s)\n", shaderRefs[i], shader->name, i, surface->name);
#endif
	}

	// No longer need the scene data...
	assImpImporter.FreeScene();

	/* return allocated pico model */
	return model;
}

extern "C" {
	int C_assimp_canload(PM_PARAMS_CANLOAD)
	{
		return CPP_assimp_canload(fileName, buffer, bufSize);
	}

	picoModel_t *C_assimp_load(PM_PARAMS_LOAD)
	{
		return CPP_assimp_load(fileName, frameNum, buffer, bufSize);
	}
}