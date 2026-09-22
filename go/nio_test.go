package dynamips_test

import (
	"errors"
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
	autoNIO, localPort, autoErr := runtime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "go-bindings-udp-auto", LocalAddr: "", PortStart: 0, PortEnd: 0,
	})
	if autoErr != nil || localPort == 0 {
		t.Fatalf("create UDP auto NIO: got port %d, %v", localPort, autoErr)
	}
	autoNIO.Close()
	nio.Close()
}
