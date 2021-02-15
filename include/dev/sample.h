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

#ifndef _STMODBUS_SAMPLE_H_
#define _STMODBUS_SAMPLE_H_

#ifdef __cplusplus
extern          "C"
{
#endif

#include "modbus.h"

/*
 * function mbus_isma_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
*/

mbus_status_t mbus_somedev_read_3xxxx(mbus_t mb_context);

mbus_status_t mbus_somedev_read_4xxxx(mbus_t mb_context);

uint16_t mbus_somedev_read(uint32_t la);

uint16_t mbus_somedev_write(uint32_t la, uint16_t value);


mbus_t mbus_somedev_open(Modbus_Conf_t* pconf);

mbus_t mbus_somedevice_open(Modbus_Conf_t *pconf);



#ifdef __cplusplus
}
#endif

#endif // _STMODBUS_SAMPLE_H_
