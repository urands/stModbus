# stModbus (beta 0.1b10)

Small **Modbus RTU slave** stack in C, aimed at STM32 (F1/F3/F4) with CMSIS/HAL, and usable on desktop for tests. You can run several independent Modbus contexts (e.g. multiple UART ports). A companion **`stmodbus-master`** binary implements a **minimal RTU master** for scripts and AI agents (it uses the library for **CRC only**; the stack itself remains slave-only).

License: see [LICENSE.md](LICENSE.md).

## Requirements

- C11 toolchain for the library and CLI.
- **CMake 3.16+** (optional but recommended) or any C compiler to build sources manually.
- For MCU: vendor HAL (e.g. STM32 HAL) for UART and, in real links, a monotonic `mbus_tickcount()` for inter-frame timing.

## Repository layout

| Path | Purpose |
|------|---------|
| `include/` | Public headers (`modbus.h`, `modbus_conf.h`, …) |
| `src/` | Library sources (`modbus.c`, `rtu/mbutils.c`) |
| `src/dev/` | Example device integration (board-specific; not used by CMake CLI) |
| `tools/stmodbus-cli/` | Internal desktop harness: self-test, binary **stdio** pipe, or **serial** slave |
| `tools/stmodbus-master/` | **RTU master** CLI (read/write coils and registers); JSON-friendly output |
| `projects/` | Legacy IDE projects (Keil, MSVC, Qt `.pro`) |

## Build (CMake)

Configure and build from the repository root:

```sh
cmake -S . -B build
cmake --build build
```

Artifacts:

- Static library: `build/` (library target `stmodbus`).
- Slave harness: `build/stmodbus-cli` — see [Using stmodbus-cli](#using-stmodbus-cli).
- Master CLI: `build/stmodbus-master` — see [stmodbus-master (RTU master)](#stmodbus-master-rtu-master).

To skip the CLI (library only):

```sh
cmake -S . -B build -DBUILD_CLI=OFF
cmake --build build
```

### Makefile wrapper

If you prefer `make`:

```sh
make          # cmake -S . -B build && cmake --build build
make clean    # rm -rf build
```

### Windows note

Use a CMake generator appropriate for your environment, for example Visual Studio (`cmake -G "Visual Studio 17 2022" -A x64 -S . -B build`) or Ninja.

## stmodbus-master (RTU master)

The **stModbus** library implements a **Modbus slave** (responses via `mbus_poll` / callbacks). There is **no master API** in the library. **`stmodbus-master`** is a separate program that speaks RTU on a serial port as a **client**: it builds request ADUs, sends them, reads the response, and checks CRC using **`mbus_crc16`** from the same linked library.

Default output is **one JSON line** on stdout (`{"ok":true,...}` or `{"ok":false,"error":"..."}`), suitable for tools and LLM workflows. Use **`--format text`** for human-readable lines.

Subcommand name and flags can appear in **any order** (for example `--tty` before or after `read-holding`).

| Subcommand | Purpose |
|------------|---------|
| `read-holding` | FC 3 — **`--mb-address`** logical holding `40001`–`49999`, **`--count`** `1`–`64` |
| `read-input` | FC 4 — **`--mb-address`** input `30001`–`39999`, **`--count`** `1`–`64` |
| `read-coils` | FC 1 — **`--mb-address`** coil `1`–`9999`, **`--count`** `1`–`2000` |
| `write-holding` | FC 6 — single holding register, **`--value`** `0`–`65535` |
| `write-coil` | FC 5 — **`--on`** `0` or `1` |

Common flags: **`--tty`** (required), **`--baud`**, **`--unit`**, **`--timeout-ms`**, **`--post-tx-ms`**, **`--format json|text`**.

**Examples**

```sh
# Read two holding registers at 40001 from slave 1; JSON line to stdout
./build/stmodbus-master read-holding --tty /dev/ttyUSB0 --mb-address 40001 --count 2

# Same with explicit baud and text output
./build/stmodbus-master --tty /dev/ttyUSB0 --baud 9600 write-holding --mb-address 40001 --value 99 --format text

# Windows
build\Release\stmodbus-master.exe read-input --tty COM4 --mb-address 30001 --count 1
```

**Agent-oriented example** (parse `values` when `ok` is true):

```text
{"ok":true,"cmd":"read_holding","count":2,"values":[4660,13124]}
```

Errors include Modbus exceptions, timeouts (`"error":"timeout"`), CRC/length issues (`"bad_response"`, `"unexpected_response"`), and bad CLI usage (non-zero exit, message on stderr for some cases; JSON errors still go to stdout when `--format json`).

## Using stmodbus-cli

The tool embeds the same **sample slave** as the self-test (slave address **1** by default, holding registers **40001** / **40002** seeded with `0x1122` and `0x3344`). It is meant for quick checks against a Modbus master or scripts, not production field use.

```text
stmodbus-cli [--help|-h]
stmodbus-cli [selftest] [options...]           # default: in-process RTU self-test
stmodbus-cli stdio [options...] [addr]         # RTU on stdin → responses on stdout (binary)
stmodbus-cli tty <device> [baud] [addr] [options...]
```

- **`addr`** (positional): Modbus slave id `1`–`247` (default `1`). On **`tty`**, if you pass **`addr`** you must also pass **`baud`** (e.g. `stmodbus-cli tty /dev/ttyUSB0 115200 2`). Use **`--addr`** to set the id without relying on position (flags may appear in any order after the subcommand).
- **`baud`**: ignored in **stdio** mode. In **tty** mode the default is **115200**; supported values: `9600`, `19200`, `38400`, `57600`, `115200`, `230400`.

**Optional flags** (after `selftest`, `stdio`, or mixed with `tty` positionals):

| Flag | Meaning |
|------|---------|
| `--addr <1–247>` | Slave unit id; overrides the trailing `[addr]` on `stdio` / `tty` when set. |
| `--regs-from <path>` | Load holding registers **40001**–**40008** before serving. See **`--regs-type`**. |
| `--regs-type bin` or `hex` | **`bin`** (default): file is up to 16 bytes of **big-endian uint16** values in order. **`hex`**: text file — decimal or `0x` hex values, separated by whitespace or commas; `#` starts a comment to end of line. |
| `--log-reads <path>` | Append one line per logical read: `R <la> <value>` (decimal). |
| `--log-writes <path>` | Append one line per logical write: `W <la> <value>`. |

For **`--log-reads`** / **`--log-writes`**, the path **`-`** means **stderr** so text logs do not corrupt binary RTU on stdout.

**`--regs-from -`** reads **stdin** once (binary or hex per **`--regs-type`**). That is allowed for **`selftest`** and **`tty`**; it is **rejected for `stdio`**, because stdin is already the RTU byte stream.

### stdio (binary pipe)

**macOS / Linux:** stdin and stdout are used as raw byte streams. Feed RTU request frames on stdin; the slave writes RTU responses on stdout. Suitable for piping from another program or from recorded hex.

Example (same request as the self-test: read two holding registers from slave 1):

```sh
# 01 03 00 00 00 02 + CRC — response appears on stdout (binary)
printf '\x01\x03\x00\x00\x00\x02\xc4\x0b' | ./build/stmodbus-cli stdio | xxd
```

**Windows** (Developer Command Prompt / PowerShell): the CLI sets **binary** mode on stdin/stdout so `0x0A` is not translated. You can still pipe from another tool that speaks binary:

```bat
stmodbus-cli stdio < request.bin > response.bin
```

Use slave id 5:

```sh
./build/stmodbus-cli stdio 5
```

### Serial device (tty)

**Linux:** USB adapters are usually `/dev/ttyUSB0` or `/dev/ttyACM0`. You need read/write access (often `dialout` group).

```sh
./build/stmodbus-cli tty /dev/ttyUSB0
./build/stmodbus-cli tty /dev/ttyUSB0 9600
./build/stmodbus-cli tty /dev/ttyUSB0 115200 2
```

**macOS:** use the **call-out** device (e.g. `cu.usbserial-*`, `cu.usbmodem*`) so the port is not held by a login session:

```sh
./build/stmodbus-cli tty /dev/cu.usbserial-1410
./build/stmodbus-cli tty /dev/cu.usbserial-1410 115200 1
```

**Windows:** pass the COM name as shown in Device Manager (`COM3`, `COM12`, …). Internally the program opens `\\.\COMn` (required for COM10 and above).

```bat
build\Release\stmodbus-cli.exe tty COM3
build\Release\stmodbus-cli.exe tty COM3 9600
build\Release\stmodbus-cli.exe tty COM3 115200 2
```

### COM, stdio, and pipes

Each `stmodbus-cli` run uses a **single bidirectional** path: **`stdio`** uses binary stdin and stdout together; **`tty`** uses one serial port for both RX and TX. The program does not implement split paths (for example COM in with responses only on a pipe). Modbus RTU masters expect the response on the **same** medium as the request.

**Bridging a serial port to `stdio` (no `stmodbus-cli` changes):** on macOS and Linux, run **`socat`** so the UART is one end of the bridge and `stmodbus-cli stdio` is the other:

```sh
socat /dev/cu.usbserial-XXXX,raw,echo=0,b115200 EXEC:'./build/stmodbus-cli stdio',pty,raw,echo=0
```

Match the device path and `b115200` to the adapter. Traffic on the serial port is forwarded to the slave’s stdin/stdout behind `socat`.

**Pipes with `stdio`:** use shell redirection and binary-safe tools (`|`, `>`, `tee`, another program) on the stdin/stdout pair.

**Windows:** use a virtual serial pair (for example com0com) or a TCP–serial bridge, then attach **`tty`** to the COM port that faces the master.

**Split RX/TX** (different file descriptors, or COM in with TX only to a pipe) is **not** supported by this CLI; it would require a custom `send` path and read loop in application code.

### Limitations

- **Inter-frame timing:** `mbus_tickcount()` defaults to **0** in the library’s weak stub, so **3.5 character** RTU gaps are not enforced in this CLI build. Many masters still work; if framing fails, run the stack on hardware with a real tick source or extend the CLI with a monotonic timer.
- **Serial settings:** `tty` mode uses **8N1**, no hardware flow control.

## Embedding on MCU

1. Add `src/modbus.c` and `src/rtu/mbutils.c` to your firmware project and set the include path to `include/`.
2. Edit [`include/modbus_conf.h`](include/modbus_conf.h) (buffer sizes, coil/discrete counts, number of contexts, RTU vs ASCII).
3. Provide **`mbus_tickcount()`** with a weak implementation in `mbutils.c` on some toolchains; override it on the MCU so it returns a millisecond (or tick) counter used for the 3.5 character timeout logic in `mbus_poll`.
4. Allocate **`sendbuf` / `recvbuf`** and set `sendbuf_sz` / `recvbuf_sz` in `Modbus_Conf_t` before `mbus_open` (must be large enough for the longest frame you handle; see `STMODBUS_MAX_RESPONSE_SIZE`).
5. Implement **`send`** to push bytes to your UART (or queue). It must return the number of bytes accepted (the stack expects the full size including CRC on transmit).
6. Implement **`read`** / **`write`** callbacks using your logical Modbus addresses (40001-style holding registers, 30001-style input registers, coils, discretes — see below).
7. In the UART RX path, call **`mbus_poll(context, byte)`** for each received byte (or after DMA idle, for each byte in the buffer).

Legacy project files under `projects/` are optional references; CMake is the supported path for the desktop harness.

## Address map (conventional)

Typical logical addressing:

- `1–9999` — coils (discrete outputs), read/write  
- `10001–19999` — discrete inputs, read-only  
- `30001–39999` — input registers, read-only  
- `40001–49999` — holding registers, read/write  

You may use other schemes, but some masters expect these ranges.

## API sketch

Types match [`include/modbus.h`](include/modbus.h): `read` / `write` return `uint16_t`; use `mbus_error(...)` to signal Modbus exception conditions from the callbacks.

```c
#include <modbus.h>
#include <stdint.h>
#include <string.h>

static uint8_t txbuf[256];
static uint8_t rxbuf[256];

static int mbus_uart_send(const mbus_t ctx, const uint8_t *data, const uint16_t size) {
    (void)ctx;
    /* TODO: HAL_UART_Transmit / DMA — must return (int)size on full success */
    return (int)size;
}

static uint16_t app_read(const uint32_t la) {
    if (la == 40001) {
        return 0x1234;
    }
    return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
}

static uint16_t app_write(const uint32_t la, const uint16_t value) {
    if (la == 40001) {
        /* store value */
        return value;
    }
    return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
}

void app_modbus_init(void) {
    Modbus_Conf_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.devaddr = 1;
    cfg.coils = STMODBUS_COUNT_COILS;
    cfg.discrete = STMODBUS_COUNT_DISCRETE;
    cfg.device = NULL;
    cfg.send = mbus_uart_send;
    cfg.read = app_read;
    cfg.write = app_write;
    cfg.sendbuf = txbuf;
    cfg.sendbuf_sz = sizeof(txbuf);
    cfg.recvbuf = rxbuf;
    cfg.recvbuf_sz = sizeof(rxbuf);

    mbus_t mb = mbus_open(&cfg);
    if (mb < 0) {
        /* handle error */
    }
}
```

On reception:

```c
/* For each byte from the line: */
mbus_poll(mb, byte);
```

## Changelog

See [CHANGELOG.md](CHANGELOG.md).
