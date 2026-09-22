package dynamips

/*
#cgo CFLAGS: -I${SRCDIR}/include
#cgo linux,amd64 LDFLAGS: -L${SRCDIR}/build/linux/x86_64/debug -ldynamips-bindings -ldynamips-core -lstdc++ -lelf -lpcap -ldl -lrt -lnsl -lpthread

#include <stdlib.h>
#include <dynamips/dynamips.h>

static dyn_result dyn_go_runtime_init(void) {
	char program[] = "dynamips-go";
	char option[] = "-H";
	char endpoint[] = "127.0.0.1:0";
	char *argv[] = {program, option, endpoint, NULL};

	return dyn_runtime_init(3, argv);
}
*/
import "C"

import (
	"sync"
	"unsafe"

	"github.com/tethux/dynamips-downgrade/errs"
)

var runtimeMu sync.Mutex

// Runtime owns the process-global Dynamips runtime.
type Runtime struct {
	active bool
	refs   uint64
}

// VMConfig describes a VM before it is created.
type VMConfig struct {
	Name       string
	InstanceID int32
	Platform   string
}

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

// VM owns one reference to a Dynamips VM.
type VM struct {
	runtime *Runtime
	handle  *C.dyn_vm
}

// VMStatus is the VM's current execution state.
type VMStatus uint32

const (
	VMHalted    VMStatus = C.DYN_VM_HALTED
	VMShutdown  VMStatus = C.DYN_VM_SHUTDOWN
	VMRunning   VMStatus = C.DYN_VM_RUNNING
	VMSuspended VMStatus = C.DYN_VM_SUSPENDED
)

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

// New initializes the process-global Dynamips runtime for embedding.
func New() (*Runtime, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	status := C.dyn_go_runtime_init()
	if status != C.DYN_OK {
		return nil, nativeError("initialize runtime", status)
	}

	return &Runtime{active: true}, nil
}

// Close shuts down the runtime when all VM and NIO handles are closed.
func (r *Runtime) Close() error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if r == nil || !r.active {
		return nil
	}
	if r.refs != 0 {
		return operationError("close runtime", errs.ErrInUse, nil)
	}

	C.dyn_runtime_shutdown()
	r.active = false
	return nil
}

// CreateVM creates a VM and returns an owned reference to it.
func (r *Runtime) CreateVM(config VMConfig) (*VM, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if r == nil || !r.active {
		return nil, operationError("create VM", errs.ErrClosed, nil)
	}

	name := C.CString(config.Name)
	defer C.free(unsafe.Pointer(name))
	platform := C.CString(config.Platform)
	defer C.free(unsafe.Pointer(platform))

	var handle *C.dyn_vm
	status := C.dyn_vm_create(
		name,
		C.int32_t(config.InstanceID),
		platform,
		&handle,
	)
	if status != C.DYN_OK {
		return nil, nativeError("create VM", status)
	}

	r.refs++
	return &VM{runtime: r, handle: handle}, nil
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

// Start starts the VM.
func (vm *VM) Start() error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("start VM", errs.ErrClosed, nil)
	}

	status := C.dyn_vm_start(vm.handle)
	if status != C.DYN_OK {
		return nativeError("start VM", status)
	}
	return nil
}

// Stop stops the VM.
func (vm *VM) Stop() error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("stop VM", errs.ErrClosed, nil)
	}

	status := C.dyn_vm_stop(vm.handle)
	if status != C.DYN_OK {
		return nativeError("stop VM", status)
	}
	return nil
}

// Status returns the VM's current execution state.
func (vm *VM) Status() (VMStatus, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return VMHalted, operationError("get VM status", errs.ErrClosed, nil)
	}

	var value C.dyn_vm_status
	status := C.dyn_vm_get_status(vm.handle, &value)
	if status != C.DYN_OK {
		return VMHalted, nativeError("get VM status", status)
	}
	return VMStatus(value), nil
}

// SetRAM sets the VM's RAM size in megabytes.
func (vm *VM) SetRAM(megabytes uint32) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("set VM RAM", errs.ErrClosed, nil)
	}
	if status := C.dyn_vm_set_ram(vm.handle, C.uint32_t(megabytes)); status != C.DYN_OK {
		return nativeError("set VM RAM", status)
	}
	return nil
}

// Close releases the VM reference.
func (vm *VM) Close() {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return
	}

	C.dyn_vm_release(vm.handle)
	vm.handle = nil
	vm.runtime.refs--
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

func nativeError(operation string, status C.dyn_result) error {
	var kind error
	switch status {
	case C.DYN_ERR_INVALID_ARGUMENT:
		kind = errs.ErrInvalidArgument
	case C.DYN_ERR_NOT_INITIALIZED:
		kind = errs.ErrNotInitialized
	case C.DYN_ERR_ALREADY_INITIALIZED:
		kind = errs.ErrAlreadyInitialized
	case C.DYN_ERR_OUT_OF_MEMORY:
		kind = errs.ErrOutOfMemory
	case C.DYN_ERR_CREATE_FAILED:
		kind = errs.ErrCreateFailed
	case C.DYN_ERR_START_FAILED:
		kind = errs.ErrStartFailed
	case C.DYN_ERR_STOP_FAILED:
		kind = errs.ErrStopFailed
	default:
		kind = errs.ErrInternal
	}

	return operationError(operation, kind, nil)
}

func operationError(operation string, kind, cause error) error {
	return &errs.Operation{
		Op:    operation,
		Kind:  kind,
		Cause: cause,
	}
}
