#pragma once
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
public:
	virtual void onLoad();
	virtual void onUnload();

	void StartGame(std::string eventName);
	void EndGame(std::string eventName);
	void Render(CanvasWrapper canvas);
	void ResetStats();
};

