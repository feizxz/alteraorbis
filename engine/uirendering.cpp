/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../grinliz/glvector.h"

#include "uirendering.h"
#include "texture.h"
#include "text.h"
#include "shadermanager.h"
#include "surface.h"


using namespace grinliz;
using namespace gamui;


FontSingleton* FontSingleton::instance = 0;

FontSingleton::FontSingleton()
{
	GLASSERT(instance == 0);
	instance = this;
	fontHeight = 0;
	fontTexture = TextureManager::Instance()->CreateTexture("fontTexture", 512, 256, TEX_RGBA16, Texture::PARAM_NEAREST, this);
}


FontSingleton::~FontSingleton()
{
	GLASSERT(instance == this);
	instance = 0;
}


void FontSingleton::CreateTexture(Texture* t)
{
	if (StrEqual(t->Name(), "fontTexture")) {
		GLASSERT(fontHeight);
		if (!fontHeight) return;

		const int w = t->Width();
		const int h = t->Height();

		uint8_t* d8 = new uint8_t[w*h];
		Generate(fontHeight, d8, w, h, true);
		t->UploadAlphaToRGBA16(d8, w * h);
		GLOUTPUT(("FontSingleton::CreateTexture %d pixels\n", fontHeight));

		delete[] d8;
	}
	else {
		GLASSERT(0);
	}
}


gamui::RenderAtom FontSingleton::TextAtom(bool disabled)
{
	RenderAtom atom;
	if (!disabled)
		atom.Init((const void*)UIRenderer::RENDERSTATE_UI_TEXT, (const void*)fontTexture, 0, 0, 1, 1);
	else
		atom.Init((const void*)UIRenderer::RENDERSTATE_UI_TEXT_DISABLED, (const void*)fontTexture, 0, 0, 1, 1);
	return atom;
}


void FontSingleton::SetPhysicalPixel(int h)
{
	if (h != fontHeight) {
		fontHeight = h;
		CreateTexture(fontTexture);
	}
}



void UIRenderer::BeginRender( int nIndex, const uint16_t* index, int nVertex, const gamui::Gamui::Vertex* vertex )
{
	// Should be completely uneeded, but fixes bugs on a netbook. (With questionable drivers.)
	GPUDevice::Instance()->ResetState();
	vbo = GPUDevice::Instance()->GetUIVBO( layer );
	ibo = GPUDevice::Instance()->GetUIIBO( layer );
	vbo->Upload( vertex, nVertex*sizeof(*vertex), 0 );
	ibo->Upload( index, nIndex, 0 );
}


void UIRenderer::EndRender()
{
}


void UIRenderer::BeginRenderState( const void* renderState )
{
	int state = int(intptr_t(renderState)) & 0xffff;
	shader = CompositingShader();

	switch ( state )
	{
	case RENDERSTATE_UI_NORMAL:
		shader.SetColor( 1, 1, 1, 1 );
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_NORMAL_OPAQUE:
		shader.SetColor( 1, 1, 1, 1 );
		shader.SetBlendMode( BLEND_NONE );
		break;

	case RENDERSTATE_UI_GRAYSCALE_OPAQUE:
		shader.SetColor( 1, 1, 1, 1 );
		shader.SetShaderFlag(ShaderManager::SATURATION);
		shader.SetBlendMode( BLEND_NONE );
		break;

	case RENDERSTATE_UI_DISABLED:
		shader.SetColor( 1, 1, 1, 0.5f );
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_TEXT:
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_TEXT_DISABLED:
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_DECO:
		shader.SetColor( 1, 1, 1, 0.7f );
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_DECO_DISABLED:
		shader.SetColor( 1, 1, 1, 0.2f );
		shader.SetBlendMode( BLEND_NORMAL );
		break;

	case RENDERSTATE_UI_CLIP_XFORM_MAP_0:
	case RENDERSTATE_UI_CLIP_XFORM_MAP_1:
	case RENDERSTATE_UI_CLIP_XFORM_MAP_2:
	case RENDERSTATE_UI_CLIP_XFORM_MAP_3:
		{
			shader.SetColor( 1, 1, 1, 1 );
			shader.SetBlendMode( BLEND_NORMAL );
			shader.SetShaderFlag( ShaderManager::TEXTURE0_CLIP);
			shader.SetShaderFlag( ShaderManager::TEXTURE0_COLORMAP );
		}
		break;

	default:
		GLASSERT( 0 );
		break;
	}
}


void UIRenderer::BeginTexture( const void* textureHandle )
{
	texture = (Texture*)textureHandle;
}


void UIRenderer::Render( const void* renderState, const void* textureHandle, int start, int count )
{
	GPUStream stream( GPUStream::kGamuiType );
	
	GPUStreamData data;
	data.texture0 = (Texture*)textureHandle;
	data.indexBuffer  = ibo->ID();
	data.vertexBuffer = vbo->ID();
	GPUControlParam control;
	control.saturation = 0;

	int rs = int(intptr_t(renderState));
	if (rs == RENDERSTATE_UI_GRAYSCALE_OPAQUE) {
		data.controlParam = &control;
	}

	if (rs >= RENDERSTATE_UI_CLIP_XFORM_MAP_0 && rs <= RENDERSTATE_UI_CLIP_XFORM_MAP_3) {
		int id = rs - RENDERSTATE_UI_CLIP_XFORM_MAP_0;
		data.texture0XForm = uv + id;
		data.texture0Clip = uvClip + id;
		data.texture0ColorMap = colorXForm + id;
	}
	GLASSERT(count < 16000);	// should be fine, sanity check. think there is a hard cutoff for 16bit vertices
	GPUDevice::Instance()->Draw( shader, stream, data, start, count, 1 );
}


void UIRenderer::SetAtomCoordFromPixel( int x0, int y0, int x1, int y1, int w, int h, RenderAtom* atom )
{
	atom->tx0 = (float)x0 / (float)w;
	atom->tx1 = (float)x1 / (float)w;

	atom->ty0 = (float)(h-y1) / (float)h;
	atom->ty1 = (float)(h-y0) / (float)h;
}


void DecoEffect::Play( int startPauseTime, bool invisibleWhenDone )	
{
	this->startPauseTime = startPauseTime;
	this->invisibleWhenDone = invisibleWhenDone;
	rotation = 0;
	if ( item ) {
		item->SetVisible( true );
		item->SetRotationY( 0 );
	}
}


void DecoEffect::DoTick( U32 delta )
{
	startPauseTime -= (int)delta;
	if ( startPauseTime <= 0 ) {
		static const float CYCLE = 5000.f;
		rotation += (float)delta * 360.0f / CYCLE;

		if ( rotation > 90.f && invisibleWhenDone ) {
			if ( item ) 
				item->SetVisible( false );
		}
		while( rotation >= 360.f )
			rotation -= 360.f;

		if ( rotation > 90 && rotation < 270 )
			rotation += 180.f;

		if ( item && item->Visible() )
			item->SetRotationY( rotation );
	}
}

