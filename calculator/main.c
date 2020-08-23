/*
 * Created: 23.8.2020. 9:19:45
 * Authors : Petra Avsec, Filip Nikolaus, Lena Novak
 */ 
#define F_CPU 7372800UL
#define MAX_Y 320
#define MAX_X 240
#define BLACK 0x0000
#define WHITE 0xffff

#include <avr/io.h>
#include <util/delay.h>
#include <avr/cpufunc.h>
#include <string.h>
#include <stdbool.h>
#include "font.c"		
#include "calculatorFunc.c"

/*display config*/
#define LCD_DataLow PORTA	// data pins D0-D7
#define LCD_DataHigh PORTB  // data pins D8-D15

#define LCD_RS PC0	//register select pin - choose instruction/character mode
#define LCD_CS PC1	//chip select pin
#define LCD_RD PC6	//read data
#define LCD_WR PC7	//write data

#define LCD_RESET PD7	//display reset
/*end display*/

/*touch config*/
#define T_IRQ PD0	//1 if screen is touched
#define T_OUT PD1	//get data from touch (serial)
#define T_IN PD2	//send data (x, y coordinate) to touch
#define T_CLK PD3   //touch controller clock
#define T_CS PD4	//touch chip select
/*end touch*/

unsigned int T_X, T_Y;	//x and y coordinates

bool getBit(int reg, int offset) {
	return !!( (reg >> offset) & 1 );
}

void touch_start() {
	PORTD |= _BV(T_CS) | _BV(T_CLK) | _BV(T_IN);       
}

void touch_write(unsigned char num) {	
	int count = 0;
	PORTD &= ~_BV(T_CLK);
	for(count = 7; count >= 0; count--){
		
		if ( getBit(num,count) ) {
			PORTD |= _BV(T_IN);
			} else {
			PORTD &= ~_BV(T_IN);
		}
		PORTD &= ~_BV(T_CLK);
		PORTD |= _BV(T_CLK);
	}
}

unsigned int touch_read_char() {						//read data from ADC (touch) 
	unsigned char count = 0;
	unsigned int Num = 0;
	for(count = 0; count < 12; count++){
		Num <<= 1;
		PORTD |= _BV(T_CLK);					//high signal to T_CLK
		PORTD &= ~_BV(T_CLK);					//low signal to T_CLK - initialize data transfer (from datasheet)
		Num += getBit(PIND, T_OUT);				//touch ADC - 12 bits, get 1bit at a time from T_OUT
	}
	
	return Num;
}

void touch_read_xy(void)													//touch read x, y coordinate
{
	_delay_ms(1);
	PORTD &= ~_BV(T_CS);													//to start transmission, CS is set to low
	
	touch_write(0x90);       	                                            //send command to touch to get ready to write y coordinate
	_delay_ms(1);
	PORTD |= _BV(T_CLK); 
	_NOP();  _NOP();  _NOP();  _NOP();										//do nothing, delay for next instruction
	PORTD &= ~_BV(T_CLK); 
	_NOP();  _NOP();  _NOP();  _NOP();
	T_Y = touch_read_char();
	
	touch_write(0xD0);														//send command to touch to get ready to write x coordinate
	PORTD |= _BV(T_CLK); 
	_NOP();  _NOP();  _NOP();  _NOP();
	PORTD &= ~_BV(T_CLK); 
	_NOP();  _NOP();  _NOP();  _NOP();
	T_X = touch_read_char();
	
	PORTD |= _BV(T_CS);														//to end transmission, CS is set to high
}

void int_to_str(int n, char *str)		//for LCD output
{
	str[0] = n / 10000 + 48;
	str[1] = (n / 1000) - ( (n / 10000) * 10 ) + 48;
	str[2] = ( n/ 100) - ( (n / 1000) * 10 ) + 48;
	str[3] = (n / 10) - ( (n / 100) * 10 ) + 48;
	str[4] = n - ( (n / 10) * 10 ) + 48;
	str[5] = 0;
}


void LCD_write_cmd(int  DH)	
{
	PORTC &= ~_BV(LCD_RS);
	PORTC &= ~_BV(LCD_CS);
	LCD_DataHigh = DH >> 8;
	LCD_DataLow = DH;
	PORTC |= _BV(LCD_WR);
	PORTC &= ~_BV(LCD_WR);
	PORTC |= _BV(LCD_CS);	
}

void LCD_write_color(char hh, char ll)	
{
	PORTC |= _BV(LCD_RS);
	PORTC &= ~_BV(LCD_CS);
	LCD_DataHigh = hh;
	LCD_DataLow = ll;
	PORTC |= _BV(LCD_WR);
	PORTC &= ~_BV(LCD_WR);
	PORTC |= _BV(LCD_CS);
}

void LCD_write_data(int DH)	
{
	PORTC |= _BV(LCD_RS);
	PORTC &= ~_BV(LCD_CS);
	LCD_DataHigh = DH >> 8;
	LCD_DataLow = DH;
	PORTC |= _BV(LCD_WR);
	PORTC &= ~_BV(LCD_WR);
	PORTC |= _BV(LCD_CS);
}


void LCD_write_cmd_data(int com1, int dat1)				//write cmd and save to memory
{
	LCD_write_cmd(com1);
	LCD_write_data(dat1);
}

void address_set(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)	//set up memory to draw on
{
	LCD_write_cmd_data(0x0044, (x2 << 8) + x1);
	LCD_write_cmd_data(0x0045, y1);
	LCD_write_cmd_data(0x0046, y2);
	LCD_write_cmd_data(0x004e, x1);
	LCD_write_cmd_data(0x004f, y1);
	LCD_write_cmd(0x0022);
}


void init(void)
{
	DDRA = 0xff;
	DDRB = 0xff;
	DDRD = 0xff;
	DDRC = 0xff;
	
	DDRD = ~(_BV(T_OUT) | _BV(T_IRQ));						//pinovi kao ulazni za primanje podataka
	
	TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);		//set on bottom, clear on match
	TCCR1B =  _BV(WGM12) | _BV(WGM13) | _BV(CS11);			//WG - fast PWM, top on ICR1, prescaler = 8
	
	ICR1 = 18431;											// top - 50Hz/20ms
	
	OCR1A = 1500;
	OCR1B = 1500;
	
	DDRD |= (1 << PD6) | (1 << PD5);						//pinovi kao izlazni za pwm
	
	//LCD config setup
	PORTD |= _BV(LCD_RESET);
	_delay_ms(5);
	PORTD &= ~_BV(LCD_RESET);
	_delay_ms(10);
	PORTD |= _BV(LCD_RESET);
	PORTC |= _BV(LCD_CS);
	PORTC |= _BV(LCD_RD);
	PORTC &= ~_BV(LCD_WR);
	_delay_ms(20);

	LCD_write_cmd_data(0x0000,0x0001);    _delay_ms(1);
	LCD_write_cmd_data(0x0003,0xA8A4);    _delay_ms(1);
	LCD_write_cmd_data(0x000C,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x000D,0x080C);    _delay_ms(1);
	LCD_write_cmd_data(0x000E,0x2B00);    _delay_ms(1);
	LCD_write_cmd_data(0x001E,0x00B0);    _delay_ms(1);
	LCD_write_cmd_data(0x0001,0x2B3F);    _delay_ms(1);
	LCD_write_cmd_data(0x0002,0x0600);    _delay_ms(1);
	LCD_write_cmd_data(0x0010,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0011,0x6070);    _delay_ms(1);
	LCD_write_cmd_data(0x0005,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0006,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0016,0xEF1C);    _delay_ms(1);
	LCD_write_cmd_data(0x0017,0x0003);    _delay_ms(1);
	LCD_write_cmd_data(0x0007,0x0233);    _delay_ms(1);
	LCD_write_cmd_data(0x000B,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x000F,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0041,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0042,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0048,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0049,0x013F);    _delay_ms(1);
	LCD_write_cmd_data(0x004A,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x004B,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0044,0xEF00);    _delay_ms(1);
	LCD_write_cmd_data(0x0045,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0046,0x013F);    _delay_ms(1);
	LCD_write_cmd_data(0x0030,0x0707);    _delay_ms(1);
	LCD_write_cmd_data(0x0031,0x0204);    _delay_ms(1);
	LCD_write_cmd_data(0x0032,0x0204);    _delay_ms(1);
	LCD_write_cmd_data(0x0033,0x0502);    _delay_ms(1);
	LCD_write_cmd_data(0x0034,0x0507);    _delay_ms(1);
	LCD_write_cmd_data(0x0035,0x0204);    _delay_ms(1);
	LCD_write_cmd_data(0x0036,0x0204);    _delay_ms(1);
	LCD_write_cmd_data(0x0037,0x0502);    _delay_ms(1);
	LCD_write_cmd_data(0x003A,0x0302);    _delay_ms(1);
	LCD_write_cmd_data(0x003B,0x0302);    _delay_ms(1);
	LCD_write_cmd_data(0x0023,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x0024,0x0000);    _delay_ms(1);
	LCD_write_cmd_data(0x004f,0);
	LCD_write_cmd_data(0x004e,0);
	LCD_write_cmd(0x0022);
}


void LCD_screen_color(unsigned int color)
{
	int i,j;
	address_set(0, 0, 239, 319);

	for(i = 0; i < 320; i++)
	{
		for (j = 0; j < 240; j++)
		{
			LCD_write_data(color);
		}
	}
}

void swap(signed int *a, signed int *b)
{
	return;
	signed int temp = 0x0000;

	temp = *b;
	*b = *a;
	*a = temp;
}

void TFT_set_cursor(signed int x_pos, signed int y_pos)
{
	swap(&x_pos, &y_pos);
	
	y_pos = (MAX_Y - 1 - y_pos);
	
	LCD_write_cmd_data(0x004E, x_pos);
	LCD_write_cmd_data(0x004F, y_pos);
	LCD_write_cmd(0x0022);
}

void draw_pixel(signed int x_pos, signed int y_pos, unsigned int colour)
{
	if((x_pos >= MAX_X) || (y_pos >= MAX_Y) || (x_pos < 0) || (y_pos < 0)) return;
	
	PORTC &= ~_BV(LCD_CS);
	TFT_set_cursor(x_pos, y_pos);
	LCD_write_data(colour);
	PORTC |= _BV(LCD_CS);
}

void draw_line(signed int x1, signed int y1, signed int x2, signed int y2, unsigned int colour)
{
	signed int dx = 0x0000;
	signed int dy = 0x0000;
	signed int stepx = 0x0000;
	signed int stepy = 0x0000;
	signed int fraction = 0x0000;

	dy = (y2 - y1);
	dx = (x2 - x1);

	if(dy < 0)
	{
		dy = -dy;
		stepy = -1;
	}
	else
	{
		stepy = 1;
	}

	if(dx < 0)
	{
		dx = -dx;
		stepx = -1;
	}
	else
	{
		stepx = 1;
	}

	dx <<= 0x01;
	dy <<= 0x01;

	draw_pixel(x1, y1, colour);

	if(dx > dy)
	{
		fraction = (dy - (dx >> 1));
		while(x1 != x2)
		{
			if(fraction >= 0)
			{
				y1 += stepy;
				fraction -= dx;
			}
			
			x1 += stepx;
			fraction += dy;

			draw_pixel(x1, y1, colour);
		}
	}
	else
	{
		fraction = (dx - (dy >> 1));

		while(y1 != y2)
		{
			if (fraction >= 0)
			{
				x1 += stepx;
				fraction -= dy;
			}
			y1 += stepy;
			fraction += dx;
			draw_pixel(x1, y1, colour);
		}
	}
}


void draw_line_vertical(signed int x1, signed int y1, signed int y2, unsigned colour)
{
	if(y1 > y2)
	{
		swap(&y1, &y2);
	}
	
	while(y2 > (y1 - 1))
	{
		draw_pixel(x1, y2, colour);
		y2--;
	}
}

void draw_line_horizontal(signed int x1, signed int x2, signed int y1, unsigned colour)
{
	if(x1 > x2)
	{
		swap(&x1, &x2);
	}

	while(x2 > (x1 - 1))
	{
		draw_pixel(x2, y1, colour);
		x2--;
	}
}

void draw_font_pixel(unsigned int x_pos, unsigned int y_pos, unsigned int colour, unsigned char pixel_size)
{
	int i = 0x0000;

	PORTC &= ~_BV(LCD_CS);
	
	TFT_set_cursor(x_pos, y_pos);
	
	for(i = 0x0000; i < (pixel_size * pixel_size); i++)
	{
		LCD_write_data(colour);
	}
	
	PORTC |= _BV(LCD_CS);
}

void print_char(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, char ch)
{
	signed char i = 0x00;
	unsigned char j = 0x00;

	unsigned int value = 0x0000;

	if(font_size <= 0)
	{
		font_size = 1;
	}

	if(x_pos < font_size)
	{
		x_pos = font_size;
	}

	for(i = 0x04; i >= 0x00; i--)
	{
		for(j = 0x00; j < 0x08; j++)
		{
			value = 0x0000;
			value = ( (font[ ( (unsigned char)ch ) - 0x20 ][i]) );

			if(((value >> j) & 0x01) != 0x00)
			{
				draw_font_pixel(x_pos, y_pos, colour, font_size);
			}
			else
			{
				draw_font_pixel(x_pos, y_pos, back_colour, font_size);
			}

			y_pos += font_size;
		}
		y_pos -= (font_size << 0x03);
		x_pos++;
	}
}

void print_str(unsigned int x_pos, unsigned int y_pos, unsigned char font_size, unsigned int colour, unsigned int back_colour, char *ch)
{
	int cnt = 0;
	
	do
	{
		print_char(x_pos, y_pos, font_size, colour, back_colour, ch[cnt++]);
		x_pos += 0x06;
	} while( (ch[cnt] >= 0x20) && (ch[cnt] <= 0x7F) );
}


void draw_calc()
{
	//draw top menu for choosing decimal system
	draw_line(0, 20, MAX_X, 20, WHITE);
	for (int i = MAX_X / 4; i < MAX_X; i = i + MAX_X / 4)
	{
		draw_line(i, 0, i, 20, WHITE);
	}
	
	//draw actual calculator lines
	for (int j = 100; j < MAX_Y; j = j + (MAX_Y - 100) / 5)
	{
		draw_line(0, j, MAX_X, j, WHITE);
	}
	
	for (int i = MAX_X / 4; i < MAX_X; i = i + MAX_X / 4)
	{
		draw_line(i, 100, i, MAX_Y - (MAX_Y - 100) / 5, WHITE);
	}
	
	for (int i = MAX_X / 5; i < MAX_X; i = i + MAX_X / 6)
	{
		draw_line(i, MAX_Y - (MAX_Y - 100) / 6, i, MAX_Y, WHITE);
	}
	
	//draw characters
	print_str(200, 5, 2, WHITE, BLACK, "N I B \0");
	print_str(140, 5, 2, WHITE, BLACK, "T C O \0");
	print_str(80, 5, 2, WHITE, BLACK, "C E D \0");
	print_str(20, 5, 2, WHITE, BLACK, "X E H \0");
	
	print_str(200, 110, 3, WHITE, BLACK, "7 \0");
	print_str(140, 110, 3, WHITE, BLACK, "8 \0");
	print_str(80, 110, 3, WHITE, BLACK, "9 \0");
	print_str(20, 110, 3, WHITE, BLACK, "/ \0");
	
	print_str(200, 154, 3, WHITE, BLACK, "4 \0");
	print_str(140, 154, 3, WHITE, BLACK, "5 \0");
	print_str(80, 154, 3, WHITE, BLACK, "6 \0");
	print_str(20, 154, 3, WHITE, BLACK, "x \0");
	
	print_str(200, 198, 3, WHITE, BLACK, "1 \0");
	print_str(140, 198, 3, WHITE, BLACK, "2 \0");
	print_str(80, 198, 3, WHITE, BLACK, "3 \0");
	print_str(20, 198, 3, WHITE, BLACK, "+ \0");
	
	print_str(200, 242, 3, WHITE, BLACK, "0 \0");
	print_str(140, 242, 3, WHITE, BLACK, "R L C \0");
	print_str(80, 242, 3, WHITE, BLACK, "= \0");
	print_str(20, 242, 3, WHITE, BLACK, "- \0");
	
	print_str(220, 286, 3, WHITE, BLACK, "A \0");
	print_str(180, 286, 3, WHITE, BLACK, "B \0");
	print_str(150, 286, 3, WHITE, BLACK, "C \0");
	print_str(100, 286, 3, WHITE, BLACK, "D \0");
	print_str(60, 286, 3, WHITE, BLACK, "E \0");
	print_str(20, 286, 3, WHITE, BLACK, "F \0");
}

int main(void)
{
	init();
	
	LCD_screen_color(BLACK);
	
	draw_calc();
	
    while (1) 
    {
    }
}

