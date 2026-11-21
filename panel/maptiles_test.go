package panel

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestMapTiles(t *testing.T) {
	dir := t.TempDir()
	tiles := filepath.Join(dir, "maps", "namalsk")
	os.MkdirAll(tiles, 0o755)
	for _, f := range []string{"0_0.jpg", "1_0.jpg", "0_1.jpg", "15_3.jpg", "notes.txt", "a_b.jpg"} {
		os.WriteFile(filepath.Join(tiles, f), []byte("jpeg "+f), 0o644)
	}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /api/mod/map/{world}/tiles", mapTileList(dir))
	mux.HandleFunc("GET /api/mod/map/{world}/tile/{file}", mapTile(dir))
	get := func(path string) *httptest.ResponseRecorder {
		w := httptest.NewRecorder()
		mux.ServeHTTP(w, httptest.NewRequest("GET", path, nil))
		return w
	}

	w := get("/api/mod/map/Namalsk/tiles")
	if w.Code != 200 || !strings.Contains(w.Body.String(), `"cols":16`) || !strings.Contains(w.Body.String(), `"rows":4`) || !strings.Contains(w.Body.String(), `"ext":"jpg"`) {
		t.Fatalf("tiles: %d %s", w.Code, w.Body)
	}
	if get("/api/mod/map/chernarusplus/tiles").Code != 404 {
		t.Error("a world with no tiles: not 404")
	}
	if w := get("/api/mod/map/namalsk/tile/15_3.jpg?v=1"); w.Code != 200 || w.Body.String() != "jpeg 15_3.jpg" {
		t.Errorf("a tile: %d %q", w.Code, w.Body)
	}
	for _, bad := range []string{
		"/api/mod/map/namalsk/tile/2_2.jpg",   // not there
		"/api/mod/map/namalsk/tile/notes.txt", // not a tile
		"/api/mod/map/namalsk/tile/..%2F..%2Fp.db",
		"/api/mod/map/..%2Fx/tile/0_0.jpg",
	} {
		if c := get(bad).Code; c != 404 {
			t.Errorf("%s: %d, want 404", bad, c)
		}
	}
}
