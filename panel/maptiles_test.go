package panel

import (
	"image"
	"image/color"
	"image/jpeg"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// quadrants is a 1024 px picture: red north-west, green north-east, blue
// south-west, white south-east.
func quadrants() *image.RGBA {
	img := image.NewRGBA(image.Rect(0, 0, 1024, 1024))
	for y := 0; y < 1024; y++ {
		for x := 0; x < 1024; x++ {
			c := color.RGBA{255, 0, 0, 255}
			switch {
			case x >= 512 && y < 512:
				c = color.RGBA{0, 255, 0, 255}
			case x < 512 && y >= 512:
				c = color.RGBA{0, 0, 255, 255}
			case x >= 512 && y >= 512:
				c = color.RGBA{255, 255, 255, 255}
			}
			img.SetRGBA(x, y, c)
		}
	}
	return img
}

func near(t *testing.T, what string, got color.RGBA, want color.RGBA) {
	t.Helper()
	d := func(a, b uint8) int {
		if a > b {
			return int(a - b)
		}
		return int(b - a)
	}
	if d(got.R, want.R) > 24 || d(got.G, want.G) > 24 || d(got.B, want.B) > 24 {
		t.Errorf("%s: %v, want %v", what, got, want)
	}
}

func TestCutTile(t *testing.T) {
	pic := quadrants()
	// A 2048 m map: at zoom -3 one tile (2048 m) is the whole map.
	whole := cutTile(pic, 2048, -3, 0, -1)
	near(t, "whole map, top left", whole.RGBAAt(10, 10), color.RGBA{255, 0, 0, 255})
	near(t, "whole map, top right", whole.RGBAAt(245, 10), color.RGBA{0, 255, 0, 255})
	near(t, "whole map, bottom left", whole.RGBAAt(10, 245), color.RGBA{0, 0, 255, 255})
	near(t, "whole map, bottom right", whole.RGBAAt(245, 245), color.RGBA{255, 255, 255, 255})
	// Zoom -1: 512 m tiles, 4 across; y -1 is the southern row, -4 the northern.
	near(t, "north-west tile", cutTile(pic, 2048, -1, 0, -4).RGBAAt(128, 128), color.RGBA{255, 0, 0, 255})
	near(t, "south-east tile", cutTile(pic, 2048, -1, 3, -1).RGBAAt(128, 128), color.RGBA{255, 255, 255, 255})
	// Zoom -2: 1024 m tiles, 2 across; (0, -2) is the north-west quarter.
	near(t, "zoom -2 north-west", cutTile(pic, 2048, -2, 0, -2).RGBAAt(128, 128), color.RGBA{255, 0, 0, 255})
	// Enlarged past the picture's own pixels (zoom 3), still the right colour.
	near(t, "zoom 3", cutTile(pic, 2048, 3, 0, -64).RGBAAt(5, 5), color.RGBA{255, 0, 0, 255})
	// Zoom -4: one 4096 m tile, the map in its bottom-left quarter; the rest
	// is background.
	past := cutTile(pic, 2048, -4, 0, -1)
	near(t, "past the map", past.RGBAAt(250, 5), tileBg)
	near(t, "the map in it", past.RGBAAt(10, 250), color.RGBA{0, 0, 255, 255})
}

func TestTileRoutes(t *testing.T) {
	dir := t.TempDir()
	os.MkdirAll(filepath.Join(dir, "maps"), 0o755)
	f, _ := os.Create(filepath.Join(dir, "maps", "testmap.jpg"))
	jpeg.Encode(f, quadrants(), &jpeg.Options{Quality: 95})
	f.Close()
	ts := newTileServer(dir)
	mux := http.NewServeMux()
	mux.HandleFunc("GET /api/mod/map/{world}/info", ts.handleInfo)
	mux.HandleFunc("GET /api/mod/maptile/{world}/{version}/{size}/{z}/{x}/{y}", ts.handleTile)
	get := func(path string) *httptest.ResponseRecorder {
		w := httptest.NewRecorder()
		mux.ServeHTTP(w, httptest.NewRequest("GET", path, nil))
		return w
	}

	info := get("/api/mod/map/testmap/info?size=2048")
	if info.Code != 200 || !strings.Contains(info.Body.String(), `"px":1024`) || !strings.Contains(info.Body.String(), `"maxNativeZoom":-1`) {
		t.Fatalf("info: %d %s", info.Code, info.Body)
	}
	_, version, _ := ts.source("testmap")
	if get("/api/mod/map/nomap/info?size=2048").Code != 404 {
		t.Error("a world with no picture: not 404")
	}
	tile := "/api/mod/maptile/testmap/" + version + "/2048/-1/0/-4"
	w := get(tile)
	if w.Code != 200 || w.Header().Get("Content-Type") != "image/jpeg" {
		t.Fatalf("tile: %d", w.Code)
	}
	img, err := jpeg.Decode(w.Body)
	if err != nil {
		t.Fatal(err)
	}
	r, g, b, _ := img.At(128, 128).RGBA()
	near(t, "served tile", color.RGBA{uint8(r >> 8), uint8(g >> 8), uint8(b >> 8), 255}, color.RGBA{255, 0, 0, 255})
	cached := filepath.Join(dir, "maptiles", "testmap-"+version+"-2048", "-1", "0_-4.jpg")
	if _, err := os.Stat(cached); err != nil {
		t.Errorf("the tile wasn't kept: %v", err)
	}
	if get(tile).Code != 200 {
		t.Error("the kept tile wasn't served")
	}
	for _, bad := range []string{
		"/api/mod/maptile/testmap/" + version + "/2048/-1/4/-4", // past the east edge
		"/api/mod/maptile/testmap/" + version + "/2048/-1/0/0",  // south of the map
		"/api/mod/maptile/testmap/" + version + "/2048/9/0/-1",  // too close a zoom
		"/api/mod/maptile/testmap/old/2048/-1/0/-4",             // an old picture's tile
		"/api/mod/maptile/..%2Fx/" + version + "/2048/-1/0/-4",  // not a world name
	} {
		if c := get(bad).Code; c != 404 {
			t.Errorf("%s: %d, want 404", bad, c)
		}
	}
}
