package dynamips_test

import (
	"errors"
	"fmt"
	"os"
	"testing"

	dynamips "github.com/tethux/dynamips-downgrade/go"
	"github.com/tethux/dynamips-downgrade/go/errs"
)

var testRuntime *dynamips.Runtime

func TestMain(m *testing.M) {
	workingDirectory, err := os.Getwd()
	if err != nil {
		fmt.Fprintln(os.Stderr, "get working directory:", err)
		os.Exit(1)
	}
	testDirectory, makeErr := os.MkdirTemp("", "dynamips-go-test-")
	if makeErr != nil {
		fmt.Fprintln(os.Stderr, "create test directory:", makeErr)
		os.Exit(1)
	}
	if changeErr := os.Chdir(testDirectory); changeErr != nil {
		fmt.Fprintln(os.Stderr, "enter test directory:", changeErr)
		if removeErr := os.RemoveAll(testDirectory); removeErr != nil {
			fmt.Fprintln(os.Stderr, "remove test directory:", removeErr)
		}
		os.Exit(1)
	}
	testRuntime, err = dynamips.New()
	if err != nil {
		fmt.Fprintln(os.Stderr, "initialize runtime:", err)
		if changeErr := os.Chdir(workingDirectory); changeErr != nil {
			fmt.Fprintln(os.Stderr, "restore working directory:", changeErr)
		}
		if removeErr := os.RemoveAll(testDirectory); removeErr != nil {
			fmt.Fprintln(os.Stderr, "remove test directory:", removeErr)
		}
		os.Exit(1)
	}

	status := m.Run()
	if closeErr := testRuntime.Close(); closeErr != nil {
		fmt.Fprintln(os.Stderr, "close runtime:", closeErr)
		status = 1
	}
	if changeErr := os.Chdir(workingDirectory); changeErr != nil {
		fmt.Fprintln(os.Stderr, "restore working directory:", changeErr)
		status = 1
	}
	if removeErr := os.RemoveAll(testDirectory); removeErr != nil {
		fmt.Fprintln(os.Stderr, "remove test directory:", removeErr)
		status = 1
	}
	os.Exit(status)
}

func TestRuntimeCloseWithLiveVM(t *testing.T) {
	runtime := testRuntime

	vm, err := runtime.CreateVM(dynamips.VMConfig{
		Name: "go-bindings-runtime", InstanceID: 42, Platform: "c7200",
	})
	if err != nil {
		t.Fatalf("create VM: %v", err)
	}
	t.Cleanup(vm.Close)
	if closeErr := runtime.Close(); !errors.Is(closeErr, errs.ErrInUse) {
		t.Fatalf("close runtime with live VM: got %v, want ErrInUse", closeErr)
	}
}
