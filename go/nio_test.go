package dynamips_test

import (
	"errors"
	"strings"
	"testing"

	dynamips "github.com/tethux/dynamips-downgrade/go"
	"github.com/tethux/dynamips-downgrade/go/errs"
)

func TestNIO(t *testing.T) {
	runtime := testRuntime

	nio, err := runtime.CreateUDP(dynamips.UDPConfig{
		Name: "go-bindings-udp", LocalPort: 0,
		RemoteHost: "127.0.0.1", RemotePort: 9,
	})
	if err != nil {
		t.Fatalf("create UDP NIO: %v", err)
	}
	t.Cleanup(nio.Close)
	if stats, statsErr := nio.Stats(); statsErr != nil || stats != (dynamips.NIOStats{}) {
		t.Fatalf("new NIO stats: got %+v, %v; want zero counters", stats, statsErr)
	}
	if filterErr := nio.SetupFilter(dynamips.FilterRX, "2"); !errors.Is(filterErr, errs.ErrInvalidState) {
		t.Fatalf("setup unbound NIO filter: got %v, want ErrInvalidState", filterErr)
	}
	if filterErr := nio.SetupFilter(dynamips.FilterDirection(9), "2"); !errors.Is(filterErr, errs.ErrInvalidArgument) {
		t.Fatalf("setup invalid NIO filter direction: got %v, want ErrInvalidArgument", filterErr)
	}
	if filterErr := nio.SetupFilter(dynamips.FilterDirection(-1), "2"); !errors.Is(filterErr, errs.ErrInvalidArgument) {
		t.Fatalf("setup negative NIO filter direction: got %v, want ErrInvalidArgument", filterErr)
	}
	autoNIO, localPort, autoErr := runtime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "go-bindings-udp-auto", LocalAddr: "", PortStart: 0, PortEnd: 0,
	})
	if autoErr != nil || localPort == 0 {
		t.Fatalf("create UDP auto NIO: got port %d, %v", localPort, autoErr)
	}
	if connectErr := autoNIO.ConnectUDPAuto("127.0.0.1", 9); connectErr != nil {
		t.Fatalf("connect UDP auto NIO: %v", connectErr)
	}
	if connectErr := autoNIO.ConnectUDPAuto("127.0.0.1", 9); !errors.Is(connectErr, errs.ErrInvalidState) {
		t.Fatalf("connect already connected NIO: got %v, want ErrInvalidState", connectErr)
	}
	if deleteErr := autoNIO.Delete(); deleteErr != nil {
		t.Fatalf("delete UDP auto NIO: %v", deleteErr)
	}
	if _, statsErr := autoNIO.Stats(); !errors.Is(statsErr, errs.ErrClosed) {
		t.Fatalf("deleted NIO stats: got %v, want ErrClosed", statsErr)
	}
	if connectErr := autoNIO.ConnectUDPAuto("127.0.0.1", 9); !errors.Is(connectErr, errs.ErrClosed) {
		t.Fatalf("connect deleted NIO: got %v, want ErrClosed", connectErr)
	}
	if deleteErr := autoNIO.Delete(); !errors.Is(deleteErr, errs.ErrClosed) {
		t.Fatalf("delete deleted NIO: got %v, want ErrClosed", deleteErr)
	}
	recreated, _, recreateErr := runtime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "go-bindings-udp-auto", LocalAddr: "", PortStart: 0, PortEnd: 0,
	})
	if recreateErr != nil {
		t.Fatalf("recreate deleted NIO: %v", recreateErr)
	}
	if deleteErr := recreated.Delete(); deleteErr != nil {
		t.Fatalf("delete recreated NIO: %v", deleteErr)
	}
	if _, createErr := runtime.CreateTAP(dynamips.TAPConfig{Name: "bad-tap", Device: "too/long\x00name"}); !errors.Is(createErr, errs.ErrInvalidArgument) {
		t.Fatalf("invalid TAP device: got %v, want ErrInvalidArgument", createErr)
	}
	if _, createErr := runtime.CreateTAP(dynamips.TAPConfig{Name: "bad-tap", Device: strings.Repeat("x", 1024)}); !errors.Is(createErr, errs.ErrCreateFailed) {
		t.Fatalf("overlong TAP device: got %v, want ErrCreateFailed", createErr)
	}
	sw, switchErr := runtime.CreateEthernetSwitch("go-bindings-nio-delete-switch")
	if switchErr != nil {
		t.Fatalf("create switch for NIO ownership: %v", switchErr)
	}
	defer sw.Close()
	if addErr := sw.AddNIO(nio); addErr != nil {
		t.Fatalf("attach NIO to switch: %v", addErr)
	}
	if deleteErr := nio.Delete(); !errors.Is(deleteErr, errs.ErrInvalidState) {
		t.Fatalf("delete attached NIO: got %v, want ErrInvalidState", deleteErr)
	}
	if _, statsErr := nio.Stats(); statsErr != nil {
		t.Fatalf("attached NIO remains usable after failed delete: %v", statsErr)
	}
	nio.Close()
}
