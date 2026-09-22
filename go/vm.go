package dynamips

/*
#include <stdlib.h>
#include <dynamips/dynamips.h>
*/
import "C"

import (
	"unsafe"

	"github.com/tethux/dynamips-downgrade/go/errs"
)

// VMConfig describes a VM before it is created.
type VMConfig struct {
	Name       string
	InstanceID int32
	Platform   string
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

// AttachNIO binds a NIO to a VM slot and port.
func (vm *VM) AttachNIO(slot, port uint32, nio *NIO) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil || nio == nil || nio.handle == nil {
		return operationError("attach NIO to VM", errs.ErrClosed, nil)
	}
	if status := C.dyn_vm_attach_nio(vm.handle, C.uint32_t(slot), C.uint32_t(port), nio.handle); status != C.DYN_OK {
		return nativeError("attach NIO to VM", status)
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

// ConfigData contains native startup and private configuration bytes.
type ConfigData struct {
	Startup []byte
	Private []byte
}

// ExtractConfig copies the VM's NVRAM configuration into Go-owned memory.
func (vm *VM) ExtractConfig() (ConfigData, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return ConfigData{}, operationError("extract VM config", errs.ErrClosed, nil)
	}
	var startup, private C.dyn_bytes
	if status := C.dyn_vm_extract_config(vm.handle, &startup, &private); status != C.DYN_OK {
		return ConfigData{}, nativeError("extract VM config", status)
	}
	defer C.dyn_bytes_release(&startup)
	defer C.dyn_bytes_release(&private)

	startupData, startupErr := copyNativeBytes(startup)
	if startupErr != nil {
		return ConfigData{}, startupErr
	}
	privateData, privateErr := copyNativeBytes(private)
	if privateErr != nil {
		return ConfigData{}, privateErr
	}
	return ConfigData{Startup: startupData, Private: privateData}, nil
}

func copyNativeBytes(value C.dyn_bytes) ([]byte, error) {
	if value.size == 0 {
		return nil, nil
	}
	if value.data == nil {
		return nil, operationError("copy native bytes", errs.ErrInternal, nil)
	}
	if uint64(value.size) > uint64(^uint(0)>>1) {
		return nil, operationError("copy native bytes", errs.ErrOutOfMemory, nil)
	}
	source := unsafe.Slice((*byte)(unsafe.Pointer(value.data)), int(value.size))
	return append([]byte(nil), source...), nil
}
