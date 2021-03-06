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

#ifndef LUMOS_LUMOSGAME_INCLUDED
#define LUMOS_LUMOSGAME_INCLUDED

#include "../xegame/game.h"

class GameItem;

class LumosGame : public Game
{
	typedef Game super;

public:
	enum {
		DECO_NONE = 32
	};

	enum {
		PALETTE_NORM, PALETTE_BRIGHT, PALETTE_DARK
	};

	LumosGame(  int width, int height, int rotation );
	virtual ~LumosGame();

	virtual LumosGame* ToLumosGame() { return this; }

	enum {
		SCENE_TITLE,
		SCENE_DIALOG,
		SCENE_RENDERTEST,
		SCENE_COLORTEST,
		SCENE_PARTICLE,
		SCENE_NAVTEST,
		SCENE_NAVTEST2,
		SCENE_BATTLETEST,
		SCENE_ANIMATION,
		SCENE_LIVEPREVIEW,
		SCENE_WORLDGEN,
		SCENE_GAME,
		SCENE_CHARACTER,
		SCENE_MAP,
		SCENE_FORGE,
		SCENE_CENSUS,
		SCENE_SOUND,
		SCENE_CREDITS,
		SCENE_FLUID
	};

	virtual Scene* CreateScene( int id, SceneData* data );
	virtual void CreateTexture( Texture* t );

	void CopyFile( const char* src, const char* target );
	virtual void Save();

	enum {
		DECO_OKAY = 15,
		DECO_CANCEL = 22
	};
	static gamui::RenderAtom CalcParticleAtom( int id, bool enabled=true );
	static gamui::RenderAtom CalcIconAtom( int id, bool enabled=true );
	static gamui::RenderAtom CalcIconAtom( const char* name );
	static gamui::RenderAtom CalcPaletteAtom( int x, int y );
	static gamui::RenderAtom CalcUIIconAtom(const char* name, bool enabled = true, float* ratio = 0);
	static gamui::RenderAtom CalcDeityAtom(int team);

	grinliz::IString SoundName(const grinliz::IString& name, int seed);

	enum {
		BUTTON_LOOK_STD,
		BUTTON_LOOK_COUNT
	};
	const gamui::ButtonLook& GetButtonLook( int i ) { GLASSERT( i>=0 && i<BUTTON_LOOK_COUNT ); return buttonLookArr[i]; }
	
	// Put the item text and symbols on a button.
	// If for sale, pass in the costMult: <1 for the amount it can be sold for, >1 for the buying price
	void ItemToButton( const GameItem* item, gamui::Button* button );

protected:

private:

	void InitButtonLooks();
	grinliz::GLString nameBuffer;

	gamui::ButtonLook buttonLookArr[BUTTON_LOOK_COUNT];
};


#endif //  LUMOS_LUMOSGAME_INCLUDED