#include "SessionStats.h"
#include "utils/parser.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/MMRWrapper.h"

#include <sstream>
#include <map>
#include <set>
#include <fstream>

//#include <sysinfoapi.h>
BAKKESMOD_PLUGIN(SessionStatsPlugin, "Session Stats plugin", "1.0", 0)


void SessionStatsPlugin::onLoad()
{
	//obsOut = make_shared<bool>();
	//screenOut = make_shared<bool>();
	//obsDir = make_shared<std::string>();
	cvarManager->registerCvar("cl_sessionstats_obs_output", "0", "Output text stats to files to be used as OBS sources", true, true, 0, true, 1, true); //.bindTo(obsOut);
	cvarManager->registerCvar("cl_sessionstats_obs_directory", ".", "File to write OBS text output files to", true, false, (0.0F), false, (0.0F), true); //.bindTo(obsDir);
	cvarManager->registerCvar("cl_sessionstats_display_stats", "0", "Display session stats on screen", true, true, 0, true, 1, true); //.bindTo(screenOut);
	
	cvarManager->registerNotifier("cl_sessionstats_reset", [this](std::vector<string> params) {
		ResetStats();
	}, "Start a fresh stats session", PERMISSION_ALL);
	
	currentPlaylist = -1;
	//*obsDir = "F:\\Downloads";
	//*obsOut = true;
	//*screenOut = true;
	gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState", bind(&SessionStatsPlugin::StartGame, this, std::placeholders::_1));
	//gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));

	//gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));
	
	gameWrapper->RegisterDrawable(std::bind(&SessionStatsPlugin::Render, this, std::placeholders::_1));
}

void SessionStatsPlugin::ResetStats() {
	stats.clear();
	if (cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue())
		writeStats();
}
void SessionStatsPlugin::StartGame(std::string eventName) {
	cvarManager->log("====BeginState==============================");

	if (!gameWrapper->IsInOnlineGame())
		return;
	//gameWrapper->SetTimeout()
	CarWrapper me = gameWrapper->GetLocalCar();
	PriWrapper mePRI = me.GetPRI();
	mySteamID = mePRI.GetUniqueId();
	cvarManager->log("SteamID: " + std::to_string(mySteamID.ID));

	ServerWrapper sw = gameWrapper->GetOnlineGame();
	if (!sw.IsNull()) {
		MMRWrapper mw = gameWrapper->GetMMRWrapper();
		currentPlaylist = mw.GetCurrentPlaylist();
		float mmr = mw.GetPlayerMMR(mySteamID, currentPlaylist);
		if (stats.find(currentPlaylist) == stats.end()) 
			stats[currentPlaylist] = StatsStruct { mmr, mmr, 0, 0, 0, 0, 0 };
		
		stringstream ss;
		ss << "steamID: " << std::to_string(mySteamID.ID) << " MMR: " << mmr << " currentPlaylist: " << currentPlaylist;
		cvarManager->log(ss.str());
	}
}

void SessionStatsPlugin::EndGame(std::string eventName) {
	cvarManager->log("===EndGame==================================");

	bool obsOut = cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue();
	gameWrapper->SetTimeout([&](GameWrapper* gameWrapper) {
		bool gotNewMMR = false;
		int count = 0; 
		float mmr = -1.0f;
		while (!gotNewMMR && (count < 10)) {// only try 10 times, then give up
			mmr = gameWrapper->GetMMRWrapper().GetPlayerMMR(mySteamID, currentPlaylist);

			if (mmr > stats[currentPlaylist].currentMMR) {
				stats[currentPlaylist].wins++;
				if (stats[currentPlaylist].streak < 0)
					stats[currentPlaylist].streak = 1;
				else
					stats[currentPlaylist].streak++;
				gotNewMMR = true;
			}
			else if (mmr < stats[currentPlaylist].currentMMR) {
				stats[currentPlaylist].losses++;
				if (stats[currentPlaylist].streak > 0)
					stats[currentPlaylist].streak = -1;
				else
					stats[currentPlaylist].streak--;
				gotNewMMR = true;
			}
			count++;
			Sleep(200);
		}
		stats[currentPlaylist].currentMMR = mmr;
		
		if (obsOut) 
		
			writeStats();
		
	}, 3);

	stringstream ss;
	ss << "======== w:" << stats[currentPlaylist].wins << " l:" << stats[currentPlaylist].losses << " MMR: " << (stats[currentPlaylist].currentMMR - stats[currentPlaylist].initialMMR) << " currentPlaylist: " << currentPlaylist;
	cvarManager->log(ss.str());
	cvarManager->log("================================================");
}

static int writeIntFile(const std::string & fn, int val) {
	std::ofstream ofs(fn, std::ofstream::out);
	if (!ofs) return 0;
	ofs << val;
	if (!ofs) return 0;
	ofs.close();
	return 1; 
}
static int writeFloatFile(const std::string & fn, float val) {
	std::ofstream ofs(fn, std::ofstream::out);
	if (!ofs) return 0;
	ofs << val;
	if (!ofs) return 0;
	ofs.close();
	return 1;
}

void SessionStatsPlugin::writeStats() {
	std::string obsDir = cvarManager->getCvar("cl_sessionstats_obs_directory").getStringValue();
	if (stats.find(currentPlaylist) == stats.end()) {

		writeIntFile(obsDir + "\\wins.txt", 0) &&
		writeIntFile(obsDir + "\\losses.txt", 0) &&
		writeIntFile(obsDir + "\\streak.txt", 0) &&
		writeFloatFile(obsDir + "\\mmrChange.txt", 0.0f);
		return;
	}
	writeIntFile(obsDir + "\\wins.txt", stats[currentPlaylist].wins) &&
	writeIntFile(obsDir + "\\losses.txt", stats[currentPlaylist].losses) &&
	writeIntFile(obsDir + "\\streak.txt", stats[currentPlaylist].streak) &&
	writeFloatFile(obsDir + "\\mmrChange.txt", stats[currentPlaylist].currentMMR - stats[currentPlaylist].initialMMR);

}


void SessionStatsPlugin::Render(CanvasWrapper canvas)
{
	bool screenOut = cvarManager->getCvar("cl_sessionstats_display_stats").getBoolValue();
	if (!(screenOut))
		return;
	static int count = 0, delta = 1;
#define STEP 5
	count += STEP;
	if (count < STEP)
		delta = 1;
	if (count > (255 - STEP))
		delta = -STEP;

	int w, l, s;
	float mmrDelta;
	if (currentPlaylist != -1 && (stats.find(currentPlaylist) != stats.end())) {
		w = stats[currentPlaylist].wins;
		l = stats[currentPlaylist].losses;
		s = stats[currentPlaylist].streak;
		mmrDelta = stats[currentPlaylist].currentMMR - stats[currentPlaylist].initialMMR;
	}
	else {
		w = l = s = 0;
		mmrDelta = 0.0f;
	}
#define SPACER 200
	Vector2 sz = canvas.GetSize();
	canvas.SetColor(0, 255, 0, 255);
	canvas.SetPosition(Vector2{ 50,sz.Y-350 });
	canvas.DrawString("Wins: " + std::to_string(w),3,3);

	canvas.SetColor(255, 0, 0, 255);
	canvas.SetPosition(Vector2{ 50, sz.Y-300 });
	canvas.DrawString("Losses: " + std::to_string(l),3,3);

	canvas.SetColor(0, 0, 255, 255);
	canvas.SetPosition(Vector2{ 50, sz.Y-250 });
	std::string streak = "Streak :";
	streak += ((s > 0) ? "+" : "");
	streak += std::to_string(s);
	canvas.DrawString(streak, 3,3);

	canvas.SetColor(255, 0, 0, 255);
	canvas.SetPosition(Vector2{ 50, sz.Y - 200 });
	canvas.DrawString("MMR +/-: " + std::to_string(mmrDelta), 3, 3);

	canvas.SetColor(255, count, 255-count, 255);
	canvas.SetPosition(Vector2{ 50, sz.Y - 150 });
	//canvas.DrawString("PLUGIN BY MEGASPLAT" + s, 10, 10);






	//canvas.SetPosition(Vector2{ (int)(maxx / 2.0f + yaw / 2.0 * (float)(maxx - minx)), (int)(maxy / 2.0f + pitch / 2.0 * (float)(maxy - miny)) });
	//canvas.DrawLine(Vector2{ (int)(maxx / 2.0f), (int)(maxy / 2.0f) }, Vector2{ (int)(maxx / 2.0f + yaw /2.0 * (float)(maxx - maxy)), (int)(maxy / 2.0f + pitch /2.0 * (float)(maxy - miny)) });

	//canvas.DrawBox(Vector2{ 10, 10 });
	// draw second box
}



void SessionStatsPlugin::onUnload()
{
	//  TODO: call gameWrapper->UnhookEvent() for events?
	gameWrapper->UnregisterDrawables();
	gameWrapper->UnhookEvent("Function GameEvent_TA.Countdown.BeginState");
	gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed");
}
