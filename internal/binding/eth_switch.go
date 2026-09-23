package dynamips

/*
#include <stdlib.h>
#include "dynamips.h"
*/
import "C"

import (
	"unsafe"

	"github.com/tethux/dynamips-downgrade/errs"
)

// EthernetSwitch owns one reference to a Dynamips Ethernet switch.
type EthernetSwitch struct {
	runtime *Runtime
	handle  *C.dyn_eth_switch
}

// CreateEthernetSwitch creates a switch and returns an owned reference.
func (r *Runtime) CreateEthernetSwitch(name string) (*EthernetSwitch, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if r == nil || !r.active {
		return nil, operationError("create Ethernet switch", errs.ErrClosed, nil)
	}
	ownedName := C.CString(name)
	defer C.free(unsafe.Pointer(ownedName))
	var handle *C.dyn_eth_switch
	if status := C.dyn_eth_switch_create(ownedName, &handle); status != C.DYN_OK {
		return nil, nativeError("create Ethernet switch", status)
	}
	r.refs++
	return &EthernetSwitch{runtime: r, handle: handle}, nil
}

// AddNIO binds a NIO to the switch.
func (sw *EthernetSwitch) AddNIO(nio *NIO) error {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if sw == nil || sw.handle == nil || nio == nil || nio.handle == nil {
		return operationError("add NIO to Ethernet switch", errs.ErrClosed, nil)
	}
	if status := C.dyn_eth_switch_add_nio(sw.handle, nio.handle); status != C.DYN_OK {
		return nativeError("add NIO to Ethernet switch", status)
	}
	return nil
}

// Close releases the switch reference.
func (sw *EthernetSwitch) Close() {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	if sw == nil || sw.handle == nil {
		return
	}
	C.dyn_eth_switch_release(sw.handle)
	sw.handle = nil
	sw.runtime.refs--
}
