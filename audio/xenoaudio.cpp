#include "xenoaudio.h"
#include "../xegame/cgame.h"
#include "../grinliz/glgeometry.h"
#include "../libs/SDL2/include/SDL_mixer.h"
#include "../libs/SDL2/include/SDL.h"

using namespace grinliz;

XenoAudio* XenoAudio::instance = 0;

static const float MAX_DISTANCE = 20.0f;

XenoAudio::XenoAudio(const gamedb::Reader* db, const char* pathToDB)
{
	GLASSERT(!instance);
	instance = this;
	database = db;

//	Mix_Init(0);	// just need wav.
	dataFP = SDL_RWFromFile(pathToDB, "rb");
	GLASSERT(dataFP);
	sounds.PushArr(CHANNELS);
	listenerPos.Zero();
	listenerDir.Set(1, 0, 0);
}


XenoAudio::~XenoAudio()
{
	if (audioOn) {
		Mix_CloseAudio();
	}
	for (int i = 0; i < chunks.NumValues(); ++i) {
		Mix_FreeChunk(chunks.GetValue(i));
	}
	GLASSERT(instance == this);
	instance = 0;
	SDL_RWclose(dataFP);
}


void XenoAudio::SetAudio(bool on) 
{
	if (on == audioOn) {
		return;
	}
	audioOn = on;
	if (audioOn) {
		int error = Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 2048);
		GLASSERT(error == 0);
	}
	else {
		Mix_CloseAudio();
	}
}

void XenoAudio::Play(const char* _sound, const Vector3F* pos)
{
	if (!audioOn) return;

	// The listener will get updated at the end of the frame,
	// so this isn't quite correct. But hopefully good enough
	// to prevent saturating the game with sounds all
	// over the world.
	if (   pos 
		&& (*pos - listenerPos).LengthSquared() > (MAX_DISTANCE*MAX_DISTANCE)) 
	{
		return;
	}

	IString iSound = StringPool::Intern(_sound);
	const char* sound = iSound.c_str();

	Mix_Chunk* chunk = 0;
	chunks.Query(sound, &chunk);

	if (!chunk) {
		const gamedb::Item* data = database->Root()->Child("data");
		const gamedb::Item* item = data->Child(sound);
		SDL_RWops* fp = 0;
		bool needClose = false;

		// Search external path first.
		GLString path;
		GLString inPath = sound;
		inPath.append(".wav");
		GetSystemPath(GAME_APP_DIR, inPath.c_str(), &path);

		fp = SDL_RWFromFile(path.c_str(), "rb");
		if (fp) {
			needClose = true;
		}

		// Now check the database
		if (!fp) {
			int offset = 0, size = 0;
			bool compressed = false;

			item->GetDataInfo("binary", &offset, &size, &compressed);
			GLASSERT(compressed == false);
			fp = dataFP;
			SDL_RWseek(fp, offset, RW_SEEK_SET);
		}
		if (fp) {
			U8* buf = 0;
			U32 len = 0;
			chunk = Mix_LoadWAV_RW(fp, false);
			if (!chunk) {
				GLOUTPUT(("Audio error: %s\n", Mix_GetError()));
			}
			else {
				chunks.Add(sound, chunk);
			}
			if (needClose) {
				SDL_RWclose(fp);
			}
		}
	}
	GLASSERT(chunk);
	if (chunk) {
		int channel = Mix_PlayChannel(-1, chunk, 0);
		if (channel >= 0 && channel < CHANNELS ) {
			sounds[channel].channel = channel;
			sounds[channel].pos.Zero();
			if (pos) {
				sounds[channel].pos = *pos;
			}
			SetChannelPos(channel);
		}
	}
}


void XenoAudio::SetListener(const grinliz::Vector3F& pos, const grinliz::Vector3F& dir)
{
	listenerPos = pos;
	listenerDir = dir;
	if (listenerDir.Length())
		listenerDir.Normalize();
	else
		listenerDir.Set(1, 0, 0);
	for (int i = 0; i < CHANNELS; ++i) {
		SetChannelPos(i);
	}
}


void XenoAudio::SetChannelPos(int i)
{
#ifdef DEBUG
	if (Mix_Playing(i)) {
		int debug = 1;
	}
#endif

	if (sounds[i].pos.IsZero()) {
		Mix_SetPosition(i, 0, 0);
	}
	else {
		Vector3F delta = sounds[i].pos - listenerPos;
		float len = delta.Length();
		float df = len / MAX_DISTANCE;
		int d = LRintf(df*255.0f);
		d = Clamp(d, 0, 255);

		if (delta.LengthSquared() > 0.001f) {
			delta.Normalize();
		}
		else {
			delta.Set(0, 0, -1);
		}

		static const Vector3F UP = { 0, 1, 0 };
		Vector3F listenerRight = CrossProduct(listenerDir, UP);

		float dotFront = DotProduct(delta, listenerDir);
		float dotRight = DotProduct(delta, listenerRight);

		// 0 is north, 90 is east, etc. WTF.
		float rad = atan2(dotRight, dotFront);
		float deg = rad * 180.0f / PI;
		int degi = int(deg);
		if (degi < 0) degi += 360;

		int result = Mix_SetPosition(i, degi, d);
		GLASSERT(result != 0);
	}
}