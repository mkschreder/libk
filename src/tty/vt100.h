/**
	This file is part of martink project.

	martink firmware project is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	martink firmware is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with martink firmware.  If not, see <http://www.gnu.org/licenses/>.

	Author: Martin K. Schröder
	Email: info@fortmax.se
	Github: https://github.com/mkschreder
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "tty.h"
#include <serial/serial.h>

#define VT100_CHAR_WIDTH 6
#define VT100_CHAR_HEIGHT 8

#define VT100_MAX_COMMAND_ARGS 4

#define VT100_FLAG_CURSOR_WRAP (1 << 0)
#define VT100_FLAG_SCROLL_MODE (1 << 1)
#define VT100_FLAG_ORIGIN_MODE (1 << 2)

struct vt100 {
	uint8_t flags;

	struct tty_device *display;
	
	//uint16_t screen_width, screen_height;
	// cursor position on the screen (0, 0) = top left corner. 
	uint16_t cursor_x, cursor_y;
	uint16_t saved_cursor_x, saved_cursor_y; // used for cursor save restore
	uint16_t scroll_start_row, scroll_end_row; 
	// character width and height
	//uint8_t char_width, char_height;
	// colors used for rendering current characters
	uint16_t back_color, front_color;
	// the starting y-position of the screen scroll
	uint16_t scroll_value; 
	// command arguments that get parsed as they appear in the terminal
	uint8_t narg; uint16_t args[VT100_MAX_COMMAND_ARGS];
	// current arg pointer (we use it for parsing) 
	uint8_t carg;
	uint16_t screen_height, screen_width;
	
	void (*state)(struct vt100 *term, uint8_t ev, uint16_t arg);
	//void (*send_response)(char *str);
	void (*ret_state)(struct vt100 *term, uint8_t ev, uint16_t arg);
	
	struct serial_device serial; 
};

//void vt100_init(void (*send_response)(char *str));
void vt100_init(struct vt100 *self, tty_dev_t display); 
void vt100_putc(struct vt100 *self, uint8_t ch);
void vt100_puts(struct vt100 *self, const char *str);
struct serial_device *vt100_to_serial_device(struct vt100 *self); 

#ifdef __cplusplus
}
#endif
