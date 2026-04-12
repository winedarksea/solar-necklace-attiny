MCU        = attiny416
F_CPU      = 3333333UL
PORT       = /dev/cu.usbserial-10
BAUD       = 115200

AVR_GCC_PREFIX = /opt/homebrew/opt/avr-gcc@14/bin

CC      = $(AVR_GCC_PREFIX)/avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size

TARGET  = main

CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=gnu11 \
          -flto -ffunction-sections -fdata-sections
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections -flto

all: $(TARGET).hex

$(TARGET).elf: $(TARGET).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

flash: $(TARGET).hex
	avrdude -p t416 -c serialupdi -P $(PORT) -b $(BAUD) -U flash:w:$<:i

size: $(TARGET).elf
	$(SIZE) --mcu=$(MCU) -C $<

clean:
	rm -f $(TARGET).elf $(TARGET).hex

.PHONY: all flash size clean
