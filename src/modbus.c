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

#include "modbus.h"


//Global variable for modbus context
_stmodbus_context_t g_mbusContext[STMODBUS_COUNT_CONTEXT];

void lock(){


}

void unlock(){

}


void test(){

    mbus_t modbus = mbus_open(&lock, &unlock);




    mbus_close(modbus);


}




/*
 * function mbus_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
*/
mbus_t mbus_open(stmbFunc lock, stmbFunc unlock){
    mbus_t context;

    if ( ( lock == 0 ) || (unlock == 0 )) return MBUS_ERROR;

    for( context = 0; context < STMODBUS_COUNT_CONTEXT; context++){
        if ( g_mbusContext[context].open == 0 ){
            break;
        }
    }
    if ( context == STMODBUS_COUNT_CONTEXT ) return MBUS_ERROR;

    g_mbusContext[context].lock  = lock;
    g_mbusContext[context].unlock  = unlock;

}




