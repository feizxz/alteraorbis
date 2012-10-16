#include "../shared/lodepng.h"

#include "livepreviewscene.h"

#include "../engine/engine.h"
#include "../engine/model.h"
#include "../xegame/testmap.h"

#include "../game/lumosgame.h"

using namespace gamui;
using namespace grinliz;

static const float CENTER_X = 4.0f;
static const float CENTER_Z = 4.0f;
static const int SIZE = 256;

LivePreviewScene::LivePreviewScene( LumosGame* game ) : Scene( game )
{
	TestMap* map = 0;
	//TestMap* map = new TestMap( 8, 8 );
	//Color3F c = { 0.5f, 0.5f, 0.5f };
	//map->SetColor( c );

	engine = new Engine( game->GetScreenportMutable(), game->GetDatabase(), map );
	const ModelResource* modelResource = ModelResourceManager::Instance()->GetModelResource( "unitPlateProcedural" );
	model = engine->AllocModel( modelResource );
	model->SetPos( CENTER_X, 0, CENTER_Z );

	model->SetYRotation( 180.0f );	// FIXME: compensating for camera issue, below. Camera is "upside down" when looking down.
	engine->camera.SetPosWC( CENTER_X, 8, CENTER_Z );
	static const Vector3F out = { 0, 0, 1 };
	engine->camera.SetDir( V3F_DOWN, out );		// look straight down. This works! cool.

	TextureManager* texman = TextureManager::Instance();
	Texture* t = texman->GetTexture( "procedural" );
	GLASSERT( t );
	CreateTexture( t );

	game->InitStd( &gamui2D, &okay, 0 );

}


LivePreviewScene::~LivePreviewScene()
{
	Map* map = engine->GetMap();
	engine->FreeModel( model );
	delete engine;
	delete map;
}


void LivePreviewScene::Resize()
{
	//const Screenport& port = game->GetScreenport();
	static_cast<LumosGame*>(game)->PositionStd( &okay, 0 );

	//LayoutCalculator layout = lumosGame->DefaultLayout();
}


void LivePreviewScene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		game->PopScene();
	}
}


void LivePreviewScene::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
}


void LivePreviewScene::CreateTexture( Texture* t )
{
	unsigned w=0, h=0;
	U8* pixels = 0;
	int error = lodepng_decode32_file( &pixels, &w, &h, "./res/face.png" );
	GLASSERT( error == 0 );
	GLASSERT( w == SIZE*4 );
	GLASSERT( h == SIZE );
	U16 buffer[SIZE*SIZE];

	if ( error == 0 ) {
		int scanline = w*4;

		for( int j=0; j<SIZE; ++j ) {
			for( int i=0; i<SIZE; ++i ) {
				const U8* p = pixels + scanline*j + i*4;
				Color4U8 color = { p[0], p[1], p[2], p[3] };
				U16 c = Surface::CalcRGBA16( color );
				buffer[SIZE*(SIZE-1-j)+i] = c;
			}
		}
		t->Upload( buffer, SIZE*SIZE*sizeof(buffer[0]) );
		free( pixels );
	}
	else {
		GLASSERT( 0 );
	}	
}