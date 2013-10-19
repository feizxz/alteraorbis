#include "characterscene.h"
#include "../game/lumosgame.h"
#include "../engine/engine.h"
#include "../xegame/itemcomponent.h"
#include "../script/procedural.h"
#include "../script/battlemechanics.h"

using namespace gamui;
using namespace grinliz;

static const float NEAR = 0.1f;
static const float FAR  = 10.0f;

CharacterScene::CharacterScene( LumosGame* game, CharacterSceneData* csd ) : Scene( game ), screenport( game->GetScreenport() )
{
	this->lumosGame = game;
	this->itemComponent = csd->itemComponent;
	model = 0;

	screenport.SetNearFar( NEAR, FAR );
	engine = new Engine( &screenport, lumosGame->GetDatabase(), 0 );
	engine->SetGlow( true );
	engine->LoadConfigFiles( "./res/particles.xml", "./res/lighting.xml" );

	game->InitStd( &gamui2D, &okay, 0 );
	dropButton.Init( &gamui2D, lumosGame->GetButtonLook(0));
	dropButton.SetText( "Drop" );

	faceWidget.Init( &gamui2D, lumosGame->GetButtonLook(0));
	faceWidget.SetFace( &uiRenderer, itemComponent->GetItem(0) );

	desc.Init( &gamui2D );

	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		itemButton[i].Init( &gamui2D, lumosGame->GetButtonLook(0) );
		faceWidget.GetToggleButton()->AddToToggleGroup( &itemButton[i] );
	}

	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		textKey[i].Init( &gamui2D );
		textVal[i].Init( &gamui2D );
	}

	moneyWidget.Init( &gamui2D );
	moneyWidget.Set( itemComponent->GetWallet() );

	engine->lighting.direction.Set( 0, 1, 1 );
	engine->lighting.direction.Normalize();
	
	engine->camera.SetPosWC( 2, 2, 2 );
	static const Vector3F out = { 1, 0, 0 };
	engine->camera.SetDir( out, V3F_UP );

	SetButtonText();
}


CharacterScene::~CharacterScene()
{
	engine->FreeModel( model );
	delete engine;
}


void CharacterScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );

	LayoutCalculator layout = lumosGame->DefaultLayout();

	layout.PosAbs( &faceWidget, 0, 0 );
	layout.PosAbs( &itemButton[0], 1, 0 );

	layout.PosAbs( &moneyWidget, 1, 0, false );

	int col=1;
	int row=0;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		layout.PosAbs( &itemButton[i], col, row+1 );
		++col;
		if ( col == 4 ) {
			++row;
			col = 1;
		}
	}
	layout.PosAbs( &dropButton, 1, row+1 );

	layout.SetSize( layout.Width(), layout.Height()*0.4f );

	layout.PosAbs( &desc, -4, 0 );
	desc.SetSize( layout.Width() * 4.0f, layout.Height() * 3.0f );

	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		layout.PosAbs( &textKey[i], -4, i+2 );
		layout.PosAbs( &textVal[i], -2, i+2 );
	}

}


void CharacterScene::SetItemInfo( const GameItem* item, const GameItem* user )
{
	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		textKey[i].SetText( "" );
		textVal[i].SetText( "" );
	}

	if ( !item )
		return;

	CStr< 128 > str;

	str.Format( "%s\nLevel: %d  XP: %d / %d", item->ProperName() ? item->ProperName() : item->Name(), 
										 item->traits.Level(),
										 item->traits.Experience(),
										 GameTrait::LevelToExperience( item->traits.Level()+1 ));
	desc.SetText( str.c_str() );


	/*			Ranged		Melee	Shield
		STR		Dam			Dam		Capacity
		WILL	EffRange	D/TU	Reload
		CHR		Clip/Reload	x		x
		INT		Ranged D/TU x		x
		DEX		Melee D/TU	x		x
	*/

	int i = 0;
	if ( item->ToRangedWeapon() ) {
		textKey[i].SetText( "Ranged Damage" );
		str.Format( "%.1f", item->traits.Damage() * item->rangedDamage );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Effective Range" );
		float radAt1 = BattleMechanics::ComputeRadAt1( user, item->ToRangedWeapon(), false, false );
		str.Format( "%.1f", BattleMechanics::EffectiveRange( radAt1 ));
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Clip/Reload" );
		str.Format( "%d / %.1f", item->clipCap, 0.001f * (float)item->reload.Threshold() );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Ranged D/TU" );
		str.Format( "%.1f", BattleMechanics::RangedDPTU( item->ToRangedWeapon(), false ));
		textVal[i++].SetText( str.c_str() );
	}
	if ( item->ToMeleeWeapon() ) {
		textKey[i].SetText( "Melee Damage" );
		DamageDesc dd;
		BattleMechanics::CalcMeleeDamage( user, item->ToMeleeWeapon(), &dd );
		str.Format( "%.1f", dd.damage );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Melee D/TU" );
		str.Format( "%.1f", BattleMechanics::MeleeDPTU( user, item->ToMeleeWeapon() ));
		textVal[i++].SetText( str.c_str() );

		float boost = BattleMechanics::ComputeShieldBoost( item->ToMeleeWeapon() );
		if ( boost > 1.0f ) {
			textKey[i].SetText( "Shield Boost" );
			str.Format( "%.1f", boost );
			textVal[i++].SetText( str.c_str() );
		}
	}
	if ( item->ToShield() ) {
		textKey[i].SetText( "Capacity" );
		str.Format( "%d", item->clipCap );
		if ( i<NUM_TEXT_KV ) textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Reload" );
		str.Format( "%.1f", 0.001f * (float)item->reload.Threshold() );
		if ( i<NUM_TEXT_KV ) textVal[i++].SetText( str.c_str() );
	}

	if ( !(item->ToMeleeWeapon() || item->ToShield() || item->ToRangedWeapon() )) {
		str.Format( "%d", item->traits.Strength() );
		textKey[KV_STR].SetText( "Strength" );
		textVal[KV_STR].SetText( str.c_str() );

		str.Format( "%d", item->traits.Will() );
		textKey[KV_WILL].SetText( "Will" );
		textVal[KV_WILL].SetText( str.c_str() );

		str.Format( "%d", item->traits.Charisma() );
		textKey[KV_CHR].SetText( "Charisma" );
		textVal[KV_CHR].SetText( str.c_str() );

		str.Format( "%d", item->traits.Intelligence() );
		textKey[KV_INT].SetText( "Intelligence" );
		textVal[KV_INT].SetText( str.c_str() );

		str.Format( "%d", item->traits.Dexterity() );
		textKey[KV_DEX].SetText( "Dexterity" );
		textVal[KV_DEX].SetText( str.c_str() );
		i = KV_DEX+1;
	}

	++i;	// put in space
	MicroDBIterator it( item->microdb );
	for( ; !it.Done() && i < NUM_TEXT_KV; it.Next() ) {
		
		const char* key = it.Key();
		if ( it.NumSub() == 1 && it.SubType(0) == 'd' ) {
			textKey[i].SetText( key );
			str.Format( "%d", it.Int(0) );
			textVal[i++].SetText( str.c_str() );
		}
	}
}


void CharacterScene::SetButtonText()
{
	int count=0;
	const GameItem* down = 0;
	const GameItem* mainItem = itemComponent->GetItem(0);
	const IRangedWeaponItem* ranged = itemComponent->GetRangedWeapon(0);
	const IMeleeWeaponItem*  melee  = itemComponent->GetMeleeWeapon();
	const IShield*           shield = itemComponent->GetShield();
	const GameItem* rangedItem = ranged ? ranged->GetItem() : 0;
	const GameItem* meleeItem  = melee  ? melee->GetItem() : 0;
	const GameItem* shieldItem = shield ? shield->GetItem() : 0;

	RenderAtom nullAtom;
	RenderAtom iconAtom = LumosGame::CalcUIIconAtom( "okay" );
	memset( itemButtonIndex, 0, sizeof(itemButtonIndex[0])*NUM_ITEM_BUTTONS );

	int src = 1;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		const GameItem* item = itemComponent->GetItem(src);
		if ( !item || item->Intrinsic() ) {
			++src;
			continue;
		}

		// Set the text to the proper name, if we have it.
		// Then an icon for what it is, and a check
		// mark if the object is in use.
		lumosGame->ItemToButton( item, &itemButton[count] );
		itemButtonIndex[count] = src;

		// Set the "active" icons.
		if ( item == rangedItem || item == meleeItem || item == shieldItem ) {
			itemButton[count].SetIcon( iconAtom, iconAtom );
		}
		else {
			itemButton[count].SetIcon( nullAtom, nullAtom );
		}

		if ( itemButton[count].Down() ) {
			down = item;
		}
		++count;
		++src;
	}
	for( ; count<NUM_ITEM_BUTTONS; ++count ) {
		itemButton[count].SetText( "" );
		itemButton[count].SetIcon( nullAtom, nullAtom );
		itemButton[count].SetDeco( nullAtom, nullAtom );
	}

	if ( !down ) {
		down = itemComponent->GetItem(0);
	}

	if ( down ) {
		if ( model && !StrEqual( model->GetResource()->Name(), down->ResourceName() )) {
			engine->FreeModel( model );
			model = 0;
		}
		if ( !model ) {
			model = engine->AllocModel( down->ResourceName() );
			model->SetPos( 0,0,0 );

			Rectangle3F aabb = model->AABB();
			float size = Max( aabb.SizeX(), Max( aabb.SizeY(), aabb.SizeZ() ));
			float d = 1.5f + size;
			engine->camera.SetPosWC( d, d, d );
			engine->CameraLookAt( engine->camera.PosWC(), aabb.Center() );

			if ( !down->GetValue( "procedural" ).empty() ) {
				ProcRenderInfo info;
				AssignProcedural( down->GetValue( "procedural" ).c_str(), strstr( down->Name(), "emale" )!=0, down->id, down->primaryTeam, false, down->flags & GameItem::EFFECT_MASK, &info );
				model->SetTextureXForm( info.te.uvXForm );
				model->SetColorMap( true, info.color );
				model->SetBoneFilter( info.filterName, info.filter );
			}
		}
	}

	if ( down != itemComponent->GetItem(0)) {
		SetItemInfo( down, itemComponent->GetItem(0) );
	}
	else {
		SetItemInfo( down, 0 );
	}
}


void CharacterScene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		lumosGame->PopScene();
	}
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( item == &itemButton[i] ) {
			SetButtonText();
		}
	}
	if ( item == faceWidget.GetButton() ) {
		SetButtonText();
	}
}

void CharacterScene::DoTick( U32 deltaTime )
{
}
	
void CharacterScene::Draw3D( U32 deltaTime )
{
	// we use our own screenport
	screenport.SetPerspective();
	engine->Draw( deltaTime, 0, 0 );
	screenport.SetUI();
}


gamui::RenderAtom CharacterScene::DragStart( const gamui::UIItem* item )
{
	RenderAtom atom, nullAtom;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( &itemButton[i] == item ) {

			itemButton[i].GetDeco( &atom, 0 );
			if ( !atom.Equal( nullAtom ) ) {
				itemButton[i].SetDeco( nullAtom, nullAtom );
			}
			return atom;
		}
	}
	return nullAtom;
}


void CharacterScene::DragEnd( const gamui::UIItem* start, const gamui::UIItem* end )
{
	int startIndex = 0;
	int endIndex   = 0;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( start == &itemButton[i] ) {
			startIndex = itemButtonIndex[i];
			break;
		}
	}
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( end == &itemButton[i] ) {
			endIndex = itemButtonIndex[i];
			break;
		}
	}

	if ( startIndex && endIndex ) {
		itemComponent->Swap( startIndex, endIndex );
	}
	if ( start && startIndex && end == &dropButton ) {
		itemComponent->Drop( itemComponent->GetItem( startIndex ));
	}

	SetButtonText();
}
