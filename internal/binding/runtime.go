package dynamips

/*
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/libdynamips-bindings_linux_amd64.a ${SRCDIR}/libdynamips-core_linux_amd64.a -lstdc++ -lelf -lpcap -ldl -lrt -lnsl -lpthread
#include <stdlib.h>
#include "dynamips.h"
*/
import "C"

import (
	"sync"

	"github.com/tethux/dynamips-downgrade/errs"
)

var runtimeMu sync.Mutex

// Runtime owns the process-global Dynamips runtime.
type Runtime struct {
	active bool
	refs   uint64
}

// New initializes the process-global Dynamips runtime for embedding.
func New() (*Runtime, error) {
	runtimeMu.Lock()
	defer runtimeMu.Unlock()

	status := C.dyn_runtime_init_embedded()
	if status != C.DYN_OK {
		return nil, nativeError("initialize runtime", status)
	}

	return &Runtime{active: true}, nil
}

// Close shuts down the runtime when all owned handles are closed.
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
