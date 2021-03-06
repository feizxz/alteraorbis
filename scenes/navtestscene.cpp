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

#include "navtestscene.h"

#include "../grinliz/glgeometry.h"

#include "../engine/engine.h"
#include "../engine/ufoutil.h"
#include "../engine/text.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/chitcontext.h"

#include "../game/lumosgame.h"
#include "../game/worldmap.h"
#include "../game/pathmovecomponent.h"
#include "../game/debugpathcomponent.h"
#include "../game/lumoschitbag.h"


using namespace grinliz;
using namespace gamui;


// Draw calls as a proxy for world subdivision:
// 20+20 blocks:
// quad division: 249 -> 407 -> 535
// block growing:  79 -> 135 -> 173
//  32x32          41 ->  97 -> 137
// Wow. Sticking with the growing block algorithm.
// Need to tune size on real map. (Note that the 32vs16 is approaching the same value.)
// 
NavTestScene::NavTestScene( LumosGame* game ) : Scene( game )
{
	debugRay.direction.Zero();
	debugRay.origin.Zero();

	InitStd( &gamui2D, &okay, 0 );

	LayoutCalculator layout = DefaultLayout();
	block.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	block.SetText( "block" );

	block20.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	block20.SetText( "block20" );

	showOverlay.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	showOverlay.SetText( "Over" );
	showOverlay.SetDown();

	toggleBlock.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	toggleBlock.SetText( "TogBlock" );

	showAdjacent.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	showAdjacent.SetText( "Adj" );

	showZonePath.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	showZonePath.SetText( "Region\nPath" );

	textLabel.Init( &gamui2D );

	context.worldMap = new WorldMap( SIZE, SIZE );
	context.worldMap->InitCircle();
	context.engine = new Engine( game->GetScreenportMutable(), game->GetDatabase(), context.worldMap );
	context.worldMap->AttachEngine( context.engine, this );

	context.engine->LoadConfigFiles( "./res/particles.xml", "./res/lighting.xml" );

	context.chitBag = new LumosChitBag( context, 0 );

	Rectangle2I b;
	b.Set( 0, 0, context.worldMap->Width()-1, context.worldMap->Height()-1 );
	context.worldMap->ShowRegionOverlay( b );

	context.engine->CameraLookAt( 10, 10, 40 );
	tapMark.Zero();

	for( int i=0; i<NUM_CHITS; ++i ) {
		chit[i] = context.chitBag->NewChit();
		chit[i]->Add( new RenderComponent( "humanFemale" ) );
		chit[i]->Add( new PathMoveComponent() );
		chit[i]->Add( new DebugPathComponent() );
		chit[i]->SetPosition( 10.0f + (float)i*2.f, 0.0f, 10.0f );
	}
}


NavTestScene::~NavTestScene()
{
	delete context.chitBag;
	delete context.engine;
	context.worldMap->AttachEngine( 0, 0 );
	delete context.worldMap;
}


void NavTestScene::Resize()
{
//	LumosGame* lumosGame = static_cast<LumosGame*>( game );
	PositionStd( &okay, 0 );
	LayoutCalculator layout = DefaultLayout();
	layout.PosAbs( &block, 1, -1 );
	layout.PosAbs( &block20, 2, -1 );

	layout.PosAbs( &showOverlay,   0, -2 );
	layout.PosAbs( &showAdjacent, 1, -2 );
	layout.PosAbs( &showZonePath, 2, -2 );
	layout.PosAbs( &toggleBlock, 3, -2 );

	layout.PosAbs( &textLabel, 0, -3 );
}


void NavTestScene::Zoom( int style, float delta )
{
	if ( style == GAME_ZOOM_PINCH )
		context.engine->SetZoom( context.engine->GetZoom() *( 1.0f+delta) );
	else
		context.engine->SetZoom( context.engine->GetZoom() + delta );
}


void NavTestScene::MoveCamera(float dx, float dy)
{
	MoveImpl(dx, dy, context.engine);
}


void NavTestScene::Pan(int action, const grinliz::Vector2F& view, const grinliz::Ray& world)
{
	Process3DTap(action, view, world, context.engine);
}


void NavTestScene::Rotate( float degrees ) 
{
	context.engine->camera.Orbit( degrees );
}


void NavTestScene::MouseMove( const grinliz::Vector2F& view, const grinliz::Ray& world )
{
	debugRay = world;
}


bool NavTestScene::MapGridBlocked( int x, int y )
{
	return blocks.IsSet(x,y) ? true : false;
}


bool NavTestScene::Tap( int action, const grinliz::Vector2F& view, const grinliz::Ray& world )				
{
	bool uiHasTap = ProcessTap( action, view, world );

	if ( !uiHasTap && action == GAME_TAP_UP) {
//		Vector3F oldMark = tapMark;

		Matrix4 mvpi;
		Ray ray;
		game->GetScreenport().ViewProjectionInverse3D( &mvpi );
		context.engine->RayFromViewToYPlane( view, mvpi, &ray, &tapMark );
		tapMark.y += 0.1f;

		char buf[40];
		SNPrintf( buf, 40, "xz = %.1f,%.1f nSubZ=%d", tapMark.x, tapMark.z, -1 ); //context.worldMap->NumRegions() );
		textLabel.SetText( buf );

		if ( toggleBlock.Down() ) {
			Vector2I d = { (int)tapMark.x, (int)tapMark.z };
			if ( blocks.IsSet( d.x, d.y ) )
				blocks.Clear( d.x, d.y );
			else
				blocks.Set( d.x, d.y );
			context.worldMap->UpdateBlock( d.x, d.y );
		}
		else {
			// Move to the marked location.
			Vector2F d = { tapMark.x, tapMark.z };
			PathMoveComponent* pmc = static_cast<PathMoveComponent*>( chit[0]->GetComponent( "PathMoveComponent" ) );
			GLASSERT( pmc );
			pmc->QueueDest( d );
		}

		if ( showAdjacent.Down() ) {
			context.worldMap->ShowAdjacentRegions( tapMark.x, tapMark.z );
		}
	}
	return true;
}


void NavTestScene::ItemTapped( const gamui::UIItem* item )
{
	int makeBlocks = 0;

	if ( item == &okay ) {
		game->PopScene();
	}
	else if ( item == &block ) {
		makeBlocks = 1;
	}
	else if ( item == &block20 ) {
		makeBlocks = 20;
	}
	else if ( item == &showZonePath ) {
		PathMoveComponent* pmc = static_cast<PathMoveComponent*>( chit[0]->GetComponent( "PathMoveComponent" ) );
		if ( pmc ) 
			pmc->SetPathDebugging( showZonePath.Down() );
	}
	else if ( item == &showOverlay ) {
		Rectangle2I b;
		if ( showOverlay.Down() )
			b.Set( 0, 0, context.worldMap->Width()-1, context.worldMap->Height()-1 );
		else
			b.Set( 0, 0, 0, 0 );

		context.worldMap->ShowRegionOverlay( b );
	}

	while ( makeBlocks ) {
		Rectangle2I b = context.worldMap->Bounds();
		int x = random.Rand( b.Width() );
		int y = random.Rand( b.Height() );
		if ( context.worldMap->IsLand( x, y ) && !blocks.IsSet(x,y) ) {
			blocks.Set( x, y );
			--makeBlocks;
			context.worldMap->UpdateBlock( x, y );
		}
	}
}


void NavTestScene::DoTick( U32 deltaTime )
{
	context.chitBag->DoTick( deltaTime );
}


void NavTestScene::DrawDebugText()
{
	UFOText* ufoText = UFOText::Instance();
	if ( debugRay.direction.x ) {
		Model* root = context.engine->IntersectModel( debugRay.direction, debugRay.origin, FLT_MAX, TEST_TRI, 0, 0, 0, 0 );
		int y = 16;
		Chit* chit = root ? root->userData : 0;
		if ( chit ) {
			GLString str;
			chit->DebugStr( &str );
			ufoText->Draw( 0, y, "%s", str.c_str() );
			y += 16;
		}
	}
}


void NavTestScene::Draw3D( U32 deltaTime )
{
	context.engine->Draw( deltaTime );

	FlatShader flat;
	flat.SetColor( 1, 0, 0 );
	Vector3F delta = { 0.2f, 0, 0.2f };
	
	GPUDevice::Instance()->DrawQuad( flat, 0, tapMark-delta, tapMark+delta, false );
}
