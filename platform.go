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

// SetC7200NPE selects the processor engine before the router starts.
func (vm *VM) SetC7200NPE(npe string) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("set C7200 NPE", errs.ErrClosed, nil)
	}
	if strings.IndexByte(npe, 0) >= 0 {
		return operationError("set C7200 NPE", errs.ErrInvalidArgument, nil)
	}
	value := C.CString(npe)
	defer C.free(unsafe.Pointer(value))
	if status := C.dyn_c7200_set_npe(vm.handle, value); status != C.DYN_OK {
		return nativeError("set C7200 NPE", status)
	}
	return nil
}

// SetC7200Midplane selects the chassis midplane before the router starts.
func (vm *VM) SetC7200Midplane(midplane string) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("set C7200 midplane", errs.ErrClosed, nil)
	}
	if strings.IndexByte(midplane, 0) >= 0 {
		return operationError("set C7200 midplane", errs.ErrInvalidArgument, nil)
	}
	value := C.CString(midplane)
	defer C.free(unsafe.Pointer(value))
	if status := C.dyn_c7200_set_midplane(vm.handle, value); status != C.DYN_OK {
		return nativeError("set C7200 midplane", status)
	}
	return nil
}

// SetC7200MACAddr sets the chassis base MAC address.
func (vm *VM) SetC7200MACAddr(mac [6]byte) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if vm == nil || vm.handle == nil {
		return operationError("set C7200 MAC address", errs.ErrClosed, nil)
	}
	if status := C.dyn_c7200_set_mac_addr(vm.handle, (*C.uint8_t)(unsafe.Pointer(&mac[0]))); status != C.DYN_OK {
		return nativeError("set C7200 MAC address", status)
	}
	return nil
}
