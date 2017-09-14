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

#ifdef _cplusplus
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
} Modbus_ConnectFuncType;


typedef int8_t mbus_t;

/* Simple function for many usage
 *
*/
typedef void (*stmbFunc)(void);

typedef void (*stmbCallBackFunc)(uint8_t func, uint16_t address, uint16_t size);

typedef struct __stmodbus_context_t {
    uint8_t     open;
    stmbCallBackFunc func[255];
    stmbFunc    lock;
    stmbFunc    unlock;

} _stmodbus_context_t;


/*  Function  definition */

/*
 * function mbus_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
*/
mbus_t mbus_open(stmbFunc lock, stmbFunc unlock);

/*
 * function mbus_close()
 * close modbus context
 * return: none
*/
void   mbus_close(mbus_t mb_context);

/*
 * function mbus_connect()
 * connect function to callback modbus context
 * return: none
*/
mbus_status_t mbus_connect(mbus_t mb_context, stmbCallBackFunc func, Modbus_ConnectFuncType type );


mbus_status_t mbus_response(void);


#ifdef _cplusplus
}
#endif

#endif // _STMODBUS_DEFINE_H_