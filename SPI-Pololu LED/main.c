/* 
   SPI -> Pololu LED strip controller.
   This is designed to control the LED strip based on commands received via SPI connection.
   Purpose was to offload the direct LED control from Rasberry Pi to dedicated that can handle 1-wire reliably.
   Whole system had following architecture:
   Raspi  ---SPI--->  Atmega  ---1-wire--->  Pololu LED strip
   
   The main LED 1-wire code is from Pololu examples.
*/ 




#define F_CPU 8000000


#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>



#define LED_STRIP_PORT PORTD
#define LED_STRIP_DDR  DDRD
#define LED_STRIP_PIN  7
/** The rgb_color struct represents the color for an 8-bit RGB LED.
    Examples:
      Black:      (rgb_color){ 0, 0, 0 }
      Pure red:   (rgb_color){ 255, 0, 0 }
      Pure green: (rgb_color){ 0, 255, 0 }
      Pure blue:  (rgb_color){ 0, 0, 255 }
      White:      (rgb_color){ 255, 255, 255} */
typedef struct rgb_color
{
  unsigned char red, green, blue;
} rgb_color;


typedef struct sweep_color
{
  int red, green, blue;
} sweep_color;

/** led_strip_write sends a series of colors to the LED strip, updating the LEDs.
 The colors parameter should point to an array of rgb_color structs that hold the colors to send.
 The count parameter is the number of colors to send.
 This function takes about 1.1 ms to update 30 LEDs.
 Interrupts must be disabled during that time, so any interrupt-based library
 can be negatively affected by this function.
 Timing details at 20 MHz (the numbers slightly different at 16 MHz and 8MHz):
  0 pulse  = 400 ns
  1 pulse  = 850 ns
  "period" = 1300 ns
 */
void __attribute__((noinline)) led_strip_write(rgb_color * colors, unsigned int count) 
{
  // Set the pin to be an output driving low.
  LED_STRIP_PORT &= ~(1<<LED_STRIP_PIN);
  LED_STRIP_DDR |= (1<<LED_STRIP_PIN);

  cli();   // Disable interrupts temporarily because we don't want our pulse timing to be messed up.
  while(count--)
  {
    // Send a color to the LED strip.
    // The assembly below also increments the 'colors' pointer,
    // it will be pointing to the next color at the end of this loop.
    asm volatile(
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0\n"
        "rcall send_led_strip_byte%=\n"  // Send red component.
        "ld __tmp_reg__, -%a0\n"
        "rcall send_led_strip_byte%=\n"  // Send green component.
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0+\n"
        "rcall send_led_strip_byte%=\n"  // Send blue component.
        "rjmp led_strip_asm_end%=\n"     // Jump past the assembly subroutines.

        // send_led_strip_byte subroutine:  Sends a byte to the LED strip.
        "send_led_strip_byte%=:\n"
        "rcall send_led_strip_bit%=\n"  // Send most-significant bit (bit 7).
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"  // Send least-significant bit (bit 0).
        "ret\n"

        // send_led_strip_bit subroutine:  Sends single bit to the LED strip by driving the data line
        // high for some time.  The amount of time the line is high depends on whether the bit is 0 or 1,
        // but this function always takes the same time (2 us).
        "send_led_strip_bit%=:\n"
#if F_CPU == 8000000
        "rol __tmp_reg__\n"                      // Rotate left through carry.
#endif
        "sbi %2, %3\n"                           // Drive the line high.

#if F_CPU != 8000000
        "rol __tmp_reg__\n"                      // Rotate left through carry.
#endif

#if F_CPU == 16000000
        "nop\n" "nop\n"
#elif F_CPU == 20000000
        "nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU != 8000000
#error "Unsupported F_CPU"
#endif

        "brcs .+2\n" "cbi %2, %3\n"              // If the bit to send is 0, drive the line low now.

#if F_CPU == 8000000
        "nop\n" "nop\n"
#elif F_CPU == 16000000
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU == 20000000
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n"
#endif

        "brcc .+2\n" "cbi %2, %3\n"              // If the bit to send is 1, drive the line low now.

        "ret\n"
        "led_strip_asm_end%=: "
        : "=b" (colors)
        : "0" (colors),         // %a0 points to the next color to display
          "I" (_SFR_IO_ADDR(LED_STRIP_PORT)),   // %2 is the port register (e.g. PORTC)
          "I" (LED_STRIP_PIN)     // %3 is the pin number (0-8)
    );

    // Uncomment the line below to temporarily enable interrupts between each color.
    //sei(); asm volatile("nop\n"); cli();
  }
  sei();          // Re-enable interrupts now that we are done.
  _delay_us(80);  // Send the reset signal.
}




#define ACK '#'
#define READY '^'
#define DONE '%'
#define MESSAGE '#'
#define FAIL '½'

#define COLOR 'c'
#define EXECUTE 'E'
#define LCOLOR 'l'
#define HCOLOR 'h'
#define SWEEP 'S'

char ack = ACK;
unsigned char count = 0;
char data_ready = 0;
unsigned char data[5] = "     ";

void spi_init_slave (void)
{
    DDRB|=(1<<DDB4);                        //MISO as OUTPUT
    SPCR=((1<<SPE) | (1<<SPIE));                                //Enable SPI
}


ISR(SPI_STC_vect){
    if (SPSR){
        data[count] = SPDR;
    }
    if (count == 0){
        switch(data[0]) {
            case(COLOR):
                ack = COLOR;
                break;
            case(EXECUTE):
                ack = EXECUTE;
                break;
            case(LCOLOR):
                ack = LCOLOR;
                break;
            case(HCOLOR):
                ack = HCOLOR;
                break;
            case(SWEEP):
                ack = SWEEP;
                break;
            default:
                ack = ACK;
        }
    } else if (count == 4){
        ack = ACK;
        count = 0;
        data_ready = 1;
    }
    
    SPDR = ack;
    if (ack != ACK){
        count++;
    }    
}


#define LED_HEARTBEAT 6
#define LED_ACTIVITY 0

void led_blink (uint16_t i, unsigned char led)
{
    //Blink LED "i" number of times
    for (; i>0; --i)
    {
        PORTB&= ~(1<<led);
        _delay_ms(105);
        PORTB|=(1<<led);
        _delay_ms(105);
    }
}

void led_hold (uint16_t i, unsigned char led)
{
    PORTB &= ~(1<<led);
    for (; i>0; --i)
    {
        _delay_us(1);
    }
    PORTB |= (1<<led);   
}
#define LED_COUNT 21
rgb_color colours[LED_COUNT];
rgb_color colours2[LED_COUNT];
rgb_color sweep_lcolour = {250, 20, 5};
rgb_color sweep_hcolour = {100, 0, 250};
rgb_color sweep_colour = {0, 0, 0};
rgb_color colour = {255, 255, 255};

unsigned int time = 100;
unsigned char mult = 10;
unsigned char divider  = 1; 
char sweep = 0;
int loopsize = 0;
sweep_color sweep_increment = {0,0,0};



// Set colours of all leds in high or low buffers
void set_colours(unsigned char r, unsigned char g, unsigned char b, unsigned char hi){
    if (hi){
        for(int i = 0; i < LED_COUNT; i++){
            colours2[i] = (rgb_color){ r, g, b};
        }
    } else {
        for(int i = 0; i < LED_COUNT; i++){
            colours[i] = (rgb_color){ r, g, b};
        }
    }
    led_hold(1, LED_ACTIVITY);
}

void set_colours_s(rgb_color * c){
    set_colours(c->red, c->green, c->blue, 0);
}

// set single colour in low buffer
void set_one_colour(unsigned char r, unsigned char g, unsigned char b, int i){
    colours[i] = (rgb_color) {r, g, b};
}

void execute_colours(){
    led_strip_write(colours, LED_COUNT);
}

void calculate_sweep(){
    int red = 0;
    int green = 0;
    int blue = 0;
    loopsize = 0;
    red = (sweep_hcolour.red - sweep_lcolour.red );
    green = (sweep_hcolour.green - sweep_lcolour.green);
    blue = (sweep_hcolour.blue - sweep_lcolour.blue);

    if (red < 0){
        loopsize = red * (-1);
    }else if (red > 0){
        loopsize = red;
    }
    if (green < 0){
        loopsize = loopsize > (green * (-1)) ? loopsize : (green * (-1));
    }else if (green > 0){
        loopsize = loopsize > green ? loopsize : green;
    }
    
    if (blue < 0){
        loopsize = loopsize > (blue * (-1)) ? loopsize : (blue * (-1));
    }else if (blue > 0){
        loopsize = loopsize > blue ? loopsize : blue;
    }
    
    loopsize = loopsize / divider;
    
    // calculate colour increments
    sweep_increment.red = ((red * 100) / loopsize);
    sweep_increment.green = ((green * 100) / loopsize);
    sweep_increment.blue = ((blue * 100 )/ loopsize);
    sweep_colour = sweep_lcolour;;
}

// Set new sweep colour
void run_sweep(int sign, rgb_color * initial, sweep_color * inc, int round){
    sweep_colour.red = (unsigned char) (((initial->red * 100) + inc->red * sign * round) / 100) ;
    sweep_colour.green = (unsigned char) (((initial->green * 100) + inc->green * sign * round) / 100);
    sweep_colour.blue = (unsigned char) (((initial->blue * 100) + inc->blue * sign * round) / 100);

    set_colours(sweep_colour.red, sweep_colour.green, sweep_colour.blue, 0);
    execute_colours();
}



int main()
{
  //Set Data direction for ports B
  DDRB = 0x43;
  PORTB = 0x41;
  
  
  spi_init_slave();
  sei();
  _delay_ms(1500);
  //set_colours_s(&colour);
  //execute_colours();
  calculate_sweep();
  sweep = 1;

  int dir = 1;
  unsigned int loop = 0;


  SPSR = ACK;
  while(1){
      if (sweep){
          if (dir > 0){
            run_sweep(dir, &sweep_lcolour, &sweep_increment, loop);
            
          }else{
            run_sweep(dir, &sweep_hcolour, &sweep_increment, loop);  
          }
          
          loop++;
          if (loop >= loopsize){
              dir = dir > 0 ? -1 : 1;
              loop = 0;
              if (dir > 0){
                led_hold(20000, LED_HEARTBEAT);
              }
          }
          for (int t = 0; t < (time * mult); t++){
            _delay_us(10);
          }
      }else{
        led_blink(2, LED_HEARTBEAT);
      }
      if (data_ready){
          data_ready = 0;
          switch (data[0]){
              case ('c'):
                set_colours(data[1], data[2], data[3], 0);
                colour.red = data[1];
                colour.green = data[2];
                colour.blue = data[3];
                break;
              case ('C'):
                set_colours(data[1], data[2], data[3], 1);
                break;
              case ('E'):
                execute_colours();
                if (sweep){
                    sweep ^= 1;
                }
                break;
              case ('S'):
                sweep ^= 1;
                time = data[1];
                mult = data[2] + 1;
                divider = data[3] + 1;
                if (sweep){
                    calculate_sweep();
                    loop = 0;
                }else{
                    set_colours(0, 0, 0, 0);
                    execute_colours();
                }
                break;
              case ('l'):
                sweep_lcolour = (rgb_color){data[1], data[2], data[3]};
                break;
              case ('h'):
                sweep_hcolour = (rgb_color){data[1], data[2], data[3]};
                break;
          }
      }
  }
}



