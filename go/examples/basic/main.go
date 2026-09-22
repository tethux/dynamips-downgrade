// Command basic boots a local C7200 IOS image until interrupted.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"

	dynamips "github.com/tethux/dynamips-downgrade/go"
)

func main() {
	image := flag.String("ios", "", "path to a C7200 IOS image")
	ram := flag.Uint("ram", 256, "router RAM in MiB")
	consolePort := flag.Uint("console-port", 2000, "TCP console port")
	flag.Parse()
	if *image == "" || *ram == 0 || *ram > 2048 || *consolePort == 0 || *consolePort > 65535 {
		flag.Usage()
		os.Exit(2)
	}
	if err := run(*image, uint32(*ram), uint16(*consolePort)); err != nil {
		log.Fatal(err)
	}
}

func run(image string, ram uint32, consolePort uint16) (runErr error) {
	workingDirectory, err := os.Getwd()
	if err != nil {
		return err
	}
	image, err = filepath.Abs(image)
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
	defer func() { runErr = errors.Join(runErr, runtime.Close()) }()

	vm, err := runtime.CreateVM(dynamips.VMConfig{Name: "router-1", InstanceID: 1, Platform: "c7200"})
	if err != nil {
		return err
	}
	started := false
	defer func() {
		if started {
			runErr = errors.Join(runErr, vm.Stop())
		}
		runErr = errors.Join(runErr, vm.Delete())
		vm.Close()
	}()
	for _, step := range []struct {
		name string
		run  func() error
	}{
		{"RAM", func() error { return vm.SetRAM(ram) }},
		{"IOS", func() error { return vm.SetIOS(image) }},
		{"NPE", func() error { return vm.SetC7200NPE("npe-400") }},
		{"midplane", func() error { return vm.SetC7200Midplane("vxr") }},
		{"card", func() error { return vm.AddCard(1, "PA-FE-TX") }},
		{"console", func() error { return vm.SetConsoleTCPPort(consolePort) }},
	} {
		if err := step.run(); err != nil {
			return fmt.Errorf("configure %s: %w", step.name, err)
		}
	}
	if err := vm.Start(); err != nil {
		return err
	}
	started = true
	fmt.Printf("C7200 booting; connect to 127.0.0.1:%d for the IOS console. Press Ctrl-C to stop.\n", consolePort)
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	<-ctx.Done()
	return nil
}
