package panel

import (
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

// A map's picture can be pre-sliced into tiles: maps/<world>/<column>_<row>.jpg
// (or .png, .webp), a square grid of any size, column 0 at the west edge
// and row 0 at the north. The page then loads only the tiles in view (and
// just around it), not the whole picture. ReCON only lists and serves them.

var tileFile = regexp.MustCompile(`^(\d{1,3})_(\d{1,3})\.(jpg|jpeg|png|webp)$`)

func tileDir(dataDir, world string) (string, bool) {
	world = strings.ToLower(world)
	if dataDir == "" || world == "" || strings.ContainsAny(world, `/\.`) {
		return "", false
	}
	return filepath.Join(dataDir, "maps", world), true
}

// mapTileList answers GET /api/mod/map/{world}/tiles: {"cols", "rows",
// "ext", "version"} for maps/<world>/, 404 when it has no tiles.
func mapTileList(dataDir string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		dir, ok := tileDir(dataDir, r.PathValue("world"))
		if !ok {
			http.NotFound(w, r)
			return
		}
		entries, err := os.ReadDir(dir)
		if err != nil {
			http.NotFound(w, r)
			return
		}
		cols, rows, ext := 0, 0, ""
		var newest int64
		for _, e := range entries {
			m := tileFile.FindStringSubmatch(strings.ToLower(e.Name()))
			if m == nil || e.IsDir() {
				continue
			}
			x, _ := strconv.Atoi(m[1])
			y, _ := strconv.Atoi(m[2])
			cols, rows = max(cols, x+1), max(rows, y+1)
			if ext == "" {
				ext = e.Name()[len(e.Name())-len(m[3]):] // as it is spelled
			}
			if info, err := e.Info(); err == nil && info.ModTime().Unix() > newest {
				newest = info.ModTime().Unix()
			}
		}
		if cols == 0 {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("Cache-Control", "no-cache")
		json.NewEncoder(w).Encode(map[string]any{"cols": cols, "rows": rows, "ext": ext, "version": strconv.FormatInt(newest, 36)})
	}
}

// mapTile serves one tile, GET /api/mod/map/{world}/tile/{file}.
func mapTile(dataDir string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		dir, ok := tileDir(dataDir, r.PathValue("world"))
		file := r.PathValue("file")
		if !ok || !tileFile.MatchString(strings.ToLower(file)) {
			http.NotFound(w, r)
			return
		}
		p := filepath.Join(dir, file)
		if st, err := os.Stat(p); err != nil || st.IsDir() {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Cache-Control", "public, max-age=86400") // the page adds ?v=<version>
		http.ServeFile(w, r, p)
	}
}
