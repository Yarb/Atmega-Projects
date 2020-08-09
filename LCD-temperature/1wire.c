// ***********************************************************
// Project: AVR-sensors
// 2014
// Author: Johan "Arb" Strandman
// Module description: Shit that fucks up lights and buttons
// 
// ***********************************************************

#include <avr/io.h>              // Most basic include files
#include <avr/interrupt.h>       // Add the necessary ones
#include <util/delay.h>
#include <dallas.h>
#include <ds18b20.h>

#define F_CPU 					1000000UL


	

int main(void) {
	

	DDRB = 0x00;
	PORTB = 0x00;
		
	int command_done = 0;
	int ready = 0;
	uint8_t command = 0x00;
	char dev = 0x00;
	
	uint8_t num_devices;
	num_devices = ds18b20Init();
	
	u16 data = 0x0000;
	
	uint8_t output[4];
	while (1){
		data = 0x0000;
		
		//Command read loop	
		while ((PINB & 0x01)){
        	if (!command_done ){
    			PORTD = 0x00;
    			DDRD = 0xF0;
    			PORTD = 0x00;
    			
    			//Read command
    			command = PIND & 0x0F;   
    			PORTD = 0x80;
    			while ((PINB & 0x03) == 0x03){}
    			PORTD = 0x00;
    			command <<= 4;
    			command ^= (PIND & 0x0F);
    			
    			
    			//Parse device id from read command
    			if ((command & 0xF0) == 0xF0){
    				dev = command & 0x0F; // Store device identifier
    				command = 0xF0; // Set mode to read
    			}
        			
				switch (command) {
					case 0xF0:
						//Read temp of given device
						//readDevice(1, &data);
						
						//WILL RETURN WRONG VALUES IF SENSOR IS DETACHED
						if (dev == 0){
						    output[0] = 0x0F;
							output[1] = 0x0A;
							output[2] = 0x0F;
							output[3] = 0x0A;
						}else if (readDevice(dev, &data) == DALLAS_NO_ERROR){
							output[0] = data & 0x000F;
							output[1] = (data & 0x00F0) >> 4;
							output[2] = (data & 0x0F00) >> 8;
							output[3] = (data & 0xF000) >> 12;
						}else{
							output[0] = 0x0F;
							output[1] = 0x0A;
							output[2] = 0x0F;
							output[3] = 0x0A;
						}
							
						ready = 4;
						command_done = 1;
						command = 0x00;
						break;
						
					// List devices command.
					// Return number of devices	
					case 0x0A:
						ready = 2;
						command_done = 1;
						
						output[0] = num_devices;
						output[1] = 0x00;
						command = 0x00;
						break;
					
					// Update the number of devices
					// Return number of devices
					case 0xAD:
						ready = 2;
						command_done = 1;
						num_devices = ds18b20Init();
						
						output[0] = num_devices;
						output[1] = 0x00;
						
						command = 0x00;
						break;
					
					default:
						ready = 2;
						command_done = 1;
						output[0] = 0x0A;
						output[1] = 0x0A; //ERRORCODE
						command = 0x00;
				}
			}else{
			    _delay_ms(20);
			}
		}

		DDRD = 0xFF;
		PORTD = 0x80;
		command_done = 0;

				
		// If there is ready data go to output mode
		while((ready > 0) && ((PINB & 0x04) == 0x04)){
			PORTD = output[ready - 1];
			//Wait for output signal. 
			//When received, send new data to channel
			if ((PINB & 0x06) == 0x06){
				PORTD = 0x00;
				ready--;
				output[ready] = 0x00;
				while((PINB & 0x06) == 0x06){}
			}
		}
	}
}
	
