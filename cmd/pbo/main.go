// Command pbo packs the ReCON folder into @ReCON/addons/ReCON.pbo, the
// mod DayZ loads, on any OS, with no DayZ Tools needed. The mod is scripts
// and a text config.cpp, which DayZ reads as they are, so nothing needs
// binarizing.
//
//	go run ./cmd/pbo                  # ReCON -> @ReCON/addons/ReCON.pbo
//	go run ./cmd/pbo -src ReCON -prefix ReCON -out @ReCON
//
// It also writes @ReCON/mod.cpp (the name the launcher shows) and copies
// LICENSE and NOTICE into @ReCON.
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/MowMdown/ReCON-Mod/pbo"
)

func main() {
	src := flag.String("src", "ReCON", "the folder to pack")
	prefix := flag.String("prefix", "ReCON", "the PBO's prefix: the path its files have in the game (config.cpp's files[] start with it)")
	out := flag.String("out", "@ReCON", "the mod folder to write: <out>/addons/<prefix>.pbo")
	flag.Parse()

	files, err := collect(*src)
	if err != nil {
		log.Fatal(err)
	}
	if _, ok := findFile(files, "config.cpp"); !ok {
		log.Fatalf("%s has no config.cpp", *src)
	}
	data, err := pbo.Pack(*prefix, files, time.Now())
	if err != nil {
		log.Fatal(err)
	}
	addons := filepath.Join(*out, "addons")
	if err := os.MkdirAll(addons, 0o755); err != nil {
		log.Fatal(err)
	}
	pboPath := filepath.Join(addons, *prefix+".pbo")
	if err := os.WriteFile(pboPath, data, 0o644); err != nil {
		log.Fatal(err)
	}
	modCpp := "name = \"ReCON\";\npicture = \"\";\nactionName = \"GitHub\";\naction = \"https://github.com/MowMdown/ReCON-Mod\";\n" +
		"tooltip = \"ReCON companion mod (server-side). Not for monetized servers.\";\noverview = \"Live player data and actions for ReCON. Not for monetized servers: see NOTICE.\";\nauthor = \"MowMdown\";\n"
	if err := os.WriteFile(filepath.Join(*out, "mod.cpp"), []byte(modCpp), 0o644); err != nil {
		log.Fatal(err)
	}
	for _, f := range []string{"LICENSE", "NOTICE"} {
		if b, err := os.ReadFile(f); err == nil {
			if err := os.WriteFile(filepath.Join(*out, f), b, 0o644); err != nil {
				log.Fatal(err)
			}
		}
	}
	fmt.Printf("%s: %d files, %d bytes\n", pboPath, len(files), len(data))
}

// collect reads every file under dir, with its path inside dir.
func collect(dir string) ([]pbo.File, error) {
	var files []pbo.File
	err := filepath.WalkDir(dir, func(p string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			if strings.HasPrefix(d.Name(), ".") && p != dir {
				return filepath.SkipDir
			}
			return nil
		}
		if strings.HasPrefix(d.Name(), ".") {
			return nil
		}
		b, err := os.ReadFile(p)
		if err != nil {
			return err
		}
		rel, err := filepath.Rel(dir, p)
		if err != nil {
			return err
		}
		files = append(files, pbo.File{Name: filepath.ToSlash(rel), Data: b})
		return nil
	})
	return files, err
}

func findFile(files []pbo.File, name string) (pbo.File, bool) {
	for _, f := range files {
		if strings.EqualFold(f.Name, name) {
			return f, true
		}
	}
	return pbo.File{}, false
}
