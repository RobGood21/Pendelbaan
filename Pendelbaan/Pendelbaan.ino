/*
 Name:		Pendelbaan.ino V1-01
 Created:	2/10/2019 11:11:06 AM
 Author:	Rob Antonisse
 Web: www.wisselmotor.nl

 Sketch for Arduino UNO.
 Shuttle train module for direct current model trains
 Seperate power supply possible for train or usage of 12V supplied to DC connection Arduino.
 Two position sensors needed.
 Power 5V if needed for sensors suppied by module.
 Works with high active and low active sensors.
 More info www.wisselmotor.nl
 free for hobby, for commercial use please contact wisselmotor. nl.
 Special Arduino shield "PenDel dc" available.
 */

#include <EEPROM.h> //use build in eeprom program
byte COM_reg;
byte INIT_reg;
byte INIT_fase;
int INIT_count;
byte SW_old;
unsigned long SW_time;
unsigned long PWM_time; //timer for PWM puls
byte PWM_cycle; //tijd tussen twee pulsen in ms
unsigned long PWM_pulstime; //timer voor pwm puls breedte
byte PWM_puls; //ingestelde breedte van puls in ms eeprom #30
byte PWM_px; //automatisch verhoging PWM_puls
byte PWM_min;
byte PWM_max;
byte PRG_fase = 0;
byte RUN_count[5]; //5 counters for run mode
unsigned int MINtime; //counts in 20ms time loc moves in minimum speed
byte LED_count[4]; //4 available counters for blinking leds
byte LED_mode;
byte route;
byte STATION_adres[2];
boolean STATION_dir[2];
byte STATION_tijd[2]; //tijd dat trein stilstaat in station in seconden max 255 0=melder 1 adres 3 1=melder 2adres 5
byte MEM_reg; //bit 0=station 3 bit 1=station 5
byte MEM_puls; //30
byte MEM_max; //31
byte MEM_min; //32
unsigned long tijd; //tijd meting duration route
int stoptijd;//tijd wanneer na begin route afremmen begint
unsigned long routetijd[4]; //routes have there own stoptime, locs have different speeds 0=3>5 rechtsom  1=5>3 rechtsom 2=34>5
unsigned long Dectime;
byte station;
byte newstation; //het te verwachten station voor een stop
byte oldstation;  //station excluded for stop
unsigned int wachttijd; //stoptijd in seconden
unsigned long RUN_wachttijd;
unsigned long RUN_meting;

void setup() {
	Serial.begin(9600);
	DDRC = 0; //set port C as input (pins A0~A5)
	PORTC = 0xFF; //set pull up resisters to port C, also with hardware
	SW_old = PINC;
	DDRB |= (1 << 0); //direction H-bridge
	DDRD |= (1 << 7); //output PIN7 enable H-bridge
	DDRD |= (1 << 6); //Output PIN 6 Green control Led
	DDRD |= (1 << 5); //output pin 5 red control led
	DDRD &= ~(1 << 4); //set PIN4 as input function SHORT (kortsluiting)

	if (bitRead(PINC, 1) == false) {
		//Serial.println("factory");
		EEPROM_clear();  //optional clear the eeprom memory, both buttons pressed no loc in station
	}
	stoptijd = 0xFFFFFFFF; //max tijd	
	MEM_reg = EEPROM.read(10);
	STATION_tijd[0] = EEPROM.read(20);
	STATION_tijd[1] = EEPROM.read(21);
	//Serial.println(MEM_reg);

	if (EEPROM.read(30) == 0xFF)EEPROM.write(30, 3); //puls breedte in ms
	MEM_puls = EEPROM.read(30);
	if (EEPROM.read(31) == 0xFF)EEPROM.write(31, 3);
	MEM_min = EEPROM.read(31);
	if (EEPROM.read(32) == 0xFF)EEPROM.write(32, 3);
	MEM_max = EEPROM.read(32);
	PWM_px = 0;
	INIT_engine();
}

void EEPROM_clear() {
	//resets the eeprom memory to 0xFF
	for (int i = 0; i < EEPROM.length(); i++) {
		EEPROM.write(i, 0xFF);
	}
	//Serial.println("EEprom memory set to 0xFF");
}
void PWM_clk() {
	if (millis() - PWM_time > PWM_cycle) {
		PWM_time = millis();
		PORTD |= (1 << 7);
		COM_reg |= (1 << 0);
		PWM_pulstime = millis();
	}
	if (bitRead(COM_reg, 0) == true) {
		if (millis() - PWM_pulstime > PWM_puls) {
			//als short PIN4 nu laag is dan is er sprake van kortsluiting	
			if (bitRead(PIND, 4) == true) SHORT();
			PORTD &= ~(1 << 7);
			COM_reg &= ~(1 << 0);
		}
	}
}
void SHORT() {
	//Serial.println("Stop kortsluiting");
	stoploc;
	COM_reg &= ~(1 << 4);
	LED_mode = 5;
}
void INIT_engine() {
	//sets parameters for engine
	//PWM_puls = breedte van de puls
	//PWM_max=maximale snelheid minimale periode tussen twee pulsen (pulsbreedte)
	//PWM_min = minimale snelheid maximale periode tussen twee pulsen
	byte pls;
	byte cnt;
	switch (MEM_puls) {
	case 1:
		PWM_puls = 6;
		break;
	case 2:
		PWM_puls = 8;
		break;
	case 3:
		PWM_puls = 10;
		break;
	case 4:
		PWM_puls = 12;
		break;
	case 5:
		PWM_puls = 14;
		break;
	}
	PWM_puls = PWM_puls + PWM_px;
	pls = PWM_puls / 2; //* instelbaar integer 1,2,3,4,5 instelling maximale snelheid
	cnt = MEM_min - 1;
	PWM_min = (PWM_puls * 5) - (pls * cnt);
	cnt = 5 - MEM_max; //0-4
	pls = PWM_puls / 4;
	PWM_max = PWM_puls + (pls*cnt);

	/*
		Serial.print("PWM_puls= ");
		Serial.println(PWM_puls);
		Serial.print("PWM_min= ");
		Serial.println(PWM_min);
		Serial.print("PWM_max= ");
		Serial.println(PWM_max);
	*/
}

void INIT_exe() {
	COM_reg &= ~(1 << 7); //reset request flag
	switch (INIT_fase) {
	case 0:
		PWM_px = 0;
		INIT_engine();
		oldstation = 20;
		LED_mode = 2;
		station = INIT_station();
		COM_reg |= (1 << 4);

		if (bitRead(MEM_reg, 4) == true) {
			COM_reg |= (1 << 3);
		}
		else {
			COM_reg &= ~(1 << 3);
		}

		//RUN_dir(false);   ??????????????????????????????????????????????????????????????????????????????

		RUN_dl();//koppel richting aan richting flag
		if (station == 7) {	//niet in station dus station opzoeken.			
			startloc();
			newstation = 10;
			INIT_fase = 1;
		}
		else { //loc is in station
			INIT_fase = 20;
			COM_reg |= (1 << 7);
			startloc();
		}
		break;
	case 1:
		STATION_adres[0] = station; //set station 0
		RUN_dir(true);
		INIT_fase = 2;
		COM_reg |= (1 << 7); //request ININT_exe
		break;
	case 2:
		// Serial.println("begin minimumsnelheids meting");
		PWM_cycle = PWM_min;
		COM_reg |= (1 << 1); //start PWM send, motor on
		INIT_fase = 3;
		COM_reg |= (1 << 7); //request init ??????
		break;
	case 3:
		station = INIT_station();
		if (station == 7) { //loc has moved			
			//stoploc();	
			INIT_count = 0;
			//Serial.println(" langzaam werkt");
			//loc keren terug naar station niet nodig denderen van contact maakt richting toch onduidelijk hier			

			RUN_dir(true); //direction
			//PWM_puls++; //pulsbreedte 1 ms langer	
			INIT_engine();
			PWM_cycle = PWM_max;
			INIT_fase = 4;
			oldstation = 0;
			newstation = STATION_adres[0];
		}
		else { //loc has not moved

			COM_reg |= (1 << 7); //request INIT
			INIT_count++;

			if (INIT_count > 50) { //period 1 second
				//stoploc();				
				//Serial.println("FOUT loc beweegt niet....");
				//verleng aanstuurpuls
				PWM_px++;
				INIT_engine();
				INIT_count = 0;
				if (PWM_puls > 18) {
					stoploc();
					//Serial.println("Alles stop");
					COM_reg &= ~(1 << 7);
					//hier leds alarm geven 
					LED_mode = 3;
				}
			}
		}
		//}
		break;
	case 4:
		//Serial.println("hier kijken of er gekeerd moet worden....");
		RUN_dir(false);
		if (STATION_adres[0] == 3) {
			newstation = 5;
		}
		else {
			newstation = 3;
		}
		INIT_fase = 5;
		RUN_start();
		break;
	case 5:
		RUN_dir(false);
		newstation = STATION_adres[0];
		RUN_route();
		RUN_start();
		INIT_fase = 10;
		break;
	case 10:
		RUN_route();
		for (byte i = 0; i < 4; i++) {
			/*
			Serial.print("Route ");
			Serial.print(i);
			Serial.print(", Tijd= ");
			Serial.println(routetijd[i]);
			*/
		}
		INIT_fase = 0; //start pendel
		RUN_dir(false);
		RUN_start();
		LED_mode = 1;
		break;

	case 20:
		//loc is in station, finding correct direction for start
		RUN_count[0]++;
		COM_reg |= (1 << 7);

		if (RUN_count[0] > 12) {
			RUN_count[0] = 0;
			station = INIT_station();
			//stoploc();
			//Serial.println(station);
			COM_reg &= ~(1 << 7);

			if (station == 7) {
				//Serial.println("loc nu vrije baan");
				RUN_dir(true);
				RUN_dl();
				//startloc();
				//COM_reg |= (1 << 4);
				newstation = 10;
				INIT_fase = 1;
			}
			else { //station NIET vrij
				//stoploc();
				if (station == 3) {
					newstation = 5;
				}
				else {
					newstation = 3;
				}
				RUN_dir(true);
				RUN_dl();
				//Serial.println("station was niet vrij!!!");
				INIT_fase = 1;
			}
		}
		break;
	}
}
void RUN_dir(boolean always) {
	//bepaald richting voor het nieuw af te leggen traject, alleen keren als turn is true
	if (always == false) {
		switch (station) {
		case 3:
			if (bitRead(MEM_reg, 0) == true)COM_reg ^= (1 << 3);
			break;
		case 5:
			if (bitRead(MEM_reg, 1) == true)COM_reg ^= (1 << 3);
			break;
		}
	}
	else {
		COM_reg ^= (1 << 3);
	}
	RUN_dl();
}
void RUN_dl() { //dl=direction locomotive
	//Bepaald richting aan de hand van bit3 van COM_reg
	PORTB &= ~(1 << 0);
	if (bitRead(COM_reg, 3) == true) PORTB |= (1 << 0);
}
void RUN_route() {
	//bepalen hoe de sensors keren
	byte sd;
	sd = MEM_reg << 6; //only bit 0 and 1
	sd = sd >> 6;

	tijd = tijd - ((tijd / 100) * 35); //tijd minus %

	switch (sd) {
	case 0: //beide niet keren
		if (station == 5) { //driven from 3 to 5
			routetijd[0] = tijd;
			routetijd[2] = tijd; //direction true		
		}
		else { //driven 5 to 3
			routetijd[1] = tijd;
			routetijd[3] = tijd;
		}
		break;
	case 3: //beide keren

		if (station == 5) { //driven from 3 to 5
			routetijd[0] = tijd;
			routetijd[1] = tijd; //direction true		
		}
		else { //driven 5 to 3
			routetijd[2] = tijd;
			routetijd[3] = tijd;
		}

		break;
	default: // 1 station niet keren

		//Serial.print("richting= ");
		//Serial.println(bitRead(COM_reg, 3));
		if (station == 5) { //driven from 3 to 5

			if (bitRead(COM_reg, 3) == false) {
				routetijd[0] = tijd;
				routetijd[3] = tijd; //direction true	
			}
			else { //vooruit
				routetijd[2] = tijd;
				routetijd[1] = tijd;
			}
		}
		else { //driven 5 to 3

			if (bitRead(COM_reg, 3) == false) {
				routetijd[1] = tijd;
				routetijd[2] = tijd; //direction true	
			}
			else {
				routetijd[0] = tijd;
				routetijd[3] = tijd; //direction true	
			}
		}
		break;
	}
}

void stoploc() {
	//COM_reg &= ~(1 << 0); //duty cycle flag
	PORTD &= ~(1 << 7); //PWM poort low			
	COM_reg &= ~(1 << 1); //disable motor pwm
	COM_reg &= ~(1 << 4); //reset manual switch to off
}
void startloc() {
	//used in INIT_exe to start loc during setup
	PWM_cycle = PWM_max;
	COM_reg |= (1 << 1); //start PWM send, motor on
	COM_reg &= ~(1 << 6); //disable slow down	
}
byte INIT_station() {
	//calc current station
	byte stn;
	stn = PINC;
	stn = stn << 2;
	stn = stn >> 5;
	return stn;
}

void RUN_start() {
	byte temp;
	RUN_count[1] = 0;
	RUN_count[3] = 0;
	COM_reg |= (1 << 4);
	COM_reg |= (1 << 1);
	COM_reg |= (1 << 5); //set versnellen
	PWM_cycle = PWM_min;

	if (INIT_fase == 0) {
		COM_reg |= (1 << 6); //set vertragen
		Dectime = millis(); //reset counter voor begin vertragen
		LED_control(station);
		LED_mode = 0;
		switch (station) {
		case 3:
			newstation = 5;
			if (bitRead(COM_reg, 3) == false) {
				stoptijd = routetijd[0];
				temp = 0;
			}
			else {
				stoptijd = routetijd[2];
				temp = 2;
			}
			break;
		case 5:
			newstation = 3;
			if (bitRead(COM_reg, 3) == false) {
				stoptijd = routetijd[1];
				temp = 1;
			}
			else {
				stoptijd = routetijd[3];
				temp = 3;
			}
			break;
		default:
			//nu gaat het fout, opnieuw initialiseren????
			//Serial.println("gaat fout geen goed begin station");
			break;
		}
		/*

		Serial.print("route= ");
		Serial.print(temp);
		Serial.print(",  Stoptijd: ");
		Serial.println(stoptijd);
*/
	}
	else {
		RUN_meting = millis();
		//Serial.println("start meting");
	}
}
void LED_blink() {  //called every 20ms from SW_read
	byte temp;
	switch (LED_mode) {
	case  0:
		break;
	case 1: //normal mode
		break;

	case 2: //init
		LED_count[0]++;
		if (LED_count[0] > 50)LED_count[0] = 0; //period 1 second
		switch (LED_count[0]) {
		case 10:
			LED_control(10);
			break;
		case 12:
			LED_control(0);
			break;
		}
		break;
	case 3: //loc staat stil
		LED_count[0]++;
		PORTD &= ~(1 << 6); //kill green led
		if (LED_count[0] > 5) {
			LED_count[0] = 0;
			PIND |= (1 << 5); //flash red led
		}
		break;
	case 4:
		break;
	case 5: //SHortcut kortsluiting, beide leds afwisselend snel
		LED_count[0]++;
		if (LED_count[0] > 4) {
			LED_count[0] = 0;
			PIND |= (1 << 5);
		}
		LED_count[1]++;
		if (LED_count[1] > random(2, 8)) {
			LED_count[1] = 0;
			LED_count[0] = 10;
			PIND |= (1 << 6);
		}
		break;

	case 6: //PRG_fase 2 keren melder 0 (3)	single flash		
		LED_count[0]++;
		if (LED_count[0] > 1) PORTD &= ~(1 << 5);
		if (LED_count[0] > 50) {
			LED_count[0] = 0;
			PORTD |= (1 << 5);
		}
		break;
	case 7: //PRG_fase 3 keren melder 1 (5) double flash
		LED_count[0]++;
		if (LED_count[0] == 12)PORTD |= (1 << 5);
		if (LED_count[0] == 2 | LED_count[0] == 13) PORTD &= ~(1 << 5);

		if (LED_count[0] > 50) {
			LED_count[0] = 0;
			PORTD |= (1 << 5);
		}
		break;
	case 8:
		//flashed red led during station wachttijd
		LED_count[0]++;
		LED_count[1]++;
		if (LED_count[0] > 50) { //interval 1 sec
			LED_count[0] = 0;
			LED_count[1] = 0;
			PORTD |= (1 << 6);
		}
		if (LED_count[1] == 3)PORTD &= ~(1 << 6);
		break;
	case 9:
		//flashed green led during station wachttijd
		//flashed red led during station wachttijd
		LED_count[0]++;
		LED_count[1]++;
		if (LED_count[0] > 50) { //interval 1 sec
			LED_count[0] = 0;
			LED_count[1] = 0;
			PORTD |= (1 << 5);
		}
		if (LED_count[1] == 3)PORTD &= ~(1 << 5);
		break;
	case 10: //program mode set stoptime begin
		//slow blinking red led, green led depends on settings
		LED_count[0] ++;
		if (LED_count[0] > 20) {
			PIND |= (1 << 5);
			LED_count[0] = 0;
		}
		if (bitRead(MEM_reg, 2) == false | bitRead(MEM_reg, 3) == false) {
			temp = 16;
			if (bitRead(INIT_reg, 2) == true) temp = 4;
			//Green led blinking 
			LED_count[1] ++;
			if (LED_count[1] > temp) {
				PIND |= (1 << 6);
				LED_count[1] = 0;
			}
		}
		break;
	case 11: //setting puls width PWM_puls
		LED_count[0]++;
		switch (LED_count[0]) {
		case 10:
			LED_control(11);
			break;
		case 40:
			LED_control(12);
			break;
		case 55:
			LED_control(11);
			break;
		case 60:
			LED_control(12);

			break;
		case 70:
			LED_count[0] = 0;
			break;
		}
		LED_blinkx(MEM_puls);
		break;
	case 12:
		LED_count[0]++;
		switch (LED_count[0]) {
		case 10:
			LED_control(11);
			break;
		case 40:
			LED_control(12);
			break;
		case 55:
			LED_control(11);
			break;
		case 60:
			LED_control(12);

			break;
		case 70:
			LED_control(11);
			break;
		case 75:
			LED_control(12);

			break;
		case 85:
			LED_count[0] = 0;
			break;
		}
		LED_blinkx(MEM_min);
		break;

	case 13:
		LED_count[0]++;
		switch (LED_count[0]) {
		case 10:
			LED_control(11);
			break;
		case 40:
			LED_control(12);
			break;
		case 55:
			LED_control(11);
			break;
		case 60:
			LED_control(12);
			break;
		case 70:
			LED_control(11);
			break;
		case 75:
			LED_control(12);
			break;
		case 85:
			LED_control(11);
			break;
		case 90:
			LED_control(12);
			break;
		case 100:
			LED_count[0] = 0;
			break;
		}
		LED_blinkx(MEM_max);
		break;
	case 14: //confirm direction change
		LED_count[0]++;
		if (LED_count[0] == 2) {


			if (bitRead(MEM_reg, 4) == false) {
				LED_control(11);
			}
			else {
				LED_control(13);
			}
		}
		if (LED_count[0] > 20) {
			LED_mode = 0;
			LED_count[0] = 0;
			LED_control(0);
		}
		break;
	case 15: //start direction just flashing
		LED_count[0]++;
		if (LED_count[0] > 10) { //periode
			PIND |= (1 << 5);
			LED_count[0] = 0;
		}
		break;
	case 16: //confirm programming eeprom
		LED_count[0]++;
		if (LED_count[0] == 15)LED_control(10);
		if (LED_count[0] > 40) {
			LED_count[0] == 0;
			LED_mode = 0;
			LED_control(0);
		}
		break;

	}
}
void LED_blinkx(byte cnt) {
	//sub function for LED_blink
	//green led
	LED_count[1]++;

	if (LED_count[1] == 1) {
		LED_control(14); //green off
		INIT_reg &= ~(1 << 3); //flag for led status
		LED_count[2] = LED_count[1] + 5;
	}


	if (LED_count[1] == LED_count[2]) {
		PIND |= (1 << 6);
		INIT_reg ^= (1 << 3);

		if (bitRead(INIT_reg, 3) == false) { //led off new flash needed?
			LED_count[2] = LED_count[1] + 20;
			LED_count[3]++;
			if (LED_count[3] >= cnt) {
				//Serial.println(LED_count[3]);
				LED_count[3] = 0;
				LED_count[2] = 200;
			}
		}
		else {
			LED_count[2] = LED_count[1] + 1;
		}
	}
	if (LED_count[1] > 120) { //counter 1 second
		LED_count[1] = 0;
		LED_count[2] = 0;
		LED_count[3] = 0;
	}
}

void LED_control(byte lprg) {
	switch (lprg) {
	case 0:
		PORTD &= ~(1 << 6);
		PORTD &= ~(1 << 5);
		break;
	case 3:
		PORTD |= (1 << 6);
		PORTD &= ~(1 << 5);
		break;
	case 5:
		PORTD |= (1 << 5);
		PORTD &= ~(1 << 6);
		break;

	case 10:
		PORTD |= (1 << 6);
		PORTD |= (1 << 5);
		break;

		//singles
	case 11: //red on
		PORTD |= (1 << 5);
		break;
	case 12:
		PORTD &= ~(1 << 5);
		break;
	case 13:
		PORTD |= (1 << 6);
		break;
	case 14:
		PORTD &= ~(1 << 6);
		break;

	}
}

void RUN_acc() { //called every 20ms from loop
	RUN_count[0]++;
	if (RUN_count[0] > (25 - PWM_puls)) { //12
		RUN_count[0] = 0;
		PWM_cycle--;
		if (PWM_cycle < PWM_max) {
			COM_reg &= ~(1 << 5); //stop versnellen
		}
	}
}

void RUN_dec() {
	RUN_count[4]++;
	if (RUN_count[4] > (30 - PWM_puls)) { //25
		RUN_count[4] = 0;
		if (millis() - Dectime > stoptijd) {
			COM_reg &= ~(1 << 5);  //stop versnellen
			PWM_cycle++;
			//check snelheid
			if (PWM_cycle > PWM_min) COM_reg &= ~(1 << 6);  //stop vertragen, start timer
			INIT_reg |= (1 << 0); //starts timer for loc moving to slow
			MINtime = 0; //reset counter
		}
	}
}


void RUN_stop() {
	//Serial.print("Runstop station= ");
	//Serial.println(station);
	//
	if (newstation == 10)newstation = station; //stoppen in alle stations, op zoek naar begin station
	if (station != oldstation & station == newstation & bitRead(COM_reg, 4) == true) { //denderen in stopplaats voorkomen, never stop on 0
		oldstation = station;
		//	Serial.print("Runstop station: ");
		//	Serial.println(station);
		TIJDmeting();
		INIT_reg &= ~(1 << 0); //stop timer loc moving to slow
		COM_reg &= ~(1 << 0);
		PORTD &= ~(1 << 7); //PWM poort low			
		COM_reg &= ~(1 << 1); //disable motor pwm
		switch (INIT_fase) {
		case 0: //init finished, normal operation
			switch (station) {
			case 3: //van 5 > 3				
				if (bitRead(COM_reg, 3) == false) {
					route = 1;
				}
				else {
					route = 3;
				}
				break;
			case 5: //van 3>>5				
				if (bitRead(COM_reg, 3) == false) {
					route = 0;
				}
				else {
					route = 2;
				}
				break;
			}

			//Serial.print("PWM cycle= ");
			//Serial.println(PWM_cycle);


			if (bitRead(INIT_reg, 1) == true) {
				//loc moved minimum speed for more then 1 second
				INIT_reg &= ~(1 << 1); //reset this flag, is set in SW_read
				routetijd[route] = routetijd[route] + 500; //remtijd aanpassen
			}
			else {

				if (PWM_cycle >= PWM_min) { //true is te snel afgeremd, tijd tot remmen verhogen					
					routetijd[route] = routetijd[route] + 200;
				}
				else {
					if (PWM_min - PWM_cycle > 3 * PWM_puls)routetijd[route] = routetijd[route] - 800;
					if (PWM_min - PWM_cycle > 2 * PWM_puls)routetijd[route] = routetijd[route] - 600;
					if (PWM_min - PWM_cycle > PWM_puls)routetijd[route] = routetijd[route] - 300;
					if (PWM_min - PWM_cycle > 2)routetijd[route] = routetijd[route] - 100;
					routetijd[route] = routetijd[route] - 100;
				}
			}
			/*
			
			Serial.print("aangepaste route= ");
			Serial.println(route);
			Serial.println("");
*/

			RUN_dir(false); //richting instellen
			LED_control(0);
			//wachttijd bij station instellen

			switch (station) {
			case 3:
				LED_mode = 8;
				if (bitRead(MEM_reg, 2) == true) {
					wachttijd = random(1000, 60000);
				}
				else {
					wachttijd = STATION_tijd[0] * 1000;
				}
				break;
			case 5:
				LED_mode = 9;
				if (bitRead(MEM_reg, 3) == true) {
					wachttijd = random(1000, 45000);
				}
				else {
					wachttijd = STATION_tijd[1] * 1000;
				}
				break;

			}


			//Serial.print("Wachttijd= ");
			//Serial.println(wachttijd);

			RUN_wachttijd = millis();
			COM_reg &= ~(1 << 6);  //stop vertragen.
			break;

		case 1:
			if (bitRead(MEM_reg, 1) == bitRead(MEM_reg, 0)) {
				INIT_exe();
			}
			else {
				//Serial.println("ongelijke melders");
				switch (station) {
				case 3:
					if (bitRead(MEM_reg, 0) == true) {
						//doorgaan met INIT
						INIT_exe();
					}
					else {
						//Serial.println("stil blijven staan");
						startloc();
						newstation = 10;
					}
					break;
				case 5:
					if (bitRead(MEM_reg, 1) == true) {
						INIT_exe();
					}
					else {
						//Serial.println("hier straks doorrijden");
						startloc();
						newstation = 10;
					}
					break;
				}
			}
			break;
		default:
			INIT_exe();
			break;
		}
	}
}

void SW_read() { // called fromm loop every 20ms

	if (bitRead(INIT_reg, 0) == true) { //timer too slow moving loc active, loc moves minimum speed if true
		MINtime++;
		if (MINtime > 70) { //loc runs min speed for 1,5 second
			INIT_reg |= (1 << 1);
			INIT_reg &= ~(1 << 0);
			PWM_cycle = PWM_min - (1.8*PWM_puls);
		}
	}
	LED_blink();
	byte changed;
	byte swnew;
	byte instel;
	swnew = PINC;
	changed = swnew ^ SW_old;
	if (changed > 0) {
		//check if both buttons pressed
		instel = swnew << 6;
		if (instel == 0) {
			SW_both();
		}
		else
		{
			for (byte i = 0; i < 6; i++) {
				if (bitRead(changed, i) == true) {
					if (bitRead(swnew, i) == true) { //port C is high
						switch (PRG_fase) {
						case 1: //test melder release
							switch (i) {
							case 4:
								PORTD &= ~(1 << 6);
								break;
							case 5:
								PORTD &= ~(1 << 5);
								break;
							}
							break;
						case 4: //set stoptijd
							TIJDmeting();
							tijd = tijd / 1000; //millisec to sec
							//Serial.print("stop tijd in station =");
							//Serial.println(tijd);
							INIT_reg &= ~(1 << 2); //stops fast flashing green led

							switch (i) {
							case 4:
								STATION_tijd[1] = tijd;
								break;
							case 5:
								STATION_tijd[0] = tijd;
								break;
							}
							break;
						}
					}
					else { //port is low
						switch (i) {
						case 0: //knop R
							switch (PRG_fase) {
							case 0:
								MEM_reg ^= (1 << 4); //change start direction
								LED_mode = 14;
								LED_count[0] = 0;
								break;
							case 1:
								break;
							case 2: //keren melder 1 aanpassen
								MEM_reg ^= (1 << 0);
								PIND |= (1 << 6);
								break;
							case 3: //keren melder 2 aanpassen
								MEM_reg ^= (1 << 1);
								PIND |= (1 << 6);
								break;
							case 4: //reset stationtijd naar random
								MEM_reg |= (1 << 2);
								MEM_reg |= (1 << 3);
								PORTD |= (1 << 6); //green led on
								break;
							case 5: //MEM_puls
								MEM_puls++;
								if (MEM_puls > 5)MEM_puls = 1;
								break;
							case 6: //MEM_min
								MEM_min++;
								if (MEM_min > 5) MEM_min = 1;
								break;
							case 7: //MEM_max
								MEM_max++;
								if (MEM_max > 5)MEM_max = 1;
								break;
							case 8: //start direction
								MEM_reg ^= (1 << 4);

								if (bitRead(MEM_reg, 4) == true) {
									LED_control(13);
								}
								else {
									LED_control(14);
								}
								break;
							}
							break;
						case 1: //Knop S
							switch (PRG_fase) {
							case 0:
								INIT_reg ^= (1 << 5);
								//COM_reg ^= (1 << 4); //manual stop
								if (bitRead(INIT_reg, 5) == false) { //all stop
									//Serial.println("stop nu geheel...?");
									stoploc();
									LED_control(0); //kill leds
									LED_mode = 0;
								}
								else { //Start manual	
									INIT_fase = 0;
									INIT_exe();
								}
								break;

							case 1: //increment PRG_fase	
								PRG_fase = 2;
								LED_mode = 6;
								if (bitRead(MEM_reg, 0) == true) {
									PORTD |= (1 << 6);
								}
								else {
									PORTD &= ~(1 << 6);
								}
								break;
							case 2:
								PRG_fase = 3;
								LED_mode = 7;

								if (bitRead(MEM_reg, 1) == true) {
									PORTD |= (1 << 6);
								}
								else {
									PORTD &= ~(1 << 6);
								}

								break;
							case 3: //instellen wachttijden stations								
								LED_mode = 10;
								PORTD |= (1 << 5); // led green on

								//prg_fase 3 wordt hier prg_fase 4.... dus alle acties van 3 zitten onder 4.... 

								PRG_fase = 4;
								break;

							case 4: //engine mode 1, instellen PWM_puls
								PRG_fase = 5;
								LED_mode = 11;
								break;
							case 5: //engine mode 2, instellen PWM_min
								PRG_fase = 6;
								LED_mode = 12;
								break;
							case 6: //engine mode 3, instellen PWM_max
								PRG_fase = 7;
								LED_mode = 13;
								break;
							case 7: //direction in start
								PRG_fase = 8;
								LED_mode = 15;

								if (bitRead(MEM_reg, 4) == true) {
									LED_control(13);
								}
								else {
									LED_control(14);
								}
								break;

							case 8: //verlaat program mode
								PRG_fase = 0;
								LED_mode = 0;
								LED_control(0);
								MEM_changed();
								break;
							}
							break;

						case 4: //sensor PINA4 station 5
							station = INIT_station();

							if (station == 5) { //om spookmeldingen te verminderen
								switch (PRG_fase) {
								case 0:
									RUN_stop();
									break;
								case 1:
									PORTD |= (1 << 6);
									break;
								case 4:
									//start tijdmeting station 5
									//Serial.println("start tijdmeting 5");
									RUN_meting = millis();
									MEM_reg &= ~(1 << 3);
									INIT_reg |= (1 << 2); //fast flashing green led
									break;
								}
							}
							break;
						case 5: //Sensor PINA5 station 3
							station = INIT_station();
							if (station == 3) {

								switch (PRG_fase) {
								case 0:
									RUN_stop();
									break;
								case 1:
									PORTD |= (1 << 5);
									break;
								case 4:
									//start tijdmeting station stoptijd
									//Serial.println("start tijdmeting 3");
									RUN_meting = millis();
									MEM_reg &= ~(1 << 2);
									INIT_reg |= (1 << 2);  //fast flashing green led
									break;
								}
							}
							break;
						}
					}
				}

			}
		}
	}
	SW_old = swnew;
}
void TIJDmeting() {
	tijd = millis() - RUN_meting;
}
void SW_both() {
	//Serial.println("beide knoppen, begin instellingen");
	stoploc();
	INIT_fase = 0;
	LED_mode = 0;
	INIT_reg &= ~(1 << 4); //stops flashing during INIT fase
	if (PRG_fase == 0) {
		LED_control(10);
		PRG_fase = 1;
	}
	else {
		LED_control(0);
		PRG_fase = 0;
		//aanpassen eventueel veranderede geheugens
		MEM_changed();
	}
}
void MEM_changed() {
	if (EEPROM.read(10) != MEM_reg) EEPROM.write(10, MEM_reg);
	if (EEPROM.read(20) != STATION_tijd[0]) EEPROM.write(20, STATION_tijd[0]);
	if (EEPROM.read(21) != STATION_tijd[1]) EEPROM.write(21, STATION_tijd[1]);
	if (EEPROM.read(30) != MEM_puls) EEPROM.write(30, MEM_puls);
	if (EEPROM.read(31) != MEM_min) EEPROM.write(31, MEM_min);
	if (EEPROM.read(32) != MEM_max) EEPROM.write(32, MEM_max);
	LED_mode = 16;
}

void loop() {
	if (bitRead(COM_reg, 4) == true) {
		//pwm 
		if (bitRead(COM_reg, 1) == true) {
			PWM_clk();
		}
		else {
			if (millis() - RUN_wachttijd > wachttijd) { //& bitRead(COM_reg,7)==true
				if (INIT_fase == 0) RUN_start();
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
