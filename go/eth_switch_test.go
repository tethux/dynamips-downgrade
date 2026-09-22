package dynamips_test

import (
	"testing"

	dynamips "github.com/tethux/dynamips-downgrade/go"
)

func TestEthernetSwitch(t *testing.T) {
	runtime := testRuntime

	nio, _, err := runtime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "go-bindings-switch-nio", PortStart: 0, PortEnd: 0,
	})
	if err != nil {
		t.Fatalf("create UDP auto NIO: %v", err)
	}
	t.Cleanup(nio.Close)
	sw, err := runtime.CreateEthernetSwitch("go-bindings-switch")
	if err != nil {
		t.Fatalf("create Ethernet switch: %v", err)
	}
	t.Cleanup(sw.Close)
	if addErr := sw.AddNIO(nio); addErr != nil {
		t.Fatalf("add NIO to Ethernet switch: %v", addErr)
	}
	sw.Close()
	nio.Close()
}
