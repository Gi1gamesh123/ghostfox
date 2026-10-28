// Command necko-worker is the engine's HTTP transfer process: it reads
// request frames from stdin and writes response frames to stdout until the
// input closes. Spawned by the engine automatically; not for direct use.
package main

import (
	"log"
	"os"

	"ghostfox/ghostfox/necko"
)

func main() {
	// Log output must stay away from stdout (the protocol stream).
	log.SetOutput(os.Stderr)
	worker := necko.New()
	if err := worker.Serve(os.Stdin, os.Stdout); err != nil {
		log.Fatal(err)
	}
}
