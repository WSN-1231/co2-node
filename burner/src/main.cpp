#include <Arduino.h>
#include <EEPROM.h>

void setup()
{
	EEPROM.begin();
	Serial.begin(9600);
	EEPROM.write(0x0fff, 01);
	EEPROM.write(0x0ffe, 00);
	Serial.println(EEPROM.read(0x0ffe), OCT);
	Serial.println(EEPROM.read(0x0fff), OCT);
}

void loop(/* arguments */) {
	;
}
