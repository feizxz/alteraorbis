#include "gamescene.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/cameracomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/istringconst.h"

#include "../game/lumosgame.h"
#include "../game/lumoschitbag.h"
#include "../game/sim.h"
#include "../game/pathmovecomponent.h"
#include "../game/worldmap.h"
#include "../game/worldinfo.h"
#include "../game/aicomponent.h"
#include "../game/reservebank.h"
#include "../game/workqueue.h"
#include "../game/mapspatialcomponent.h"

#include "../engine/engine.h"
#include "../engine/text.h"

#include "../script/procedural.h"
#include "../script/corescript.h"
#include "../script/itemscript.h"

using namespace grinliz;
using namespace gamui;

static const float DEBUG_SCALE = 1.0f;
static const float MINI_MAP_SIZE = 150.0f*DEBUG_SCALE;
static const float MARK_SIZE = 6.0f*DEBUG_SCALE;

#define USE_MOUSE_MOVE_SELECTION

GameScene::GameScene( LumosGame* game ) : Scene( game )
{
	fastMode = false;
	simTimer = 0;
	simPS = 1.0f;
	simCount = 0;
	targetChit = 0;
	possibleChit = 0;
	infoID = 0;
	selectionModel = 0;
	buildActive = NO_BUILD;
	voxelInfoID.Zero();
	lumosGame = game;
	game->InitStd( &gamui2D, &okay, 0 );
	sim = new Sim( lumosGame );

	Load();

	Vector3F delta = { 14.0f, 14.0f, 14.0f };
	Vector3F target = { (float)sim->GetWorldMap()->Width() *0.5f, 0.0f, (float)sim->GetWorldMap()->Height() * 0.5f };
	if ( sim->GetPlayerChit() ) {
		target = sim->GetPlayerChit()->GetSpatialComponent()->GetPosition();
		CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
		cc->SetTrack( sim->GetPlayerChit()->ID() );
	}
	sim->GetEngine()->CameraLookAt( target + delta, target );
	
	RenderAtom atom;
	minimap.Init( &gamui2D, atom, false );
	minimap.SetSize( MINI_MAP_SIZE, MINI_MAP_SIZE );
	minimap.SetCapturesTap( true );

	atom = lumosGame->CalcPaletteAtom( PAL_TANGERINE*2, PAL_ZERO );
	playerMark.Init( &gamui2D, atom, true );
	playerMark.SetSize( MARK_SIZE, MARK_SIZE );

	static const char* serialText[NUM_SERIAL_BUTTONS] = { "Save", "Load", "Cycle" }; 
	for( int i=0; i<NUM_SERIAL_BUTTONS; ++i ) {
		serialButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		serialButton[i].SetText( serialText[i] );
	}

	freeCameraButton.Init( &gamui2D, game->GetButtonLook(0) );
	freeCameraButton.SetText( "Free\nCamera" );

	static const char* buildButtonText[NUM_BUILD_BUTTONS] = { 
		"None", "Clear", "Ice", 
		"News\nKiosk", "Media\nKiosk", "Commerce\nKiosk", "Social\nKiosk", "Vault",
		"Rotate"
	};
	for( int i=0; i<NUM_BUILD_BUTTONS; ++i ) {
		buildButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		buildButton[i].SetText( buildButtonText[i] );
		buildButton[0].AddToToggleGroup( &buildButton[i] );
	}

	createWorkerButton.Init( &gamui2D, game->GetButtonLook(0) );
	createWorkerButton.SetText( "Create\nWorker" );

	ejectButton.Init( &gamui2D, game->GetButtonLook(0) );
	ejectButton.SetText( "Eject\nCore" );
	
	allRockButton.Init( &gamui2D, game->GetButtonLook(0) );
	allRockButton.SetText( "All Rock" );

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		newsButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		newsButton[i].SetSize( NEWS_BUTTON_WIDTH, NEWS_BUTTON_HEIGHT );
		newsButton[i].SetText( "news" );
	}
	clearButton.Init( &gamui2D, game->GetButtonLook(0) );
	clearButton.SetSize( NEWS_BUTTON_WIDTH, NEWS_BUTTON_HEIGHT );
	clearButton.SetText( "Clear" );

	RenderAtom nullAtom;
	faceImage.Init( &gamui2D, nullAtom, true );
	faceImage.SetSize( 100, 100 );
	SetFace();

	RenderAtom green = LumosGame::CalcPaletteAtom( 1, 3 );	
	RenderAtom grey  = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue   = LumosGame::CalcPaletteAtom( 8, 0 );	

	healthBar.Init( &gamui2D, 10, green, grey );
	ammoBar.Init( &gamui2D, 10, blue, grey );
	shieldBar.Init( &gamui2D, 10, blue, grey );

	dateLabel.Init( &gamui2D );
	goldLabel.Init( &gamui2D );
	xpLabel.Init( &gamui2D );
}


GameScene::~GameScene()
{
	if ( selectionModel ) {
		sim->GetEngine()->FreeModel( selectionModel );
	}
	delete sim;
}


void GameScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );
	
	LayoutCalculator layout = static_cast<LumosGame*>(game)->DefaultLayout();
	for( int i=0; i<NUM_SERIAL_BUTTONS; ++i ) {
		layout.PosAbs( &serialButton[i], i, -2 );
	}
	layout.PosAbs( &allRockButton, NUM_SERIAL_BUTTONS, -2 );
	layout.PosAbs( &freeCameraButton, 0, -3 );

	for( int i=0; i<NUM_BUILD_BUTTONS; ++i ) {
		int x = i/4;
		int y = i - x*4;
		layout.PosAbs( &buildButton[i], x, y+2 );
	}
	layout.PosAbs( &createWorkerButton, 0, 6 );
	layout.PosAbs( &ejectButton, 1, 6 );

	const Screenport& port = lumosGame->GetScreenport();
	minimap.SetPos( port.UIWidth()-MINI_MAP_SIZE, 0 );

	faceImage.SetPos( minimap.X()-faceImage.Width(), 0 );

	layout.SetSize( faceImage.Width(), 10.0f );
	layout.SetSpacing( 5.0f );
	layout.SetOffset( faceImage.X(), faceImage.Y()+faceImage.Height() );
	layout.SetGutter( 0, 5.0f );

	layout.PosAbs( &healthBar,	0, 0 );
	layout.PosAbs( &ammoBar,	0, 1 );
	layout.PosAbs( &shieldBar,  0, 2 );

	dateLabel.SetPos( faceImage.X()-faceImage.Width()*2.0f, 0 );
	goldLabel.SetPos( dateLabel.X(), dateLabel.Y() + gamui2D.GetTextHeight() );
	xpLabel.SetPos(   goldLabel.X(), goldLabel.Y() + gamui2D.GetTextHeight() );

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		newsButton[i].SetPos( port.UIWidth()- (NEWS_BUTTON_WIDTH), MINI_MAP_SIZE + (NEWS_BUTTON_HEIGHT+2)*i );
	}
	clearButton.SetPos( newsButton[0].X() - clearButton.Width(), newsButton[0].Y() );

	bool visible = game->DebugUIEnabled();
	allRockButton.SetVisible( visible );
	serialButton[CYCLE].SetVisible( visible );
}


void GameScene::SetFace()
{
	Chit* chit = sim->GetPlayerChit();
	if ( chit ) {
		int id = chit->ID();
		GLASSERT( chit->GetItem() );
		const GameItem& item = *chit->GetItem();
		
		ProcRenderInfo info;
		FaceGen faceGen( true );
		faceGen.Render( id, &info );

		RenderAtom procAtom( (const void*) (UIRenderer::RENDERSTATE_UI_CLIP_XFORM_MAP), 
							 info.texture,
							 info.te.uv.x, info.te.uv.y, info.te.uv.z, info.te.uv.w );
		faceImage.SetAtom( procAtom );
		faceImage.SetVisible( true );

		uiRenderer.uv[0]			= info.te.uv;
		uiRenderer.uvClip[0]		= info.te.clip;
		uiRenderer.colorXForm[0]	= info.color;
	}
	else {
		faceImage.SetVisible( false );
	}

#if 0 
	faceImage.SetVisible( true );
	RenderAtom atom = LumosGame::CalcPaletteAtom( 0, 0 );
	faceImage.SetAtom( atom );
#endif
}


void GameScene::SetBars()
{
	RenderAtom orange = LumosGame::CalcPaletteAtom( 4, 0 );
	RenderAtom grey   = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue   = LumosGame::CalcPaletteAtom( 8, 0 );	

	Chit* chit = sim->GetPlayerChit();
	if ( chit && chit->GetItem() ) {
		const GameItem* item = chit->GetItem();
		healthBar.SetRange( item->HPFraction() );

		IShield* ishield			= 0;
		IRangedWeaponItem* iweapon	= 0;

		ItemComponent* itemComp = chit->GetItemComponent();
		if ( itemComp ) {
			ishield	= itemComp->GetShield();
			iweapon	= itemComp->GetRangedWeapon(0);
		}

		if ( iweapon ) {
			float r = 0;
			if ( iweapon->GetItem()->Reloading() ) {
				r = iweapon->GetItem()->ReloadFraction();
				ammoBar.SetLowerAtom( orange );
			}
			else {
				r = iweapon->GetItem()->RoundsFraction();
				ammoBar.SetLowerAtom( blue );
			}
			ammoBar.SetRange( Clamp( r, 0.f, 1.f ) );
		}
		else {
			ammoBar.SetRange( 0 );
		}

		if ( ishield ) {
			float r = ishield->GetItem()->RoundsFraction();
			shieldBar.SetRange( Clamp( r, 0.f, 1.0f ));
		}
		else {
			shieldBar.SetRange( 0 );
		}
	}

	healthBar.SetVisible( chit != 0 );
	ammoBar.SetVisible( chit != 0 );
	shieldBar.SetVisible( chit != 0 );
}

void GameScene::Save()
{
	const char* datPath = game->GamePath( "map", 0, "dat" );
	const char* gamePath = game->GamePath( "game", 0, "dat" );
	sim->Save( datPath, gamePath );
}


void GameScene::Load()
{
	const char* datPath  = lumosGame->GamePath( "map", 0, "dat" );
	const char* gamePath = lumosGame->GamePath( "game", 0, "dat" );

	if ( lumosGame->HasFile( gamePath )) {
		sim->Load( datPath, gamePath );
	}
	else {
		sim->Load( datPath, 0 );
	}
	SetFace();
}


void GameScene::Zoom( int style, float delta )
{
	Engine* engine = sim->GetEngine();
	if ( style == GAME_ZOOM_PINCH )
		engine->SetZoom( engine->GetZoom() *( 1.0f+delta) );
	else
		engine->SetZoom( engine->GetZoom() + delta );

}

void GameScene::Rotate( float degrees )
{
	sim->GetEngine()->camera.Orbit( degrees );
}


void GameScene::MouseMove( const grinliz::Vector2F& view, const grinliz::Ray& world )
{
	// --- Info and debugging info. ---
	ModelVoxel mv = this->ModelAtMouse( view, sim->GetEngine(), TEST_TRI, 0, 0, 0, 0 );
	MoveModel( mv.model ? mv.model->userData : 0 );

	if ( mv.model && mv.model->userData ) {
		infoID = mv.model->userData->ID();
	}
	if ( mv.VoxelHit() ) {
		voxelInfoID = mv.Voxel2();
	}

	SetSelectionModel( view );
}


void GameScene::SetSelectionModel( const grinliz::Vector2F& view )
{
#ifdef USE_MOUSE_MOVE_SELECTION
	Vector3F at = { 0, 0, 0 };
	ModelVoxel mv = this->ModelAtMouse( view, sim->GetEngine(), TEST_TRI, 0, 0, 0, &at );
	Vector2I pos2i = { (int)at.x, (int)at.z };

	// --- Selection display. (Only in desktop interface.)
	Engine* engine = sim->GetEngine();
	float size = 1.0f;
	int height = 1;
	const char* name = "";
	if ( buildActive ) {
		// Which should we use?
		switch ( buildActive ) {
		case CLEAR_ROCK: 
			{
				const WorldGrid& wg = sim->GetWorldMap()->GetWorldGrid( pos2i.x, pos2i.y );
				switch ( wg.Height() ) {
				case 3:	name = "clearMarker3";	break;
				case 2:	name = "clearMarker2";	break;
				default:	name = "clearMarker1";	break;
				}
			}
			break;
		case BUILD_ICE:
		case BUILD_KIOSK_N:
		case BUILD_KIOSK_M:
		case BUILD_KIOSK_C:
		case BUILD_KIOSK_S:
			name = "buildMarker1";
			break;
		case BUILD_VAULT:
			size = 2.0f;
			name = "buildMarker2";
			break;
		default:
			break;
		}
	}
	if ( *name ) {
		// Make the model current.
		if ( !selectionModel || !StrEqual( selectionModel->GetResource()->Name(), name )) {
			if ( selectionModel ) {
				engine->FreeModel( selectionModel );
			}
			selectionModel = engine->AllocModel( name );
			GLASSERT( selectionModel );
		}
	}
	else {
		if ( selectionModel ) {
			engine->FreeModel( selectionModel );
			selectionModel = 0;
		}
	}
	if ( selectionModel ) {
		// Move away from the eye so that the new color is visible.
		Vector3F pos = at;
		pos.x = floorf( at.x ) + size*0.5f;
		pos.z = floorf( at.z ) + size*0.5f;
		
		Vector3F dir = pos - engine->camera.PosWC();
		dir.Normalize();
		pos = pos + dir*0.01f;

		selectionModel->SetPos( pos );
		Vector4F color = { 1,1,1, 0.3f };
		selectionModel->SetColor( color );
	}
#endif
}


void GameScene::ClearTargetFlags()
{
	Chit* target = sim->GetChitBag()->GetChit( targetChit );
	if ( target && target->GetRenderComponent() ) {
		target->GetRenderComponent()->Deco( 0, 0, 0 );
	}
	target = sim->GetChitBag()->GetChit( possibleChit );
	if ( target && target->GetRenderComponent() ) {
		target->GetRenderComponent()->Deco( 0, 0, 0 );
	}
	targetChit = possibleChit = 0;
}


void GameScene::MoveModel( Chit* target )
{
	Chit* player = sim->GetPlayerChit();
	if ( !player ) {
		ClearTargetFlags();
		return;
	}

	Chit* oldTarget = sim->GetChitBag()->GetChit( possibleChit );
	if ( oldTarget ) {
		RenderComponent* rc = oldTarget->GetRenderComponent();
		if ( rc ) {
			rc->Deco( 0, 0, 0 );
		}
	}
	Chit* focusedTarget = sim->GetChitBag()->GetChit( targetChit );
	if ( target && target != focusedTarget ) {
		AIComponent* ai = player->GetAIComponent();
		if ( ai && ai->GetTeamStatus( target ) == AIComponent::ENEMY ) {
			possibleChit = 0;
			RenderComponent* rc = target->GetRenderComponent();
			if ( rc ) {
				rc->Deco( "possibleTarget", RenderComponent::DECO_FOOT, INT_MAX );
				possibleChit = target->ID();
			}
		}
	}
}


void GameScene::TapModel( Chit* target )
{
	if ( !target ) {
		return;
	}
	Chit* player = sim->GetPlayerChit();
	if ( !player ) {
		ClearTargetFlags();

		AIComponent* ai = target->GetAIComponent();
		if ( ai ) {
			ai->EnableDebug( true );
		}
		return;
	}
	AIComponent* ai = player->GetAIComponent();
	if ( ai && ai->GetTeamStatus( target ) == AIComponent::ENEMY ) {
		ai->Target( target, true );
		ClearTargetFlags();

		RenderComponent* rc = target->GetRenderComponent();
		if ( rc ) {
			rc->Deco( "target", RenderComponent::DECO_FOOT, INT_MAX );
			targetChit = target->ID();
		}
	}
}


grinliz::IString GameScene::BuildActiveInfo( int* size )
{
	static const char* name[NUM_BUILD_BUTTONS] = {
		"",	// no build 
		"", // clear
		"",	// ice
		"kiosk.n",
		"kiosk.m",
		"kiosk.c",
		"kiosk.s",
		"vault",
		"",			// rotate
	};
	IString str;
	if ( *name[buildActive] ) {
		str = StringPool::Intern( name[buildActive], true );

		if ( size ) {
			// Get the size
			*size = 1;
			ItemDefDB::GetProperty( str.c_str(), "size", size );
		}
	}
	return str;
}



void GameScene::Tap( int action, const grinliz::Vector2F& view, const grinliz::Ray& world )
{
	bool uiHasTap = ProcessTap( action, view, world );
	Engine* engine = sim->GetEngine();
	CoreScript* coreMode = sim->GetChitBag()->IsBoundToCore( sim->GetPlayerChit(), true );
	
	if ( coreMode ) {
		CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
		int playerID =  sim->GetPlayerChit() ?  sim->GetPlayerChit()->ID() : 0;
		if ( cc && cc->Tracking() && cc->Tracking() == playerID ) {
			cc->SetTrack( 0 );
		}
	}
	enable3DDragging = (sim->GetPlayerChit() == 0) || (coreMode != 0);
	
	buildActive = NO_BUILD;
	if ( coreMode ) {
		for( int i=1; i<NUM_BUILD_BUTTONS; ++i ) {
			if ( buildButton[i].Down() ) {
				buildActive = i;
				break;
			}
		}
	}
	SetSelectionModel( view );

	if ( !uiHasTap ) {
		Vector3F atModel = { 0, 0, 0 };
		Vector3F plane   = { 0, 0, 0 };
		ModelVoxel mv = ModelAtMouse( view, sim->GetEngine(), TEST_HIT_AABB, 0, MODEL_CLICK_THROUGH, 0, &plane );
		GLASSERT( plane.x > 0 && plane.z > 0 );
		Vector2I plane2i = { (int)plane.x, (int)plane.z };

		bool tap = Process3DTap( action, view, world, sim->GetEngine() );

		if ( tap ) {

			if ( coreMode ) {
				WorkQueue* wq = coreMode->GetWorkQueue();
				GLASSERT( wq );
				RemovableFilter removableFilter;

				if ( buildActive == CLEAR_ROCK ) {
#ifdef USE_MOUSE_MOVE_SELECTION
					wq->AddClear( plane2i );
#else
					if ( mv.VoxelHit()) {
						wq->Add( WorkQueue::CLEAR, mv.Voxel2(), IString() );
					}
					else if ( mv.ModelHit() && mv.model->userData && removableFilter.Accept( mv.model->userData )) {
						MapSpatialComponent* msc = GET_SUB_COMPONENT( mv.model->userData, SpatialComponent, MapSpatialComponent );
						GameItem* gameItem = mv.model->userData->GetItem();
						GLASSERT( msc );
						if ( msc && gameItem  ) {
							wq->Add( WorkQueue::CLEAR, msc->MapPosition(), gameItem->name );
						}
					}
#endif
				}
				else if ( buildActive == NO_BUILD ) {
#ifdef USE_MOUSE_MOVE_SELECTION
					wq->Remove( plane2i );
#else
					if ( mv.VoxelHit() ) {
						// Clear a voxel.
						wq->Remove( mv.Voxel2() );
					}
					else if ( mv.ModelHit() ) {
						MapSpatialComponent* msc = GET_SUB_COMPONENT( mv.model->userData, SpatialComponent, MapSpatialComponent );
						if ( msc ) {
							wq->Remove( msc->MapPosition() );
						}
					}
#endif
				}
				else if ( buildActive == ROTATE ) {
					if ( mv.ModelHit() ) {
						MapSpatialComponent* msc = GET_SUB_COMPONENT( mv.model->userData, SpatialComponent, MapSpatialComponent );
						if ( msc ) {
							float r = msc->GetYRotation();
							r += 90.0f;
							r = NormalizeAngleDegrees( r );
							msc->SetYRotation( r );
						}
					}
				}
#ifdef USE_MOUSE_MOVE_SELECTION
				else {
#else
				else if ( !mv.VoxelHit() ) {
#endif
					int size = 1;
					IString name = BuildActiveInfo( &size );
					wq->AddBuild( plane2i, name );
				}
			}
			
			/*
			if ( mv.VoxelHit() ) {
				// clicked on a rock. Melt away!
				Chit* player = sim->GetPlayerChit();
				if ( player && player->GetAIComponent() ) {
					player->GetAIComponent()->RockBreak( mv.Voxel2() );
					return;
				}
			}
			*/

			int tapMod = lumosGame->GetTapMod();

			if ( tapMod == 0 ) {
				Chit* playerChit = sim->GetPlayerChit();
				if ( mv.model ) {
					TapModel( mv.model->userData );
				}
				else if ( playerChit ) {
					Vector2F dest = { plane.x, plane.z };
					DoDestTapped( dest );
				}
				else {
					sim->GetEngine()->CameraLookAt( plane.x, plane.z );
				}
			}
			else if ( tapMod == GAME_TAP_MOD_CTRL ) {

				Vector2I v = { (int)plane.x, (int)plane.z };
				sim->CreatePlayer( v, 0 ); 
				SetFace();
#if 0
				sim->CreateVolcano( (int)at.x, (int)at.z, 6 );
				sim->CreatePlant( (int)at.x, (int)at.z, -1 );
#endif
			}
			else if ( tapMod == GAME_TAP_MOD_SHIFT ) {
				for( int i=0; i<NUM_PLANT_TYPES; ++i ) {
					sim->CreatePlant( (int)plane.x+i, (int)plane.z, i );
				}
			}
		}
	}
}


void GameScene::ItemTapped( const gamui::UIItem* item )
{
	Vector2F dest = { -1, -1 };

	if ( item == &okay ) {
		game->PopScene();
	}
	else if ( item == &minimap ) {
		float x=0, y=0;
		gamui2D.GetRelativeTap( &x, &y );
		GLOUTPUT(( "minimap tapped nx=%.1f ny=%.1f\n", x, y ));

		Engine* engine = sim->GetEngine();
		dest.x = x*(float)engine->GetMap()->Width();
		dest.y = y*(float)engine->GetMap()->Height();
	}
	else if ( item == &serialButton[SAVE] ) {
		Save();
	}
	else if ( item == &serialButton[LOAD] ) {
		delete sim;
		sim = new Sim( lumosGame );
		Load();
	}
	else if ( item == &serialButton[CYCLE] ) {
		Save();
		delete sim;
		sim = new Sim( lumosGame );
		Load();
	}
	else if ( item == &allRockButton ) {
		sim->SetAllRock();
	}
	else if ( item == &createWorkerButton ) {
		Chit* playerChit = sim->GetPlayerChit();
		CoreScript* coreMode = sim->GetChitBag()->IsBoundToCore( playerChit, true );
		if ( coreMode ) {
			int team = playerChit->GetItem()->primaryTeam;
			sim->GetChitBag()->NewWorkerChit( playerChit->GetSpatialComponent()->GetPosition(), team );
		}
	}
	else if ( item == &ejectButton ) {
		// Should be disabled unless we are bound to a core.
		Chit* playerChit = sim->GetPlayerChit();
		if ( playerChit ) {
			CoreScript* coreMode   = sim->GetChitBag()->IsBoundToCore( playerChit, true );
			if ( coreMode ) {
				GLASSERT( !playerChit->GetMoveComponent() );
				playerChit->Add( new PathMoveComponent( sim->GetWorldMap() ));
			}
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( cc ) {
				cc->SetTrack( playerChit->ID() );
			}
		}
	}

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {

		if ( item == &newsButton[i] ) {
			if ( freeCameraButton.Down() ) {
				const NewsEvent* ne = sim->GetChitBag()->News() + i;
				CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
				if ( cc && ne->chitID ) {
					cc->SetTrack( ne->chitID );
				}
				else {
					sim->GetEngine()->CameraLookAt( ne->pos.x, ne->pos.y );
				}
			}
		}
	}
	if ( item == &clearButton ) {
		if ( freeCameraButton.Down() ) {
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( cc  ) {
				cc->SetTrack( 0 );
			}
		}
	}
	if ( item == &freeCameraButton ) {
		CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
		if ( cc ) {
			if ( freeCameraButton.Down() ) {
				cc->SetTrack( 0 );
			}
			else {
				Chit* playerChit = sim->GetPlayerChit();
				CoreScript* coreMode = sim->GetChitBag()->IsBoundToCore( playerChit, true );
				if ( playerChit ) {
					if ( coreMode ) {
						Vector3F pos = playerChit->GetSpatialComponent()->GetPosition();
						pos.y = sim->GetEngine()->camera.PosWC().y;
						//cc->SetPanTo( pos, 100.0f );
						sim->GetEngine()->CameraLookAt( pos.x, pos.z );
					}
					else {
						cc->SetTrack( playerChit->ID() );
					}
				}
			}
		}
	}

	if ( dest.x >= 0 ) {
		DoDestTapped( dest );
	}
}


void GameScene::DoDestTapped( const Vector2F& _dest )
{
	Vector2F dest = _dest;

	if ( !freeCameraButton.Down() ) {
		Chit* chit = sim->GetPlayerChit();
		if ( chit ) {
			AIComponent* ai = chit->GetAIComponent();
			GLASSERT( ai );
			if ( ai ) {
				Vector2F pos = chit->GetSpatialComponent()->GetPosition2D();
				// Is this grid travel or normal travel?
				Vector2I currentSector = SectorData::SectorID( pos.x, pos.y );
				Vector2I destSector    = SectorData::SectorID( dest.x, dest.y );
				SectorPort sectorPort;

				// Are we on the grid?
				GridMoveComponent* gmc = GET_SUB_COMPONENT( chit, MoveComponent, GridMoveComponent );
					
				if (    currentSector != destSector		// we want to get on the grid
					 || gmc )							// we are on the grid, and want to change coordinates.
				{
					// Find the nearest port.
					int id = chit->ID();
					SectorPort local = sim->GetWorldMap()->NearestPort( pos );
					if ( local.IsValid() ) {
						Rectangle2I portRect = sim->GetWorldMap()->GetSector( local.sector ).GetPortLoc( local.port );
						dest = SectorData::PortPos( portRect, chit->ID() );
						sectorPort.sector = destSector;
						sectorPort.port   = sim->GetWorldMap()->GetSector( sectorPort.sector ).NearestPort( pos );
					}
				}
				if ( sectorPort.IsValid() ) {
					ai->Move( sectorPort, true );
				}
				else {
					ai->Move( dest, true );
				}
			}
		}
		else {
			sim->GetEngine()->CameraLookAt( dest.x, dest.y );
		}
	}
	else {
		sim->GetEngine()->CameraLookAt( dest.x, dest.y );
	}
}



void GameScene::HandleHotKey( int mask )
{
	if ( mask == GAME_HK_SPACE ) {
		fastMode = !fastMode;
#if 0
		if ( FreeCamera() ) {
			Chit* visitor = sim->GetChitBag()->GetChit( Visitors::Instance()->visitorData[0].id );
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( visitor && cc ) {
				cc->SetTrack( visitor->ID() );
			}
		}
#endif
	}
	else if ( mask == GAME_HK_TOGGLE_COLORS ) {
		static int colorSeed = 0;
		static const char* NAMES[4] = { "kiosk.m", "kiosk.n", "kiosk.c", "kiosk.s" };
		ItemNameFilter filter( NAMES, 4 );
		Vector3F at;
		sim->GetEngine()->CameraLookingAt( &at );
		Vector2F at2 = { at.x, at.z };
		CChitArray queryArr;
		sim->GetChitBag()->QuerySpatialHash( &queryArr, at2, 10.f, 0, &filter );
		for( int i=0; i<queryArr.Size(); ++i ) {
			RenderComponent* rc = queryArr[i]->GetRenderComponent();
			TeamGen gen;
			ProcRenderInfo info;
			gen.Assign( colorSeed, &info );
			rc->SetProcedural( IStringConst::kmain, info );
		}
		++colorSeed;
	}
	else if ( mask == GAME_HK_TOGGLE_PATHING ) {
		Rectangle2I r;
		if ( sim->GetWorldMap()->IsShowingRegionOverlay() ) {
			r.Set( 0, 0, 0, 0 );
		}
		else {
			Vector3F at;
			sim->GetEngine()->CameraLookingAt( &at );
			Vector2I sector = { (int)at.x / SECTOR_SIZE, (int)at.z / SECTOR_SIZE };
			r.Set( sector.x*SECTOR_SIZE, sector.y*SECTOR_SIZE, (sector.x+1)*SECTOR_SIZE-1, (sector.y+1)*SECTOR_SIZE-1 );
		}
		sim->GetWorldMap()->ShowRegionOverlay( r );
	}
	else {
		super::HandleHotKey( mask );
	}
}

/*
class NewsSorter
{
public:
	static bool Less( const NewsEvent& v0, const NewsEvent& v1 )	
	{ 
		int score0 = v0.priority * 30*1000 - (int)v0.time;
		int score1 = v1.priority * 30*1000 - (int)v1.time;
		return score0 < score1; 
	}
};
*/

void GameScene::DoTick( U32 delta )
{
	clock_t startTime = clock();

	sim->DoTick( delta );
	++simCount;

	if ( fastMode ) {
		while( clock() - startTime < 100 ) {
			sim->DoTick( 30 );
			++simCount;
		}
	}

	const NewsEvent* news = sim->GetChitBag()->News();
	int nNews = sim->GetChitBag()->NumNews();

#if 0
	for( int i=0; i<nNews; ++i ) {
		//float h = sim->GetEngine()->camera.PosWC().y;
		if (    news[i].processed == false 
			 && news[i].name == "SectorHerd" 
			 && !sim->GetPlayerChit() ) 
		{
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( cc ) {
				cc->SetTrack( 0 );
				cc->SetTrack( news[i].chitID );
			}
		}
	}
#endif
	sim->GetChitBag()->SetNewsProcessed();
	/*
	localNews.Clear();
	for( int i=0; i<nNews; ++i ) {
		localNews.Push( news[i] );
	}
	Sort<NewsEvent, NewsSorter>( localNews.Mem(), localNews.Size() );
	*/
	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		if ( i < nNews ) {
			//newsButton[i].SetText( localNews[i].name.c_str() );
			newsButton[i].SetText( news[i].name.c_str() );
			newsButton[i].SetEnabled( true );
		}
		else {
			newsButton[i].SetText( "" );
			newsButton[i].SetEnabled( false );
		}
	}

	clock_t endTime = clock();
	simTimer += (int)(endTime-startTime);

	Vector3F lookAt = { 0, 0, 0 };
	sim->GetEngine()->CameraLookingAt( &lookAt );
	Vector2I sector = { (int)lookAt.x / SECTOR_SIZE, (int)lookAt.z / SECTOR_SIZE };
	const SectorData& sd = sim->GetWorldMap()->GetWorldInfo().GetSector( sector );

	CStr<64> str;
	str.Format( "%.2f %s", sim->DateInAge(), sd.name.c_str() );
	dateLabel.SetText( str.c_str() );

	Chit* playerChit = sim->GetPlayerChit();
	if ( !playerChit ) {
		SetFace();
	}
	str.Clear();

	Wallet wallet;
	if ( playerChit && playerChit->GetItemComponent() ) {
		wallet = playerChit->GetItemComponent()->GetWallet();
	}
	str.Format( "Au:%d g:%d r:%d b:%d v:%d", 
				wallet.gold, 
				wallet.crystal[CRYSTAL_GREEN], 
				wallet.crystal[CRYSTAL_RED], 
				wallet.crystal[CRYSTAL_BLUE], 
				wallet.crystal[CRYSTAL_VIOLET] );
	goldLabel.SetText( str.c_str() );

	str.Clear();
	if ( playerChit && playerChit->GetItem() ) {
		const GameStat& stat = playerChit->GetItem()->stats;
		str.Format( "XP:%d Level:%d", stat.Experience(), stat.Level() );
	}
	xpLabel.SetText( str.c_str() );

	SetBars();

	CoreScript* coreMode = sim->GetChitBag()->IsBoundToCore( sim->GetPlayerChit(), true );
	for( int i=0; i<NUM_BUILD_BUTTONS; ++i ) {
		buildButton[i].SetVisible( coreMode != 0 );
	}
	createWorkerButton.SetVisible( coreMode != 0 );
	ejectButton.SetVisible( coreMode != 0 );

	sim->GetEngine()->RestrictCamera( 0 );
}


void GameScene::Draw3D( U32 deltaTime )
{
	sim->Draw3D( deltaTime );

	RenderAtom atom( (const void*)UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE, 
					 (const void*)sim->GetMiniMapTexture(), 
					 0, 0, 1, 1 );
	/* coordinate test. they are correct.
	RenderAtom atom( (const void*)UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE, 
					 (const void*)TextureManager::Instance()->GetTexture( "palette" ),
					 0, 0, 1, 1 );
	*/

	minimap.SetAtom( atom );
	Vector3F lookAt = { 0, 0, 0 };
	sim->GetEngine()->CameraLookingAt( &lookAt );
	Map* map = sim->GetEngine()->GetMap();
	
	float x = minimap.X() + Lerp( 0.f, minimap.Width(),  lookAt.x / (float)map->Width() );
	float y = minimap.Y() + Lerp( 0.f, minimap.Height(), lookAt.z / (float)map->Height() );

	playerMark.SetCenterPos( x, y );
}


void GameScene::DrawDebugText()
{
	DrawDebugTextDrawCalls( 16, sim->GetEngine() );

	UFOText* ufoText = UFOText::Instance();
	Chit* chit = sim->GetPlayerChit();
	Engine* engine = sim->GetEngine();
	LumosChitBag* chitBag = sim->GetChitBag();
	Vector3F at = { 0, 0, 0 };
	int y = 32;

	if ( chit && chit->GetSpatialComponent() ) {
		const Vector3F& v = chit->GetSpatialComponent()->GetPosition();
		at = v;
		ufoText->Draw( 0, y, "Player: %.1f, %.1f, %.1f  Camera: %.1f %.1f %.1f", 
			           v.x, v.y, v.z,
					   engine->camera.PosWC().x, engine->camera.PosWC().y, engine->camera.PosWC().z );
		y += 16;
	}
	else {
		engine->CameraLookingAt( &at );
	}

	if ( simTimer > 1000 ) {
		simPS = 1000.0f * (float)simCount / (float)simTimer;
		simTimer = 0;
		simCount = 0;
	}

	Wallet w = ReserveBank::Instance()->GetWallet();
	ufoText->Draw( 0, y,	"Date=%.2f %s. Sim/S=%.1f x%.1f ticks=%d/%d Reserve Au=%d r%dg%dv%d", 
							sim->DateInAge(),
							fastMode ? "fast" : "norm", 
							simPS,
							simPS / 30.0f,
							chitBag->NumTicked(), chitBag->NumChits(),
							w.gold, w.crystal[0], w.crystal[1], w.crystal[2] ); 
	y += 16;

	int typeCount[NUM_PLANT_TYPES];
	for( int i=0; i<NUM_PLANT_TYPES; ++i ) {
		typeCount[i] = 0;
		for( int j=0; j<MAX_PLANT_STAGES; ++j ) {
			typeCount[i] += chitBag->census.plants[i][j];
		}
	}
	int stageCount[MAX_PLANT_STAGES];
	for( int i=0; i<MAX_PLANT_STAGES; ++i ) {
		stageCount[i] = 0;
		for( int j=0; j<NUM_PLANT_TYPES; ++j ) {
			stageCount[i] += chitBag->census.plants[j][i];
		}
	}

	ufoText->Draw( 0, y,	"Plants type: %d %d %d %d %d %d %d %d stage: %d %d %d %d", 
									typeCount[0], typeCount[1], typeCount[2], typeCount[3], typeCount[4], typeCount[5], typeCount[6], typeCount[7],
									stageCount[0], stageCount[1], stageCount[2], stageCount[3] );
	y += 16;

	micropather::CacheData cacheData;
	Vector2I sector = { (int)at.x/SECTOR_SIZE, (int)at.z/SECTOR_SIZE };
	sim->GetWorldMap()->PatherCacheHitMiss( sector, &cacheData );
	ufoText->Draw( 0, y, "Pather(%d,%d) kb=%d/%d %.2f cache h:m=%d:%d %.2f", 
						  sector.x, sector.y,
						  cacheData.nBytesUsed/1024,
						  cacheData.nBytesAllocated/1024,
						  cacheData.memoryFraction,
						  cacheData.hit,
						  cacheData.miss,
						  cacheData.hitFraction );
	y += 16;

	Chit* info = sim->GetChitBag()->GetChit( infoID );
	if ( info ) {
		GLString str;
		info->DebugStr( &str );
		ufoText->Draw( 0, y, "id=%d: %s", infoID, str.c_str() );
		y += 16;
	}
	if ( !voxelInfoID.IsZero() ) {
		const WorldGrid& wg = sim->GetWorldMap()->GetWorldGrid( voxelInfoID.x, voxelInfoID.y );
		ufoText->Draw( 0, y, "voxel=%d,%d hp=%d/%d", voxelInfoID.x, voxelInfoID.y, wg.HP(), wg.TotalHP() );
		y += 16;
	}
}
