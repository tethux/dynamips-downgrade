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

func TestVMConfigurationAndDelete(t *testing.T) {
	vm, err := testRuntime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-vm-delete", InstanceID: 43, Platform: "c7200",
	})
	if err != nil {
		t.Fatalf("create VM: %v", err)
	}
	t.Cleanup(vm.Close)
	if setErr := vm.SetIOS("/nonexistent/router.image"); setErr != nil {
		t.Fatalf("set IOS: %v", setErr)
	}
	if setErr := vm.SetNVRAM(128); setErr != nil {
		t.Fatalf("set NVRAM: %v", setErr)
	}
	if setErr := vm.SetSparseMemory(true); setErr != nil {
		t.Fatalf("set sparse memory: %v", setErr)
	}
	if setErr := vm.SetConfigRegister(0x2102); setErr != nil {
		t.Fatalf("set config register: %v", setErr)
	}
	if setErr := vm.SetIdlePC(0x12345678); setErr != nil {
		t.Fatalf("set idle PC: %v", setErr)
	}
	if setErr := vm.SetConsoleTCPPort(2001); setErr != nil {
		t.Fatalf("set console port: %v", setErr)
	}
	if setErr := vm.SetConsoleTCPPort(0); !errors.Is(setErr, errs.ErrInvalidArgument) {
		t.Fatalf("zero console port: got %v, want invalid argument", setErr)
	}
	if suspendErr := vm.Suspend(); suspendErr != nil {
		t.Fatalf("suspend halted VM: %v", suspendErr)
	}
	if resumeErr := vm.Resume(); resumeErr != nil {
		t.Fatalf("resume halted VM: %v", resumeErr)
	}
	if _, pushErr := vm.ExtractConfig(); !errors.Is(pushErr, errs.ErrIO) {
		t.Fatalf("extract before NVRAM init: got %v, want I/O error", pushErr)
	}
	startup := []byte("hostname vm-delete\n")
	if pushErr := vm.PushConfig(startup, []byte{}); pushErr != nil {
		t.Fatalf("push config: %v", pushErr)
	}
	config, extractErr := vm.ExtractConfig()
	if extractErr != nil || string(config.Startup) != string(startup) {
		t.Fatalf("extract pushed config: got %q, %v", config.Startup, extractErr)
	}
	if deleteErr := vm.Delete(); deleteErr != nil {
		t.Fatalf("delete VM: %v", deleteErr)
	}
	if _, statusErr := vm.Status(); !errors.Is(statusErr, errs.ErrClosed) {
		t.Fatalf("status after deletion: got %v, want closed", statusErr)
	}
	recreated, createErr := testRuntime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-vm-delete", InstanceID: 43, Platform: "c7200",
	})
	if createErr != nil {
		t.Fatalf("recreate deleted VM: %v", createErr)
	}
	t.Cleanup(recreated.Close)
}

func TestVMSlotBindings(t *testing.T) {
	vm, err := testRuntime.CreateVM(dynamips.VMConfig{Name: "go-bindings-slots", InstanceID: 44, Platform: "c7200"})
	if err != nil {
		t.Fatalf("create VM: %v", err)
	}
	t.Cleanup(vm.Close)
	if addErr := vm.AddCard(1, "unknown-card"); !errors.Is(addErr, errs.ErrBindingFailed) {
		t.Fatalf("unknown card: got %v, want binding failure", addErr)
	}
	if addErr := vm.AddCard(1, "PA-FE-TX"); addErr != nil {
		t.Fatalf("add card: %v", addErr)
	}
	if addErr := vm.AddCard(1, "PA-FE-TX"); !errors.Is(addErr, errs.ErrBindingFailed) {
		t.Fatalf("duplicate card: got %v, want binding failure", addErr)
	}
	nio, _, nioErr := testRuntime.CreateUDPAuto(dynamips.UDPAutoConfig{Name: "go-bindings-slots-nio", PortStart: 0, PortEnd: 0})
	if nioErr != nil {
		t.Fatalf("create NIO: %v", nioErr)
	}
	t.Cleanup(nio.Close)
	if attachErr := vm.AttachNIO(1, 0, nio); attachErr != nil {
		t.Fatalf("attach NIO: %v", attachErr)
	}
	if attachErr := vm.AttachNIO(1, 0, nio); !errors.Is(attachErr, errs.ErrBindingFailed) {
		t.Fatalf("duplicate NIO: got %v, want binding failure", attachErr)
	}
	if deleteErr := nio.Delete(); !errors.Is(deleteErr, errs.ErrInvalidState) {
		t.Fatalf("delete bound NIO: got %v, want invalid state", deleteErr)
	}
	if detachErr := vm.DetachNIO(1, 0); detachErr != nil {
		t.Fatalf("detach NIO: %v", detachErr)
	}
	if detachErr := vm.DetachNIO(1, 0); !errors.Is(detachErr, errs.ErrBindingFailed) {
		t.Fatalf("detach absent NIO: got %v, want binding failure", detachErr)
	}
	if attachErr := vm.AttachNIO(1, 0, nio); attachErr != nil {
		t.Fatalf("reattach NIO: %v", attachErr)
	}
	if removeErr := vm.RemoveCard(1); removeErr != nil {
		t.Fatalf("remove card: %v", removeErr)
	}
	if removeErr := vm.RemoveCard(1); !errors.Is(removeErr, errs.ErrBindingFailed) {
		t.Fatalf("remove absent card: got %v, want binding failure", removeErr)
	}
	if deleteErr := nio.Delete(); deleteErr != nil {
		t.Fatalf("delete NIO after card removal: %v", deleteErr)
	}
	vm.Close()
	if addErr := vm.AddCard(1, "PA-FE-TX"); !errors.Is(addErr, errs.ErrClosed) {
		t.Fatalf("add card on closed VM: got %v, want closed", addErr)
	}
	if removeErr := vm.RemoveCard(1); !errors.Is(removeErr, errs.ErrClosed) {
		t.Fatalf("remove card on closed VM: got %v, want closed", removeErr)
	}
	if detachErr := vm.DetachNIO(1, 0); !errors.Is(detachErr, errs.ErrClosed) {
		t.Fatalf("detach on closed VM: got %v, want closed", detachErr)
	}
}
