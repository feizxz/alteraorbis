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

#ifndef XENOENGINE_GAME_LIMITS_INCLUDED
#define XENOENGINE_GAME_LIMITS_INCLUDED

#include "../grinliz/gltypes.h"

// Cross-everything constant. How big can the base
// of a Chit be? Impacts pathing, collision, map,
// etc. Can't be >= 0.5f since the chit could get
// stuck in hallways.
static const float	MAX_BASE_RADIUS = 0.4f;

// What is the maximum map size? Used to allocate
// the spatial cache. Power of 2, of course.
#ifdef ALTERA_MICRO
static const int	MAX_MAP_SIZE	= 512;
static const int	MAP_Y_SHIFT		= 9;
static const int	MAP_X_MASK		= 511;
#else
static const int	MAX_MAP_SIZE    = 1024;
static const int	MAP_Y_SHIFT		= 10;
static const int	MAP_X_MASK		= 1023;	
#endif

inline int IndexToMapX(int index) {
	return index & MAP_X_MASK;
}

inline int IndexToMapY(int index) {
	return (index >> MAP_Y_SHIFT) & MAP_X_MASK;
}

static const int	MAX_MAP_SIZE_2 = MAX_MAP_SIZE * MAX_MAP_SIZE;

static const U32	CROSS_FADE_TIME = 300;		// time to blend animations

static const int	VERY_LONG_TICK	= 1000*1000;
static const int	ABOUT_1_SEC		= 1017;
static const int	SLOW_TICK		= 500;
static const U32	MAX_FRAME_TIME	= 100;		// At less that 10fps, just slow down the game.
static const U32	STD_FRAME_TIME	= 30;		
static const int	STD_DECO		= 2000;		// typical time for over-head icons to be displayed

#endif // XENOENGINE_GAME_LIMITS_INCLUDED
