// Command basic boots a local C7200 IOS image until interrupted.
package main

import (
	"archive/zip"
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"

	dynamips "github.com/tethux/dynamips-downgrade"
)

func main() {
	image := flag.String("ios", "", "path to a C7200 IOS image or zip archive")
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
	image, err = imageFromArchive(image, exampleDirectory)
	if err != nil {
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
		{"initial config", func() error { return vm.PushConfig([]byte("hostname Router\nno ip domain-lookup\nend\n"), []byte{}) }},
		{"config register", func() error { return vm.SetConfigRegister(0x2102) }},
	} {
		if err := step.run(); err != nil {
			return fmt.Errorf("configure %s: %w", step.name, err)
		}
	}
	if err := vm.Start(); err != nil {
		return err
	}
	started = true
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	fmt.Printf("C7200 booting on console port %d. Press Ctrl+] then type quit to stop.\n", consolePort)
	console := exec.CommandContext(ctx, "telnet", "127.0.0.1", strconv.Itoa(int(consolePort)))
	console.Stdin = os.Stdin
	console.Stdout = os.Stdout
	console.Stderr = os.Stderr
	if err := console.Run(); err != nil && ctx.Err() == nil {
		return err
	}
	return nil
}

func imageFromArchive(path, directory string) (image string, resultErr error) {
	if !strings.EqualFold(filepath.Ext(path), ".zip") {
		return path, nil
	}
	archive, err := zip.OpenReader(path)
	if err != nil {
		return "", err
	}
	defer func() { resultErr = errors.Join(resultErr, archive.Close()) }()
	var selected *zip.File
	for _, file := range archive.File {
		lower := strings.ToLower(file.Name)
		if strings.HasSuffix(lower, ".bin") || strings.HasSuffix(lower, ".image") {
			if selected != nil {
				return "", errors.New("IOS archive contains multiple images")
			}
			selected = file
		}
	}
	if selected == nil || selected.UncompressedSize64 == 0 || selected.UncompressedSize64 > 256<<20 {
		return "", errors.New("IOS archive needs one image of at most 256 MiB")
	}
	source, err := selected.Open()
	if err != nil {
		return "", err
	}
	defer func() { resultErr = errors.Join(resultErr, source.Close()) }()
	image = filepath.Join(directory, "ios.bin")
	target, err := os.OpenFile(image, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0o600)
	if err != nil {
		return "", err
	}
	written, copyErr := io.Copy(target, io.LimitReader(source, 256<<20+1))
	closeErr := target.Close()
	if joined := errors.Join(copyErr, closeErr); joined != nil {
		return "", joined
	}
	if written != int64(selected.UncompressedSize64) {
		return "", errors.New("IOS archive has an incomplete image")
	}
	return image, nil
}
