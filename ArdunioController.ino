/*
 Name:		ArdunioController.ino
 Created:	1/17/2017 11:20:08 AM
 Author:	Evalds Urtans
*/

// kill by voltage, tranistor, serial overflow, strings heap
//http://playground.arduino.cc/Code/Timer1

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include "MemoryFree.h"
#include "TimerThree.h"
#include <avr/wdt.h>

const int PIN_SD_CARD_CLK = 52;
const int PIN_SD_CARD_DO = 50;
const int PIN_SD_CARD_DI = 51;
const int PIN_SD_CARD_CS = 53;

const int PIN_RELAY_1_WATER_PUMP = 2;
const int PIN_RELAY_2_HEATER = 4;
const int PIN_RELAY_3_FAN = 9;
const int PIN_RELAY_4_VALVE = 7;
const int PIN_RELAY_5_COMPRESSOR = 5;

const int PIN_TIMER_3 = 3;

const int PIN_SERIAL_1_TX = 18;
const int PIN_SERIAL_1_RX = 19;

const int PIN_SDA_TIMER = 20;
const int PIN_SDL_TIMER = 21;

const int PIN_TEMPERATURE_BOARD = 22;
const int PIN_TEMPERATURE_OUTSIDE = 24;

const int PIN_SNOW_SENSOR = A7;

const int PIN_A_TEMPERATURE_WATER_IN = A11;
const int PIN_A_TEMPERATURE_WATER_OUT = A14;
const int PIN_A_TEMPERATURE_EVAPORATOR = A15;
const int PIN_A_TEMPERATURE_REFERENCE = A12;
const int PIN_A_TEMPERATURE_OUTSIDE = A10;

const int PIN_SWITCH_HIGH_PRESSURE = 10;
const int PIN_SWITCH_LOW_PRESSURE = 11;
const int PIN_SWITCH_PRESSURE_LINER = 12;

void (*reset_force)(void) = 0;

RTC_DS1307 RTC; //PIN 21 20

OneWire oneWire_TemperatureBoard(PIN_TEMPERATURE_BOARD);
DallasTemperature TemperatureBoard(&oneWire_TemperatureBoard);

OneWire oneWire_TemperatureOutside(PIN_TEMPERATURE_OUTSIDE);
DallasTemperature TemperatureOutside(&oneWire_TemperatureOutside);

const char* PARAM_FILE = "par_new.txt";

const String MODE_NONE = "none";
const String MODE_HEATING = "heating";
const String MODE_DEFROST = "defrost";

const float MAX_TEMP = 55.0f;
const float MIN_TEMP = 45.0f;
const float TEMP_COEF_PLUS_MINUS = -0.3f; //Adaptive heating temperature
const float TEMP_THERESHOLD_DROP = 7.0f; //At which start to heat again

const int TIME_SECONDS_WARMUP = 30;

const int SNOW_SENSOR_RADIATOR_FROZEN = 500; //550
const int SNOW_SENSOR_RADIATOR_CLEAN = 800; // 840;

int soft_wdt = 0;
int heart_beat = LOW;

float setting_temperature = 49.0f;
float setting_temperature_buffer = 3.0f;
float setting_time_between_heating = 60.0f * 7.0f; // 20
float setting_time_between_defrost = 60.0f * 30.0f; //120
float setting_time_min_defrost = 60.0f * 2.0f;

bool setting_is_logging_disabled = false;

bool setting_is_rtc_enabled = false;
bool setting_is_debug = false;
bool setting_is_output_log = true;
float setting_debug_TEMPERATURE_WATER_IN = 0.0f;
float setting_debug_TEMPERATURE_WATER_OUT = 0.0f;
float setting_debug_TEMPERATURE_EVAPORATOR = 0.0f;
float setting_debug_TEMPERATURE_REFERENCE = 0.0f;
float setting_debug_TEMPERATURE_OUTSIDE = 0.0f;
bool setting_debug_reset_rtc = false;

String param_mode;
long param_time_startup = 0;
long param_time_last_log = 0;
long param_time_last_defrost = 0;
long param_time_last_heating = 0;
long param_time_last_heating_stop = 0;
bool param_water_heater_status = false;
bool param_is_force_defrost = false;
bool param_is_force_start_heating = false;
bool param_is_force_stop_heating = false;

bool param_relay_1 = false;
bool param_relay_2 = false;
bool param_relay_3 = false;
bool param_relay_4 = false;
bool param_relay_5 = false;

bool param_is_started = false;

int param_temperature_fuse_blown_counter = 0;

volatile long param_rtc_simulation = 0; // 90648
int _temp_rtc_counter = 0;

bool is_warning_log = false;
bool is_warning_params = false;

bool is_show_voltage = true;

char _temp_cast_string_buf[50];
char _temp_datetime_string_buf[100];
char _temp_log_message[300];

void delay_safe(int milisec) {
	int parts = ceil((float)milisec / 500.0);
	for(int i = 0; i < parts; i++)
	{
		soft_wdt = 0;
		wdt_reset();
		delay(500);
	}
	
	//If timer do not work
	//param_rtc_simulation += (long)ceil((float)milisec / 1000.0);
}

char* long_to_string(long value)
{
	ltoa(value, _temp_cast_string_buf, 10);
	return _temp_cast_string_buf;
}

int compare_string(char *first, char *second) {
   while (*first == *second) {
      if (*first == '\0' || *second == '\0')
         break;
 
      first++;
      second++;
   }
 
   if (*first == '\0' && *second == '\0')
      return 1;
   else
      return 0;
}

char* float_to_string(float value)
{
	dtostrf(value, 10, 4, _temp_cast_string_buf);
	return _temp_cast_string_buf;
}

// Free memory http://playground.arduino.cc/Code/AvailableMemory
// https://hackingmajenkoblog.wordpress.com/2016/02/04/the-evils-of-arduino-strings/

void save_params() {
	//SD.remove(PARAM_FILE);
	//delay(100);
	//Serial.println("exist: " + String(SD.exists("pr.txt")));
	File myFile = SD.open(PARAM_FILE, O_TRUNC | O_WRITE | O_CREAT);
	if (myFile) {
		wdt_reset();
		myFile.seek(0);

		if (!setting_is_rtc_enabled)
		{
			myFile.write(long_to_string(param_rtc_simulation));
			myFile.write("\n");
		}
		myFile.write(long_to_string(param_time_last_defrost));
		myFile.write("\n");
		myFile.write(long_to_string(param_time_last_heating));
		myFile.write("\n");
		myFile.write(long_to_string(param_time_last_heating_stop));
		myFile.write("\n");
		myFile.write(float_to_string(setting_temperature));
		myFile.write("\n");
		myFile.close();
		wdt_reset();
	}
	else {
		if (!is_warning_params)
		{
			is_warning_params = true;
			Serial.println(F("error writing params"));
		}
	}
}

void load_params() {
	File myFile = SD.open(PARAM_FILE);
	if (myFile) {
		myFile.seek(0);

		if (myFile.available() && !setting_is_rtc_enabled) {
			String str = myFile.readStringUntil('\n');
			param_rtc_simulation = atol((str.c_str()));
		}
		if (myFile.available()) {
			String str = myFile.readStringUntil('\n');
			str.trim();
			param_time_last_defrost = atol((str.c_str()));
		}
		if (myFile.available()) {
			String str = myFile.readStringUntil('\n');
			str.trim();
			param_time_last_heating = atol((str.c_str()));
		}
		if (myFile.available()) {
			String str = myFile.readStringUntil('\n');
			str.trim();
			param_time_last_heating_stop = atol((str.c_str()));
		}
		if (myFile.available()) {
			String str = myFile.readStringUntil('\n');
			str.trim();
			setting_temperature = str.toFloat();
		}

		//Unifinished heating cycle
		//Restart heating
		if (param_time_last_heating > param_time_last_heating_stop) {
			param_time_last_heating = param_time_last_heating_stop;
		}

		// close the file:
		myFile.close();
	}
	else {
		if (!is_warning_params)
		{
			is_warning_params = true;
			Serial.println(F("error reading params"));
		}
	}

	output_params();
}


long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

void output_voltage(bool isForceShown = false)
{
	if(is_show_voltage || isForceShown)
	{
		Serial.print(F("voltage = "));
		Serial.println(long_to_string(readVcc()));
	}
}

void output_params() {
	Serial.print(F("param_rtc_simulation = "));
	Serial.println(long_to_string(param_rtc_simulation));

	Serial.print(F("param_time_last_defrost = "));
	Serial.println(long_to_string(param_time_last_defrost));

	Serial.print(F("param_time_last_heating = "));
	Serial.println(long_to_string(param_time_last_heating));

	Serial.print(F("param_time_last_heating_stop = "));
	Serial.println(long_to_string(param_time_last_heating_stop));

	Serial.print(F("setting_time_between_heating = "));
	Serial.println(float_to_string(setting_time_between_heating));

	Serial.print(F("setting_temperature = "));
	Serial.println(float_to_string(setting_temperature));
}

char* get_day_of_week(uint8_t dow) {

	switch (dow) {
	case 0: strcpy(_temp_cast_string_buf, "7day"); break;
	case 1: strcpy(_temp_cast_string_buf, "1day"); break;
	case 2: strcpy(_temp_cast_string_buf, "2day"); break;
	case 3: strcpy(_temp_cast_string_buf, "3day"); break;
	case 4: strcpy(_temp_cast_string_buf, "4day"); break;
	case 5: strcpy(_temp_cast_string_buf, "5day"); break;
	case 6: strcpy(_temp_cast_string_buf, "6day"); break;
	}

	return _temp_cast_string_buf;
}

DateTime get_datetime()
{
	DateTime datetime;
	if (!setting_is_rtc_enabled)
	{
		datetime = DateTime(param_rtc_simulation);
	}
	else
	{
		datetime = RTC.now();
	}

	return datetime;
}

long get_secondstime()
{
	return setting_is_rtc_enabled ? RTC.now().secondstime() : param_rtc_simulation;
}

char* get_full_datetime() {
	DateTime datetime = get_datetime();
	sprintf(_temp_datetime_string_buf,
		"%s, %u.%u.%u %u:%u:%u\0",
		get_day_of_week(datetime.dayOfTheWeek()),
		datetime.day(),
		datetime.month(),
		datetime.year(),
		datetime.hour(),
		datetime.minute(),
		datetime.second()
		);
	return _temp_datetime_string_buf;
}

char* get_day_and_month(int time = 0) {
	
	DateTime datetime = time == 0 ? get_datetime() : DateTime(time);

	sprintf(_temp_datetime_string_buf,
		"%u-%u\0",
		datetime.day(),
		datetime.month()
		);
	return _temp_datetime_string_buf;
}

void i2c_scanner()
{
	Serial.println();
	Serial.println(F("I2C scanner. Scanning ..."));
	byte count = 0;

	Wire.begin();
	for (byte i = 8; i < 120; i++)
	{
		Wire.beginTransmission(i);
		if (Wire.endTransmission() == 0)
		{
			Serial.print(F("Found address: "));
			Serial.print(i, DEC);
			Serial.print(F(" (0x"));
			Serial.print(i, HEX);
			Serial.println(F(")"));
			count++;
			delay(1);  // maybe unneeded?
		} // end of good response
	} // end of for loop
	Serial.println(F("Done."));
	Serial.print(F("Found "));
	Serial.print(count, DEC);
	Serial.println(F(" device(s)."));
}

float get_temperature_analog(int pin) {
	float read = (float)analogRead(pin);
	float slope = -0.12045424f;
	float intercept = 107.6556123f;

	if (read > 990)
	{
		slope = -0.207546876;
		intercept = 185.8904819;
	}

	if (setting_is_debug)
	{
		switch (pin)
		{
		case PIN_A_TEMPERATURE_WATER_IN:
			return setting_debug_TEMPERATURE_WATER_IN;
			break;
		case PIN_A_TEMPERATURE_WATER_OUT:
			return setting_debug_TEMPERATURE_WATER_OUT;
			break;
		case PIN_A_TEMPERATURE_EVAPORATOR:
			return setting_debug_TEMPERATURE_EVAPORATOR;
			break;
		case PIN_A_TEMPERATURE_REFERENCE:
			return setting_debug_TEMPERATURE_REFERENCE;
			break;
		case PIN_A_TEMPERATURE_OUTSIDE:
			return setting_debug_TEMPERATURE_OUTSIDE;
			break;

		}
	}

	return read * slope + intercept;
}

float get_temperature_digital(int pin) {
	float temp = 99.0f;

	if (pin == PIN_TEMPERATURE_BOARD) {
		TemperatureBoard.requestTemperatures();
		temp = TemperatureBoard.getTempCByIndex(0);
	}
	else if (pin == PIN_TEMPERATURE_OUTSIDE) {
		TemperatureOutside.requestTemperatures();
		temp = TemperatureOutside.getTempCByIndex(0);
	}

	return temp;
}

void write_log(const char* str, const char* fileType = "L" )
{
	if (setting_is_output_log)
	{
		Serial.println(str);
	}

	if (setting_is_logging_disabled)
		return;

	wdt_reset();
	char fileNameHistory[80];
	//Keep last 14 days of log
	sprintf(fileNameHistory, "%s%s\0", fileType, get_day_and_month(get_secondstime() - 86400 * 14));
	if(SD.exists(fileNameHistory))
	{
		SD.remove(fileNameHistory);
	}
	wdt_reset();

	char fileName[80];
	sprintf(fileName, "%s%s\0", fileType, get_day_and_month());

	File myFile = SD.open(fileName, O_CREAT | O_WRITE | O_APPEND);
	if (myFile) {
		wdt_reset();
		myFile.print(get_full_datetime());
		myFile.print(";"); 
		myFile.println(str);
		wdt_reset();
		myFile.close();
		wdt_reset();
	}
	else {
		if (!is_warning_log)
		{
			is_warning_log = true;
			Serial.print(F("error writing log: "));
			Serial.println(fileName);
			Serial.println(str);
		}
	}

	if (param_is_started)
		save_params();
}

void transmit_log(const char* name)
{
	wdt_reset();
	File myFile = SD.open(name);
	if (myFile) {
		Serial.println(F("File Found"));
		while (myFile.available()) {
			Serial.write(myFile.read());
			wdt_reset();
		}
		myFile.close();
	}
	else {
		if (!is_warning_log)
		{
			is_warning_log = true;
			Serial.print(F("error opening log: "));
			Serial.println(name);
		}
	}
}

bool set_relay(int pin, bool state)
{
	char id[30];
	strcpy(id, "Unknown");
	bool isChanged = false;

	switch (pin)
	{
	case PIN_RELAY_1_WATER_PUMP:
		strcpy(id, "PIN_RELAY_1_WATER_PUMP");
		if (param_relay_1 != state)
		{
			param_relay_1 = state;
			isChanged = true;
		}
		break;
	case PIN_RELAY_2_HEATER:
		strcpy(id,"PIN_RELAY_2_HEATER");
		if (param_relay_2 != state)
		{
			param_relay_2 = state;
			isChanged = true;
		}
		break;
	case PIN_RELAY_3_FAN:
		strcpy(id, "PIN_RELAY_3_FAN");
		if (param_relay_3 != state)
		{
			param_relay_3 = state;
			isChanged = true;
		}
		break;
	case PIN_RELAY_4_VALVE:
		strcpy(id, "PIN_RELAY_4_VALVE");
		if (param_relay_4 != state)
		{
			param_relay_4 = state;
			isChanged = true;
		}
		break;
	case PIN_RELAY_5_COMPRESSOR:
		strcpy(id, "PIN_RELAY_5_COMPRESSOR");
		if (param_relay_5 != state)
		{
			param_relay_5 = state;
			isChanged = true;
		}
		break;
	}

	char msg[100];
	sprintf(msg, "%s %s\0", id, state ? "True" : "False");
	write_log(msg);
	output_voltage();
	delay_safe(500);

	digitalWrite(pin, state ? HIGH : LOW);

	for(int i = 0; i < 10; i++)
	{
		output_voltage();
		delay_safe(500);
	}
	return state ? HIGH : LOW;
}

void mode_start_heating()
{
	param_mode = MODE_HEATING;
	write_log("mode: start heating");

	set_relay(PIN_RELAY_1_WATER_PUMP, true);
	set_relay(PIN_RELAY_2_HEATER, param_water_heater_status);
	set_relay(PIN_RELAY_4_VALVE, true);
  set_relay(PIN_RELAY_3_FAN, true);  
	set_relay(PIN_RELAY_5_COMPRESSOR, true);
}

void mode_stop_heating()
{
	param_mode = MODE_NONE;
	write_log("mode: stop heating");

  param_time_last_heating_stop = get_secondstime();

	set_relay(PIN_RELAY_5_COMPRESSOR, false);
	set_relay(PIN_RELAY_1_WATER_PUMP, true);
	set_relay(PIN_RELAY_2_HEATER, param_water_heater_status);		
	set_relay(PIN_RELAY_4_VALVE, true);	
	set_relay(PIN_RELAY_3_FAN, false);
}

void mode_start_defrost()
{
	param_mode = MODE_DEFROST;
	write_log("mode: start defrost");

	set_relay(PIN_RELAY_1_WATER_PUMP, true);
	set_relay(PIN_RELAY_2_HEATER, param_water_heater_status);
	set_relay(PIN_RELAY_3_FAN, false);
	set_relay(PIN_RELAY_5_COMPRESSOR, false);
	set_relay(PIN_RELAY_4_VALVE, false);
	set_relay(PIN_RELAY_5_COMPRESSOR, true);
}

void mode_stop_defrost()
{
	param_mode = MODE_NONE;
	write_log("mode: stop defrost");

	set_relay(PIN_RELAY_5_COMPRESSOR, false);
	set_relay(PIN_RELAY_1_WATER_PUMP, true);
	set_relay(PIN_RELAY_2_HEATER, false);
	set_relay(PIN_RELAY_3_FAN, false);	
	set_relay(PIN_RELAY_4_VALVE, true);
	delay_safe(30000);
}

void mode_stop()
{
	if (param_mode == MODE_DEFROST)
	{
		mode_stop_defrost();
	}
	else if (param_mode == MODE_HEATING)
	{
		mode_stop_heating();
	}
}

void log_sensors(bool is_write_log = true)
{
	if (is_write_log || !setting_is_logging_disabled)
	{
		/*
		Serial.println(get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN));
		Serial.println(get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT));
		Serial.println(get_temperature_analog(PIN_A_TEMPERATURE_REFERENCE));
		Serial.println(get_temperature_analog(PIN_A_TEMPERATURE_OUTSIDE));
		Serial.println(get_temperature_digital(PIN_TEMPERATURE_OUTSIDE));
		Serial.println("----");*/

		char temp_in[50];
		strcpy(temp_in, float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN)));
		char temp_out[50];
		strcpy(temp_out, float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT)));

		char temp_evap[50];
		strcpy(temp_evap, float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_EVAPORATOR)));

		//char temp_ref[50];
		//strcpy(temp_ref, float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_REFERENCE)));

		char temp_ou2[50];
		strcpy(temp_ou2, float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_OUTSIDE)));

		char temp_ou[50];
		strcpy(temp_ou, float_to_string(get_temperature_digital(PIN_TEMPERATURE_OUTSIDE)));

		sprintf(_temp_log_message,
			"%d;%d;%s;%s;%s;%s;%s;%d;%d;%d;%d\0",
			(param_mode == MODE_HEATING ? 1 : 0),
			(param_mode == MODE_DEFROST ? 1 : 0),
			temp_in,
			temp_out,
			//temp_ref,
			temp_evap,
			temp_ou2,
			temp_ou,
			analogRead(PIN_SNOW_SENSOR),
			digitalRead(PIN_SWITCH_HIGH_PRESSURE),
			digitalRead(PIN_SWITCH_LOW_PRESSURE),
			digitalRead(PIN_SWITCH_PRESSURE_LINER)
		);
		if (is_write_log)
		{
			write_log(_temp_log_message, "X");
		}
		else
		{
			Serial.println(_temp_log_message);
		}
	}
}


void setup() {

	wdt_disable();

	// Ardunio/Geduino Mega 1220 board
	// http://www.electroschematics.com/wp-content/uploads/2013/01/Arduino-Mega-2560-Pinout.jpg
	pinMode(LED_BUILTIN, OUTPUT);

	TemperatureBoard.begin();
	TemperatureOutside.begin();

	pinMode(PIN_SWITCH_HIGH_PRESSURE, INPUT_PULLUP);
	pinMode(PIN_SWITCH_LOW_PRESSURE, INPUT_PULLUP);
	pinMode(PIN_SWITCH_PRESSURE_LINER, INPUT_PULLUP);

	pinMode(PIN_RELAY_1_WATER_PUMP, OUTPUT);
	pinMode(PIN_RELAY_2_HEATER, OUTPUT);
	pinMode(PIN_RELAY_3_FAN, OUTPUT);
	pinMode(PIN_RELAY_4_VALVE, OUTPUT);
	pinMode(PIN_RELAY_5_COMPRESSOR, OUTPUT);
	
	pinMode(PIN_SNOW_SENSOR, INPUT);

	Serial.begin(9600);
	while (!Serial) {}
	Serial.println("Opened");


	if (!SD.begin()) {
		Serial.println("SD failed!");
	}
	else
	{
		Serial.println("SD connected.");
	}

	write_log("SETUP");

	//i2c_scanner();

	if (setting_is_rtc_enabled)
	{
		Serial.println("RTC begin");
		RTC.begin();
		Serial.println("RTC status");
		Serial.println(RTC.isrunning());
		Serial.println("RTC set date time");

		if (RTC.isrunning() || setting_debug_reset_rtc)
		{
			RTC.adjust(DateTime(__DATE__, __TIME__));
		}

		Serial.println(RTC.isrunning());
		Serial.println(get_full_datetime());

		DateTime dateTime = RTC.now();
	}

	param_time_last_defrost = get_secondstime();
	param_time_last_heating_stop = get_secondstime();

	load_params();

	is_show_voltage = false;
	setting_is_output_log = true; 
	write_log("RESTART");

	delay(10000);

	wdt_enable(WDTO_8S);
	wdt_reset();

	param_mode = MODE_NONE;
	set_relay(PIN_RELAY_5_COMPRESSOR, false);
	set_relay(PIN_RELAY_2_HEATER, param_water_heater_status);
	set_relay(PIN_RELAY_3_FAN, false);		
	set_relay(PIN_RELAY_4_VALVE, true);
	set_relay(PIN_RELAY_1_WATER_PUMP, true);

		
	write_log("RESTART FINISHED");
	
	//http://astro.neutral.org/arduino/arduino-pwm-pins-frequency.shtml
	Timer3.initialize(500000);         // initialize timer1, and set a 1/2 second period	
	Timer3.pwm(PIN_TIMER_3, 512);                // setup pwm on pin 9, 50% duty cycle
	Timer3.attachInterrupt(timer_callback);  // attaches callback() as a timer overflow interrupt


	is_show_voltage = false;
	param_is_started = true;
	setting_is_output_log = false;

	param_time_startup = get_secondstime();
}

void timer_callback()
{
	_temp_rtc_counter++;
	if(_temp_rtc_counter >= 20)
	{
		_temp_rtc_counter = 0;
		param_rtc_simulation += 10;
	}

}

void loop() {
	wdt_reset();
	soft_wdt = 0;

	heart_beat = (heart_beat == HIGH ? LOW : HIGH);
	digitalWrite(LED_BUILTIN, heart_beat);

	long secondstime = get_secondstime();

	if(param_temperature_fuse_blown_counter > 30)
	{
		//20 sec of abnormal temperature
		//Shutdown everything
		delay(1000);
		return;
	}

	
	//Adaptive temperature
	//setting_temperature @ 0 deg
	float target_temperature = setting_temperature + get_temperature_digital(PIN_TEMPERATURE_OUTSIDE) * TEMP_COEF_PLUS_MINUS; //coef
	if (target_temperature < MIN_TEMP) target_temperature = MIN_TEMP;
	if (target_temperature > MAX_TEMP) target_temperature = MAX_TEMP - 1.5f;


	if (Serial.available() > 0) {
		String query = Serial.readString();
		String msg = query;
		String param = "";

		int pos = query.indexOf(" ");
		if (pos >= 0)
		{
			msg = query.substring(0, pos);
			param = query.substring(pos);
		}

		msg.trim();
		param.trim();

		Serial.print(F("msg: '"));
		Serial.print(msg);
		Serial.print(F("' '"));
		Serial.print(param);
		Serial.println(F("'"));
		
		//Great explanation https://learn.adafruit.com/memories-of-an-arduino/optimizing-sram

		if (msg == F("RTC")) {
			Serial.println(get_full_datetime());
		}
		else if (msg == F("setting_debug_TEMPERATURE_WATER_IN"))
		{
			setting_debug_TEMPERATURE_WATER_IN = param.toFloat();
		}
		else if (msg == F("setting_debug_TEMPERATURE_WATER_OUT"))
		{
			setting_debug_TEMPERATURE_WATER_OUT = param.toFloat();
		}
		else if (msg == F("setting_debug_TEMPERATURE_EVAPORATOR"))
		{
			setting_debug_TEMPERATURE_EVAPORATOR = param.toFloat();
		}
		else if (msg == F("setting_debug_TEMPERATURE_OUTSIDE"))
		{
			setting_debug_TEMPERATURE_OUTSIDE = param.toFloat();
		}
		else if (msg == F("setting_debug_TEMPERATURE_REFERENCE"))
		{
			setting_debug_TEMPERATURE_REFERENCE = param.toFloat();
		}
		else if(msg == F("is_show_voltage"))
		{
			is_show_voltage = (param == F("ON"));
			Serial.println(F("is_show_voltage SET"));
		}
		else if(msg == F("show_voltage"))
		{
			output_voltage(true);
		}
		else if(msg == F("load_params"))
		{
			load_params();
			output_params();
		}
    else if(msg == "erase")
    {
      File root = SD.open("/");
      while (true) {
        root.rewindDirectory();
        File entry =  root.openNextFile();        
        if (! entry) {
          // no more files
          break;
        }

        Serial.println(entry.name());

        if (entry.isDirectory()) {
          SD.rmdir(entry.name());

        }
        else
        {
          SD.remove(entry.name());
        }

        wdt_reset();
      }
    }
		else if(msg == F("target_temperature"))
		{
			Serial.print(F("target_temperature ="));
			Serial.println(target_temperature);
		}
		else if(msg == F("free_memory"))
		{
			Serial.print(F("freeMemory ="));
			Serial.println(freeMemory());
		}
		else if(msg == F("param_rtc_simulation"))
		{
			param_rtc_simulation = atol((param.c_str()));
			Serial.print(F("param_rtc_simulation ="));
			Serial.println(param_rtc_simulation);
		}
		else if (msg == F("PARAMS"))
		{
			Serial.print(F("secondstime = "));
			Serial.println(secondstime);
			Serial.print(F("setting_time_between_heating = "));
			Serial.println(setting_time_between_heating);
			output_params();
		}
		else if (msg == F("setting_time_between_heating"))
		{
			setting_time_between_heating = atol((param.c_str()));
			Serial.println(F("setting_time_between_heating SET"));
		}
		else if (msg == F("OUTPUT"))
		{
			setting_is_output_log = (param == F("ON"));
			Serial.println(F("OUTPUT SET"));
		}
		else if (msg == F("MODE_DEFROST"))
		{
			param_is_force_defrost = (param == F("ON"));
			param_time_last_defrost = (param == F("ON") ? secondstime - setting_time_between_defrost * 2 : secondstime + setting_time_between_defrost * 2);
			Serial.println(F("MODE_DEFROST SET"));
		}
		else if (msg == F("MODE_HEATING"))
		{
			param_time_last_heating_stop = 
				param_time_last_heating
				= (param == F("ON") ? secondstime - 100 * 60 : secondstime + 100 * 60);

			param_is_force_stop_heating = (param != F("ON"));
			param_is_force_start_heating = (param == F("ON"));

			Serial.println(F("MODE_HEATING SET"));
		}
		else if (msg == F("SET_TEMP")) {
			setting_temperature = param.toFloat();
			char msg[80];
			sprintf(msg, "%s %s\0", query.c_str(), float_to_string(setting_temperature));
			write_log(msg);
			Serial.println(F("TEMPERATURE SET"));
			save_params();
		}
		else if (msg == F("log_sensors")) {
			log_sensors(false);
		}
		else if (msg == F("PIN_RELAY_1_WATER_PUMP")) {
			set_relay(PIN_RELAY_1_WATER_PUMP, param == F("ON"));
		}
		else if (msg == F("PIN_RELAY_2_HEATER")) {
			set_relay(PIN_RELAY_2_HEATER, param == F("ON"));
		}
		else if (msg == F("PIN_RELAY_3_FAN")) {
			set_relay(PIN_RELAY_3_FAN, param == F("ON"));
		}
		else if (msg == F("PIN_RELAY_4_VALVE")) {
			set_relay(PIN_RELAY_4_VALVE, param == "ON");
		}
		else if (msg == F("PIN_RELAY_5_COMPRESSOR")) {
			set_relay(PIN_RELAY_5_COMPRESSOR, param == F("ON"));
		}
		else if (msg == F("PIN_SNOW_SENSOR")) {
			Serial.println(analogRead(PIN_SNOW_SENSOR));
		}
		else if (msg == F("LOG"))
		{
			if (param == "")
			{
				param = String("L") + String(get_day_and_month());
				Serial.println(param);
			}
			transmit_log(param.c_str());
		}
		else if (msg == F("TEST_SD")) {

			Serial.println(F("Check"));

			Serial.flush();
			delay_safe(1000);

			File myFile = SD.open(F("test2.txt"), FILE_WRITE);

			// if the file opened okay, write to it:
			if (myFile) {
				Serial.print(F("Writing to test2.txt..."));
				myFile.println(get_full_datetime());
				// close the file:
				myFile.close();
				Serial.println(F("done."));
			}
			else {
				// if the file didn't open, print an error:
				Serial.println(F("error opening test.txt"));
			}

			Serial.flush();
			delay_safe(1000);

			myFile = SD.open(F("test2.txt"));
			if (myFile) {
				Serial.println(F("test2.txt:"));

				// read from the file until there's nothing else in it:
				while (myFile.available()) {
					Serial.write(myFile.read());
				}
				// close the file:
				myFile.close();
			}
			else {
				// if the file didn't open, print an error:
				Serial.println(F("error opening test.txt"));
			}


			Serial.flush();

			delay_safe(1000);
		}

		save_params();
	}

	//Program operations fuses
	int saftey_reading = (setting_is_debug ? HIGH : LOW);
	int value_pin_high_pressure = digitalRead(PIN_SWITCH_HIGH_PRESSURE);
	int value_pin_low_pressure = digitalRead(PIN_SWITCH_LOW_PRESSURE);
	int value_pin_pressure_liner = digitalRead(PIN_SWITCH_PRESSURE_LINER);


	if (target_temperature > 0  &&
		(secondstime - param_time_startup) > TIME_SECONDS_WARMUP &&
		value_pin_high_pressure == saftey_reading &&
		value_pin_low_pressure == saftey_reading &&
		value_pin_pressure_liner == saftey_reading)
	{
		if (param_mode == MODE_NONE)
		{
			//TODO pec defrost gaidit
			if (param_is_force_defrost ||
				(secondstime - param_time_last_heating_stop > setting_time_between_heating * 0.5 &&
					secondstime - param_time_last_defrost > setting_time_between_defrost
					&& get_temperature_digital(PIN_TEMPERATURE_OUTSIDE) < 5.0 // Not warm enough outside
					&& analogRead(PIN_SNOW_SENSOR) < SNOW_SENSOR_RADIATOR_FROZEN)) //Frozen radiator
			{
				param_is_force_defrost = false;
				param_time_last_defrost = secondstime;
				save_params();

				mode_start_defrost();
			}
			else
			{
				if (param_is_force_start_heating ||
					(secondstime - param_time_last_heating_stop > setting_time_between_heating && //No sooner than 10 min
					get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT) < target_temperature - TEMP_THERESHOLD_DROP))
				{
					param_is_force_start_heating = false;

					param_time_last_heating = secondstime;
					save_params();

					char tmp[100];
					sprintf(tmp,
						"target_temperature:%s\0",
						float_to_string(target_temperature));
					write_log(tmp);

					mode_start_heating();
				}
			}
		}
		else
		{
			if (param_mode == MODE_HEATING)
			{
				if ((secondstime - param_time_last_heating > 60 * 3 && //Not shorter than 3min
					get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN) >= target_temperature + setting_temperature_buffer) ||
					get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN) >= MAX_TEMP || //Fuse
					get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT) >= MAX_TEMP || //Fuse
					secondstime - param_time_last_heating >= 60 * 20 || //Not longer than 20min
					param_is_force_stop_heating) 
				{
					param_is_force_stop_heating = false;					
					save_params();

					mode_stop_heating();
				}
			}
			else if (param_mode == MODE_DEFROST)
			{
				if ((secondstime - param_time_last_defrost > setting_time_min_defrost && 
					analogRead(PIN_SNOW_SENSOR) > SNOW_SENSOR_RADIATOR_CLEAN) ||  //2min+limit
					secondstime - param_time_last_defrost > 60 * 4) 
				{
					mode_stop_defrost();
				}
			}
		}
	}
	else
	{
		if (param_mode != MODE_NONE)
		{
      mode_stop();
    
			write_log("FUSE BLOWN");
      char tmp[100];
      sprintf(tmp, "high:%d low:%d liner:%d", value_pin_high_pressure, value_pin_low_pressure, value_pin_pressure_liner);
      write_log(tmp);
			save_params();			
		}
	}

	if (secondstime - param_time_last_log > 60)
	{
		log_sensors();
		param_time_last_log = secondstime;
	}

	if(get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN) >= MAX_TEMP + 10.0f || //Fuse
		get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT) >= MAX_TEMP + 10.0f)
	{
		param_temperature_fuse_blown_counter++;

		char tmp[100];
		sprintf(tmp,
			"abnormal temperature:%s ; %s\0",
			float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_WATER_IN)), 
			float_to_string(get_temperature_analog(PIN_A_TEMPERATURE_WATER_OUT)));
		write_log(tmp);

    if(param_temperature_fuse_blown_counter > 5)
    {
		  mode_stop();
    }
	}
  else
  {
    param_temperature_fuse_blown_counter = 0;
  }


	delay_safe(1000);

}
