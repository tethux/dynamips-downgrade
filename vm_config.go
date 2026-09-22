package dynamips

/*
#include <dynamips/dynamips.h>
*/
import "C"

import (
	"unsafe"

	"github.com/tethux/dynamips-downgrade/errs"
)

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
