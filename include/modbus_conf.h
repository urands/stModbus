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

#ifndef _STMODBUS_CONF_H_
#define _STMODBUS_CONF_H_

#ifdef _cplusplus
extern "C" {
#endif

//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

// <q>  Use Modbus-RTU
#define STMODBUS_USE_RTU 1
// <q>  Use Modbus-ASCII
#define STMODBUS_USE_ASCII 0
// <q>  Use critical sections (recommended)
#define STMODBUS_USE_CRITICAL_SECTIONS 0
// <o> Count modbus context
// <i> Don't set a lot of count for memory saving
#define STMODBUS_COUNT_CONTEXT 1

// <o> Count modbus func
// <i> Don't set a lot of count for memory saving
#define STMODBUS_COUNT_FUNC 0

// <o> Count modbus coils (discrete output)
// <i> Don't set a lot of count for memory saving
#define STMODBUS_COUNT_COILS 10

// <o> Count modbus discrete (discrete inputs)
// <i> Don't set a lot of count for memory saving
#define STMODBUS_COUNT_DISCRETE 8

// <o> Response buffer size
// <i> Don't set a lot of count for memory saving
#define STMODBUS_MAX_RESPONSE_SIZE 255

//   <o> Debug information messages level <0=>Off <1=>Low <2=>Medium <3=>High
//	 <i> Debug information messages sending or stop  on warnings
#define STACKOS_DEBUG 3
#if (STACKOS_DEBUG > 3)
#error "Invalid debug information messages level!"
#endif

//	<h> Default Stack OS module parameters
//<o> Debug information messages level <4=>idle (lowest) <5=>low <6=>below
//normal <0=>normal (default) <1=>above normal <2=>high <3=>realtime (highest)
//<i> Thread priority
#define STACKOS_MODULE_THREAD_LEVEL 0
//<o> Thread stack size
//<i> Thread
#define STACKOS_MODULE_THREAD_STACK 0
//	</h>

//	<e> Enable test unit (Benchmark)
#define STOS_TESTUNIT 1
// <q> Enable exclusive test unit
#define STOS_TESTUNIT_EXCLUSIVE 0
//	</e>

//-------- <<< end of configuration section >>>    --------------------

#include "mbutils.h"

#ifdef _cplusplus
}
#endif

#endif // _STMODBUS_CONF_H_
