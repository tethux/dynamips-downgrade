package main

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"math"
	"net"
	"os"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/ergochat/readline"
	"github.com/tethux/dynamips-downgrade/errs"
	"golang.org/x/sys/unix"
	"golang.org/x/term"
)

func consoleError(operation string, cause error) error {
	return &errs.Operation{Op: operation, Kind: errs.ErrIO, Cause: cause}
}

func runEnhancedConsole(ctx context.Context, port uint16) (raw bool, resultErr error) {
	address := net.JoinHostPort("127.0.0.1", strconv.Itoa(int(port)))
	dialer := net.Dialer{Timeout: 5 * time.Second}
	conn, dialErr := dialer.DialContext(ctx, "tcp", address)
	if dialErr != nil {
		return false, consoleError("connect console", dialErr)
	}
	defer func() {
		closeErr := conn.Close()
		if !errors.Is(closeErr, net.ErrClosed) {
			resultErr = errors.Join(resultErr, closeErr)
		}
	}()

	// Own a separate terminal descriptor so closing the editor cannot steal
	// input from the Telnet client when /raw reconnects.
	terminal, openErr := os.OpenFile("/dev/tty", os.O_RDONLY|syscall.O_NONBLOCK, 0)
	if openErr != nil {
		return false, consoleError("open console terminal", openErr)
	}
	descriptor := terminal.Fd()
	if descriptor > math.MaxInt32 {
		return false, consoleError("open console terminal", errors.Join(unix.EBADF, terminal.Close()))
	}
	terminalFD := int32(descriptor)
	state, rawErr := term.MakeRaw(unix.Stdin)
	if rawErr != nil {
		return false, consoleError("prepare console terminal", errors.Join(rawErr, terminal.Close()))
	}
	defer func() { resultErr = errors.Join(resultErr, term.Restore(unix.Stdin, state)) }()
	attributes, attributesErr := unix.IoctlGetTermios(unix.Stdin, unix.TCGETS)
	if attributesErr != nil {
		return false, consoleError("read terminal output mode", errors.Join(attributesErr, terminal.Close()))
	}
	// Keep newline translation for readline while input stays raw.
	attributes.Oflag |= unix.OPOST | unix.ONLCR
	if outputModeErr := unix.IoctlSetTermios(unix.Stdin, unix.TCSETS, attributes); outputModeErr != nil {
		return false, consoleError("prepare terminal output mode", errors.Join(outputModeErr, terminal.Close()))
	}
	sessionCtx, cancel := context.WithCancel(ctx)
	defer cancel()
	console := &telnetConsole{conn: conn}
	var display *consoleDisplay
	var interaction rune
	var completing bool
	var completionErr error
	editor, editorErr := readline.NewFromConfig(&readline.Config{
		Stdin: consoleInput{terminal},
		FuncFilterInputRune: func(key rune) (rune, bool) {
			if key == '\t' {
				completing = true
				// Let the listener handle Tab without inserting it or submitting the line.
				return readline.MetaShiftTab, true
			}
			if key == '?' {
				interaction = key
				return readline.CharEnter, true
			}
			return key, true
		},
		Listener: func(line []rune, position int, _ rune) ([]rune, int, bool) {
			if !completing {
				return nil, 0, false
			}
			completing = false
			draft, completeErr := display.complete(sessionCtx, console, string(line[:position]))
			if completeErr != nil {
				completionErr = completeErr
				cancel()
				return nil, 0, false
			}
			if draft == string(line[:position]) {
				return nil, 0, false
			}
			if position < len(line) && line[position] == ' ' {
				draft = strings.TrimSuffix(draft, " ")
			}
			prefix := []rune(draft)
			return append(prefix, line[position:]...), len(prefix), true
		},
		HistoryLimit:           500,
		DisableAutoSaveHistory: true,
		FuncMakeRaw:            func() error { return nil },
		FuncExitRaw:            func() error { return nil },
	})
	if editorErr != nil {
		return false, consoleError("open console editor", errors.Join(editorErr, terminal.Close()))
	}
	defer func() { resultErr = errors.Join(resultErr, editor.Close()) }()

	outputDone := make(chan error, 1)
	shutdownDone := make(chan error, 1)
	display = newConsoleDisplay(editor)
	go func() {
		copyErr := displayConsoleOutput(display, console)
		outputDone <- copyErr
		cancel()
	}()
	go func() {
		<-sessionCtx.Done()
		shutdownDone <- errors.Join(conn.Close(), terminal.Close(), editor.Close())
	}()
	defer func() {
		cancel()
		shutdownErr := <-shutdownDone
		outputErr := <-outputDone
		if errors.Is(outputErr, net.ErrClosed) {
			outputErr = nil
		}
		resultErr = errors.Join(resultErr, shutdownErr, outputErr)
	}()

	_, bannerErr := fmt.Fprint(editor.Stdout(), "Enhanced console: Up/Down history, Ctrl+R search, Ctrl+C sends interrupt.\n/quit stops the VM; /raw reconnects using Telnet. History stays in memory.\nWaiting for IOS. Input unlocks when its prompt arrives.\n")
	if bannerErr != nil {
		return false, consoleError("write console help", bannerErr)
	}
	booting := true
	for {
		prompt, promptErr := waitConsolePrompt(sessionCtx, display.prompts, console, terminal, terminalFD, booting)
		if sessionCtx.Err() != nil {
			return false, nil
		}
		if errors.Is(promptErr, errConsolePromptTimeout) {
			_, noticeErr := fmt.Fprintln(editor.Stdout(), "IOS prompt not recognized; reconnecting with raw Telnet.")
			return true, noticeErr
		}
		if promptErr != nil {
			return false, consoleError("wait for IOS prompt", promptErr)
		}
		if booting {
			if flushErr := unix.IoctlSetInt(int(terminalFD), unix.TCFLSH, unix.TCIFLUSH); flushErr != nil {
				return false, consoleError("discard input during boot", flushErr)
			}
		}
		booting = false
		editor.SetPrompt(prompt.name + " ")
		editor.SetDefault(prompt.draft)
		interaction = 0
		line, readErr := editor.ReadLine()
		if completionErr != nil && !errors.Is(completionErr, context.Canceled) {
			return false, consoleError("complete IOS command", completionErr)
		}
		if sessionCtx.Err() != nil || errors.Is(readErr, io.EOF) {
			return false, nil
		}
		if errors.Is(readErr, readline.ErrInterrupt) {
			display.awaitPrompt("", 0)
			if writeErr := console.send([]byte{3}); writeErr != nil {
				return false, consoleError("interrupt guest console", writeErr)
			}
			continue
		}
		if readErr != nil {
			return false, consoleError("read console input", readErr)
		}
		switch line {
		case "/quit":
			return false, nil
		case "/raw":
			return true, nil
		}
		suffix := "\r"
		if interaction != 0 {
			suffix = string(interaction)
		}
		echo := line
		if interaction == '?' {
			echo += "?"
		}
		display.awaitPrompt(echo, interaction)
		// IOS retains the draft after help and completion; replace it with the edited line.
		if writeErr := console.send([]byte("\x15" + line + suffix)); writeErr != nil {
			return false, consoleError("write guest console", writeErr)
		}
		if line != "" && interaction == 0 {
			if historyErr := editor.SaveToHistory(line); historyErr != nil {
				return false, consoleError("save console history", historyErr)
			}
		}
	}
}

func displayConsoleOutput(display *consoleDisplay, console *telnetConsole) error {
	buffer := make([]byte, 4096)
	pending := make([]byte, 0, 8192)
	for {
		if deadlineErr := console.conn.SetReadDeadline(time.Now().Add(100 * time.Millisecond)); deadlineErr != nil {
			return deadlineErr
		}
		count, readErr := console.Read(buffer)
		pending = append(pending, buffer[:count]...)
		for {
			newline := bytes.IndexByte(pending, '\n')
			if newline < 0 {
				break
			}
			if displayErr := display.line(pending[:newline+1]); displayErr != nil {
				return displayErr
			}
			pending = append(pending[:0], pending[newline+1:]...)
		}
		var timeout net.Error
		idle := errors.As(readErr, &timeout) && timeout.Timeout()
		if len(pending) != 0 && (idle || readErr != nil || len(pending) >= 4096) {
			if displayErr := display.line(pending); displayErr != nil {
				return displayErr
			}
			pending = pending[:0]
		}
		if idle {
			if completeErr := display.finishCompletion(); completeErr != nil {
				return completeErr
			}
		}
		if readErr != nil && !idle {
			if errors.Is(readErr, io.EOF) {
				return nil
			}
			return readErr
		}
	}
}
