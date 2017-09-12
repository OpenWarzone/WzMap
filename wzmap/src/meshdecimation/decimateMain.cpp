/* Copyright (c) 2011 Khaled Mamou (kmamou at gmail dot com)
 All rights reserved.
 
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 3. The names of the contributors may not be used to endorse or promote products derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


//#define __DECIMATE_METHOD__


#define _CRT_SECURE_NO_WARNINGS
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <vector>

//#define _CRTDBG_MAP_ALLOC

#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif // _CRTDBG_MAP_ALLOC

#include "../q3map2.h"

#ifdef __DECIMATE_METHOD__
#include "mdMeshDecimator.h"

using namespace MeshDecimation;
using namespace std;

bool SaveOBJ(const std::string          & fileName, 
			 const vector< Vec3<Float> > & points,
             const vector< Vec3<int> > & triangles)
{
    std::cout << "Saving " <<  fileName << std::endl;
    std::ofstream fout(fileName.c_str());
    if (fout.is_open()) 
    {           
        const size_t nV = points.size();
        const size_t nT = triangles.size();
        for(size_t v = 0; v < nV; v++)
        {
            fout << "v " << points[v][0] << " " 
                         << points[v][1] << " " 
                         << points[v][2] << std::endl;
        }
        for(size_t f = 0; f < nT; f++)
        {
            fout <<"f " << triangles[f][0]+1 << " " 
                        << triangles[f][1]+1 << " "
                        << triangles[f][2]+1 << std::endl;
        }
        fout.close();
        return true;
    }
    return false;
}

void CallBack(const char * msg)
{
    Sys_Printf(msg);
}
#else //__SIMPLIFY_METHOD__
#include "../meshsimplification/simplify.h"
#endif //__DECIMATE_METHOD__

extern void LoadShaderImages( shaderInfo_t *si );

void Decimate ( picoModel_t *model, char *fileNameOut )
{
#ifdef __DECIMATE_METHOD__
	vector< Vec3<Float> > points;
	vector< Vec3<int> > triangles;

	int					s, numSurfaces;

	int skin = 0;

	if( model == NULL )
		return;

	/* each surface on the model will become a new map drawsurface */
	numSurfaces = PicoGetModelNumSurfaces( model );

	for( s = 0; s < numSurfaces; s++ )
	{
		int					i;
		picoVec_t			*xyz;
		picoSurface_t		*surface;

		/* get surface */
		surface = PicoGetModelSurface( model, s );

		if( surface == NULL )
			continue;

		/* only handle triangle surfaces initially (fixme: support patches) */
		if( PicoGetSurfaceType( surface ) != PICO_TRIANGLES )
			continue;

		char				*picoShaderName;
		picoShaderName = PicoGetSurfaceShaderNameForSkin( surface, skin );

		shaderInfo_t		*si = ShaderInfoForShader( picoShaderName );

		LoadShaderImages( si );

		if(!si->clipModel) continue;

		if ((si->compileFlags & C_TRANSLUCENT) || (si->compileFlags & C_SKIP) || (si->compileFlags & C_FOG) || (si->compileFlags & C_NODRAW) || (si->compileFlags & C_HINT))
		{
			continue;
		}

		if( !si->clipModel 
			&& ((si->compileFlags & C_TRANSLUCENT) || !(si->compileFlags & C_SOLID)) )
		{
			continue;
		}
		
		/* copy vertexes */
		for( i = 0; i < PicoGetSurfaceNumVertexes( surface ); i++ )
		{
			vec3_t xyz2;
			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ( surface, i );
			VectorCopy( xyz, xyz2 );
			//m4x4_transform_point( transform, xyz2 );

			Vec3<Float> x;
			x[0] = xyz[0];
			x[1] = xyz[1];
			x[2] = xyz[2];
			points.push_back(x);

			

			picoSurface_t		*surface;
			picoIndex_t			*indexes;

			/* get surface */
			surface = PicoGetModelSurface( model, s );
			if( surface == NULL )
				continue;

			/* only handle triangle surfaces initially (fixme: support patches) */
			if( PicoGetSurfaceType( surface ) != PICO_TRIANGLES )
				continue;

			int numIndexes = PicoGetSurfaceNumIndexes( surface );

			indexes = PicoGetSurfaceIndexes( surface, 0 );

			/* walk triangle list */
			for( int k = 0; k < numIndexes; k += 3 )
			{
				Vec3<int> ip;
				int					j;

				/* make points and back points */
				for( j = 0; j < 3; j++ )
				{
					/* get vertex */
					ip[j] = indexes[ k + j ];
				}

				triangles.push_back(ip);
			}
		}
	}

	Sys_Printf("PRE-DECIMATE: model %s. verts %i. indexes %i.\n", model->fileName, (int)points.size(), (int)triangles.size());


	size_t        targetNTrianglesDecimatedMesh = (int)triangles.size()/3;//200;//1000;          // 1000
	size_t		  targetNVerticesDecimatedMesh  = points.size()/3;//100;//500;          // 500
	Float		  maxDecimationError = static_cast<Float>(0.3); // 1.0

	// decimate mesh
    MeshDecimator myMDecimator;
    myMDecimator.SetCallBack(&CallBack);
    myMDecimator.Initialize(points.size(), triangles.size(), &points[0], &triangles[0]);
	myMDecimator.Decimate(targetNVerticesDecimatedMesh, 
						  targetNTrianglesDecimatedMesh, 
						  maxDecimationError);

	// allocate memory for decimated mesh
	vector< Vec3<Float> > decimatedPoints;
    vector< Vec3<int> > decimatedtriangles;
    decimatedPoints.resize(myMDecimator.GetNVertices());
    decimatedtriangles.resize(myMDecimator.GetNTriangles());

	// retreive decimated mesh
    myMDecimator.GetMeshData(&decimatedPoints[0], &decimatedtriangles[0]);

	Sys_Printf("POST-DECIMATE: model %s. verts %i. indexes %i.\n", model->fileName, (int)decimatedPoints.size(), (int)decimatedtriangles.size());

	char fileNameOut[128] = { 0 };
	char tempfileNameOut[128] = { 0 };

	strcpy(tempfileNameOut, model->fileName);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	strcpy(tempfileNameOut, model->fileName);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	strcpy(tempfileNameOut, model->fileName);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	strcpy(tempfileNameOut, model->fileName);
	StripExtension( tempfileNameOut );
	sprintf(fileNameOut, "%s_lod.obj", tempfileNameOut);
    SaveOBJ(fileNameOut, decimatedPoints, decimatedtriangles);
#else //__SIMPLIFY_METHOD__
	Simplify::UsePicoModel(model);

	if ((Simplify::triangles.size() < 3) || (Simplify::vertices.size() < 3))
		return;

	//int target_count =  Simplify::triangles.size() >> 1;
	int target_count =  Simplify::triangles.size() * 0.3;
    double agressiveness = 3.0;

	clock_t start = clock();

	Sys_Printf("Input: vertices: %i. triangles %i. reduction target %i.\n", (int)Simplify::vertices.size(), (int)Simplify::triangles.size(), target_count);

	int startSize = (int)Simplify::triangles.size();

	
	//Simplify::simplify_mesh(target_count, agressiveness, false);
	Simplify::simplify_mesh_lossless( false );

	if ( Simplify::triangles.size() >= startSize) {
		printf("Unable to reduce mesh.\n");
    	return;
	}
	

	char tempfileNameOut[128] = { 0 };

	strcpy(tempfileNameOut, fileNameOut);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	strcpy(tempfileNameOut, fileNameOut);
	StripFilename(tempfileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	strcpy(tempfileNameOut, fileNameOut);
	StripFilename(tempfileNameOut);
	if (strlen(tempfileNameOut) > 0)
	{
		//Sys_Printf("Create %s.\n", tempfileNameOut);
		Q_mkdir( tempfileNameOut );
	}

	Simplify::write_obj(fileNameOut);

	Sys_Printf("Output: vertices: %i. triangles %i. (%f reduction; %i sec)\n", (int)Simplify::vertices.size(), (int)Simplify::triangles.size()
		, (float)((float)Simplify::triangles.size()/ (float)startSize)*100.0  , ((int)(clock()-start))/CLOCKS_PER_SEC );
#endif //__DECIMATE_METHOD__
}
