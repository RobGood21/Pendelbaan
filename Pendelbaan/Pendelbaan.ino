/*
 Name:		Pendelbaan.ino
 Created:	2/10/2019 11:11:06 AM
 Author:	Rob Antonisse
 Web: www.wisselmotor.nl
*/
#include <EEPROM.h> //use build in eeprom program
byte COM_reg;
byte INIT_reg;

byte SW_old;
unsigned long SW_time;
unsigned long PWM_freq;
unsigned long PWM_duty;
byte PWM_cycle = 12; //=
byte PWM_speed = 10;

byte PWM_min = 40;
byte PWM_max = 10;

byte RUN_countA;
byte RUN_countD;

byte INIT_fase;


int RUN_dectijd;
int routeDT[2]; //routes have there own stoptime, locs have different speeds
byte route;
byte laststop; //vorige stop

unsigned long Dectime;


unsigned int RUN_stoptijd; //stoptijd in seconden
unsigned long RUN_time;

byte traject;

unsigned long RUN_meting;


void setup() {
	Serial.begin(9600);
	//EEPROM_clear();  //optional clear the eeprom memory
	DDRC = 0; //set port C as input (pins A0~A5)
	PORTC = 0xFF; //set pull up resisters to port C
	SW_old = PINC;
	//set pin7,8 as output
	DDRB |= (1 << 0);
	DDRD |= (1 << 7);
	RUN_dectijd = 0xFF; //max tijd


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

void RUN_init() {
	Serial.println("nu dus beginnen");
	PWM_cycle = PWM_max;
	COM_reg |= (1 << 1); //start PWM send, motor on
	COM_reg &= ~(1 << 6); //disable slow down
	INIT_reg |= (1 << 0); //request tijdmeting
	RUN_dectijd = 0xFF; //max time to slow down
}

void RUN_start() {
	COM_reg |= (1 << 1);
	COM_reg |= (1 << 5);
	PWM_cycle = PWM_min;
	RUN_meting = millis();
	
	if (bitRead(INIT_reg, 0) == true) {
		INIT_reg |= (1 << 1); //set program slow down time
		INIT_reg &= ~(1 << 0); //reset reqest
	}
	else {
	COM_reg |= (1 << 6); //activate run_dec()
	Dectime = millis();

	}
}
void RUN_acc() { //called every 20ms from loop

	RUN_countA++;
	if (RUN_countA > 10) { // random(0, 15);
		RUN_countA = 0;
		PWM_cycle--;
		if (PWM_cycle <= PWM_max) {
			COM_reg &= ~(1 << 5);
		}
	}
}

void RUN_dec() {

	RUN_countD++;
	if (RUN_countD > 10) {

		RUN_countD = 0;
		if (millis() - Dectime > RUN_dectijd) {
			COM_reg &= ~(1 << 5);  //stop versnellen
			PWM_cycle++;
			Serial.println(PWM_cycle);
			if (PWM_cycle >= PWM_min) COM_reg &= ~(1 << 6);  //stop vertragen.
		}
	}
}


void RUN_stop(byte station) {
	if (laststop > 0) {
	if (laststop == 4 & station == 5)route = 0;
	if (laststop == 5 & station == 4)route = 1;
	}
	else {
		route = 20; //no route during init
	}
	laststop = station;

	Serial.print("route= ");
	Serial.println(route);

	TIJDmeting();
	PORTD &= ~(1 << 7); //PWM poort low			
	COM_reg ^= (1 << 1); //enable motor pwm
	RUN_stoptijd = 2000;
	RUN_time = millis();
	COM_reg &= ~(1 << 6);  //stop vertragen.
}

void SW_read() {

	byte changed;
	byte swnew;
	swnew = PINC;
	changed = swnew ^ SW_old;
	if (changed > 0) {

		for (byte i = 0; i < 6; i++) {
			if (bitRead(changed, i) == true) {
				if (bitRead(swnew, i) == true) {
					//Serial.println(" los.");
					switch (i) {
					case 4:
						if (bitRead(PINB, 0) == true) {
							PORTB &= ~(1 << 0);
							RUN_stop(i);
							//PWM_speed--;
							//PWM_cycle = PWM_cycle + 5;
						}

						break;
					case 5:
						if (bitRead(PINB, 0) == false) {
							PORTB |= (1 << 0);
							RUN_stop(i);
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
						COM_reg ^= (1 << 4); //manual stop

						if (bitRead(COM_reg, 4) == false) { //all stop
							COM_reg &= ~(1 << 0);
							PORTD &= ~(1 << 7);

						}
						else { //Start manual
							RUN_init();
						}


						break;
					}
				}
			}

		}
	}
	SW_old = swnew;
}


void TIJDmeting() { //0-9 routes 10=niet opslaan
	int tijd;

	tijd = (millis() - RUN_meting) / 1000;
	if (bitRead(INIT_reg, 1) == true) {
		RUN_dectijd = (tijd - 1) * 1000;
		INIT_reg &= ~(1 << 1); //reset initialise
	}


	Serial.print("Vertragen na: ");
	Serial.println(RUN_dectijd);

	//Serial.print("Snelheid: ");
	//Serial.print(PWM_cycle);
	Serial.print("Tijd  ");
	Serial.print(tijd);
	Serial.println(" sec.");

}
void SW_both() {
	Serial.println("beide knoppen");
	//PWM_speed = 30;
}
void loop() {

	if (bitRead(COM_reg, 4) == true) {
		//pwm 
		if (bitRead(COM_reg, 1) == true) {
			PWM_clk();
		}
		else {
			if (millis() - RUN_time > RUN_stoptijd) { //& bitRead(COM_reg,7)==true
				RUN_start();
			}
		}
	}

	//check input
	if (millis() - SW_time > 20) {
		SW_time = millis();
		SW_read();

		if (bitRead(COM_reg, 5) == true)RUN_acc(); //versnellen
		if (bitRead(COM_reg, 6) == true) RUN_dec(); //vertragen
	}
}
