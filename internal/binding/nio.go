package dynamips

/*
#include <stdlib.h>
#include <dynamips/dynamips.h>
*/
import "C"

import (
	"strings"
	"unsafe"

	"github.com/tethux/dynamips-downgrade/errs"
)

// UDPConfig describes a UDP network I/O endpoint.
type UDPConfig struct {
	Name       string
	LocalPort  uint16
	RemoteHost string
	RemotePort uint16
}

// UDPAutoConfig describes a UDP NIO with a locally chosen port.
type UDPAutoConfig struct {
	Name      string
	LocalAddr string
	PortStart uint16
	PortEnd   uint16
}

// TAPConfig describes a host TAP network I/O endpoint.
type TAPConfig struct {
	Name   string
	Device string
}

// NIO owns one reference to a Dynamips network I/O object.
type NIO struct {
	runtime *Runtime
	handle  *C.dyn_nio
}

// NIOStats contains traffic counters for one NIO.
type NIOStats struct {
	PacketsIn  uint64
	PacketsOut uint64
	BytesIn    uint64
	BytesOut   uint64
}

// CreateUDP creates a UDP NIO and returns an owned reference to it.
func (r *Runtime) CreateUDP(config UDPConfig) (*NIO, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if r == nil || !r.active {
		return nil, operationError("create UDP NIO", errs.ErrClosed, nil)
	}

	name := C.CString(config.Name)
	defer C.free(unsafe.Pointer(name))
	remoteHost := C.CString(config.RemoteHost)
	defer C.free(unsafe.Pointer(remoteHost))

	var handle *C.dyn_nio
	status := C.dyn_nio_create_udp(
		name,
		C.uint16_t(config.LocalPort),
		remoteHost,
		C.uint16_t(config.RemotePort),
		&handle,
	)
	if status != C.DYN_OK {
		return nil, nativeError("create UDP NIO", status)
	}

	r.refs++
	return &NIO{runtime: r, handle: handle}, nil
}

// CreateUDPAuto creates a UDP NIO and returns its bound local port.
func (r *Runtime) CreateUDPAuto(config UDPAutoConfig) (*NIO, uint16, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if r == nil || !r.active {
		return nil, 0, operationError("create UDP auto NIO", errs.ErrClosed, nil)
	}
	name := C.CString(config.Name)
	defer C.free(unsafe.Pointer(name))
	localAddr := C.CString(config.LocalAddr)
	defer C.free(unsafe.Pointer(localAddr))

	var handle *C.dyn_nio
	var localPort C.uint16_t
	status := C.dyn_nio_create_udp_auto(name, localAddr,
		C.uint16_t(config.PortStart), C.uint16_t(config.PortEnd),
		&handle, &localPort)
	if status != C.DYN_OK {
		return nil, 0, nativeError("create UDP auto NIO", status)
	}
	r.refs++
	return &NIO{runtime: r, handle: handle}, uint16(localPort), nil
}

// CreateTAP opens a host TAP device and returns an owned NIO reference.
func (r *Runtime) CreateTAP(config TAPConfig) (*NIO, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()
	if r == nil || !r.active {
		return nil, operationError("create TAP NIO", errs.ErrClosed, nil)
	}
	if strings.IndexByte(config.Name, 0) >= 0 || strings.IndexByte(config.Device, 0) >= 0 {
		return nil, operationError("create TAP NIO", errs.ErrInvalidArgument, nil)
	}
	name := C.CString(config.Name)
	defer C.free(unsafe.Pointer(name))
	device := C.CString(config.Device)
	defer C.free(unsafe.Pointer(device))
	var handle *C.dyn_nio
	status := C.dyn_nio_create_tap(name, device, &handle)
	if status != C.DYN_OK {
		return nil, nativeError("create TAP NIO", status)
	}
	r.refs++
	return &NIO{runtime: r, handle: handle}, nil
}

// ConnectUDPAuto connects an auto UDP NIO to its remote endpoint once.
func (nio *NIO) ConnectUDPAuto(remoteHost string, remotePort uint16) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()
	if nio == nil || nio.handle == nil {
		return operationError("connect UDP auto NIO", errs.ErrClosed, nil)
	}
	if strings.IndexByte(remoteHost, 0) >= 0 {
		return operationError("connect UDP auto NIO", errs.ErrInvalidArgument, nil)
	}
	host := C.CString(remoteHost)
	defer C.free(unsafe.Pointer(host))
	status := C.dyn_nio_connect_udp_auto(nio.handle, host, C.uint16_t(remotePort))
	if status != C.DYN_OK {
		return nativeError("connect UDP auto NIO", status)
	}
	return nil
}

// Delete removes an unused NIO and consumes its handle on success.
func (nio *NIO) Delete() error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()
	if nio == nil || nio.handle == nil {
		return operationError("delete NIO", errs.ErrClosed, nil)
	}
	status := C.dyn_nio_delete(&nio.handle)
	if status != C.DYN_OK {
		return nativeError("delete NIO", status)
	}
	nio.runtime.refs--
	return nil
}

// Stats returns the NIO's traffic counters.
func (nio *NIO) Stats() (NIOStats, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if nio == nil || nio.handle == nil {
		return NIOStats{}, operationError("get NIO stats", errs.ErrClosed, nil)
	}
	var value C.dyn_nio_stats
	if status := C.dyn_nio_get_stats(nio.handle, &value); status != C.DYN_OK {
		return NIOStats{}, nativeError("get NIO stats", status)
	}
	return NIOStats{
		PacketsIn:  uint64(value.packets_in),
		PacketsOut: uint64(value.packets_out),
		BytesIn:    uint64(value.bytes_in),
		BytesOut:   uint64(value.bytes_out),
	}, nil
}

// Close releases the NIO reference.
func (nio *NIO) Close() {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if nio == nil || nio.handle == nil {
		return
	}

	C.dyn_nio_release(nio.handle)
	nio.handle = nil
	nio.runtime.refs--
}

// FilterDirection selects the NIO traffic direction for a filter.
type FilterDirection int32

const (
	FilterRX   FilterDirection = C.DYN_FILTER_RX
	FilterTX   FilterDirection = C.DYN_FILTER_TX
	FilterBoth FilterDirection = C.DYN_FILTER_BOTH
)

// SetupFilter configures a filter already bound to the NIO.
func (nio *NIO) SetupFilter(direction FilterDirection, options ...string) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if nio == nil || nio.handle == nil {
		return operationError("setup NIO filter", errs.ErrClosed, nil)
	}
	cOptions := make([]*C.char, len(options))
	defer func() {
		for _, option := range cOptions {
			C.free(unsafe.Pointer(option))
		}
	}()
	for i, option := range options {
		if strings.IndexByte(option, 0) >= 0 {
			return operationError("setup NIO filter", errs.ErrInvalidArgument, nil)
		}
		cOptions[i] = C.CString(option)
	}
	var optionPtr **C.char
	if len(cOptions) != 0 {
		optionPtr = (**C.char)(unsafe.Pointer(&cOptions[0]))
	}
	status := C.dyn_nio_setup_filter(nio.handle, C.int32_t(direction),
		C.size_t(len(cOptions)), optionPtr)
	if status != C.DYN_OK {
		return nativeError("setup NIO filter", status)
	}
	return nil
}
