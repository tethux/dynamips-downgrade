package dynamips_test

import (
	"fmt"
	"runtime"
	"slices"
	"strings"
	"testing"
)

type iosPair struct{ embedded, legacy iosRun }

type goMemory struct {
	heap, heapInUse, stacks, backed, reserved uint64
	allocated, mallocs, pause                 uint64
	collections                               uint32
	goroutines                                int
}

func sampleGoMemory() goMemory {
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	return goMemory{
		heap: stats.HeapAlloc, heapInUse: stats.HeapInuse, stacks: stats.StackInuse,
		backed: stats.Sys - stats.HeapReleased, reserved: stats.Sys,
		allocated: stats.TotalAlloc, mallocs: stats.Mallocs, pause: stats.PauseTotalNs,
		collections: stats.NumGC, goroutines: runtime.NumGoroutine(),
	}
}

type iosReportRow struct {
	name, unit string
	value      func(iosRun) float64
}

func iosReportSections() []struct {
	title  string
	goOnly bool
	rows   []iosReportRow
} {
	return []struct {
		title  string
		goOnly bool
		rows   []iosReportRow
	}{
		{"TIME & CPU", false, []iosReportRow{
			{"VM startup", "s", func(r iosRun) float64 { return r.startup.Seconds() }},
			{"IOS prompt ready", "s", func(r iosRun) float64 { return r.ready.Seconds() }},
			{"CLI checks", "ms", func(r iosRun) float64 { return float64(r.cli.Microseconds()) / 1000 }},
			{"CPU total through CLI", "s", func(r iosRun) float64 { return (r.metrics.cpuUser + r.metrics.cpuSystem).Seconds() }},
			{"CPU user", "s", func(r iosRun) float64 { return r.metrics.cpuUser.Seconds() }},
			{"CPU system", "s", func(r iosRun) float64 { return r.metrics.cpuSystem.Seconds() }},
			{"CLI output", "KiB", func(r iosRun) float64 {
				var total int
				for _, output := range r.outputs {
					total += len(output)
				}
				return float64(total) / 1024
			}},
		}},
		{"PROCESS MEMORY", false, []iosReportRow{
			{"RSS before VM", "MiB", func(r iosRun) float64 { return mebibytes(r.metrics.rssStart) }},
			{"RSS after CLI", "MiB", func(r iosRun) float64 { return mebibytes(r.metrics.rssReady) }},
			{"RSS sampled peak", "MiB", func(r iosRun) float64 { return mebibytes(r.metrics.rssPeak) }},
			{"RSS growth", "MiB", func(r iosRun) float64 { return mebibytes(r.metrics.rssReady) - mebibytes(r.metrics.rssStart) }},
		}},
		{"GO MEMORY · embedded host, including test/console client", true, []iosReportRow{
			{"Heap objects before VM", "MiB", func(r iosRun) float64 { return mebibytes(r.goStart.heap) }},
			{"Heap objects after CLI", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.heap) }},
			{"Heap spans in use", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.heapInUse) }},
			{"Go stacks in use", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.stacks) }},
			{"Runtime backed estimate", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.backed) }},
			{"Runtime reserved", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.reserved) }},
			{"Allocated during run", "MiB", func(r iosRun) float64 { return mebibytes(r.goEnd.allocated - r.goStart.allocated) }},
			{"Allocations during run", "count", func(r iosRun) float64 { return float64(r.goEnd.mallocs - r.goStart.mallocs) }},
			{"GC cycles during run", "count", func(r iosRun) float64 { return float64(r.goEnd.collections - r.goStart.collections) }},
			{"GC pause during run", "ms", func(r iosRun) float64 { return float64(r.goEnd.pause-r.goStart.pause) / 1e6 }},
			{"Goroutines after CLI", "count", func(r iosRun) float64 { return float64(r.goEnd.goroutines) }},
		}},
	}
}

func iosDistribution(values []float64, unit string) (float64, string) {
	ordered := slices.Clone(values)
	slices.Sort(ordered)
	if len(ordered) == 0 {
		return 0, "n/a"
	}
	middle := len(ordered) / 2
	median := ordered[middle]
	if len(ordered)%2 == 0 {
		median = (ordered[middle-1] + median) / 2
	}
	format := "%.3f [%.3f–%.3f]"
	if unit == "count" {
		format = "%g [%g–%g]"
	}
	return median, fmt.Sprintf(format, median, ordered[0], ordered[len(ordered)-1])
}

func formatIOSReport(pairs []iosPair, ramMB uint32, detailed bool) string {
	var out strings.Builder

	if !detailed {
		fmt.Fprintf(&out, "C7200 IOS · %d pairs · %d MiB RAM · medians\n", len(pairs), ramMB)
	} else {
		fmt.Fprintf(&out, "C7200 IOS  /  API comparison\n%d paired trials · RAM %d MiB · %s %s/%s · GOMAXPROCS %d\n", len(pairs), ramMB, runtime.Version(), runtime.GOOS, runtime.GOARCH, runtime.GOMAXPROCS(0))
		out.WriteString("Values: median [min–max]. Δ compares medians: (embedded − legacy) / legacy.\n")
	}
	width := 10
	if detailed {
		width = 29
	}
	for _, section := range iosReportSections() {
		fmt.Fprintf(&out, "\n%s\n%s\n", section.title, strings.Repeat("─", 47+2*width))
		if section.goOnly {
			fmt.Fprintf(&out, "%-27s %-5s %*s\n", "Metric", "Unit", width, "Go host")
		} else {
			fmt.Fprintf(&out, "%-27s %-5s %*s %*s %9s\n", "Metric", "Unit", width, "Embedded", width, "Legacy", "Δ")
		}
		for _, row := range section.rows {
			if !detailed && !slices.Contains([]string{"VM startup", "IOS prompt ready", "CLI checks", "CPU total through CLI", "RSS after CLI", "RSS sampled peak", "Heap objects after CLI", "Runtime backed estimate", "Allocated during run", "GC pause during run"}, row.name) {
				continue
			}
			embedded, legacy := make([]float64, 0, len(pairs)), make([]float64, 0, len(pairs))
			for _, pair := range pairs {
				embedded = append(embedded, row.value(pair.embedded))
				if !section.goOnly {
					legacy = append(legacy, row.value(pair.legacy))
				}
			}
			em, et := iosDistribution(embedded, row.unit)
			if !detailed {
				et = fmt.Sprintf("%.2f", em)
			}
			if section.goOnly {
				fmt.Fprintf(&out, "%-27s %-5s %*s\n", row.name, row.unit, width, et)
				continue
			}
			lm, lt := iosDistribution(legacy, row.unit)
			if !detailed {
				lt = fmt.Sprintf("%.2f", lm)
			}
			delta := "—"
			if lm != 0 {
				delta = fmt.Sprintf("%+.1f%%", 100*(em-lm)/lm)
			}
			fmt.Fprintf(&out, "%-27s %-5s %*s %*s %9s\n", row.name, row.unit, width, et, width, lt, delta)
		}
	}
	if !detailed {
		out.WriteString("\nΔ: negative = less time/memory. RSS includes native + Go; Go backing is an estimate.\n")
		out.WriteString("Go includes the test/console client; shared-process caches may persist.\n")
		out.WriteString("DYNAMIPS_IOS_DETAILS=1 shows ranges, all metrics and measurement notes.\n")
		return out.String()
	}
	out.WriteString("\nReading the results\n")
	out.WriteString("  Lower time/memory is better; CLI output volume is informational.\n")
	out.WriteString("  Same native core; alternating sequential order, no warm-up exclusion.\n")
	out.WriteString("  Startup: embedded begins at VM configuration; legacy includes process launch.\n")
	out.WriteString("  CPU/RSS include CLI checks; RSS sampled every 100 ms, so brief peaks may be missed.\n")
	out.WriteString("  Go heap objects include uncollected objects. No forced GC is performed.\n")
	out.WriteString("  Runtime backed estimate = Sys − HeapReleased; it is not measured resident memory.\n")
	out.WriteString("  RSS includes native allocations, Go and mappings. Subtracting Go stats cannot isolate native RSS.\n")
	out.WriteString("  Repeated embedded runs share a Go process; allocator/runtime caches may persist.\n")
	return out.String()
}

func TestIOSReport(t *testing.T) {
	if _, empty := iosDistribution(nil, "s"); empty != "n/a" {
		t.Fatalf("empty distribution: %q", empty)
	}
	median, formatted := iosDistribution([]float64{3, 1, 2, 4}, "s")
	if median != 2.5 || formatted != "2.500 [1.000–4.000]" {
		t.Fatalf("distribution: %v %q", median, formatted)
	}
	if _, count := iosDistribution([]float64{3, 2}, "count"); count != "2.5 [2–3]" {
		t.Fatalf("count formatting lost fractional median: %q", count)
	}
	report := formatIOSReport([]iosPair{{embedded: iosRun{goStart: goMemory{allocated: 1 << 20}, goEnd: goMemory{heap: 2 << 20, allocated: 4 << 20}}}}, 256, true)
	for _, marker := range []string{"1 paired trials", "GO MEMORY", "2.000 [2.000–2.000]", "3.000 [3.000–3.000]", "Go host", "Sys − HeapReleased"} {
		if !strings.Contains(report, marker) {
			t.Errorf("report missing %q", marker)
		}
	}
	compact := formatIOSReport([]iosPair{{}}, 256, false)
	if strings.Contains(compact, "Heap spans") || strings.Contains(compact, "[0.000") || !strings.Contains(compact, "DYNAMIPS_IOS_DETAILS=1") {
		t.Fatalf("unexpected compact report: %s", compact)
	}
	if strings.Contains(report, "NaN") || strings.Contains(report, "Inf") {
		t.Fatal("zero legacy baseline produced invalid delta")
	}
}

func TestIOSGoMemory(t *testing.T) {
	before := sampleGoMemory()
	payload := make([]byte, 2<<20)
	for i := range payload {
		payload[i] = byte(i)
	}
	after := sampleGoMemory()
	runtime.KeepAlive(payload)
	if after.allocated-before.allocated < uint64(len(payload)) {
		t.Fatalf("Go allocation counter missed a real %d-byte allocation: before=%d after=%d", len(payload), before.allocated, after.allocated)
	}
	if after.heapInUse < after.heap || after.reserved < after.backed || after.goroutines < 1 {
		t.Fatalf("inconsistent Go memory snapshot: %+v", after)
	}
}
