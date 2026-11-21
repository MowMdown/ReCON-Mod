package panel

import (
	"bytes"
	"encoding/json"
	"fmt"
	"image"
	"image/color"
	"image/jpeg"
	_ "image/png" // maps/<world>.png
	"log"
	"math"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

// The map's picture is served in 256 px tiles, the way Leaflet asks for
// them (CRS.Simple: at zoom z a tile is 256/2^z metres across), so the page
// only loads what is on screen, sized for the zoom: a phone never holds the
// whole picture. Each tile is cut from the picture the first time it is
// asked for and kept in <data>/maptiles (the maps folder can be read-only).
// The decoded picture stays in memory while tiles are being asked for and
// is dropped a few minutes after the last.

const (
	tilePx      = 256
	tileMinZoom = -6              // the page's furthest zoom
	pictureIdle = 3 * time.Minute // how long a decoded picture is kept unused
)

// picture is one map's picture, decoded.
type picture struct {
	img     image.Image
	version string // changes when the file does: the tiles' cache key
	timer   *time.Timer
}

type tileServer struct {
	dataDir string
	mu      sync.Mutex
	pics    map[string]*picture // world -> picture
	loading map[string]*sync.Mutex
}

func newTileServer(dataDir string) *tileServer {
	return &tileServer{dataDir: dataDir, pics: map[string]*picture{}, loading: map[string]*sync.Mutex{}}
}

// source finds maps/<world>.jpg or .png (tiles aren't made from .webp: Go
// can't decode it without another module; the page loads it whole).
func (ts *tileServer) source(world string) (path string, version string, ok bool) {
	if ts.dataDir == "" || world == "" || strings.ContainsAny(world, `/\.`) {
		return "", "", false
	}
	for _, ext := range []string{".jpg", ".jpeg", ".png"} {
		p := filepath.Join(ts.dataDir, "maps", world+ext)
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p, strconv.FormatInt(st.ModTime().Unix(), 36) + "-" + strconv.FormatInt(st.Size(), 36), true
		}
	}
	return "", "", false
}

// picture returns world's picture decoded, decoding it once.
func (ts *tileServer) picture(world, path, version string) (*picture, error) {
	ts.mu.Lock()
	l := ts.loading[world]
	if l == nil {
		l = &sync.Mutex{}
		ts.loading[world] = l
	}
	ts.mu.Unlock()
	l.Lock() // one decode at a time per world
	defer l.Unlock()

	ts.mu.Lock()
	p := ts.pics[world]
	if p != nil && p.version == version {
		p.timer.Reset(pictureIdle)
		ts.mu.Unlock()
		return p, nil
	}
	ts.mu.Unlock()

	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	img, _, err := image.Decode(f)
	f.Close()
	if err != nil {
		return nil, fmt.Errorf("decoding %s: %w", path, err)
	}
	if b := img.Bounds(); b.Dx() != b.Dy() {
		log.Printf("recon-mod: %s isn't square (%d×%d); it is stretched to fit the map", path, b.Dx(), b.Dy())
	}
	p = &picture{img: img, version: version}
	ts.mu.Lock()
	if old := ts.pics[world]; old != nil {
		old.timer.Stop()
	}
	ts.pics[world] = p
	p.timer = time.AfterFunc(pictureIdle, func() {
		ts.mu.Lock()
		if ts.pics[world] == p {
			delete(ts.pics, world)
		}
		ts.mu.Unlock()
	})
	ts.mu.Unlock()
	return p, nil
}

// maxNativeZoom is the closest zoom whose tiles still have a picture pixel
// for every tile pixel (or nearly); closer in, the page enlarges those.
func maxNativeZoom(px, size int) int {
	z := int(math.Round(math.Log2(float64(px) / float64(size))))
	return max(tileMinZoom, min(3, z))
}

// handleInfo answers whether world has a picture to tile, and how: GET
// /api/mod/map/{world}/info?size=<metres> -> {"version", "px", "maxNativeZoom"}.
func (ts *tileServer) handleInfo(w http.ResponseWriter, r *http.Request) {
	world := strings.ToLower(r.PathValue("world"))
	size, _ := strconv.Atoi(r.URL.Query().Get("size"))
	path, version, ok := ts.source(world)
	if !ok || size <= 0 {
		http.NotFound(w, r)
		return
	}
	cfg, err := pictureSize(path)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-cache")
	json.NewEncoder(w).Encode(map[string]any{"version": version, "px": cfg, "maxNativeZoom": maxNativeZoom(cfg, size)})
}

// pictureSize reads only the picture's header for its width.
func pictureSize(path string) (int, error) {
	f, err := os.Open(path)
	if err != nil {
		return 0, err
	}
	defer f.Close()
	c, _, err := image.DecodeConfig(f)
	if err != nil {
		return 0, fmt.Errorf("reading %s: %w", path, err)
	}
	return c.Width, nil
}

// handleTile serves one tile: GET
// /api/mod/maptile/{world}/{version}/{size}/{z}/{x}/{y}.
func (ts *tileServer) handleTile(w http.ResponseWriter, r *http.Request) {
	world := strings.ToLower(r.PathValue("world"))
	size, e1 := strconv.Atoi(r.PathValue("size"))
	z, e2 := strconv.Atoi(r.PathValue("z"))
	x, e3 := strconv.Atoi(r.PathValue("x"))
	y, e4 := strconv.Atoi(r.PathValue("y"))
	path, version, ok := ts.source(world)
	if !ok || e1 != nil || e2 != nil || e3 != nil || e4 != nil || size <= 0 || size > 1<<17 || z < tileMinZoom || z > 3 {
		http.NotFound(w, r)
		return
	}
	// Only tiles that touch the map: at zoom z it is size*2^z px across,
	// x from 0 and y (north at the top, Leaflet's y grows south) up to 0.
	n := int(math.Ceil(float64(size) * math.Pow(2, float64(z)) / tilePx))
	if x < 0 || x >= n || y >= 0 || y < -n {
		http.NotFound(w, r)
		return
	}
	if r.PathValue("version") != version { // the picture changed: the page reloads its info
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "image/jpeg")
	w.Header().Set("Cache-Control", "public, max-age=604800, immutable") // the version is in the URL

	cache := filepath.Join(ts.dataDir, "maptiles", world+"-"+version+"-"+strconv.Itoa(size), strconv.Itoa(z), fmt.Sprintf("%d_%d.jpg", x, y))
	if b, err := os.ReadFile(cache); err == nil {
		w.Write(b)
		return
	}
	pic, err := ts.picture(world, path, version)
	if err != nil {
		log.Printf("recon-mod: map tiles: %v", err)
		http.Error(w, "can't read the map's picture", http.StatusInternalServerError)
		return
	}
	var buf bytes.Buffer
	if err := jpeg.Encode(&buf, cutTile(pic.img, size, z, x, y), &jpeg.Options{Quality: 85}); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	w.Write(buf.Bytes())
	// Keep it for next time (best effort; clear older versions first).
	dir := filepath.Dir(filepath.Dir(cache))
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		ts.dropOldTiles(world, filepath.Base(dir))
	}
	if os.MkdirAll(filepath.Dir(cache), 0o755) == nil {
		tmp := cache + ".tmp" + strconv.Itoa(os.Getpid())
		if os.WriteFile(tmp, buf.Bytes(), 0o644) == nil {
			os.Rename(tmp, cache)
		}
	}
}

// dropOldTiles removes world's cached tiles other than keep.
func (ts *tileServer) dropOldTiles(world, keep string) {
	dirs, _ := os.ReadDir(filepath.Join(ts.dataDir, "maptiles"))
	for _, d := range dirs {
		if d.IsDir() && d.Name() != keep && strings.HasPrefix(d.Name(), world+"-") {
			os.RemoveAll(filepath.Join(ts.dataDir, "maptiles", d.Name()))
		}
	}
}

// tileBg is the map's background, for a tile's part past the picture.
var tileBg = color.RGBA{0x16, 0x1a, 0x17, 0xff}

// cutTile makes tile (z, x, y) of a map size metres across from its
// picture (square, north up, covering the map): each tile pixel averages
// the picture pixels under it, or takes the nearest when enlarging.
func cutTile(img image.Image, size, z, x, y int) *image.RGBA {
	b := img.Bounds()
	pw, ph := float64(b.Dx()), float64(b.Dy())
	m := tilePx / math.Pow(2, float64(z))                        // metres across the tile
	kx, ky := pw/float64(size), ph/float64(size)                 // picture px a metre
	sx0, sy0 := float64(x)*m*kx, (float64(size)+float64(y)*m)*ky // the tile's top-left in picture px
	step := m * kx / tilePx                                      // picture px a tile px
	stepY := m * ky / tilePx
	out := image.NewRGBA(image.Rect(0, 0, tilePx, tilePx))
	ycc, _ := img.(*image.YCbCr)
	rgba, _ := img.(*image.RGBA)
	nrgba, _ := img.(*image.NRGBA)
	at := func(px, py int) (uint32, uint32, uint32) {
		px, py = px+b.Min.X, py+b.Min.Y
		switch {
		case ycc != nil:
			yi, ci := ycc.YOffset(px, py), ycc.COffset(px, py)
			r, g, bb := color.YCbCrToRGB(ycc.Y[yi], ycc.Cb[ci], ycc.Cr[ci])
			return uint32(r), uint32(g), uint32(bb)
		case rgba != nil:
			i := rgba.PixOffset(px, py)
			return uint32(rgba.Pix[i]), uint32(rgba.Pix[i+1]), uint32(rgba.Pix[i+2])
		case nrgba != nil:
			i := nrgba.PixOffset(px, py)
			return uint32(nrgba.Pix[i]), uint32(nrgba.Pix[i+1]), uint32(nrgba.Pix[i+2])
		}
		r, g, bb, _ := img.At(px, py).RGBA()
		return r >> 8, g >> 8, bb >> 8
	}
	// Average at most 8×8 samples a pixel: plenty for a smooth picture,
	// and the furthest zooms (a whole 16k picture in one tile) stay quick.
	nx := max(1, min(8, int(math.Ceil(step))))
	ny := max(1, min(8, int(math.Ceil(stepY))))
	for ty := 0; ty < tilePx; ty++ {
		for tx := 0; tx < tilePx; tx++ {
			var r, g, bb, n uint32
			for j := 0; j < ny; j++ {
				py := int(sy0 + (float64(ty)+(float64(j)+0.5)/float64(ny))*stepY)
				if py < 0 || py >= int(ph) {
					continue
				}
				for i := 0; i < nx; i++ {
					px := int(sx0 + (float64(tx)+(float64(i)+0.5)/float64(nx))*step)
					if px < 0 || px >= int(pw) {
						continue
					}
					cr, cg, cb := at(px, py)
					r, g, bb, n = r+cr, g+cg, bb+cb, n+1
				}
			}
			o := out.PixOffset(tx, ty)
			if n == 0 {
				out.Pix[o], out.Pix[o+1], out.Pix[o+2], out.Pix[o+3] = tileBg.R, tileBg.G, tileBg.B, 255
				continue
			}
			out.Pix[o], out.Pix[o+1], out.Pix[o+2], out.Pix[o+3] = uint8(r/n), uint8(g/n), uint8(bb/n), 255
		}
	}
	return out
}
