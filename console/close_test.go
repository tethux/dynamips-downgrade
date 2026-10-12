package console

import (
	"context"
	"errors"
	"net"
	"sync"
	"sync/atomic"
	"testing"
)

type failedClose struct {
	net.Conn
	calls atomic.Int64
	cause error
}

func (c *failedClose) Close() error { c.calls.Add(1); return c.cause }

func TestCancellationPreservesCloseFailure(t *testing.T) {
	cause := errors.New("injected close failure")
	native := &failedClose{cause: cause}
	ctx, cancel := context.WithCancel(t.Context())
	conn := &Connection{stream: &telnetConsole{conn: native}}
	conn.stop = context.AfterFunc(ctx, conn.closeStream)
	cancel()
	var workers sync.WaitGroup
	for range 8 {
		workers.Go(func() {
			if closeErr := conn.Close(); !errors.Is(closeErr, cause) {
				t.Errorf("lost close error: %v", closeErr)
			}
		})
	}
	workers.Wait()
	if native.calls.Load() != 1 {
		t.Fatalf("native Close called %d times", native.calls.Load())
	}
}
