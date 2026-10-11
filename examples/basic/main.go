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
	"github.com/tethux/dynamips-downgrade/errs"
)

var (
	errVMConfiguration        = errors.New("VM configuration failed")
	errArchiveMultipleImages  = errors.New("IOS archive contains multiple images")
	errArchiveInvalidSize     = errors.New("IOS archive needs one image of at most 256 MiB")
	errArchiveIncompleteImage = errors.New("IOS archive has an incomplete image")
)

func main() {
	image := flag.String("ios", "", "path to a C7200 IOS image or zip archive")
	ram := flag.Uint("ram", 256, "router RAM in MiB")
	consolePort := flag.Uint("console-port", 2000, "TCP console port")
	enhanced := flag.Bool("enhanced", false, "use local console editing and history")
	flag.Parse()
	if *image == "" || *ram == 0 || *ram > 2048 || *consolePort == 0 || *consolePort > 65535 {
		flag.Usage()
		os.Exit(2)
		return
	}
	if err := run(*image, uint32(*ram), uint16(*consolePort), *enhanced); err != nil {
		log.Fatal(err)
	}
}

func run(image string, ram uint32, consolePort uint16, enhanced bool) (runErr error) {
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
	if changeErr := os.Chdir(exampleDirectory); changeErr != nil {
		return changeErr
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
		if configErr := step.run(); configErr != nil {
			return &errs.Operation{Op: "configure " + step.name, Kind: errVMConfiguration, Cause: configErr}
		}
	}
	if startErr := vm.Start(); startErr != nil {
		return startErr
	}
	started = true
	signals := []os.Signal{syscall.SIGTERM}
	if !enhanced {
		signals = append(signals, os.Interrupt)
	}
	ctx, stop := signal.NotifyContext(context.Background(), signals...)
	defer stop()
	if enhanced {
		raw, consoleErr := runEnhancedConsole(ctx, consolePort)
		if consoleErr != nil || !raw {
			return consoleErr
		}
	}
	return runTelnet(ctx, consolePort)
}

func runTelnet(ctx context.Context, consolePort uint16) error {
	fmt.Printf("C7200 booting on console port %d. Press Ctrl+] then type quit to stop.\n", consolePort)
	//nolint:gosec // Fixed executable and loopback host; the port is a uint16.
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
				return "", &errs.Operation{Op: "select IOS image", Kind: errs.ErrInvalidArgument, Cause: errArchiveMultipleImages}
			}
			selected = file
		}
	}
	if selected == nil || selected.UncompressedSize64 == 0 || selected.UncompressedSize64 > 256<<20 {
		return "", &errs.Operation{Op: "select IOS image", Kind: errs.ErrInvalidArgument, Cause: errArchiveInvalidSize}
	}
	source, err := selected.Open()
	if err != nil {
		return "", err
	}
	defer func() { resultErr = errors.Join(resultErr, source.Close()) }()
	image = filepath.Join(directory, "ios.bin")
	//nolint:gosec // The destination is fixed inside the example-owned temporary directory.
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
		return "", &errs.Operation{Op: "extract IOS image", Kind: errs.ErrIO, Cause: errArchiveIncompleteImage}
	}
	return image, nil
}
