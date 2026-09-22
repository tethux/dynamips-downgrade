package dynamips

/*
#include <dynamips/dynamips.h>
*/
import "C"

import "github.com/tethux/dynamips-downgrade/go/errs"

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
	case C.DYN_ERR_BINDING_FAILED:
		kind = errs.ErrBindingFailed
	case C.DYN_ERR_UNSUPPORTED:
		kind = errs.ErrUnsupported
	case C.DYN_ERR_IO:
		kind = errs.ErrIO
	case C.DYN_ERR_INVALID_STATE:
		kind = errs.ErrInvalidState
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
