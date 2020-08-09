/* This is AVR code for driving the RGB LED strips from Pololu.
   It allows complete control over the color of an arbitrary number of LEDs.
   This implementation disables interrupts while it does bit-banging with inline assembly.
 */



/* This line specifies the frequency your AVR is running at.
   This code supports 20 MHz, 16 MHz and 8MHz */

//  #define F_CPU 8000000


#include <avr/io.h>
#include <avr/interrupt.h>
//#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>

#include <dallas.h>
#include <ds18b20.h>


#define LINE1 0
#define LINE2 64
#define LINE3 20
#define LINE4 84

char empty_buffer[20] = "                    ";
char s_buf[8] = "        ";

/*
LCD PIN | ATMEGA PIN

RS      |   D0
RW      |   D1
Enable  |   D2
Data0   |   B2
Data1   |   B1
Data2   |   B0
Data3   |   D7
Data4   |   D6
Data5   |   D5
Data6   |   B7
Data7   |   B6

*/

#define DISPLAYMODE 0x0c // DISPLAY ON
#define INITMODE 0x38    // FOUR ROWS, 20 characters
#define CMD_WRITE 0x00
#define DATA_WRITE 0x01
#define CLEAR_LCD 0x01

#define ENABLE 0x04
#define DATA0 0x04
#define DATA1 0x02
#define DATA2 0x01
#define DATA3 0x80
#define DATA4 0x40
#define DATA5 0x20
#define DATA6 0x80
#define DATA7 0x40

void set_lcd_pins(unsigned char control, unsigned char data){
  PORTD = control;
  PORTB = 0x00;
  
  if (data & 0x01) 
    PORTB |= DATA0;
  if (data & 0x02)
    PORTB |= DATA1;
  if (data & 0x04)
    PORTB |= DATA2;
  if (data & 0x08)
    PORTD |= DATA3;
  if (data & 0x10)
    PORTD |= DATA4;
  if (data & 0x20)
    PORTD |= DATA5;
  if (data & 0x40)
    PORTB |= DATA6;
  if (data & 0x80)
    PORTB |= DATA7;
  //Annoying mapping...ugh
  
  //Toggle enable to tell matrix to read
  PORTD |= ENABLE;
  _delay_us(40);
  PORTD &= ~(ENABLE);
}

void clear_lcd(){
    set_lcd_pins(CMD_WRITE, CLEAR_LCD);
}

void init_lcd(){
    clear_lcd();
    _delay_ms(20);
    set_lcd_pins(CMD_WRITE, INITMODE);
    _delay_ms(20);
    set_lcd_pins(CMD_WRITE, DISPLAYMODE);
}

void set_lcd_addr(unsigned char addr){
    set_lcd_pins(CMD_WRITE, (0x80 | addr)); //Address is always in format 1XXXXXXX thus the OR 
}
void write_lcd_data(unsigned char data){
    set_lcd_pins(DATA_WRITE, data);
}
void write_buffer(char buf[], int size, char start){
    if (start > -1){
       set_lcd_pins(CMD_WRITE, (0x80 | start));
    }
    _delay_us(40);
    for(int i = 0; size > i; i++){
       write_lcd_data(buf[i]);
    }
}

void clear_line(unsigned char line){
    write_buffer(empty_buffer, 20, line);
}



int main()

{
  //Set Data direction for ports B,C,D
  DDRC = 0x03;
  DDRB = 0xFF;
  DDRD = 0xFF;
  PORTC = 0xFC;
  _delay_ms(1500);
  init_lcd();
  _delay_ms(250);
  clear_lcd();
  _delay_ms(200);
  clear_line(LINE2);
  clear_line(LINE4);
  
  write_buffer("Temperatures:", 13, LINE1);
  
  
  ds18b20Init();
  _delay_ms(2500);
  
  clear_line(LINE2);
  clear_line(LINE3);
  clear_line(LINE4);
  
  //INIT OK, TEMP MAGICK TIME
  unsigned short temp;
  char test[2];
  char reg1;
  char reg2;
  char tempBuffer[7] = "       ";
  write_buffer("Temp1 : ", 8, LINE2);
  write_buffer("Temp2 : ", 8, LINE3);
  write_buffer("Temp3 : ", 8, LINE4);
  while(1){
      write_buffer("-", 1, LINE1 + 18);
      _delay_ms(25);
      if (readDeviceExt(3, &temp, &reg1, &reg2) == DALLAS_NO_ERROR){
          temp = temp >> 1;
         
          dtostrf(((double)temp - 0.25 - 0.1875 + ((reg2 - reg1) / (double)reg2)), 3, 3, tempBuffer); // 0.186 deg calibration
          
          if ((temp < 0) | (temp > 99)){
            write_buffer(tempBuffer, 7, LINE2 + 8);  
          }else{
            write_buffer(tempBuffer, 6, LINE2 + 8);            
          }
          temp = 0;          
      }else{
          test[0] = (char)readDevice(3, &temp);
          write_buffer(s_buf, 5, LINE2 + 7);
          write_buffer(test, 1, LINE2 + 12);
          ds18b20Init();
      }
      write_buffer("\\", 1, LINE1 + 18);
      _delay_ms(25);
      if (readDeviceExt(1, &temp, &reg1, &reg2) == DALLAS_NO_ERROR){
          temp = temp >> 1;
          dtostrf(((double)temp - 0.25 + ((reg2 - reg1) / (double)reg2)), 3, 3, tempBuffer);
          if ((temp < 0) | (temp > 99)){
            write_buffer(tempBuffer, 7, LINE3 + 8);  
          }else{
            write_buffer(tempBuffer, 6, LINE3 + 8);            
          }
          temp = 0;          
      }else{
          test[0] = (char)readDevice(1, &temp);
          write_buffer(s_buf, 5, LINE3 + 7);
          write_buffer(test, 1, LINE3 + 12);
          ds18b20Init();
      }
	  write_buffer("|", 1, LINE1 + 18);
      _delay_ms(25);
      if (readDeviceExt(2, &temp, &reg1, &reg2) == DALLAS_NO_ERROR){
          temp = temp >> 1;
          dtostrf(((double)temp - 0.25 + ((reg2 - reg1) / (double)reg2)), 3, 3, tempBuffer);
          if ((temp < 0) | (temp > 99)){
            write_buffer(tempBuffer, 7, LINE4 + 8);  
          }else{
            write_buffer(tempBuffer, 6, LINE4 + 8);            
          }
          temp = 0;          
      }else{
          test[0] = (char)readDevice(2, &temp);
          write_buffer(s_buf, 5, LINE4 + 7);
          write_buffer(test, 1, LINE4 + 12);
          ds18b20Init();
      }
      write_buffer("/", 1, LINE1 + 18);
      _delay_ms(370);
  }
}
