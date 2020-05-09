// UQ1: BSP file normals smoother

/* marker */
#define OPTIMIZE_C

/* dependencies */
#include "q3map2.h"

extern shaderInfo_t *bspShaderInfos[MAX_MAP_MODELS];

#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "glad/gl.h"
#include "GLFW/glfw3.h"

#pragma comment(lib, "../dependencies/x64/glfw3.lib")

#define LIGHTMAPPER_IMPLEMENTATION
#define LM_DEBUG_INTERPOLATION

#define LIGHTMAPPER_IMPLEMENTATION
#include "lightmapper.h"

#ifndef M_PI // even with _USE_MATH_DEFINES not always available
#define M_PI 3.14159265358979323846
#endif

typedef struct {
	float p[3];
	float t[2];
} vertex_t;

typedef struct
{
	GLuint program;
	GLint u_lightmap;
	GLint u_projection;
	GLint u_view;

	int w, h;

	unsigned int numMeshes = 0;

	GLuint lightmap[65536];
	float *lightmapData[65536];

	GLuint vao[65536], vbo[65536], ibo[65536];
	vertex_t *vertices[65536];
	unsigned short *indices[65536];
	unsigned int vertexCount[65536], indexCount[65536];
} scene_t;

static int initScene(scene_t *scene);
static void drawScene(scene_t *scene, float *view, float *projection);
static void destroyScene(scene_t *scene);

static int bake(scene_t *scene)
{
	lm_context *ctx = lmCreate(
		64,               // hemisphere resolution (power of two, max=512)
		1.0f/*0.001f*/, 1024.0f/*65536.0f*//*100.0f*/,   // zNear, zFar of hemisphere cameras
		1.0f, 1.0f, 1.0f, // background color (white for ambient occlusion)
		2, 0.01f,         // lightmap interpolation threshold (small differences are interpolated rather than sampled)
						  // check debug_interpolation.tga for an overview of sampled (red) vs interpolated (green) pixels.
		0.0f);            // modifier for camera-to-surface distance for hemisphere rendering.
						  // tweak this to trade-off between interpolated normals quality and other artifacts (see declaration).

	if (!ctx)
	{
		fprintf(stderr, "Error: Could not initialize lightmapper.\n");
		return 0;
	}

	int w = scene->w, h = scene->h;

	for (int m = 0; m < scene->numMeshes; m++)
	{
		Sys_Printf("Mesh %i/%i - %u triangles. %u vertexes.\n", m, scene->numMeshes, scene->indexCount[m], scene->vertexCount[m]);

		scene->lightmapData[m] = (float *)calloc(w * h * 4, sizeof(float));
		lmSetTargetLightmap(ctx, scene->lightmapData[m], w, h, 4);

		lmSetGeometry(ctx, NULL,                                                                 // no transformation in this example
			LM_FLOAT, (unsigned char*)scene->vertices[m] + offsetof(vertex_t, p), sizeof(vertex_t),
			LM_NONE, NULL, 0, // no interpolated normals in this example
			LM_FLOAT, (unsigned char*)scene->vertices[m] + offsetof(vertex_t, t), sizeof(vertex_t),
			scene->indexCount[m], LM_UNSIGNED_SHORT, scene->indices[m]);

		int vp[4];
		float view[16], projection[16];
		double lastUpdateTime = 0.0;
		while (lmBegin(ctx, vp, view, projection))
		{
			// render to lightmapper framebuffer
			glViewport(vp[0], vp[1], vp[2], vp[3]);
			drawScene(scene, view, projection);

			// display progress every second (printf is expensive)
			double time = glfwGetTime();
			if (time - lastUpdateTime > 1.0)
			{
				lastUpdateTime = time;
				printf("\r%6.2f%%", lmProgress(ctx) * 100.0f);
				fflush(stdout);
			}

			lmEnd(ctx);
		}

		printf("\rFinished baking %d triangles.\n", scene->indexCount[m] / 3);
	}

	lmDestroy(ctx);

	for (int m = 0; m < scene->numMeshes; m++)
	{
		// postprocess texture
		float *temp = (float *)calloc(w * h * 4, sizeof(float));
		for (int i = 0; i < 16; i++)
		{
			lmImageDilate(scene->lightmapData[m], temp, w, h, 4);
			lmImageDilate(temp, scene->lightmapData[m], w, h, 4);
		}
		lmImageSmooth(scene->lightmapData[m], temp, w, h, 4);
		lmImageDilate(temp, scene->lightmapData[m], w, h, 4);
		lmImagePower(scene->lightmapData[m], w, h, 4, 1.0f / 2.2f, 0x7); // gamma correct color channels
		free(temp);

		// save result to a file
		if (lmImageSaveTGAf(va("lightmap_%i.tga", m), scene->lightmapData[m], w, h, 4, 1.0f))
			printf("Saved lightmap_%i.tga\n", m);

		// upload result
		glBindTexture(GL_TEXTURE_2D, scene->lightmap[m]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, scene->lightmapData[m]);
		free(scene->lightmapData[m]);
	}

	return 1;
}

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void fpsCameraViewMatrix(GLFWwindow *window, float *view);
static void perspectiveMatrix(float *out, float fovy, float aspect, float zNear, float zFar);

static void mainLoop(GLFWwindow *window, scene_t *scene)
{
	glfwPollEvents();
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		bake(scene);

	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);

	// camera for glfw window
	float view[16], projection[16];
	fpsCameraViewMatrix(window, view);
	perspectiveMatrix(projection, 45.0f, (float)w / (float)h, 0.01f, 100.0f);

	// draw to screen with a blueish sky
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawScene(scene, view, projection);

	glfwSwapBuffers(window);
}


// helpers ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint loadProgram(const char *vp, const char *fp, const char **attributes, int attributeCount);

static int initScene(scene_t *scene)
{
	// load mesh
	int numCompleted = 0;

	Sys_PrintHeading("--- GPU Lightmapper ---\n");

	shaderInfo_t *caulkShader = ShaderInfoForShader("textures/system/caulk");
	shaderInfo_t *skipShader = ShaderInfoForShader("textures/system/skip");

	for (int s = 0; s < numBSPDrawSurfaces; s++)
	{
		{
			printLabelledProgress("SetShaderInfos", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *ds = &bspDrawSurfaces[s];
		shaderInfo_t *shaderInfo1 = ShaderInfoForShader(bspShaders[ds->shaderNum].shader);

		bspShaderInfos[s] = shaderInfo1;
	}

	numCompleted = 0;

	for (int m = 0; m < numBSPDrawSurfaces; m++)
	{
		{
			printLabelledProgress("GenerateMeshes", numCompleted, numBSPDrawSurfaces);
			numCompleted++;
		}

		bspDrawSurface_t *ds = &bspDrawSurfaces[m];
		shaderInfo_t *shaderInfo1 = bspShaderInfos[m];

		if (!shaderInfo1)
			continue;

		if (shaderInfo1 == caulkShader)
			continue;

		if (shaderInfo1 == skipShader)
			continue;

		scene->vertexCount[m] = ds->numVerts;
		scene->indexCount[m] = ds->numIndexes;

		scene->vertices[m] = (vertex_t *)calloc(scene->vertexCount[m], sizeof(vertex_t));
		scene->indices[m] = (unsigned short *)calloc(scene->indexCount[m], sizeof(unsigned short));

		//Sys_Printf("Mesh %i - %i verts %i inx.\n", s, ds->numVerts, ds->numIndexes);

		bspDrawVert_t *vs = &bspDrawVerts[ds->firstVert];
		int *idxs = &bspDrawIndexes[ds->firstIndex];

		for (int i = 0; i < ds->numIndexes; i += 3)
		{
			int tri[3];
			tri[0] = idxs[i];
			tri[1] = idxs[i + 1];
			tri[2] = idxs[i + 2];

			if (tri[0] >= ds->numVerts)
			{
				continue;
			}
			if (tri[1] >= ds->numVerts)
			{
				continue;
			}
			if (tri[2] >= ds->numVerts)
			{
				continue;
			}

			//Sys_Printf(" - tri %i %i %i.\n", tri[0], tri[1], tri[2]);

			scene->indices[m][i] = tri[0];
			scene->indices[m][i+1] = tri[1];
			scene->indices[m][i+2] = tri[2];

			scene->vertices[m][tri[0]].p[0] = vs[tri[0]].xyz[0];
			scene->vertices[m][tri[0]].p[1] = vs[tri[0]].xyz[1];
			scene->vertices[m][tri[0]].p[2] = vs[tri[0]].xyz[2];

			scene->vertices[m][tri[1]].p[0] = vs[tri[1]].xyz[0];
			scene->vertices[m][tri[1]].p[1] = vs[tri[1]].xyz[1];
			scene->vertices[m][tri[1]].p[2] = vs[tri[1]].xyz[2];

			scene->vertices[m][tri[2]].p[0] = vs[tri[2]].xyz[0];
			scene->vertices[m][tri[2]].p[1] = vs[tri[2]].xyz[1];
			scene->vertices[m][tri[2]].p[2] = vs[tri[2]].xyz[2];

			scene->vertices[m][tri[0]].t[0] = vs[tri[0]].st[0];
			scene->vertices[m][tri[0]].t[1] = vs[tri[0]].st[1];

			scene->vertices[m][tri[1]].t[0] = vs[tri[1]].st[0];
			scene->vertices[m][tri[1]].t[1] = vs[tri[1]].st[1];

			scene->vertices[m][tri[2]].t[0] = vs[tri[2]].st[0];
			scene->vertices[m][tri[2]].t[1] = vs[tri[2]].st[1];
		}


		glGenVertexArrays(1, &scene->vao[scene->numMeshes]);
		glBindVertexArray(scene->vao[scene->numMeshes]);

		glGenBuffers(1, &scene->vbo[scene->numMeshes]);
		glBindBuffer(GL_ARRAY_BUFFER, scene->vbo[scene->numMeshes]);
		glBufferData(GL_ARRAY_BUFFER, scene->vertexCount[scene->numMeshes] * sizeof(vertex_t), scene->vertices[scene->numMeshes], GL_STATIC_DRAW);

		glGenBuffers(1, &scene->ibo[scene->numMeshes]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->ibo[scene->numMeshes]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, scene->indexCount[scene->numMeshes] * sizeof(unsigned short), scene->indices[scene->numMeshes], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, p));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, t));

		// create lightmap texture
		scene->w = 654;
		scene->h = 654;
		glGenTextures(1, &scene->lightmap[scene->numMeshes]);
		glBindTexture(GL_TEXTURE_2D, scene->lightmap[scene->numMeshes]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		unsigned char emissive[] = { 0, 0, 0, 255 };
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, emissive);

		scene->numMeshes++;
	}

	// load shader
	const char *vp =
		"#version 150 core\n"
		"in vec3 a_position;\n"
		"in vec2 a_texcoord;\n"
		"uniform mat4 u_view;\n"
		"uniform mat4 u_projection;\n"
		"out vec2 v_texcoord;\n"

		"void main()\n"
		"{\n"
		"gl_Position = u_projection * (u_view * vec4(a_position, 1.0));\n"
		"v_texcoord = a_texcoord;\n"
		"}\n";

	const char *fp =
		"#version 150 core\n"
		"in vec2 v_texcoord;\n"
		"uniform sampler2D u_lightmap;\n"
		"out vec4 o_color;\n"

		"void main()\n"
		"{\n"
		"o_color = vec4(texture(u_lightmap, v_texcoord).rgb, gl_FrontFacing ? 1.0 : 0.0);\n"
		"}\n";

	const char *attribs[] =
	{
		"a_position",
		"a_texcoord"
	};

	scene->program = loadProgram(vp, fp, attribs, 2);
	if (!scene->program)
	{
		fprintf(stderr, "Error loading shader\n");
		return 0;
	}
	scene->u_view = glGetUniformLocation(scene->program, "u_view");
	scene->u_projection = glGetUniformLocation(scene->program, "u_projection");
	scene->u_lightmap = glGetUniformLocation(scene->program, "u_lightmap");

	return 1;
}

static void drawScene(scene_t *scene, float *view, float *projection)
{
	glEnable(GL_DEPTH_TEST);

	glUseProgram(scene->program);

	for (int m = 0; m < scene->numMeshes; m++)
	{
		glUniform1i(scene->u_lightmap, 0);
		glUniformMatrix4fv(scene->u_projection, 1, GL_FALSE, projection);
		glUniformMatrix4fv(scene->u_view, 1, GL_FALSE, view);

		glBindTexture(GL_TEXTURE_2D, scene->lightmap[m]);

		glBindVertexArray(scene->vao[m]);
		glDrawElements(GL_TRIANGLES, scene->indexCount[m], GL_UNSIGNED_SHORT, 0);
	}
}

static void destroyScene(scene_t *scene)
{
	for (int m = 0; m < scene->numMeshes; m++)
	{
		free(scene->vertices[m]);
		free(scene->indices[m]);
		glDeleteVertexArrays(1, &scene->vao[m]);
		glDeleteBuffers(1, &scene->vbo[m]);
		glDeleteBuffers(1, &scene->ibo[m]);
		glDeleteTextures(1, &scene->lightmap[m]);
	}

	glDeleteProgram(scene->program);
}

static GLuint loadShader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	if (shader == 0)
	{
		fprintf(stderr, "Could not create shader!\n");
		return 0;
	}
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		fprintf(stderr, "Could not compile shader!\n");
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen)
		{
			char* infoLog = (char*)malloc(infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "%s\n", infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}
static GLuint loadProgram(const char *vp, const char *fp, const char **attributes, int attributeCount)
{
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vp);
	if (!vertexShader)
		return 0;
	GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fp);
	if (!fragmentShader)
	{
		glDeleteShader(vertexShader);
		return 0;
	}

	GLuint program = glCreateProgram();
	if (program == 0)
	{
		fprintf(stderr, "Could not create program!\n");
		return 0;
	}
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	for (int i = 0; i < attributeCount; i++)
		glBindAttribLocation(program, i, attributes[i]);

	glLinkProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		fprintf(stderr, "Could not link program!\n");
		GLint infoLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen)
		{
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(program, infoLen, NULL, infoLog);
			fprintf(stderr, "%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

static void multiplyMatrices(float *out, float *a, float *b)
{
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
			out[y * 4 + x] = a[x] * b[y * 4] + a[4 + x] * b[y * 4 + 1] + a[8 + x] * b[y * 4 + 2] + a[12 + x] * b[y * 4 + 3];
}
static void translationMatrix(float *out, float x, float y, float z)
{
	out[0] = 1.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 0.0f;
	out[4] = 0.0f; out[5] = 1.0f; out[6] = 0.0f; out[7] = 0.0f;
	out[8] = 0.0f; out[9] = 0.0f; out[10] = 1.0f; out[11] = 0.0f;
	out[12] = x;    out[13] = y;    out[14] = z;    out[15] = 1.0f;
}
static void rotationMatrix(float *out, float angle, float x, float y, float z)
{
	angle *= (float)M_PI / 180.0f;
	float c = cosf(angle), s = sinf(angle), c2 = 1.0f - c;
	out[0] = x*x*c2 + c;   out[1] = y*x*c2 + z*s; out[2] = x*z*c2 - y*s; out[3] = 0.0f;
	out[4] = x*y*c2 - z*s; out[5] = y*y*c2 + c;   out[6] = y*z*c2 + x*s; out[7] = 0.0f;
	out[8] = x*z*c2 + y*s; out[9] = y*z*c2 - x*s; out[10] = z*z*c2 + c;   out[11] = 0.0f;
	out[12] = 0.0f;         out[13] = 0.0f;         out[14] = 0.0f;         out[15] = 1.0f;
}
static void transformPosition(float *out, float *m, float *p)
{
	float d = 1.0f / (m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15]);
	out[2] = d * (m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]);
	out[1] = d * (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]);
	out[0] = d * (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]);
}
static void transposeMatrix(float *out, float *m)
{
	out[0] = m[0]; out[1] = m[4]; out[2] = m[8]; out[3] = m[12];
	out[4] = m[1]; out[5] = m[5]; out[6] = m[9]; out[7] = m[13];
	out[8] = m[2]; out[9] = m[6]; out[10] = m[10]; out[11] = m[14];
	out[12] = m[3]; out[13] = m[7]; out[14] = m[11]; out[15] = m[15];
}
static void perspectiveMatrix(float *out, float fovy, float aspect, float zNear, float zFar)
{
	float f = 1.0f / tanf(fovy * (float)M_PI / 360.0f);
	float izFN = 1.0f / (zNear - zFar);
	out[0] = f / aspect; out[1] = 0.0f; out[2] = 0.0f;                       out[3] = 0.0f;
	out[4] = 0.0f;       out[5] = f;    out[6] = 0.0f;                       out[7] = 0.0f;
	out[8] = 0.0f;       out[9] = 0.0f; out[10] = (zFar + zNear) * izFN;      out[11] = -1.0f;
	out[12] = 0.0f;       out[13] = 0.0f; out[14] = 2.0f * zFar * zNear * izFN; out[15] = 0.0f;
}

static void fpsCameraViewMatrix(GLFWwindow *window, float *view)
{
	// initial camera config
	static float position[] = { 0.0f, 0.3f, 1.5f };
	static float rotation[] = { 0.0f, 0.0f };

	// mouse look
	static double lastMouse[] = { 0.0, 0.0 };
	double mouse[2];
	glfwGetCursorPos(window, &mouse[0], &mouse[1]);
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		rotation[0] += (float)(mouse[1] - lastMouse[1]) * -0.2f;
		rotation[1] += (float)(mouse[0] - lastMouse[0]) * -0.2f;
	}
	lastMouse[0] = mouse[0];
	lastMouse[1] = mouse[1];

	float rotationY[16], rotationX[16], rotationYX[16];
	rotationMatrix(rotationX, rotation[0], 1.0f, 0.0f, 0.0f);
	rotationMatrix(rotationY, rotation[1], 0.0f, 1.0f, 0.0f);
	multiplyMatrices(rotationYX, rotationY, rotationX);

	// keyboard movement (WSADEQ)
	float speed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 0.1f : 0.01f;
	float movement[3] = { 0 };
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement[2] -= speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement[2] += speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement[0] -= speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement[0] += speed;
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) movement[1] -= speed;
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) movement[1] += speed;

	float worldMovement[3];
	transformPosition(worldMovement, rotationYX, movement);
	position[0] += worldMovement[0];
	position[1] += worldMovement[1];
	position[2] += worldMovement[2];

	// construct view matrix
	float inverseRotation[16], inverseTranslation[16];
	transposeMatrix(inverseRotation, rotationYX);
	translationMatrix(inverseTranslation, -position[0], -position[1], -position[2]);
	multiplyMatrices(view, inverseRotation, inverseTranslation); // = inverse(translation(position) * rotationYX);
}


int LightmapGPUMain(int argc, char **argv)
{
	int i = 1;

	/* arg checking */
	if (argc < 1)
	{
		Sys_Printf("Usage: wzmap -lightmapGPU <mapname>\n");
		return 0;
	}

	/* process arguments */
	Sys_PrintHeading("--- CommandLine ---\n");
	strcpy(externalLightmapsPath, "");

	/* process arguments */
	for (i = 1; i < (argc - 1); i++)
	{
		if (!Q_stricmp(argv[i], "-wholemap")) 
		{
			
		}
		else 
		{
			Sys_Printf("WARNING: Unknown option \"%s\"\n", argv[i]);
		}
	}

	/* clean up map name */
	strcpy(source, ExpandArg(argv[i]));
	StripExtension(source);

	DefaultExtension(source, ".bsp");

	/* load shaders */
	LoadShaderInfo();

	/* load map */
	Sys_PrintHeading("--- LoadBSPFile ---\n");
	Sys_Printf("loading %s\n", source);

	/* load surface file */
	LoadBSPFile(source);

	
	//
	//
	//

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
	{
		fprintf(stderr, "Could not initialize GLFW.\n");
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_RED_BITS, 8);
	glfwWindowHint(GLFW_GREEN_BITS, 8);
	glfwWindowHint(GLFW_BLUE_BITS, 8);
	glfwWindowHint(GLFW_ALPHA_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 32);
	glfwWindowHint(GLFW_STENCIL_BITS, GLFW_DONT_CARE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 4);

	GLFWwindow *window = glfwCreateWindow(1024, 768, "Lightmapping Example", NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "Could not create window.\n");
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);
	gladLoadGL((GLADloadfunc)glfwGetProcAddress);
	glfwSwapInterval(1);

	scene_t scene;
	memset(&scene, 0, sizeof(scene));

	if (!initScene(&scene))
	{
		fprintf(stderr, "Could not initialize scene.\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	printf("Ambient Occlusion Baking Example.\n");
	printf("Use your mouse and the W, A, S, D, E, Q keys to navigate.\n");
	printf("Press SPACE to start baking one light bounce!\n");
	printf("This will take a few seconds and bake a lightmap illuminated by:\n");
	printf("1. The mesh itself (initially black)\n");
	printf("2. A white sky (1.0f, 1.0f, 1.0f)\n");

	while (!glfwWindowShouldClose(window))
	{
		mainLoop(window, &scene);
	}

	destroyScene(&scene);
	glfwDestroyWindow(window);
	glfwTerminate();


	//
	//
	//

	/* return to sender */
	return 0;
}
