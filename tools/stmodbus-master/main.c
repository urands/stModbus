/**
 * stmodbus-master — Modbus RTU master CLI for scripts and AI agents.
 *
 * The stModbus library is a slave stack only; this program implements a
 * minimal RTU master (frame build/parse) and links the library for CRC
 * (mbus_crc16).
 */

#include <modbus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#define MAX_PDU_READ_REGS 64
#define SERIAL_BUF 512

#ifdef _WIN32
static HANDLE g_serial = INVALID_HANDLE_VALUE;
#else
static int g_serial_fd = -1;
#endif

static uint16_t crc16_rtu(const uint8_t *data, size_t len) {
  uint16_t c = 0xFFFFu;
  for (size_t i = 0; i < len; i++) {
    c = mbus_crc16(c, data[i]);
  }
  return c;
}

static void append_crc(uint8_t *pdu, size_t pdu_len) {
  const uint16_t crc = crc16_rtu(pdu, pdu_len);
  pdu[pdu_len] = (uint8_t)(crc & 0xFFu);
  pdu[pdu_len + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
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

static int serial_open(const char *path, unsigned baud) {
  const int fd = open(path, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    fprintf(stderr, "stmodbus-master: open %s: %s\n", path, strerror(errno));
    return -1;
  }
  struct termios tio;
  if (tcgetattr(fd, &tio) != 0) {
    fprintf(stderr, "stmodbus-master: tcgetattr: %s\n", strerror(errno));
    (void)close(fd);
    return -1;
  }
  cfmakeraw(&tio);
  const speed_t spd = baud_to_constant(baud);
  if (spd == B0) {
    fputs("stmodbus-master: unsupported baud\n", stderr);
    (void)close(fd);
    return -1;
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
    fprintf(stderr, "stmodbus-master: tcsetattr: %s\n", strerror(errno));
    (void)close(fd);
    return -1;
  }
  (void)tcflush(fd, TCIOFLUSH);
  g_serial_fd = fd;
  return 0;
}

static void serial_close(void) {
  if (g_serial_fd >= 0) {
    (void)close(g_serial_fd);
    g_serial_fd = -1;
  }
}

static int serial_write(const uint8_t *buf, size_t len) {
  size_t left = len;
  const uint8_t *p = buf;
  while (left > 0) {
    const ssize_t w = write(g_serial_fd, p, left);
    if (w <= 0) {
      return -1;
    }
    p += (size_t)w;
    left -= (size_t)w;
  }
  return 0;
}

static int serial_read_byte(uint8_t *b, int timeout_ms) {
  struct pollfd pfd;
  pfd.fd = g_serial_fd;
  pfd.events = POLLIN;
  const int pr = poll(&pfd, 1, timeout_ms);
  if (pr <= 0) {
    return pr < 0 ? -2 : -1;
  }
  const ssize_t n = read(g_serial_fd, b, 1);
  if (n != 1) {
    return -2;
  }
  return 0;
}
#else
static int serial_open(const char *path, unsigned baud) {
  char win_path[MAX_PATH];
  if (strncmp(path, "\\\\.\\", 4) != 0 && strncmp(path, "//./", 4) != 0) {
    (void)snprintf(win_path, sizeof(win_path), "\\\\.\\%s", path);
  } else {
    (void)strncpy(win_path, path, sizeof(win_path) - 1);
    win_path[sizeof(win_path) - 1] = '\0';
  }
  g_serial = CreateFileA(win_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (g_serial == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "stmodbus-master: CreateFileA %s failed\n", win_path);
    return -1;
  }
  DCB dcb;
  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(g_serial, &dcb)) {
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
    return -1;
  }
  dcb.BaudRate = (DWORD)baud;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  if (!SetCommState(g_serial, &dcb)) {
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
    return -1;
  }
  COMMTIMEOUTS timeouts;
  memset(&timeouts, 0, sizeof(timeouts));
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;
  (void)SetCommTimeouts(g_serial, &timeouts);
  PurgeComm(g_serial, PURGE_RXCLEAR | PURGE_TXCLEAR);
  return 0;
}

static void serial_close(void) {
  if (g_serial != INVALID_HANDLE_VALUE) {
    CloseHandle(g_serial);
    g_serial = INVALID_HANDLE_VALUE;
  }
}

static int serial_write(const uint8_t *buf, size_t len) {
  size_t left = len;
  const uint8_t *p = buf;
  while (left > 0) {
    DWORD w = 0;
    if (!WriteFile(g_serial, p, (DWORD)left, &w, NULL) || w == 0) {
      return -1;
    }
    p += w;
    left -= w;
  }
  return 0;
}

static int serial_read_byte(uint8_t *b, int timeout_ms) {
  DWORD got = 0;
  COMMTIMEOUTS t0, t1;
  memset(&t0, 0, sizeof(t0));
  t0.ReadTotalTimeoutConstant = (DWORD)timeout_ms;
  t0.ReadIntervalTimeout = 0;
  t0.ReadTotalTimeoutMultiplier = 0;
  (void)SetCommTimeouts(g_serial, &t0);
  if (!ReadFile(g_serial, b, 1, &got, NULL) || got != 1) {
    memset(&t1, 0, sizeof(t1));
    t1.ReadIntervalTimeout = MAXDWORD;
    (void)SetCommTimeouts(g_serial, &t1);
    return -1;
  }
  memset(&t1, 0, sizeof(t1));
  t1.ReadIntervalTimeout = MAXDWORD;
  (void)SetCommTimeouts(g_serial, &t1);
  return 0;
}
#endif

static void post_tx_delay_ms(unsigned ms) {
  if (ms == 0u) {
    return;
  }
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000u);
  ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
  (void)nanosleep(&ts, NULL);
#endif
}

static int expect_response_len(const uint8_t *buf, size_t len) {
  if (len < 2) {
    return -1;
  }
  if (buf[1] & 0x80u) {
    return 5;
  }
  switch (buf[1]) {
    case 1:
    case 2:
    case 3:
    case 4:
      if (len < 3) {
        return -1;
      }
      return 5 + (int)buf[2];
    case 5:
    case 6:
      return 8;
    default:
      return -1;
  }
}

/**
 * Reads one RTU frame into buf. Returns frame length on success, -1 timeout, -2 parse/CRC.
 */
static int read_rtu_frame(uint8_t *buf, size_t cap, int per_byte_timeout_ms) {
  size_t len = 0;
  for (;;) {
    uint8_t b;
    const int r = serial_read_byte(&b, per_byte_timeout_ms);
    if (r != 0) {
      return len >= 4 ? -2 : -1;
    }
    if (len >= cap) {
      return -2;
    }
    buf[len++] = b;
    if (len < 3u) {
      continue;
    }
    const int need = expect_response_len(buf, len);
    if (need < 0) {
      if (len > 300u) {
        return -2;
      }
      continue;
    }
    if ((size_t)need > cap) {
      return -2;
    }
    if (len >= (size_t)need) {
      if (crc16_rtu(buf, (size_t)need) == 0u) {
        return need;
      }
      return -2;
    }
  }
}

typedef enum { OUT_JSON = 0, OUT_TEXT = 1 } outfmt_t;

static void out_ok_read_regs(outfmt_t fmt, const char *cmd, const uint16_t *vals, size_t n) {
  if (fmt == OUT_TEXT) {
    printf("%s:", cmd);
    for (size_t i = 0; i < n; i++) {
      printf(" %u", (unsigned)vals[i]);
    }
    putchar('\n');
    return;
  }
  printf("{\"ok\":true,\"cmd\":\"%s\",\"count\":%zu,\"values\":[", cmd, n);
  for (size_t i = 0; i < n; i++) {
    printf("%s%u", i ? "," : "", (unsigned)vals[i]);
  }
  printf("]}\n");
}

static void out_ok_write(outfmt_t fmt, const char *cmd) {
  if (fmt == OUT_TEXT) {
    printf("%s: OK\n", cmd);
    return;
  }
  printf("{\"ok\":true,\"cmd\":\"%s\"}\n", cmd);
}

static void out_err(outfmt_t fmt, const char *code, int excode) {
  if (fmt == OUT_TEXT) {
    if (excode >= 0) {
      fprintf(stderr, "error: %s (exception %d)\n", code, excode);
    } else {
      fprintf(stderr, "error: %s\n", code);
    }
    return;
  }
  if (excode >= 0) {
    printf("{\"ok\":false,\"error\":\"%s\",\"exception\":%d}\n", code, excode);
  } else {
    printf("{\"ok\":false,\"error\":\"%s\"}\n", code);
  }
}

static int mb_to_pdu_holding(uint32_t mb, uint16_t *pdu) {
  if (mb < 40001u || mb > 49999u) {
    return -1;
  }
  *pdu = (uint16_t)(mb - 40001u);
  return 0;
}

static int mb_to_pdu_input(uint32_t mb, uint16_t *pdu) {
  if (mb < 30001u || mb > 39999u) {
    return -1;
  }
  *pdu = (uint16_t)(mb - 30001u);
  return 0;
}

static int mb_to_pdu_coil(uint32_t mb, uint16_t *pdu) {
  if (mb < 1u || mb > 9999u) {
    return -1;
  }
  *pdu = (uint16_t)(mb - 1u);
  return 0;
}

static int do_read_holding(uint8_t unit, uint32_t mb, uint16_t count, int timeout_ms, unsigned post_tx_ms,
                           outfmt_t fmt) {
  uint16_t pdu0;
  if (mb_to_pdu_holding(mb, &pdu0) != 0 || count == 0 || count > MAX_PDU_READ_REGS) {
    out_err(fmt, "bad_mb_address_or_count", -1);
    return 1;
  }
  uint8_t req[8];
  req[0] = unit;
  req[1] = 3;
  req[2] = (uint8_t)(pdu0 >> 8);
  req[3] = (uint8_t)(pdu0 & 0xFFu);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFFu);
  append_crc(req, 6);

  if (serial_write(req, sizeof(req)) != 0) {
    out_err(fmt, "write_failed", -1);
    return 1;
  }
  post_tx_delay_ms(post_tx_ms);

  uint8_t resp[SERIAL_BUF];
  const int flen = read_rtu_frame(resp, sizeof(resp), timeout_ms);
  if (flen < 0) {
    out_err(fmt, flen == -1 ? "timeout" : "bad_response", -1);
    return 1;
  }
  if (resp[0] != unit) {
    out_err(fmt, "unit_mismatch", -1);
    return 1;
  }
  if (resp[1] == (uint8_t)(0x80u | 3u)) {
    out_err(fmt, "modbus_exception", (int)resp[2]);
    return 1;
  }
  if (resp[1] != 3u || (size_t)flen != 5u + (size_t)resp[2] || resp[2] != (uint8_t)(count * 2u)) {
    out_err(fmt, "unexpected_response", -1);
    return 1;
  }
  uint16_t vals[MAX_PDU_READ_REGS];
  for (uint16_t i = 0; i < count; i++) {
    vals[i] = (uint16_t)(((uint16_t)resp[3u + (size_t)i * 2u] << 8) | resp[3u + (size_t)i * 2u + 1u]);
  }
  out_ok_read_regs(fmt, "read_holding", vals, count);
  return 0;
}

static int do_read_input(uint8_t unit, uint32_t mb, uint16_t count, int timeout_ms, unsigned post_tx_ms,
                         outfmt_t fmt) {
  uint16_t pdu0;
  if (mb_to_pdu_input(mb, &pdu0) != 0 || count == 0 || count > MAX_PDU_READ_REGS) {
    out_err(fmt, "bad_mb_address_or_count", -1);
    return 1;
  }
  uint8_t req[8];
  req[0] = unit;
  req[1] = 4;
  req[2] = (uint8_t)(pdu0 >> 8);
  req[3] = (uint8_t)(pdu0 & 0xFFu);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFFu);
  append_crc(req, 6);
  if (serial_write(req, sizeof(req)) != 0) {
    out_err(fmt, "write_failed", -1);
    return 1;
  }
  post_tx_delay_ms(post_tx_ms);
  uint8_t resp[SERIAL_BUF];
  const int flen = read_rtu_frame(resp, sizeof(resp), timeout_ms);
  if (flen < 0) {
    out_err(fmt, flen == -1 ? "timeout" : "bad_response", -1);
    return 1;
  }
  if (resp[0] != unit) {
    out_err(fmt, "unit_mismatch", -1);
    return 1;
  }
  if (resp[1] == (uint8_t)(0x80u | 4u)) {
    out_err(fmt, "modbus_exception", (int)resp[2]);
    return 1;
  }
  if (resp[1] != 4u || (size_t)flen != 5u + (size_t)resp[2] || resp[2] != (uint8_t)(count * 2u)) {
    out_err(fmt, "unexpected_response", -1);
    return 1;
  }
  uint16_t vals[MAX_PDU_READ_REGS];
  for (uint16_t i = 0; i < count; i++) {
    vals[i] = (uint16_t)(((uint16_t)resp[3u + (size_t)i * 2u] << 8) | resp[3u + (size_t)i * 2u + 1u]);
  }
  out_ok_read_regs(fmt, "read_input", vals, count);
  return 0;
}

static int do_read_coils(uint8_t unit, uint32_t mb, uint16_t count, int timeout_ms, unsigned post_tx_ms,
                         outfmt_t fmt) {
  uint16_t pdu0;
  if (mb_to_pdu_coil(mb, &pdu0) != 0 || count == 0 || count > 2000) {
    out_err(fmt, "bad_mb_address_or_count", -1);
    return 1;
  }
  uint8_t req[8];
  req[0] = unit;
  req[1] = 1;
  req[2] = (uint8_t)(pdu0 >> 8);
  req[3] = (uint8_t)(pdu0 & 0xFFu);
  req[4] = (uint8_t)(count >> 8);
  req[5] = (uint8_t)(count & 0xFFu);
  append_crc(req, 6);
  if (serial_write(req, sizeof(req)) != 0) {
    out_err(fmt, "write_failed", -1);
    return 1;
  }
  post_tx_delay_ms(post_tx_ms);
  uint8_t resp[SERIAL_BUF];
  const int flen = read_rtu_frame(resp, sizeof(resp), timeout_ms);
  if (flen < 0) {
    out_err(fmt, flen == -1 ? "timeout" : "bad_response", -1);
    return 1;
  }
  if (resp[0] != unit) {
    out_err(fmt, "unit_mismatch", -1);
    return 1;
  }
  if (resp[1] == (uint8_t)(0x80u | 1u)) {
    out_err(fmt, "modbus_exception", (int)resp[2]);
    return 1;
  }
  const int nbytes = (int)resp[2];
  if (resp[1] != 1u || (size_t)flen != 5u + (size_t)nbytes) {
    out_err(fmt, "unexpected_response", -1);
    return 1;
  }
  if (fmt == OUT_TEXT) {
    printf("read_coils:");
    for (uint16_t i = 0; i < count; i++) {
      const uint8_t bit = (uint8_t)((resp[3u + (size_t)(i / 8u)] >> (i % 8u)) & 1u);
      printf(" %u", (unsigned)bit);
    }
    putchar('\n');
    return 0;
  }
  printf("{\"ok\":true,\"cmd\":\"read_coils\",\"count\":%u,\"bits\":[", (unsigned)count);
  for (uint16_t i = 0; i < count; i++) {
    const uint8_t bit = (uint8_t)((resp[3u + (size_t)(i / 8u)] >> (i % 8u)) & 1u);
    printf("%s%u", i ? "," : "", (unsigned)bit);
  }
  printf("]}\n");
  return 0;
}

static int do_write_holding(uint8_t unit, uint32_t mb, uint16_t value, int timeout_ms, unsigned post_tx_ms,
                            outfmt_t fmt) {
  uint16_t pdu0;
  if (mb_to_pdu_holding(mb, &pdu0) != 0) {
    out_err(fmt, "bad_mb_address", -1);
    return 1;
  }
  uint8_t req[8];
  req[0] = unit;
  req[1] = 6;
  req[2] = (uint8_t)(pdu0 >> 8);
  req[3] = (uint8_t)(pdu0 & 0xFFu);
  req[4] = (uint8_t)(value >> 8);
  req[5] = (uint8_t)(value & 0xFFu);
  append_crc(req, 6);
  if (serial_write(req, sizeof(req)) != 0) {
    out_err(fmt, "write_failed", -1);
    return 1;
  }
  post_tx_delay_ms(post_tx_ms);
  uint8_t resp[SERIAL_BUF];
  const int flen = read_rtu_frame(resp, sizeof(resp), timeout_ms);
  if (flen < 0) {
    out_err(fmt, flen == -1 ? "timeout" : "bad_response", -1);
    return 1;
  }
  if (resp[0] != unit) {
    out_err(fmt, "unit_mismatch", -1);
    return 1;
  }
  if (resp[1] == (uint8_t)(0x80u | 6u)) {
    out_err(fmt, "modbus_exception", (int)resp[2]);
    return 1;
  }
  if (flen != 8 || memcmp(resp, req, 6) != 0) {
    out_err(fmt, "unexpected_response", -1);
    return 1;
  }
  out_ok_write(fmt, "write_holding");
  return 0;
}

static int do_write_coil(uint8_t unit, uint32_t mb, int on, int timeout_ms, unsigned post_tx_ms, outfmt_t fmt) {
  uint16_t pdu0;
  if (mb_to_pdu_coil(mb, &pdu0) != 0) {
    out_err(fmt, "bad_mb_address", -1);
    return 1;
  }
  const uint16_t coilv = on ? 0xFF00u : 0u;
  uint8_t req[8];
  req[0] = unit;
  req[1] = 5;
  req[2] = (uint8_t)(pdu0 >> 8);
  req[3] = (uint8_t)(pdu0 & 0xFFu);
  req[4] = (uint8_t)(coilv >> 8);
  req[5] = (uint8_t)(coilv & 0xFFu);
  append_crc(req, 6);
  if (serial_write(req, sizeof(req)) != 0) {
    out_err(fmt, "write_failed", -1);
    return 1;
  }
  post_tx_delay_ms(post_tx_ms);
  uint8_t resp[SERIAL_BUF];
  const int flen = read_rtu_frame(resp, sizeof(resp), timeout_ms);
  if (flen < 0) {
    out_err(fmt, flen == -1 ? "timeout" : "bad_response", -1);
    return 1;
  }
  if (resp[0] != unit) {
    out_err(fmt, "unit_mismatch", -1);
    return 1;
  }
  if (resp[1] == (uint8_t)(0x80u | 5u)) {
    out_err(fmt, "modbus_exception", (int)resp[2]);
    return 1;
  }
  if (flen != 8 || memcmp(resp, req, 6) != 0) {
    out_err(fmt, "unexpected_response", -1);
    return 1;
  }
  out_ok_write(fmt, "write_coil");
  return 0;
}

typedef struct {
  const char *tty;
  unsigned baud;
  uint8_t unit;
  uint32_t mb;
  uint16_t count;
  uint16_t value;
  int coil_on;
  int timeout_ms;
  unsigned post_tx_ms;
  outfmt_t fmt;
  int has_value;
  int has_coil;
} master_args_t;

static void usage(void) {
  fputs(
      "stmodbus-master — Modbus RTU master (stModbus lib used for CRC only; slave stack not used)\n"
      "\n"
      "Global flags (any order; subcommand name can appear anywhere among args):\n"
      "  --tty <path>         Serial device (required), e.g. /dev/ttyUSB0 or COM3\n"
      "  --baud <n>           Default 115200 (9600, 19200, 38400, 57600, 115200, 230400)\n"
      "  --unit <1-247>       Default 1\n"
      "  --timeout-ms <n>     Per-byte read timeout / inter-char; default 800\n"
      "  --post-tx-ms <n>     Pause after TX before reading; default 15\n"
      "  --format json|text   Default json (one line stdout; errors as JSON with ok:false)\n"
      "\n"
      "Subcommands (Modbus logical addresses):\n"
      "  read-holding   --mb-address <40001-49999> --count <1-64>\n"
      "  read-input     --mb-address <30001-39999> --count <1-64>\n"
      "  read-coils     --mb-address <1-9999> --count <1-2000>\n"
      "  write-holding  --mb-address <40001-49999> --value <0-65535>\n"
      "  write-coil     --mb-address <1-9999> --on 0|1\n"
      "\n",
      stderr);
}

static const char *find_cmd(int argc, char **argv) {
  static const char *const names[] = {"read-holding", "read-input", "read-coils", "write-holding", "write-coil", NULL};
  for (int i = 1; i < argc; i++) {
    for (int j = 0; names[j] != NULL; j++) {
      if (strcmp(argv[i], names[j]) == 0) {
        return names[j];
      }
    }
  }
  return NULL;
}

static int parse_args(int argc, char **argv, const char *cmd, master_args_t *a) {
  memset(a, 0, sizeof(*a));
  a->baud = 115200;
  a->unit = 1;
  a->timeout_ms = 800;
  a->post_tx_ms = 15;
  a->fmt = OUT_JSON;
  a->coil_on = -1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], cmd) == 0) {
      continue;
    }
    if (strcmp(argv[i], "--tty") == 0 && i + 1 < argc) {
      a->tty = argv[++i];
    } else if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
      a->baud = (unsigned)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
      const unsigned long u = strtoul(argv[++i], NULL, 10);
      if (u < 1 || u > 247) {
        return -1;
      }
      a->unit = (uint8_t)u;
    } else if (strcmp(argv[i], "--mb-address") == 0 && i + 1 < argc) {
      a->mb = (uint32_t)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
      a->count = (uint16_t)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc) {
      a->value = (uint16_t)strtoul(argv[++i], NULL, 10);
      a->has_value = 1;
    } else if (strcmp(argv[i], "--on") == 0 && i + 1 < argc) {
      a->coil_on = (int)strtoul(argv[++i], NULL, 10);
      a->has_coil = 1;
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      a->timeout_ms = (int)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--post-tx-ms") == 0 && i + 1 < argc) {
      a->post_tx_ms = (unsigned)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
      i++;
      if (strcmp(argv[i], "json") == 0) {
        a->fmt = OUT_JSON;
      } else if (strcmp(argv[i], "text") == 0) {
        a->fmt = OUT_TEXT;
      } else {
        return -1;
      }
    } else {
      fprintf(stderr, "stmodbus-master: unknown argument %s\n", argv[i]);
      return -1;
    }
  }
  if (a->tty == NULL) {
    fputs("stmodbus-master: --tty is required\n", stderr);
    return -1;
  }
  if (strcmp(cmd, "write-holding") == 0) {
    if (!a->has_value) {
      fputs("stmodbus-master: write-holding needs --value\n", stderr);
      return -1;
    }
  }
  if (strcmp(cmd, "write-coil") == 0) {
    if (!a->has_coil || (a->coil_on != 0 && a->coil_on != 1)) {
      fputs("stmodbus-master: write-coil needs --on 0 or 1\n", stderr);
      return -1;
    }
  }
  if (strncmp(cmd, "read-", 5) == 0 && a->count == 0) {
    fputs("stmodbus-master: read-* needs --count\n", stderr);
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage();
    return argc < 2 ? 1 : 0;
  }

  const char *cmd = find_cmd(argc, argv);
  if (cmd == NULL) {
    usage();
    return 1;
  }
  master_args_t a;
  if (parse_args(argc, argv, cmd, &a) != 0) {
    return 1;
  }

  if (serial_open(a.tty, a.baud) != 0) {
    return 1;
  }

  int rc = 1;
  if (strcmp(cmd, "read-holding") == 0) {
    rc = do_read_holding(a.unit, a.mb, a.count, a.timeout_ms, a.post_tx_ms, a.fmt);
  } else if (strcmp(cmd, "read-input") == 0) {
    rc = do_read_input(a.unit, a.mb, a.count, a.timeout_ms, a.post_tx_ms, a.fmt);
  } else if (strcmp(cmd, "read-coils") == 0) {
    rc = do_read_coils(a.unit, a.mb, a.count, a.timeout_ms, a.post_tx_ms, a.fmt);
  } else if (strcmp(cmd, "write-holding") == 0) {
    rc = do_write_holding(a.unit, a.mb, a.value, a.timeout_ms, a.post_tx_ms, a.fmt);
  } else if (strcmp(cmd, "write-coil") == 0) {
    rc = do_write_coil(a.unit, a.mb, a.coil_on, a.timeout_ms, a.post_tx_ms, a.fmt);
  } else {
    usage();
    rc = 1;
  }

  serial_close();
  return rc;
}
