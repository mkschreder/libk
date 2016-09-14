includedir=$(prefix)/include/libk/arm32/
lib_LTLIBRARIES=\
libk-avr.la 
include_HEADERS=
AM_CFLAGS=$(CODE_COVERAGE_CFLAGS) -std=gnu99 -Wall -Werror -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
-Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
-Wshadow -Wcast-align -Wchar-subscripts -Winline \
-Wnested-externs -Wredundant-decls -Wmissing-field-initializers -Wextra \
-Wformat=2 -Wno-format-nonliteral -Wpointer-arith -Wno-missing-braces \
-Wno-unused-parameter -Wno-unused-variable -Wno-inline

#libk_arm32_la_SOURCES=arm_adc.c arm_dma.c arm_gpio.c arm_pwm.c arm_spi.c arm_time.c arm_twi.c arm_uart.c
#libk_arm32_la_CFLAGS=$(AM_CFLAGS)

libk_avr_la_SOURCES=\
atmega_adc.c \
atmega_eeprom.c \
atmega_gpio.c \
atmega_i2c.c \
atmega_pwm.c \
atmega_spi.c \
atmega_uart.c 
libk_arm32_stm32f10x_la_CFLAGS=$(AM_CFLAGS) -DF_CPU=16000000

