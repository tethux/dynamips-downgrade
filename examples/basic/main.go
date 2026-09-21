package main

import (
	"errors"
	"log"
	"os"

	"github.com/tethux/dynamips-downgrade"
)

func main() {
	if err := run(); err != nil {
		log.Fatal(err)
	}
}

func run() (runErr error) {
	workingDirectory, err := os.Getwd()
	if err != nil {
		return err
	}
	exampleDirectory, err := os.MkdirTemp("", "dynamips-example-")
	if err != nil {
		return err
	}
	defer func() {
		runErr = errors.Join(runErr, os.Chdir(workingDirectory), os.RemoveAll(exampleDirectory))
	}()
	if err := os.Chdir(exampleDirectory); err != nil {
		return err
	}

	runtime, err := dynamips.New()
	if err != nil {
		return err
	}

	vm, err := runtime.CreateVM(dynamips.VMConfig{
		Name:       "router-1",
		InstanceID: 1,
		Platform:   "c7200",
	})
	if err != nil {
		return errors.Join(err, runtime.Close())
	}

	if err := vm.Stop(); err != nil {
		vm.Close()
		return errors.Join(err, runtime.Close())
	}

	vm.Close()
	return runtime.Close()
}
