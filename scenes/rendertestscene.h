#ifndef RENDERTESTSCENE_INCLUDED
#define RENDERTESTSCENE_INCLUDED

#include "../xegame/scene.h"
#include "../gamui/gamui.h"

class LumosGame;
class Engine;
class TestMap;
class Model;

class RenderTestSceneData : public SceneData
{
public:
	RenderTestSceneData( int _id ) : id( _id ) {}
	int id;
};


class RenderTestScene : public Scene
{
public:
	RenderTestScene( LumosGame* game, const RenderTestSceneData* data );
	virtual ~RenderTestScene();

	virtual int RenderPass( grinliz::Rectangle2I* clip3D, grinliz::Rectangle2I* clip2D )
	{
		return RENDER_2D | RENDER_3D;	
	}
	virtual void Resize();

	virtual void Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world )	{ ProcessTap( action, screen, world ); }
	virtual void ItemTapped( const gamui::UIItem* item );
	virtual void Zoom( int style, float normal );
	virtual void Rotate( float degrees );
	virtual void HandleHotKeyMask( int mask );

	virtual void Draw3D( U32 deltaTime );

private:
	enum { NUM_ITEMS = 4,
		   NUM_MODELS = 8 };

	void SetupTest0();
	void SetupTest1();

	void LoadLighting();

	int testID;
	LumosGame* lumosGame;
	gamui::PushButton okay;
	gamui::PushButton whiteButton;
	gamui::ToggleButton hemiButton;
	gamui::PushButton refreshButton;
	gamui::TextBox textBox;

	Engine* engine;
	Model*  model[NUM_MODELS];
	TestMap* testMap;
};

#endif // RENDERTESTSCENE_INCLUDED