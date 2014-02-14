#include "facewidget.h"
#include "../script/procedural.h"
#include "../game/gameitem.h"
#include "../game/lumosgame.h"
#include "../grinliz/glstringutil.h"
#include "../ai/aineeds.h"
#include "../game/aicomponent.h"
#include "../xegame/itemcomponent.h"	

using namespace gamui;
using namespace grinliz;

static const float HEIGHT = 10.0f;
static const float SPACE = 5.0f;

void FaceToggleWidget::Init( gamui::Gamui* gamui, const gamui::ButtonLook& look, int f )
{
	toggle.Init( gamui, look );
	toggle.SetEnabled( true );
	BaseInit( gamui, look, f );
}


void FacePushWidget::Init( gamui::Gamui* gamui, const gamui::ButtonLook& look, int f )
{
	push.Init( gamui, look );
	push.SetEnabled( true );
	BaseInit( gamui, look, f );
}


void FaceWidget::BaseInit( gamui::Gamui* gamui, const gamui::ButtonLook& look, int f )
{
	flags = f;
	upper.Init( gamui );

	RenderAtom green = LumosGame::CalcPaletteAtom( 1, 3 );	
	RenderAtom grey  = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue  = LumosGame::CalcPaletteAtom( 8, 0 );	

	// Must keep the Needs and Bars in sync.
	GLASSERT( BAR_FOOD + ai::Needs::NUM_NEEDS == MAX_BARS );
	bar[BAR_HP].Init(		gamui, 2, green, grey );
	bar[BAR_AMMO].Init(		gamui, 2, blue, grey );
	bar[BAR_SHIELD].Init(	gamui, 2, blue, grey );
	bar[BAR_LEVEL].Init(	gamui, 2, blue, grey );
	bar[BAR_MORALE].Init(	gamui, 2, blue, grey );

	bar[BAR_HP].SetText( "HP" );
	bar[BAR_AMMO].SetText( "Weapon" );
	bar[BAR_SHIELD].SetText( "Shield" );
	bar[BAR_MORALE].SetText( "Morale" );

	for( int i=0; i<ai::Needs::NUM_NEEDS; i++ ) {
		GLASSERT( i < MAX_BARS );
		bar[i+BAR_FOOD].Init( gamui, 2, green, grey );
		bar[i+BAR_FOOD].SetText( ai::Needs::Name( i ) );
	}

	upper.SetVisible( false );
	for( int i=0; i<MAX_BARS; ++i ) {
		bar[i].SetVisible( (flags & (1<<i)) != 0 );
	}
}


void FaceWidget::SetFace( UIRenderer* renderer, const GameItem* item )
{
	if ( item ) {
		ProcRenderInfo info;
		HumanGen faceGen( strstr( item->ResourceName(), "emale" ) != 0, item->ID(), item->primaryTeam, false );
		faceGen.AssignFace( &info );

		RenderAtom procAtom(	(const void*) (UIRenderer::RENDERSTATE_UI_CLIP_XFORM_MAP), 
								info.texture,
								info.te.uv.x, info.te.uv.y, info.te.uv.z, info.te.uv.w );

		GetButton()->SetDeco( procAtom, procAtom );

		renderer->uv[0]			= info.te.uv;
		renderer->uvClip[0]		= info.te.clip;
		renderer->colorXForm[0]	= info.color;

		GetButton()->SetVisible( true );

		CStr<40> str;
		if ( flags & SHOW_NAME ) {
			upper.SetVisible( true );
			str.AppendFormat( "%s", item->IBestName().c_str() );
			//str.AppendFormat( "%s", item->INameAndTitle().c_str() );
		}
		upper.SetText( str.c_str() );
	}
	else {
		GetButton()->SetVisible( false );
		upper.SetText( "" );
	}
 
	for( int i=0; i < MAX_BARS; ++i ) {
		bool on = ((1<<i) & flags) != 0;
		on = on && GetButton()->Visible();
		bar[i].SetVisible( on );
	}	
}


void FaceWidget::SetMeta( ItemComponent* ic, AIComponent* ai ) 
{
	RenderAtom orange = LumosGame::CalcPaletteAtom( 4, 0 );
	RenderAtom grey   = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue   = LumosGame::CalcPaletteAtom( 8, 0 );	

	CStr<30> str;

	if ( ic ) {
		const GameItem* item = ic->GetItem(0);

		if ( flags & LEVEL_BAR ) {
			bar[BAR_HP].SetRange( item->HPFraction() );

			int lev = item->Traits().Level();
			int xp  = item->Traits().Experience();
			int nxp = item->Traits().LevelToExperience( item->Traits().Level()+1 );
			int pxp = item->Traits().LevelToExperience( item->Traits().Level() );

			str.Format( "Level %d", lev );
			bar[BAR_LEVEL].SetText( str.c_str() );
			bar[BAR_LEVEL].SetRange( float( xp - pxp ) / float( nxp - pxp ));
		}
		IShield* ishield			= ic->GetShield();
		IRangedWeaponItem* iweapon	= ic->GetRangedWeapon(0);

		if ( iweapon ) {
			float r = 0;
			if ( iweapon->GetItem()->Reloading() ) {
				r = iweapon->GetItem()->ReloadFraction();
				bar[BAR_AMMO].SetLowerAtom( orange );
			}
			else {
				r = iweapon->GetItem()->RoundsFraction();
				bar[BAR_AMMO].SetLowerAtom( blue );
			}
			bar[BAR_AMMO].SetRange( r );
		}
		else {
			bar[BAR_AMMO].SetRange( 0 );
		}

		if ( ishield ) {
			float r = ishield->GetItem()->RoundsFraction();
			bar[BAR_SHIELD].SetRange( r );
		}
		else {
			bar[BAR_SHIELD].SetRange( 0 );
		}
	}


	if ( ai ) {
		const ai::Needs& needs = ai->GetNeeds();
		for( int i=0; i<ai::Needs::NUM_NEEDS; ++i ) {
			bar[i+BAR_FOOD].SetRange( (float)needs.Value(i) );
		}
		bar[BAR_MORALE].SetRange( (float)needs.Morale() );
	}
}


void FaceWidget::SetPos( float x, float y )			
{ 
	GetButton()->SetPos( x, y );  
	upper.SetPos( x, y ); 

	float cy = GetButton()->Y() + GetButton()->Height() + SPACE;
	for( int i=0; i < MAX_BARS; ++i ) {
		int on = (1<<i) & flags;

		if ( on ) {
			bar[i].SetPos( x, cy );
			bar[i].SetSize( GetButton()->Width(), HEIGHT );
			bar[i].SetVisible( true );
			cy += HEIGHT + SPACE;
		}
		else {
			bar[i].SetVisible( false );
		}
	}
}


void FaceWidget::SetSize( float w, float h )		
{ 
	Button* button = GetButton();
	button->SetSize( w, h ); 
	upper.SetBounds( w, 0 ); 
	for( int i=0; i < MAX_BARS; ++i ) {
		bar[i].SetSize( GetButton()->Width(), HEIGHT );
	}
	// SetSize calls SetPos, but NOT vice versa
	SetPos( GetButton()->X(), GetButton()->Y() );
}


void FaceWidget::SetVisible( bool vis )
{ 
	GetButton()->SetVisible( vis ); 
	upper.SetVisible( vis );
	for( int i=0; i < MAX_BARS; ++i ) {
		if ( !vis )
			bar[i].SetVisible( false );
		else
			bar[i].SetVisible( ((1<<i) & flags) != 0 );
	}
}
