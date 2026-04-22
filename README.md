# stModbus (beta 0.1b10)
Modbus RTU implementation for Cortex-M (STM32 series: F1/F3/F4)

This is free tiny library for Desktop and MCU application.
The main purpose of this library usage on STM32 controllers together with CMSIS library

The library allows you to create multiple modbus contexts for devices. You will be able to implement several modbus ports on single MCU.

## Requirements


## How usage

An example of implementing a device with some registers can be viewed in the example folder. You can also use these examples as a template for your own implementations.

It's library easy for usage. you need to take a few steps:

1. Define address space for you device:

	Usually  protocol address split like that:
	- 1-9999		Discrete Output Coils  (DO)			r/w
	- 10001-19999	Discrete Input Contacts (DI)		r
	- 30001-39999	Analog Input Registers (AI)			r
	- 40001-49999	Analog Output Holding Registers(AO) r/w

	You may not follow these guidelines but some apps may not work correctly with your device.

2. Define read and write callback function for you device registers as you need

```c
	/* It's modbus request on read register by logical address (la) */
	uint16_t user_mbus_protocol_read(uint32_t la){
		switch(la){
			case 40001:
				return my_buffer_reg_4xxxx[0];
			case 40002:
				return some_register_from_you_device;
			...
		}
	}

	//Function for write to some register by logic address
	uint16_t user_mbus_protocol_write(uint32_t la, uint16_t value)
	{
		if ( la > 40000 && la <= 40013 ){
			my_buffer_reg_4xxxx[la-40001] = value;
		}
		if (la == 40018 ){
			 output = (value >> 8) | (value << 8);
		}
		
    return value;
}	
```

3. Add recv modbus function 

   Place mbus_poll(modbus, modbus_byte); in the receiving loop of the incoming stream.

   mbus_poll processes incoming data byte-by-byte. In the examples, you can find batch processing using dma. 

```c
void HAL_UART_DMA_IdleLineDetect(UART_HandleTypeDef *huart){
	...
	if (huart == &huart1){
		for (i=... {
			mbus_poll(modbus, data[i] );
		}
		clear buffer....
	}
}
```

---
   But it should be noted that the calls read and write functions from point 2 called in the context of this function. we plan to add the ability to use queues in future for different OS.

---

4. Add send modbus function

	Implement the function of sending a data stream:

```c
int mbus_send(const mbus_t context,const uint8_t* data, const uint16_t size){
	HAL_UART_Transmit( &huart2, (uint8_t*) data,size, 1000);
	...
}

```

5. After all this just configure modbus library

```c
	// Modbus context (we can use many context for example 
	// if we use 2 or more RS485 ports or ethernet and serial)
	mbus_t modbus; 
	Modbus_Conf_t mb_config;

	/* Device slave address */
	mb_config.devaddr = 0x01;

	/* Just ptr on any external object, you can get it by context */
	mb_config.device = (void*) 0;

	/* This that function for sending some data (use sendbuf for buf) */
	mb_config.send = &mbus_send;

	//User Read callback function ( read by logical address) 
	pconf->read = user_mbus_protocol_read;
	
	//Write callback function
	pconf->write = user_mbus_protocol_write;
	
	//Open modbus contex
	modbus = mbus_open(pconf);
```
 

6. Customize config file

	Customize the modbus_conf.h file to suit your requirements (internal buffer size, number of context). The keil  configuration wizard can be used for configuration.

| Full documentation is planned for the release of the first version of alpha





