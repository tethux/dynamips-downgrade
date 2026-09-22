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
	if setErr := vm.SetRAM(256); setErr != nil {
		t.Fatalf("set VM RAM: %v", setErr)
	}
	if _, extractErr := vm.ExtractConfig(); !errors.Is(extractErr, errs.ErrIO) {
		t.Fatalf("extract config without NVRAM: got %v, want ErrIO", extractErr)
	}
	if status, statusErr := vm.Status(); statusErr != nil || status != dynamips.VMHalted {
		t.Fatalf("new VM status: got %v, %v; want halted", status, statusErr)
	}

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
	if bindErr := vm.AttachNIO(0, 0, nio); !errors.Is(bindErr, errs.ErrBindingFailed) {
		t.Fatalf("attach NIO without slot card: got %v, want ErrBindingFailed", bindErr)
	}
	if stats, statsErr := nio.Stats(); statsErr != nil || stats != (dynamips.NIOStats{}) {
		t.Fatalf("new NIO stats: got %+v, %v; want zero counters", stats, statsErr)
	}
	if filterErr := nio.SetupFilter(dynamips.FilterRX, "2"); !errors.Is(filterErr, errs.ErrInvalidState) {
		t.Fatalf("setup unbound NIO filter: got %v, want ErrInvalidState", filterErr)
	}
	if filterErr := nio.SetupFilter(dynamips.FilterDirection(9), "2"); !errors.Is(filterErr, errs.ErrInvalidArgument) {
		t.Fatalf("setup invalid NIO filter direction: got %v, want ErrInvalidArgument", filterErr)
	}
	autoNIO, localPort, autoErr := runtime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "go-bindings-udp-auto", LocalAddr: "",
		PortStart: 0, PortEnd: 0,
	})
	if autoErr != nil || localPort == 0 {
		t.Fatalf("create UDP auto NIO: got port %d, %v", localPort, autoErr)
	}
	t.Cleanup(autoNIO.Close)
	sw, switchErr := runtime.CreateEthernetSwitch("go-bindings-switch")
	if switchErr != nil {
		t.Fatalf("create Ethernet switch: %v", switchErr)
	}
	t.Cleanup(sw.Close)
	if addErr := sw.AddNIO(autoNIO); addErr != nil {
		t.Fatalf("add NIO to Ethernet switch: %v", addErr)
	}

	if closeErr := runtime.Close(); !errors.Is(closeErr, errs.ErrInUse) {
		t.Fatalf("close runtime with live handles: got %v, want ErrInUse", closeErr)
	}
	if stopErr := vm.Stop(); stopErr != nil {
		t.Fatalf("stop VM: %v", stopErr)
	}
	if status, statusErr := vm.Status(); statusErr != nil || status != dynamips.VMHalted {
		t.Fatalf("stopped VM status: got %v, %v; want halted", status, statusErr)
	}
	nio.Close()
	autoNIO.Close()
	sw.Close()
	vm.Close()
	if _, statusErr := vm.Status(); !errors.Is(statusErr, errs.ErrClosed) {
		t.Fatalf("closed VM status: got %v, want ErrClosed", statusErr)
	}
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
