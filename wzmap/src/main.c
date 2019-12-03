/* -------------------------------------------------------------------------------

Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

-------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define MAIN_C



/* dependencies */
#include "q3map2.h"


/*
Random()
returns a pseudorandom number between 0 and 1
*/

vec_t Random( void )
{
	return (vec_t) rand() / RAND_MAX;
}



/*
ExitQ3Map()
cleanup routine
*/

static void ExitQ3Map( void )
{
	if( bspDrawVerts != NULL )
		free( bspDrawVerts );
	if( bspDrawSurfaces != NULL )
		free( bspDrawSurfaces );
	if( bspLightBytes != NULL )
		free( bspLightBytes );
	if( bspGridPoints != NULL )
		free( bspGridPoints );
	if( mapDrawSurfs != NULL )
		free( mapDrawSurfs );
}



/*
MD4BlockChecksum()
calculates an md4 checksum for a block of data
*/

static int MD4BlockChecksum(void *buffer, int length)
{
	int			digest[4], checksum;
	MD4_CTX		md4;

	/* run MD4 */
	MD4Init(&md4);
	MD4Update(&md4, (unsigned char *)buffer, length);
	MD4Final((unsigned char *)digest, &md4);
	
	/* xor the bits and return */
	checksum = digest[ 0 ] ^ digest[ 1 ] ^ digest[ 2 ] ^ digest[ 3 ];
	return checksum;
}

/*
FixAAS()
resets an aas checksum to match the given BSP
*/

int FixAAS( int argc, char **argv )
{
	int			length, checksum;
	void		*buffer;
	FILE		*file;
	char		aas[ MAX_OS_PATH ], **ext;
	char		*exts[] =
				{
					".aas",
					"_b0.aas",
					"_b1.aas",
					NULL
				};
	
	
	/* arg checking */
	if( argc < 2 )
	{
		Sys_Printf( "Usage: q3map -fixaas [-v] <mapname>\n" );
		return 0;
	}
	
	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	/* note it */
	Sys_PrintHeading ( "--- FixAAS ---\n" );
	
	/* load the bsp */
	Sys_PrintHeading ( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", source );
	length = LoadFile( source, &buffer );
	
	/* create bsp checksum */
	Sys_Printf( "Creating checksum...\n" );
	checksum = LittleLong( MD4BlockChecksum( buffer, length ) );
	
	/* write checksum to aas */
	ext = exts;
	while( *ext )
	{
		/* mangle name */
		strcpy( aas, source );
		StripExtension( aas );
		strcat( aas, *ext );
		Sys_Printf( "Trying %s\n", aas );
		ext++;
		
		/* fix it */
		file = fopen( aas, "r+b" );
		if( !file )
			continue;
		if( fwrite( &checksum, 4, 1, file ) != 1 )
			Error( "Error writing checksum to %s", aas );
		fclose( file );
	}
	
	/* return to sender */
	return 0;
}



/*
AnalyzeBSP() - ydnar
analyzes a Quake engine BSP file
*/

typedef struct abspHeader_s
{
	char			ident[ 4 ];
	int				version;
	
	bspLump_t		lumps[ 1 ];	/* unknown size */
}
abspHeader_t;

typedef struct abspLumpTest_s
{
	int				radix, minCount;
	char			*name;
}
abspLumpTest_t;

int AnalyzeBSP( int argc, char **argv )
{
	abspHeader_t			*header;
	int						size, i, version, offset, length, lumpInt, count;
	char					ident[ 5 ];
	void					*lump;
	float					lumpFloat;
	char					lumpString[ 1024 ], source[ 1024 ];
	qboolean				lumpSwap = qfalse;
	abspLumpTest_t			*lumpTest;
	static abspLumpTest_t	lumpTests[] =
							{
								{ sizeof( bspPlane_t ),			6,		"IBSP LUMP_PLANES" },
								{ sizeof( bspBrush_t ),			1,		"IBSP LUMP_BRUSHES" },
								{ 8,							6,		"IBSP LUMP_BRUSHSIDES" },
								{ sizeof( bspBrushSide_t ),		6,		"RBSP LUMP_BRUSHSIDES" },
								{ sizeof( bspModel_t ),			1,		"IBSP LUMP_MODELS" },
								{ sizeof( bspNode_t ),			2,		"IBSP LUMP_NODES" },
								{ sizeof( bspLeaf_t ),			1,		"IBSP LUMP_LEAFS" },
								{ 104,							3,		"IBSP LUMP_DRAWSURFS" },
								{ 44,							3,		"IBSP LUMP_DRAWVERTS" },
								{ 4,							6,		"IBSP LUMP_DRAWINDEXES" },
								{ 128 * 128 * 3,				1,		"IBSP LUMP_LIGHTMAPS" },
								{ 256 * 256 * 3,				1,		"IBSP LUMP_LIGHTMAPS (256 x 256)" },
								{ 512 * 512 * 3,				1,		"IBSP LUMP_LIGHTMAPS (512 x 512)" },
								{ 0, 0, NULL }
							};
	
	
	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	for( i = 1; i < (argc - 1) && argv[ i ]; i++ )
	{
		/* -format map|ase|... */
		if( !strcmp( argv[ i ],  "-lumpswap" ) )
		{
			Sys_Printf( " Swapped lump structs enabled\n" );
 			lumpSwap = qtrue;
 		}
	}
	
	/* clean up map name */
	Sys_PrintHeading ( "--- LoadBSPFile ---\n" );
	strcpy( source, ExpandArg( argv[ i ] ) );
	Sys_Printf( "loading %s\n", source );
	
	/* load the file */
	size = LoadFile( source, (void**) &header );
	if( size == 0 || header == NULL )
	{
		Sys_Printf( "Unable to load %s.\n", source );
		return -1;
	}
	
	/* analyze ident/version */
	memcpy( ident, header->ident, 4 );
	ident[ 4 ] = '\0';
	version = LittleLong( header->version );
	
	Sys_Printf( "Identity:      %s\n", ident );
	Sys_Printf( "Version:       %d\n", version );
	Sys_Printf( "---------------------------------------\n" );
	
	/* analyze each lump */
	for( i = 0; i < 100; i++ )
	{
		/* call of duty swapped lump pairs */
		if( lumpSwap )
		{
			offset = LittleLong( header->lumps[ i ].length );
			length = LittleLong( header->lumps[ i ].offset );
		}
		
		/* standard lump pairs */
		else
		{
			offset = LittleLong( header->lumps[ i ].offset );
			length = LittleLong( header->lumps[ i ].length );
		}
		
		/* extract data */
		lump = (byte*) header + offset;
		lumpInt = LittleLong( (int) *((int*) lump) );
		lumpFloat = LittleFloat( (float) *((float*) lump) );
		memcpy( lumpString, (char*) lump, (length < 1024 ? length : 1024) );
		lumpString[ 1023 ] = '\0';
		
		/* print basic lump info */
		Sys_Printf( "Lump:          %d\n", i );
		Sys_Printf( "Offset:        %d bytes\n", offset );
		Sys_Printf( "Length:        %d bytes\n", length );
		
		/* only operate on valid lumps */
		if( length > 0 )
		{
			/* print data in 4 formats */
			Sys_Printf( "As hex:        %08X\n", lumpInt );
			Sys_Printf( "As int:        %d\n", lumpInt );
			Sys_Printf( "As float:      %f\n", lumpFloat );
			Sys_Printf( "As string:     %s\n", lumpString );
			
			/* guess lump type */
			if( lumpString[ 0 ] == '{' && lumpString[ 2 ] == '"' )
				Sys_Printf( "Type guess:    IBSP LUMP_ENTITIES\n" );
			else if( strstr( lumpString, "textures/" ) )
				Sys_Printf( "Type guess:    IBSP LUMP_SHADERS\n" );
			else
			{
				/* guess based on size/count */
				for( lumpTest = lumpTests; lumpTest->radix > 0; lumpTest++ )
				{
					if( (length % lumpTest->radix) != 0 )
						continue;
					count = length / lumpTest->radix;
					if( count < lumpTest->minCount )
						continue;
					Sys_Printf( "Type guess:    %s (%d x %d)\n", lumpTest->name, count, lumpTest->radix );
				}
			}
		}
		
		Sys_Printf( "---------------------------------------\n" );
		
		/* end of file */
		if( offset + length >= size )
			break;
	}
	
	/* last stats */
	Sys_Printf( "Lump count:    %d\n", i + 1 );
	Sys_Printf( "File size:     %d bytes\n", size );
	
	/* return to caller */
	return 0;
}



/*
BSPInfo()
emits statistics about the bsp file
*/

int BSPInfo( int count, char **fileNames )
{
	int			i;
	char		source[ MAX_OS_PATH ], ext[ 64 ];
	int			size;
	FILE		*f;
	
	
	/* dummy check */
	if( count < 1 )
	{
		Sys_Printf( "No files to dump info for.\n");
		return -1;
	}
	
	/* enable info mode */
	infoMode = qtrue;
	
	/* walk file list */
	for( i = 0; i < count; i++ )
	{
		Sys_Printf( "---------------------------------\n" );
		
		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		ExtractFileExtension( source, ext );
		if( !Q_stricmp( ext, "map" ) )
			StripExtension( source );
		DefaultExtension( source, ".bsp" );
		f = fopen( source, "rb" );
		if( f )
		{
			size = Q_filelength (f);
			fclose( f );
		}
		else
			size = 0;
		
		/* load the bsp file and print lump sizes */
		Sys_Printf( "%s\n", source );
		LoadBSPFile( source );		
		PrintBSPFileSizes();
		
		/* print sizes */
		Sys_Printf( "\n" );
		Sys_Printf( "          total         %9d\n", size );
		Sys_Printf( "                        %9d KB\n", size / 1024 );
		Sys_Printf( "                        %9d MB\n", size / (1024 * 1024) );
		
		Sys_Printf( "---------------------------------\n" );
	}
	
	/* return count */
	return i;
}

/*
NavmeshBSPMain()
UQ1: amaze and confuse your enemies with wierd AI! :)
*/

#include <cstdio>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <stdio.h>
#include <cstring>

#include "Recast/Recast/Recast.h"
#include "Recast/InputGeom.h"
#include "Recast/NavMeshGenerate.h"
#include "Recast/NavMeshPathFind.h"

char finalNavmeshFilename[1024] = { 0 };

int NavmeshBSPMain(int argc, char **argv)
{
	int			i;
	float		f, scale;
	vec3_t		vec;
	char		str[MAX_OS_PATH];
	char		outNavmeshFilename[1024] = { 0 };


	/* arg checking */
	if (argc < 1)
	{
		Sys_Printf("Usage: q3map -navmesh [-v] <mapname>\n");
		return 0;
	}

	/* do some path mangling */
	strcpy(source, ExpandArg(argv[argc - 1]));
	StripExtension(source);
	DefaultExtension(source, ".bsp");

	strcpy(outNavmeshFilename, ExpandArg(argv[argc - 1]));
	StripExtension(outNavmeshFilename);
	DefaultExtension(outNavmeshFilename, ".navMesh");

	strcpy(finalNavmeshFilename, outNavmeshFilename);

	/* load the bsp */
	Sys_PrintHeading("--- LoadBSPFile ---\n");
	Sys_Printf("loading %s\n", source);
	LoadBSPFile(source);
	ParseEntities();

	/* note it */
	Sys_PrintHeading("--- Navmesh ---\n");
	Sys_Printf("%9d entities\n", numEntities);

	InputGeom* geom = 0;
	Sample* sample = 0;

	BuildContext ctx;
	
	geom = new InputGeom;
	sample = new Sample;

	sample->setContext(&ctx);
	sample->resetCommonSettings();
	sample->handleCommonSettings();
	
	if (!geom->convertBspDataToMesh())
	{
		delete geom;
		geom = 0;

		// Destroy the sample if it already had geometry loaded, as we've just deleted it!
		if (sample && sample->getInputGeom())
		{
			delete sample;
			sample = 0;
		}

		Sys_Printf("ERROR: Failed to convert world data.\n");
		return 0;
	}

	sample->handleSettings();
	sample->handleMeshChanged(geom);
	
	//const float* bmin = 0;
	//const float* bmax = 0;
	//bmin = geom->getNavMeshBoundsMin();
	//bmax = geom->getNavMeshBoundsMax();

	char text[64];
	snprintf(text, 64, "Verts: %.1fk  Tris: %.1fk\n",
		geom->getMesh()->getVertCount() / 1000.0f,
		geom->getMesh()->getTriCount() / 1000.0f);
	Sys_Printf("%s", text);

	sample->handleBuild();

	NavMeshGenerate *navMesh = new NavMeshGenerate();
	navMesh->setContext(&ctx);
	navMesh->resetCommonSettings();
	navMesh->handleCommonSettings();
	navMesh->handleMeshChanged(geom);
	navMesh->handleBuild();

	NavMeshPathfind *navPath = new NavMeshPathfind;
	navPath->init(sample);
	navPath->reset();

	/*
	TOOLMODE_PATHFIND_FOLLOW,
	TOOLMODE_PATHFIND_STRAIGHT,
	TOOLMODE_PATHFIND_SLICED,
	TOOLMODE_RAYCAST,
	TOOLMODE_DISTANCE_TO_WALL,
	TOOLMODE_FIND_POLYS_IN_CIRCLE,
	TOOLMODE_FIND_POLYS_IN_SHAPE,
	TOOLMODE_FIND_LOCAL_NEIGHBOURHOOD,
	*/
	
	/*
	navPath->handleAction(TOOLMODE_PATHFIND_STRAIGHT, DT_STRAIGHTPATH_ALL_CROSSINGS);
	*/
	{
		BuildSettings settings;
		memset(&settings, 0, sizeof(settings));

		rcVcopy(settings.navMeshBMin, geom->getNavMeshBoundsMin());
		rcVcopy(settings.navMeshBMax, geom->getNavMeshBoundsMax());

		settings.agentHeight = 64.0;
		settings.agentRadius = 48.0;
		settings.agentMaxClimb = 18.0;

		sample->collectSettings(settings);

		geom->saveGeomSet(&settings);
	}
	
	/*
	for (int i = 0; i < ctx.getLogCount(); i++)
	{
		Sys_Printf("%s", ctx.getLogText(i));
	}
	*/

	return 0;
}

/*
ScaleBSPMain()
amaze and confuse your enemies with wierd scaled maps!
*/

int ScaleBSPMain( int argc, char **argv )
{
	int			i;
	float		f, scale;
	vec3_t		vec;
	char		str[ MAX_OS_PATH ];
	
	
	/* arg checking */
	if( argc < 2 )
	{
		Sys_Printf( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* get scale */
	scale = atof( argv[ argc - 2 ] );
	if( scale == 0.0f )
	{
		Sys_Printf( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		Sys_Printf( "Non-zero scale value required.\n" );
		return 0;
	}
	
	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	/* load the bsp */
	Sys_PrintHeading ( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();
	
	/* note it */
	Sys_PrintHeading ( "--- ScaleBSP ---\n" );
	Sys_Printf( "%9d entities\n", numEntities );
	
	/* scale entity keys */
	for( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* scale origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if( (vec[ 0 ] + vec[ 1 ] + vec[ 2 ]) )
		{
			VectorScale( vec, scale, vec );
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}
		
		/* scale door lip */
		f = FloatForKey( &entities[ i ], "lip" );
		if( f )
		{
			f *= scale;
			sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "lip", str );
		}

		/* UQ1: scale modelScale's ffs */
		f = FloatForKey(&entities[i], "modelscale");
		if (f)
		{
			f *= scale;
			sprintf(str, "%f", f);
			SetKeyValue(&entities[i], "modelscale", str);
		}

		GetVectorForKey(&entities[i], "modelscale_vec", vec);
		if ((vec[0] + vec[1] + vec[2]))
		{
			VectorScale(vec, scale, vec);
			sprintf(str, "%f %f %f", vec[0], vec[1], vec[2]);
			SetKeyValue(&entities[i], "modelscale_vec", str);
		}
	}
	
	/* scale models */
	for( i = 0; i < numBSPModels; i++ )
	{
		VectorScale( bspModels[ i ].mins, scale, bspModels[ i ].mins );
		VectorScale( bspModels[ i ].maxs, scale, bspModels[ i ].maxs );
	}
	
	/* scale nodes */
	for( i = 0; i < numBSPNodes; i++ )
	{
		VectorScale( bspNodes[ i ].mins, scale, bspNodes[ i ].mins );
		VectorScale( bspNodes[ i ].maxs, scale, bspNodes[ i ].maxs );
	}
	
	/* scale leafs */
	for( i = 0; i < numBSPLeafs; i++ )
	{
		VectorScale( bspLeafs[ i ].mins, scale, bspLeafs[ i ].mins );
		VectorScale( bspLeafs[ i ].maxs, scale, bspLeafs[ i ].maxs );
	}
	
	/* scale drawverts */
	for( i = 0; i < numBSPDrawVerts; i++ )
		VectorScale( bspDrawVerts[ i ].xyz, scale, bspDrawVerts[ i ].xyz );
	
	/* scale planes */
	for( i = 0; i < numBSPPlanes; i++ )
		bspPlanes[ i ].dist *= scale;
	
	/* scale gridsize */
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if( (vec[ 0 ] + vec[ 1 ] + vec[ 2 ]) == 0.0f )
		VectorCopy( gridSize, vec );
	VectorScale( vec, scale, vec );
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );
	
	/* write the bsp */
	Sys_PrintHeading ( "--- WriteBSPFile ---\n" );
	UnparseEntities(qfalse);
	StripExtension( source );
	DefaultExtension( source, "_s.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );
	
	/* return to sender */
	return 0;
}



/*
ConvertBSPMain()
main argument processing function for bsp conversion
*/

int ConvertBSPMain( int argc, char **argv )
{
	int		i;
	int		(*convertFunc)( char *, int );
	game_t	*convertGame;
	int		convertCollapseByTexture;

	/* note it */
	Sys_PrintHeading ( "--- ConvertBSP ---\n" );
	
	/* set default */
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	convertCollapseByTexture = 0;
	
	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	Sys_PrintHeading ( "--- CommandLine ---\n" );
	for( i = 1; i < (argc - 1) && argv[ i ]; i++ )
	{
		/* -entitysaveid */
		if( !strcmp( argv[ i ],  "-entitysaveid") )
		{
			Sys_Printf( "Entity unique savegame identifiers enabled\n" );
			useEntitySaveId = qtrue;
		}
		/* -collapsebytexture */
		else if( !strcmp( argv[ i ],  "-collapsebytexture" ) )
		{
			convertCollapseByTexture = 1;
			Sys_Printf( "Collapsing by shader name\n" );
		}
		/* -format map|ase|... */
		else if( !strcmp( argv[ i ],  "-format" ) )
 		{
			i++;
			if( !Q_stricmp( argv[ i ], "ase" ) )
				convertFunc = ConvertBSPToASE;
			else if( !Q_stricmp( argv[ i ], "map" ) )
				convertFunc = ConvertBSPToMap;
			else
			{
				convertGame = GetGame( argv[ i ] );
				if( convertGame == NULL )
					Sys_Printf( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
			}
 		}
	}
	
	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	LoadShaderInfo();
	
	Sys_PrintHeading ( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", source );
	
	/* ydnar: load surface file */
	//%	LoadSurfaceExtraFile( source );
	
	LoadBSPFile( source );
	
	/* parse bsp entities */
	ParseEntities();
	
	/* bsp format convert? */
	if( convertGame != NULL )
	{
		/* set global game */
		game = convertGame;
		
		/* write bsp */
		Sys_PrintHeading ( "--- WriteBSPFile ---\n" );
		StripExtension( source );
		DefaultExtension( source, "_c.bsp" );
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );
		
		/* return to sender */
		return 0;
	}
	
	/* normal convert */
	return convertFunc( source, convertCollapseByTexture );
}



/*
main()
q3map mojo...
*/

float subdivisionMult;				/* default face subdivisions multipier */

extern int NavMain(int argc, char **argv);
extern int GenFoliageMain(int argc, char **argv);

int main(int argc, char **argv)
{
	int	i, r;
	double start, end;

	/* we want consistent 'randomness' */
	srand(0);

	/* start timer */
	start = I_FloatTime();

	/* set exit call */
	atexit(ExitQ3Map);

	gpu = qfalse;

	subdivisionMult = 1.0;

	/* read general options first */
	for (i = 1; i < argc && argv[i]; i++)
	{
		/* -buildconfig */
		if (!strcmp(argv[i], "-buildconfig"))
		{
			useBuildCfg = qtrue;
			argv[i] = NULL;
		}

		/* -custinfoparms */
		else if (!strcmp(argv[i], "-custinfoparms"))
		{
			useCustomInfoParms = qtrue;
			argv[i] = NULL;
		}

		/* -connect */
		else if (!strcmp(argv[i], "-connect"))
		{
			argv[i] = NULL;
			i++;
			Broadcast_Setup(argv[i]);
			argv[i] = NULL;
		}

		/* verbose */
		else if (!strcmp(argv[i], "-v"))
		{
			verbose = qtrue;
			argv[i] = NULL;
		}

		/* force */
		else if (!strcmp(argv[i], "-force"))
		{
			force = qtrue;
			argv[i] = NULL;
		}

		/* patch subdivisions */
		else if (!strcmp(argv[i], "-subdivisions"))
		{
			argv[i] = NULL;
			i++;
			patchSubdivisions = atof(argv[i]);
			argv[i] = NULL;
			if (patchSubdivisions <= 0.0)
				patchSubdivisions = 1.0;
		}

		else if (!strcmp(argv[i], "-subdivisionMult"))
		{
			argv[i] = NULL;
			i++;
			subdivisionMult = atof(argv[i]);
			argv[i] = NULL;
			if (subdivisionMult <= 0.0)
				subdivisionMult = 1.0;
		}

		/* threads */
		else if (!strcmp(argv[i], "-threads"))
		{
			argv[i] = NULL;
			i++;
			numthreads = atoi(argv[i]);
			argv[i] = NULL;
		}

		else if (!strcmp(argv[i], "-gpu")) {
			//gpu = qtrue;

			argv[i] = NULL;
		}

		/* memlog (write a memlog.txt) */
		else if (!strcmp(argv[i], "-memlog"))
		{
			memlog = qtrue;
			safe_malloc_logstart();
			argv[i] = NULL;
		}

		/* vortex: fake launch to make Radiant monitor happy */
		else if (!strcmp(argv[i], "-fake"))
		{
			/* shut down connection */
			Broadcast_Shutdown();
			return 0;
		}
	}

	/* init model library */
	PicoInit();
	PicoSetMallocFunc(safe_malloc_f);
	PicoSetFreeFunc(free);
	PicoSetPrintFunc(PicoPrintFunc);
	PicoSetLoadFileFunc(PicoLoadFileFunc);
	PicoSetFreeFileFunc(free);

	/* set number of threads */
	ThreadSetDefault();

	/* generate sinusoid jitter table */
	for (i = 0; i < MAX_JITTERS; i++)
	{
		jitters[i] = sin(i * 139.54152147);
		//%	Sys_Printf( "Jitter %4d: %f\n", i, jitters[ i ] );
	}


	bspLeafs = (bspLeaf_t*)malloc(sizeof(bspLeaf_t)*MAX_MAP_LEAFS);
	bspPlanes = (bspPlane_t*)malloc(sizeof(bspPlane_t)*MAX_MAP_PLANES);
	bspBrushes = (bspBrush_t*)malloc(sizeof(bspBrush_t)*MAX_MAP_BRUSHES);
	bspBrushSides = (bspBrushSide_t*)malloc(sizeof(bspBrushSide_t)*MAX_MAP_BRUSHSIDES);
	bspNodes = (bspNode_t*)malloc(sizeof(bspNode_t)*MAX_MAP_NODES);
	bspLeafSurfaces = (int*)malloc(sizeof(int)*MAX_MAP_LEAFFACES);
	bspLeafBrushes = (int*)malloc(sizeof(int)*MAX_MAP_LEAFBRUSHES);
	//bspVisBytes = (byte*)malloc(sizeof(byte)*MAX_MAP_VISIBILITY);

	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */

	setcolor(blue, black);
	Sys_Printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	setcolor(magenta, black);
	Sys_Printf(" Q3Map         - v1.0r (c) 1999 Id Software Inc.\n");
	Sys_Printf(" Q3Map (ydnar) - v2.5.16 FS20g\n");
	Sys_Printf(" GtkRadiant    - v1.5.0\n");
	Sys_Printf(" BloodMap      - v1.5.3 - (c) Pavel [VorteX] Timofeyev\n");
	setcolor(yellow, black);
	Sys_Printf(" WzMap         - v" WZMAP_VERSION " - " __DATE__ " " __TIME__ " - by UniqueOne\n");
	setcolor(white, black);
	Sys_Printf(" %s\n", WZMAP_MOTD);
	setcolor(red, black);
	if (useBuildCfg)
		Sys_Printf(" Enabled usage of .build config file\n");
	if (useCustomInfoParms)
		Sys_Printf(" Custom info parms enabled\n");
#if defined(__HIGH_MEMORY__)
	Sys_Printf(" High memory enabled (only for computers with *more* than 16 GB of RAM!!!).\n");
#endif
#if MAX_LIGHTMAPS == 1
	Sys_Printf(" Light styles are disabled in this build\n");
#endif
	setcolor(cyan, black);
	ThreadStats();
	setcolor(blue, black);
	Sys_Printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

	setcolor(gray, black);

	Sys_PrintHeading("--- Memory Usage ---\n");
	Sys_Printf("mapplanes: %i MB.\n", sizeof(plane_t)*MAX_MAP_PLANES / (1024 * 1024));
	Sys_Printf("bspLeafs: %i MB.\n", sizeof(bspLeaf_t)*MAX_MAP_LEAFS / (1024 * 1024));
	Sys_Printf("bspPlanes: %i MB.\n", sizeof(bspPlane_t)*MAX_MAP_PLANES / (1024 * 1024));
	Sys_Printf("bspBrushes: %i MB.\n", sizeof(bspBrush_t)*MAX_MAP_BRUSHES / (1024 * 1024));
	Sys_Printf("bspBrushSides: %i MB.\n", sizeof(bspBrushSide_t)*MAX_MAP_BRUSHSIDES / (1024 * 1024));
	Sys_Printf("bspNodes: %i MB.\n", sizeof(bspNode_t)*MAX_MAP_NODES / (1024 * 1024));
	Sys_Printf("bspLeafSurfaces: %i MB.\n", sizeof(int)*MAX_MAP_LEAFFACES / (1024 * 1024));
	Sys_Printf("bspLeafBrushes: %i MB.\n", sizeof(int)*MAX_MAP_LEAFBRUSHES / (1024 * 1024));
	Sys_Printf("bspVisBytes: %i MB.\n", sizeof(byte)*MAX_MAP_VISIBILITY / (1024 * 1024));

	/* ydnar: new path initialization */
	InitPaths(&argc, argv);

	/* set game options */
	if (patchSubdivisions <= 0.0)
		patchSubdivisions = game->patchSubdivisions;

	/* check if we have enough options left to attempt something */
	if (argc < 2)
	{
		setcolor(white, black);
		Error("Usage: %s [general options] [options] mapfile", argv[0]);
	}

#ifdef __USE_OPENCL__
	/* Delayed warning message for enabling gpu usage so program information can be sent to console beforehand */
	if (gpu) {
		Sys_Printf("\nWARNING: GPU will now attempt to use your videocard to accelerate compilation. This is highly experimental and can break!\n\n");

		/*Identify ocl platforms and devices then set up the context and command queue
		  for compilation and execution of the ocl kernels*/
		if (!InitOpenCL()) {
			gpu = qfalse; /*fall back to cpu if opencl fails to start for some reason*/
		}
	}
#endif //__USE_OPENCL__

	/* fixaas */
	if (!strcmp(argv[1], "-fixaas"))
		r = FixAAS(argc - 1, argv + 1);

	/* analyze */
	else if (!strcmp(argv[1], "-analyze"))
		r = AnalyzeBSP(argc - 1, argv + 1);

	/* info */
	else if (!strcmp(argv[1], "-info"))
		r = BSPInfo(argc - 2, argv + 2);

	/* vis */
	else if (!strcmp(argv[1], "-vis"))
	{
		//portals = (vportal_t*)malloc(sizeof(vportal_t)*MAX_PORTALS_ON_LEAF*2);
		//sorted_portals = (vportal_t*)malloc(sizeof(vportal_t*)*MAX_MAP_PORTALS * 2);
		//sorted_portals = (vportal_t*)malloc(sizeof(vportal_t*)*MAX_MAP_PORTALS * 2);
		//memset(sorted_portals, 0, (sizeof(vportal_t*)*MAX_MAP_PORTALS * 2));
		r = VisMain(argc - 1, argv + 1);
	}

	/* light */
	else if (!strcmp(argv[1], "-light"))
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		//portals = (vportal_t*)malloc(sizeof(vportal_t)*MAX_PORTALS_ON_LEAF);
		//*sorted_portals = (vportal_t*)malloc(sizeof(vportal_t*)*MAX_MAP_PORTALS * 2);
		r = LightMain(argc - 1, argv + 1);
	}

	/* vlight */
	else if (!strcmp(argv[1], "-vlight"))
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		//portals = (vportal_t*)malloc(sizeof(vportal_t)*MAX_PORTALS_ON_LEAF);
		//*sorted_portals = (vportal_t*)malloc(sizeof(vportal_t*)*MAX_MAP_PORTALS * 2);
		Sys_Warning("VLight is no longer supported, defaulting to -light -fast instead");
		argv[1] = "-fast";	/* eek a hack */
		r = LightMain(argc, argv);
	}

	/* syphter: Navigation Mesh Generation */
	else if (!strcmp(argv[1], "-nav"))
	{
		r = NavMain(argc - 1, argv + 1);
	}
	/* syphter: NavMesh Help */
	else if (!strcmp(argv[1], "-navhelp"))
	{
		Sys_Printf("Usage: wzmap -nav [-cellheight F] [-cellSizeMult F] [-stepsize F] [-includecaulk] [-includesky] [-nogapfilter] MAPNAME\n");
		exit(2);
	}

	/* UQ1: Generate warzone foliage points... */
	else if (!strcmp(argv[1], "-genfoliage"))
	{
		r = GenFoliageMain(argc - 1, argv + 1);
	}

	/* ydnar: lightmap export */
	else if (!strcmp(argv[1], "-export"))
		r = ExportLightmapsMain(argc - 1, argv + 1);

	/* ydnar: lightmap import */
	else if (!strcmp(argv[1], "-import"))
		r = ImportLightmapsMain(argc - 1, argv + 1);

	/* ydnar: bsp scaling */
	else if (!strcmp(argv[1], "-scale"))
		r = ScaleBSPMain(argc - 1, argv + 1);

#if 0
	/* UQ1: Navmesh Generation */
	else if (!strcmp(argv[1], "-navmesh"))
		r = NavmeshBSPMain(argc - 1, argv + 1);
#endif

	/* ydnar: bsp conversion */
	else if (!strcmp(argv[1], "-convert"))
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		r = ConvertBSPMain(argc - 1, argv + 1);
	}

	/* vortex: bsp optimisation */
	else if( !strcmp( argv[ 1 ], "-optimize" ) )
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		//portals = (vportal_t*)malloc(sizeof(vportal_t)*MAX_PORTALS_ON_LEAF);
		//*sorted_portals = (vportal_t*)malloc(sizeof(vportal_t*)*MAX_MAP_PORTALS * 2);
		r = OptimizeBSPMain( argc - 1, argv + 1 );
	}

	/* UQ1: bsp normals smoothing */
	else if (!strcmp(argv[1], "-smooth"))
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		r = SmoothBSPMain(argc - 1, argv + 1);
	}

	/* UQ1: bsp GPU lightmapping */
	else if (!strcmp(argv[1], "-lightmapGPU"))
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		r = LightmapGPUMain(argc - 1, argv + 1);
	}

	/* vortex: entity compile */
	else if( !strcmp( argv[ 1 ], "-patch" ) )
		r = PatchBSPMain( argc - 1, argv + 1 );
	
	/* ydnar: otherwise create a bsp */
	else
	{
		mapplanes = (plane_t*)malloc(sizeof(plane_t)*MAX_MAP_PLANES);
		r = BSPMain( argc, argv );
	}

	/* emit time */
	end = I_FloatTime();
	Sys_Printf( "%9.0f seconds elapsed\n", end - start );

	/* end memlog */
	if (memlog)
		safe_malloc_logend();

	/*Clean up after OpenCL*/ 
#ifdef __USE_OPENCL__
	if(gpu){
	    CleanOpenCL();
	}
#endif //__USE_OPENCL__

	if (mapplanes) free(mapplanes);
	if (bspLeafs) free(bspLeafs);
	if (bspPlanes) free(bspPlanes);
	if (bspBrushes) free(bspBrushes);
	if (bspBrushSides) free(bspBrushSides);
	if (bspNodes) free(bspNodes);
	//if (bspVisBytes) free(bspVisBytes);
	if (bspLeafSurfaces) free(bspLeafSurfaces);
	if (bspLeafBrushes) free(bspLeafBrushes);
	//if (portals) free(portals);
	//if (*sorted_portals) free(*sorted_portals);

	/* shut down connection */
	Broadcast_Shutdown();
	
	/* return any error code */
	return r;
}
