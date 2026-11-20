package panel

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"io/fs"
)

// The mod's JSON reader fills its classes from ReCON's reply, so every
// command field must be in it, even when empty.
func TestModSyncOverHTTP(t *testing.T) {
	setup(t)
	openTestDB(t)
	mux, quiet := startTest(t, "secret")
	post := func(path, body string) (int, map[string]any) {
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, httptest.NewRequest(http.MethodPost, path, strings.NewReader(body)))
		var out map[string]any
		json.Unmarshal(rec.Body.Bytes(), &out)
		return rec.Code, out
	}
	sync := `{"v":1,"mod":"0.1.0","key":"%s","world":"chernarusplus","year":2026,"month":11,"day":20,"hour":14,"minute":5,
		"players":[{"steamId":"76561198000000001","name":"bob","x":7500.5,"y":10,"z":7600,"alive":true,"health":100,"unconscious":false}],
		"events":[],"results":[]}`
	if code, out := post("/api/mod/sync", strings.Replace(sync, "%s", "wrong", 1)); code != http.StatusForbidden || out["ok"] != false || out["error"] == "" {
		t.Fatalf("wrong key: %d %v", code, out)
	}
	if code, out := post("/api/mod/sync", strings.Replace(sync, "%s", "secret", 1)); code != 200 || out["ok"] != true {
		t.Fatalf("sync: %d %v", code, out)
	}
	guid := battlEyeGUID(76561198000000001)
	if code, out := post("/api/mod/players/"+guid+"/message", `{"text":"hi"}`); code != 200 {
		t.Fatalf("queue: %d %v", code, out)
	}
	if code, out := post("/api/mod/players/"+guid+"/fly", `{}`); code != http.StatusBadRequest {
		t.Fatalf("unknown action: %d %v", code, out)
	}
	_, out := post("/api/mod/sync", strings.Replace(sync, "%s", "secret", 1))
	cmds, _ := out["commands"].([]any)
	if len(cmds) != 1 {
		t.Fatalf("reply %v", out)
	}
	for _, k := range []string{"id", "action", "steamId", "target", "x", "y", "z", "stat", "value", "text", "title", "seconds"} {
		if _, ok := cmds[0].(map[string]any)[k]; !ok {
			t.Errorf("command has no %q: %v", k, cmds[0])
		}
	}
	for _, k := range []string{"ok", "error", "interval", "commands"} {
		if _, ok := out[k]; !ok {
			t.Errorf("reply has no %q", k)
		}
	}

	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, httptest.NewRequest(http.MethodGet, "/api/mod/players/"+guid, nil))
	var one map[string]any
	json.Unmarshal(rec.Body.Bytes(), &one)
	if one["live"] != true || one["kills"] != float64(0) {
		t.Errorf("player: %v", one)
	}
	if !quiet(httptest.NewRequest(http.MethodPost, "/api/mod/sync", nil)) {
		t.Error("the mod's syncs would fill the log")
	}
}

// startTest starts the panel with a stand-in for ReCON and returns its
// routes and request-log filter.
func startTest(t *testing.T, password string) (*http.ServeMux, func(*http.Request) bool) {
	t.Helper()
	mux := http.NewServeMux()
	var quiet func(*http.Request) bool
	var nav []string
	err := Start(Host{
		DB:           db,
		RCONPassword: func() string { return password },
		AddRoute:     func(p string, h http.HandlerFunc) { mux.HandleFunc(p, h) },
		AddNavLink:   func(key, label, href string) { nav = append(nav, href) },
		AddQuiet:     func(f func(*http.Request) bool) { quiet = f },
		Page: func(files fs.FS, name string) http.HandlerFunc {
			if _, err := fs.Stat(files, name); err != nil {
				t.Errorf("page %s: %v", name, err)
			}
			return func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("page")) }
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(nav) != 1 || nav[0] != "/live" {
		t.Errorf("nav %v", nav)
	}
	t.Cleanup(func() { keySource = func() string { return "" } })
	return mux, quiet
}
