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
// nav.cpp -- navigation mesh generator interface
//extern "C" {
#include "q3map2.h"
//}
#include <vector>
#include <queue>
#include <iostream>
#include <cstdlib>
#include "navgen.h"
#include "inifile.h"

Geometry geo;


#define __IGNORE_EXTRA_SURFACES__


#define STEPSIZE 18

float NAVMESH_SCALE = 1.0f;
float NAVMESH_ENTITY_SCALE = 1.0f;

float cellHeight = 2.0f;
float cellheightDivisor = 3.0f;
qboolean ignoreSmallTriangles = qfalse;
qboolean ignoreRock = qfalse;
qboolean ignoreTreeLeaves = qfalse;
float mergeSizeMult = 1.0;
float stepSize = STEPSIZE;
int   tileSize = 64;

extern float MAP_WATER_LEVEL;

struct Character_nav
{
	const char *name;   //appended to filename
	float radius; //radius of agents (BBox maxs[0] or BBox maxs[1])
	float height; //height of agents (BBox maxs[2] - BBox mins[2])
};

#if 0
static const Character_nav navcharacters[] = {
	{ "builder",     20, 40 },
//	{ "builderupg",  20, 40 },
	{ "human_naked", 15, 56 },
//	{ "human_light", 15, 56 },
//	{ "human_medium",15, 56 },
	{ "human_bsuit", 15, 76 },
	{ "level0",      15, 30 },
//	{ "level0upg",   15, 30 },
	{ "level1",      18, 36 },
	{ "level1upg",   21, 42 },
	{ "level2",      23, 36 },
	{ "level2upg",   25, 40 },
	{ "level3",      26, 55 },
	{ "level3upg",   26, 66 },
	{ "level4",      32, 92 }
};
#else
static const Character_nav navcharacters[] = {
	//{ "humanoid",     16, 56 }
	{ "humanoid",     32, 64 }
};
#endif

//flag for excluding caulk surfaces
static qboolean excludeCaulk = qtrue;

//flag for excluding surfaces with surfaceparm sky from navmesh generation
static qboolean excludeSky = qtrue;

//flag for adding new walkable spans so bots can walk over small gaps
static qboolean filterGaps = qtrue;

//copied from cm_patch.c
static const int MAX_GRID_SIZE = 129;
#define POINT_EPSILON 0.1
#define WRAP_POINT_EPSILON 0.1
static const int SUBDIVIDE_DISTANCE = 16;

typedef struct
{
	int      width;
	int      height;
	qboolean wrapWidth;
	qboolean wrapHeight;
	vec3_t   points[ MAX_GRID_SIZE ][ MAX_GRID_SIZE ]; // [width][height]
} cGrid_t;

static void WriteNavMeshFile( const char* agentname, const dtTileCache *tileCache, const dtNavMeshParams *params ) {
	int numTiles = 0;
	FILE *file = NULL;
	char filename[ 1024 ];
	NavMeshSetHeader header;
	const int maxTiles = tileCache->getTileCount();

	for( int i = 0; i < maxTiles; i++ )
	{
		const dtCompressedTile *tile = tileCache->getTile( i );
		if ( !tile || !tile->header || !tile->dataSize )
		{
			continue;
		}
		numTiles++;
	}

	header.magic = NAVMESHSET_MAGIC;
	header.version = NAVMESHSET_VERSION;
	header.numTiles = numTiles;
	header.cacheParams = *tileCache->getParams();
	header.params = *params;

	SwapNavMeshSetHeader( header );

	strcpy( filename, source );
	StripExtension( filename );
	sprintf( filename, "%s-%s", filename, agentname );
	DefaultExtension( filename, ".navMesh" );
	file = fopen( filename, "wb" );

	if( !file )
	{
		Error( "Error opening %s: %s\n", filename, strerror( errno ) );
	}

	fwrite( &header, sizeof( header ), 1, file );

	vec3_t bmin;
	VectorCopy(geo.getMins(), bmin);
	vec3_t bmax;
	VectorCopy(geo.getMaxs(), bmax);
	recast2quake(bmin);
	recast2quake(bmax);
	
	//printf("mins: %f %f %f. maxs: %f %f %f.\n", bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]);

	fwrite(bmin, sizeof(vec3_t), 1, file);
	fwrite(bmax, sizeof(vec3_t), 1, file);

	if (NAVMESHSET_VERSION >= 4)
	{// UQ1: Added in NAVMESHSET_VERSION 4
		fwrite(&NAVMESH_SCALE, sizeof(float), 1, file);
	}

	for( int i = 0; i < maxTiles; i++ )
	{
		const dtCompressedTile *tile = tileCache->getTile( i );

		if ( !tile || !tile->header || !tile->dataSize )
		{
			continue;
		}

		NavMeshTileHeader tileHeader;
		tileHeader.tileRef = tileCache->getTileRef( tile );
		tileHeader.dataSize = tile->dataSize;

		SwapNavMeshTileHeader( tileHeader );
		fwrite( &tileHeader, sizeof( tileHeader ), 1, file );

		unsigned char* data = ( unsigned char * ) malloc( tile->dataSize );

		memcpy( data, tile->data, tile->dataSize );
		if ( LittleLong( 1 ) != 1 )
		{
			dtTileCacheHeaderSwapEndian( data, tile->dataSize );
		}

		fwrite( data, tile->dataSize, 1, file );

		free( data );
	}
	fclose( file );
}

//need this to get the windings for brushes
extern qboolean FixWinding( winding_t* w );

static void AddVert(std::vector<float> &verts, std::vector<int> &tris, vec3_t vert) {
	vec3_t recastVert;
	VectorCopy(vert, recastVert);
	quake2recast(recastVert);
	int index = 0;
	for(int i=0;i<3;i++) {
		verts.push_back(recastVert[i]);
	}
	index = (verts.size() - 3)/3;
	tris.push_back(index);
}

static void AddTri(std::vector<float> &verts, std::vector<int> &tris, vec3_t v1, vec3_t v2, vec3_t v3) {
	AddVert(verts, tris, v1);
	AddVert(verts, tris, v2);
	AddVert(verts, tris, v3);
}

extern qboolean StringContainsWord(const char *haystack, const char *needle);

qboolean isTreeSolid( char *strippedName )
{
	if (StringContainsWord(strippedName, "bark") || StringContainsWord(strippedName, "trunk") || StringContainsWord(strippedName, "giant_tree") || StringContainsWord(strippedName, "vine01"))
		return qtrue;

	return qfalse;
}

static void LoadGeometry()
{
	std::vector<float> verts;
	std::vector<int> tris;

	//Sys_Printf("Loading geometry...\n");
	Sys_PrintHeading("--- Loading geometry ---\n");
	int numVerts, numTris;

#if 0
	//count surfaces
	LoadBrushTris( verts, tris );
	LoadPatchTris( verts, tris );
#else
	int solidFlags = C_SOLID;

	for (int i = 0; i < numBSPDrawSurfaces; i++)
	{
		printLabelledProgress("LoadGeometry", i, numBSPDrawSurfaces);

		bspDrawSurface_t *surf = &bspDrawSurfaces[i];
		bspShader_t *shader = &bspShaders[surf->shaderNum];
		
		if (!(shader->contentFlags & solidFlags)) {
			continue;
		}

#ifdef __IGNORE_EXTRA_SURFACES__
		if (ignoreRock && StringContainsWord(shader->shader, "warzone/rock"))
		{
			continue;
		}

		if (ignoreTreeLeaves && StringContainsWord(shader->shader, "warzone/tree") && !isTreeSolid(shader->shader))
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
#endif //__IGNORE_EXTRA_SURFACES__

		extern float Distance(vec3_t pos1, vec3_t pos2);

		bspDrawVert_t *vs = &bspDrawVerts[surf->firstVert];
		int *idxs = &bspDrawIndexes[surf->firstIndex];

		for (int j = 0; j < surf->numIndexes; j+=3)
		{
			vec3_t iverts[3];

			VectorCopy(vs[idxs[j]].xyz, iverts[0]);
			VectorCopy(vs[idxs[j + 1]].xyz, iverts[1]);
			VectorCopy(vs[idxs[j + 2]].xyz, iverts[2]);

#ifdef __IGNORE_EXTRA_SURFACES__
			if (MAP_WATER_LEVEL > -999999.0)
			{
				if (iverts[0][2] <= MAP_WATER_LEVEL
					&& iverts[1][2] <= MAP_WATER_LEVEL
					&& iverts[2][2] <= MAP_WATER_LEVEL)
				{
					//Sys_Printf("Skipping triangle because it is below water level.\n");
					continue;
				}
			}
#endif //__IGNORE_EXTRA_SURFACES__

			if (NAVMESH_SCALE != 1.0)
			{
				iverts[0][0] *= NAVMESH_SCALE;
				iverts[0][1] *= NAVMESH_SCALE;
				iverts[0][2] *= NAVMESH_SCALE;

				iverts[1][0] *= NAVMESH_SCALE;
				iverts[1][1] *= NAVMESH_SCALE;
				iverts[1][2] *= NAVMESH_SCALE;

				iverts[2][0] *= NAVMESH_SCALE;
				iverts[2][1] *= NAVMESH_SCALE;
				iverts[2][2] *= NAVMESH_SCALE;
			}

			if (ignoreSmallTriangles)
			{
				float d1 = Distance(iverts[0], iverts[1]);
				float d2 = Distance(iverts[0], iverts[2]);
				float d3 = Distance(iverts[1], iverts[2]);

				float tringleSize = max(d1, max(d2, d3));

				if (tringleSize < 16.0)
				{
					continue;
				}
			}

			AddTri(verts, tris, iverts[0], iverts[1], iverts[2]);
		}
	}
	/*for (int i = 0; i < numBSPDrawIndexes; i += 3)
	{
		AddTri(verts, tris, bspDrawVerts[bspDrawIndexes[i]].xyz, bspDrawVerts[bspDrawIndexes[i + 1]].xyz, bspDrawVerts[bspDrawIndexes[i + 2]].xyz);
	}*/
#endif

	numTris = tris.size() / 3;
	numVerts = verts.size() / 3;

	Sys_Printf("Using %d triangles\n", numTris);
	Sys_Printf("Using %d vertices\n", numVerts);

	geo.init( &verts[ 0 ], numVerts, &tris[ 0 ], numTris );

	const float *mins = geo.getMins();
	const float *maxs = geo.getMaxs();

	Sys_Printf("Set recast world bounds to:\n");
	Sys_Printf(" min: %f %f %f\n", mins[0], mins[1], mins[2]);
	Sys_Printf(" max: %f %f %f\n", maxs[0], maxs[1], maxs[2]);
}

// Modified version of Recast's rcErodeWalkableArea that uses an AABB instead of a cylindrical radius
static bool rcErodeWalkableAreaByBox(rcContext* ctx, int boxRadius, rcCompactHeightfield& chf)
{
	rcAssert(ctx);

	const int w = chf.width;
	const int h = chf.height;

	ctx->startTimer(RC_TIMER_ERODE_AREA);

	unsigned char* dist = (unsigned char*)rcAlloc(sizeof(unsigned char)*chf.spanCount, RC_ALLOC_TEMP);
	if (!dist)
	{
		ctx->log(RC_LOG_ERROR, "erodeWalkableArea: Out of memory 'dist' (%d).", chf.spanCount);
		return false;
	}

	// Init distance.
	memset(dist, 0xff, sizeof(unsigned char)*chf.spanCount);

	// Mark boundary cells.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				if (chf.areas[i] == RC_NULL_AREA)
				{
					dist[i] = 0;
				}
				else
				{
					const rcCompactSpan& s = chf.spans[i];
					int nc = 0;
					for (int dir = 0; dir < 4; ++dir)
					{
						if (rcGetCon(s, dir) != RC_NOT_CONNECTED)
						{
							const int nx = x + rcGetDirOffsetX(dir);
							const int ny = y + rcGetDirOffsetY(dir);
							const int nidx = (int)chf.cells[nx+ny*w].index + rcGetCon(s, dir);
							if (chf.areas[nidx] != RC_NULL_AREA)
							{
								nc++;
							}
						}
					}
					// At least one missing neighbour.
					if (nc != 4)
						dist[i] = 0;
				}
			}
		}
	}

	unsigned char nd;

	// Pass 1
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];

				if (rcGetCon(s, 0) != RC_NOT_CONNECTED)
				{
					// (-1,0)
					const int ax = x + rcGetDirOffsetX(0);
					const int ay = y + rcGetDirOffsetY(0);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 0);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rcMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;

					// (-1,-1)
					if (rcGetCon(as, 3) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(3);
						const int aay = ay + rcGetDirOffsetY(3);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 3);
						nd = (unsigned char)rcMin((int)dist[aai]+2, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
				if (rcGetCon(s, 3) != RC_NOT_CONNECTED)
				{
					// (0,-1)
					const int ax = x + rcGetDirOffsetX(3);
					const int ay = y + rcGetDirOffsetY(3);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 3);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rcMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;

					// (1,-1)
					if (rcGetCon(as, 2) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(2);
						const int aay = ay + rcGetDirOffsetY(2);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 2);
						nd = (unsigned char)rcMin((int)dist[aai]+2, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
			}
		}
	}

	// Pass 2
	for (int y = h-1; y >= 0; --y)
	{
		for (int x = w-1; x >= 0; --x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];

				if (rcGetCon(s, 2) != RC_NOT_CONNECTED)
				{
					// (1,0)
					const int ax = x + rcGetDirOffsetX(2);
					const int ay = y + rcGetDirOffsetY(2);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 2);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rcMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;

					// (1,1)
					if (rcGetCon(as, 1) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(1);
						const int aay = ay + rcGetDirOffsetY(1);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 1);
						nd = (unsigned char)rcMin((int)dist[aai]+2, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
				if (rcGetCon(s, 1) != RC_NOT_CONNECTED)
				{
					// (0,1)
					const int ax = x + rcGetDirOffsetX(1);
					const int ay = y + rcGetDirOffsetY(1);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 1);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rcMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;

					// (-1,1)
					if (rcGetCon(as, 0) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(0);
						const int aay = ay + rcGetDirOffsetY(0);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 0);
						nd = (unsigned char)rcMin((int)dist[aai]+2, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
			}
		}
	}

	const unsigned char thr = (unsigned char)(boxRadius*2);
	for (int i = 0; i < chf.spanCount; ++i) {
		if (dist[i] < thr)
			chf.areas[i] = RC_NULL_AREA;
	}

	rcFree(dist);

	ctx->stopTimer(RC_TIMER_ERODE_AREA);

	return true;
}

/*
rcFilterGaps
Does a super simple sampling of ledges to detect and fix
any "gaps" in the heightfield that are narrow enough for us to walk over
because of our AABB based collision system
*/
static void rcFilterGaps(rcContext *ctx, int walkableRadius, int walkableClimb, int walkableHeight, rcHeightfield &solid) {
	const int h = solid.height;
	const int w = solid.width;
	const int MAX_HEIGHT = 0xffff;
	std::vector<int> spanData;
	spanData.reserve(500*3);
	std::vector<int> data;
	data.reserve((walkableRadius*2-1)*3);

	//for every span in the heightfield
	for(int y=0;y<h;++y) {
		for(int x=0;x<w;++x) {
			//check each span in the column
			for(rcSpan *s = solid.spans[x + y*w]; s;s = s->next) {
				//bottom and top of the "base" span
				const int sbot = s->smax;
				const int stop = (s->next) ? (s->next->smin) : MAX_HEIGHT;

				//if the span is walkable
				if(s->area != RC_NULL_AREA) {
					//check all neighbor connections
					for(int dir=0;dir<4;dir++) {
						const int dirx = rcGetDirOffsetX(dir);
						const int diry = rcGetDirOffsetY(dir);
						int dx = x;
						int dy = y;
						bool freeSpace = false;
						bool stop = false;

						if ( dx < 0 || dy < 0 || dx >= w || dy >= h )
						{
							continue;
						}

						//keep going the direction for walkableRadius * 2 - 1 spans
						//because we can walk as long as at least part of our bbox is on a solid surface
						for(int i=1;i<walkableRadius*2;i++) {
							dx = dx + dirx;
							dy = dy + diry;
							if(dx < 0 || dy < 0 || dx >= w || dy >= h)
							{
								i--;
								freeSpace = false;
								stop = false;
								break;
							}

							//tells if there is space here for us to stand
							freeSpace = false;

							//go through the column
							for(rcSpan *ns = solid.spans[dx + dy*w];ns;ns = ns->next) {
								int nsbot = ns->smax;
								int nstop = (ns->next) ? (ns->next->smin) : MAX_HEIGHT;

								//if there is a span within walkableClimb of the base span, we have reached the end of the gap (if any)
								if(rcAbs(sbot - nsbot) <= walkableClimb && ns->area != RC_NULL_AREA) {
									//set flag telling us to stop
									stop = true;

									//only add spans if we have gone for more than 1 iteration
									//if we stop at the first iteration, it means there was no gap to begin with
									if(i>1)
										freeSpace = true;
									break;
								}

								if(nsbot < sbot && nstop >= sbot + walkableHeight) {
									//tell that we found walkable space within reach of the previous span
									freeSpace = true;
									//add this span to a temporary storage location
									data.push_back(dx);
									data.push_back(dy);
									data.push_back(sbot);
									break;
								}
							}

							//stop if there is no more freespace, or we have reached end of gap
							if(stop || !freeSpace) {
								break;
							}
						}
						//move the spans from the temp location to the storage
						//we check freeSpace to make sure we don't add a
						//span when we stop at the first iteration (because there is no gap)
						//checking stop tells us if there was a span to complete the "bridge"
						if(freeSpace && stop) {
							const int N = data.size();
							for(int i=0;i<N;i++)
								spanData.push_back(data[i]);
						}
						data.clear();
					}
				}
			}
		}
	}

	//add the new spans
	//we cant do this in the loop, because the loop would then iterate over those added spans
	for(int i=0;i<spanData.size();i+=3) {
		rcAddSpan(ctx, solid, spanData[i], spanData[i+1], spanData[i+2]-1, spanData[i+2], RC_WALKABLE_AREA, walkableClimb);
	}
}

static int rasterizeTileLayers( rcContext &context, int tx, int ty, const rcConfig &mcfg, TileCacheData *data, int maxLayers )
{
	rcConfig cfg;

	FastLZCompressor comp;
	RasterizationContext rc;

	const float tcs = mcfg.tileSize * mcfg.cs;

	memcpy( &cfg, &mcfg, sizeof( cfg ) );

	// find tile bounds
	cfg.bmin[ 0 ] = mcfg.bmin[ 0 ] + tx * tcs;
	cfg.bmin[ 1 ] = mcfg.bmin[ 1 ];
	cfg.bmin[ 2 ] = mcfg.bmin[ 2 ] + ty * tcs;

	cfg.bmax[ 0 ] = mcfg.bmin[ 0 ] + ( tx + 1 ) * tcs;
	cfg.bmax[ 1 ] = mcfg.bmax[ 1 ];
	cfg.bmax[ 2 ] = mcfg.bmin[ 2 ] + ( ty + 1 ) * tcs;

	// expand bounds by border size
	cfg.bmin[ 0 ] -= cfg.borderSize * cfg.cs;
	cfg.bmin[ 2 ] -= cfg.borderSize * cfg.cs;

	cfg.bmax[ 0 ] += cfg.borderSize * cfg.cs;
	cfg.bmax[ 2 ] += cfg.borderSize * cfg.cs;

	rc.solid = rcAllocHeightfield();

	if ( !rcCreateHeightfield( &context, *rc.solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch ) )
	{
		Error ("Failed to create heightfield for navigation mesh.\n");
	}

	const float *verts = geo.getVerts();
	const int nverts = geo.getNumVerts();
	const rcChunkyTriMesh *chunkyMesh = geo.getChunkyMesh();

	rc.triareas = new unsigned char[ chunkyMesh->maxTrisPerChunk ];
	if ( !rc.triareas )
	{
		Error( "Out of memory rc.triareas\n" );
	}

	float tbmin[ 2 ], tbmax[ 2 ];

	tbmin[ 0 ] = cfg.bmin[ 0 ];
	tbmin[ 1 ] = cfg.bmin[ 2 ];
	tbmax[ 0 ] = cfg.bmax[ 0 ];
	tbmax[ 1 ] = cfg.bmax[ 2 ];

	int *cid = new int[ chunkyMesh->nnodes ];

	const int ncid = rcGetChunksOverlappingRect( chunkyMesh, tbmin, tbmax, cid, chunkyMesh->nnodes );
	if ( !ncid )
	{
		delete[] cid;
		return 0;
	}

	for ( int i = 0; i < ncid; i++ )
	{
		const rcChunkyTriMeshNode &node = chunkyMesh->nodes[ cid[ i ] ];
		const int *tris = &chunkyMesh->tris[ node.i * 3 ];
		const int ntris = node.n;

		memset( rc.triareas, 0, ntris * sizeof( unsigned char ) );

		rcMarkWalkableTriangles( &context, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, rc.triareas );
		rcRasterizeTriangles( &context, verts, nverts, tris, rc.triareas, ntris, *rc.solid, cfg.walkableClimb );
	}

	delete[] cid;

	rcFilterLowHangingWalkableObstacles( &context, cfg.walkableClimb, *rc.solid );

	//dont filter ledge spans since characters CAN walk on ledges due to using a bbox for movement collision
	//rcFilterLedgeSpans (&context, cfg.walkableHeight, cfg.walkableClimb, *rc.solid);

	rcFilterWalkableLowHeightSpans( &context, cfg.walkableHeight, *rc.solid );

	if(filterGaps)
	{
		rcFilterGaps( &context, cfg.walkableRadius, cfg.walkableClimb, cfg.walkableHeight, *rc.solid );
	}

	rc.chf = rcAllocCompactHeightfield();

	if ( !rcBuildCompactHeightfield( &context, cfg.walkableHeight, cfg.walkableClimb, *rc.solid, *rc.chf ) )
	{
		Error ("Failed to create compact heightfield for navigation mesh.\n");
	}

	if ( !rcErodeWalkableAreaByBox(&context, cfg.walkableRadius, *rc.chf) )
	{
		Error ("Unable to erode walkable surfaces.\n");
	}

	rc.lset = rcAllocHeightfieldLayerSet();

	if ( !rc.lset )
	{
		Error( "Out of memory heightfield layer set\n" );
	}

	if ( !rcBuildHeightfieldLayers( &context, *rc.chf, cfg.borderSize, cfg.walkableHeight, *rc.lset ) )
	{
		Error( "Could not build heightfield layers\n" );
	}

	rc.ntiles = 0;

	for ( int i = 0; i < rcMin( rc.lset->nlayers, MAX_LAYERS ); i++ )
	{
		TileCacheData *tile = &rc.tiles[ rc.ntiles++ ];
		const rcHeightfieldLayer *layer = &rc.lset->layers[ i ];

		dtTileCacheLayerHeader header;
		header.magic = DT_TILECACHE_MAGIC;
		header.version = DT_TILECACHE_VERSION;

		header.tx = tx;
		header.ty = ty;
		header.tlayer = i;
		dtVcopy( header.bmin, layer->bmin );
		dtVcopy( header.bmax, layer->bmax );

		header.width = ( unsigned char ) layer->width;
		header.height = ( unsigned char ) layer->height;
		header.minx = ( unsigned char ) layer->minx;
		header.maxx = ( unsigned char ) layer->maxx;
		header.miny = ( unsigned char ) layer->miny;
		header.maxy = ( unsigned char ) layer->maxy;
		header.hmin = ( unsigned short ) layer->hmin;
		header.hmax = ( unsigned short ) layer->hmax;

		dtStatus status = dtBuildTileCacheLayer( &comp, &header, layer->heights, layer->areas, layer->cons, &tile->data, &tile->dataSize );

		if ( dtStatusFailed( status ) )
		{
			return 0;
		}
	}

	// transfer tile data over to caller
	int n = 0;
	for ( int i = 0; i < rcMin( rc.ntiles, maxLayers ); i++ )
	{
		data[ n++ ] = rc.tiles[ i ];
		rc.tiles[ i ].data = 0;
		rc.tiles[ i ].dataSize = 0;
	}

	return n;
}

static void BuildNavMesh( int characterNum )
{
	Sys_PrintHeading("--- BuildNavMesh ---\n");

	const Character_nav &agent = navcharacters[ characterNum ];

	dtTileCache *tileCache;
	const float *bmin = geo.getMins();
	const float *bmax = geo.getMaxs();
	int gw = 0, gh = 0;

	rcConfig cfg;
	memset( &cfg, 0, sizeof ( cfg ) );

	gw = 0;
	gh = 0;

	float r = agent.radius * NAVMESH_ENTITY_SCALE;
	float h = agent.height * NAVMESH_ENTITY_SCALE;

	const float cellSize = r / 2.0;// r / 3.0;
	cellHeight = cellSize / cellheightDivisor;//3.0; //cellSize / 2.0; // dec if holes

	rcCalcGridSize(bmin, bmax, cellSize, &gw, &gh);

	tileSize = 64.0;// 32.0;// 96.0;// 1500.0;// 2048.0;// 512.0;// 64.0;// cellSize * 32.0;

	const int ts = tileSize;
	const int tw = (gw + ts - 1) / ts;
	const int th = (gh + ts - 1) / ts;

	cfg.cs = cellSize;
	cfg.ch = cellHeight;
	cfg.walkableSlopeAngle = 47.0;// RAD2DEG(acosf(MIN_WALK_NORMAL));
	cfg.walkableHeight = (int)ceilf(agent.height * NAVMESH_ENTITY_SCALE / cfg.ch);
	cfg.walkableClimb = (int)floorf(stepSize * NAVMESH_ENTITY_SCALE / cfg.ch);
	cfg.walkableRadius = (int)ceilf(agent.radius * NAVMESH_ENTITY_SCALE / cfg.cs);
	cfg.maxEdgeLen = cfg.walkableRadius * 8.0;
	cfg.maxSimplificationError = 1.3f;
	cfg.minRegionArea = rcSqr(25);
	cfg.mergeRegionArea = rcSqr(50);
	cfg.maxVertsPerPoly = 6;// 131072;// 6;
	cfg.tileSize = ts;
	cfg.borderSize = cfg.walkableRadius * 2;
	cfg.width = cfg.tileSize + cfg.borderSize * 2;
	cfg.height = cfg.tileSize + cfg.borderSize * 2;
	cfg.detailSampleDist = cfg.cs * 6.0f;
	cfg.detailSampleMaxError = cfg.ch * 1.0f;

	Sys_Printf("Cell size: %0.2f\n", cfg.cs);
	Sys_Printf("Cell height: %0.2f\n", cfg.ch);
	Sys_Printf("Walkable slope angle : %0.2f\n", cfg.walkableSlopeAngle);
	Sys_Printf("Walkable height: %d\n", cfg.walkableHeight);
	Sys_Printf("Walkable climb: %d\n", cfg.walkableClimb);
	Sys_Printf("Walkable radius: %d\n", cfg.walkableRadius);
	Sys_Printf("Max edge length : %d\n", cfg.maxEdgeLen);
	Sys_Printf("Tile size: %d\n", cfg.tileSize);
	Sys_Printf("Border size: %d\n", cfg.borderSize);
	Sys_Printf("Height: %d\n", cfg.height);
	Sys_Printf("Width: %d\n", cfg.width);
	Sys_Printf("Max edge length float: %0.2f\n", 1.0f / cfg.cs);

	rcVcopy( cfg.bmin, bmin );
	rcVcopy( cfg.bmax, bmax );

	dtTileCacheParams tcparams;
	memset( &tcparams, 0, sizeof( tcparams ) );
	rcVcopy( tcparams.orig, bmin );
	tcparams.cs = cellSize;
	tcparams.ch = cellHeight;
	tcparams.width = ts;
	tcparams.height = ts;
	tcparams.walkableHeight = agent.height * NAVMESH_ENTITY_SCALE;
	tcparams.walkableRadius = agent.radius * NAVMESH_ENTITY_SCALE;
	tcparams.walkableClimb = stepSize * NAVMESH_ENTITY_SCALE;
	tcparams.maxSimplificationError = 1.3;
	tcparams.maxTiles = tw * th * EXPECTED_LAYERS_PER_TILE;
	tcparams.maxObstacles = 256;

	tileCache = dtAllocTileCache();

	if ( !tileCache )
	{
		Error( "Could not allocate tile cache\n" );
	}

	LinearAllocator alloc( 32000 );
	FastLZCompressor comp;
	MeshProcess proc;

	dtStatus status = tileCache->init( &tcparams, &alloc, &comp, &proc );

	if ( dtStatusFailed( status ) )
	{
		if ( dtStatusDetail( status, DT_INVALID_PARAM ) )
		{
			Error( "Could not init tile cache: Invalid parameter\n" );
		}
		else
		{
			Error( "Could not init tile cache\n" );
		}
	}

	rcContext context( false );

	int currentCount = 0;
	int totalCount = th*tw;

#pragma omp parallel for schedule(dynamic) num_threads(numthreads)
	for ( int y = 0; y < th; y++ )
	{
		for ( int x = 0; x < tw; x++ )
		{
#pragma omp critical (BuildNavMesh)
			{
				printLabelledProgress("BuildNavMesh", currentCount, totalCount);
				currentCount++;
			}

			TileCacheData tiles[ MAX_LAYERS ];
			memset( tiles, 0, sizeof( tiles ) );

			int ntiles = rasterizeTileLayers( context, x, y, cfg, tiles, MAX_LAYERS );

			for ( int i = 0; i < ntiles; i++ )
			{
				TileCacheData *tile = &tiles[ i ];
				dtStatus this_status = tileCache->addTile( tile->data, tile->dataSize, DT_COMPRESSEDTILE_FREE_DATA, 0 );
				if ( dtStatusFailed(this_status) )
				{
					dtFree( tile->data );
					tile->data = 0;
					continue;
				}
			}
		}
	}

	// there are 22 bits to store a tile and its polys
	int tileBits = rcMin( ( int ) dtIlog2( dtNextPow2( tcparams.maxTiles ) ), 14 );
	int polyBits = 22 - tileBits;

	dtNavMeshParams params;
	dtVcopy( params.orig, tcparams.orig );
	params.tileHeight = ts * cfg.cs;
	params.tileWidth = ts * cfg.cs;
	params.maxTiles = 1 << tileBits;
	params.maxPolys = 1 << polyBits;

	WriteNavMeshFile( agent.name, tileCache, &params );
	dtFreeTileCache( tileCache );
}

/*
===========
NavMain
===========
*/
int NavMain(int argc, char **argv)
{
	float temp;
	int i;

	if(argc < 2)
	{
		Sys_Printf("Usage: wzmap -nav [-scale F] [-entityscale F] [-cellheightDivisor F] [-ignoreRock] [-ignoreTreeLeaves] [-ignoreSmallTriangles] [-cellheight F] [-mergeSizeMult F] [-stepsize F] [-includecaulk] [-includesky] [-nogapfilter] MAPNAME\n");
		return 0;
	}

	/* note it */
	Sys_PrintHeading("--- Nav ---\n");

	game = GetGame("ja");
	printf("* Game forced to %s (bspIdent %s - bspVersion %i).\n", game->arg, game->bspIdent, game->bspVersion);

	/* process arguments */
	for(i = 1; i < (argc - 1); i++)
	{
		if (!Q_stricmp(argv[i], "-cellheightDivisor")) {
			i++;
			if (i < (argc - 1)) {
				temp = atof(argv[i]);
				if (temp > 0) {
					cellheightDivisor = temp;
				}
			}
		} else if (!Q_stricmp(argv[i], "-mergeSizeMult")) {
			i++;
			if (i < (argc - 1)) {
				temp = atof(argv[i]);
				if (temp > 0) {
					mergeSizeMult = temp;
				}
			}
		} else if (!Q_stricmp(argv[i], "-stepsize")) {
			i++;
			if(i<(argc - 1)) {
				temp = atof(argv[i]);
				if(temp > 0) {
					stepSize = temp;
				}
			}
		}
		else if (!Q_stricmp(argv[i], "-scale")) {
			i++;
			if (i<(argc - 1)) {
				temp = atof(argv[i]);
				if (temp > 0) {
					NAVMESH_SCALE = temp;

					if (NAVMESH_SCALE > 1.0)
					{// Scale up the entity size when using small navmeshes, scaled up...
						NAVMESH_ENTITY_SCALE = NAVMESH_SCALE;
						//Sys_Printf("NAVMESH_ENTITY_SCALE %f.\n", NAVMESH_ENTITY_SCALE);
					}
				}
			}
		}
		else if (!Q_stricmp(argv[i], "-entityscale")) {
			i++;
			if (i<(argc - 1)) {
				temp = atof(argv[i]);
				if (temp > 0) {
					NAVMESH_ENTITY_SCALE = temp;
				}
			}
		}
		else if (!Q_stricmp(argv[i], "-ignoreSmallTriangles")) {
			ignoreSmallTriangles = qtrue;
		} else if (!Q_stricmp(argv[i], "-ignoreRock")) {
			ignoreRock = qtrue;
		} else if (!Q_stricmp(argv[i], "-ignoreTreeLeaves")) {
			ignoreTreeLeaves = qtrue;
		} else if(!Q_stricmp(argv[i], "-includecaulk")) {
			excludeCaulk = qfalse;
		} else if(!Q_stricmp(argv[i], "-includesky")) {
			excludeSky = qfalse;
		} else if(!Q_stricmp(argv[i], "-nogapfilter")) {
			filterGaps = qfalse;
		} else {
			Sys_Printf("WARNING: Unknown option \"%s\"\n", argv[i]);
		}
	}

	/* load the bsp */
	sprintf(source, "%s%s", inbase, ExpandArg(argv[i]));
	StripExtension(source);
	strcat(source, ".bsp");
	//LoadShaderInfo();

	Sys_Printf("Loading %s\n", source);
	
	LoadBSPFile(source);

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
	
	ParseEntities();

	LoadGeometry();

	/*float height = rcAbs(geo.getMaxs()[1]) + rcAbs(geo.getMins()[1]);
	if(height / cellHeight > RC_SPAN_MAX_HEIGHT) {
		Sys_Printf("WARNING: Map geometry is too tall for specified cell height. Increasing cell height to compensate. This may cause a less accurate navmesh.\n");
		float prevCellHeight = cellHeight;
		float minCellHeight = height / RC_SPAN_MAX_HEIGHT;

		int divisor = ( int ) stepSize;

		while ( divisor && cellHeight < minCellHeight )
		{
			cellHeight = stepSize / divisor;
			divisor--;
		}

		if ( !divisor )
		{
			Error( "ERROR: Map is too tall to generate a navigation mesh\n" );
		}

		Sys_Printf("Previous cellheight: %f\n", prevCellHeight);
		Sys_Printf("New cellheight: %f\n", cellHeight);
	}*/

	//RunThreadsOnIndividual( "Nav", sizeof( navcharacters ) / sizeof( navcharacters[ 0 ] ), qtrue, BuildNavMesh );
	for (i = 0; i < sizeof(navcharacters) / sizeof(navcharacters[0]); i++)
	{
		BuildNavMesh(i);
	}

	return 0;
}
