// What the mod and ReCON say to each other: one POST to <url>api/mod/sync
// every intervalMs. The mod sends every online player's state, what
// happened since the last sync (kills) and how the commands from the last
// reply went; ReCON answers with the commands to run next. The field names
// are the JSON keys, so they must match ReCON's (internal/modlink).

const int RECON_PROTOCOL = 1;

class ReCON_Sync
{
	int v = RECON_PROTOCOL;
	string mod = "0.1.0";
	string key;      // the RCON password
	string world;    // "chernarusplus"
	int year;        // the in-game date and time
	int month;
	int day;
	int hour;
	int minute;
	ref array<ref ReCON_PlayerState> players = new array<ref ReCON_PlayerState>();
	ref array<ref ReCON_Event> events = new array<ref ReCON_Event>();
	ref array<ref ReCON_Result> results = new array<ref ReCON_Result>();
};

class ReCON_PlayerState
{
	string steamId;  // SteamID64; ReCON works out the BattlEye GUID from it
	string name;
	float x;         // position: x east, y height, z north (metres)
	float y;
	float z;
	float heading;   // degrees, 0 = north
	bool alive;
	float health;    // 0-100
	float blood;     // 0-5000
	float shock;     // 0-100 (low knocks out)
	float water;     // 0-5000
	float energy;    // 0-5000 (food)
	float heat;      // heat comfort, -1 (freezing) .. 1 (overheating)
	float heatBuffer;    // the heat buff ("comfort"), 0 (none) .. heatBufferMax
	float heatBufferMax;
	float heatBufferCap; // how much of it their clothes allow, 0.35 .. 1
	int heatBufferStage; // the HUD's buff icon: 0 none, 1-3
	int wet;         // 0 dry, 1 wet
	float stamina;
	int bleeding;    // open wounds
	int agents;      // illnesses, eAgents bits
	int brokenLegs;  // 0 no, 1 broken, 2 splinted
	bool unconscious;
	bool restrained;
	string hands;    // class name of the item in hand
	string vehicle;  // class name of the vehicle they are in
	int kills;       // this session, as the mod saw them
	int deaths;
};

class ReCON_Event
{
	string type;       // "kill"
	string victim;     // SteamID64 ("" for an AI or animal)
	string victimName;
	string killer;     // SteamID64 ("" for the environment, an AI or animal)
	string killerName;
	string weapon;     // class name
	float distance;
	float x;           // where the victim died
	float y;
	float z;
	int at;            // seconds since the mod started (ReCON stamps the real time)
};

class ReCON_Command
{
	int id;
	string action;   // heal | kill | teleport | teleportTo | set | message
	string steamId;  // the player it acts on
	string target;   // teleportTo: the player to go to
	float x;         // teleport: where (y 0 = ground level)
	float y;
	float z;
	string stat;     // set: health | blood | shock | water | energy
	float value;
	string text;     // message
	string title;
	float seconds;
};

class ReCON_Result
{
	int id;
	bool ok;
	string error;
};

class ReCON_Reply
{
	bool ok;
	string error;
	int interval;    // ms; 0 keeps the configured one
	ref array<ref ReCON_Command> commands = new array<ref ReCON_Command>();
};
