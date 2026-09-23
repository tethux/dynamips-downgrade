// Command consumer demonstrates using the published Dynamips Go module.
package main

import (
	"log"

	dynamips "github.com/tethux/dynamips-downgrade"
)

func main() {
	runtime, err := dynamips.New()
	if err != nil {
		log.Fatal(err)
	}
	if err := runtime.Close(); err != nil {
		log.Fatal(err)
	}
}
