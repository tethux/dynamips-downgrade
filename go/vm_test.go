package dynamips_test

import (
	"errors"
	"testing"

	dynamips "github.com/tethux/dynamips-downgrade/go"
	"github.com/tethux/dynamips-downgrade/go/errs"
)

func TestVM(t *testing.T) {
	runtime := testRuntime

	vm, err := runtime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-vm", InstanceID: 42, Platform: "c7200",
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
	nio, nioErr := runtime.CreateUDP(dynamips.UDPConfig{
		Name: "go-bindings-vm-nio", LocalPort: 0,
		RemoteHost: "127.0.0.1", RemotePort: 9,
	})
	if nioErr != nil {
		t.Fatalf("create UDP NIO: %v", nioErr)
	}
	t.Cleanup(nio.Close)
	if bindErr := vm.AttachNIO(0, 0, nio); !errors.Is(bindErr, errs.ErrBindingFailed) {
		t.Fatalf("attach NIO without slot card: got %v, want ErrBindingFailed", bindErr)
	}
	if stopErr := vm.Stop(); stopErr != nil {
		t.Fatalf("stop VM: %v", stopErr)
	}
	if status, statusErr := vm.Status(); statusErr != nil || status != dynamips.VMHalted {
		t.Fatalf("stopped VM status: got %v, %v; want halted", status, statusErr)
	}
	vm.Close()
	if _, statusErr := vm.Status(); !errors.Is(statusErr, errs.ErrClosed) {
		t.Fatalf("closed VM status: got %v, want ErrClosed", statusErr)
	}
}
