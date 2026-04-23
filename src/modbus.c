/**
 * stModbus Libary: A tiny Modbus implementation for Modbus ASCII/RTU
 * Copyright (c) 2017+ [Iurii Bell] ds@inbox.ru (http://www.bel-tech.ru/)
 * All rights reserved.
 *
 * [MIT License](http://www.opensource.org/licenses/mit-license.php)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @copyright   Copyright (C) 2017+
 * @author      Iurii Bell (ds@inbox.ru)
 * @license     MIT
 * @package     stModbus
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "modbus.h"

#include <stdio.h>
#include <string.h>

// Global variable for modbus context
_stmodbus_context_t g_mbusContext[STMODBUS_COUNT_CONTEXT];
Modbus_ResponseType    g_userError = MBUS_RESPONSE_OK;

void lock() {}

void unlock() {}

/*
 * function mbus_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
 */
mbus_t mbus_open(Modbus_Conf_t *pconf) {
  mbus_t context;
  for (context = 0; context < STMODBUS_COUNT_CONTEXT; context++) {
    if (g_mbusContext[context].open == 0) {
      break;
    }
  }
  if (context == STMODBUS_COUNT_CONTEXT)
    return (mbus_t)MBUS_ERROR;
  // Clear context
  memset(&g_mbusContext[context], 0, sizeof(_stmodbus_context_t));
  // Copy config to context
  memcpy((void *)&g_mbusContext[context].conf, (void *)pconf,
         sizeof(Modbus_Conf_t));

  g_mbusContext[context].open = 1;
  return context;
}

mbus_status_t mbus_flush(const mbus_t context) {
  g_mbusContext[context].crc16 = 0xFFFF;
  g_mbusContext[context].state = MBUS_STATE_IDLE;
  return MBUS_OK;
}

mbus_status_t mbus_response(mbus_t mb_context, Modbus_ResponseType response) {

  if (response != MBUS_RESPONSE_OK) {
    return mbus_send_error(mb_context, response);
  } else {
  }
  return MBUS_ERROR;
}

uint16_t mbus_error(Modbus_ResponseType error) {
    g_userError = error;
    return 0;
}

inline mbus_status_t mbus_poll_response(mbus_t mb_context) {
  stmbCallBackFunc func = 0;
  _stmodbus_context_t *ctx = &g_mbusContext[mb_context];
  int read = 1, la;

#if STMODBUS_COUNT_FUNC > 0
  for (int i = 0; i < STMODBUS_COUNT_FUNC; i++) {
    if ((ctx->func[i].code == ctx->header.func)) {
      func = ctx->func[i].func;
      break;
    }
  }
#endif

  /* Per-function quantity and address validation (spec-compliant caps). */
  switch (ctx->header.func) {
  case MBUS_FUNC_READ_COILS:
    if ((ctx->header.num == 0) || (ctx->header.num > 0x07D0)) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    if (((uint32_t)ctx->header.addr + ctx->header.num) > ctx->conf.coils) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
    }
    break;
  case MBUS_FUNC_READ_DISCRETE:
    if ((ctx->header.num == 0) || (ctx->header.num > 0x07D0)) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    if (((uint32_t)ctx->header.addr + ctx->header.num) > ctx->conf.discrete) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
    }
    break;
  case MBUS_FUNC_WRITE_COILS:
    if ((ctx->header.num == 0) || (ctx->header.num > 0x07B0)) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    if (((uint32_t)ctx->header.addr + ctx->header.num) > ctx->conf.coils) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
    }
    break;
  case MBUS_FUNC_READ_REGS:
  case MBUS_FUNC_READ_INPUT_REGS:
    if ((ctx->header.num == 0) || (ctx->header.num > 0x007D)) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    break;
  case MBUS_FUNC_WRITE_REGS:
    if ((ctx->header.num == 0) || (ctx->header.num > 0x007B)) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    break;
  default:
    break;
  }

  if (func != 0) {
    return func(mb_context);
  }

  la = mbus_proto_address((Modbus_ConnectFuncType)ctx->header.func, &read);
  if (la <= 0) {
    return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
  }
  la += ctx->header.addr;

  if (ctx->conf.sendbuf == 0) {
    return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
  }
  ctx->conf.sendbuf[0] = ctx->header.devaddr;
  ctx->conf.sendbuf[1] = ctx->header.func;

  switch (ctx->header.func) {
  case MBUS_FUNC_READ_COILS:
  case MBUS_FUNC_READ_DISCRETE: {
    if (ctx->conf.read == 0) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }
    const uint16_t bytecount = (uint16_t)((ctx->header.num + 7u) / 8u);
    /* sendbuf must hold: devaddr+func+bytecount+N data+CRC(2) */
    if (((uint32_t)3u + bytecount + 2u) > ctx->conf.sendbuf_sz) {
      return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
    }
    ctx->conf.sendbuf[2] = (uint8_t)bytecount;
    memset(ctx->conf.sendbuf + 3, 0, bytecount);
    g_userError = MBUS_RESPONSE_OK;
    for (uint16_t i = 0; i < ctx->header.num; i++) {
      const uint16_t v = ctx->conf.read((uint32_t)la + i);
      if (v != 0u) {
        ctx->conf.sendbuf[3 + (i >> 3)] |= (uint8_t)(1u << (i & 7u));
      }
    }
    if (g_userError != MBUS_RESPONSE_OK) {
      return mbus_response(mb_context, g_userError);
    }
    return mbus_send_data(mb_context, (uint16_t)(3u + bytecount));
  }

  case MBUS_FUNC_READ_REGS:
  case MBUS_FUNC_READ_INPUT_REGS: {
    if (ctx->conf.read == 0) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }
    const uint16_t bytecount = (uint16_t)(ctx->header.num * 2u);
    if (((uint32_t)3u + bytecount + 2u) > ctx->conf.sendbuf_sz) {
      return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
    }
    ctx->conf.sendbuf[2] = (uint8_t)bytecount;
    g_userError = MBUS_RESPONSE_OK;
    for (uint16_t i = 0; i < ctx->header.num; i++) {
      const uint16_t v = ctx->conf.read((uint32_t)la + i);
      ctx->conf.sendbuf[3 + (i << 1)] = (uint8_t)(v >> 8);
      ctx->conf.sendbuf[4 + (i << 1)] = (uint8_t)(v & 0xFFu);
    }
    if (g_userError != MBUS_RESPONSE_OK) {
      return mbus_response(mb_context, g_userError);
    }
    return mbus_send_data(mb_context, (uint16_t)(3u + bytecount));
  }

  case MBUS_FUNC_WRITE_REG:
  case MBUS_FUNC_WRITE_COIL: {
    if (ctx->conf.write == 0) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }
    if (ctx->conf.recvbuf == 0 || ctx->conf.recvbuf_sz < 2u ||
        ctx->conf.sendbuf_sz < 8u) {
      return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
    }
    /* FSM stored HI at recvbuf[1] and LO at recvbuf[0] (endian-safe combine). */
    const uint16_t value =
        (uint16_t)(((uint16_t)ctx->conf.recvbuf[1] << 8) | ctx->conf.recvbuf[0]);
    g_userError = MBUS_RESPONSE_OK;
    ctx->conf.write((uint32_t)la, value);
    if (g_userError != MBUS_RESPONSE_OK) {
      return mbus_response(mb_context, g_userError);
    }
    ctx->conf.sendbuf[2] = (uint8_t)(ctx->header.addr >> 8);
    ctx->conf.sendbuf[3] = (uint8_t)(ctx->header.addr & 0xFFu);
    ctx->conf.sendbuf[4] = ctx->conf.recvbuf[1];
    ctx->conf.sendbuf[5] = ctx->conf.recvbuf[0];
    return mbus_send_data(mb_context, 6);
  }

  case MBUS_FUNC_WRITE_REGS: {
    if (ctx->conf.write == 0) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }
    const uint16_t expected = (uint16_t)(ctx->header.num * 2u);
    if (ctx->header.size != expected) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    if (ctx->conf.recvbuf == 0 || ctx->conf.recvbuf_sz < expected ||
        ctx->conf.sendbuf_sz < 8u) {
      return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
    }
    g_userError = MBUS_RESPONSE_OK;
    /* FC16 wire order is big-endian: recvbuf[2i]=hi, recvbuf[2i+1]=lo. */
    for (uint16_t i = 0; i < ctx->header.num; i++) {
      const uint16_t v = (uint16_t)(((uint16_t)ctx->conf.recvbuf[i << 1] << 8) |
                                    ctx->conf.recvbuf[(i << 1) + 1]);
      ctx->conf.write((uint32_t)la + i, v);
    }
    if (g_userError != MBUS_RESPONSE_OK) {
      return mbus_response(mb_context, g_userError);
    }
    ctx->conf.sendbuf[2] = (uint8_t)(ctx->header.addr >> 8);
    ctx->conf.sendbuf[3] = (uint8_t)(ctx->header.addr & 0xFFu);
    ctx->conf.sendbuf[4] = (uint8_t)(ctx->header.num >> 8);
    ctx->conf.sendbuf[5] = (uint8_t)(ctx->header.num & 0xFFu);
    return mbus_send_data(mb_context, 6);
  }

  case MBUS_FUNC_WRITE_COILS: {
    if (ctx->conf.write == 0) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }
    const uint16_t expected = (uint16_t)((ctx->header.num + 7u) / 8u);
    if (ctx->header.size != expected) {
      return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
    }
    if (ctx->conf.recvbuf == 0 || ctx->conf.recvbuf_sz < expected ||
        ctx->conf.sendbuf_sz < 8u) {
      return mbus_response(mb_context, MBUS_RESPONSE_SERVICE_DEVICE_FAILURE);
    }
    g_userError = MBUS_RESPONSE_OK;
    for (uint16_t i = 0; i < ctx->header.num; i++) {
      const uint16_t bit =
          (uint16_t)((ctx->conf.recvbuf[i >> 3] >> (i & 7u)) & 1u);
      ctx->conf.write((uint32_t)la + i, bit);
    }
    if (g_userError != MBUS_RESPONSE_OK) {
      return mbus_response(mb_context, g_userError);
    }
    ctx->conf.sendbuf[2] = (uint8_t)(ctx->header.addr >> 8);
    ctx->conf.sendbuf[3] = (uint8_t)(ctx->header.addr & 0xFFu);
    ctx->conf.sendbuf[4] = (uint8_t)(ctx->header.num >> 8);
    ctx->conf.sendbuf[5] = (uint8_t)(ctx->header.num & 0xFFu);
    return mbus_send_data(mb_context, 6);
  }

  default:
    break;
  }
  return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_FUNCTION);
}

//#include <windows.h>
/*
 * function mbus_close()
 * close modbus context
 * return: none
 */
mbus_status_t mbus_poll(mbus_t mb_context, uint8_t byte) {
  // State machine
  _stmodbus_context_t *ctx = &g_mbusContext[mb_context];

  if (mbus_tickcount() - ctx->timer > 4) {
    mbus_flush(mb_context);
  }
  ctx->timer = mbus_tickcount();

  switch (ctx->state) {
  case MBUS_STATE_IDLE:
    mbus_flush(mb_context);
    ctx->state = MBUS_STATE_FUNCTION;
    ctx->header.devaddr = byte;
    break;
  case MBUS_STATE_FUNCTION:
    ctx->header.func = byte;
    switch (byte) {
    case MBUS_FUNC_WRITE_REG:
      ctx->header.rnum = 1;
      ctx->header.num = 1;
      ctx->state = MBUS_STATE_REGADDR_HI;
      break;
    case MBUS_FUNC_WRITE_REGS:
    case MBUS_FUNC_WRITE_COILS:
      ctx->header.rnum = 1;
      ctx->header.num = 0;
      ctx->state = MBUS_STATE_REGADDR_HI;
      break;
    case MBUS_FUNC_READ_INPUT_REGS:
    case MBUS_FUNC_READ_DISCRETE:
    case MBUS_FUNC_READ_COILS:
    case MBUS_FUNC_READ_REGS:
      ctx->state = MBUS_STATE_REGADDR_HI;
      ctx->header.rnum = 0;
      ctx->header.num = 0;
      break;
    case MBUS_FUNC_WRITE_COIL:
      ctx->header.rnum = 1;
      ctx->header.num = 1;
      ctx->state = MBUS_STATE_REGADDR_HI;
      break;
    default:
      // ctx->state = MBUS_STATE_IDLE;
      mbus_flush(mb_context);
      break;
    }
    break;
  case MBUS_STATE_REGADDR_HI:
    ctx->state = MBUS_STATE_REGADDR_LO;
    ctx->header.addr = byte << 8;
    break;
  case MBUS_STATE_REGADDR_LO:
    ctx->header.addr |= byte;
    if (ctx->header.num == 1 && ctx->header.rnum == 1) {
      ctx->state = MBUS_STATE_DATA_HI;
    } else {
      ctx->state = MBUS_STATE_REGNUM_HI;
    }
    break;
  case MBUS_STATE_DATA_HI:
    ctx->state = MBUS_STATE_DATA_LO;
    ctx->conf.recvbuf[2 * (ctx->header.num - ctx->header.rnum) + 1] = byte;
    break;
  case MBUS_STATE_DATA_LO:
    ctx->conf.recvbuf[2 * (ctx->header.num - ctx->header.rnum)] = byte;
    ctx->header.rnum--;
    if (ctx->header.rnum == 0) {
      ctx->state = MBUS_STATE_CRC_LO;
    } else {
      ctx->state = MBUS_STATE_DATA_HI;
    }
    break;
  case MBUS_STATE_DATA_SIZE:
    ctx->state = MBUS_STATE_DATA;
    ctx->header.size = byte;
    ctx->header.rsize = byte;
    break;
  case MBUS_STATE_DATA:
    ctx->conf.recvbuf[ctx->header.size - ctx->header.rsize] = byte;
    ctx->header.rsize--;
    if (ctx->header.rsize == 0) {
      ctx->state = MBUS_STATE_CRC_LO;
    }
    break;
  case MBUS_STATE_REGNUM_HI:
    ctx->state = MBUS_STATE_REGNUM_LO;
    ctx->header.num = byte << 8;
    break;
  case MBUS_STATE_REGNUM_LO:
    ctx->state = MBUS_STATE_CRC_LO;
    ctx->header.num |= byte;
    if (ctx->header.rnum == 0) {
      ctx->state = MBUS_STATE_CRC_LO;
    } else {
      ctx->header.rnum = ctx->header.num;
      ctx->state = MBUS_STATE_DATA_HI;
      if (ctx->header.func == MBUS_FUNC_WRITE_REGS ||
          ctx->header.func == MBUS_FUNC_WRITE_COILS) {
        ctx->state = MBUS_STATE_DATA_SIZE;
      }
    }

    break;
  case MBUS_STATE_CRC_LO:
    ctx->state = MBUS_STATE_CRC_HI;
    break;
  case MBUS_STATE_CRC_HI:
    ctx->state = MBUS_STATE_FINISH;
    break;
  // We can't processing any more before callback not returned
  case MBUS_STATE_RESPONSE:
    return MBUS_ERROR;
  default:
    mbus_flush(mb_context);
    break;
  }

  ctx->crc16 = mbus_crc16(ctx->crc16, byte);

  if (ctx->state == MBUS_STATE_FINISH) {
    // CRC error
    if (ctx->crc16 != 0) {
      mbus_flush(mb_context);
      return MBUS_ERROR;
    }

    // TODO: Add broadcast messages
    if (ctx->header.devaddr == ctx->conf.devaddr) {
      ctx->state = MBUS_STATE_RESPONSE;
      if (mbus_poll_response(mb_context) == MBUS_OK) {
        mbus_flush(mb_context);
        return MBUS_OK;
      }
      mbus_flush(mb_context);
      return MBUS_ERROR;
    }
    mbus_flush(mb_context);
  }
  return MBUS_OK;
}

mbus_context_t mbus_device(mbus_t mb_context) {
  return (mbus_context_t)&g_mbusContext[mb_context];
}

mbus_context_t mbus_context(mbus_t mb_context) {
  if (g_mbusContext[mb_context].open)
    return (mbus_context_t)&g_mbusContext[mb_context];
  return 0;
}

mbus_status_t mbus_send_error(mbus_t mb_context, Modbus_ResponseType response) {
  _stmodbus_context_t *ctx = &g_mbusContext[mb_context];
  /* Exception ADU: devaddr, func|0x80, exception_code, CRC_lo, CRC_hi (5 bytes) */
  if (ctx->conf.sendbuf == 0 || ctx->conf.sendbuf_sz < 5) {
    return MBUS_ERROR;
  }
  ctx->conf.sendbuf[0] = ctx->header.devaddr;
  ctx->conf.sendbuf[1] = (uint8_t)(ctx->header.func | 0x80u);
  ctx->conf.sendbuf[2] = (uint8_t)response;
  return mbus_send_data(mb_context, 3);
}

mbus_status_t mbus_send_data(mbus_t mb_context, uint16_t size) {
  // if size > ( conf.send_sz-2) error
  uint16_t crc32 = 0xFFFF;
  const _stmodbus_context_t *ctx = &g_mbusContext[mb_context];
  uint8_t *pbuf = ctx->conf.sendbuf;
  if (ctx->conf.send == 0 || pbuf == 0 || ctx->conf.sendbuf_sz < (size + 2))
    return MBUS_ERROR;
  for (int i = 0; i < size; i++) {
    crc32 = mbus_crc16(crc32, pbuf[i]);
  }
  pbuf[size++] = crc32 & 0xFF;
  pbuf[size++] = (crc32 >> 8);

  if (ctx->conf.send(mb_context, pbuf, size) != size)
    return MBUS_ERROR;
  return MBUS_OK;
}

mbus_status_t mbus_connect(const mbus_t mb_context, stmbCallBackFunc func,
                           Modbus_ConnectFuncType type) {
#if STMODBUS_COUNT_FUNC > 0
  _stmodbus_context_t *ctx = &g_mbusContext[mb_context];
  for (int i = 0; i < STMODBUS_COUNT_FUNC; i++) {
    if ((ctx->func[i].code == 0)) {
      ctx->func[i].func = func;
      ctx->func[i].code = (uint8_t)type;
      return MBUS_OK;
    }
  }
#endif
  return MBUS_ERROR;
}

int mbus_proto_address( Modbus_ConnectFuncType func, int *r) {
  int adr = 0;
  *r = 1;
  switch (func) {
  case MBUS_FUNC_WRITE_REG:
  case MBUS_FUNC_WRITE_REGS:
  case MBUS_FUNC_READ_WRITE_MASK_REGS:
  case MBUS_FUNC_READ_WRITE_REGS:
    *r = 0;
  case MBUS_FUNC_READ_REGS:
    adr = 40001;
    break;
  case MBUS_FUNC_READ_INPUT_REGS:
    adr = 30001;
    break;
  case MBUS_FUNC_READ_DISCRETE:
    adr = 10001;
    break;
  case MBUS_FUNC_WRITE_COILS:
  case MBUS_FUNC_WRITE_COIL:
    *r = 0;
  case MBUS_FUNC_READ_COILS:
    adr = 0001;
    break;
  default:
    break;
  }
  return adr;
}

#ifdef __cplusplus
}
#endif
