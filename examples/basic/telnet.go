package main

import (
	"bytes"
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
