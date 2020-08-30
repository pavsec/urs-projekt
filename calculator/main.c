/*
 * Created: 23.8.2020. 9:19:45
 * Authors : Petra Avsec, Filip Nikolaus, Lena Novak
 */ 
#define F_CPU 7372800UL
#define MAX_Y 320
#define MAX_X 240
#define BLACK 0x0000
#define WHITE 0xffff
#define RED 0xD369

#include <avr/io.h>
#include <util/delay.h>
#include <avr/cpufunc.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "font.c"		

/*display config*/
#define LCD_DataLow PORTA	// data pins D0-D7
#define LCD_DataHigh PORTB  // data pins D8-D15

#define LCD_RS PC0	//register select pin - choose instruction/character mode
#define LCD_CS PC7	//chip select pin
#define LCD_RD PC6	//read data
#define LCD_WR PC1	//write data

#define LCD_RESET PD7	//display reset
/*end display*/

/*touch config*/
#define T_IRQ PD0	//1 if screen is touched
#define T_OUT PD3	//get data from touch (serial)
#define T_IN PD6	//send data (x, y coordinate) to touch
#define T_CLK PD1   //touch controller clock
#define T_CS PD2	//touch chip select
/*end touch*/
#define BLANK "_______"
#define MAX_CHARS 16

unsigned int T_X, T_Y;			//x and y coordinates
char number_1[MAX_CHARS + 1] = BLANK;	//number that is being written
int number_1_mem = 0;			//written number, save it for later use
char tmp[MAX_CHARS + 1] = BLANK;		//number in memory

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


void draw_calc()
{
	//draw top menu for choosing decimal system
	draw_line(0, 40, MAX_X, 40, WHITE);
	for (int i = MAX_X / 4; i < MAX_X; i = i + MAX_X / 4)
	{
		draw_line(i, 0, i, 40, WHITE);
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
	print_str(220, 286, 3, WHITE, BLACK, "A");
	print_str(180, 286, 3, WHITE, BLACK, "B");
	print_str(140, 286, 3, WHITE, BLACK, "C");
	print_str(100, 286, 3, WHITE, BLACK, "D");
	print_str(60, 286, 3, WHITE, BLACK, "E");
	print_str(20, 286, 3, WHITE, BLACK, "F");
	
	print_str(195, 12, 2, WHITE, BLACK, "BIN");
	print_str(135, 12, 2, WHITE, BLACK, "OCT");
	print_str(75, 12, 2, WHITE, BLACK, "DEC");
	print_str(15, 12, 2, WHITE, BLACK, "HEX");
	
	print_str(200, 110, 3, WHITE, BLACK, "7");
	print_str(140, 110, 3, WHITE, BLACK, "8");
	print_str(80, 110, 3, WHITE, BLACK, "9");
	print_str(20, 110, 3, WHITE, BLACK, "/");
	
	print_str(200, 154, 3, WHITE, BLACK, "4");
	print_str(140, 154, 3, WHITE, BLACK, "5");
	print_str(80, 154, 3, WHITE, BLACK, "6");
	print_str(20, 154, 3, WHITE, BLACK, "x");
	
	print_str(200, 198, 3, WHITE, BLACK, "1");
	print_str(140, 198, 3, WHITE, BLACK, "2");
	print_str(80, 198, 3, WHITE, BLACK, "3");
	print_str(20, 198, 3, WHITE, BLACK, "+");
	
	print_str(200, 242, 3, WHITE, BLACK, "0");
	print_str(140, 242, 3, WHITE, BLACK, "CLR");
	print_str(80, 242, 3, WHITE, BLACK, "=");
	print_str(20, 242, 3, WHITE, BLACK, "-");
	
	
}

void init(void)
{
	DDRA = 0xff;
	DDRB = 0xff;
	DDRD = 0xff;
	DDRC = 0xff;
	
	DDRD = ~(_BV(T_OUT) | _BV(T_IRQ));						//input pins that read data
		
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
	
	LCD_screen_color(BLACK);
	
	draw_calc();
}

void TFT_set_cursor(signed int x_pos, signed int y_pos)
{
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
	strrev(ch);
	
	while( (ch[cnt] >= 0x20) && (ch[cnt] <= 0x7F) )
	{
		if (ch[cnt] == 0x5f) 
		{
			cnt++;
			continue;
		}
		print_char(x_pos, y_pos, font_size, colour, back_colour, ch[cnt++]);
		x_pos += 0x06;
		print_char(x_pos, y_pos, font_size, colour, back_colour, 0x20);
		x_pos += 0x06;
	}
}

char num_to_char(int n)
{
	if (n < 10)
		return n + 48;
	if (n < 16)
		return n + 'A' - 10;
	return 0;
}

int char_to_num(char c)
{
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return c - '0';
}

int convert(int system,  char *number)
{
	int n = 0;
	int negate = -(number[0] == '-');
	for (int i = -negate; number[i] != '_'; i++)
	{
		n = n * system + char_to_num(number[i]);
	}
	
	return n + negate ^ negate;
}

void convert_system(int res, int system, char *out)
{
	int n = 0, i = 0;
	char negative = res < 0;
	res = abs(res);
	do
	{
		n = res % system;
		out[i++] = num_to_char(n);
		res /= system;
	}
	while (res != 0);
	if (negative)
		out[i++] = '-';
	out[i] = 0;
	strrev(out);
	out[i] = '_';
}


int calculate(int a, int b, char sign)
{
	int result = 0;
	
	if (sign == '+') result = a + b;
	else if (sign == '-') result = a - b;
	else if (sign == '/') result = a / b;
	else if (sign == 'x') result = a * b;
	
	return result;
}

char get_clicked_number(int cnt, int system)
{
	if (cnt < MAX_CHARS)
	{
		if (system >= 16)
		{
			//A
			if (T_X >= 200 && T_X < 240 && T_Y >= 276 && T_Y < 320)
			{
				return 'A';
			}
		
			//B
			else if (T_X >= 160 && T_X < 200 && T_Y >= 276 && T_Y < 320)
			{
				return 'B';
			}
		
			//C
			else if (T_X >= 120 && T_X < 160 && T_Y >= 276 && T_Y < 320)
			{
				return 'C';
			}
		
			//D
			else if (T_X >= 80 && T_X < 120 && T_Y >= 276 && T_Y < 320)
			{
				return 'D';
			}
		
			//E
			else if (T_X >= 40 && T_X < 80 && T_Y >= 276 && T_Y < 320)
			{
				return 'E';
			}
		
			//F
			else if (T_X < 40 && T_Y >= 276 && T_Y < 320)
			{
				return 'F';
			}
		}
		
		if (system >= 10)
		{
			//8
			if (T_X >= 120 && T_X < 180 && T_Y >= 100 && T_Y < 144)
			{
				return '8';
			}
			
			//9
			else if (T_X >= 60 && T_X < 120 && T_Y >= 100 && T_Y < 144)
			{
				return '9';
			}
		}
		
		if (system >= 8)
		{
			//2
			if (T_X >= 120 && T_X < 180 && T_Y >= 188 && T_Y < 232)
			{
				return '2';
			}
		
			//3
			else if (T_X >= 60 && T_X < 120 && T_Y >= 188 && T_Y < 232)
			{
				return '3';
			}
	
			//4
			else if (T_X >= 180 && T_Y >= 144 && T_Y < 188)
			{
				return '4';
			}
		
			//5
			else if (T_X >= 120 && T_X < 180 && T_Y >= 144 && T_Y < 188)
			{
				return '5';
			}
		
			//6
			else if (T_X >= 60 && T_X < 120 && T_Y >= 144 && T_Y < 188)
			{
				return '6';
			}
		
			//7
			else if (T_X >= 180 && T_Y >= 100 && T_Y < 144)
			{
				return '7';
			}
		}
	
		
		//0
		if (T_X >= 180 && T_Y >= 232 && T_Y < 276)
		{
			return '0';
		}
	
		//1
		else if (T_X >= 180 && T_Y >= 188 && T_Y < 232)
		{
			return '1';
		}
	}
	return 0;
}

int main(void)
{
	init();
	
	int res = 0, system = 10;
	int cnt = 0;
	int calc = 0;
	char sign = '_';
	char res_print[MAX_CHARS];
	int print_calculated = 0;
	int remember_ans = 0;
	
    while (1) 
    {
		
		if (getBit(PIND, T_IRQ) == 0)
		{
			touch_read_xy();
			
			T_X = (T_X - 80) / 8;
			T_Y = (T_Y - 80) / 6;
			
			_delay_ms(500);
			
			//HEX
			if (T_X <= 60 && T_X > 0 && T_Y <= 40 && T_Y > 0)
			{
				int number = convert(system, number_1);
				system = 16;
				convert_system(number, system, number_1);
				
			}
			
			//DEC
			if (T_X >= 60 && T_X < 120 && T_Y <= 40 && T_Y > 0)
			{
				int number = convert(system, number_1);
				system = 10;
				convert_system(number, system, number_1);
			}
			
			//OCT
			if (T_X >= 120 && T_X < 180 && T_Y <= 40 && T_Y > 0)
			{
				int number = convert(system, number_1);
				system = 8;
				convert_system(number, system, number_1);
			}
			
			//BIN
			if (T_X >= 180 && T_X < 240 && T_Y <= 40 && T_Y > 0)
			{
				int number = convert(system, number_1);
				system = 2;
				convert_system(number, system, number_1);
			}
			
			char cur_num = get_clicked_number(cnt, system);
			if (cur_num)
			{
				if (remember_ans)
				{
					strcpy(number_1, BLANK);
					remember_ans = 0;
				}
				number_1[cnt++] = cur_num;
			}
			
			

			if (T_X < 60 && T_Y >= 100 && T_Y < 276)
			{
				char sign_mem = sign;
					
				// /
				if (T_X < 60 && T_Y >= 100 && T_Y < 144)
				{
					sign = '/';
				}
					
				//x
				else if (T_X < 60 && T_Y >= 144 && T_Y < 188)
				{
					sign = 'x';
				}
					
				//+
				else if (T_X < 60 && T_Y >= 188 && T_Y < 232)
				{
					sign = '+';
				}
					
				//-
				else if (T_X < 60 && T_Y >= 232 && T_Y < 276)
				{
					sign = '-';
				}
					
				if (calc)
				{
					if (sign_mem == '_') sign_mem = sign;
						
					int a;
					a = convert(system, number_1);
						
					res = calculate(number_1_mem, a, sign_mem);
					number_1_mem = res;
					convert_system(res, system, number_1);
				}
				else 
				{
					number_1_mem = convert(system, number_1);
					calc = 1;
				}
					
				remember_ans = 1;
				print_calculated = 1;
				cnt = 0;
				//strcpy(number_1, BLANK);
			}
			//=
			else if (T_X >= 60 && T_X < 120 && T_Y >= 232 && T_Y < 276)
			{
				cnt = 0;
				print_calculated = 1;
					
				int a;
				a = convert(system, number_1);
					
				res = calculate(number_1_mem, a, sign);
				number_1_mem = res;
				//strcpy(number_1, BLANK);
				convert_system(res, system, number_1);
					
				sign = '_';
			}
						
			//CLR
			if (T_X >= 120 && T_X < 180 && T_Y >= 232 && T_Y < 276)
			{
				print_str(20, 60, 3, WHITE, BLACK, "                ");
				
				strcpy(number_1, BLANK);
				sign = '_';
				res = 0;
				number_1_mem = 0;
				cnt = 0;
				calc = 0;
				print_calculated = 0;
				continue;
			}
			
			if (!print_calculated)
			{
				print_str(20, 60, 3, WHITE, BLACK, "                ");
					
				res = convert(system, number_1);
			}
			else
			{	
				print_calculated = 0;
				res = number_1_mem;
			}
		
			/*if (system != 10)
			{
				res = convert_system(res, system);
				system = 10;
			}*/
			
//			sprintf(res_print, "%d", res);
			for (uint8_t i = 0; i < MAX_CHARS; i++)
			{
				if (number_1[i] == '_')
				{
					res_print[i] = 0;
					break;
				}
				res_print[i] = number_1[i];
				
			}
			//strcpy(res_print, number_1);
			print_str(20, 60, 3, WHITE, BLACK, res_print);
		
		}
    }
}

