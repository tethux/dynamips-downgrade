package dynamips_test

import (
	"archive/zip"
	"bytes"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"testing"
	"time"

	dynamips "github.com/tethux/dynamips-downgrade/go"
)

var iosPrompt = regexp.MustCompile(`(?:^|[\r\n])([A-Za-z][A-Za-z0-9_-]*(?:\(config\))?)([>#])\s*$`)

type iosRun struct {
	startup time.Duration
	ready   time.Duration
	metrics processMetrics
	outputs map[string]string
}

type processMetrics struct {
	cpuUser   time.Duration
	cpuSystem time.Duration
	rssStart  uint64
	rssReady  uint64
	rssPeak   uint64
}

type processSample struct {
	userTicks   uint64
	systemTicks uint64
	rssBytes    uint64
}

// Set DYNAMIPS_IOS_ARCHIVE to a C7200 IOS zip to run this hardware integration test.
// DYNAMIPS_IOS_RAM_MB selects the image's required RAM; it defaults to 256.
func TestC7200IOSBootParity(t *testing.T) {
	archive := os.Getenv("DYNAMIPS_IOS_ARCHIVE")
	if archive == "" {
		t.Skip("set DYNAMIPS_IOS_ARCHIVE to run the C7200 IOS boot test")
	}
	image := extractC7200IOS(t, archive)
	ramMB := uint32(256)
	if configured := os.Getenv("DYNAMIPS_IOS_RAM_MB"); configured != "" {
		parsed, err := strconv.ParseUint(configured, 10, 32)
		if err != nil || parsed < 128 || parsed > 2048 {
			t.Fatalf("DYNAMIPS_IOS_RAM_MB must be 128..2048: %q", configured)
		}
		ramMB = uint32(parsed)
	}

	_, source, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("find integration test source")
	}
	binary := filepath.Join(filepath.Dir(source), "..", "build", "linux", "x86_64", "debug", "dynamips")
	if _, err := os.Stat(binary); err != nil {
		t.Fatalf("legacy hypervisor executable %s: %v; run mise exec -- xmake build -y dynamips", binary, err)
	}
	hz := clockTicksPerSecond(t)

	embedded := bootEmbeddedC7200(t, image, ramMB, hz)
	legacy := bootLegacyC7200(t, binary, image, ramMB, hz)
	for _, command := range []string{"terminal length 0", "show version", "show ip interface brief", "configure terminal", "hostname paritytest", "end"} {
		if _, ok := embedded.outputs[command]; !ok {
			t.Errorf("embedded IOS did not complete %q", command)
		}
		if _, ok := legacy.outputs[command]; !ok {
			t.Errorf("legacy IOS did not complete %q", command)
		}
	}
	for _, command := range []string{"show version", "show ip interface brief"} {
		for _, marker := range map[string][]string{
			"show version":            {"Cisco IOS Software", "uptime is"},
			"show ip interface brief": {"Interface", "FastEthernet1/0"},
		}[command] {
			if !strings.Contains(embedded.outputs[command], marker) || !strings.Contains(legacy.outputs[command], marker) {
				t.Errorf("%q missing %q in embedded or legacy output", command, marker)
			}
		}
	}
	for _, run := range []struct {
		name string
		data iosRun
	}{{"embedded", embedded}, {"legacy", legacy}} {
		m := run.data.metrics
		t.Logf("%s: startup=%s IOS-ready=%s CPU through CLI checks(user/system)=%s/%s RSS(start/CLI-checks-done/peak)=%.1f/%.1f/%.1f MiB", run.name, run.data.startup, run.data.ready, m.cpuUser, m.cpuSystem, mebibytes(m.rssStart), mebibytes(m.rssReady), mebibytes(m.rssPeak))
	}
	t.Log("RSS peak is sampled every 100ms. Embedded figures are Go process totals and include the preinitialized runtime and test harness; only ready-minus-start deltas isolate this VM approximately. Legacy startup includes hypervisor process launch; embedded startup begins at VM configuration.")
}

func extractC7200IOS(t *testing.T, archive string) string {
	t.Helper()
	reader, err := zip.OpenReader(archive)
	if err != nil {
		t.Fatalf("open IOS archive: %v", err)
	}
	defer func() {
		if closeErr := reader.Close(); closeErr != nil {
			t.Errorf("close IOS archive: %v", closeErr)
		}
	}()

	var entry *zip.File
	for _, candidate := range reader.File {
		lower := strings.ToLower(candidate.Name)
		if strings.HasSuffix(lower, ".bin") || strings.HasSuffix(lower, ".image") {
			if entry != nil {
				t.Fatal("IOS archive contains multiple IOS images")
			}
			entry = candidate
		}
	}
	if entry == nil || entry.UncompressedSize64 == 0 || entry.UncompressedSize64 > 256<<20 {
		t.Fatal("IOS archive needs one nonempty .bin or .image file of at most 256 MiB")
	}

	source, err := entry.Open()
	if err != nil {
		t.Fatalf("open IOS image in archive: %v", err)
	}
	defer func() {
		if closeErr := source.Close(); closeErr != nil {
			t.Errorf("close IOS image in archive: %v", closeErr)
		}
	}()
	path := filepath.Join(t.TempDir(), "c7200-ios.bin")
	target, err := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0o600)
	if err != nil {
		t.Fatalf("create temporary IOS image: %v", err)
	}
	written, copyErr := io.Copy(target, io.LimitReader(source, 256<<20+1))
	closeErr := target.Close()
	if copyErr != nil || closeErr != nil || written != int64(entry.UncompressedSize64) {
		t.Fatalf("extract IOS image: copied %d of %d bytes, copy: %v, close: %v", written, entry.UncompressedSize64, copyErr, closeErr)
	}
	return path
}

func bootEmbeddedC7200(t *testing.T, image string, ramMB uint32, hz uint64) iosRun {
	t.Helper()
	startedAt := time.Now()
	pid := os.Getpid()
	baseline := sampleProcess(t, pid)
	port := reserveConsolePort(t)
	vm, err := testRuntime.CreateVM(dynamips.VMConfig{Name: "ios-embedded", InstanceID: 181, Platform: "c7200"})
	if err != nil {
		t.Fatalf("create embedded C7200: %v", err)
	}
	defer vm.Close()
	started := false
	defer func() {
		if started {
			if stopErr := vm.Stop(); stopErr != nil {
				t.Errorf("stop embedded C7200: %v", stopErr)
			}
		}
	}()
	for _, step := range []struct {
		name string
		run  func() error
	}{
		{"RAM", func() error { return vm.SetRAM(ramMB) }},
		{"IOS", func() error { return vm.SetIOS(image) }},
		{"NPE", func() error { return vm.SetC7200NPE("npe-400") }},
		{"midplane", func() error { return vm.SetC7200Midplane("vxr") }},
		{"card", func() error { return vm.AddCard(1, "PA-FE-TX") }},
		{"console", func() error { return vm.SetConsoleTCPPort(port) }},
	} {
		if err := step.run(); err != nil {
			t.Fatalf("configure embedded C7200 %s: %v", step.name, err)
		}
	}
	if err := vm.Start(); err != nil {
		t.Fatalf("start embedded C7200: %v", err)
	}
	started = true
	startup := time.Since(startedAt)
	status, err := vm.Status()
	if err != nil || status != dynamips.VMRunning {
		t.Fatalf("embedded C7200 status after start: %d, %v", status, err)
	}
	_, stopSampling := watchRSS(pid, baseline.rssBytes)
	defer stopSampling()
	outputs, readyAt := exerciseIOSConsole(t, "embedded", port)
	ready := readyAt.Sub(startedAt)
	final := sampleProcess(t, pid)
	return iosRun{startup: startup, ready: ready, metrics: calculateMetrics(baseline, final, stopSampling(), hz), outputs: outputs}
}

func bootLegacyC7200(t *testing.T, binary, image string, ramMB uint32, hz uint64) iosRun {
	t.Helper()
	startedAt := time.Now()
	client := startLegacyHypervisor(t, binary)
	pid := client.cmd.Process.Pid
	baseline := processSample{rssBytes: sampleProcess(t, pid).rssBytes}
	_, stopSampling := watchRSS(pid, 0)
	defer stopSampling()
	name := "ios-legacy"
	port := reserveConsolePort(t)
	legacyOK(t, client, "vm create "+name+" 182 c7200")
	legacyOK(t, client, "vm set_ram "+name+" "+strconv.FormatUint(uint64(ramMB), 10))
	legacyOK(t, client, "vm set_ios "+name+" "+image)
	legacyOK(t, client, "c7200 set_npe "+name+" npe-400")
	legacyOK(t, client, "c7200 set_midplane "+name+" vxr")
	legacyOK(t, client, "vm slot_add_binding "+name+" 1 0 PA-FE-TX")
	legacyOK(t, client, "vm set_con_tcp_port "+name+" "+strconv.Itoa(int(port)))
	legacyOK(t, client, "vm start "+name)
	defer legacyOK(t, client, "vm stop "+name)
	startup := time.Since(startedAt)
	status := legacyOK(t, client, "vm get_status "+name)
	if status != strconv.Itoa(int(dynamips.VMRunning)) {
		t.Fatalf("legacy C7200 status after start: %s", status)
	}
	outputs, readyAt := exerciseIOSConsole(t, "legacy", port)
	ready := readyAt.Sub(startedAt)
	final := sampleProcess(t, pid)
	return iosRun{startup: startup, ready: ready, metrics: calculateMetrics(baseline, final, stopSampling(), hz), outputs: outputs}
}

func reserveConsolePort(t *testing.T) uint16 {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve console port: %v", err)
	}
	port := uint16(listener.Addr().(*net.TCPAddr).Port)
	if err := listener.Close(); err != nil {
		t.Fatalf("release console port: %v", err)
	}
	return port
}

func exerciseIOSConsole(t *testing.T, kind string, port uint16) (map[string]string, time.Time) {
	t.Helper()
	address := net.JoinHostPort("127.0.0.1", strconv.Itoa(int(port)))
	deadline := time.Now().Add(180 * time.Second)
	var conn net.Conn
	var err error
	for time.Now().Before(deadline) {
		conn, err = net.DialTimeout("tcp", address, 250*time.Millisecond)
		if err == nil {
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
	if conn == nil {
		t.Fatalf("connect %s IOS console: %v", kind, err)
	}
	defer func() {
		if closeErr := conn.Close(); closeErr != nil {
			t.Errorf("close %s IOS console: %v", kind, closeErr)
		}
	}()

	prompt, boot := readIOSPrompt(t, conn, kind, deadline, true, "")
	if !strings.Contains(boot, "Cisco IOS Software") {
		t.Fatalf("%s reached IOS prompt without IOS boot banner; output: %.1000s", kind, boot)
	}
	if strings.HasSuffix(prompt, ">") {
		writeIOS(t, conn, kind, "enable")
		prompt, _ = readIOSPrompt(t, conn, kind, time.Now().Add(30*time.Second), false, "enable")
		if !strings.HasSuffix(prompt, "#") {
			t.Fatalf("%s IOS enable did not reach privileged prompt: %q", kind, prompt)
		}
	}
	readyAt := time.Now()
	outputs := make(map[string]string)
	for _, command := range []string{"terminal length 0", "show version", "show ip interface brief", "configure terminal", "hostname paritytest", "end"} {
		writeIOS(t, conn, kind, command)
		prompt, output := readIOSPrompt(t, conn, kind, time.Now().Add(30*time.Second), false, command)
		if strings.Contains(output, "% Invalid input") || strings.Contains(output, "% Incomplete command") || strings.Contains(output, "% Error") {
			t.Fatalf("%s IOS command %q failed: %.1000s", kind, command, output)
		}
		if command == "end" && prompt != "paritytest#" {
			t.Fatalf("%s IOS hostname command did not take effect: prompt %q", kind, prompt)
		}
		outputs[command] = output
	}
	return outputs, readyAt
}

func writeIOS(t *testing.T, conn net.Conn, kind, command string) {
	t.Helper()
	if err := conn.SetWriteDeadline(time.Now().Add(3 * time.Second)); err != nil {
		t.Fatalf("set %s IOS write deadline: %v", kind, err)
	}
	if _, err := io.WriteString(conn, command+"\r"); err != nil {
		t.Fatalf("write %s IOS command %q: %v", kind, command, err)
	}
}

func readIOSPrompt(t *testing.T, conn net.Conn, kind string, deadline time.Time, boot bool, command string) (string, string) {
	t.Helper()
	var output bytes.Buffer
	chunk := make([]byte, 4096)
	lastNudge := time.Time{}
	answeredSetup := false
	answeredAutoinstall := false
	for time.Now().Before(deadline) {
		if err := conn.SetReadDeadline(time.Now().Add(time.Second)); err != nil {
			t.Fatalf("set %s console deadline: %v", kind, err)
		}
		count, readErr := conn.Read(chunk)
		output.Write(chunk[:count])
		text := output.String()
		if output.Len() > 2<<20 {
			t.Fatalf("%s console exceeded 2 MiB; output tail: %.1000s", kind, text[len(text)-1000:])
		}
		if match := iosPrompt.FindStringSubmatch(text); match != nil && (command == "" || strings.Contains(text, command)) {
			return match[1] + match[2], text
		}
		if boot {
			lower := strings.ToLower(text)
			if !answeredSetup && strings.Contains(lower, "initial configuration dialog") && strings.Contains(lower, "[yes/no]") {
				writeIOS(t, conn, kind, "no")
				answeredSetup = true
			}
			if !answeredAutoinstall && strings.Contains(lower, "terminate autoinstall") {
				writeIOS(t, conn, kind, "yes")
				answeredAutoinstall = true
			}
			if strings.Contains(lower, "press return to get started") && time.Since(lastNudge) > 3*time.Second {
				writeIOS(t, conn, kind, "")
				lastNudge = time.Now()
			}
			if strings.Contains(text, "Cisco IOS Software") && time.Since(lastNudge) > 5*time.Second {
				writeIOS(t, conn, kind, "")
				lastNudge = time.Now()
			}
		}
		var timeout net.Error
		if readErr != nil && !errors.As(readErr, &timeout) {
			t.Fatalf("read %s IOS console: %v; output tail: %.1000s", kind, readErr, text)
		}
	}
	text := output.String()
	if len(text) > 1000 {
		text = text[len(text)-1000:]
	}
	t.Fatalf("%s IOS prompt not ready before deadline; output tail: %q", kind, text)
	return "", ""
}

func clockTicksPerSecond(t *testing.T) uint64 {
	t.Helper()
	output, err := exec.Command("getconf", "CLK_TCK").Output()
	if err != nil {
		t.Fatalf("get Linux clock ticks per second: %v", err)
	}
	hz, err := strconv.ParseUint(strings.TrimSpace(string(output)), 10, 64)
	if err != nil || hz == 0 {
		t.Fatalf("parse clock ticks per second %q: %v", output, err)
	}
	return hz
}

func sampleProcess(t *testing.T, pid int) processSample {
	t.Helper()
	sample, err := readProcess(pid)
	if err != nil {
		t.Fatalf("read process %d resource usage: %v", pid, err)
	}
	return sample
}

func readProcess(pid int) (processSample, error) {
	data, err := os.ReadFile(filepath.Join("/proc", strconv.Itoa(pid), "stat"))
	if err != nil {
		return processSample{}, err
	}
	end := bytes.LastIndexByte(data, ')')
	if end < 0 {
		return processSample{}, fmt.Errorf("malformed /proc/%d/stat", pid)
	}
	fields := bytes.Fields(data[end+1:])
	if len(fields) < 22 {
		return processSample{}, fmt.Errorf("short /proc/%d/stat", pid)
	}
	user, err := strconv.ParseUint(string(fields[11]), 10, 64)
	if err != nil {
		return processSample{}, err
	}
	system, err := strconv.ParseUint(string(fields[12]), 10, 64)
	if err != nil {
		return processSample{}, err
	}
	rssPages, err := strconv.ParseUint(string(fields[21]), 10, 64)
	if err != nil {
		return processSample{}, err
	}
	return processSample{userTicks: user, systemTicks: system, rssBytes: rssPages * uint64(os.Getpagesize())}, nil
}

func watchRSS(pid int, initial uint64) (*uint64, func() uint64) {
	peak := &initial
	stop := make(chan struct{})
	done := make(chan struct{})
	go func() {
		defer close(done)
		ticker := time.NewTicker(100 * time.Millisecond)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				if sample, err := readProcess(pid); err == nil && sample.rssBytes > *peak {
					*peak = sample.rssBytes
				}
			case <-stop:
				return
			}
		}
	}()
	var stopped bool
	return peak, func() uint64 {
		if !stopped {
			close(stop)
			<-done
			stopped = true
		}
		return *peak
	}
}

func calculateMetrics(start, end processSample, peak, hz uint64) processMetrics {
	if end.rssBytes > peak {
		peak = end.rssBytes
	}
	return processMetrics{
		cpuUser:   time.Duration((end.userTicks - start.userTicks) * uint64(time.Second) / hz),
		cpuSystem: time.Duration((end.systemTicks - start.systemTicks) * uint64(time.Second) / hz),
		rssStart:  start.rssBytes,
		rssReady:  end.rssBytes,
		rssPeak:   peak,
	}
}

func mebibytes(bytes uint64) float64 {
	return float64(bytes) / (1 << 20)
}
