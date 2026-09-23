package dynamips_test

import (
	"fmt"

	dynamips "github.com/tethux/dynamips-downgrade"
)

// Example shows the VM configuration passed to the embedded runtime.
func Example() {
	config := dynamips.VMConfig{Name: "router-1", InstanceID: 1, Platform: "c7200"}
	fmt.Printf("%s %d %s\n", config.Name, config.InstanceID, config.Platform)
	// Output: router-1 1 c7200
}

// ExampleRuntime_CreateVM shows handle ownership. To boot IOS, use the
// runnable example in examples/basic with a local image.
func ExampleRuntime_CreateVM() {
	runtime, err := dynamips.New()
	if err != nil {
		fmt.Println(err)
		return
	}
	vm, err := runtime.CreateVM(dynamips.VMConfig{
		Name: "router-1", InstanceID: 1, Platform: "c7200",
	})
	if err != nil {
		fmt.Println(err)
		if closeErr := runtime.Close(); closeErr != nil {
			fmt.Println(closeErr)
		}
		return
	}
	if err := vm.SetRAM(256); err != nil {
		fmt.Println(err)
	}
	vm.Close()
	if err := runtime.Close(); err != nil {
		fmt.Println(err)
	}
}
