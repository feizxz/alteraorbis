#include "newsconsole.h"
#include "news.h"
#include "lumoschitbag.h"
#include "lumosgame.h"
#include "../engine/uirendering.h"
#include "../xegame/spatialcomponent.h"
#include "../widget/consolewidget.h"
#include "../script/corescript.h"
#include "../script/procedural.h"

using namespace gamui;
using namespace grinliz;

NewsConsole::NewsConsole()
{
	currentNews = 0;
}


NewsConsole::~NewsConsole()
{
}


void NewsConsole::Init(Gamui* gamui2D, LumosChitBag* cb)
{
	chitBag = cb;
	consoleWidget.Init(gamui2D);
}


void NewsConsole::DoTick(int time, CoreScript* homeCore)
{
	consoleWidget.DoTick(time);
	ProcessNewsToConsole(homeCore);
}


void NewsConsole::ProcessNewsToConsole(CoreScript* homeCore)
{
	NewsHistory* history = chitBag->GetNewsHistory();
	currentNews = Max(currentNews, history->NumNews() - 40);
	GLString str;
	Vector2I homeSector = { 0, 0 };
	int homeCoreTeam = -1;
	if (homeCore) {
		homeSector = ToSector(homeCore->ParentChit()->Position());
		homeCoreTeam = homeCore->ParentChit()->Team();
	}

	for (; currentNews < history->NumNews(); ++currentNews) {
		const NewsEvent& ne = history->News(currentNews);
		Vector2I sector = ne.Sector();
		Vector2F pos2 = ne.Pos();
		RenderAtom atom;
		RenderAtom background;

		str = "";

		switch (ne.What()) {
			case NewsEvent::DENIZEN_CREATED:
			case NewsEvent::ROGUE_DENIZEN_JOINS_TEAM:

			GLASSERT(   ne.What() == NewsEvent::DENIZEN_CREATED	// usually does NOT have team - random gen.
					 || Team::ID(ne.FirstTeam()));				// but Rogue should have team
			if (homeCore && (homeCoreTeam == ne.FirstTeam())) {
				ne.Console(&str, chitBag, 0);
				atom = LumosGame::CalcUIIconAtom("greeninfo");
			}
			break;

			case NewsEvent::DENIZEN_KILLED:
			case NewsEvent::STARVATION:
			case NewsEvent::BLOOD_RAGE:
			case NewsEvent::VISION_QUEST:

			if (sector == homeSector) {
				ne.Console(&str, chitBag, 0);
				atom = LumosGame::CalcUIIconAtom("warning");
			}
			break;

			case NewsEvent::FORGED:
			case NewsEvent::UN_FORGED:
			case NewsEvent::PURCHASED:

			if (   (homeCoreTeam == ne.FirstTeam())		// actually refers to the forged thing team - the forger is more complex.
				|| (sector == homeSector))				// probably the more useful check.
			{
				ne.Console(&str, chitBag, 0);
				atom = LumosGame::CalcUIIconAtom("greeninfo");
			}
			break;

			case NewsEvent::GREATER_MOB_CREATED:
			case NewsEvent::GREATER_MOB_KILLED:
			ne.Console(&str, chitBag, 0);
			atom = LumosGame::CalcUIIconAtom("greeninfo");
			break;

			case NewsEvent::DOMAIN_DESTROYED:
			case NewsEvent::GREATER_SUMMON_TECH:
			case NewsEvent::DOMAIN_CONQUER:
			case NewsEvent::SUPERTEAM_DELETED:
			case NewsEvent::SUBTEAM_DELETED:
			background = LumosGame::CalcPaletteAtom(PAL_TANGERINE * 2, PAL_TANGERINE);
			background.renderState = (const void*)UIRenderer::RENDERSTATE_UI_DISABLED;
			ne.Console(&str, chitBag, 0);
			break;

			case NewsEvent::PLOT_START:
			case NewsEvent::PLOT_EVENT:
			case NewsEvent::PLOT_END:
			background = LumosGame::CalcPaletteAtom(PAL_TANGERINE * 2, PAL_TANGERINE);
			background.renderState = (const void*)UIRenderer::RENDERSTATE_UI_DISABLED;
			ne.Console(&str, chitBag, 0);
			atom = LumosGame::CalcUIIconAtom("warning");
			break;

			case NewsEvent::DOMAIN_CREATED:
			case NewsEvent::DOMAIN_TAKEOVER:
			ne.Console(&str, chitBag, 0);
			break;

			case NewsEvent::ATTITUDE_FRIEND:
			case NewsEvent::ATTITUDE_NEUTRAL:
			case NewsEvent::ATTITUDE_ENEMY:
			if (   !homeCore 
				|| homeCore->ParentChit()->GetItemID() == ne.FirstItemID() 
				|| homeCore->ParentChit()->GetItemID() == ne.SecondItemID())
			{
				ne.Console(&str, chitBag, 0);
			}
			default:
			break;
		}
		if (!str.empty()) {
			consoleWidget.Push(str, atom, pos2, background);
		}
	}
}

