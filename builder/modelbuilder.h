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

#ifndef UFOATTACK_MODELBUILDER_INCLUDED		
#define UFOATTACK_MODELBUILDER_INCLUDED

#include "../engine/vertex.h"
#include "../engine/enginelimits.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glcontainer.h"

class Atlas;
class AtlasSubTex;


struct VertexGroup {
	VertexGroup() : nVertex( 0 ), nIndex( 0 ) {}

	grinliz::CStr< EL_FILE_STRING_LEN > textureName;

	int nVertex;
	Vertex	vertex[EL_MAX_VERTEX_IN_GROUP];
	int nIndex;
	U16 index[EL_MAX_INDEX_IN_GROUP];
};


struct VertexStream {
	VertexStream() : nVertex( 0 )	{}

	enum { MAX_VERTEX = EL_MAX_VERTEX_IN_GROUP*4 };
	int				nVertex;
	Vertex			vertex[ MAX_VERTEX ];
	bool			normalProcessed[ MAX_VERTEX ];
};


class ModelBuilder {
public:
	ModelBuilder() :	current( 0 ),
						polyCulled( 0 ),
						nGroup( 0 ),
						shading( FLAT ),
						polyRemoval( POLY_NONE ),
//						atlasArr( 0 ),
//						nAtlas( 0 ),
//						currentSubTex( 0 ),
						depth( -1 ),
						boneID( -1 ),
						enableBones( false )
	{}

	//void SetAtlasPool( const Atlas* atlasArr, int nAtlas ) { this->atlasArr = atlasArr; this->nAtlas = nAtlas; }

	void SetMatrix( const grinliz::Matrix4& mat )		{ matrix = mat; }

	// Set the current texture. Can be empty string. Must be called before AddTri.
	void SetTexture( const char* textureName );

	// Set smooth shading, generally true for characters and false for buildings.
	enum ShadingType {
		FLAT,
		CREASE,
		SMOOTH
	};
	void SetShading( ShadingType value )			{ this->shading = value; }
	enum {
		POLY_PRE,
		POLY_POST,
		POLY_NONE
	};
	void SetPolygonRemoval( int pass )	{	GLASSERT( pass != POLY_POST );	// not yet implemented
											this->polyRemoval = pass; }
	
	void EnableBones( bool enable ) { enableBones = enable; }
	// Used at this point to create bones for objects. The bone weight
	// should be in the data file.
	void PushObjectName( const char* name );
	void PopObjectName();

	// Add the tri for the current texture.
	void AddTri( const Vertex& v0, const Vertex& v1, const Vertex& v2 );

	// Finishes adding triangle, runs the optimizer, calculates normals.
	void Flush();

	const VertexGroup* Groups()		{ return group; }
	int NumGroups()					{ return nGroup; }
	int PolyCulled() const			{ return polyCulled; }

	const grinliz::Rectangle3F& Bounds()		{ return bounds; }

	grinliz::CArray<grinliz::IString, EL_MAX_BONES> boneNames;

private:
	int current;
	int polyCulled;

	int nGroup;
	ShadingType shading;
	int polyRemoval;

//	const Atlas* atlasArr;
//	int nAtlas;
//	const AtlasSubTex* currentSubTex;
	int depth;
	int boneID;
	bool enableBones;
	
	grinliz::Matrix4 matrix;
	grinliz::Rectangle3F bounds;

	VertexGroup group[EL_MAX_MODEL_GROUPS];
	VertexStream stream[EL_MAX_MODEL_GROUPS];
};


#endif // UFOATTACK_MODELBUILDER_INCLUDED