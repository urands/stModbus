/**

 * stModbus Libary: A tiny Modbus implementation for Modbus ASCII/RTU
 * Copyright (c) 2021+ [Iurii Bell] ds@inbox.ru (http://www.bel-tech.ru/)
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
 * @copyright   Copyright (C) 2021+
 * @author      Iurii Bell (ds@inbox.ru)
 * @license     MIT
 * @package     stModbus
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <modbus.h>

/*	This is user's file for working with the modbus interface 
	Here functions that need to be modified to work with the 
	library.

	Let's define the address space of the modbus interface:

	Protocol address split like that:
	- 1-9999		Discrete Output Coils  (DO)			r/w
	- 10001-19999	Discrete Input Contacts (DI)		r
	- 30001-39999	Analog Input Registers (AI)			r
	- 40001-49999	Analog Output Holding Registers(AO) r/w

	For our example device define next protocol address space:
	- 1-8			Discrete Output Coils  (DO)		8 coils
	- 10001-10008	Discrete Input Contacts  (DI)	8 inputs
	- 30001			Input Registers			1 analog input
	- 40001-40008	Holding Registers		8 analog input

	ALL DATA IN 16-BIT

*/

//Let's go. Describe our sample device.

// Define discrete output coils (emulation)
uint8_t		dev_output_coils;

// Discrete input contacts 
// let it be the same coils (dev_output_coils)

// Define input registers
// let it be a counter of the number of writing
uint16_t	dev_input_registers_30001;

// Define holding registers
uint16_t	holding_registers_4000x[8];

/*
  let's define a user function that will handle
  reading from registers

  @param la - protocol address (logic address)
  @return - value by logic address
*/
uint16_t mbus_protocol_read(uint32_t la) {

	// 1-8
	if (la >= 1 && la <= 8) {
		// get bit from reg
		return dev_output_coils & ( 1<<(la-1) );
	// 10001-10008
	}else if (la >= 10001 && la <= 10008) {
		//the same
		return dev_output_coils & (1 << (la - 10001));
	// 30001
	}else if (la == 30001) {
		return dev_input_registers_30001;
	}else if (la >= 40001 && la <= 40008) {
		return holding_registers_4000x[la - 40001];
	}
	// else error code 02 (the query have illegal address)
	return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
}


/*
  let's define a user function that will handle
  writing to registers

  @param la		- protocol address (logic address)
  @param value  - value for writing
  @return - none
*/
void mbus_protocol_write(uint32_t la, uint16_t value) {

	//Counter of writings
	dev_input_registers_30001++;
	// 1-8
	if (la >= 1 && la <= 8) {
		// get bit from reg
		if (value)
			dev_output_coils|=  (1 << (la - 1));
		else
			dev_output_coils&= ~(1 << (la - 1));
		return value;
		// 10001-10008
	}else if (la >= 10001 && la <= 10008) {
		// we can't write to here address
		return mbus_error(MBUS_RESPONSE_ILLEGAL_FUNCTION);
	}
	else if (la == 30001) {
		// we can't write to here address 
		return mbus_error(MBUS_RESPONSE_ILLEGAL_FUNCTION);
	}
	else if (la >= 40001 && la <= 40008) {
		holding_registers_4000x[la - 40001] = value;
		return value;
	}
	// else error code 02 (the query have illegal address)
	return mbus_error(MBUS_RESPONSE_ILLEGAL_DATA_ADDRESS);
}



#ifdef __cplusplus
}
#endif