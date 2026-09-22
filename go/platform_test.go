package dynamips_test

import (
	"errors"
	"testing"

	dynamips "github.com/tethux/dynamips-downgrade/go"
	"github.com/tethux/dynamips-downgrade/go/errs"
)

func TestC7200Configuration(t *testing.T) {
	vm, err := testRuntime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-platform-c7200", InstanceID: 71, Platform: "c7200",
	})
	if err != nil {
		t.Fatalf("create C7200: %v", err)
	}
	t.Cleanup(vm.Close)
	if err := vm.SetC7200NPE("npe-400"); err != nil {
		t.Fatalf("set NPE: %v", err)
	}
	if err := vm.SetC7200Midplane("vxr"); err != nil {
		t.Fatalf("set midplane: %v", err)
	}
	if err := vm.SetC7200MACAddr([6]byte{0xca, 0xfe, 0x00, 0x00, 0x00, 0x71}); err != nil {
		t.Fatalf("set MAC address: %v", err)
	}
	if err := vm.SetC7200NPE("unknown-npe"); !errors.Is(err, errs.ErrInvalidState) {
		t.Fatalf("unknown NPE: got %v, want ErrInvalidState", err)
	}

	other, err := testRuntime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-platform-c3600", InstanceID: 72, Platform: "c3600",
	})
	if err != nil {
		t.Fatalf("create C3600: %v", err)
	}
	t.Cleanup(other.Close)
	if err := other.SetC7200NPE("npe-400"); !errors.Is(err, errs.ErrUnsupported) {
		t.Fatalf("wrong platform: got %v, want ErrUnsupported", err)
	}
}
