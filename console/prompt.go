package console

import (
	"bytes"
	"context"
	"errors"
	"os"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/ergochat/readline"
	"golang.org/x/sys/unix"
)

var (
	consolePrompt               = regexp.MustCompile(`^[A-Za-z][A-Za-z0-9_.-]*(?:\([^()\r\n]*\))?[>#]`)
	errConsoleCompletionTimeout = errors.New("timed out waiting for IOS completion")
	errConsolePromptTimeout     = errors.New("timed out waiting for IOS prompt")
)

// Read only requested input so the editor cannot prefetch pager keys.
type consoleInput struct{ terminal *os.File }

func (input consoleInput) Read(buffer []byte) (int, error) {
	if len(buffer) > 1 {
		buffer = buffer[:1]
	}
	return input.terminal.Read(buffer)
}

type guestPrompt struct{ name, draft string }

type consoleDisplay struct {
	editor      *readline.Instance
	prompts     chan guestPrompt
	mu          sync.Mutex
	awaiting    bool
	echo        string
	interaction rune
	echoPending bool
	completion  *consoleCompletion
}

func newConsoleDisplay(editor *readline.Instance) *consoleDisplay {
	return &consoleDisplay{editor: editor, prompts: make(chan guestPrompt, 1), awaiting: true}
}

func (display *consoleDisplay) awaitPrompt(command string, interaction rune) {
	display.mu.Lock()
	defer display.mu.Unlock()
	display.awaiting = true
	display.echo = command
	display.echoPending = true
	display.interaction = interaction
}

func (display *consoleDisplay) line(data []byte) error {
	display.mu.Lock()
	if display.completion != nil {
		display.completion.output = append(display.completion.output, data...)
		display.mu.Unlock()
		return nil
	}
	display.mu.Unlock()
	text := consoleText(data)
	display.mu.Lock()
	prompt := ""
	for {
		prefix := consolePrompt.FindString(text)
		if prefix == "" {
			break
		}
		prompt = prefix
		text = text[len(prefix):]
	}
	if display.echoPending && (prompt == "" || display.echo != "") && strings.TrimSpace(text) == strings.TrimSpace(display.echo) {
		display.echo = ""
		display.echoPending = false
		display.mu.Unlock()
		return nil
	}
	if prompt != "" {
		if display.awaiting && (display.echo == "" || display.interaction != 0) {
			display.awaiting = false
			draft := ""
			if display.interaction != 0 {
				draft = text
				text = ""
			}
			display.prompts <- guestPrompt{name: prompt, draft: draft}
		}
	}
	display.mu.Unlock()
	if prompt != "" {
		if text == "" {
			return nil
		}
		data = []byte(text)
	}
	if len(data) == 0 {
		return nil
	}
	if !bytes.Contains(data, []byte("--More--")) {
		data = append(bytes.Clone(bytes.TrimRight(data, "\r\n")), '\n')
	}
	_, writeErr := display.editor.Stdout().Write(data)
	return writeErr
}

func waitConsolePrompt(ctx context.Context, prompts <-chan guestPrompt, console *telnetConsole, terminal *os.File, terminalFD int32, booting bool) (guestPrompt, error) {
	timeout := 30 * time.Second
	if booting {
		timeout = 180 * time.Second
	}
	deadline := time.NewTimer(timeout)
	var expired <-chan time.Time
	if booting {
		expired = deadline.C
	}
	defer deadline.Stop()
	nudge := time.NewTicker(50 * time.Millisecond)
	lastNudge := time.Now()
	defer nudge.Stop()
	for {
		select {
		case prompt := <-prompts:
			return prompt, nil
		case <-ctx.Done():
			return guestPrompt{}, ctx.Err()
		case <-expired:
			return guestPrompt{}, errConsolePromptTimeout
		case <-nudge.C:
			if booting && time.Since(lastNudge) >= 3*time.Second {
				lastNudge = time.Now()
				if writeErr := console.send([]byte{'\r'}); writeErr != nil {
					return guestPrompt{}, writeErr
				}
			}
			if !booting {
				ready := []unix.PollFd{{Fd: terminalFD, Events: unix.POLLIN}}
				_, pollErr := unix.Poll(ready, 0)
				if errors.Is(pollErr, unix.EINTR) {
					continue
				}
				if pollErr != nil {
					return guestPrompt{}, pollErr
				}
				if ready[0].Revents&unix.POLLIN != 0 {
					var key [1]byte
					_, readErr := terminal.Read(key[:])
					if readErr != nil {
						return guestPrompt{}, readErr
					}
					if writeErr := console.send(key[:]); writeErr != nil {
						return guestPrompt{}, writeErr
					}
				}
			}
		}
	}
}

func consoleText(data []byte) string {
	// IOS erases a pending draft or pager marker with backspaces.
	visible := make([]rune, 0, len(data))
	for _, character := range string(bytes.Trim(data, "\r\n")) {
		if character == '\b' {
			if len(visible) != 0 {
				visible = visible[:len(visible)-1]
			}
		} else if character != '\a' {
			visible = append(visible, character)
		}
	}
	return string(visible)
}

type consoleCompletion struct {
	prefix  string
	output  []byte
	done    chan string
	started time.Time
}

func (display *consoleDisplay) complete(ctx context.Context, console *telnetConsole, prefix string) (string, error) {
	request := &consoleCompletion{prefix: prefix, done: make(chan string, 1), started: time.Now()}
	display.mu.Lock()
	display.completion = request
	display.mu.Unlock()
	if writeErr := console.send([]byte("\x15" + prefix + "\t")); writeErr != nil {
		return "", writeErr
	}
	deadline := time.NewTimer(5 * time.Second)
	defer deadline.Stop()
	select {
	case draft := <-request.done:
		return draft, nil
	case <-ctx.Done():
		return "", ctx.Err()
	case <-deadline.C:
		return "", errConsoleCompletionTimeout
	}
}

func (display *consoleDisplay) finishCompletion() error {
	display.mu.Lock()
	request := display.completion
	if request == nil || time.Since(request.started) < 100*time.Millisecond {
		display.mu.Unlock()
		return nil
	}
	draft := request.prefix
	var messages []string
	for _, line := range bytes.Split(request.output, []byte{'\n'}) {
		text := consoleText(line)
		for {
			prompt := consolePrompt.FindString(text)
			if prompt == "" {
				break
			}
			text = text[len(prompt):]
		}
		if strings.HasPrefix(text, "*") || strings.HasPrefix(text, "%") {
			messages = append(messages, strings.TrimSuffix(text, request.prefix))
			continue
		}
		if text != "" {
			draft = text
		}
	}
	display.completion = nil
	display.mu.Unlock()
	for _, message := range messages {
		if writeErr := display.line([]byte(message + "\n")); writeErr != nil {
			return writeErr
		}
	}
	request.done <- draft
	return nil
}
