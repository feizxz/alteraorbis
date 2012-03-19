#include "particlescene.h"
#include "../game/lumosgame.h"
#include "../engine/engine.h"
#include "../xegame/testmap.h"
#include "../tinyxml2/tinyxml2.h"

using namespace gamui;
using namespace grinliz;
using namespace tinyxml2;


ParticleScene::ParticleScene( LumosGame* game ) : Scene( game )
{
	game->InitStd( &gamui2D, &okay, 0 );
	engine = new Engine( game->GetScreenportMutable(), game->GetDatabase() );
	testMap = new TestMap( 12, 12 );
	Color3F c = { 0.5f, 0.5f, 0.5f };
	testMap->SetColor( c );
	engine->SetMap( testMap );
	engine->CameraLookAt( 6, 6, 10 );

	Load();
}


ParticleScene::~ParticleScene()
{
	Clear();
	delete engine;
	delete testMap;
}


void ParticleScene::Clear()
{
	for( int i=0; i<buttonArr.Size(); ++i ) {
		delete buttonArr[i];
	}
	buttonArr.Clear();
}


void ParticleScene::Resize()
{
	LumosGame* lumosGame = static_cast<LumosGame*>( game );
	lumosGame->PositionStd( &okay, 0 );

	LayoutCalculator layout = lumosGame->DefaultLayout();
	for( int i=0; i<buttonArr.Size(); ++i ) {
		layout.PosAbs( buttonArr[i], i*2, 0 );
	}
}


void ParticleScene::Load()
{
	Clear(); 

	LumosGame* lumosGame = static_cast<LumosGame*>( game );
	LayoutCalculator layout = lumosGame->DefaultLayout();
	const ButtonLook& look = lumosGame->GetButtonLook( LumosGame::BUTTON_LOOK_STD );

	XMLDocument doc;
	doc.LoadFile( "particles.xml" );

	// FIXME: switch to safe version.
	for( const XMLElement* partEle = doc.FirstChildElement( "particles" )->FirstChildElement( "particle" );
		 partEle;
		 partEle = partEle->NextSiblingElement( "particle" ) )
	{
		ParticleDef pd;
		pd.Load( partEle );
		particleDefArr.Push( pd );

		Button* button = 0;
		if ( pd.time == ParticleDef::CONTINUOUS ) {
			button = new ToggleButton( &gamui2D, look );
		}
		else {
			button = new PushButton( &gamui2D, look );
		}
		button->SetSize( layout.Width()*2, layout.Height() );
		button->SetText( pd.name.c_str() );
		buttonArr.Push( button );
	}
	Resize();
}


void ParticleScene::ItemTapped( const gamui::UIItem* item )
{
	GLASSERT( buttonArr.Size() == particleDefArr.Size() );
	if ( item == &okay ) {
		game->PopScene();
	}
	for( int i=0; i<buttonArr.Size(); ++i ) {
		if ( buttonArr[i] == item ) {
			Vector3F pos = { 6.f, 0.f, 6.f };
			Vector3F normal = { 0, 1, 0 };
			ParticleSystem::Instance()->EmitPD( particleDefArr[i], pos, normal );
		}
	}
}


void ParticleScene::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
}
