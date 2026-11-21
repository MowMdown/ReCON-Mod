// What the mod and ReCON say to each other: one POST to <url>api/mod/sync
// every intervalMs. The mod sends every online player's state, what
// happened since the last sync (kills) and how the commands from the last
// reply went; ReCON answers with the commands to run next. The field names
// are the JSON keys, so they must match ReCON's (internal/modlink).

const int RECON_PROTOCOL = 1;

class ReCON_Sync
{
	int v = RECON_PROTOCOL;
	string mod = "0.0.1m";
	string key;      // the RCON password
	string world;    // "chernarusplus"
	int year;        // the in-game date and time
	int month;
	int day;
	int hour;
	int minute;
	ref ReCON_World weather = new ReCON_World();
	ref array<ref ReCON_PlayerState> players = new array<ref ReCON_PlayerState>();
	ref array<ref ReCON_Event> events = new array<ref ReCON_Event>();
	ref array<ref ReCON_Result> results = new array<ref ReCON_Result>();
	// The items the server can spawn, sent a page at a time while ReCON asks
	// (the reply's catalogNext): catalog holds entries catalogAt onwards of
	// catalogTotal.
	int catalogTotal;
	int catalogAt;
	ref array<ref ReCON_CatalogItem> catalog = new array<ref ReCON_CatalogItem>();
	// Every vehicle on the server, sent every few seconds (vehiclesSent
	// says this sync has them).
	bool vehiclesSent;
	ref array<ref ReCON_Vehicle> vehicles = new array<ref ReCON_Vehicle>();
	// The map's dynamic events (helicopter crashes, toxic zones), with the
	// vehicles.
	ref array<ref ReCON_WorldEvent> worldEvents = new array<ref ReCON_WorldEvent>();
	// The bears, wolves and guard dogs alive on the server, every sync
	// (predatorsSent says this mod sends them).
	bool predatorsSent;
	ref array<ref ReCON_Predator> predators = new array<ref ReCON_Predator>();
	// The map's place names, when the reply asked for them (wantPlaces).
	bool placesSent;
	ref array<ref ReCON_Place> places = new array<ref ReCON_Place>();
	// The vehicles ReCON asked to see inside (the reply's watchVehicles).
	ref array<ref ReCON_VehicleDetail> vehicleDetails = new array<ref ReCON_VehicleDetail>();
};

class ReCON_Predator
{
	int id;
	string type;     // class name
	string kind;     // bear | wolf | dog
	float x;
	float z;
	float heading;   // degrees, 0 = north, clockwise
};

class ReCON_WorldEvent
{
	int id;
	string type;     // class name
	string kind;     // crash | gas | area
	float x;
	float z;
	float radius;    // an area's, in metres (0 for a crash)
};

class ReCON_Place
{
	string name;
	string type;     // Capital, City, Village, Local, Marine, Hill, ViewPoint, Camp, Ruin, RailroadStation, LocalOffice...
	float x;
	float z;
};

class ReCON_VehicleDetail
{
	int id;
	ref array<ref ReCON_Zone> zones = new array<ref ReCON_Zone>(); // its damage zones: engine, fuel tank, body...
	ref array<ref ReCON_Item> items = new array<ref ReCON_Item>(); // its parts (attachments) and cargo
};

class ReCON_Zone
{
	string name;
	float health;    // 0-1
};

class ReCON_Vehicle
{
	int id;          // its object id (deleteVehicle's text)
	string type;     // class name
	string name;     // display name
	float x;
	float z;
	float heading;   // degrees, 0 = north
	float health;    // 0-1
	float fuel;      // 0-1, -1 when it isn't a car
	int crew;        // how many are in it
	bool engine;     // the engine is running
};

class ReCON_CatalogItem
{
	string type;  // class name
	string name;  // display name
	string kind;  // weapon | magazine | ammo | clothing | vehicle | animal | item
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
	float staminaMax;    // DayZ's stamina maximum (100 by default)
	float staminaCap;    // the most they can have now: carried weight lowers it
	int bleeding;    // open wounds
	int agents;      // illnesses, eAgents bits
	int brokenLegs;  // 0 no, 1 broken, 2 splinted
	bool unconscious;
	bool restrained;
	string hands;    // class name of the item in hand
	string vehicle;  // class name of the vehicle they are in
	string handsName;   // their names as the game shows them ("Wood Axe")
	string vehicleName;
	int kills;       // this session, as the mod saw them
	int deaths;
	// Everything they carry, only while ReCON is watching them (the reply's
	// watch list); inv says whether inventory was read (it may be empty).
	bool inv;
	ref array<ref ReCON_Item> inventory = new array<ref ReCON_Item>();
};

// The weather now, and where each part is heading (the forecast).
// ReCON_Item is one item a player carries, with what is attached to it and
// what is in its cargo.
class ReCON_Item
{
	int id;              // the entity's ID this session (stays put between syncs)
	string type;         // class name
	string name;         // display name
	string slot;         // the slot it is attached in ("Back", "magazine"; "Hands"), "" in cargo
	int row;             // in cargo: where (-1 when attached)
	int col;
	float quantity;      // stack, rounds, ml...; quantityMax 0 when it has none
	float quantityMax;
	string unit;         // "rounds", "ml" or ""
	float health;
	float healthMax;
	int healthLevel;     // 0 pristine, 1 worn, 2 damaged, 3 badly damaged, 4 ruined
	float wet;           // 0-1
	float temperature;   // degrees C
	ref array<ref ReCON_Item> attachments = new array<ref ReCON_Item>();
	ref array<ref ReCON_Item> cargo = new array<ref ReCON_Item>();
};

class ReCON_World
{
	float overcast;      // 0-1
	float rain;          // 0-1
	float fog;           // 0-1
	float snow;          // 0-1
	float windSpeed;     // m/s
	float windDir;       // degrees
	float overcastTarget;
	float rainTarget;
	float fogTarget;
	float snowTarget;
	float windSpeedTarget;
	float windDirTarget;
	string mode;         // world: the map's weather; forecast: only what was set; static: frozen
	bool timeFrozen;
	float sunrise;       // today's sunrise and sunset on this map, in hours (5.5 = 05:30)
	float sunset;
};

class ReCON_Event
{
	string type;       // "kill"
	string victim;     // SteamID64 ("" for an AI or animal)
	string victimName;
	string killer;     // SteamID64 ("" for the environment, an AI or animal)
	string killerName;
	string weapon;     // class name
	// What killed them: player, suicide, admin, vehicle, infected, animal,
	// explosion, trap, fall, fire, drowned, bleeding, dehydration,
	// starvation, cold, illness, or "" when it can't tell.
	string cause;
	int agents;        // the illnesses they had (eAgents bits)
	float distance;
	float x;           // where the victim died
	float y;
	float z;
	int at;            // seconds since the mod started (ReCON stamps the real time)
};

class ReCON_Command
{
	int id;
	// On the server: deleteVehicle (text: its id), spawnAt (text: the class,
	// value: how many, x, z: where; a vehicle is one, whole).
	// On a player: heal | dry | kill | teleport | teleportTo | set | message | spawn |
	// condition (text: the item's id, value: 0 pristine to 4 ruined).
	// On the world: weather | weatherMode | storm | rainThresholds | date | freezeTime.
	string action;
	string steamId;  // the player it acts on
	string target;   // teleportTo: the player to go to
	float x;         // teleport: where (y 0 = ground level)
	float y;
	float z;
	string stat;     // set: health | blood | shock | water | energy; spawn: where (ground | inventory | hands)
	float value;
	string text;     // message
	string replace;  // spawn: a vehicle (persistent ID) to delete once the new one exists, crew put out first
	string title;
	float seconds;
	// weather: stat is overcast | rain | fog | snow | windSpeed | windDir (degrees),
	// value where it goes, over transition seconds, staying at least hold seconds.
	float transition;
	float hold;
	// weatherMode: text is world | forecast | static. freezeTime: value 1 or 0.
	// storm: value is the density (0-1), min the overcast lightning starts
	// at, hold the seconds between strikes.
	// rainThresholds: rain falls when overcast is between min and max;
	// transition is how long it takes to stop.
	float min;
	float max;
	// spawn: text is the class name, value how many (on the ground in
	// front of the player).
	// date: the in-game date and time.
	int year;
	int month;
	int day;
	int hour;
	int minute;
};

class ReCON_Result
{
	int id;
	bool ok;
	string error;
	string entity;   // a vehicle spawn's new vehicle, by persistent ID (b1:b2:b3:b4); "" otherwise
};

class ReCON_Reply
{
	bool ok;
	string error;
	int interval;    // ms; 0 keeps the configured one
	ref array<string> watch = new array<string>(); // SteamID64s to send the inventory of
	ref array<int> watchVehicles = new array<int>(); // vehicle ids to send the parts and cargo of
	bool wantPlaces; // send the map's place names
	int catalogNext; // 1 + the first catalog entry ReCON wants next; 0 none
	ref array<ref ReCON_Command> commands = new array<ref ReCON_Command>();
};
