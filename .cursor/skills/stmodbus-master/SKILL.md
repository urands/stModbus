---
name: stmodbus-master
description: >-
  Run Modbus RTU master commands via the stmodbus-master CLI (serial port,
  JSON output). Use when the user wants to read/write Modbus from the host,
  drive hardware, or parse register values in scripts. The stModbus library is
  slave-only; the master tool is separate—see repo README.
---

# stmodbus-master (Modbus RTU master)

## When to use

- User wants **host-side Modbus RTU** (not firmware): read holding/input registers, read coils, write single holding or coil.
- User wants **machine-readable** results: default stdout is **one JSON line**.

## Build

From repo root: `cmake -S . -B build && cmake --build build` → `build/stmodbus-master`.

## Rules

1. **`--tty` is required** (e.g. `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, or `COM3` on Windows).
2. Subcommand and flags can be in **any order** (e.g. `--tty` before `read-holding`).
3. **`--mb-address`** is always **Modbus logical** address: holding `40001+`, input `30001+`, coil `1+`.
4. Default **`--format json`**: success looks like `{"ok":true,"cmd":"read_holding","count":2,"values":[4660,13124]}`; failure `{"ok":false,"error":"timeout"}` (etc.).
5. **Do not** use `stmodbus-cli` for master operations—that binary is a **slave** harness only.

## Examples

Read two holding registers starting at 40001, unit id 1:

```sh
./build/stmodbus-master read-holding --tty /dev/ttyUSB0 --mb-address 40001 --count 2
```

Write holding 40001 = 1234:

```sh
./build/stmodbus-master write-holding --tty /dev/ttyUSB0 --mb-address 40001 --value 1234
```

Read coils 1–8:

```sh
./build/stmodbus-master read-coils --tty /dev/ttyUSB0 --mb-address 1 --count 8
```

## Reference

Full flag list and limitations: [README.md](../../../README.md) section **stmodbus-master (RTU master)**.
