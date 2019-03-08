/*
 Name:		Pendelbaan.ino
 Created:	2/10/2019 11:11:06 AM
 Author:	Rob Antonisse
 Web: www.wisselmotor.nl

 Programma voor het maken van een PenDel baan voor de modelspoor.
 Trein rijdt heen en weer...
 Bijzondere ervan is dat de gebruiker niets hoeft in stellen.
 Aansluiten volgens handleiding, locomotief op de rails, de rest gaat automatisch.

*/
#include <EEPROM.h> //use build in eeprom program
byte COM_reg;
byte INIT_reg;
byte INIT_fase;
int INIT_count;
unsigned long tijd; //tijd meting duration route

byte SW_old;
unsigned long SW_time;
unsigned long PWM_time; //timer for PWM puls
byte PWM_cycle; //tijd tussen twee pulsen in ms


unsigned long PWM_pulstime; //timer voor pwm puls breedte
byte PWM_puls; //ingestelde breedte van puls in ms

byte PWM_min = 38;
byte PWM_max;

byte PRG_fase = 0;
byte RUN_count[6]; //6 conters for run mode
byte LED_count[10]; //10 available counters for blinking ledss
byte LED_mode;
byte route;
byte STATION_adres[2];
boolean STATION_dir[2];
byte MEM_reg; //bit 0=station 3 bit 1=station 5


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

	boolean temp;
	Serial.begin(9600);

	Serial.println(PINC);



	DDRC = 0; //set port C as input (pins A0~A5)
	PORTC = 0xFF; //set pull up resisters to port C, also with hardware

	SW_old = PINC;
	//set pin7,8 as output
	DDRB |= (1 << 0); //direction H-bridge
	DDRD |= (1 << 7); //output PIN7 enable H-bridge
	DDRD |= (1 << 6); //Output PIN 6 Green control Led
	DDRD |= (1 << 5); //output pin 5 red control led

	DDRD &= ~(1 << 4); //set PIN4 as input function SHORT (kortsluiting)

	if (bitRead(PINC, 1) == false) {
		Serial.println("factory");
		EEPROM_clear();  //optional clear the eeprom memory, both buttons pressed no loc in station
	}


	stoptijd = 0xFFFFFFFF; //max tijd	

	MEM_reg = EEPROM.read(10);
	Serial.println(MEM_reg);
}

void EEPROM_clear() {
	//resets the eeprom memory to 0xFF
	for (int i = 0; i < EEPROM.length(); i++) {
		EEPROM.write(i, 0xFF);
	}
	Serial.println("EEprom memory set to 0xFF");


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
			if (bitRead(PIND, 4) == true) SHORT(); // & bitRead(INIT_reg, 6) == true

			PORTD &= ~(1 << 7);
			COM_reg &= ~(1 << 0);
		}
	}
}
void SHORT() {
	Serial.println("Stop kortsluiting");
	stoploc;
	COM_reg &= ~(1 << 4);
	LED_mode = 5;
	//INIT_fase = 0;
	//COM_reg &= ~(1 << 7);
}
void INIT_exe() {
	//Serial.print("INIT_fase= ");
	//Serial.println(INIT_fase);
	COM_reg &= ~(1 << 7); //reset request flag
	switch (INIT_fase) {
	case 0:
		INIT_reg &= ~(1 << 3); // reset teller voor route tijd metingen 
		PWM_puls = 10;
		PWM_max = PWM_puls + 2;
		oldstation = 20;
		LED_mode = 2;
		// Serial.println("begin initialisatie");
		station = INIT_station();
		if (station == 7) {
			//niet in station dus station opzoeken.
			RUN_dl();//koppel richting aan richting flag
			startloc();
			newstation = 10;
			INIT_fase = 1;
		}
		else {
			station = INIT_station();
			STATION_adres[0] = station;

			if (EEPROM.read(station) == true) {
				COM_reg |= (1 << 3);
			}
			else {
				COM_reg &= ~(1 << 3);
			}
			RUN_dl();

			INIT_fase = 2;
			COM_reg |= (1 << 0);
			newstation = station;
			COM_reg &= ~(1 << 6); //disable slow down	
			COM_reg |= (1 << 4);
			COM_reg |= (1 << 7);
		}

		break;


	case 1: //bepaal station 0
		//bepaal station en richting om dit station te bereiken
		STATION_adres[0] = station; //set station 0
		STATION_dir[0] = bitRead(COM_reg, 3); // set richting om station 0 te bereiken
		//als waardes afwijken van gelezen waardes in EEPROM is er hardware matig iets veranderd, melders verplaatst of anders aangesloten. nieuwe waardes in eeprom schrijven.
		if (EEPROM.read(STATION_adres[0]) != STATION_dir[0]) {
			EEPROM.write(STATION_adres[0], STATION_dir[0]);
		}

		if (STATION_adres[0] == 3) {
			STATION_adres[1] = 5;
		}
		else {
			STATION_adres[1] = 3;
		}

		STATION_dir[1] = !STATION_dir[0];

		if (EEPROM.read(STATION_adres[1]) != STATION_dir[1]) {
			EEPROM.write(STATION_adres[1], STATION_dir[1]);
			//Serial.println("write in EEPROM");
		}
		/*
		Serial.print("Adres station0= ");
		Serial.print(STATION_adres[0]);
		Serial.print(",  Richting=");
		Serial.println(STATION_dir[0]);

		Serial.print("Adres station1= ");
		Serial.print(STATION_adres[1]);
		Serial.print(",  Richting=");
		Serial.println(STATION_dir[1]);
		*/
		//toggle direction ALTIJD ook als turn uit staat 		 
		//RUN_dir();	

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
			Serial.println(" langzaam werkt");
			//loc keren terug naar station niet nodig denderen van contact maakt richting toch onduidelijk hier			

			RUN_dir(true); //direction
			PWM_puls++; //8mrt minimum snelheid 1 tikje hoger zetten
			PWM_cycle = PWM_max;
			INIT_fase = 4;
			oldstation = 0;
			newstation = STATION_adres[0];
		}
		else { //loc has not moved

			COM_reg |= (1 << 7); //request INIT
			INIT_count++;

			if (INIT_count > 150) {
				//stoploc();				
				Serial.println("FOUT loc beweegt niet....");
				//verleng aanstuurpuls
				PWM_puls++;
				PWM_max++;
				INIT_count = 0;
				//
				//Serial.println(PWM_speed);
				if (PWM_puls > 16) {
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
		Serial.println("hier kijken of er gekeerd moet worden....");

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

		Serial.println("klaar....");
		Serial.print("Route 0= ");
		Serial.println(routetijd[0]);
		Serial.print("Route 1= ");
		Serial.println(routetijd[1]);
		//stoploc();
		//check of beide metingen zijn gelukt, moeten ongeveer gelijk zijn

		if (routetijd[0] > 0 & routetijd[1] > 0 & routetijd[0] * 2 > routetijd[1] & routetijd[1] * 2 > routetijd[0]) {
			INIT_fase = 0; //start pendel
//waarschijnlijk een nodige kering omdat de kering in run_stop niet wordt gedaan, was eerder in run start
			RUN_dir(false);


			RUN_start();
			LED_mode = 1;



		}
		else {
			Serial.println("Tijdmeting is mislukt!!!!!");
			INIT_fase = 0;
			COM_reg |= (1 << 7); //request init exe, herhaal initialisatie
			COM_reg |= (1 << 4);
		}
		break;
	}
}
void RUN_dir(boolean always) {
	//bepaald richting voor het nieuw af te leggen traject, alleen keren als turn is ingesteld
	//Serial.println("RUN_dir");

	if (always == false) {
		//station = INIT_station();
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
	boolean meld = false;
	/*
	Meting twee keer.
	Meerdere opties door wel of niet keren:
	1 beide melders gelijk
	2 melders ongelijk
	*/
	if (bitRead(MEM_reg, 0) == bitRead(MEM_reg, 1)) meld = true;//melders gelijk

	tijd = tijd - 3000; //gemeten tijd zonder remmen minus geschatte afremtijd
	if (station == 5) { //driven from 3 to 5
		routetijd[0] = tijd;
		if (meld == true) {
			routetijd[2] = tijd;
		}
		else {
			routetijd[3] = tijd;
		}
	}
	else { //driven 5 to 3
		routetijd[1] = tijd;
		if (meld == true) {
			routetijd[3] = tijd;
		}
		else {
			routetijd[2] = tijd;
		}
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
	PWM_cycle = PWM_max;	//PWM_max;
	COM_reg |= (1 << 1); //start PWM send, motor on
	COM_reg &= ~(1 << 6); //disable slow down	
}
byte INIT_station() {
	//calc current station
	byte stn;
	stn = PINC;
	stn = stn << 2;
	stn = stn >> 5;

	//spookmeldingen verminderen

	//Serial.print("station in INIT_station: ");
	//Serial.println(station);

	return stn;
}

void RUN_start() {

	byte temp;

	//	Serial.print("RUN_start begin in station:  ");
	//	Serial.println(station);
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

		//tijd tot afremmen bepalen 4 keuzes
		switch (station) {
		case 3:
			newstation = 5;
			stoptijd = routetijd[0];
			if (bitRead(COM_reg, 3) == false)stoptijd = routetijd[2];
			break;
		case 5:
			newstation = 3;
			stoptijd = routetijd[1];
			if (bitRead(COM_reg, 3) == false)stoptijd = routetijd[3];
			break;
		default:
			//nu gaat het fout, opnieuw initialiseren????
			Serial.println("gaat fout geen goed begin station");
			break;
		}
		Serial.print("Stoptijd: ");
		Serial.println(stoptijd);

		//Serial.print("route: ");
		//Serial.print(route);
		//Serial.print(",  Tijd tot vertragen: ");
		//Serial.println(routetijd[route]);

	}
	else {
		RUN_meting = millis();
	}

}
void LED_blink() {  //called every 20ms from SW_read
	switch (LED_mode) {
	case  0:

		break;
	case 1: //normal mode
		break;

	case 2: //init
		LED_init();
		//counter 400ms
		LED_count[0]++;
		if (LED_count[0] > 20) {
			LED_count[0] = 0;
			//payload of this counter
			INIT_reg |= (1 << 4);
			LED_count[1] = 0;
		}

		LED_count[1]++; //counter 40ms
		if (LED_count[1] > 2) {
			LED_count[1] = 0;
			INIT_reg &= ~(1 << 4);
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
	}
}
void LED_init() {
	if (INIT_fase > 0 & bitRead(COM_reg, 4) == true) {
		if (bitRead(INIT_reg, 4) == true) {
			LED_control(10);
		}
		else {
			LED_control(0);
		}
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

	}
}

void RUN_acc() { //called every 20ms from loop
	RUN_count[0]++;

	if (RUN_count[0] > 15) {
		RUN_count[0] = 0;
		PWM_cycle--;
		if (PWM_cycle < PWM_max) {
			COM_reg &= ~(1 << 5); //stop versnellen
		}
	}
}

void RUN_dec() {
	//dit heeft meer waardes nodig

	RUN_count[4]++;
	if (RUN_count[4] > 25) {
		RUN_count[4] = 0;
		if (millis() - Dectime > stoptijd) {
			COM_reg &= ~(1 << 5);  //stop versnellen
			PWM_cycle++;
			if (PWM_cycle > PWM_min) COM_reg &= ~(1 << 6);  //stop vertragen.
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
		//TIJDmeting();
		COM_reg &= ~(1 << 0);
		PORTD &= ~(1 << 7); //PWM poort low			
		COM_reg &= ~(1 << 1); //disable motor pwm
		switch (INIT_fase) {
		case 0: //init finished, normal operation
			/*
			hier moet richting en route worden bepaald, bedenk de loc kan gekeerd zijn... dit kan niet dus richting later aanpassen
			eerst dus de afgelegde route de tid opslaan, richting is nu nog als die route
			*/
			switch (station) {
			case 3:
				route = 1;
				if (bitRead(COM_reg, 3) == false)route = 3;
				break;
			case 5:
				route = 0;
				if (bitRead(COM_reg, 3) == false)route = 2;
				break;
			}
			if (PWM_cycle >= PWM_min) { //hoe hoger pwm_min hoe langzamer de loc bij true dus te langzaam aangekomen
				//if (PWM_cycle - PWM_min > 5) routetijd[route] = routetijd[route] + 200;
				routetijd[route] = routetijd[route] + 200;
			}
			else {
				if (PWM_min - PWM_cycle > 20)routetijd[route] = routetijd[route] - 200;

				routetijd[route] = routetijd[route] - 100;
			}

			RUN_dir(false); //richting instellen
			LED_control(0);
			wachttijd = 2000;
			RUN_wachttijd = millis();
			COM_reg &= ~(1 << 6);  //stop vertragen.

			break;
		case 10: //start init searching station,**********WERKT NIET    zonlicht op melders, aan werken als pcb klaar is. 
			//bij ongelijke melder kering, alleen beginnen in melder die keert.

			if (bitRead(MEM_reg, 0) == bitRead(MEM_reg, 1)) {
				INIT_exe();
			}
			else {
				Serial.println(station);
				Serial.println(bitRead(MEM_reg, 0));
				if (station == 5) {
					if (bitRead(MEM_reg, 0) == true) {
						INIT_exe;
					}
					else {
						newstation = 3;
						startloc();
					}
				}
				else { //station=3
					if (bitRead(MEM_reg, 1) == true) {
						INIT_exe();
					}
					else {
						newstation = 5;
						startloc();
					}
				}
			}

			break;

		default:
			//PORTB ^= (1 << 0); //toggle direction
			//Serial.println("keren stop");
			TIJDmeting();
			INIT_exe();
			break;
		}
	}
}

void SW_read() { // called fromm loop
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

						if (PRG_fase == 1) { //testen melders
							switch (i) {
							case 4:
								PORTD &= ~(1 << 6);
								break;
							case 5:
								PORTD &= ~(1 << 5);
								break;
							}
						}
					}
					else { //port is low
						switch (i) {
						case 0:
							switch (PRG_fase) {
							case 0:
								RUN_dir(true);
								newstation = 10;
								break;
							case 1:
								break;
							case 2: //keren melder 1 aanpassen
								MEM_reg ^= (1 << 0);
								PIND |= (1 << 6);
								break;
							case 3:
								MEM_reg ^= (1 << 1);
								PIND |= (1 << 6);
								break;

							}
							break;
						case 1:
							switch (PRG_fase) {
							case 0:
								COM_reg ^= (1 << 4); //manual stop
								if (bitRead(COM_reg, 4) == false) { //all stop
									stoploc();
									LED_control(0); //kill leds
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
							case 3:
								PRG_fase = 0;
								LED_mode = 0;
								LED_control(0);
								MEM_changed();
								break;
							}
							break;

						case 4: //sensor PINA4 
							station = INIT_station();
							if (station == 5) { //om spookmeldingen te verminderen
								if (PRG_fase == 1) {//meldertest 
									PORTD |= (1 << 6);
								}
								else {
									RUN_stop();
								}
							}

							break;
						case 5: //Sensor PINA5
							station = INIT_station();
							if (station == 3) {
								if (PRG_fase == 1) { //melder test
									PORTD |= (1 << 5);
								}
								else {
									RUN_stop();
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
	tijd = (millis() - RUN_meting);
	//Serial.print("tijd: ");
	//Serial.println(tijd);
}
void SW_both() {
	Serial.println("beide knoppen, begin instellingen");
	stoploc();
	INIT_fase = 0;
	LED_mode = 0;
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
