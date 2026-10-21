// Command consumer demonstrates using the published Dynamips Go module.
package main

import (
	"errors"
	"log"
	"os"

	dynamips "github.com/tethux/dynamips-downgrade"
)

func main() {
	if err := run(); err != nil {
		log.Print(err)
		os.Exit(1)
	}
}

func run() (result error) {
	previous, err := os.Getwd()
	if err != nil {
		return err
	}
	directory, directoryErr := os.MkdirTemp("", "dynamips-consumer-")
	if directoryErr != nil {
		return directoryErr
	}
	defer func() {
		result = errors.Join(result, os.Chdir(previous), os.RemoveAll(directory))
	}()
	if changeErr := os.Chdir(directory); changeErr != nil {
		return changeErr
	}
	runtime, initErr := dynamips.New()
	if initErr != nil {
		return initErr
	}
	return runtime.Close()
}
