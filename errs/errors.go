package errs

import "errors"

var (
	// ErrInvalidArgument reports an invalid value at the native boundary.
	ErrInvalidArgument = errors.New("invalid argument")
	// ErrNotInitialized reports use before runtime initialization.
	ErrNotInitialized = errors.New("runtime not initialized")
	// ErrAlreadyInitialized reports a second process-global runtime.
	ErrAlreadyInitialized = errors.New("runtime already initialized")
	// ErrOutOfMemory reports a native allocation failure.
	ErrOutOfMemory = errors.New("out of memory")
	// ErrCreateFailed reports failure to create a native object.
	ErrCreateFailed = errors.New("create failed")
	// ErrStartFailed reports failure to start a VM.
	ErrStartFailed = errors.New("start failed")
	// ErrStopFailed reports failure to stop a VM.
	ErrStopFailed = errors.New("stop failed")
	// ErrBindingFailed reports failure to bind one object to another.
	ErrBindingFailed = errors.New("binding failed")
	// ErrUnsupported reports an operation unavailable on the VM platform.
	ErrUnsupported = errors.New("unsupported")
	// ErrIO reports failure to read or write native data.
	ErrIO = errors.New("I/O error")
	// ErrInvalidState reports an operation unavailable in the object's current state.
	ErrInvalidState = errors.New("invalid state")
	// ErrInUse reports a runtime with live VM or NIO references.
	ErrInUse = errors.New("runtime still has open objects")
	// ErrInternal reports an unexpected native failure.
	ErrInternal = errors.New("internal error")
	// ErrClosed reports use of a closed runtime or handle.
	ErrClosed = errors.New("closed")
)

// Operation adds operation context while preserving category and cause.
type Operation struct {
	Op    string
	Kind  error
	Cause error
}

// Error returns a stable human-readable diagnostic.
func (e *Operation) Error() string {
	message := "dynamips: " + e.Op + ": " + e.Kind.Error()
	if e.Cause != nil {
		message += ": " + e.Cause.Error()
	}
	return message
}

// Unwrap exposes both the category and underlying cause to errors.Is and errors.As.
func (e *Operation) Unwrap() []error {
	if e.Cause == nil {
		return []error{e.Kind}
	}

	return []error{e.Kind, e.Cause}
}
