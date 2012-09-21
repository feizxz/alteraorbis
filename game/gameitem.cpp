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

#include "gameitem.h"

#include "../grinliz/glstringutil.h"
#include "../tinyxml2/tinyxml2.h"

#include "../xegame/inventorycomponent.h"
#include "../xegame/chit.h"

using namespace grinliz;
using namespace tinyxml2;

#define READ_FLAG( flags, str, name )	{ if ( strstr( str, #name )) flags |= name; }
#define READ_FLOAT_ATTR( ele, name )	{ ele->QueryFloatAttribute( #name, &name ); }
#define READ_INT_ATTR( ele, name )		{ ele->QueryIntAttribute( #name, &name ); }
#define READ_UINT_ATTR( ele, name )		{ ele->QueryUnsignedAttribute( #name, &name ); }


void GameItem::Save( tinyxml2::XMLPrinter* )
{
	GLASSERT( 0 );
}


void GameItem::Load( const tinyxml2::XMLElement* ele )
{
	this->CopyFrom( 0 );

	GLASSERT( StrEqual( ele->Name(), "item" ));
	
	name		= ele->Attribute( "name" );
	resource	= ele->Attribute( "resource" );
	flags = 0;
	const char* f = ele->Attribute( "flags" );

	READ_FLAG( flags, f, CHARACTER );
	READ_FLAG( flags, f, MELEE_WEAPON );
	READ_FLAG( flags, f, RANGED_WEAPON );
	READ_FLAG( flags, f, INTRINSIC_AT_HARDPOINT );
	READ_FLAG( flags, f, INTRINSIC_FREE );
	READ_FLAG( flags, f, HELD_AT_HARDPOINT );
	READ_FLAG( flags, f, HELD_FREE );
	READ_FLAG( flags, f, IMMUNE_FIRE );
	READ_FLAG( flags, f, FLAMMABLE );
	READ_FLAG( flags, f, EXPLOSIVE );
	READ_FLAG( flags, f, EFFECT_FIRE );
	READ_FLAG( flags, f, EFFECT_SHOCK );
	READ_FLAG( flags, f, RENDER_TRAIL );

	if ( EFFECT_FIRE )	flags |= IMMUNE_FIRE;

	READ_FLOAT_ATTR( ele, mass );
	READ_INT_ATTR( ele, primaryTeam );
	READ_UINT_ATTR( ele, cooldown );
	READ_UINT_ATTR( ele, cooldownTime );
	READ_UINT_ATTR( ele, reload );
	READ_UINT_ATTR( ele, reloadTime );
	READ_INT_ATTR( ele, clipCap );
	READ_INT_ATTR( ele, rounds );
	READ_FLOAT_ATTR( ele, speed );

	const XMLElement* meleeEle = ele->FirstChildElement( "melee" );
	if ( meleeEle ) {
		meleeDamage.Load( "melee", meleeEle );
	}
	const XMLElement* rangedEle = ele->FirstChildElement( "ranged" );
	if ( rangedEle ) {
		rangedDamage.Load( "ranged", rangedEle );
	}
	const XMLElement* resistEle = ele->FirstChildElement( "resist" );
	if ( resistEle ) {
		resist.Load( "resist", resistEle );
	}

	hardpoint = NO_HARDPOINT;
	const char* h = ele->Attribute( "hardpoint" );
	if ( h ) {
		hardpoint = InventoryComponent::HardpointNameToFlag( h );
		GLASSERT( hardpoint >= 0 );
	}

	hp = mass;
	ele->QueryFloatAttribute( "hp", &hp );

	GLASSERT( TotalHP() > 0 );
	GLASSERT( hp > 0 );	// else should be destroyed
}


bool GameItem::Use() {
	if ( Ready() && HasRound() ) {
		UseRound();
		cooldownTime = 0;
		if ( parentChit ) {
			parentChit->SetTickNeeded();
		}
		return true;
	}
	return false;
}


bool GameItem::Reload() {
	if ( CanReload()) {
		reloadTime = 0;
		if ( parentChit ) {
			parentChit->SetTickNeeded();
		}
		return true;
	}
	return false;
}


void GameItem::UseRound() { 
	if ( clipCap > 0 ) { 
		GLASSERT( rounds > 0 ); 
		--rounds; 
		if ( parentChit ) {
			parentChit->SendMessage( ChitMsg( ChitMsg::ITEM_ROUNDS_CHANGED, 0, this ), 0 );
		}
	} 
}


bool GameItem::DoTick( U32 delta )
{
	cooldownTime += delta;
	if ( reloadTime < reload ) {
		reloadTime += delta;

		if ( reloadTime >= reload ) {
			rounds = clipCap;
			if ( parentChit ) {
				parentChit->SendMessage( ChitMsg( ChitMsg::ITEM_ROUNDS_CHANGED, 0, this ), 0 );
			}
		}
		else {
			if ( parentChit ) {
				parentChit->SendMessage( ChitMsg( ChitMsg::ITEM_RELOADING, 0, this ), 0 );
			}
		}
	}
	return !Ready() || Reloading();	
}


void GameItem::Apply( const GameItem* intrinsic )
{
	if ( intrinsic->flags & EFFECT_FIRE )
		flags |= IMMUNE_FIRE;
//	if ( intrinsic->flags & EFFECT_ENERGY )
//		flags |= IMMUNE_ENERGY;
}


void GameItem::AbsorbDamage( const DamageDesc& dd, DamageDesc* remain, const char* log )
{
	float total = 0;
	GLLOG(( "%s Damage ", log ));
	for( int i=0; i<DamageDesc::NUM_COMPONENTS && hp > 0; ++i ) {
		float d = resist.components[i] * dd.components[i];
		GLLOG(( "%.1f ", d ));
		if ( remain ) {
			// Damage that passes through this 
			float r = (1.f - Clamp(resist.components[i], 0.f, 1.f ) * dd.components[i] );
			remain->components[i] = r;
		}
		total += d;
	}
	hp = Max( 0.f, hp-total );
	GLLOG(( "total=%.1f hp=%.1f", total, hp ));
}


void DamageDesc::Save( const char* prefix, tinyxml2::XMLPrinter* )
{
	GLASSERT( 0 );	// FIXME
}


void DamageDesc::Load( const char* prefix, const tinyxml2::XMLElement* doc )
{
	components.Zero();

	doc->QueryFloatAttribute( "kinetic", &components[KINETIC] );
	doc->QueryFloatAttribute( "energy", &components[ENERGY] );
	doc->QueryFloatAttribute( "fire", &components[FIRE] );
	doc->QueryFloatAttribute( "shock", &components[SHOCK] );
}


void DamageDesc::Log()
{
	GLLOG(( "[k=%.1f e=%.1f f=%.1f s=%.1f sm=%.1f]",
			Kinetic(), Energy(), Fire(), Shock(), Total() ));
}
