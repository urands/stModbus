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
extern          "C"
{
#endif

#include "modbus.h"

#include <stdio.h>





//Global variable for modbus context
_stmodbus_context_t g_mbusContext[STMODBUS_COUNT_CONTEXT];

void lock(){


}

void unlock(){

}






/*
 * function mbus_open()
 * open new modbus context for new port
 * return: MODBUS_ERROR - if can't open context
*/
mbus_t mbus_open(Modbus_Conf_t* pconf){
    mbus_t context;

    //if ( ( lock == 0 ) || (unlock == 0 )) return MBUS_ERROR;

    for( context = 0; context < STMODBUS_COUNT_CONTEXT; context++){
        if ( g_mbusContext[context].open == 0 ){
            break;
        }
    }
    if ( context == STMODBUS_COUNT_CONTEXT ) return MBUS_ERROR;

    //Clear context
    memset(&g_mbusContext[context],0,sizeof(_stmodbus_context_t));
    //Copy config to context
    memcpy((void*)&g_mbusContext[context].conf,(void*)pconf, sizeof(Modbus_Conf_t));

    return context;
}


mbus_status_t mbus_flush(mbus_t context)
{
    g_mbusContext[context].crc16_lo = 0xFF;
    g_mbusContext[context].crc16_hi = 0xFF;
    g_mbusContext[context].state = MBUS_STATE_IDLE;
    return MBUS_OK;
}





/*
 * function mbus_close()
 * close modbus context
 * return: none
*/
mbus_status_t mbus_poll(mbus_t mb_context, uint8_t byte)
{
    //State machine
    uint16_t crc16;
    _stmodbus_context_t* ctx =  &g_mbusContext[mb_context];

    switch (ctx->state) {
    case MBUS_STATE_IDLE:
        mbus_flush(mb_context);
        ctx->state = MBUS_STATE_FUNCTION;
        ctx->header.devaddr = byte;
        break;
    case MBUS_STATE_FUNCTION:
        ctx->header.func = byte;
        switch (byte) {
        case MBUS_FUNC_COILS:
        case MBUS_FUNC_HOLD_REGS:
            ctx->state = MBUS_STATE_REGADDR_HI;
            break;
        default:
            ctx->state = MBUS_STATE_IDLE;
            break;
        }
        break;
    case MBUS_STATE_REGADDR_HI:
        ctx->state = MBUS_STATE_REGADDR_LO;
        ctx->header.addr = byte << 8;
        break;
    case MBUS_STATE_REGADDR_LO:
        ctx->state = MBUS_STATE_REGNUM_HI;
        ctx->header.addr|= byte;
        break;
    case MBUS_STATE_REGNUM_HI:
        ctx->state = MBUS_STATE_REGNUM_LO;
        ctx->header.num = byte << 8;
        break;
    case MBUS_STATE_REGNUM_LO:
        ctx->state = MBUS_STATE_CRC_LO;
        ctx->header.num|= byte;
        break;
    case MBUS_STATE_CRC_LO:
        ctx->state = MBUS_STATE_CRC_HI;
        break;
    case MBUS_STATE_CRC_HI:
        ctx->state = MBUS_STATE_FINISH;
        break;
    //We can't processing any more before callback not returned
    case MBUS_STATE_RESPONSE:
        return MBUS_ERROR;
    default:
        ctx->state = MBUS_STATE_IDLE;
        break;
    }

    crc16 = mbus_hal_crc16(mb_context,byte);

    printf("\tcrc:0x%X -0x%X state: %x\n",byte,crc16,  ctx->state);

    if (  ctx->state == MBUS_STATE_FINISH ){
        //CRC error
        if ( crc16 != 0 ) {
              ctx->state = MBUS_STATE_IDLE;
              return MBUS_ERROR;
        }

        if (  ctx->header.devaddr ==  ctx->conf.devaddr ){
            ctx->response.devaddr = ctx->conf.devaddr;
            ctx->response.func = ctx->header.func;

            printf("We get func: %x need ansver %x %d\n",  ctx->header.func,  ctx->header.addr, ctx->header.num );

           ctx->state = MBUS_STATE_RESPONSE;

           mbus_poll_response(mb_context);

           //callback

           ctx->state = MBUS_STATE_IDLE;
					
					 return MBUS_OK;
        }



    }






    return MBUS_OK;
}


inline mbus_status_t mbus_poll_response(_stmodbus_context_t* ctx)
{
    stmbCallBackFunc func = 0;
    for(int i=0;i< STMODBUS_COUNT_FUNC; i++){
        if ( (ctx->func[i].code == ctx->response.func) ){
            func = ctx->func[i].func;
            break;
        }
    }
    if ( func == 0 ){
        return mbus_response(MBUS_RESPONSE_ILLEGAL_FUNCTION);
    }

    switch (ctx->response.func) {
    case MBUS_FUNC_COILS:
        if ( (ctx->response.num == 0) || (ctx->response.num > 0x07D0 )){
            return mbus_response(MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
        }
        if ( (ctx->response.addr + ctx->response.num) >= ctx->conf.coils  ){
            return mbus_response(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
        }
        break;
    case MBUS_FUNC_DISCRETE:
        if ( (ctx->response.num == 0) || (ctx->response.num > 0x07D0 )){
            return mbus_response(MBUS_RESPONSE_ILLEGAL_DATA_VALUE);
        }
        if ( (ctx->response.addr + ctx->response.num) > ctx->conf.discrete ){
            return mbus_response(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
        }
        break;
    default:
        break;
    }



}


mbus_status_t mbus_response(Modbus_ResponseType response){

    if (response == MBUS_RESPONSE_OK ){


    }else{


    }




}

inline void *mbus_device(mbus_t mb_context)
{
    return g_mbusContext[mb_context].conf.device;
}




#ifdef __cplusplus
}
#endif



