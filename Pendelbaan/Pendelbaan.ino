/*
 Name:		Pendelbaan.ino
 Created:	2/10/2019 11:11:06 AM
 Author:	Rob Antonisse
 Web: www.wisselmotor.nl
*/
#include <EEPROM.h> //use build in eeprom program
byte COM_reg;
byte SW_old;
unsigned long SW_time;
unsigned long PWM_freq;
unsigned long PWM_duty;
byte PWM_cycle = 40; //=
byte PWM_speed;

unsigned long tijdmeting;


void setup() {
	Serial.begin(9600);
	//EEPROM_clear();  //optional clear the eeprom memory
	DDRC = 0; //set port C as input (pins A0~A5)
	PORTC = 0xFF; //set pull up resisters to port C
	SW_old = PINC;
	//set pin7,8 as output
	DDRB |= (1 << 0);
	DDRD |= (1 << 7);
	//start speed
	PWM_speed = 20;
}

void EEPROM_clear() {
	//resets the eeprom memory to 0xFF
	for (int i = 0; i < EEPROM.length(); i++) {
		EEPROM.write(i, 0xFF);
	}
	Serial.print("eeprom memory set to 0xFF");
}

void PWM_clk() {
	if (millis() - PWM_freq > PWM_cycle) {
		PWM_freq = millis();
		PORTD |= (1 << 7);
		COM_reg |= (1 << 0);
		PWM_duty = millis();
	}
	if (bitRead(COM_reg, 0) == true) {
		if (millis() - PWM_duty > PWM_speed) {
			PORTD &= ~(1 << 7);
			COM_reg &= ~(1 << 0);
		}
	}
}

void SW_read() {
	int tijd;
	byte changed;
	byte swnew;
	swnew = PINC;
	changed = swnew ^ SW_old;
	if (changed > 0) {

		for (byte i = 0; i < 6; i++) {
			if (bitRead(changed, i) == true) {
				//Serial.print("Poort ");
				//Serial.print(i);

				if (bitRead(swnew, i) == true) {
					//Serial.println(" los.");
					switch (i) {
					case 4:
						if (bitRead(PINB, 0) == true) {
							PORTB &= ~(1 << 0);
							PWM_speed--;
						}

						break;
					case 5:
						if (bitRead(PINB, 0) == false) {
							PORTB |= (1 << 0);
							tijd = (millis() - tijdmeting) / 1000;
							PWM_speed--;
							Serial.print("Snelheid: ");
							Serial.print(PWM_speed);
							Serial.print(",  tijd heen en terug: ");
							Serial.print(tijd);
							Serial.println(" sec.");
							tijdmeting = millis();
						}
						break;
					}
				}
				else {
					//Serial.println(" ingedrukt.");
					switch (i) {
					case 0:
						if (bitRead(swnew, 1) == false) {
							SW_both();
						}
						else {
							PORTB ^= (1 << 0); //toggle direction
						}
						break;
					case 1:
						if (bitRead(swnew, 0) == false) {
							SW_both();
							COM_reg ^= (1 << 1); //enable motor pwm
						}
						else {
							PORTD ^= (1 << 7); //toggle on/off					
							COM_reg ^= (1 << 1); //enable motor pwm
						}
						if (bitRead(COM_reg, 1) == false)PORTD &= ~(1 << 7);
						break;
					}
				}
			}

		}
	}
	SW_old = swnew;
}

void SW_both() {
	Serial.println("beide knoppen");
	PWM_speed = 30;
}
void loop() {
	//pwm 
	if (bitRead(COM_reg, 1) == true) PWM_clk();

	//check input
	if (millis() - SW_time > 50) {
		SW_time = millis();
		SW_read();
	}
}
