//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include "Sample.h"
#include "InputGeom.h"
#include "Recast/Recast.h"
//#include "RecastDebugDraw.h"
//#include "DetourDebugDraw.h"
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"
#include "DetourCrowd/DetourCrowd.h"

#ifdef WIN32
#	define snprintf _snprintf
#endif

Sample::Sample() :
	m_geom(0),
	m_navMesh(0),
	m_navQuery(0),
	m_crowd(0),
	m_navMeshDrawFlags(0/*DU_DRAWNAVMESH_OFFMESHCONS|DU_DRAWNAVMESH_CLOSEDLIST*/),
	m_tool(0),
	m_ctx(0)
{
	resetCommonSettings();
	m_navQuery = dtAllocNavMeshQuery();
	m_crowd = dtAllocCrowd();

	for (int i = 0; i < MAX_TOOLS; i++)
		m_toolStates[i] = 0;
}

Sample::~Sample()
{
	dtFreeNavMeshQuery(m_navQuery);
	dtFreeNavMesh(m_navMesh);
	dtFreeCrowd(m_crowd);
	delete m_tool;
	for (int i = 0; i < MAX_TOOLS; i++)
		delete m_toolStates[i];
}

void Sample::setTool(SampleTool* tool)
{
	delete m_tool;
	m_tool = tool;
	if (tool)
		m_tool->init(this);
}

void Sample::handleSettings()
{
}

void Sample::handleTools()
{
}

void Sample::handleDebugMode()
{
}

void Sample::handleRender()
{
#if 0
	if (!m_geom)
		return;
	
	DebugDrawGL dd;
		
	// Draw mesh
	duDebugDrawTriMesh(&dd, m_geom->getMesh()->getVerts(), m_geom->getMesh()->getVertCount(),
					   m_geom->getMesh()->getTris(), m_geom->getMesh()->getNormals(), m_geom->getMesh()->getTriCount(), 0, 1.0f);
	// Draw bounds
	const float* bmin = m_geom->getMeshBoundsMin();
	const float* bmax = m_geom->getMeshBoundsMax();
	duDebugDrawBoxWire(&dd, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f);
#endif
}

void Sample::handleRenderOverlay(double* /*proj*/, double* /*model*/, int* /*view*/)
{
}

void Sample::handleMeshChanged(InputGeom* geom)
{
	m_geom = geom;

	const BuildSettings* buildSettings = geom->getBuildSettings();
	if (buildSettings)
	{
		m_cellSize = buildSettings->cellSize;
		m_cellHeight = buildSettings->cellHeight;
		m_agentHeight = buildSettings->agentHeight;
		m_agentRadius = buildSettings->agentRadius;
		m_agentMaxClimb = buildSettings->agentMaxClimb;
		m_agentMaxSlope = buildSettings->agentMaxSlope;
		m_regionMinSize = buildSettings->regionMinSize;
		m_regionMergeSize = buildSettings->regionMergeSize;
		m_edgeMaxLen = buildSettings->edgeMaxLen;
		m_edgeMaxError = buildSettings->edgeMaxError;
		m_vertsPerPoly = buildSettings->vertsPerPoly;
		m_detailSampleDist = buildSettings->detailSampleDist;
		m_detailSampleMaxError = buildSettings->detailSampleMaxError;
		m_partitionType = buildSettings->partitionType;
	}
}

void Sample::collectSettings(BuildSettings& settings)
{
	settings.cellSize = m_cellSize;
	settings.cellHeight = m_cellHeight;
	settings.agentHeight = m_agentHeight;
	settings.agentRadius = m_agentRadius;
	settings.agentMaxClimb = m_agentMaxClimb;
	settings.agentMaxSlope = m_agentMaxSlope;
	settings.regionMinSize = m_regionMinSize;
	settings.regionMergeSize = m_regionMergeSize;
	settings.edgeMaxLen = m_edgeMaxLen;
	settings.edgeMaxError = m_edgeMaxError;
	settings.vertsPerPoly = m_vertsPerPoly;
	settings.detailSampleDist = m_detailSampleDist;
	settings.detailSampleMaxError = m_detailSampleMaxError;
	settings.partitionType = m_partitionType;
}

#define Q3_SCALE 64.0

void Sample::resetCommonSettings()
{
	m_cellSize = 0.3f * Q3_SCALE;
	m_cellHeight = 0.2f * Q3_SCALE;
	m_agentHeight = 64.0;// 2.0f * Q3_SCALE;
	m_agentRadius = 48.0;// 0.6f * Q3_SCALE;
	m_agentMaxClimb = 18.0;// 0.9f;
	m_agentMaxSlope = 46.0;// 45.0f;
	m_regionMinSize = 8 * Q3_SCALE;
	m_regionMergeSize = 20 * Q3_SCALE;
	m_edgeMaxLen = 12.0f * Q3_SCALE;
	m_edgeMaxError = 1.3f;
	m_vertsPerPoly = 6.0f;
	m_detailSampleDist = 6.0f * Q3_SCALE;
	m_detailSampleMaxError = 1.0f;
	m_partitionType = SAMPLE_PARTITION_WATERSHED;
}

void Sample::handleCommonSettings()
{
#if 0
	imguiLabel("Rasterization");
	imguiSlider("Cell Size", &m_cellSize, 0.1f, 1.0f, 0.01f);
	imguiSlider("Cell Height", &m_cellHeight, 0.1f, 1.0f, 0.01f);
	
	if (m_geom)
	{
		const float* bmin = m_geom->getNavMeshBoundsMin();
		const float* bmax = m_geom->getNavMeshBoundsMax();
		int gw = 0, gh = 0;
		rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
		char text[64];
		snprintf(text, 64, "Voxels  %d x %d", gw, gh);
		imguiValue(text);
	}
	
	imguiSeparator();
	imguiLabel("Agent");
	imguiSlider("Height", &m_agentHeight, 0.1f, 5.0f, 0.1f);
	imguiSlider("Radius", &m_agentRadius, 0.0f, 5.0f, 0.1f);
	imguiSlider("Max Climb", &m_agentMaxClimb, 0.1f, 5.0f, 0.1f);
	imguiSlider("Max Slope", &m_agentMaxSlope, 0.0f, 90.0f, 1.0f);
	
	imguiSeparator();
	imguiLabel("Region");
	imguiSlider("Min Region Size", &m_regionMinSize, 0.0f, 150.0f, 1.0f);
	imguiSlider("Merged Region Size", &m_regionMergeSize, 0.0f, 150.0f, 1.0f);

	imguiSeparator();
	imguiLabel("Partitioning");
	if (imguiCheck("Watershed", m_partitionType == SAMPLE_PARTITION_WATERSHED))
		m_partitionType = SAMPLE_PARTITION_WATERSHED;
	if (imguiCheck("Monotone", m_partitionType == SAMPLE_PARTITION_MONOTONE))
		m_partitionType = SAMPLE_PARTITION_MONOTONE;
	if (imguiCheck("Layers", m_partitionType == SAMPLE_PARTITION_LAYERS))
		m_partitionType = SAMPLE_PARTITION_LAYERS;
	
	imguiSeparator();
	imguiLabel("Polygonization");
	imguiSlider("Max Edge Length", &m_edgeMaxLen, 0.0f, 50.0f, 1.0f);
	imguiSlider("Max Edge Error", &m_edgeMaxError, 0.1f, 3.0f, 0.1f);
	imguiSlider("Verts Per Poly", &m_vertsPerPoly, 3.0f, 12.0f, 1.0f);		

	imguiSeparator();
	imguiLabel("Detail Mesh");
	imguiSlider("Sample Distance", &m_detailSampleDist, 0.0f, 16.0f, 1.0f);
	imguiSlider("Max Sample Error", &m_detailSampleMaxError, 0.0f, 16.0f, 1.0f);
	
	imguiSeparator();
#endif
}

void Sample::handleClick(const float* s, const float* p, bool shift)
{
	if (m_tool)
		m_tool->handleClick(s, p, shift);
}

void Sample::handleToggle()
{
	if (m_tool)
		m_tool->handleToggle();
}

void Sample::handleStep()
{
	if (m_tool)
		m_tool->handleStep();
}

bool Sample::handleBuild()
{
	return true;
}

void Sample::handleUpdate(const float dt)
{
	if (m_tool)
		m_tool->handleUpdate(dt);
	updateToolStates(dt);
}


void Sample::updateToolStates(const float dt)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleUpdate(dt);
	}
}

void Sample::initToolStates(Sample* sample)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->init(sample);
	}
}

void Sample::resetToolStates()
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->reset();
	}
}

void Sample::renderToolStates()
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleRender();
	}
}

void Sample::renderOverlayToolStates(double* proj, double* model, int* view)
{
	for (int i = 0; i < MAX_TOOLS; i++)
	{
		if (m_toolStates[i])
			m_toolStates[i]->handleRenderOverlay(proj, model, view);
	}
}

