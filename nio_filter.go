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
	status := C.dyn_nio_setup_filter(nio.handle, C.dyn_filter_direction(direction),
		C.size_t(len(cOptions)), optionPtr)
	if status != C.DYN_OK {
		return nativeError("setup NIO filter", status)
	}
	return nil
}
