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
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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


void SessionStatsPlugin::onLoad() {

	std::string obsDir = "F:/Downloads";
	int pos = 0;
	while ((pos = obsDir.find("/", pos)) != std::string::npos) {
		obsDir.replace(pos, 1, "\\");
		pos += 1;
	}
	cvarManager->log("OUTPUT DIR:");
	cvarManager->log(obsDir);
	// log startup to console
	std::stringstream ss;
	ss << exports.pluginName << " version: " << exports.pluginVersion;
	cvarManager->log(ss.str());

	// create cvars
	cvarManager->registerCvar("cl_sessionstats_obs_output", "0", "Output text stats to files to be used as OBS sources", true, true, 0, true, 1, true); 
	cvarManager->registerCvar("cl_sessionstats_obs_directory", "bakkesmod/data", "Directory to write OBS text output files to (use forward slash '/' as separator in path", true, false, (0.0F), false, (0.0F), true); //.bindTo(obsDir);
	cvarManager->registerCvar("cl_sessionstats_display_stats", "0", "Display session stats on screen", true, true, 0, true, 1, true); 

	// hook cvars
	cvarManager->registerNotifier("cl_sessionstats_reset", [this](std::vector<string> params) {
		ResetStats();
	}, "Start a fresh stats session", PERMISSION_ALL);
	cvarManager->getCvar("cl_sessionstats_obs_output").addOnValueChanged(std::bind(&SessionStatsPlugin::logStatusToConsole, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("cl_sessionstats_obs_directory").addOnValueChanged(std::bind(&SessionStatsPlugin::logStatusToConsole, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("cl_sessionstats_display_stats").addOnValueChanged(std::bind(&SessionStatsPlugin::logStatusToConsole, this, std::placeholders::_1, std::placeholders::_2));

	// init state
	currentPlaylist = -1;
	ResetStats();

	// hook events - still need to handle "rage quit" case
	gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState", bind(&SessionStatsPlugin::StartGame, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));
	//gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));
	//gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", bind(&SessionStatsPlugin::EndGame, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.OnlineGame_TA.OnMainMenuOpened", bind(&SessionStatsPlugin::onMainMenu, this, std::placeholders::_1));
	gameWrapper->RegisterDrawable(std::bind(&SessionStatsPlugin::Render, this, std::placeholders::_1));
}

void SessionStatsPlugin::ResetStats() {
	stats.clear();
	if (cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue())
		writeStats();
}

void SessionStatsPlugin::onMainMenu(std::string eventName) {
	cvarManager->log("=======Main Menu=======");
	cvarManager->log("=======================");
	updateStats();
}
void SessionStatsPlugin::StartGame(std::string eventName) {
	cvarManager->log("====BeginState==============================");

	if (!gameWrapper->IsInOnlineGame())
		return;
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
		if (cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue())
			writeStats();
	}
	else {
		cvarManager->log("ServerWrapper is NULL?");
	}
}

void SessionStatsPlugin::EndGame(std::string eventName) {
	updateStats();
}

void SessionStatsPlugin::updateStats() {
	cvarManager->log("===EndGame==================================");

	if (stats.count(currentPlaylist) != 0) {
		cvarManager->log("Updating current playlist");
		gameWrapper->SetTimeout([&](GameWrapper* gameWrapper) {
			bool gotNewMMR = false;
			int count = 0;
			float mmr = -1.0f;
			bool writeObs = cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue();

			while (!gotNewMMR && (count < 10)) {// only try 10 times, then give up
				//ss.clear();
				mmr = gameWrapper->GetMMRWrapper().GetPlayerMMR(mySteamID, currentPlaylist);
				//ss << "Got updated MMR " << mmr << " old MMR was: " << stats[currentPlaylist].currentMMR;
				//cvarManager->log(ss.str());

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
				cvarManager->log("slept");
			}
			stats[currentPlaylist].currentMMR = mmr;
			std::stringstream ss;
			ss << "writeObs: " << writeObs << " write to: " << cvarManager->getCvar("cl_sessionstats_obs_directory").getStringValue();
			cvarManager->log(ss.str());
			if (writeObs)
				writeStats();
			//ss.clear();
			//ss << "======== w:" << stats[currentPlaylist].wins << " l:" << stats[currentPlaylist].losses << " MMR: " << (stats[currentPlaylist].currentMMR - stats[currentPlaylist].initialMMR) << " currentPlaylist: " << currentPlaylist;
			//cvarManager->log(ss.str());
			//cvarManager->log("================================================");

		}, 3);
	}
	else {
		cvarManager->log("Current Playlist MMR not stored?");
	}

	
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
	std::stringstream ss;
	
	std::string obsDir = cvarManager->getCvar("cl_sessionstats_obs_directory").getStringValue();
	if (obsDir == "") // if field is empty, write to current directory inside RL folder
		obsDir = ".";
	std::string::size_type pos = 0u;
	while ((pos = obsDir.find("/", pos)) != std::string::npos) {
		obsDir.replace(pos, 1, "\\");
		pos += 1;
	}

	ss << "Writing file to " << obsDir + "\\wins.txt";
	cvarManager->log(ss.str());
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


void SessionStatsPlugin::Render(CanvasWrapper canvas) {
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

	// lol credits
	//canvas.DrawString("PLUGIN BY MEGASPLAT" + s, 10, 10);
	//canvas.SetPosition(Vector2{ (int)(maxx / 2.0f + yaw / 2.0 * (float)(maxx - minx)), (int)(maxy / 2.0f + pitch / 2.0 * (float)(maxy - miny)) });
	//canvas.DrawLine(Vector2{ (int)(maxx / 2.0f), (int)(maxy / 2.0f) }, Vector2{ (int)(maxx / 2.0f + yaw /2.0 * (float)(maxx - maxy)), (int)(maxy / 2.0f + pitch /2.0 * (float)(maxy - miny)) });
	//canvas.DrawBox(Vector2{ 10, 10 });
	// draw second box
}

void SessionStatsPlugin::logStatusToConsole(std::string oldValue, CVarWrapper cvar) {
	std::stringstream ss;
	ss << "cl_sessionstats_obs_directory: '" << cvarManager->getCvar("cl_sessionstats_obs_directory").getStringValue() << 
		"' cl_sessionstats_display_stats: " << cvarManager->getCvar("cl_sessionstats_display_stats").getBoolValue() <<
		" cl_sessionstats_obs_output: " << cvarManager->getCvar("cl_sessionstats_obs_output").getBoolValue();
	cvarManager->log(ss.str());
}

void SessionStatsPlugin::onUnload()
{
	//  TODO: call gameWrapper->UnhookEvent() for events?
	gameWrapper->UnregisterDrawables();
	gameWrapper->UnhookEvent("Function GameEvent_TA.Countdown.BeginState");
	gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed");
}
