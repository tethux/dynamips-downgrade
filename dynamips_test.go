package dynamips_test

import (
	"errors"
	"os"
	"testing"

	"github.com/tethux/dynamips-downgrade"
	"github.com/tethux/dynamips-downgrade/errs"
)

func TestRuntimeVMAndUDP(t *testing.T) {
	useTempWorkingDirectory(t)

	runtime, err := dynamips.New()
	if err != nil {
		t.Fatalf("initialize runtime: %v", err)
	}
	t.Cleanup(func() {
		if closeErr := runtime.Close(); closeErr != nil {
			t.Errorf("close runtime: %v", closeErr)
		}
	})

	vm, err := runtime.CreateVM(dynamips.VMConfig{
		Name:       "go-bindings-test",
		InstanceID: 42,
		Platform:   "c7200",
	})
	if err != nil {
		t.Fatalf("create VM: %v", err)
	}
	t.Cleanup(vm.Close)

	nio, err := runtime.CreateUDP(dynamips.UDPConfig{
		Name:       "go-bindings-udp",
		LocalPort:  0,
		RemoteHost: "127.0.0.1",
		RemotePort: 9,
	})
	if err != nil {
		t.Fatalf("create UDP NIO: %v", err)
	}
	t.Cleanup(nio.Close)

	if closeErr := runtime.Close(); !errors.Is(closeErr, errs.ErrInUse) {
		t.Fatalf("close runtime with live handles: got %v, want ErrInUse", closeErr)
	}
	if stopErr := vm.Stop(); stopErr != nil {
		t.Fatalf("stop VM: %v", stopErr)
	}
	nio.Close()
	vm.Close()
	if closeErr := runtime.Close(); closeErr != nil {
		t.Fatalf("close runtime: %v", closeErr)
	}
}

func useTempWorkingDirectory(t *testing.T) {
	t.Helper()

	workingDirectory, err := os.Getwd()
	if err != nil {
		t.Fatalf("get working directory: %v", err)
	}
	if err := os.Chdir(t.TempDir()); err != nil {
		t.Fatalf("enter temporary working directory: %v", err)
	}
	t.Cleanup(func() {
		if err := os.Chdir(workingDirectory); err != nil {
			t.Errorf("restore working directory: %v", err)
		}
	})
}
