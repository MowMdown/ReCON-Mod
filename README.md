# ReCON-Mod

The DayZ server-side companion mod for [ReCON](https://github.com/MowMdown/ReCON).
ReCON shows what it sends on its Live Map page; the page, the API the mod
talks to and the map pictures are documented in
[ReCON's README](https://github.com/MowMdown/ReCON#live-map-live).

> **Monetized servers need Bohemia Interactive's approval.** The MIT
> licence lets you use ReCON-Mod on any server, but it gives no permission
> from Bohemia Interactive to monetize a DayZ server, or to monetize other
> mods or content on it. If your server makes money, getting Bohemia's
> approval (and the other authors' permission) is up to you: see
> Bohemia's [DayZ monetization policy](https://www.bohemia.net/en/monetization/dayz)
> and [NOTICE](NOTICE).

| Path | Contents |
|---|---|
| `ReCON/` | The DayZ mod (Enforce script, built on CF), packed into `@ReCON/addons/ReCON.pbo` |
| `cmd/pbo/`, `pbo/` | The packer that builds `@ReCON/addons/ReCON.pbo` (see Build) |
| `.github/workflows/build.yml` | Builds and tests on every push; a `v*` tag publishes a release |

The mod, on the DayZ server, sends ReCON every second every online player's live state: position,
health, blood, shock, food, water, heat, wetness, bleeding, illnesses,
whether they are knocked out or restrained, the item in hand, the vehicle
they're in, and their kills and deaths this session. While a player is open
on the Live Map, it also sends everything they carry: each slot's item, and
inside it every attachment and every item in its cargo, with its condition,
quantity, wetness and temperature. It also runs ReCON's
actions on them (heal, dry, kill, teleport, set a stat, set an item's condition, on-screen messages, spawn
any item the server has loaded on the ground in front of them, or a
vehicle, whole) and
on the world (weather, the in-game date and time).

- **Server-side only.** It loads with `-servermod`, so players download
  nothing and no keys are needed.
- **Needs [Community Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036)**
  (CF), loaded with `-mod` or `-servermod`, before ReCON-Mod.
- **Talks to ReCON over HTTP.** The mod posts to ReCON's web port, and
  ReCON's reply carries the commands to run. The game server needs no
  open ports, but it must be able to reach ReCON.
- **Proves it's your server with the RCON password**, which ReCON already
  has. It is read from BattlEye's config, or set in the mod's settings.

## Build

The mod is the `ReCON` folder: scripts and a plain-text `config.cpp`, packed
into a PBO with the prefix `ReCON`. It needs no binarizing, so any of these
work:

- **Download it.** Every push builds it on GitHub: open the repository's
  **Actions** tab, the latest **Build the mod** run, and download **@ReCON**
  under Artifacts. A tag such as `v0.1.0` also publishes `ReCON-Mod.zip` as a
  release.
- **This repository's packer**, on Windows, macOS or Linux, with
  [Go](https://go.dev/dl/) installed:

  ```bash
  go run ./cmd/pbo
  ```

  It writes the mod folder:

  ```
  @ReCON/
    addons/
      ReCON.pbo
    mod.cpp
    LICENSE
    NOTICE
  ```

- **DayZ Tools** (free on Steam, Windows): open **Addon Builder**, set the
  source to this repository's `ReCON` folder and the destination to
  `@ReCON\addons`, set **Addon prefix** to `ReCON`, leave
  **Binarize** off and **Sign** off (a server-side mod needs no keys), and
  click **Pack**. PBO Manager (Pack, with the prefix set to `ReCON`) works
  too.

## Install

1. Put the `@ReCON` folder in the server folder, next to `@CF`.
2. Add it to the server's command line after CF:
   `-servermod=@CF;@ReCON` (or keep CF in `-mod=` if the players load it).
3. Start the server once. The mod writes `ReCON/ReCON.json` in the server's
   profile folder (`-profiles=`) and logs `[ReCON] wrote the default settings`.
4. Set `url` to ReCON's address as the game server reaches it, then restart.

```json
{
    "url": "http://10.0.1.20:9996",
    "password": "",
    "battlEyePath": "",
    "intervalMs": 1000
}
```

| Setting | What it is |
|---|---|
| `url` | ReCON's web address and port (the container's published port). |
| `password` | The RCON password. Leave it empty to read `RConPassword` from the `BEServer_x64_active_*.cfg` BattlEye runs from (not `BEServer_x64.cfg`; found from `-BEpath`, the profile's `BattlEye` folder, or `battleye` in the server folder). |
| `battlEyePath` | The folder BattlEye's config is in, if the mod doesn't find it. |
| `intervalMs` | How often to send player data and pick up commands, in milliseconds. The minimum is 250. |

The server's script log (`script_*.log` in the profile folder) shows
the mod's `[ReCON]` lines: where it sends data, whether ReCON answers, and
each action it runs.

## How it works

Each sync is one `POST <url>api/mod/sync` with JSON:

```json
{
  "v": 1, "mod": "0.0.1d", "key": "<RCON password>", "world": "chernarusplus",
  "year": 2026, "month": 11, "day": 20, "hour": 14, "minute": 5,
  "players": [{ "steamId": "7656…", "name": "…", "x": 0, "y": 0, "z": 0, "heading": 0,
                "alive": true, "health": 100, "blood": 5000, "shock": 100,
                "water": 2500, "energy": 9000, "heat": 0.1, "wet": 0, "stamina": 100,
                "bleeding": 0, "agents": 0, "brokenLegs": 0,
                "unconscious": false, "restrained": false,
                "hands": "M4A1", "vehicle": "", "kills": 0, "deaths": 0 }],
  "events": [{ "type": "kill", "victim": "7656…", "killer": "7656…", "weapon": "M4A1", "cause": "player", "agents": 0, "distance": 120.5, "…": "…" }],
  "results": [{ "id": 3, "ok": true, "error": "" }]
}
```

ReCON answers with:

```json
{ "ok": true, "error": "", "interval": 0,
  "commands": [{ "id": 4, "action": "heal", "steamId": "7656…", "target": "",
                 "x": 0, "y": 0, "z": 0, "stat": "", "value": 0,
                 "text": "", "title": "", "seconds": 0 }] }
```

Every 5 seconds a sync also has `vehiclesSent: true` and `vehicles`: every
car, motorbike and boat on the server (`id`, `type`, `name`, `x`, `z`,
`heading`, `health`, `fuel`, `crew`, `engine`). For each id in the reply's
`watchVehicles` (ReCON asks once, when a vehicle is clicked on the Live
Map), the next sync's `vehicleDetails` has that vehicle's damage zones
(`name`, `health`) and its parts and cargo (`items`, as in an inventory).
`deleteVehicle` (`text`: its id) deletes one, not while anyone is in it. `spawnAt` (`text`: a class from the item list, `value`: 1 to 20, `x`, `z`) spawns at a spot on the map: items in a row on the ground, a vehicle whole, facing north (a boat on the water at sea level if the spot is sea), animals in a row 1.5 m apart (recon-mod 0.0.1k on).
With the vehicles come `worldEvents`: helicopter crashes (`CrashBase`) and
areas (`EffectArea`: toxic zones), each with `id`, `type`, `kind` (crash,
gas or area), `x`, `z` and `radius`. When the reply has `wantPlaces` (once
a connection), the next sync has `placesSent: true` and `places`: the
map's place names from `CfgWorlds <world> Names` (`name`, `type`, `x`, `z`).

It also sends `weather`: the overcast, rain, fog, snow, wind speed and
direction now, where each is heading, the weather mode, and whether time
is frozen.

A death's `cause` is what killed them: `player`, `suicide` (the suicide
emote), `admin` (ReCON's Kill), `vehicle`, `infected`, `animal`,
`explosion`, `trap`, `fall`, `fire`, `drowned`, `bleeding` (blood below
DayZ's fatal level), `dehydration`, `starvation`, `cold`, `illness`, or
empty when it can't tell. A death no one caused (DayZ passes the victim as
the killer) goes by what last hurt them if it was in the last 3 seconds (a
fall, a fire), then by the state they died in, the way DayZ's modifiers
kill. `agents` is the illnesses they had (eAgents bits). `weapon` is never
their own body, and what is in their hands only for a player who killed
them or for a suicide.

Each command's result comes back in the next sync. Events and results
that ReCON didn't get, because it was down or refused them, are sent again.

| Action | Fields |
|---|---|
| `heal` | Full health, blood, shock, food, water and stamina. Stops bleeding, cures illnesses and their symptoms, mends broken legs and wakes them if knocked out. |
| `dry` | Their wetness to none, and every item they carry (clothes above all) dry. |
| `condition` | `text`: the item's `id` (from their inventory), `value`: 0 pristine, 1 worn, 2 damaged, 3 badly damaged, 4 ruined. Only an item they carry. Set to the health DayZ gives an item it steps down to that level. |
| `kill` | — |
| `teleport` | `x`, `z`, and `y` (0 = ground level). |
| `teleportTo` | `target`: the SteamID64 of the player to go to. |
| `set` | `stat` (`health`, `blood`, `water`, `energy` or `heatBuffer`) and `value`. Shock, stamina and temperature are read only. |
| `message` | `text`, plus optional `title` and `seconds`. Shown as an in-game notification. |
| `spawn` | `text` (a class from the item list), `value` (1 to 20) and `stat`, where: `ground` (or empty) in front of them, `inventory` (anywhere free in their clothing and containers; what doesn't fit goes on the ground) or `hands` (one, into empty hands). A vehicle (car, motorbike, boat) is one, 8 m in front of them and facing their way, made whole the way DayZ's developer spawn and COT make one: its `OnDebugSpawn` fits wheels, doors and engine parts, then a car's fuel, oil, brake fluid and coolant are filled. A modded vehicle is in the list too; it comes whole if its mod gives it an `OnDebugSpawn` (most do, for admin tools), otherwise with what DayZ's default fits from its config. An animal (recon-mod 0.0.1k on; `kind` `animal` in the item list) goes on the ground 4 m in front of them, 1.5 m apart, its AI running; `stat` is ignored. |

World actions, modelled on Community Online Tools' weather module:

| Action | Fields |
|---|---|
| `weather` | `stat` (`overcast`, `rain`, `fog`, `snow` from 0 to 1, `windSpeed` in m/s, `windDir` in degrees), `value`, `transition` (seconds to get there), `hold` (seconds to stay at least). |
| `weatherMode` | `text`: `world` (the map's weather rules), `forecast` (only what was set) or `static` (frozen). |
| `storm` | `value` (lightning density 0-1), `min` (the overcast it starts at), `hold` (seconds between strikes). |
| `rainThresholds` | `min` and `max` (the overcast range rain falls in), `transition` (seconds to stop). |
| `date` | `year`, `month`, `day`, `hour`, `minute`. |
| `freezeTime` | `value`: 1 stops the clock, 0 starts it. |

## Credits and licences

ReCON-Mod's own code is under the [MIT License](LICENSE); a monetized
server still needs Bohemia Interactive's approval (above). [NOTICE](NOTICE)
lists what it builds on or learned from, and their licences:

- **DayZ** (Bohemia Interactive). The mod uses DayZ's script API, and
  values from DayZ's scripts such as the player stats' ranges. DayZ's scripts are under the
  [DayZ Public License](https://www.bohemia.net/en/licenses/dayz-public-license-dpl):
  attribution, non-commercial, DayZ only, for any of that material used
  here. Keep your use of the mod within it, and within Bohemia's rules for
  servers. This project is not
  made or endorsed by Bohemia Interactive.
- **[Community Framework](https://github.com/Jacob-Mango/DayZ-Community-Framework)**
  by Jacob_Mango and Arkensor (LGPL 2.1), a dependency loaded on the server.
  It isn't included here.
- **[Community Online Tools](https://github.com/Jacob-Mango/DayZ-CommunityOnlineTools)**
  by Jacob_Mango and contributors (CC BY-SA 4.0). The heal action and the
  weather and time controls are modelled on how COT does them, and written
  here with DayZ's own API. No COT code is copied.
