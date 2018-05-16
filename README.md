# stModbus (IN PROGRESS)
Modbus RTU implementation for Cortex-M (STM32 series: STM32F103, STM32F3xx)

This is free tiny library for Desktop and MCU application.
The main purpose of this library usage on STM32 controllers together with CMSIS library

The library allows you to create multiple modbus contexts for devices. You will be able to implement several modbus ports on single MCU.

## Requirements


## How usage

It's library easy for usage:

1. Define read and write callback function for you device registers as you need

```c
	/* It's modbus request on read register by logical address (la) */
	uint16_t user_mbus_somedev_read(uint32_t la){
		switch(la){
			case 40000:
				return my_buffer_reg_4xxxx[0];
			case 40001:
				return some_register_from_you_device;
			...
		}
	}

	//Function for write to some register by logic address
	uint16_t user_mbus_somedev_write(uint32_t la, uint16_t value)
	{
		if ( la >= 40000 && la <= 40013 ){
			my_buffer_reg_4xxxx[la-40000] = value;
		}
		if (la == 40018 ){
			 output = (value >> 8) | (value << 8);
		}
		
    return value;
}	
```

2. Add recv modbus function 
   Place mbus_poll(modbus, modbus_byte);
   
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

3. After that just configure modbus library

```c
	Modbus_Conf_t mb_config;

   /* Device slave address */
    mb_config.devaddr = 0x01;
    /* Device count coils(discrete outputs) */
    mb_config.coils = 8;
    /* Device count discrete (discrete inputs) */
    mb_config.discrete = 8;
    /* Just ptr on any external object, you can get it by context */
    mb_config.device = (void*) 0;

    /* This buffer for data content when we sending data */
    mb_config.sendbuf = s_buf;
    mb_config.sendbuf_sz = sizeof(s_buf);

    /* This that function for sending some data (use sendbuf for buf) */
    mb_config.send = &send;

    /* This buffer for data content when we recv some data (no more then sz) */
    mb_config.recvbuf = r_buf;
    mb_config.recvbuf_sz = sizeof(r_buf);


   	//Read callback function ( read by logical address) 
    pconf->read = user_mbus_somedev_read;
	
	//Write callback function
    pconf->write = user_mbus_somedev_write;
	
	//Open
	mb = mbus_open(pconf);
```


## 





