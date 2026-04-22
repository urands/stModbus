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

#include "dev/sample.h"

#include "spi_ion_board.h"

#include "gpio_ion_board.h"

#include <stdio.h>

#include <string.h>


#ifdef __cplusplus
extern          "C"
{
#endif




//Define device register
uint16_t device_reg_4xxxx[13];

//Define some 16-bit output
uint16_t output;

//Define some 16-bit input
uint16_t input;


extern uint32_t ddd2;


//Function for initialization modbus on you device
mbus_t mbus_somedevice_open(Modbus_Conf_t *pconf)
{
	 mbus_t mb;

	//Define read callback function
    pconf->read = mbus_somedev_read;
	
	//Define write callback function
    pconf->write = mbus_somedev_write;

    //Open modbus context
	mb = mbus_open(pconf);

    if (mb < 0 ) return (mbus_t)MBUS_ERROR;

	//We can map some function on modbus commands
    //if ( mbus_connect(mb, mbus_somedevice_read_3xxxx, MBUS_FUNC_READ_INPUT_REGS ) ) return (mbus_t)MBUS_ERROR;

    //if ( mbus_connect(mb, mbus_somedevice_read_4xxxx, MBUS_FUNC_READ_REGS ) ) return (mbus_t)MBUS_ERROR;

	//Set default values for registers as you need

    device_reg_4xxxx[0] = 0x322A;             //40001 
    device_reg_4xxxx[1] = pconf->devaddr;     //40002 Device addr
    device_reg_4xxxx[2] = 0x7;                //40003 

    device_reg_4xxxx[3] = 0x0;                //40004 
    device_reg_4xxxx[4] = 0x0;                //

    device_reg_4xxxx[5] = 0x0;                //40006 
    device_reg_4xxxx[6] = 0x0;                //

    device_reg_4xxxx[7] = 0x0;                //40008 
    device_reg_4xxxx[8] = 0x0;                //

    device_reg_4xxxx[11] = 0x0;                //Up time // 40012


    return mb;
}


mbus_status_t mbus_somedevice_read_3xxxx(mbus_t mb_context)
{
    mbus_context_t ctx = mbus_context(mb_context);
    if (ctx == 0) return MBUS_ERROR;
   // printf("Isma read:%d\n", ctx->header.addr+30001);


    return MBUS_OK;

}

mbus_status_t mbus_somedevice_read_4xxxx(mbus_t mb_context)
{
    mbus_context_t ctx = mbus_context(mb_context);
    if (ctx == 0) return MBUS_ERROR;
    //printf("Isma read:%d\n", ctx->header.addr+40001);

    uint16_t size = 0;
    uint16_t* buf = (uint16_t*)( ctx->conf.sendbuf + 3 );

    /*Setting  register  40001  according to the table  below  will  enable  1  of  4 available
     *   actions: module reset, reloading thesettings, settingto defaultand entering thebootloader.     *  40001
     */


    if (ctx->header.addr + ctx->header.num < 13 ){
        //memcpy(buf, isma_reg_4xxxx +  ctx->header.addr, ctx->header.num*2 );
        size= ctx->header.num;
    }else{
        //Hack
        //memset(buf,0,ctx->header.num*2);
        buf[0]++;
        size= ctx->header.num;
    }


    if ( size != 0 ){
        ctx->conf.sendbuf[2] = size*2;
        return mbus_send_data(mb_context, size*2 + 3 );
    }

    return mbus_response(mb_context, MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
}

uint16_t adc[3];

uint16_t sdadc1[2];


uint16_t dac[16];

uint16_t sdadc2[3];

uint16_t sdadc3[8];

extern float vout1;
extern float v2;
extern float r1;

/* It's modbus request on read register by logical address (la) */
uint16_t mbus_somedev_read(uint32_t la)
{
	 short val;
		if ( la > 40000 && la <= 40016 ){
			return dac[la-40001];
		}
			
		//Digital outputs
		switch (la){
			case 40017: //State
				return ddd2;			
			case 40143: //Default state
				return 0;
		}
		
		uint32_t*  ff = (uint32_t*) &vout1;
		
		if (la == 40018 ){
			return (*ff)&0xFFFF;
		}
		if (la == 40019 ){
			return ((*ff)>>16)&0xFFFF;
		}

		
		ff = (uint32_t*) &v2;
		
		if (la == 40020 ){
			return (*ff)&0xFFFF;
		}
		if (la == 40021 ){
			return ((*ff)>>16)&0xFFFF;
		}
		
		ff = (uint32_t*) &r1;
		
		if (la == 40022 ){
			return (*ff)&0xFFFF;
		}
		if (la == 40023 ){
			return ((*ff)>>16)&0xFFFF;
		}		

		
		if (la > 40022 && la< 40047){
			return 0;
		}

		if (la < 40000){
			return 1;
		}
		
		//Analog outputs
		if (la == 40019){
			return 0;
		}

		//Analog inputs
		if (la == 40087){
			return 0;
		}			
		if (la == 40088){
			val = (short) sdadc1[1];
			if (val < 0 ) val = 0;
			//return  (uint16_t)((float)val*0.07*2);
		}			
		if (la == 40089){
			val = (short) sdadc1[0];
			if (val < 0 ) val = 0;
			//return  (uint16_t)((float)val*0.05);
		}			
		if (la == 40090){
			return 4;
		}			
		if (la == 40091){
			return 5;
		}			
		if (la == 40092){
			return 6;
		}			
		if (
			la > 40087){
			//return la -40087;
		}			
		
		if (la > 40100){
			return 0;
		}
		

		
		
		
    return la;
}


//Function for write to some register by logic address
uint16_t mbus_somedev_write(uint32_t la, uint16_t value)
{
    //printf("We write: %d %d\n",la, value);
		if ( la > 40000 && la <= 40016 ){
			uint8_t ch  = la - 40001;
			dac[ch] = value;
			spi_dac_set(dac[ch]&0xFF,ch);
		}
		if (la == 40017 ){
			ion_adc_writereg(0x1, 1, value&0xFF);
		}
		if (la == 40018 ){
			ion_mux_select(value&0xF);
			ion_mux_output(value&0x10);
		}
		

		
    return value;
}



#ifdef __cplusplus
}
#endif








