#include <Arduino.h>
#include <EEPROM.h>

// format alamat dalam big endian
#define NEW_ADDRESS 01

int address = 00;

void setup()
{
	EEPROM.begin();
	address = ((uint16_t) EEPROM.read(0x0ffe) << 8) | EEPROM.read(0x0fff);
	// Serial.setTimeout(500000);
	Serial.begin(9600);
	Serial.print("Current address: "); Serial.println(address, OCT);

	address = (uint16_t)NEW_ADDRESS;
	Serial.print("New address is "); Serial.println(address, OCT);
	Serial.println("Write address? (press enter to write)");
	while(!Serial.available())
		;
	char addr_low = address & 0xFF;
	char addr_high = address >> 8;
	EEPROM.write(0x0ffe, addr_high);
	EEPROM.write(0x0fff, addr_low);
	Serial.print("New address: ");
	Serial.println(((uint16_t)EEPROM.read(0x0ffe) << 8) | EEPROM.read(0x0fff), OCT);
}

void loop(/* arguments */) {
	;
}
