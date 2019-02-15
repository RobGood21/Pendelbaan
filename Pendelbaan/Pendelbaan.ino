/*
 Name:		Pendelbaan.ino
 Created:	2/10/2019 11:11:06 AM
 Author:	Rob Antonisse
 Web: www.wisselmotor.nl
*/
#include <EEPROM.h> //use build in eeprom program
byte COM_reg;

byte INIT_reg;
byte INIT_fase;
int INIT_count;
int tijd; //tijd meting duration route


byte SW_old;
unsigned long SW_time;
unsigned long PWM_freq;
unsigned long PWM_duty;

byte PWM_cycle = 12; //=
byte PWM_speed = 10;

byte PWM_min = 50;
byte PWM_max = 10;

byte RUN_count[4];


int stoptijd;//tijd wanneer na begin route afremmen begint
int routetijd[2]; //routes have there own stoptime, locs have different speeds 0=4>5 1=5>4
byte laststop = 0; //vorige stop

unsigned long Dectime;


unsigned int wachttijd; //stoptijd in seconden
unsigned long RUN_time;

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
	stoptijd = 0xFFFFFFFF; //max tijd	
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

void INIT_exe() {
	COM_reg &= ~(1 << 7); //reset request flag
	switch (INIT_fase) {
	case 0:
		INIT_reg &= ~(1 << 3); // reset teller voor route tijd metingen 
		Serial.println("begin initialisatie");
		//staat loc in station? bepaal huidig station
		INIT_station();

		Serial.print("Begin station: ");
		Serial.println(laststop);

		switch (laststop) { //set direction
		case 4:
			PORTB &= ~(1 << 0);
			COM_reg |= (1 << 7);
			break;

		case 5:
			PORTB |= (1 << 0);
			COM_reg |= (1 << 7);
			break;



		default:
			//niet in station dus station opzoeken.
			PWM_cycle = PWM_max;
			COM_reg |= (1 << 1); //start PWM send, motor on
			COM_reg &= ~(1 << 6); //disable slow down
			break;
		}
		INIT_fase = 1;
		break;


	case 1: //check minimum speed (if loc is moving then)

		Serial.println("begin minimumsnelheids meting");
		//stoploc();
		//delay(1000);
		PWM_cycle = PWM_min;
		COM_reg |= (1 << 1); //start PWM send, motor on
		INIT_fase = 2;
		COM_reg |= (1 << 7); //request init
		break;
	case 2:
		//check station
		INIT_station();

		if (laststop == 0) { //loc has moved
			//stoploc();			

			INIT_count = 0;
			Serial.println(" langzaam werkt");
			//loc keren terug naar station niet nodig denderen van contact maakt richting toch onduidelijk hier
			PORTB ^= (1 << 0);
			PWM_cycle = PWM_max;
			INIT_fase = 3;
		}
		else { //loc has not moved

			COM_reg |= (1 << 7); //request INIT
			INIT_count++;
			//Serial.println(INIT_count);
		}
		//}
		break;
	case 3:
		Serial.println("nu case 3");
		stoploc();
		INIT_station();
		switch (laststop) {
		case 4:
			PORTB &= ~(1 << 0);
			break;
		case 5:
			PORTB |= (1 << 0);
			break;
		}
		INIT_fase = 4;
		RUN_start();
		break;
	case 4:
		Serial.println("nu case 4");
		stoploc();
		//bepaal station
		INIT_reg ^= (1 << 3); //count aantal gemeten routes, voorlopig twee
		INIT_station();
		switch (laststop) {
		case 4: //route 5>4 =1
			routetijd[1] = tijd;
			break;
		case 5: //route 4>5 =0
			routetijd[0] = tijd;
			break;
		}

		if (bitRead(INIT_reg, 3) == false) {
			INIT_fase = 5; //stoppen
		}
		else {
			INIT_fase = 3; //herhalen
		}
		COM_reg |= (1 << 7); //request init exe
		break;

	case 5:
		stoploc();
		Serial.println("klaar....");
		Serial.println(routetijd[0]);
		Serial.println(routetijd[1]);
		INIT_fase = 0; //start pendel
		RUN_start();
		break;
	}
}
void stoploc() {
	COM_reg &= ~(1 << 4); //reset manual switch to off
	PORTD &= ~(1 << 7); //PWM poort low			
	COM_reg &= ~(1 << 1); //disable motor pwm

	Serial.println("stoploc()");
}
void INIT_station() {
	//bepaal station bij opstarten, initialiseren
	byte stn;
	stn = PINC;
	stn = stn << 2;
	stn = stn >> 5;
	switch (stn) {
	case 3:
		laststop = 4;
		break;
	case 5:
		laststop = 5;
		break;
	default:
		//als meer stopplaatsen worden gemaakt dan hier meer opties
		laststop = 0;
		break;
	}
	//Serial.print("INIT_station   : ");
	//Serial.println(laststop);

}

void RUN_start() {
	Serial.print("RUN_start begin in station:  ");
	Serial.println(laststop);


	RUN_count[1] = 0;
	RUN_count[3] = 0;

	COM_reg |= (1 << 4);
	COM_reg |= (1 << 1);
	COM_reg |= (1 << 5); //set versnellen
	
	PWM_cycle = PWM_min;
	RUN_meting = millis();

	if (INIT_fase == 0) {
		COM_reg |= (1 << 6); //set vertragen
		switch (laststop) {
		case 4:
			stoptijd = routetijd[0] - 3000;
			break;
		case 5:
			stoptijd = routetijd[1] - 3000;
			break;
		default:
			//nu gaat het fout, opnieuw initialiseren????
			Serial.println("gaat fout geen goed begin station");
			break;
		}
	}
}
void RUN_acc() { //called every 20ms from loop
	RUN_count[0]++;
	if (RUN_count[0] > RUN_count[1]) { // random(0, 15);

		RUN_count[3]++;
		if (RUN_count[3] > 5) {
			RUN_count[1]++;
		}

		RUN_count[0] = 0;

		PWM_cycle--;
		if (PWM_cycle <= PWM_max) {
			COM_reg &= ~(1 << 5); //stop versnellen
		}
	}
}

void RUN_dec() {

	RUN_count[4]++;
	if (RUN_count[4] > 10) {
		RUN_count[4] = 0;
		if (millis() - Dectime > stoptijd) {
			COM_reg &= ~(1 << 5);  //stop versnellen
			PWM_cycle++;
			Serial.println(PWM_cycle);
			if (PWM_cycle > PWM_min) COM_reg &= ~(1 << 6);  //stop vertragen.
		}
	}
}


void RUN_stop(byte station) {

	//Serial.print("Runstop  station: ");
	//Serial.println(station);

	if (station != laststop & bitRead(COM_reg, 4) == true) { //denderen in stopplaats voorkomen, never stop on 0
		PORTB ^= (1 << 0); //toggle direction

		Serial.print("Runstop INIT_fase = ");
		Serial.println(INIT_fase);
		TIJDmeting();
		//stop loc
		PORTD &= ~(1 << 7); //PWM poort low			
		COM_reg &= ~(1 << 1); //disable motor pwm

		switch (INIT_fase) {
		case 0: //init finished, normal operation
			wachttijd = 2000;
			RUN_time = millis();
			COM_reg &= ~(1 << 6);  //stop vertragen.		
			break;
		default:
			INIT_exe();
			break;
		}
	}
	laststop = station;
}



void SW_read() {

	byte changed;
	byte swnew;
	swnew = PINC;
	changed = swnew ^ SW_old;
	if (changed > 0) {

		for (byte i = 0; i < 6; i++) {
			if (bitRead(changed, i) == true) {
				if (bitRead(swnew, i) == true) { //port is high
					switch (i) {
					case 4:
						RUN_stop(i);
						break;
					case 5:
						RUN_stop(i);
						break;
					}
				}
				else { //port is low
					switch (i) {
					case 0:
						if (bitRead(swnew, 1) == false) {
							SW_both();
						}
						else {
							PORTB ^= (1 << 0); //toggle direction
							laststop = 0;
						}
						break;
					case 1:
						COM_reg ^= (1 << 4); //manual stop

						if (bitRead(COM_reg, 4) == false) { //all stop
							COM_reg &= ~(1 << 1);
							PORTD &= ~(1 << 7);

						}
						else { //Start manual
							INIT_fase = 0;
							INIT_exe();
						}
						break;
					case 4: //track sensor 4 released, train left station 4
						break;
					case 5: //train left station 5
						break;
					}
				}
			}

		}
	}
	SW_old = swnew;
}


void TIJDmeting() { //0-9 routes 10=niet opslaan


	tijd = (millis() - RUN_meting);
	/*

		if (bitRead(INIT_reg, 1) == true) {
			RUN_dectijd = (tijd - 1);
			INIT_reg &= ~(1 << 1); //reset initialise
		}

	*/
	//Serial.print("Vertragen na: ");
	//Serial.println(RUN_dectijd);

	//Serial.print("Snelheid: ");
	//Serial.print(PWM_cycle);
	Serial.print("Tijd  ");
	Serial.print(tijd);
	Serial.println(" sec.");

}
void SW_both() {
	Serial.println("beide knoppen");
}
void loop() {

	if (bitRead(COM_reg, 4) == true) {
		//pwm 
		if (bitRead(COM_reg, 1) == true) {
			PWM_clk();
		}
		else {
			if (millis() - RUN_time > wachttijd) { //& bitRead(COM_reg,7)==true
				RUN_start();
			}
		}
	}

	//check input
	if (millis() - SW_time > 20) {
		SW_time = millis();
		SW_read();
		if (bitRead(COM_reg, 7) == true)INIT_exe();
		if (bitRead(COM_reg, 5) == true)RUN_acc(); //versnellen
		if (bitRead(COM_reg, 6) == true) RUN_dec(); //vertragen
	}
}
