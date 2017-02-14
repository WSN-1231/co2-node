#include <Arduino.h>
#include <EEPROM.h>

int address = 00;

void setup()
{
	EEPROM.begin();
	address = ((uint16_t) EEPROM.read(0x0ffe) << 8) | EEPROM.read(0x0fff);
	Serial.begin(9600);
	Serial.print("Current address: "); Serial.println(address, OCT);
	Serial.print("Enter new address: ");
	address = Serial.parseInt();
	EEPROM.write(0x0fff, address | 8);
	EEPROM.write(0x0ffe, (address >> 8));
	Serial.print("New address: ");
	Serial.println(((uint16_t)EEPROM.read(0x0fff) << 8) | EEPROM.read(0x0ffe), OCT);
}

void loop(/* arguments */) {
	;
}
