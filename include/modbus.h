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

#ifndef _STMODBUS_DEFINE_H_
#define _STMODBUS_DEFINE_H_

#include "modbus_conf.h"

#ifdef __cplusplus
extern          "C"
{
#endif

#if ( STMODBUS_USE_CRITICAL_SECTIONS == 0 )
    #define stmbEnterCriticalSection
    #define stmbLeaveCriticalSection
#else

#ifndef stmbEnterCriticalSection
    #error "If you use RTOS/FreeRTOS plese define mutex '#define stmbEnterCriticalSection mutex.lock' or empty"
#endif

#ifndef stmbLeaveCriticalSection
    #error "If you use RTOS/FreeRTOS plese define mutex '#define stmbLeaveCriticalSection mutex.unlock' or empty"
#endif

#endif

#if ( STMODBUS_COUNT_CONTEXT < 1 )
    #error "Count modbus context must be more then 0"
#endif
	

/*  Typedef definition */
#include <stdint.h>

typedef enum {
    MBUS_OK = 0,
    MBUS_ERROR = -1
} mbus_status_t;

typedef enum {
    MBUS_FUNC_COILS = 1,
    MBUS_FUNC_DISCRETE = 2,
    MBUS_FUNC_HOLD_REGS = 3,
    MBUS_FUNC_REGS = 4,
    MBUS_FUNC_EXCEPTION = 0x81,
} Modbus_ConnectFuncType;

typedef enum {
    MBUS_STATE_IDLE = 0,
    MBUS_STATE_DEVADD,
    MBUS_STATE_FUNCTION,
    MBUS_STATE_REGADDR_LO,
    MBUS_STATE_REGADDR_HI,
    MBUS_STATE_REGNUM_LO,
    MBUS_STATE_REGNUM_HI,
    MBUS_STATE_DATA,
    MBUS_STATE_CRC_LO,
    MBUS_STATE_CRC_HI,
    MBUS_STATE_FINISH,
    MBUS_STATE_RESPONSE

} Modbus_StateType;

typedef enum MBUS_RESPONSE{


    MBUS_RESPONSE_OK = 0x00,
    MBUS_RESPONSE_NONE = 0xFF,
/* MBUS_RESPONSE_ILLEGAL_FUNCTION
The function  code  received  in  the  query  is  not  an allowable   action   for   the
server. This   may   be because  the  function  code  is  only  applicable  to
newerdevices,  and  was  not  implemented  in  the unit selected. It could also indicate  that the
serveris  in  the  wrong  state  to  process  a  request  of  this  type,  for  example  because  it  is  unconfigured
and is being asked to return register values.
*/
    MBUS_RESPONSE_ILLEGAL_FUNCTION = 0x01,

/* MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS
The  data  address  received  in  the  query  is  not  an allowable address for the server.
More specifically,  the  combination  of  reference  number and  transfer  length  is  invalid. For  a  controller  with 100 registers, the PDU addresses the first register
as  0,  and  the  last  one  as  99.  If  a  request  is submitted  with  a  starting  register  address  of  96
and  a  quantity  of  registers  of  4,  then  this  request will  successfully  operate  (address-wise  at  least)
on   registers   96,   97,   98,   99.   If   a   request   is submitted  with  a  starting  register  address  of  96
and  a  quantity  of  registers  of  5,  then  this  request will  fail  with  Exception  Code  0x02  “Illegal  Data
Address”  since  it  attempts  to  operate  on  registers 96,  97, 98,  99  and  100,  and  there  is  no  register
with address 100.
*/
    MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS = 0x02,

/* A  value contained  in the  query  data field  is  not  an allowable value for server. This indicates a fault in the
structure   of   the   remainder   of   a   complex request,   such   as   that   the   implied   length   is incorrect.
It specifically does NOT mean that a data item submitted for storage in a register has a value outside the expectation of the application
program,  since  the  MODBUS  protocol  is  unaware of  the  significance  of  any  particular  val
ue  of  any particular register.
*/
    MBUS_RESPONSE_ILLEGAL_DATA_VALUE = 0x03,
/*
An  unrecoverable  error  occurred  while  the  server
was attempting to perform the requested action.
*/
    MBUS_RESPONSE_SERVICE_DEVICE_FAILURE = 0x04,
}Modbus_ResponseType;


/* Simple function for many usage
 *
*/

typedef int8_t mbus_t;

typedef void (*stmbFunc)(void);

typedef void (*stmbCallBackFunc)(uint8_t func, uint16_t address, uint16_t size);

typedef int (*stmbSendFunc)(mbus_t func, uint8_t* data, uint16_t size);



typedef struct __stmodbus_request_header{
    uint8_t devaddr;
    uint8_t func;
    uint16_t addr;
    uint16_t num;
} _stmodbus_request_header;

typedef struct __stmodbus_bind_func{
    uint8_t          code;
    stmbCallBackFunc func;
} _stmodbus_bind_func;



typedef struct __stmodbus_conf_t{
    uint8_t         devaddr;
    uint16_t        coils;
    uint16_t        discrete;
    void*           device;
    stmbSendFunc    send;
}Modbus_Conf_t;


typedef struct __stmodbus_context_t {
    Modbus_Conf_t               conf;
    uint8_t                     open;
    Modbus_StateType            state;
    uint8_t                     crc16_lo;
    uint8_t                     crc16_hi;
    _stmodbus_bind_func         func[STMODBUS_COUNT_FUNC];
    _stmodbus_request_header    header;
    _stmodbus_request_header    response;
} _stmodbus_context_t;


/*  Function  definition */

/*
 * function mbus_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
*/
mbus_t mbus_open(Modbus_Conf_t* pconf);

/*
 * function mbus_close()
 * close modbus context
 * return: none
*/
void   mbus_close(mbus_t mb_context);


/*
 *
 *
*/
inline void* mbus_device(mbus_t mb_context);

/*
 * function mbus_connect()
 * connect function to callback modbus context
 * return: none
*/
mbus_status_t mbus_connect(mbus_t mb_context, stmbCallBackFunc func, Modbus_ConnectFuncType type );


mbus_status_t mbus_response(Modbus_ResponseType response);

mbus_status_t mbus_flush(mbus_t context);


uint16_t   mbus_hal_crc16(mbus_t mb_context, uint8_t byte);

/*
 * function mbus_close()
 * close modbus context
 * return: none
*/
mbus_status_t  mbus_poll(mbus_t mb_context, uint8_t byte);

inline mbus_status_t  mbus_poll_response(_stmodbus_context_t* ctx);

extern _stmodbus_context_t g_mbusContext[STMODBUS_COUNT_CONTEXT];

#ifdef __cplusplus
}
#endif

#endif // _STMODBUS_DEFINE_H_
