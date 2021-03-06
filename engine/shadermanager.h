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

#ifndef XENOENGINE_SHADERMANAGER_INCLUDED
#define XENOENGINE_SHADERMANAGER_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glcolor.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glmatrix.h"
#include "../grinliz/glcontainer.h"

#include "enginelimits.h"

class Texture;
class Stream;

class IDeviceLossHandler
{
public:
	virtual void DeviceLoss() = 0;
};



/*	A class to manage shaders. No actual "shader" object is returned; a
	Shader is made current with ActivateShader() and then Set methods
	are used.
*/
class ShaderManager
{
public:
	ShaderManager();
	~ShaderManager();

	static ShaderManager* Instance();

	enum {				
		// Texture state.
		TEXTURE0			= (1<<1),		// Texture is in use. Note that the sampling state (linear, nearest) is saved with the texture.
		TEXTURE0_XFORM		= (1<<2),		// u' = u*x + z, v' = v*.y + w	(instanced, vec4)
		TEXTURE0_CLIP		= (1<<3),		// clip to (x0,y0), (x1,y1) (instanced, vec4)
		TEXTURE0_COLORMAP	= (1<<4),		// remap colors (instanced, matrix)

		// Colors and Lighting
		COLORS				= (1<<5),		// Per-vertex colors.
		LIGHTING			= (1<<6),		// Lighting. Requires per vertex normals, 
											// light direction, ambient color, and diffuse color.

		// Color features.
		PREMULT				= (1<<7),		// convert to pre-multiplied in the fragment shader
		EMISSIVE			= (1<<8),		// interpret the alpha channel as emission.
		EMISSIVE_EXCLUSIVE  = (1<<9),		// everything not emissive is black

		COLOR_PARAM			= (1<<11),		// Apply per model color.
		BONE_FILTER			= (1<<11),		// Apply per model bone filtering
		SATURATION			= (1<<12),		// Per pixel saturation effect. Uses control.y

		// Features:
		BONE_XFORM			= (1<<13),
		INSTANCE			= (1<<14)
	};

	void DeviceLoss();
	void AddDeviceLossHandler( IDeviceLossHandler* handler );
	void RemoveDeviceLossHandler( IDeviceLossHandler* handler );

	// ActivateShader should be called *before* the sets, so  the sets are correctly validated. 
	void ActivateShader( int flags );

	int MaxInstances() const	{ GLASSERT( active ); return active->maxInstance; }
	int ShaderFlags() const		{ GLASSERT( active ); return active->flags; }

	// Warning: must match gAttributeName
	// Warning: must match declaration in shader
	enum {
		A_TEXTURE0,		// 2 comp
		A_POS,			// 3 comp
		A_NORMAL,		// 2 comp
		A_COLOR,		// 3 comp
		A_BONE_ID,		// 1 comp (float)
		A_INSTANCE_ID,	// 1 comp (float)
		MAX_ATTRIBUTE
	};
	void ClearStream();
	void SetStreamData( int id, int count, int type, int stride, const void* data );	 
	void SetParticleStream();

	// Warning: must match gUniformName
	enum {
		U_MVP_MAT,		
		U_M_MAT_ARR,			// array for instancing
		U_COLOR_PARAM_ARR,		// params for instancing color 
		U_FILTER_PARAM_ARR,
		U_CONTROL_PARAM_ARR,
		U_TEXTURE0_XFORM_ARR,
		U_TEXTURE0_CLIP_ARR,
		U_TEXTURE0_COLORMAP_ARR,

		U_NORMAL_MAT,
		U_COLOR_MULT,
		U_LIGHT_DIR,
		U_AMBIENT,
		U_DIFFUSE,
		U_BONEXFORM,

		U_TEXTURE0,

		MAX_UNIFORM
	};

	void SetTexture( int index, Texture* texture );
	void SetUniform( int id, const grinliz::Matrix4& mat );
	void SetUniform( int id, const grinliz::Vector4F& vector );
	void SetUniform( int id, const grinliz::Vector3F& vector );
	void SetUniform( int id, float value );

	void SetUniformArray( int id, int count, const grinliz::Matrix4* mat );
	void SetUniformArray( int id, int count, const grinliz::Vector4F* params );
	void SetUniformArray( int id, int count, const grinliz::Vector3F* params );

private:
	int  CalcMaxInstances( int flags, int* uniforms );

	static ShaderManager* instance;
	
	struct Shader {
		void Init() {
			flags = 0;
			vertexProg = fragmentProg = prog = 0;
			for( int i=0; i<MAX_ATTRIBUTE; ++i ) attributeLoc[i] = -1;
			for( int i=0; i<MAX_UNIFORM; ++i ) uniformLoc[i] = -1;
		}

		int flags;
		U32 vertexProg;
		U32 fragmentProg;
		U32 prog;
		int maxInstance;

		int attributeLoc[MAX_ATTRIBUTE];
		int uniformLoc[MAX_UNIFORM];

		int GetAttributeLocation( int attribute );
		int GetUniformLocation( int uniform );
	};

	grinliz::CDynArray< IDeviceLossHandler* > deviceLossHandlers;
	grinliz::CDynArray< Shader > shaderArr;
	grinliz::GLString header;
	grinliz::GLString fixedpipeVert, fixedpipeFrag;
	grinliz::CStr<9> hashStr;

	Shader* active;
	grinliz::CDynArray<int> activeStreams;
	U32 totalCompileTime;
	U32 vertexArrayID;
	grinliz::CDynArray<float> matrixCache;

	void LoadProgram( const char* name, grinliz::GLString *store );
	Shader* CreateProgram( int flag );
	void DeleteProgram( Shader* );

	void AppendFlag( grinliz::GLString* str, const char* flag, int set, int value=1 );
	void AppendConst( grinliz::GLString* str, const char* name, int value );
};

#endif //  XENOENGINE_SHADERMANAGER_INCLUDED