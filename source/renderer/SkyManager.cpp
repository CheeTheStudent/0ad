/* Copyright (C) 2021 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Sky settings, texture management and rendering.
 */

#include "precompiled.h"

#include "renderer/SkyManager.h"

#include "graphics/LightEnv.h"
#include "graphics/ShaderManager.h"
#include "graphics/Terrain.h"
#include "graphics/TextureManager.h"
#include "lib/tex/tex.h"
#include "lib/timer.h"
#include "lib/res/graphics/ogl_tex.h"
#include "maths/MathUtil.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/CStr.h"
#include "ps/Filesystem.h"
#include "ps/Game.h"
#include "ps/Loader.h"
#include "ps/World.h"
#include "renderer/Renderer.h"
#include "renderer/RenderingOptions.h"

#include <algorithm>

SkyManager::SkyManager()
	: m_RenderSky(true), m_SkyCubeMap(0)
{
	CFG_GET_VAL("showsky", m_RenderSky);
}

///////////////////////////////////////////////////////////////////
// Load all sky textures
void SkyManager::LoadSkyTextures()
{
	static const CStrW images[NUMBER_OF_TEXTURES + 1] = {
		L"front",
		L"back",
		L"right",
		L"left",
		L"top",
		L"top"
	};
	/*for (size_t i = 0; i < ARRAY_SIZE(m_SkyTexture); ++i)
	{
		VfsPath path = VfsPath("art/textures/skies") / m_SkySet / (Path::String(s_imageNames[i])+L".dds");

		CTextureProperties textureProps(path);
		textureProps.SetWrap(GL_CLAMP_TO_EDGE);
		CTexturePtr texture = g_Renderer.GetTextureManager().CreateTexture(textureProps);
		texture->Prefetch();
		m_SkyTexture[i] = texture;
	}*/

	///////////////////////////////////////////////////////////////////////////
	// HACK: THE HORRIBLENESS HERE IS OVER 9000. The following code is a HUGE hack and will be removed completely
	// as soon as all the hardcoded GL_TEXTURE_2D references are corrected in the TextureManager/OGL/tex libs.

	glGenTextures(1, &m_SkyCubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_SkyCubeMap);

	static const int types[] = {
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
	};

	for (size_t i = 0; i < NUMBER_OF_TEXTURES + 1; ++i)
	{
		VfsPath path = VfsPath("art/textures/skies") / m_SkySet / (Path::String(images[i])+L".dds");

		shared_ptr<u8> file;
		size_t fileSize;
		if (g_VFS->LoadFile(path, file, fileSize) != INFO::OK)
		{
			path = VfsPath("art/textures/skies") / m_SkySet / (Path::String(images[i])+L".dds.cached.dds");
			if (g_VFS->LoadFile(path, file, fileSize) != INFO::OK)
			{
				glDeleteTextures(1, &m_SkyCubeMap);
				LOGERROR("Error creating sky cubemap.");
				return;
			}
		}

		Tex tex;
		tex.decode(file, fileSize);

		tex.transform_to((tex.m_Flags | TEX_BOTTOM_UP | TEX_ALPHA) & ~(TEX_DXT | TEX_MIPMAPS));

		u8* data = tex.get_data();

		if (types[i] == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || types[i] == GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
		{
			std::vector<u8> rotated(tex.m_DataSize);

			for (size_t y = 0; y < tex.m_Height; ++y)
			{
				for (size_t x = 0; x < tex.m_Width; ++x)
				{
					size_t invx = y, invy = tex.m_Width-x-1;

					rotated[(y*tex.m_Width + x) * 4 + 0] = data[(invy*tex.m_Width + invx) * 4 + 0];
					rotated[(y*tex.m_Width + x) * 4 + 1] = data[(invy*tex.m_Width + invx) * 4 + 1];
					rotated[(y*tex.m_Width + x) * 4 + 2] = data[(invy*tex.m_Width + invx) * 4 + 2];
					rotated[(y*tex.m_Width + x) * 4 + 3] = data[(invy*tex.m_Width + invx) * 4 + 3];
				}
			}

			glTexImage2D(types[i], 0, GL_RGBA, tex.m_Width, tex.m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rotated[0]);
		}
		else
		{
			glTexImage2D(types[i], 0, GL_RGBA, tex.m_Width, tex.m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if CONFIG2_GLES
#warning TODO: fix SkyManager::LoadSkyTextures for GLES
#else
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	///////////////////////////////////////////////////////////////////////////
}


///////////////////////////////////////////////////////////////////
// Switch to a different sky set (while the game is running)
void SkyManager::SetSkySet(const CStrW& newSet)
{
	if (newSet == m_SkySet)
		return;

	if (m_SkyCubeMap)
	{
		glDeleteTextures(1, &m_SkyCubeMap);
		m_SkyCubeMap = 0;
	}

	m_SkySet = newSet;

	LoadSkyTextures();
}

///////////////////////////////////////////////////////////////////
// Generate list of available skies
std::vector<CStrW> SkyManager::GetSkySets() const
{
	std::vector<CStrW> skies;

	// Find all subdirectories in art/textures/skies

	const VfsPath path(L"art/textures/skies/");
	DirectoryNames subdirectories;
	if (g_VFS->GetDirectoryEntries(path, 0, &subdirectories) != INFO::OK)
	{
		LOGERROR("Error opening directory '%s'", path.string8());
		return std::vector<CStrW>(1, GetSkySet()); // just return what we currently have
	}

	for(size_t i = 0; i < subdirectories.size(); i++)
		skies.push_back(subdirectories[i].string());
	sort(skies.begin(), skies.end());

	return skies;
}

///////////////////////////////////////////////////////////////////
// Render sky
void SkyManager::RenderSky()
{
#if CONFIG2_GLES
#warning TODO: implement SkyManager::RenderSky for GLES
#else
	if (!m_RenderSky)
		return;

	// Draw the sky as a small box around the map, with depth write enabled.
	// This will be done before anything else is drawn so we'll be overlapped by
	// everything else.

	// Do nothing unless SetSkySet was called
	if (m_SkySet.empty())
		return;

	glDepthMask(GL_FALSE);

	const CCamera& camera = g_Renderer.GetViewCamera();

	CShaderTechniquePtr skytech =
		g_Renderer.GetShaderManager().LoadEffect(str_sky_simple);
	skytech->BeginPass();
	CShaderProgramPtr shader = skytech->GetShader();
	shader->BindTexture(str_baseTex, m_SkyCubeMap);

	// Translate so the sky center is at the camera space origin.
	CMatrix3D translate;
	translate.SetTranslation(camera.GetOrientation().GetTranslation());

	// Currently we have a hardcoded near plane in the projection matrix.
	CMatrix3D scale;
	scale.SetScaling(10.0f, 10.0f, 10.0f);

	// Rotate so that the "left" face, which contains the brightest part of
	// each skymap, is in the direction of the sun from our light
	// environment.
	CMatrix3D rotate;
	rotate.SetYRotation(M_PI + g_Renderer.GetLightEnv().GetRotation());

	shader->Uniform(
		str_transform,
		camera.GetViewProjection() * translate * rotate * scale);

	std::vector<GLfloat> vertexData;
	// 6 sides of cube with 4 vertices with 6 floats (3 uv and 3 position).
	vertexData.reserve(6 * 4 * 6);
#define ADD_VERTEX(U, V, W, X, Y, Z) \
	STMT( \
		vertexData.push_back(X); \
		vertexData.push_back(Y); \
		vertexData.push_back(Z); \
		vertexData.push_back(U); \
		vertexData.push_back(V); \
		vertexData.push_back(W);)

	// GL_TEXTURE_CUBE_MAP_NEGATIVE_X
	ADD_VERTEX(+1, +1, +1, -1.0f, -1.0f, -1.0f);
	ADD_VERTEX(+1, +1, -1, -1.0f, -1.0f, +1.0f);
	ADD_VERTEX(+1, -1, -1, -1.0f, +1.0f, +1.0f);
	ADD_VERTEX(+1, -1, +1, -1.0f, +1.0f, -1.0f);

	// GL_TEXTURE_CUBE_MAP_POSITIVE_X
	ADD_VERTEX(-1, +1, -1, +1.0f, -1.0f, +1.0f);
	ADD_VERTEX(-1, +1, +1, +1.0f, -1.0f, -1.0f);
	ADD_VERTEX(-1, -1, +1, +1.0f, +1.0f, -1.0f);
	ADD_VERTEX(-1, -1, -1, +1.0f, +1.0f, +1.0f);

	// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
	ADD_VERTEX(-1, +1, +1, +1.0f, -1.0f, -1.0f);
	ADD_VERTEX(-1, +1, -1, +1.0f, -1.0f, +1.0f);
	ADD_VERTEX(+1, +1, -1, -1.0f, -1.0f, +1.0f);
	ADD_VERTEX(+1, +1, +1, -1.0f, -1.0f, -1.0f);

	// GL_TEXTURE_CUBE_MAP_POSITIVE_Y
	ADD_VERTEX(+1, -1, +1, -1.0f, +1.0f, -1.0f);
	ADD_VERTEX(+1, -1, -1, -1.0f, +1.0f, +1.0f);
	ADD_VERTEX(-1, -1, -1, +1.0f, +1.0f, +1.0f);
	ADD_VERTEX(-1, -1, +1, +1.0f, +1.0f, -1.0f);

	// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	ADD_VERTEX(-1, +1, +1, +1.0f, -1.0f, -1.0f);
	ADD_VERTEX(+1, +1, +1, -1.0f, -1.0f, -1.0f);
	ADD_VERTEX(+1, -1, +1, -1.0f, +1.0f, -1.0f);
	ADD_VERTEX(-1, -1, +1, +1.0f, +1.0f, -1.0f);

	// GL_TEXTURE_CUBE_MAP_POSITIVE_Z
	ADD_VERTEX(+1, +1, -1, -1.0f, -1.0f, +1.0f);
	ADD_VERTEX(-1, +1, -1, +1.0f, -1.0f, +1.0f);
	ADD_VERTEX(-1, -1, -1, +1.0f, +1.0f, +1.0f);
	ADD_VERTEX(+1, -1, -1, -1.0f, +1.0f, +1.0f);
#undef ADD_VERTEX

	shader->VertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 6, &vertexData[0]);
	shader->TexCoordPointer(
		GL_TEXTURE0, 3, GL_FLOAT, sizeof(GLfloat) * 6, &vertexData[3]);
	shader->AssertPointersBound();

	glDrawArrays(GL_QUADS, 0, 6 * 4);

	skytech->EndPass();

	glDepthMask(GL_TRUE);

#endif
}
