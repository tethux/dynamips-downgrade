package console

import (
	"bytes"
	"context"
	"io"
	"net"
	"sync"
	"time"
)

const (
	telnetSE   = 240
	telnetSB   = 250
	telnetWILL = 251
	telnetWONT = 252
	telnetDO   = 253
	telnetDONT = 254
	telnetIAC  = 255
)

type telnetState uint8

const (
	telnetText telnetState = iota
	telnetCommand
	telnetOption
	telnetSubnegotiation
	telnetSubcommand
)

// telnetConsole decodes the existing Dynamips TCP console protocol.
// One goroutine reads; input and negotiation writes share a lock.
type telnetConsole struct {
	conn    net.Conn
	writeMu sync.Mutex
	state   telnetState
	command byte
}

func (c *telnetConsole) Read(buffer []byte) (int, error) {
	for {
		count, readErr := c.conn.Read(buffer)
		written := 0
		for _, value := range buffer[:count] {
			switch c.state {
			case telnetText:
				if value == telnetIAC {
					c.state = telnetCommand
				} else {
					buffer[written] = value
					written++
				}
			case telnetCommand:
				switch value {
				case telnetIAC:
					buffer[written] = value
					written++
					c.state = telnetText
				case telnetWILL, telnetWONT, telnetDO, telnetDONT:
					c.command = value
					c.state = telnetOption
				case telnetSB:
					c.state = telnetSubnegotiation
				default:
					c.state = telnetText
				}
			case telnetOption:
				if negotiationErr := c.negotiate(value); negotiationErr != nil {
					return written, negotiationErr
				}
				c.state = telnetText
			case telnetSubnegotiation:
				if value == telnetIAC {
					c.state = telnetSubcommand
				}
			case telnetSubcommand:
				if value == telnetSE {
					c.state = telnetText
				} else {
					c.state = telnetSubnegotiation
				}
			}
		}
		if written != 0 || readErr != nil {
			return written, readErr
		}
	}
}

func (c *telnetConsole) negotiate(option byte) error {
	switch c.command {
	case telnetWILL:
		response := byte(telnetDONT)
		if option == 1 || option == 3 { // Server echo and suppress-go-ahead.
			response = telnetDO
		}
		return c.write([]byte{telnetIAC, response, option})
	case telnetDO:
		return c.write([]byte{telnetIAC, telnetWONT, option})
	default:
		return nil
	}
}

func (c *telnetConsole) send(data []byte) error {
	escaped := bytes.ReplaceAll(data, []byte{telnetIAC}, []byte{telnetIAC, telnetIAC})
	return c.write(escaped)
}

func (c *telnetConsole) write(data []byte) error {
	c.writeMu.Lock()
	defer c.writeMu.Unlock()
	if deadlineErr := c.conn.SetWriteDeadline(time.Now().Add(5 * time.Second)); deadlineErr != nil {
		return deadlineErr
	}
	count, writeErr := c.conn.Write(data)
	if writeErr == nil && count != len(data) {
		return io.ErrShortWrite
	}
	return writeErr
}

// Connection owns a Telnet console stream with protocol negotiation and escaping.
type Connection struct {
	stream    *telnetConsole
	stop      func() bool
	closeOnce sync.Once
	closeErr  error
}

// Dial connects to a console. Cancellation closes the stream to unblock readers.
func Dial(ctx context.Context, address string) (*Connection, error) {
	dialer := net.Dialer{Timeout: 5 * time.Second}
	conn, dialErr := dialer.DialContext(ctx, "tcp", address)
	if dialErr != nil {
		return nil, consoleError("connect console", dialErr)
	}
	result := &Connection{stream: &telnetConsole{conn: conn}}
	result.stop = context.AfterFunc(ctx, result.closeStream)
	return result, nil
}

// Read reads guest output after Telnet negotiation.
func (c *Connection) Read(data []byte) (int, error) { return c.stream.Read(data) }

// Write sends guest input with Telnet escaping.
func (c *Connection) Write(data []byte) (int, error) {
	if writeErr := c.stream.send(data); writeErr != nil {
		return 0, writeErr
	}
	return len(data), nil
}

// Close closes the owned connection.
func (c *Connection) Close() error {
	if c == nil {
		return nil
	}
	c.stop()
	c.closeStream()
	return c.closeErr
}

// SetDeadline bounds console reads and writes.
func (c *Connection) SetDeadline(deadline time.Time) error {
	return c.stream.conn.SetDeadline(deadline)
}

func (c *Connection) closeStream() {
	c.closeOnce.Do(func() { c.closeErr = c.stream.conn.Close() })
}
