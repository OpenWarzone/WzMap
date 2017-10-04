#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>

#include "ch3d.h"
#include "q3map2.h"

#ifdef __MODEL_CONVEX_HULL__

using namespace std;

bool SaveOBJ(std::string fileName, float *verts, unsigned int numPoints, unsigned int *tris, unsigned int numTris)
{
    std::cout << "Saving " <<  fileName.c_str() << std::endl;

	char fileNameOut[128] = { 0 };
	char tempfileNameOut[128] = { 0 };

	strcpy(tempfileNameOut, fileName.c_str());
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir(tempfileNameOut);
	}

	strcpy(tempfileNameOut, fileName.c_str());
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir(tempfileNameOut);
	}

	strcpy(tempfileNameOut, fileName.c_str());
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir(tempfileNameOut);
	}

#define PATH_MAX 260
	extern char     g_strDirs[VFS_MAXDIRS][PATH_MAX];

    std::ofstream fout(va("%s%s", g_strDirs[0], fileName.c_str()));

    if (fout.is_open()) 
    {           
        for(size_t v = 0; v < numPoints; v+=3)
        {
            fout << "v " << verts[v+0] << " "
                         << verts[v+1] << " "
                         << verts[v+2] << std::endl;
        }
        for(size_t f = 0; f < numTris; f+=3)
        {
            fout <<"f " << tris[f+0] << " "
                        << tris[f+1] << " "
                        << tris[f+2] << std::endl;
        }
        fout.close();
        return true;
    }
    return false;
}

extern void LoadShaderImages( shaderInfo_t *si );
extern qboolean StringContainsWord(const char *haystack, const char *needle);

void ConvexHull ( picoModel_t *model, char *fileNameOut )
{
	if( model == NULL )
		return;

	unsigned int		numVerts = 0;
	float				*verts = (float*)malloc(3 * 1048576 * sizeof(float));
	unsigned int		numTris = 0;
	unsigned int		*tris = (unsigned int*)malloc(3 * 1048576 * sizeof(unsigned int));

	/* each surface on the model will become a new map drawsurface */
	int		numSurfaces = PicoGetModelNumSurfaces(model);

	for( int s = 0; s < numSurfaces; s++ )
	{
		int				skin = 0;

		/* get surface */
		picoSurface_t	*surface = PicoGetModelSurface( model, s );

		if( surface == NULL )
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
		{
			Sys_Warning("surface %i (%s) is not a triangles surface.", s, surface->name);
			continue;
		}

		char			*picoShaderName = PicoGetSurfaceShaderNameForSkin( surface, skin );

		shaderInfo_t	*si = ShaderInfoForShader( picoShaderName );

		LoadShaderImages( si );

		/*if (!si->clipModel)
		{
			Sys_Warning("surface %i (%s) is not a clip surface.", s, surface->name);
			continue;
		}*/

		if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
		{
			Sys_Warning("surface %i (%s) is not a visible surface.", s, surface->name);
			continue;
		}

		if( !si->clipModel 
			&& ((si->compileFlags & C_TRANSLUCENT) || !(si->compileFlags & C_SOLID)) )
		{
			Sys_Warning("surface %i (%s) is not a solid surface.", s, surface->name);
			continue;
		}
		
		//
		// UQ1: Something seems to be really wrong with this... I don't get it...
		//
#if 0
		/* get info */
		picoIndex_t *idx = PicoGetSurfaceIndexes(surface, 0);

		/* walk the triangle list */
		for (int j = 0; j < PicoGetSurfaceNumIndexes(surface); j += 3, idx += 3)
		{
			for (int k = 0; k < 3; k++)
			{
				tris[numTris++] = idx[j+k];

				picoVec_t *xyz = PicoGetSurfaceXYZ(surface, idx[k]);
				verts[numVerts++] = xyz[0]; // stupid points array crap...
				verts[numVerts++] = xyz[1];
				verts[numVerts++] = xyz[2];
			}
		}
#else
		int nV = PicoGetSurfaceNumVertexes(surface);

		for (int i = 0; i < nV; i++)
		{
			picoVec_t *xyz = PicoGetSurfaceXYZ(surface, i);

			verts[numVerts++] = xyz[0];
			verts[numVerts++] = xyz[1];
			verts[numVerts++] = xyz[2];
		}

		int nI = PicoGetSurfaceNumIndexes(surface);
		picoIndex_t	*idx = PicoGetSurfaceIndexes(surface, 0);

		for (int i = 0; i < nI; i += 3)
		{
			tris[numTris++] = idx[i + 0];
			tris[numTris++] = idx[i + 1];
			tris[numTris++] = idx[i + 2];
		}
#endif
	}

	Sys_Printf("PRE-DECIMATE: model %s. verts %u. indexes %u.\n", model->fileName, numVerts, numTris * 3);

	//PrintList((float *)verts, numVerts, indexes, numIndexes / 3, 3);

	unsigned int triSize = 3;

	ConvexHull3D(&verts, &numVerts, &tris, &numTris, &triSize, FACETS);

	Sys_Printf("POST-DECIMATE: model %s. verts %u. indexes %u.\n", model->fileName, numVerts, numTris * 3);

	SaveOBJ(fileNameOut, verts, numVerts, tris, numTris * 3);

	free(verts);
	free(tris);
}

#endif //__MODEL_CONVEX_HULL__
