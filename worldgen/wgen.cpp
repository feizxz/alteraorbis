// worldgen.cpp : Defines the entry point for the console application.
//

#include <ctime>

#include "../shared/lodepng.h"
#include "../grinliz/glcolor.h"
#include "../grinliz/glnoise.h"
#include "../grinliz/glrandom.h"
#include "../grinliz/glstringutil.h"
#include "../script/worldgen.h"

using namespace grinliz;

static const int COUNT = 5;
static const float FRACTION_LAND = 0.4f;
static const int WIDTH  = WorldGen::SIZE;
static const int HEIGHT = WorldGen::SIZE;


int main(int argc, const char* argv[])
{

	int seed = 0; //random.Rand();
	int count = COUNT;

	if ( argc >= 2 ) {
		seed = atoi( argv[1] );
		count = 1;
	}

	Color4U8* pixels = new Color4U8[WIDTH*HEIGHT];
	memset( pixels, 0xa0, sizeof(Color4U8)*WIDTH*HEIGHT );

	clock_t startTime = clock();
	clock_t loopTime = startTime;

	WorldGen worldGen;
	U32 seed0 = seed;
	U32 seed1 = seed*43+1924;

	for( int i=0; i<count; ++i ) {
		// Always change seed in case of retry.
		seed0 = seed0*3+7;
		seed1 = seed1*11+2;

		bool result = worldGen.CalcLandAndWater( seed0, seed1, FRACTION_LAND );
		if ( !result ) {
			printf( "CalcLandAndWater failed. Retry.\n" );
			--i;
			continue;
		}

		result = worldGen.CalColor();
		if ( !result ) {
			printf( "CalcColor failed. Retry.\n" );
			--i;
			continue;
		}

		const U8* land = worldGen.Land();
		const U8* color = worldGen.Color();
		static const Color4U8 LAND_COLOR[7] = {
			{ 0, 255, 33, 255 },
			{ 255, 216, 0, 255 },
			{ 182, 255, 0, 255 },
			{ 192, 192, 192, 255 },
			{ 0, 165, 19, 255 },
			{ 165, 138, 0, 255 },
			{ 118, 165, 0, 255 },
		};
	
		for( int j=0; j<HEIGHT; ++j ) {
			for( int i=0; i<WIDTH; ++i ) {
				
				Color4U8 rgb = { 0, 0, 0, 255 };
				U8 c = color[j*WIDTH+i];

				if ( land[j*WIDTH+i] ) {
					rgb = LAND_COLOR[c%7];
				}
				else {
					rgb.Set( 0, 19, 127, 255 );
				}
				pixels[j*WIDTH+i] = rgb;
			}
		}
		clock_t endTime = clock();
		printf( "loop %d: %dms\n", i, endTime - loopTime );
		loopTime = endTime;

		for( int j=0; j<Min(worldGen.NumWorldFeatures(),10); ++j ) {
			const WorldFeature& wf = *(worldGen.WorldFeatures() + j);
			if ( wf.land ) {
				printf( "%d  %s area=%d (%d,%d)-(%d,%d)\n", 
						wf.id, 
						wf.land ? "land" : "water", 
						wf.area,
						wf.bounds.min.x, wf.bounds.min.y, wf.bounds.max.x, wf.bounds.max.y );
			}
		}

		CStr<32> fname;
		fname.Format( "worldgen%02d.png", i );
		lodepng_encode32_file( fname.c_str(), (const unsigned char*)pixels, WIDTH, HEIGHT );
	}
	printf( "total time %dms\n", clock()-startTime );
	return 0;
}

