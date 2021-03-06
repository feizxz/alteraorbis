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

#ifndef UFOATTACK_STATE_MANAGER_INCLUDED
#define UFOATTACK_STATE_MANAGER_INCLUDED

// Be sure to NOT include the gl platform header, so this can be 
// used as a platform-independent header file, and still exclude
// the gl headers.
#include <stdint.h>
#include "../grinliz/gldebug.h"
#include "../grinliz/glmatrix.h"
#include "../grinliz/glcolor.h"
#include "../grinliz/glrandom.h"

#include "vertex.h"

class Texture;
class Particle;
class GPUState;

class MatrixStack
{
public:
	MatrixStack();
	~MatrixStack();

	void Push();
	void Pop();
	void Set( const grinliz::Matrix4& m )			{ stack[index] = m; }
	void Multiply( const grinliz::Matrix4& m );

	const grinliz::Matrix4& Top() const				{ GLASSERT( index < MAX_DEPTH ); return stack[index]; }
	const grinliz::Matrix4& Bottom() const			{ return stack[0]; }
	int NumMatrix() const							{ return index+1; }

private:
	enum { MAX_DEPTH = 4 };
	int index;
	grinliz::Matrix4 stack[MAX_DEPTH];
};


class GPUBuffer
{
public:
	GPUBuffer() : id( 0 )			{}
	virtual ~GPUBuffer()			{}

	U32 ID() const					{ return id; }

protected:	
	U32 id;
};


class GPUVertexBuffer : public GPUBuffer
{
public:
	// a null value for vertex will create an empty buffer
	GPUVertexBuffer( const void* data, int sizeInBytes );
	~GPUVertexBuffer();

	void Upload( const void* data, int nBytes, int start );
	int SizeInBytes() const { return sizeInBytes; }
private:
	int sizeInBytes;
};


class GPUIndexBuffer : public GPUBuffer
{
public:
	void Upload( const uint16_t* data, int size, int start );

	GPUIndexBuffer( const uint16_t* index, int nIndex );
	~GPUIndexBuffer();

	int NumIndex() const { return nIndex; }
private:
	int nIndex;
};


/* Class that defines the data layout of a stream.
*/
struct GPUStream {
	// Defines float sized components.
	int stride;
	int nPos;		
	int posOffset;
	int texture0Offset;
	int nNormal;
	int normalOffset;
	int nColor;
	int colorOffset;
	int boneOffset;
	int idOffset;

	enum GamuiType { kGamuiType };

	GPUStream() :  stride( 0 ),
				nPos( 0 ), posOffset( 0 ), 
				texture0Offset( 0 ),
				nNormal( 0 ), normalOffset( 0 ),
				nColor( 0 ), colorOffset( 0 ), boneOffset( 0 ), idOffset(0) {}

	GPUStream( const Vertex& vertex );
	GPUStream( const VertexInst& vertex );
	GPUStream( GamuiType );
	GPUStream( const PTVertex& vertex );
	GPUStream( const PTVertex2& vertex );

	void Clear();

	bool HasPos() const			{ return nPos > 0; }
	bool HasNormal() const		{ return nNormal > 0; }
	bool HasColor() const		{ return nColor > 0; }
	bool HasInstanceID() const	{ return idOffset > 0; }
};

struct GPUControlParam
{
	GPUControlParam() : fade(1), saturation(1), emissive(1), _pad1(1)	{}
	const grinliz::Vector4F* Mem() const { return reinterpret_cast<const grinliz::Vector4F*>(&fade); }
	void Init() { fade = saturation = emissive = _pad1; }
	// For serialization:
	void Set(const grinliz::Vector4F c) {
		fade = c.x;
		saturation = c.y;
		emissive = c.z;
		_pad1 = c.w;
	}

	// Really a vec4 under the hood. Must be 4 floats.
	float fade, saturation, emissive, _pad1;
};

/* Class that actually contains the pointers, buffers, and uniforms for rendering.
*/
struct GPUStreamData
{
	GPUStreamData() : vertexBuffer(0), 
					  indexBuffer(0), 
					  texture0(0), 
					  maxInstance(1),
					  matrix(0), 
					  colorParam(0), 
					  boneFilter(0), 
					  controlParam(0),
					  texture0XForm(0),
					  texture0Clip(0),
					  texture0ColorMap(0),
					  bones(0)
	{}

	U32					vertexBuffer;
	U32					indexBuffer;

	Texture*			texture0;			// NOT instanced.
	int					maxInstance;

	grinliz::Matrix4*	matrix;
	grinliz::Vector4F*	colorParam;
	grinliz::Vector4F*	boneFilter;
	GPUControlParam*	controlParam;
	grinliz::Vector4F*	texture0XForm;
	grinliz::Vector4F*	texture0Clip;
	grinliz::Matrix4*	texture0ColorMap;	// 3 vec4 per instance
	grinliz::Matrix4*	bones;				// EL_MAX_BONES per instance
};


// Flag values used for state ordering - be sure Blend is in the correct high bits!
enum StencilMode {
	STENCIL_OFF		= 0,			// ignore stencil
	STENCIL_WRITE	= (1<<0),		// draw commands write to stencil
	STENCIL_SET		= (1<<1),		// draw if stencil is set
	STENCIL_CLEAR	= (1<<2),		// draw if stencil is clear
	STENCIL_MASK    = STENCIL_WRITE | STENCIL_SET | STENCIL_CLEAR
};

enum DepthWrite {
	DEPTH_WRITE_TRUE	= 0,
	DEPTH_WRITE_FALSE	= (1<<3),
	DEPTH_WRITE_MASK	= DEPTH_WRITE_FALSE
};

enum DepthTest {
	DEPTH_TEST_TRUE		= 0,
	DEPTH_TEST_FALSE	= (1<<4),
	DEPTH_TEST_MASK		= DEPTH_TEST_FALSE
};

enum ColorWrite {
	COLOR_WRITE_TRUE	= 0,
	COLOR_WRITE_FALSE	= (1<<5),
	COLOR_WRITE_MASK	= COLOR_WRITE_FALSE
};

enum LightingType {
	LIGHTING_LAMBERT	= 0,
	LIGHTING_HEMI		= (1<<6),
	LIGHTING_MASK		= LIGHTING_HEMI
};

enum BlendMode {
	BLEND_NONE		= 0,			// opaque
	BLEND_NORMAL	= (1<<7),		// a, 1-a
	BLEND_ADD		= (1<<8),		// additive blending
	BLEND_MASK		= BLEND_NORMAL | BLEND_ADD
};


/* In the case of device loss, the GPUDevice is deleted and re-created. */
class GPUDevice
{
public:
	enum MatrixType {
		MODELVIEW_MATRIX,
		PROJECTION_MATRIX,
	};

	static GPUDevice* Instance()	{ if ( !instance ) instance = new GPUDevice(); return instance; }
	~GPUDevice();

	void ResetState();
	void Clear( float r, float g, float b, float a );

	// Set the top level state. The engine has top level (root transforms)
	// for the screen size, scissor, and transform:
	void SetViewport( int w, int h );
	void SetOrthoTransform( int width, int height );
	void SetPerspectiveTransform( const grinliz::Matrix4& perspective );
	// The top level V matrix in perspective mode.
	void SetCameraTransform( const grinliz::Matrix4& camera );

	void PushMatrix( MatrixType type );
	void SetMatrix( MatrixType type, const grinliz::Matrix4& m );
	void MultMatrix( MatrixType type, const grinliz::Matrix4& m );
	void PopMatrix( MatrixType type );

	const grinliz::Matrix4& TopMatrix( MatrixType type );
	const grinliz::Matrix4& ViewMatrix();

	void ResetTriCount()	{ trianglesDrawn = 0; drawCalls = 0; }
	int TrianglesDrawn()	{ return trianglesDrawn; }
	int DrawCalls()			{ return drawCalls; }

	grinliz::Color4F	ambient;
	grinliz::Vector4F	directionWC;
	grinliz::Color4F	diffuse;

	// Draws indexed triangles.
	void Draw(		const GPUState& state, 
					const GPUStream& stream, 
					const GPUStreamData& data, 
					int startIndex,
					int nIndex, 
					int nInstance );

	// Draws triangles. (No indices.)
	void DrawQuads(	const GPUState& state,
					const GPUStream& stream, 
					const GPUStreamData& data, 
					int nQuads );

	void DrawQuad(	const GPUState& state, Texture* texture, const grinliz::Vector3F p0, const grinliz::Vector3F p1, bool positiveWinding=true );
	void DrawArrow( const GPUState& state, const grinliz::Vector3F p0, const grinliz::Vector3F p1, float width=0.4f );

	// There are 3 UI layers, but can be many more gamui main objects
	// because of scene stacking. Rather than run around and track
	// all those VBOs, keep them here.
	enum {
		OVERLAY0,
		OVERLAY1,
		ENGINE,
		HUD,
		NUM_UI_LAYERS
	};
	GPUVertexBuffer* GetUIVBO( int id );
	GPUIndexBuffer*  GetUIIBO( int id );

private:
	static GPUDevice* instance;
	GPUDevice();

	enum { NUM_QUAD_BUFFERS = 64 };
	int currentQuadBuf;

	bool			 uiVBOInUse[NUM_UI_LAYERS];
	GPUVertexBuffer* vertexBuffer[NUM_UI_LAYERS];
	GPUIndexBuffer*	 indexBuffer[NUM_UI_LAYERS];
	GPUVertexBuffer* quadBuffer[NUM_QUAD_BUFFERS];

	// Sets up the shader.
	void Weld( const GPUState&, const GPUStream&, const GPUStreamData& );
	int Upload( const GPUState&, const GPUStream&, const GPUStreamData&, int start, int instances );

	void SwitchMatrixMode( MatrixType type );	
	static const void* PTR( const void* base, int offset ) {
		return (const void*)((const U8*)base + offset);
	}

	MatrixType	matrixMode;

	MatrixStack mvStack;
	MatrixStack projStack;

	BlendMode	currentBlend;
	DepthTest	currentDepthTest;
	DepthWrite	currentDepthWrite;
	ColorWrite	currentColorWrite;
	StencilMode	currentStencilMode;

	int					primitive;
	int					trianglesDrawn;
	int					drawCalls;
	uint32_t			uid;
	int					matrixDepth[3];
	GPUIndexBuffer*		quadIndexBuffer;

	grinliz::Matrix4	identity[EL_MAX_INSTANCE];
	GPUControlParam		defaultControl[EL_MAX_INSTANCE];
};

/* WARNING: this gets copied around, and slices.
   Sub-classes are for initialization. They can't
   store data.
*/
class GPUState 
{
public:

	int HasLighting() const;

	void SetColor( float r, float g, float b )				{ color.x = r; color.y = g; color.z = b; color.w = 1; }
	void SetColor( float r, float g, float b, float a )		{ color.x = r; color.y = g; color.z = b; color.w = a; }
	void SetColor( const grinliz::Color4F& c )				{ color = c; }
	void SetColor( const grinliz::Color4U8& color ) {
		static const float INV = 1.0f/255.0f;
		grinliz::Color4F c = { (float)color.x*INV, (float)color.y*INV, (float)color.z*INV, (float)color.w*INV };
		SetColor( c );
	}
	const grinliz::Color4F& Color() const { return color; }

	// Set any of the flags (that are boolean) from ShaderManager
	void SetShaderFlag( int flag )				{ shaderFlags |= flag; }
	void ClearShaderFlag( int flag )			{ shaderFlags &= (~flag); }
	int  ShaderFlags() const					{ return shaderFlags; }
	int  StateFlags() const						{ return stateFlags; }

	GPUState() : stateFlags( 0 ),
				 shaderFlags( 0 )
	{
		color.Set( 1,1,1,1 );
	}

    void SetStencilMode( StencilMode value )	{ stateFlags &= (~STENCIL_MASK); stateFlags |= value; }
    void SetDepthWrite( bool value )            { stateFlags &= (~DEPTH_WRITE_MASK); stateFlags |= (value ? DEPTH_WRITE_TRUE : DEPTH_WRITE_FALSE); }
    void SetDepthTest( bool value )             { stateFlags &= (~DEPTH_TEST_MASK); stateFlags |= (value ? DEPTH_TEST_TRUE : DEPTH_TEST_FALSE); }
    void SetColorWrite( bool value )            { stateFlags &= (~COLOR_WRITE_MASK); stateFlags |= (value ? COLOR_WRITE_TRUE : COLOR_WRITE_FALSE); }
    void SetBlendMode( BlendMode value )        { stateFlags &= (~BLEND_MASK); stateFlags |= value; }

protected:

	int				stateFlags;
	int				shaderFlags;
	grinliz::Color4F color;	// actual state color; render a bunch of stuff in black, for example.
							// not to be confused with per-vertex or per-instance color, also supported.

public:
	bool operator==( const GPUState& s ) const {
		return (   stateFlags == s.stateFlags
				&& shaderFlags == s.shaderFlags
				&& color == s.color );
	}
};


class CompositingShader : public GPUState
{
public:
	/** Writes texture or color and neither writes nor tests z. 
		Texture/Color support
			- no texture
			- texture0, modulated by color
			- texture0 and texture1 (light map compositing)
		Blend support
	*/
	CompositingShader( BlendMode blend=BLEND_NONE );
};


class LightShader : public GPUState
{
public:
	/** Texture or color. Writes & tests z. Enables lighting. */
	LightShader( int lightFlag, BlendMode blend = BLEND_NONE );
	
protected:
};


class FlatShader : public GPUState
{
public:
	FlatShader()	{}	// totally vanilla
};


class ParticleShader : public GPUState
{
public:
	ParticleShader();
};

#endif
