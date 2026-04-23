/**
 * stmodbus-cli: self-test, stdio slave (binary pipe), or serial slave (tty/COM).
 * Optional: --addr, --regs-from (+ --regs-type), --log-reads, --log-writes.
 */

#include <modbus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#define SEND_BUF_SZ (STMODBUS_MAX_RESPONSE_SIZE + 16)
#define RECV_BUF_SZ (STMODBUS_MAX_RESPONSE_SIZE + 16)
#define HOLDING_COUNT 8u

static uint8_t dev_output_coils;
static uint16_t dev_input_registers_30001;
static uint16_t holding_registers_4000x[HOLDING_COUNT];

static uint8_t g_last_tx[512];
static uint16_t g_last_tx_len;

#ifdef _WIN32
static HANDLE g_serial = INVALID_HANDLE_VALUE;
#else
static int g_serial_fd = -1;
#endif

typedef enum { REGS_TYPE_BIN = 0, REGS_TYPE_HEX = 1 } regs_type_t;

typedef struct {
  uint8_t addr;
  int addr_explicit;
  const char *regs_path;
  regs_type_t regs_type;
  const char *log_reads_path;
  const char *log_writes_path;
} cli_opts;

static FILE *g_log_reads;
static FILE *g_log_writes;

static void log_streams_close(void) {
  if (g_log_reads) {
    if (g_log_reads != stderr) {
      (void)fclose(g_log_reads);
    }
    g_log_reads = NULL;
  }
  if (g_log_writes) {
    if (g_log_writes != stderr) {
      (void)fclose(g_log_writes);
    }
    g_log_writes = NULL;
  }
}

static FILE *open_log_path(const char *path, const char *mode) {
  if (path == NULL) {
    return NULL;
  }
  if (strcmp(path, "-") == 0) {
    return stderr;
  }
  return fopen(path, mode);
}

static int load_regs_bin(FILE *f) {
  uint8_t raw[HOLDING_COUNT * 2u];
  const size_t n = fread(raw, 1, sizeof(raw), f);
  const size_t words = n / 2u;
  for (size_t i = 0; i < words && i < HOLDING_COUNT; i++) {
    holding_registers_4000x[i] = (uint16_t)(((uint16_t)raw[i * 2u] << 8) | raw[i * 2u + 1u]);
  }
  return 0;
}

static int load_regs_hex_text(const char *buf, size_t len) {
  size_t idx = 0;
  const char *p = buf;
  const char *end = buf + len;

  while (idx < HOLDING_COUNT && p < end) {
    while (p < end && (unsigned char)*p <= 32u) {
      p++;
    }
    if (p >= end) {
      break;
    }
    if (*p == '#') {
      while (p < end && *p != '\n') {
        p++;
      }
      continue;
    }
    char *nend = NULL;
    const unsigned long v = strtoul(p, &nend, 0);
    if (nend == p || v > 0xFFFFul) {
      return -1;
    }
    holding_registers_4000x[idx++] = (uint16_t)v;
    p = nend;
  }
  return 0;
}

static int load_regs_hex_file(FILE *f) {
  char buf[4096];
  const size_t n = fread(buf, 1, sizeof(buf) - 1u, f);
  buf[n] = '\0';
  return load_regs_hex_text(buf, n);
}

static int load_regs_from(const cli_opts *o) {
  if (o->regs_path == NULL) {
    return 0;
  }

  FILE *f = strcmp(o->regs_path, "-") == 0 ? stdin : fopen(o->regs_path, "rb");
  if (f == NULL) {
    fprintf(stderr, "stmodbus-cli: cannot open --regs-from %s\n", o->regs_path);
    return -1;
  }
  int err = 0;
  if (o->regs_type == REGS_TYPE_HEX) {
    err = load_regs_hex_file(f);
  } else {
    err = load_regs_bin(f);
  }
  if (f != stdin) {
    (void)fclose(f);
  }
  if (err != 0) {
    fputs("stmodbus-cli: invalid --regs-from (hex) content\n", stderr);
  }
  return err;
}

static int logs_open(const cli_opts *o) {
  g_log_reads = open_log_path(o->log_reads_path, "a");
  if (o->log_reads_path && g_log_reads == NULL) {
    fprintf(stderr, "stmodbus-cli: cannot open --log-reads %s\n", o->log_reads_path);
    return -1;
  }
  g_log_writes = open_log_path(o->log_writes_path, "a");
  if (o->log_writes_path && g_log_writes == NULL) {
    fprintf(stderr, "stmodbus-cli: cannot open --log-writes %s\n", o->log_writes_path);
    log_streams_close();
    return -1;
  }
  return 0;
}

static uint16_t crc_run(const uint8_t *data, uint16_t len) {
  uint16_t c = 0xFFFFu;
  for (uint16_t i = 0; i < len; i++) {
    c = mbus_crc16(c, data[i]);
  }
  return c;
}

static int capture_send(const mbus_t mb, const uint8_t *data, const uint16_t size) {
  (void)mb;
  if (size > sizeof(g_last_tx)) {
    return -1;
  }
  memcpy(g_last_tx, data, size);
  g_last_tx_len = size;
  return (int)size;
}

static int io_send(const mbus_t mb, const uint8_t *data, const uint16_t size) {
  (void)mb;
#ifdef _WIN32
  if (g_serial != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    if (!WriteFile(g_serial, data, size, &written, NULL) || written != size) {
      return -1;
    }
    return (int)size;
  }
  if (fwrite(data, 1, size, stdout) != size) {
    return -1;
  }
  (void)fflush(stdout);
  return (int)size;
#else
  const int fd = g_serial_fd >= 0 ? g_serial_fd : STDOUT_FILENO;
  size_t left = size;
  const uint8_t *p = data;
  while (left > 0) {
    const ssize_t w = write(fd, p, left);
    if (w <= 0) {
      return -1;
    }
    p += (size_t)w;
    left -= (size_t)w;
  }
  if (fd == STDOUT_FILENO) {
    (void)fflush(stdout);
  }
  return (int)size;
#endif
}

static uint16_t mbus_protocol_read(const uint32_t la) {
  uint16_t v;

  if (la >= 1u && la <= 8u) {
    v = (uint16_t)(dev_output_coils & (1u << (la - 1u)));
  } else if (la >= 10001u && la <= 10008u) {
    v = (uint16_t)(dev_output_coils & (1u << (la - 10001u)));
  } else if (la == 30001u) {
    v = dev_input_registers_30001;
  } else if (la >= 40001u && la <= 40001u + HOLDING_COUNT - 1u) {
    v = holding_registers_4000x[la - 40001u];
  } else {
    return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
  }

  if (g_log_reads) {
    (void)fprintf(g_log_reads, "R %lu %u\n", (unsigned long)la, (unsigned)v);
    (void)fflush(g_log_reads);
  }
  return v;
}

static uint16_t mbus_protocol_write(const uint32_t la, const uint16_t value) {
  dev_input_registers_30001++;

  if (la >= 1u && la <= 8u) {
    if (value) {
      dev_output_coils |= (uint8_t)(1u << (la - 1u));
    } else {
      dev_output_coils &= (uint8_t)~(1u << (la - 1u));
    }
    goto ok_out;
  }
  if (la >= 10001u && la <= 10008u) {
    return mbus_error(MBUS_RESPONSE_ILLEGAL_FUNCTION);
  }
  if (la == 30001u) {
    return mbus_error(MBUS_RESPONSE_ILLEGAL_FUNCTION);
  }
  if (la >= 40001u && la <= 40001u + HOLDING_COUNT - 1u) {
    holding_registers_4000x[la - 40001u] = value;
    goto ok_out;
  }
  return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);

ok_out:
  if (g_log_writes) {
    (void)fprintf(g_log_writes, "W %lu %u\n", (unsigned long)la, (unsigned)value);
    (void)fflush(g_log_writes);
  }
  return value;
}

static void append_crc(uint8_t *frame, uint16_t pdu_len, uint16_t *out_len) {
  const uint16_t crc = crc_run(frame, pdu_len);
  frame[pdu_len] = (uint8_t)(crc & 0xFFu);
  frame[pdu_len + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
  *out_len = (uint16_t)(pdu_len + 2u);
}

static void sample_regs_reset(void) {
  dev_output_coils = 0;
  dev_input_registers_30001 = 0;
  memset(holding_registers_4000x, 0, sizeof(holding_registers_4000x));
  holding_registers_4000x[0] = 0x1122u;
  holding_registers_4000x[1] = 0x3344u;
}

static mbus_t slave_open(uint8_t devaddr, int (*send_fn)(const mbus_t, const uint8_t *, const uint16_t)) {
  static uint8_t sendbuf[SEND_BUF_SZ];
  static uint8_t recvbuf[RECV_BUF_SZ];

  Modbus_Conf_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.devaddr = devaddr;
  cfg.coils = STMODBUS_COUNT_COILS;
  cfg.discrete = STMODBUS_COUNT_DISCRETE;
  cfg.device = NULL;
  cfg.send = send_fn;
  cfg.read = mbus_protocol_read;
  cfg.write = mbus_protocol_write;
  cfg.sendbuf = sendbuf;
  cfg.sendbuf_sz = sizeof(sendbuf);
  cfg.recvbuf = recvbuf;
  cfg.recvbuf_sz = sizeof(recvbuf);

  return mbus_open(&cfg);
}

static uint8_t slave_addr_resolve(const cli_opts *o, uint8_t positional) {
  if (o->addr_explicit) {
    return o->addr;
  }
  if (positional != 0) {
    return positional;
  }
  return 1;
}

static int run_selftest(const cli_opts *o) {
  sample_regs_reset();
  if (load_regs_from(o) != 0) {
    return 1;
  }
  if (logs_open(o) != 0) {
    return 1;
  }

  const uint8_t addr = slave_addr_resolve(o, 0);
  const mbus_t ctx = slave_open(addr, capture_send);
  if (ctx < 0) {
    fputs("mbus_open failed\n", stderr);
    log_streams_close();
    return 1;
  }

  uint8_t req[8];
  uint16_t req_len;
  req[0] = addr;
  req[1] = MBUS_FUNC_READ_REGS;
  req[2] = 0x00;
  req[3] = 0x00;
  req[4] = 0x00;
  req[5] = 0x02;
  append_crc(req, 6, &req_len);

  g_last_tx_len = 0;
  for (uint16_t i = 0; i < req_len; i++) {
    if (mbus_poll(ctx, req[i]) != MBUS_OK) {
      fprintf(stderr, "mbus_poll failed at byte %u\n", i);
      log_streams_close();
      return 1;
    }
  }

  if (g_last_tx_len != 9u) {
    fprintf(stderr, "unexpected response length %u (expected 9)\n", g_last_tx_len);
    log_streams_close();
    return 1;
  }
  const uint8_t expect_hdr[] = {addr, MBUS_FUNC_READ_REGS, 0x04};
  if (memcmp(g_last_tx, expect_hdr, sizeof(expect_hdr)) != 0) {
    fputs("response header mismatch\n", stderr);
    log_streams_close();
    return 1;
  }
  if (g_last_tx[3] != (uint8_t)(holding_registers_4000x[0] >> 8) ||
      g_last_tx[4] != (uint8_t)(holding_registers_4000x[0] & 0xFFu) ||
      g_last_tx[5] != (uint8_t)(holding_registers_4000x[1] >> 8) ||
      g_last_tx[6] != (uint8_t)(holding_registers_4000x[1] & 0xFFu)) {
    fputs("holding data mismatch vs --regs-from / defaults\n", stderr);
    log_streams_close();
    return 1;
  }
  if (crc_run(g_last_tx, g_last_tx_len) != 0u) {
    fputs("response CRC check failed\n", stderr);
    log_streams_close();
    return 1;
  }

  puts("stmodbus-cli: self-test OK (read holding regs x2)");
  log_streams_close();
  return 0;
}

static void set_binary_stdio(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
}

static int run_stdio_slave(const cli_opts *o, uint8_t positional_addr) {
  if (o->regs_path != NULL && strcmp(o->regs_path, "-") == 0) {
    fputs("stmodbus-cli: --regs-from - is incompatible with stdio (stdin is the RTU stream)\n", stderr);
    return 1;
  }
  sample_regs_reset();
  if (load_regs_from(o) != 0) {
    return 1;
  }
  if (logs_open(o) != 0) {
    return 1;
  }

  const uint8_t addr = slave_addr_resolve(o, positional_addr);
  set_binary_stdio();

  const mbus_t ctx = slave_open(addr, io_send);
  if (ctx < 0) {
    fputs("mbus_open failed\n", stderr);
    log_streams_close();
    return 1;
  }

  for (;;) {
    unsigned char b;
    const size_t n = fread(&b, 1, 1, stdin);
    if (n != 1) {
      break;
    }
    (void)mbus_poll(ctx, b);
  }
  log_streams_close();
  return 0;
}

#ifndef _WIN32
static speed_t baud_to_constant(unsigned baud) {
  switch (baud) {
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
    default:
      return B0;
  }
}

static int run_tty_slave(const char *path, unsigned baud, const cli_opts *o, uint8_t positional_addr) {
  sample_regs_reset();
  if (load_regs_from(o) != 0) {
    return 1;
  }
  if (logs_open(o) != 0) {
    return 1;
  }

  const int fd = open(path, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    fprintf(stderr, "open %s: %s\n", path, strerror(errno));
    log_streams_close();
    return 1;
  }

  struct termios tio;
  if (tcgetattr(fd, &tio) != 0) {
    fprintf(stderr, "tcgetattr: %s\n", strerror(errno));
    (void)close(fd);
    log_streams_close();
    return 1;
  }
  cfmakeraw(&tio);
  const speed_t spd = baud_to_constant(baud);
  if (spd == B0) {
    fprintf(stderr, "unsupported baud %u (try 9600, 19200, 38400, 57600, 115200, 230400)\n", baud);
    (void)close(fd);
    log_streams_close();
    return 1;
  }
  (void)cfsetispeed(&tio, spd);
  (void)cfsetospeed(&tio, spd);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~(size_t)PARENB;
  tio.c_cflag &= ~(size_t)CSTOPB;
#ifdef CSIZE
  tio.c_cflag &= ~(size_t)CSIZE;
  tio.c_cflag |= CS8;
#endif
  if (tcsetattr(fd, TCSANOW, &tio) != 0) {
    fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
    (void)close(fd);
    log_streams_close();
    return 1;
  }

  g_serial_fd = fd;
  const uint8_t addr = slave_addr_resolve(o, positional_addr);
  const mbus_t ctx = slave_open(addr, io_send);
  if (ctx < 0) {
    fputs("mbus_open failed\n", stderr);
    (void)close(fd);
    g_serial_fd = -1;
    log_streams_close();
    return 1;
  }

  for (;;) {
    unsigned char b;
    const ssize_t n = read(fd, &b, 1);
    if (n == 1) {
      (void)mbus_poll(ctx, b);
    } else if (n == 0) {
      break;
    } else if (errno == EAGAIN || errno == EINTR) {
      continue;
    } else {
      break;
    }
  }

  g_serial_fd = -1;
  (void)close(fd);
  log_streams_close();
  return 0;
}
#else
static int run_tty_slave(const char *path, unsigned baud, const cli_opts *o, uint8_t positional_addr) {
  sample_regs_reset();
  if (load_regs_from(o) != 0) {
    return 1;
  }
  if (logs_open(o) != 0) {
    return 1;
  }

  char win_path[MAX_PATH];
  if (strncmp(path, "\\\\.\\", 4) != 0 && strncmp(path, "//./", 4) != 0) {
    (void)snprintf(win_path, sizeof(win_path), "\\\\.\\%s", path);
  } else {
    (void)strncpy(win_path, path, sizeof(win_path) - 1);
    win_path[sizeof(win_path) - 1] = '\0';
  }

  g_serial = CreateFileA(win_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (g_serial == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "CreateFileA %s failed\n", win_path);
    log_streams_close();
    return 1;
  }

  DCB dcb;
  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(g_serial, &dcb)) {
    fputs("GetCommState failed\n", stderr);
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
    log_streams_close();
    return 1;
  }
  dcb.BaudRate = (DWORD)baud;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  if (!SetCommState(g_serial, &dcb)) {
    fputs("SetCommState failed\n", stderr);
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
    log_streams_close();
    return 1;
  }

  COMMTIMEOUTS timeouts;
  memset(&timeouts, 0, sizeof(timeouts));
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;
  (void)SetCommTimeouts(g_serial, &timeouts);

  const uint8_t addr = slave_addr_resolve(o, positional_addr);
  const mbus_t ctx = slave_open(addr, io_send);
  if (ctx < 0) {
    fputs("mbus_open failed\n", stderr);
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
    log_streams_close();
    return 1;
  }

  for (;;) {
    unsigned char b;
    DWORD got = 0;
    if (!ReadFile(g_serial, &b, 1, &got, NULL) || got != 1) {
      break;
    }
    (void)mbus_poll(ctx, b);
  }

  CloseHandle(g_serial);
  g_serial = INVALID_HANDLE_VALUE;
  log_streams_close();
  return 0;
}
#endif

static void usage(void) {
  fputs(
      "stmodbus-cli — internal Modbus RTU slave harness (sample register map)\n"
      "\n"
      "  stmodbus-cli [selftest] [options...]\n"
      "  stmodbus-cli stdio [options...] [addr]\n"
#ifndef _WIN32
      "  stmodbus-cli tty <device> [baud] [addr] [options...]\n"
#else
      "  stmodbus-cli tty <COMn> [baud] [addr] [options...]\n"
#endif
      "\n"
      "Options (any order after subcommand):\n"
      "  --addr <1-247>           Modbus slave id (overrides trailing [addr] on stdio/tty)\n"
      "  --regs-from <path>       Load holding 40001..40008 before running\n"
      "  --regs-type bin|hex      bin: raw big-endian uint16 stream (default)\n"
      "                           hex: text — decimal or 0x hex, whitespace/# comments\n"
      "  --log-reads <path>       Append \"R <la> <value>\\n\" on each logical read\n"
      "  --log-writes <path>      Append \"W <la> <value>\\n\" on each logical write\n"
      "                           path \"-\" means stderr (avoids corrupting binary stdout)\n"
      "\n"
      "Defaults: addr 1; holding 40001/40002 = 0x1122 / 0x3344 unless --regs-from is set.\n",
      stderr);
}

static int parse_u8(const char *s, uint8_t *out) {
  char *end = NULL;
  const unsigned long v = strtoul(s, &end, 10);
  if (end == s || *end != '\0' || v < 1 || v > 247) {
    return -1;
  }
  *out = (uint8_t)v;
  return 0;
}

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/**
 * Parse flags from argv[start..argc-1]. On success, fills `out_pos` with non-flag
 * tokens in order (pointers into argv); returns count via *out_pos_n.
 */
static int parse_opts_flags(int argc, char **argv, int start, cli_opts *o, char **out_pos, int *out_pos_n,
                            int max_pos) {
  memset(o, 0, sizeof(*o));
  o->addr = 1;
  o->regs_type = REGS_TYPE_BIN;
  int npos = 0;

  for (int i = start; i < argc; i++) {
    if (streq(argv[i], "--addr")) {
      if (i + 1 >= argc || parse_u8(argv[i + 1], &o->addr) != 0) {
        fputs("stmodbus-cli: --addr needs value 1-247\n", stderr);
        return -1;
      }
      o->addr_explicit = 1;
      i++;
      continue;
    }
    if (streq(argv[i], "--regs-from")) {
      if (i + 1 >= argc) {
        fputs("stmodbus-cli: --regs-from needs a path\n", stderr);
        return -1;
      }
      o->regs_path = argv[++i];
      continue;
    }
    if (streq(argv[i], "--regs-type")) {
      if (i + 1 >= argc) {
        fputs("stmodbus-cli: --regs-type needs bin|hex\n", stderr);
        return -1;
      }
      i++;
      if (streq(argv[i], "bin")) {
        o->regs_type = REGS_TYPE_BIN;
      } else if (streq(argv[i], "hex")) {
        o->regs_type = REGS_TYPE_HEX;
      } else {
        fputs("stmodbus-cli: --regs-type must be bin or hex\n", stderr);
        return -1;
      }
      continue;
    }
    if (streq(argv[i], "--log-reads")) {
      if (i + 1 >= argc) {
        fputs("stmodbus-cli: --log-reads needs a path\n", stderr);
        return -1;
      }
      o->log_reads_path = argv[++i];
      continue;
    }
    if (streq(argv[i], "--log-writes")) {
      if (i + 1 >= argc) {
        fputs("stmodbus-cli: --log-writes needs a path\n", stderr);
        return -1;
      }
      o->log_writes_path = argv[++i];
      continue;
    }
    if (argv[i][0] == '-') {
      fprintf(stderr, "stmodbus-cli: unknown option %s\n", argv[i]);
      return -1;
    }
    if (npos >= max_pos) {
      fputs("stmodbus-cli: too many positional arguments\n", stderr);
      return -1;
    }
    out_pos[npos++] = argv[i];
  }
  *out_pos_n = npos;
  return 0;
}

int main(int argc, char **argv) {
  const char *cmd = "selftest";
  if (argc >= 2) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      usage();
      return 0;
    }
    cmd = argv[1];
  }

  char *pos[8];
  int npos = 0;
  cli_opts opts;

  if (strcmp(cmd, "selftest") == 0 || strcmp(cmd, "test") == 0) {
    if (parse_opts_flags(argc, argv, 2, &opts, pos, &npos, 8) != 0) {
      return 1;
    }
    if (npos != 0) {
      fputs("stmodbus-cli: selftest takes no positional arguments\n", stderr);
      return 1;
    }
    return run_selftest(&opts);
  }

  if (strcmp(cmd, "stdio") == 0) {
    if (parse_opts_flags(argc, argv, 2, &opts, pos, &npos, 8) != 0) {
      return 1;
    }
    uint8_t legacy_addr = 0;
    if (npos >= 1) {
      if (parse_u8(pos[0], &legacy_addr) != 0) {
        fputs("stmodbus-cli: invalid trailing slave address (use --addr)\n", stderr);
        return 1;
      }
      if (npos > 1) {
        fputs("stmodbus-cli: stdio allows at most one positional address\n", stderr);
        return 1;
      }
    }
    return run_stdio_slave(&opts, legacy_addr);
  }

  if (strcmp(cmd, "tty") == 0) {
    if (parse_opts_flags(argc, argv, 2, &opts, pos, &npos, 8) != 0) {
      return 1;
    }
    if (npos < 1) {
      usage();
      return 1;
    }
    const char *path = pos[0];
    unsigned baud = 115200;
    uint8_t legacy_addr = 0;
    if (npos >= 2) {
      char *end = NULL;
      const unsigned long b = strtoul(pos[1], &end, 10);
      if (end == pos[1] || *end != '\0' || b == 0) {
        fputs("stmodbus-cli: invalid baud\n", stderr);
        return 1;
      }
      baud = (unsigned)b;
    }
    if (npos >= 3 && parse_u8(pos[2], &legacy_addr) != 0) {
      fputs("stmodbus-cli: invalid slave address\n", stderr);
      return 1;
    }
    if (npos > 3) {
      fputs("stmodbus-cli: tty: too many positional arguments\n", stderr);
      return 1;
    }
    return run_tty_slave(path, baud, &opts, legacy_addr);
  }

  usage();
  return 1;
}
