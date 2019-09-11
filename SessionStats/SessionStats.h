#pragma once
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

#include "bakkesmod/plugin/bakkesmodplugin.h"
#pragma comment( lib, "bakkesmod.lib" )
#include <map> 
#include <string>

typedef struct {
	float initialMMR, currentMMR;
	int wins, losses, streak;
	int tier, div;
} StatsStruct;

class SessionStatsPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	std::map<int, StatsStruct> stats;
	int currentPlaylist;
	SteamID mySteamID;

	//std::shared_ptr<bool> obsOut, screenOut; // write OBS text files, write stats on game canvas
	//std::shared_ptr<std::string> obsDir;

	void writeStats();
	void logStatusToConsole(std::string oldValue, CVarWrapper cvar);
	void updateStats(int retryCount);
	int teamNumber;
public:
	virtual void onLoad();
	virtual void onUnload();

	void StartGame(std::string eventName);
	void EndGame(std::string eventName);
	void onMainMenu(std::string eventName);
	void Render(CanvasWrapper canvas);
	void ResetStats();
};

