// Package panel is ReCON's side of recon-mod, the DayZ server-side mod in
// this repository: built into ReCON on request (see Start), it adds the
// Live Map page and the API the mod talks to. The mod posts every online player's
// state to /api/mod/sync every second or so (Sync), with the deaths since
// the last sync and how the last commands went; the reply carries the
// commands queued here since (Queue). The mod proves it is the game server
// with the RCON password, which ReCON already has.
//
// Players come by SteamID64; their BattlEye GUID (ReCON's key for a
// player) is worked out from it.
package panel

import (
	"crypto/md5"
	"crypto/subtle"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Protocol is the sync format version this ReCON speaks.
const Protocol = 1

var (
	// staleAfter is how long after the last sync the mod counts as
	// disconnected and its data as old.
	staleAfter = 5 * time.Second
	// commandTimeout is how long a queued command waits for the mod to
	// pick it up and answer before it is given up.
	commandTimeout = 30 * time.Second
	// keepResults is how many finished commands are kept for the pages.
	keepResults = 50
)

// SyncRequest is what the mod posts (ReCON_Sync in the mod).
type SyncRequest struct {
	V       int           `json:"v"`
	Mod     string        `json:"mod"`
	Key     string        `json:"key"`
	World   string        `json:"world"`
	Year    int           `json:"year"`
	Month   int           `json:"month"`
	Day     int           `json:"day"`
	Hour    int           `json:"hour"`
	Minute  int           `json:"minute"`
	Weather World         `json:"weather"`
	Players []PlayerState `json:"players"`
	Events  []Event       `json:"events"`
	Results []Result      `json:"results"`
}

// PlayerState is one player as the mod reads them (ReCON_PlayerState).
type PlayerState struct {
	SteamID string  `json:"steamId"`
	Name    string  `json:"name"`
	X       float64 `json:"x"`
	Y       float64 `json:"y"`
	Z       float64 `json:"z"`
	Heading float64 `json:"heading"`
	Alive   Flag    `json:"alive"`
	Health  float64 `json:"health"`
	Blood   float64 `json:"blood"`
	Shock   float64 `json:"shock"`
	Water   float64 `json:"water"`
	Energy  float64 `json:"energy"`
	Heat    float64 `json:"heat"`
	// The heat buff, shown as "comfort": 0 (none) to HeatBufferMax.
	HeatBuffer      float64 `json:"heatBuffer"`
	HeatBufferMax   float64 `json:"heatBufferMax"`
	HeatBufferCap   float64 `json:"heatBufferCap"`   // the share their clothes allow, 0.35 to 1
	HeatBufferStage int     `json:"heatBufferStage"` // the HUD's buff icon: 0 none, 1 to 3
	Wet             int     `json:"wet"`
	Stamina         float64 `json:"stamina"`
	Bleeding        int     `json:"bleeding"`
	Agents          int     `json:"agents"`
	BrokenLegs      int     `json:"brokenLegs"`
	Unconscious     Flag    `json:"unconscious"`
	Restrained      Flag    `json:"restrained"`
	Hands           string  `json:"hands"`
	Vehicle         string  `json:"vehicle"`
	Kills           int     `json:"kills"`
	Deaths          int     `json:"deaths"`
}

// World is the weather now and where each part is heading (ReCON_World).
type World struct {
	Overcast        float64 `json:"overcast"` // 0-1
	Rain            float64 `json:"rain"`
	Fog             float64 `json:"fog"`
	Snow            float64 `json:"snow"`
	WindSpeed       float64 `json:"windSpeed"` // m/s
	WindDir         float64 `json:"windDir"`   // degrees
	OvercastTarget  float64 `json:"overcastTarget"`
	RainTarget      float64 `json:"rainTarget"`
	FogTarget       float64 `json:"fogTarget"`
	SnowTarget      float64 `json:"snowTarget"`
	WindSpeedTarget float64 `json:"windSpeedTarget"`
	WindDirTarget   float64 `json:"windDirTarget"`
	Mode            string  `json:"mode"` // world, forecast or static
	TimeFrozen      Flag    `json:"timeFrozen"`
}

// Event is something that happened between syncs (ReCON_Event).
type Event struct {
	Type       string  `json:"type"`
	Victim     string  `json:"victim"`
	VictimName string  `json:"victimName"`
	Killer     string  `json:"killer"`
	KillerName string  `json:"killerName"`
	Weapon     string  `json:"weapon"`
	Distance   float64 `json:"distance"`
	X          float64 `json:"x"`
	Y          float64 `json:"y"`
	Z          float64 `json:"z"`
	At         int     `json:"at"`
}

// Result is how a command went (ReCON_Result).
type Result struct {
	ID    int    `json:"id"`
	OK    Flag   `json:"ok"`
	Error string `json:"error"`
}

// Command is an action for the mod to run (ReCON_Command). Every field is
// always sent: the mod's JSON reader fills its class from them.
type Command struct {
	ID      int     `json:"id"`
	Action  string  `json:"action"`
	SteamID string  `json:"steamId"`
	Target  string  `json:"target"`
	X       float64 `json:"x"`
	Y       float64 `json:"y"`
	Z       float64 `json:"z"`
	Stat    string  `json:"stat"`
	Value   float64 `json:"value"`
	Text    string  `json:"text"`
	Title   string  `json:"title"`
	Seconds float64 `json:"seconds"`
	// World actions (QueueWorld).
	Transition float64 `json:"transition"`
	Hold       float64 `json:"hold"`
	Min        float64 `json:"min"`
	Max        float64 `json:"max"`
	Year       int     `json:"year"`
	Month      int     `json:"month"`
	Day        int     `json:"day"`
	Hour       int     `json:"hour"`
	Minute     int     `json:"minute"`
}

// SyncReply is ReCON's answer (ReCON_Reply).
type SyncReply struct {
	OK       bool      `json:"ok"`
	Error    string    `json:"error"`
	Interval int       `json:"interval"`
	Commands []Command `json:"commands"`
}

// Player is a player's live state as the pages get it.
type Player struct {
	GUID string `json:"guid"`
	PlayerState
	Illnesses []string `json:"illnesses"`
}

// CommandStatus is a queued command and what became of it.
type CommandStatus struct {
	Command
	GUID     string    `json:"guid"`
	Status   string    `json:"status"` // queued, sent, done, failed, expired
	Error    string    `json:"error,omitempty"`
	Queued   time.Time `json:"queued"`
	Finished time.Time `json:"finished,omitempty"`
}

var state struct {
	sync.Mutex
	lastSync time.Time
	mod      string
	world    string
	date     [5]int // year, month, day, hour, minute
	weather  World
	players  map[string]Player // by GUID
	nextID   int
	commands []*CommandStatus // oldest first: queued and sent ones, then the last keepResults finished
	// SteamIDs saved to the players table this run (one write per player).
	steamSaved map[string]bool
	refused    string // the last refusal logged, so it isn't logged every second
}

// keySource is the RCON password the mod must send (read on every sync,
// so RCON Settings changing it applies at once).
var keySource = func() string { return "" }

// saveSteamID stores a player's SteamID64 in ReCON (nil: not stored).
var saveSteamID func(guid, steamID string) error

// ErrRefused is a sync with the wrong password.
var ErrRefused = errors.New("wrong RCON password: set the mod's \"password\" to the RCON password ReCON uses")

// Sync takes in one sync from the mod and returns the commands to run.
func Sync(req SyncRequest, now time.Time) (SyncReply, error) {
	want := keySource()
	if want == "" || subtle.ConstantTimeCompare([]byte(req.Key), []byte(want)) != 1 {
		// The lengths (never the passwords) tell a typo from a different password.
		logRefusalKeyed(ErrRefused.Error(), fmt.Sprintf("%s (the mod sent a password of %d characters; ReCON's has %d)",
			ErrRefused.Error(), len(req.Key), len(want)))
		return SyncReply{Error: ErrRefused.Error()}, ErrRefused
	}
	if req.V != Protocol {
		msg := "the mod speaks protocol " + strconv.Itoa(req.V) + ", this ReCON " + strconv.Itoa(Protocol) + ": update both to the same release"
		logRefusal(msg)
		return SyncReply{Error: msg}, errors.New(msg)
	}
	players := make(map[string]Player, len(req.Players))
	var newSteam [][2]string
	state.Lock()
	if state.lastSync.IsZero() || now.Sub(state.lastSync) > staleAfter {
		log.Printf("recon-mod: the mod (version %s) connected (%s, %d players)", req.Mod, req.World, len(req.Players))
	}
	state.refused = ""
	if state.steamSaved == nil {
		state.steamSaved = map[string]bool{}
	}
	for _, p := range req.Players {
		id, err := strconv.ParseUint(p.SteamID, 10, 64)
		if err != nil {
			continue
		}
		guid := battlEyeGUID(id)
		players[guid] = Player{GUID: guid, PlayerState: p, Illnesses: Illnesses(p.Agents)}
		if !state.steamSaved[guid] {
			state.steamSaved[guid] = true
			newSteam = append(newSteam, [2]string{guid, p.SteamID})
		}
	}
	state.lastSync, state.mod, state.world, state.players = now, req.Mod, req.World, players
	state.date = [5]int{req.Year, req.Month, req.Day, req.Hour, req.Minute}
	state.weather = req.Weather

	// Results of what was sent.
	byID := map[int]*CommandStatus{}
	for _, c := range state.commands {
		byID[c.ID] = c
	}
	for _, r := range req.Results {
		c := byID[r.ID]
		if c == nil || (c.Status != "sent" && c.Status != "expired") {
			continue
		}
		c.Status, c.Error, c.Finished = "done", "", now
		if !r.OK {
			c.Status, c.Error = "failed", r.Error
		}
	}
	// What to send: everything queued, unless it waited too long.
	reply := SyncReply{OK: true, Commands: []Command{}}
	for _, c := range state.commands {
		switch {
		case c.Status == "queued" && now.Sub(c.Queued) > commandTimeout:
			c.Status, c.Error, c.Finished = "expired", "the mod didn't pick it up in time", now
		case c.Status == "queued":
			c.Status = "sent"
			reply.Commands = append(reply.Commands, c.Command)
		case c.Status == "sent" && now.Sub(c.Queued) > commandTimeout:
			c.Status, c.Error, c.Finished = "expired", "no answer from the mod", now
		}
	}
	trimCommands()
	state.Unlock()

	if saveSteamID != nil {
		for _, g := range newSteam {
			if err := saveSteamID(g[0], g[1]); err != nil {
				log.Printf("recon-mod: saving %s's SteamID: %v", g[0], err)
			}
		}
	}
	if db != nil {
		for _, e := range req.Events {
			if e.Type != "kill" {
				continue
			}
			k := Kill{At: now, VictimName: e.VictimName, KillerName: e.KillerName,
				Weapon: e.Weapon, Distance: e.Distance, X: e.X, Z: e.Z}
			k.VictimGUID = guidOf(e.Victim)
			k.KillerGUID = guidOf(e.Killer)
			if err := addKill(k); err != nil {
				log.Printf("recon-mod: saving a kill: %v", err)
			}
		}
	}
	return reply, nil
}

// logRefusal logs why a sync was refused, once until the reason (key)
// changes or a sync is accepted.
func logRefusal(msg string) { logRefusalKeyed(msg, msg) }

func logRefusalKeyed(key, msg string) {
	state.Lock()
	defer state.Unlock()
	if state.refused != key {
		state.refused = key
		log.Printf("recon-mod: refused a sync from the mod: %s", msg)
	}
}

// guidOf is the GUID of a SteamID64 ("" for none).
func guidOf(steamID string) string {
	id, err := strconv.ParseUint(steamID, 10, 64)
	if err != nil || id == 0 {
		return ""
	}
	return battlEyeGUID(id)
}

// trimCommands keeps the open commands and the last keepResults finished.
func trimCommands() {
	finished := 0
	for _, c := range state.commands {
		if c.Status != "queued" && c.Status != "sent" {
			finished++
		}
	}
	if finished <= keepResults {
		return
	}
	drop := finished - keepResults
	kept := state.commands[:0]
	for _, c := range state.commands {
		if drop > 0 && c.Status != "queued" && c.Status != "sent" {
			drop--
			continue
		}
		kept = append(kept, c)
	}
	state.commands = kept
}

// illnessNames are eAgents' bits.
var illnessNames = []string{"cholera", "influenza", "salmonella", "brain disease", "food poisoning",
	"chemical poisoning", "wound infection", "nerve agent", "heavy metal poisoning", "silicosis", "eye irritation"}

// Illnesses names the eAgents bits in agents.
func Illnesses(agents int) []string {
	out := []string{}
	for i, n := range illnessNames {
		if agents&(1<<i) != 0 {
			out = append(out, n)
		}
	}
	return out
}

// Status is the mod's connection, as the pages show it.
type Status struct {
	Connected bool      `json:"connected"`
	LastSync  time.Time `json:"lastSync,omitempty"`
	Mod       string    `json:"mod,omitempty"`
	World     string    `json:"world,omitempty"`
	Date      [5]int    `json:"date"` // in-game year, month, day, hour, minute
	Weather   World     `json:"weather"`
}

// Current is the mod's connection and every online player's state (none
// once the mod stopped syncing), sorted by name.
func Current(now time.Time) (Status, []Player) {
	state.Lock()
	defer state.Unlock()
	st := Status{LastSync: state.lastSync, Mod: state.mod, World: state.world, Date: state.date, Weather: state.weather}
	st.Connected = !state.lastSync.IsZero() && now.Sub(state.lastSync) <= staleAfter
	out := []Player{}
	if st.Connected {
		for _, p := range state.players {
			out = append(out, p)
		}
	}
	sort.Slice(out, func(a, b int) bool { return strings.ToLower(out[a].Name) < strings.ToLower(out[b].Name) })
	return st, out
}

// PlayerOf is one online player's live state.
func PlayerOf(guid string, now time.Time) (Player, bool) {
	state.Lock()
	defer state.Unlock()
	if !connected(now) {
		return Player{}, false
	}
	p, ok := state.players[strings.ToLower(guid)]
	return p, ok
}

// Commands is the open and recent commands, newest first; for one player
// when guid isn't "".
func Commands(guid string) []CommandStatus {
	state.Lock()
	defer state.Unlock()
	out := []CommandStatus{}
	for i := len(state.commands) - 1; i >= 0; i-- {
		c := state.commands[i]
		if guid == "" || c.GUID == strings.ToLower(guid) {
			out = append(out, *c)
		}
	}
	return out
}

// Actions are the commands the mod runs, and the stats "set" can change
// with their largest values.
var (
	Actions  = map[string]bool{"heal": true, "kill": true, "teleport": true, "teleportTo": true, "set": true, "message": true}
	StatMaxs = map[string]float64{"health": 100, "blood": 5000, "shock": 100, "water": 5000, "energy": 5000, "stamina": 100, "heatBuffer": 30}
)

// Queue adds a command for an online player (guid); c.SteamID and, for
// teleportTo, c.Target (a GUID here) are filled from the live state.
// Errors are user-facing.
func Queue(guid string, c Command, now time.Time) (CommandStatus, error) {
	if !Actions[c.Action] {
		return CommandStatus{}, errors.New("unknown action")
	}
	switch c.Action {
	case "set":
		max, ok := StatMaxs[c.Stat]
		if !ok {
			return CommandStatus{}, errors.New("stat is health, blood, shock, water, energy, stamina or heatBuffer")
		}
		if c.Value < 0 || c.Value > max {
			return CommandStatus{}, errors.New(c.Stat + " is from 0 to " + strconv.FormatFloat(max, 'f', -1, 64))
		}
	case "teleport":
		if c.X <= 0 || c.Z <= 0 {
			return CommandStatus{}, errors.New("pick where to teleport them")
		}
	case "message":
		c.Text = strings.TrimSpace(c.Text)
		if c.Text == "" {
			return CommandStatus{}, errors.New("type a message")
		}
		if len(c.Text) > 500 {
			return CommandStatus{}, errors.New("the message is longer than 500 characters")
		}
	}
	guid = strings.ToLower(guid)
	state.Lock()
	defer state.Unlock()
	if !connected(now) {
		return CommandStatus{}, errors.New("recon-mod isn't connected")
	}
	p, ok := state.players[guid]
	if !ok {
		return CommandStatus{}, errors.New("the player isn't online")
	}
	c.SteamID = p.SteamID
	if c.Action == "teleportTo" {
		t, ok := state.players[strings.ToLower(c.Target)]
		if !ok {
			return CommandStatus{}, errors.New("the player to go to isn't online")
		}
		c.Target = t.SteamID
	}
	return add(c, guid, now), nil
}

// connected reports whether the mod synced lately (state is locked).
func connected(now time.Time) bool {
	return !state.lastSync.IsZero() && now.Sub(state.lastSync) <= staleAfter
}

// add queues a checked command (state is locked).
func add(c Command, guid string, now time.Time) CommandStatus {
	state.nextID++
	c.ID = state.nextID
	cs := &CommandStatus{Command: c, GUID: guid, Status: "queued", Queued: now}
	state.commands = append(state.commands, cs)
	return *cs
}

// WorldActions are the commands on the world rather than a player.
var WorldActions = map[string]bool{"weather": true, "weatherMode": true, "storm": true, "rainThresholds": true, "date": true, "freezeTime": true}

// weatherMax is each weather part's largest value.
var weatherMax = map[string]float64{"overcast": 1, "rain": 1, "fog": 1, "snow": 1, "windSpeed": 50, "windDir": 360}

// QueueWorld adds a command on the weather or the in-game time. Errors are
// user-facing.
func QueueWorld(c Command, now time.Time) (CommandStatus, error) {
	if err := checkWorld(&c); err != nil {
		return CommandStatus{}, err
	}
	state.Lock()
	defer state.Unlock()
	if !connected(now) {
		return CommandStatus{}, errors.New("recon-mod isn't connected")
	}
	return add(c, "", now), nil
}

func checkWorld(c *Command) error {
	if !WorldActions[c.Action] {
		return errors.New("unknown action")
	}
	c.SteamID, c.Target = "", ""
	switch c.Action {
	case "weather":
		max, ok := weatherMax[c.Stat]
		if !ok {
			return errors.New("the weather is overcast, rain, fog, snow, windSpeed or windDir")
		}
		if c.Value < 0 || c.Value > max {
			return errors.New(c.Stat + " is from 0 to " + strconv.FormatFloat(max, 'f', -1, 64))
		}
		if c.Transition < 0 || c.Transition > 3600 {
			return errors.New("the change takes 0 to 3600 seconds")
		}
		if c.Hold < 0 || c.Hold > 7*24*3600 {
			return errors.New("it holds for 0 seconds to 7 days")
		}
	case "weatherMode":
		if c.Text != "world" && c.Text != "forecast" && c.Text != "static" {
			return errors.New("the weather mode is world, forecast or static")
		}
	case "storm":
		if c.Value < 0 || c.Value > 1 || c.Min < 0 || c.Min > 1 {
			return errors.New("the storm's density and overcast are from 0 to 1")
		}
		if c.Hold < 1 {
			return errors.New("lightning needs at least 1 second between strikes")
		}
	case "rainThresholds":
		if c.Min < 0 || c.Max > 1 || c.Min > c.Max {
			return errors.New("rain's overcast range is from 0 to 1, lowest first")
		}
		if c.Transition < 0 {
			return errors.New("the time it takes rain to stop can't be negative")
		}
	case "date":
		t := time.Date(c.Year, time.Month(c.Month), c.Day, c.Hour, c.Minute, 0, 0, time.UTC)
		if c.Year < 1 || t.Year() != c.Year || int(t.Month()) != c.Month || t.Day() != c.Day || t.Hour() != c.Hour || t.Minute() != c.Minute {
			return errors.New("that isn't a date and time")
		}
	case "freezeTime":
		if c.Value != 0 && c.Value != 1 {
			return errors.New("freezeTime is 1 (frozen) or 0")
		}
	}
	return nil
}

// reset forgets everything (tests).
func reset() {
	state.Lock()
	defer state.Unlock()
	state.lastSync, state.mod, state.world, state.players = time.Time{}, "", "", nil
	state.nextID, state.commands, state.steamSaved, state.refused = 0, nil, nil, ""
}

// battlEyeGUID is the BattlEye GUID of a SteamID64: md5("BE" + the ID as 8
// little-endian bytes), in lower-case hex.
func battlEyeGUID(id uint64) string {
	b := make([]byte, 10)
	b[0], b[1] = 'B', 'E'
	binary.LittleEndian.PutUint64(b[2:], id)
	sum := md5.Sum(b)
	return hex.EncodeToString(sum[:])
}

// Flag is a true/false the mod sends. DayZ's JSON writer may write it as
// true/false or as 1/0; both read. It is written back as true/false.
type Flag bool

// UnmarshalJSON reads true, false, 1, 0 (or "true", "1"...).
func (f *Flag) UnmarshalJSON(b []byte) error {
	switch strings.Trim(strings.TrimSpace(string(b)), `"`) {
	case "true", "1":
		*f = true
	case "false", "0", "", "null":
		*f = false
	default:
		return fmt.Errorf("not a true/false: %s", b)
	}
	return nil
}
