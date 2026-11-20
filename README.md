# ReCON-Mod

The DayZ server-side companion mod for [ReCON](https://github.com/MowMdown/ReCON).
Every second it sends ReCON every online player's live state: position,
health, blood, shock, food, water, heat, wetness, bleeding, illnesses,
whether they are knocked out or restrained, the item in hand, the vehicle
they're in, and their kills and deaths this session. It also runs ReCON's
actions on them: heal, kill, teleport, set a stat, and on-screen messages.

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

The mod is the `ReCON` folder. Pack it into a PBO with the prefix `ReCON`,
using DayZ Tools' Addon Builder, PBO Manager or `mikero`'s pboProject, so
that you end up with:

```
@ReCON/
  addons/
    ReCON.pbo
```

## Install

1. Put `@ReCON` in the server folder, next to `@CF`.
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

Each command's result comes back in the next sync. Events and results
that ReCON didn't get, because it was down or refused them, are sent again.

| Action | Fields |
|---|---|
| `heal` | Full health, blood, shock, food and water. Stops bleeding, cures illnesses and mends broken legs. |
| `kill` | — |
| `teleport` | `x`, `z`, and `y` (0 = ground level). |
| `teleportTo` | `target`: the SteamID64 of the player to go to. |
| `set` | `stat` (`health`, `blood`, `shock`, `water` or `energy`) and `value`. |
| `message` | `text`, plus optional `title` and `seconds`. Shown as an in-game notification. |
