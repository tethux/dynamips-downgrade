package dynamips_test

import (
	"bufio"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"testing"
	"time"

	dynamips "github.com/tethux/dynamips-downgrade/go"
)

type legacyClient struct {
	conn   net.Conn
	reader *bufio.Reader
	cmd    *exec.Cmd
}

func TestLegacyHypervisorParity(t *testing.T) {
	_, source, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("find integration test source")
	}
	binary := filepath.Join(filepath.Dir(source), "..", "build", "linux", "x86_64", "debug", "dynamips")
	if _, err := os.Stat(binary); os.IsNotExist(err) {
		t.Skipf("legacy hypervisor executable is not built at %s; run mise exec -- xmake build -y dynamips", binary)
	} else if err != nil {
		t.Fatalf("stat legacy hypervisor executable: %v", err)
	}

	client := startLegacyHypervisor(t, binary)
	vmName := "legacy-parity-vm"
	legacyOK(t, client, "vm create "+vmName+" 71 c7200")
	vm, err := testRuntime.CreateVM(dynamips.VMConfig{Name: vmName, InstanceID: 71, Platform: "c7200"})
	if err != nil {
		t.Fatalf("embedded VM create: %v", err)
	}
	t.Cleanup(vm.Close)

	compareVMStatus(t, client, vmName, vm, dynamips.VMHalted)
	legacyOK(t, client, "vm set_ram "+vmName+" 256")
	if err := vm.SetRAM(256); err != nil {
		t.Fatalf("embedded VM set RAM: %v", err)
	}
	legacyOK(t, client, "vm stop "+vmName)
	if err := vm.Stop(); err != nil {
		t.Fatalf("embedded VM stop: %v", err)
	}
	compareVMStatus(t, client, vmName, vm, dynamips.VMHalted)

	legacyPort := legacyOK(t, client, "nio create_udp_auto legacy-parity-nio 127.0.0.1 0 0")
	port, err := strconv.ParseUint(legacyPort, 10, 16)
	if err != nil || port == 0 {
		t.Fatalf("legacy UDP auto local port: %q, %v", legacyPort, err)
	}
	nio, embeddedPort, err := testRuntime.CreateUDPAuto(dynamips.UDPAutoConfig{
		Name: "legacy-parity-nio", LocalAddr: "127.0.0.1", PortStart: 0, PortEnd: 0,
	})
	if err != nil || embeddedPort == 0 {
		t.Fatalf("embedded UDP auto local port: %d, %v", embeddedPort, err)
	}
	t.Cleanup(nio.Close)

	var legacyStats dynamips.NIOStats
	statsText := legacyOK(t, client, "nio get_stats legacy-parity-nio")
	if _, err := fmt.Sscan(statsText, &legacyStats.PacketsIn, &legacyStats.PacketsOut, &legacyStats.BytesIn, &legacyStats.BytesOut); err != nil {
		t.Fatalf("parse legacy NIO stats %q: %v", statsText, err)
	}
	embeddedStats, err := nio.Stats()
	if err != nil {
		t.Fatalf("embedded NIO stats: %v", err)
	}
	if embeddedStats != legacyStats {
		t.Fatalf("NIO stats differ: legacy %+v, embedded %+v", legacyStats, embeddedStats)
	}

	legacyOK(t, client, "ethsw create legacy-parity-switch")
	sw, err := testRuntime.CreateEthernetSwitch("legacy-parity-switch")
	if err != nil {
		t.Fatalf("embedded Ethernet switch create: %v", err)
	}
	t.Cleanup(sw.Close)
	legacyOK(t, client, "ethsw add_nio legacy-parity-switch legacy-parity-nio")
	if err := sw.AddNIO(nio); err != nil {
		t.Fatalf("embedded Ethernet switch add NIO: %v", err)
	}
}

func compareVMStatus(t *testing.T, client *legacyClient, name string, vm *dynamips.VM, want dynamips.VMStatus) {
	t.Helper()
	statusText := legacyOK(t, client, "vm get_status "+name)
	legacyStatus, err := strconv.ParseUint(statusText, 10, 32)
	if err != nil {
		t.Fatalf("parse legacy VM status %q: %v", statusText, err)
	}
	embeddedStatus, err := vm.Status()
	if err != nil {
		t.Fatalf("embedded VM status: %v", err)
	}
	if dynamips.VMStatus(legacyStatus) != embeddedStatus || embeddedStatus != want {
		t.Fatalf("VM status differs: legacy %d, embedded %d, want %d", legacyStatus, embeddedStatus, want)
	}
}

func startLegacyHypervisor(t *testing.T, binary string) *legacyClient {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve hypervisor TCP port: %v", err)
	}
	address := listener.Addr().String()
	if err := listener.Close(); err != nil {
		t.Fatalf("release hypervisor TCP port: %v", err)
	}

	output, err := os.Create(filepath.Join(t.TempDir(), "hypervisor.log"))
	if err != nil {
		t.Fatalf("create hypervisor output file: %v", err)
	}
	cmd := exec.Command(binary, "-H", address)
	cmd.Dir = t.TempDir()
	cmd.Stdout = output
	cmd.Stderr = output
	if err := cmd.Start(); err != nil {
		if closeErr := output.Close(); closeErr != nil {
			t.Errorf("close hypervisor output: %v", closeErr)
		}
		t.Fatalf("start legacy hypervisor: %v", err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	var conn net.Conn
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		conn, err = net.DialTimeout("tcp", address, 100*time.Millisecond)
		if err == nil {
			break
		}
		select {
		case exitErr := <-done:
			if closeErr := output.Close(); closeErr != nil {
				t.Errorf("close hypervisor output: %v", closeErr)
			}
			t.Fatalf("legacy hypervisor exited before listening: %v; output: %s", exitErr, readLegacyOutput(output.Name()))
		default:
		}
		time.Sleep(25 * time.Millisecond)
	}
	if conn == nil {
		if killErr := cmd.Process.Kill(); killErr != nil {
			t.Errorf("kill unresponsive legacy hypervisor: %v", killErr)
		}
		<-done
		if closeErr := output.Close(); closeErr != nil {
			t.Errorf("close hypervisor output: %v", closeErr)
		}
		t.Fatalf("legacy hypervisor did not listen on %s: %v; output: %s", address, err, readLegacyOutput(output.Name()))
	}
	client := &legacyClient{conn: conn, reader: bufio.NewReader(conn), cmd: cmd}
	t.Cleanup(func() {
		if err := conn.SetDeadline(time.Now().Add(2 * time.Second)); err == nil {
			_, _ = fmt.Fprint(conn, "hypervisor stop\n")
		}
		_ = conn.Close()
		select {
		case <-done:
		case <-time.After(3 * time.Second):
			_ = cmd.Process.Kill()
			<-done
			t.Errorf("legacy hypervisor did not stop within three seconds; output: %s", readLegacyOutput(output.Name()))
		}
		if err := output.Close(); err != nil {
			t.Errorf("close hypervisor output: %v", err)
		}
	})
	return client
}

func legacyOK(t *testing.T, client *legacyClient, command string) string {
	t.Helper()
	if err := client.conn.SetDeadline(time.Now().Add(3 * time.Second)); err != nil {
		t.Fatalf("set legacy command deadline: %v", err)
	}
	if _, err := fmt.Fprintln(client.conn, command); err != nil {
		t.Fatalf("send legacy command %q: %v", command, err)
	}
	for {
		line, err := client.reader.ReadString('\n')
		if err != nil {
			t.Fatalf("read legacy response to %q: %v", command, err)
		}
		line = strings.TrimRight(line, "\r\n")
		if len(line) < 4 || (line[3] != '-' && line[3] != ' ') {
			t.Fatalf("malformed legacy response to %q: %q", command, line)
		}
		if line[3] == ' ' {
			continue
		}
		if line[:3] != "100" {
			t.Fatalf("legacy command %q failed: %s", command, line)
		}
		return line[4:]
	}
}

func readLegacyOutput(path string) string {
	data, err := os.ReadFile(path)
	if err != nil {
		return err.Error()
	}
	return string(data)
}
