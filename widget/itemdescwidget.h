#ifndef ITEM_DESC_WIDGET_INCLUDED
#define ITEM_DESC_WIDGET_INCLUDED

#include "../gamui/gamui.h"
#include "../game/gamelimits.h"

class GameItem;

class ItemDescWidget : public gamui::IWidget
{
public:
	ItemDescWidget() : layout( 1000, 1000 )			{}
	void Init( gamui::Gamui* gamui );

	virtual float X() const							{ return textKey[0].X(); }
	virtual float Y() const							{ return textKey[0].Y(); }
	virtual float Width() const						{ return textVal[0].X() + textVal[0].Width() - textKey[0].X(); }
	virtual float Height() const					{ return textVal[0].Height(); }

	void SetLayout( const gamui::LayoutCalculator& _layout )	{ layout = _layout; }
	virtual void SetPos( float x, float y );

	virtual void SetSize( float width, float h )	{}
	virtual bool Visible() const					{ return textKey[0].Visible(); }
	virtual void SetVisible( bool vis );

	void SetInfo( const GameItem* item, const GameItem* user );

private:

	enum {
		KV_STR,
		KV_WILL,
		KV_CHR,
		KV_INT,
		KV_DEX,
		NUM_TEXT_KV = KV_DEX+10
	};

	gamui::LayoutCalculator	layout;
	gamui::TextLabel	textKey[NUM_TEXT_KV];
	gamui::TextLabel	textVal[NUM_TEXT_KV];
};


#endif // ITEM_DESC_WIDGET_INCLUDED

