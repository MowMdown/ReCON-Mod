# ReCON-Mod

The DayZ server-side companion mod for [ReCON](https://github.com/MowMdown/ReCON),
and ReCON's side of it (the Live Map page and the API the mod talks to).

> **Not for monetized servers.** ReCON-Mod must not be used on, or to run,
> support or promote, any DayZ server that makes money: paid access, paid
> priority queue or whitelist, selling in-game items, perks, kits, bases
> or vehicles, donation rewards, or any other paid benefit. Use it only on
> servers that are free to play and earn nothing. (This is the author's
> condition of use, stated alongside the MIT licence; it also follows the
> non-commercial terms of the DayZ Public License that DayZ's scripts are
> under.)

| Path | Contents |
|---|---|
| `ReCON/` | The DayZ mod (Enforce script, built on CF), packed into `@ReCON/addons/ReCON.pbo` |
| `panel/` | A Go package ReCON builds in on request: the Live Map page (`templates/live.html`), the API, the command queue and the kills table |
| `cmd/pbo/`, `pbo/` | The packer that builds `@ReCON/addons/ReCON.pbo` (see Build) |
| `.github/workflows/build.yml` | Builds and tests on every push; a `v*` tag publishes a release |

The mod, on the DayZ server, sends ReCON every second every online player's live state: position,
health, blood, shock, food, water, heat, wetness, bleeding, illnesses,
whether they are knocked out or restrained, the item in hand, the vehicle
they're in, and their kills and deaths this session. It also runs ReCON's
actions on them (heal, kill, teleport, set a stat, on-screen messages) and
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
| `password` | The RCON password. Leave it empty to read `RConPassword` from BattlEye's `BEServer_x64*.cfg` (found from `-BEpath`, the profile's `BattlEye` folder, or `battleye` in the server folder). |
| `battlEyePath` | The folder BattlEye's config is in, if the mod doesn't find it. |
| `intervalMs` | How often to send player data and pick up commands, in milliseconds. The minimum is 250. |

The server's script log (`script_*.log` in the profile folder) shows
the mod's `[ReCON]` lines: where it sends data, whether ReCON answers, and
each action it runs.

## How it works

Each sync is one `POST <url>api/mod/sync` with JSON:

```json
{
  "v": 1, "mod": "0.1.0", "key": "<RCON password>", "world": "chernarusplus",
  "year": 2026, "month": 11, "day": 20, "hour": 14, "minute": 5,
  "players": [{ "steamId": "7656…", "name": "…", "x": 0, "y": 0, "z": 0, "heading": 0,
                "alive": true, "health": 100, "blood": 5000, "shock": 100,
                "water": 2500, "energy": 9000, "heat": 0.1, "wet": 0, "stamina": 100,
                "bleeding": 0, "agents": 0, "brokenLegs": 0,
                "unconscious": false, "restrained": false,
                "hands": "M4A1", "vehicle": "", "kills": 0, "deaths": 0 }],
  "events": [{ "type": "kill", "victim": "7656…", "killer": "7656…", "weapon": "M4A1", "distance": 120.5, "…": "…" }],
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

It also sends `weather`: the overcast, rain, fog, snow, wind speed and
direction now, where each is heading, the weather mode, and whether time
is frozen.

Each command's result comes back in the next sync. Events and results
that ReCON didn't get, because it was down or refused them, are sent again.

| Action | Fields |
|---|---|
| `heal` | Full health, blood, shock, food, water and stamina. Stops bleeding, cures illnesses and their symptoms, mends broken legs and wakes them if knocked out. |
| `kill` | — |
| `teleport` | `x`, `z`, and `y` (0 = ground level). |
| `teleportTo` | `target`: the SteamID64 of the player to go to. |
| `set` | `stat` (`health`, `blood`, `water`, `energy` or `heatBuffer`) and `value`. Shock, stamina and temperature are read only. |
| `message` | `text`, plus optional `title` and `seconds`. Shown as an in-game notification. |

World actions, modelled on Community Online Tools' weather module:

| Action | Fields |
|---|---|
| `weather` | `stat` (`overcast`, `rain`, `fog`, `snow` from 0 to 1, `windSpeed` in m/s, `windDir` in degrees), `value`, `transition` (seconds to get there), `hold` (seconds to stay at least). |
| `weatherMode` | `text`: `world` (the map's weather rules), `forecast` (only what was set) or `static` (frozen). |
| `storm` | `value` (lightning density 0-1), `min` (the overcast it starts at), `hold` (seconds between strikes). |
| `rainThresholds` | `min` and `max` (the overcast range rain falls in), `transition` (seconds to stop). |
| `date` | `year`, `month`, `day`, `hour`, `minute`. |
| `freezeTime` | `value`: 1 stops the clock, 0 starts it. |

## ReCON's side: the Live Map

ReCON has none of this unless it is built with it:
`docker build --build-arg RECON_MOD=true .` in the ReCON repository (see its
README). That copies `cmd/recon/_mod/recon_mod.go` in, which hands
`panel.Start` ReCON's database, the RCON password and its hooks for pages,
routes and nav links.

The **Live Map** page (`/live`, in the nav after Server Info):

- **Left:** every online player, with their map grid square, what's in their
  hands or the vehicle they're in, a health bar, and chips for bleeding,
  illness, knocked out and restrained. A search box filters the list.
- **Right:** the map, with each player as an arrow showing their heading,
  updated every second. Drag to pan, scroll to zoom; **Fit** zooms to
  everyone. It is a 1 km grid with the map's coordinates; to draw the
  terrain under it, put a picture of the whole map (square, north up) in
  ReCON's data folder as `maps/<world>.jpg` (or `.png`, `.webp`), such as
  `maps/chernarusplus.jpg`. Above it: the weather and the in-game time.
- **Clicking a player** opens their stats. Drag Health, Blood, Water, Food or
  Comfort left or right to set it (shock, stamina and temperature are read
  only). They show health, blood, water and food in
  DayZ's HUD colours (white, yellow, red, blinking red at the same
  thresholds as the game), shock, stamina, temperature (the game's hot and
  cold colours) and comfort (the heat buff, with its +/++/+++ stage and the
  most their clothes allow), conditions, a mini-map of the 1 km around them,
  what's in hand, vehicle, kills and deaths, and the actions: Heal,
  Teleport (to a spot, a mini-map click or another player), Message and
  Kill. Each action's result shows below them.
- **World…** sets the in-game date and time, freezes the clock, and sets the
  weather: overcast, rain, fog, snow, wind speed and direction, and
  lightning, with presets (Clear, Cloudy, Rain, Thunderstorm, Fog, Snow),
  over how many seconds the change happens and how long it holds; whether
  the weather follows the map's rules, only what was set, or is frozen; and
  the overcast range rain falls in.

While the mod is connected, ReCON's Server Info page takes the in-game date
from it (for sunrise and sunset) rather than from `SERVER_TIME`.

### The API

ReCON stores every death the mod reports (its own `mod_kills` table) and the SteamIDs it reports.

| Method | Path | Body | Description |
| ------ | ---- | ---- | ----------- |
| POST | `/api/mod/sync` | the mod's sync | Used by the mod only: player data in, commands out (403 with the wrong password) |
| GET  | `/api/mod/players` | | `mod` (`connected`, `lastSync`, `world`, in-game `date`), every online player's live state with their `guid`, and the recent commands |
| GET  | `/api/mod/players/<guid>` | | One player: `live` and `player` (when online), `kills`, `deaths`, `recentKills`, and the commands sent to them |
| POST | `/api/mod/players/<guid>/<action>` | see below | Queue an action. The mod runs it on its next sync; its `status` (`queued`, `sent`, `done`, `failed`, `expired`) shows in the commands |
| POST | `/api/mod/world/<action>` | see below | Queue a weather or time change, the same way |

Actions: `heal` (full health, blood, shock, food and water; stops bleeding, cures illnesses, mends broken legs), `kill`, `teleport` `{"x", "z", "y"}` (`y` 0 = ground level), `teleportTo` `{"target": "<guid>"}`, `set` `{"stat": "health|blood|water|energy|heatBuffer", "value"}` (shock, stamina and temperature are read only), `message` `{"text", "title", "seconds"}`. A command the mod hasn't answered within 30 seconds expires.

World actions, modelled on Community Online Tools' weather module (`mod.weather` in `/api/mod/players` shows the weather now and where it is heading):

- `weather` `{"stat", "value", "transition", "hold"}`: `stat` is `overcast`, `rain`, `fog`, `snow` (0 to 1), `windSpeed` (m/s) or `windDir` (degrees). It changes to `value` over `transition` seconds and stays at least `hold` seconds.
- `weatherMode` `{"text"}`: `world` (the map's weather rules), `forecast` (only what was set) or `static` (frozen as it is).
- `storm` `{"value", "min", "hold"}`: lightning density (0 to 1), the overcast it starts at, and the seconds between strikes.
- `rainThresholds` `{"min", "max", "transition"}`: the overcast range rain falls in, and the seconds it takes to stop.
- `date` `{"year", "month", "day", "hour", "minute"}`: the in-game date and time.
- `freezeTime` `{"value"}`: 1 stops the in-game clock, 0 starts it again.


## Credits and licences

ReCON-Mod's own code is under the [MIT License](LICENSE), with the
condition above: **not for monetized servers**. [NOTICE](NOTICE) lists what
it builds on or learned from, and their licences:

- **DayZ** (Bohemia Interactive). The mod uses DayZ's script API, and the
  Live Map uses the thresholds and colours from DayZ's scripts for the HUD
  colours. DayZ's scripts are under the
  [DayZ Public License](https://www.bohemia.net/community/licenses/dayz-public-license-dpl):
  attribution, **non-commercial**, DayZ only. Keep your use of the mod
  within it, and within Bohemia's rules for servers. This project is not
  made or endorsed by Bohemia Interactive.
- **[Community Framework](https://github.com/Jacob-Mango/DayZ-Community-Framework)**
  by Jacob_Mango and Arkensor (LGPL 2.1), a dependency loaded on the server.
  It isn't included here.
- **[Community Online Tools](https://github.com/Jacob-Mango/DayZ-CommunityOnlineTools)**
  by Jacob_Mango and contributors (CC BY-SA 4.0). The heal action and the
  weather and time controls are modelled on how COT does them, and written
  here with DayZ's own API. No COT code is copied.
- **[Leaflet](https://leafletjs.com)** (BSD 2-Clause) draws the Live Map; ReCON
  ships it with its licence.

A picture of a map you put in ReCON's `maps` folder must be one you have the
right to use. Many online DayZ map images belong to their makers.
