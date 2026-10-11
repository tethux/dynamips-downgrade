# C7200 boot example

Go reference: [examples/basic](https://pkg.go.dev/github.com/tethux/dynamips-downgrade/examples/basic).

Set the path to a local C7200 IOS image or zip archive:

```sh
DYNAMIPS_IOS_IMAGE=/path/to/c7200-ios.zip mise run run:example
```

The program also accepts RAM and console options directly:

```sh
mise exec -- go run ./examples/basic -ios /path/to/c7200-ios.bin -ram 512 -console-port 2001
```

The task opens `telnet` in the same terminal once the VM starts. Wait for
`Press RETURN to get started!`, then press Enter. The example loads a small
startup config, so IOS skips the initial setup dialog. Type IOS commands at
the prompt. For example, enter `enable`, then `show version`. Press Ctrl+]
and type `quit` to stop and delete the VM.

The example creates temporary working files outside the repository and
removes them when it exits. Cisco IOS images are not included here.

## Enhanced Go console demo

Boot a local C7200 IOS archive with local editing and history:

```sh
mise exec -- go run ./examples/basic -enhanced -ios /path/to/c7200-ios.zip
```

Use Up/Down for session history, Ctrl+R to search, and the usual cursor editing
keys. Enter sends the edited line followed by CR. Ctrl+C clears the local line
and sends an interrupt to IOS. Ctrl+D on an empty line or `/quit` stops the VM.
`/raw` closes the enhanced session and reconnects with the existing Telnet
client, keeping the VM running. Omitting `-enhanced` starts Telnet directly.

The editor uses the IOS prompt, including hostname and configuration-mode
changes. Guest prompts and submitted-command echo are consumed, so only the
enhanced prompt is displayed. Input stays locked during boot; keys typed
before the first prompt are discarded. While IOS is producing output, keys
go directly to the guest, so Space and Enter advance the pager, and `q` or Ctrl+C can stop it. The frontend
presses Enter during boot to obtain the first prompt. If boot prompt detection
times out, it reconnects using raw Telnet.
Asynchronous IOS messages print above the editor and redraw its prompt and
unfinished input, like `logging synchronous`, without changing IOS
configuration.

Press `?` for IOS contextual help without Enter. Tab completes through IOS
on the current editor line, including at the cursor in the middle of a command.
Empty or ambiguous completion leaves the draft intact. The editor restores
the unfinished command after contextual help.
Switch to `/raw` before passwords or other interactive applications.
Commands are kept in memory for this session only; enhanced input is visible
and enters history.

The flag and frontend live in the example, outside the Go VM bindings and
native emulator. Telnet decoding stays at the connection boundary. Only one
operator should write to this guest console at a time.

Shitnet uses `chzyer/readline`. This demo uses its `ergochat/readline` fork:
the original library raced between asynchronous output and Ctrl+R search
during the real IOS checks. The fork serializes both operations.
