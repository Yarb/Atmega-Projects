//*****************************************************************************
// File Name	: dallas.c
// Title		: Dallas 1-Wire Library
// Revision		: 6
// Notes		: 
// Target MCU	: Atmel AVR series
// Editor Tabs	: 4
// 
//*****************************************************************************
 
//----- Include Files ---------------------------------------------------------
#include <avr/io.h>				// include I/O definitions (port names, pin names, etc)
#include <avr/interrupt.h>		// include interrupt support
#include <string.h>				// include string support
// #include "timer128.h"			// include timer function library
#include "dallas.h"	
#include <util/delay.h>			// include dallas support

//----- Global Variables -------------------------------------------------------
static unsigned char last_discrep = 0;	// last discrepancy for FindDevices
static unsigned char done_flag = 0;		// done flag for FindDevices

unsigned char dallas_crc;					// current crc global variable
unsigned char dallas_crc_table[] =		// dallas crc lookup table
{
	0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
	157,195, 33,127,252,162, 64, 30, 95, 1,227,189, 62, 96,130,220,
	35,125,159,193, 66, 28,254,160,225,191, 93, 3,128,222, 60, 98,
	190,224, 2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89, 7,
	219,133,103, 57,186,228, 6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135, 4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91, 5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73, 8, 86,180,234,105, 55,213,139,
	87, 9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
	116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

//----- Functions --------------------------------------------------------------

/*--------------------------------------------------------------------------
 * dallasFindNextDevice: find the next device on the bus
 * input................ rom_id - pointer to store the rom id found
 * returns.............. true or false if a device was found
 *-------------------------------------------------------------------------*/
unsigned char dallasFindNextDevice(dallas_rom_id_T* rom_id);


unsigned char dallasInit(dallas_rom_id_T devices[])
{
	return dallasFindDevices(devices);
}

unsigned char dallasReset(void)
{
	unsigned char presence = DALLAS_PRESENCE;

	cli();
	
	// pull line low
    
    //sbi(port, bit) (port) |= (1 << (bit))
    //cbi(port, bit) (port) &= ~(1 << (bit))
	DALLAS_DDR |= (1 << DALLAS_PIN);
	DALLAS_PORT &= ~(1 << DALLAS_PIN);
	
    
	// wait for presence
	_delay_us(480);
	
	// allow line to return high
	DALLAS_DDR &= ~(1 << DALLAS_PIN);
	DALLAS_PORT |= (1 << DALLAS_PIN);
	
	// wait for presence
	_delay_us(80);

	// if device is not present, pin will be 1
	if (DALLAS_PORTIN & 0x01<<DALLAS_PIN)
		presence = DALLAS_NO_PRESENCE;

	// wait for end of timeslot
	_delay_us(400);

	sei();

	// now that we have reset, let's check bus health
	// it should be noted that a delay may be needed here for devices that
	// send out an alarming presence pulse signal after a reset
	DALLAS_DDR &= ~(1 << DALLAS_PIN);
	DALLAS_PORT |= (1 << DALLAS_PIN);
	//_delay_us(200);
	if (!(DALLAS_PORTIN & (0x01<<DALLAS_PIN)))	// it should be pulled up to high
		return DALLAS_BUS_ERROR;

	return presence;
}

unsigned char dallasReadBit(void)
{
	unsigned char bit = 0;
	
	// pull line low to start timeslot
	DALLAS_DDR |= (1 << DALLAS_PIN);
	DALLAS_PORT &= ~(1 << DALLAS_PIN);
	
	// delay appropriate time
	_delay_us(6);

	// release the bus
	DALLAS_DDR &= ~(1 << DALLAS_PIN);
	DALLAS_PORT |= (1 << DALLAS_PIN);
	
	// delay appropriate time	
	_delay_us(9);

	// read the pin and set the variable to 1 if the pin is high
	if (DALLAS_PORTIN & 0x01<<DALLAS_PIN)
		bit = 1;
	
	// finish read timeslot
	_delay_us(55);
	
	return bit;
}

void dallasWriteBit(unsigned char bit)
{
	// drive bus low
	DALLAS_DDR |= (1 << DALLAS_PIN);
	DALLAS_PORT &= ~(1 << DALLAS_PIN);
	
	// delay the proper time if we want to write a 0 or 1
	if (bit)
		_delay_us(6);
	else
		_delay_us(60);

	// release bus
	DALLAS_DDR &= ~(1 << DALLAS_PIN);
	DALLAS_PORT |= (1 << DALLAS_PIN);

	// delay the proper time if we want to write a 0 or 1
	if (bit)
		_delay_us(64);
	else
		_delay_us(10);
}

unsigned char dallasReadByte(void)
{
	unsigned char i;
	unsigned char byte = 0;

	cli();
	
	// read all 8 bits
	for(i=0;i<8;i++)
	{
		if (dallasReadBit())
			byte |= 0x01<<i;

		// allow a us delay between each read
		_delay_us(1);
	}

	sei();

	return byte;
}

void dallasWriteByte(unsigned char byte)
{
	unsigned char i;

	cli();

	// write all 8 bits
	for(i=0;i<8;i++)
	{
		dallasWriteBit((byte>>i) & 0x01);
		
		// allow a us delay between each write
		_delay_us(1);
	}
	
	sei();
}

unsigned char dallasReadRAM(dallas_rom_id_T* rom_id, unsigned short addr, unsigned char len, unsigned char *data)
{
	unsigned char i;
	unsigned char error;

	union int16_var_U
	{
		unsigned short i16;
		unsigned char i08[2];
	} int16_var;

	// first make sure we actually have something to do
	if (data == NULL)
		return DALLAS_NULL_POINTER;
	if (len == 0)
		return DALLAS_ZERO_LEN;

	// reset the bus and request the device
	error = dallasMatchROM(rom_id);
	if (error != DALLAS_NO_ERROR)
		return error;
	
	// enter read mode
	dallasWriteByte(DALLAS_READ_MEMORY);
	
	// write address one byte at a time
	int16_var.i16 = addr;
	dallasWriteByte(int16_var.i08[0]);
	dallasWriteByte(int16_var.i08[1]);
	
	// read data from device 1 byte at a time
	for(i=0;i<len;i++)
		data[i] = dallasReadByte();

	return DALLAS_NO_ERROR;
}

unsigned char dallasWriteRAM(dallas_rom_id_T* rom_id, unsigned short addr, unsigned char len, unsigned char* data)
{
	unsigned char i;
	unsigned char error;

	union int16_var_U
	{
		unsigned short i16;
		unsigned char i08[2];
	} int16_var;

	// first make sure we actually have something to do
	if (data == NULL)
		return DALLAS_NULL_POINTER;
	if (len == 0)
		return DALLAS_ZERO_LEN;

	// reset the bus and request the device
	error = dallasMatchROM(rom_id);
	if (error != DALLAS_NO_ERROR)
		return error;
	
	// enter write mode
	dallasWriteByte(DALLAS_WRITE_MEMORY);

	// write address one byte at a time
	int16_var.i16 = addr;
	dallasWriteByte(int16_var.i08[0]);
	dallasWriteByte(int16_var.i08[1]);
	
	// write data one byte at a time
	for(i=0;i<len;i++)
	{
		dallasWriteByte(data[i]);
		
		// future: Check CRC16, for now just read it so we can go on
		dallasReadByte();
		dallasReadByte();

		// verify the data
		if (dallasReadByte() != data[i])
			return DALLAS_VERIFY_ERROR;
	}

	return DALLAS_NO_ERROR;
}

void dallasWaitUntilDone(void)
{
	//timerPause(6);
	
	// wait until we recieve a one
	cli();
	while(!dallasReadBit());
	sei();
}

unsigned char dallasReadROM(dallas_rom_id_T* rom_id)
{
	unsigned char i;

	// reset the 1-wire bus and look for presence
	i = dallasReset();
	if (i != DALLAS_PRESENCE)
		return i;
	
	// send READ ROM command
	dallasWriteByte(DALLAS_READ_ROM);

	// get the device's ID 1 byte at a time
	for(i=0;i<8;i++)
		rom_id->byte[i] = dallasReadByte();

	return DALLAS_NO_ERROR;
}

unsigned char dallasMatchROM(dallas_rom_id_T* rom_id)
{
	unsigned char i;

	// reset the 1-wire and look for presence
	i = dallasReset();
	if (i != DALLAS_PRESENCE)
		return i;

	// send MATCH ROM command
	dallasWriteByte(DALLAS_MATCH_ROM);

	// write id one byte at a time
	for(i=0;i<8;i++)
		dallasWriteByte(rom_id->byte[i]);

	return DALLAS_NO_ERROR;
}


unsigned char dallasAddressCheck(dallas_rom_id_T* rom_id, unsigned char family)
{
//	unsigned char i;

//	dallas_crc = 0;
	
//	for(i=1;i<7;i++)
//	{
//		dallasCRC(rom_id->byte[i]);
//		rprintfunsigned char(rom_id->byte[i]);
//		rprintfChar(' ');
//	}
//	rprintfCRLF();

//	rprintfunsigned char(dallas_crc);
//	rprintfCRLF();
	
	//run CRC on address

	//make sure we have the correct family
	if (rom_id->byte[DALLAS_FAMILY_IDX] == family)
		return DALLAS_NO_ERROR;
	
	return DALLAS_ADDRESS_ERROR;
}

unsigned char dallasCRC(unsigned char i)
{
	// update the crc global variable and return it
	dallas_crc = dallas_crc_table[dallas_crc^i];
	return dallas_crc;
}

unsigned char dallasFindDevices(dallas_rom_id_T rom_id[])
{
	unsigned char num_found = 0;
	dallas_rom_id_T id;

	// reset the rom search last discrepancy global
	last_discrep = 0;
	done_flag = 0;

	// check to make sure presence is detected before we start
	if (dallasReset() == DALLAS_PRESENCE)
	{
		// --> stang
		//while (dallasFindNextDevice(&rom_id[num_found]) && (num_found<DALLAS_MAX_DEVICES))
		//	num_found++;
		
		// continues until no additional devices are found
		while (dallasFindNextDevice(&id) && (num_found<DALLAS_MAX_DEVICES))
			memcpy(&rom_id[num_found++], &id, 8);
	}

	return num_found;
}

unsigned char dallasFindNextDevice(dallas_rom_id_T* rom_id)
{
	unsigned char bit;
	unsigned char i = 0;
	unsigned char bit_index = 1;
	unsigned char byte_index = 0;
	unsigned char bit_mask = 1;
	unsigned char discrep_marker = 0;
	
	// reset the CRC
	dallas_crc = 0;

	if (done_flag || dallasReset() != DALLAS_PRESENCE)
	{
		// no more devices parts detected
		return 0;
	}

	// send search ROM command
	dallasWriteByte(DALLAS_SEARCH_ROM);
	
	// loop until through all 8 ROM bytes
	while(byte_index<8)
	{
		// read line 2 times to determine status of devices
		//    00 - devices connected to bus with conflicting bits
		//    01 - all devices have a 0 in this position
		//    10 - all devices ahve a 1 in this position
		//    11 - there are no devices connected to bus
		i = 0;
		cli();
		if (dallasReadBit())
			i = 2;				// store the msb if 1
		_delay_us(120);
		if (dallasReadBit())
			i |= 1;				// store the lsb if 1
		sei();
		
		if (i==3)
		{
			// there are no devices on the 1-wire
			break;
		}
		else
		{
			if (i>0)
			{
				// all devices coupled have 0 or 1
				// shift 1 to determine if the msb is 0 or 1
				bit = i>>1;
			}
			else
			{
				// if this discrepancy is before the last discrepancy on a
				// previous FindNextDevice then pick the same as last time
				if (bit_index<last_discrep)
					bit = ((rom_id->byte[byte_index] & bit_mask) > 0);
				else
					bit = (bit_index==last_discrep);
				
				// if 0 was picked then record position with bit mask
				if (!bit)
					discrep_marker = bit_index;
			}

			// isolate bit in rom_id->byte[byte_index] with bit mask
			if (bit)
				rom_id->byte[byte_index] |= bit_mask;
			else
				rom_id->byte[byte_index] &= ~bit_mask;

			// ROM search write
			cli();
			dallasWriteBit(bit);
			sei();

			// ncrement bit index counter and shift the bit mask
			bit_index++; 
			bit_mask <<= 1;
			
			if (!bit_mask)
			{
				// if the mask is 0 then go to new ROM
				// accumulate the CRC and incriment the byte index and bit mask
				dallasCRC(rom_id->byte[byte_index]);
				byte_index++;
				bit_mask++;
			}
		}
	}

	if ((bit_index < 65) || dallas_crc)
	{
		// search was unsuccessful - reset the last discrepancy to 0 and return false
		last_discrep = 0;
		return 0;
	}

	// search was successful, so set last_discrep and done_flag
	last_discrep = discrep_marker;
	done_flag = (last_discrep==0);

	return 1;
}

