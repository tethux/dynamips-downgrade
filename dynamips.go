// Package dynamips provides typed Go bindings for the embedded Dynamips
// runtime. The runtime is process wide and can be initialized once. Calls are
// serialized around legacy global state. Close every VM, NIO, and switch
// handle before closing the runtime.
//
// The Go package requires the installed dynamips-bindings native package.
// `mise run build:native` installs a local copy for development. See the
// [C7200 boot example] for a runnable router.
//
// [C7200 boot example]: https://github.com/tethux/dynamips-downgrade/tree/master/examples/basic
package dynamips

import binding "github.com/tethux/dynamips-downgrade/internal/binding"

// Runtime owns the process-global Dynamips runtime.
type Runtime struct{ handle *binding.Runtime }

// VM owns one reference to a Dynamips VM.
type VM struct{ handle *binding.VM }

// VMConfig describes a VM before it is created.
type VMConfig struct {
	Name       string
	InstanceID int32
	Platform   string
}

// VMStatus is the VM's current execution state.
type VMStatus uint32

// ConfigData contains native startup and private configuration bytes.
type ConfigData struct {
	Startup []byte
	Private []byte
}

// NIO owns one reference to a Dynamips network I/O object.
type NIO struct{ handle *binding.NIO }

// UDPConfig describes a UDP network I/O endpoint.
type UDPConfig struct {
	Name       string
	LocalPort  uint16
	RemoteHost string
	RemotePort uint16
}

// UDPAutoConfig describes a UDP NIO with a locally chosen port.
type UDPAutoConfig struct {
	Name      string
	LocalAddr string
	PortStart uint16
	PortEnd   uint16
}

// TAPConfig describes a host TAP network I/O endpoint.
type TAPConfig struct {
	Name   string
	Device string
}

// NIOStats contains traffic counters for one NIO.
type NIOStats struct {
	PacketsIn  uint64
	PacketsOut uint64
	BytesIn    uint64
	BytesOut   uint64
}

// FilterDirection selects the NIO traffic direction for a filter.
type FilterDirection int32

// EthernetSwitch owns one reference to a Dynamips Ethernet switch.
type EthernetSwitch struct{ handle *binding.EthernetSwitch }

const (
	VMHalted    VMStatus        = VMStatus(binding.VMHalted)
	VMShutdown  VMStatus        = VMStatus(binding.VMShutdown)
	VMRunning   VMStatus        = VMStatus(binding.VMRunning)
	VMSuspended VMStatus        = VMStatus(binding.VMSuspended)
	FilterRX    FilterDirection = FilterDirection(binding.FilterRX)
	FilterTX    FilterDirection = FilterDirection(binding.FilterTX)
	FilterBoth  FilterDirection = FilterDirection(binding.FilterBoth)
)

// New initializes the process-global Dynamips runtime for embedding.
func New() (*Runtime, error) {
	handle, err := binding.New()
	if err != nil {
		return nil, err
	}
	return &Runtime{handle: handle}, nil
}

func (r *Runtime) native() *binding.Runtime {
	if r == nil {
		return nil
	}
	return r.handle
}

func (vm *VM) native() *binding.VM {
	if vm == nil {
		return nil
	}
	return vm.handle
}

func (nio *NIO) native() *binding.NIO {
	if nio == nil {
		return nil
	}
	return nio.handle
}

func (sw *EthernetSwitch) native() *binding.EthernetSwitch {
	if sw == nil {
		return nil
	}
	return sw.handle
}

// Close shuts down the runtime when all owned handles are closed.
func (r *Runtime) Close() error {
	return r.native().Close()
}

// CreateVM creates a VM and returns an owned reference to it.
func (r *Runtime) CreateVM(config VMConfig) (*VM, error) {
	handle, err := r.native().CreateVM(binding.VMConfig(config))
	if err != nil {
		return nil, err
	}
	return &VM{handle: handle}, nil
}

// Start starts the VM.
func (vm *VM) Start() error {
	return vm.native().Start()
}

// Stop stops the VM.
func (vm *VM) Stop() error {
	return vm.native().Stop()
}

// Delete removes the native VM and consumes this handle on success.
func (vm *VM) Delete() error {
	return vm.native().Delete()
}

// Suspend pauses a running VM.
func (vm *VM) Suspend() error {
	return vm.native().Suspend()
}

// Resume resumes a suspended VM.
func (vm *VM) Resume() error {
	return vm.native().Resume()
}

// SetIOS sets the IOS image path used when the VM starts.
func (vm *VM) SetIOS(path string) error {
	return vm.native().SetIOS(path)
}

// SetNVRAM sets NVRAM size in kilobytes.
func (vm *VM) SetNVRAM(kilobytes uint32) error {
	return vm.native().SetNVRAM(kilobytes)
}

// SetSparseMemory controls sparse RAM allocation.
func (vm *VM) SetSparseMemory(enabled bool) error {
	return vm.native().SetSparseMemory(enabled)
}

// SetConfigRegister sets the startup configuration register.
func (vm *VM) SetConfigRegister(value uint32) error {
	return vm.native().SetConfigRegister(value)
}

// SetIdlePC sets the idle program counter.
func (vm *VM) SetIdlePC(value uint64) error {
	return vm.native().SetIdlePC(value)
}

// SetConsoleTCPPort selects a TCP console endpoint.
func (vm *VM) SetConsoleTCPPort(port uint16) error {
	return vm.native().SetConsoleTCPPort(port)
}

// PushConfig writes the supplied configs to the native NVRAM device. A nil slice keeps that config.
func (vm *VM) PushConfig(startup, private []byte) error {
	return vm.native().PushConfig(startup, private)
}

// Status returns the VM's current execution state.
func (vm *VM) Status() (VMStatus, error) {
	status, err := vm.native().Status()
	return VMStatus(status), err
}

// SetRAM sets the VM's RAM size in megabytes.
func (vm *VM) SetRAM(megabytes uint32) error {
	return vm.native().SetRAM(megabytes)
}

// AddCard installs a card in a VM slot.
func (vm *VM) AddCard(slot uint32, card string) error {
	return vm.native().AddCard(slot, card)
}

// RemoveCard removes a card and its NIO bindings from a VM slot.
func (vm *VM) RemoveCard(slot uint32) error {
	return vm.native().RemoveCard(slot)
}

// AttachNIO binds a NIO to a VM slot and port.
func (vm *VM) AttachNIO(slot, port uint32, nio *NIO) error {
	return vm.native().AttachNIO(slot, port, nio.native())
}

// DetachNIO removes a NIO binding from a VM slot and port.
func (vm *VM) DetachNIO(slot, port uint32) error {
	return vm.native().DetachNIO(slot, port)
}

// Close releases the VM reference.
func (vm *VM) Close() {
	vm.native().Close()
}

// ExtractConfig copies the VM's NVRAM configuration into Go-owned memory.
func (vm *VM) ExtractConfig() (ConfigData, error) {
	data, err := vm.native().ExtractConfig()
	return ConfigData(data), err
}

// CreateUDP creates a UDP NIO and returns an owned reference to it.
func (r *Runtime) CreateUDP(config UDPConfig) (*NIO, error) {
	handle, err := r.native().CreateUDP(binding.UDPConfig(config))
	if err != nil {
		return nil, err
	}
	return &NIO{handle: handle}, nil
}

// CreateUDPAuto creates a UDP NIO and returns its bound local port.
func (r *Runtime) CreateUDPAuto(config UDPAutoConfig) (*NIO, uint16, error) {
	handle, port, err := r.native().CreateUDPAuto(binding.UDPAutoConfig(config))
	if err != nil {
		return nil, 0, err
	}
	return &NIO{handle: handle}, port, nil
}

// CreateTAP opens a host TAP device and returns an owned NIO reference.
func (r *Runtime) CreateTAP(config TAPConfig) (*NIO, error) {
	handle, err := r.native().CreateTAP(binding.TAPConfig(config))
	if err != nil {
		return nil, err
	}
	return &NIO{handle: handle}, nil
}

// ConnectUDPAuto connects an auto UDP NIO to its remote endpoint once.
func (nio *NIO) ConnectUDPAuto(remoteHost string, remotePort uint16) error {
	return nio.native().ConnectUDPAuto(remoteHost, remotePort)
}

// Delete removes an unused NIO and consumes its handle on success.
func (nio *NIO) Delete() error {
	return nio.native().Delete()
}

// Stats returns the NIO's traffic counters.
func (nio *NIO) Stats() (NIOStats, error) {
	stats, err := nio.native().Stats()
	return NIOStats(stats), err
}

// Close releases the NIO reference.
func (nio *NIO) Close() {
	nio.native().Close()
}

// SetupFilter configures a filter already bound to the NIO.
func (nio *NIO) SetupFilter(direction FilterDirection, options ...string) error {
	return nio.native().SetupFilter(binding.FilterDirection(direction), options...)
}

// SetC7200NPE selects the processor engine before the router starts.
func (vm *VM) SetC7200NPE(npe string) error {
	return vm.native().SetC7200NPE(npe)
}

// SetC7200Midplane selects the chassis midplane before the router starts.
func (vm *VM) SetC7200Midplane(midplane string) error {
	return vm.native().SetC7200Midplane(midplane)
}

// SetC7200MACAddr sets the chassis base MAC address.
func (vm *VM) SetC7200MACAddr(mac [6]byte) error {
	return vm.native().SetC7200MACAddr(mac)
}

// CreateEthernetSwitch creates a switch and returns an owned reference.
func (r *Runtime) CreateEthernetSwitch(name string) (*EthernetSwitch, error) {
	handle, err := r.native().CreateEthernetSwitch(name)
	if err != nil {
		return nil, err
	}
	return &EthernetSwitch{handle: handle}, nil
}

// AddNIO binds a NIO to the switch.
func (sw *EthernetSwitch) AddNIO(nio *NIO) error {
	return sw.native().AddNIO(nio.native())
}

// Close releases the switch reference.
func (sw *EthernetSwitch) Close() {
	sw.native().Close()
}
