/*

  Completed:
   SD card attached to SPI bus as follows:
 ** MOSI - pin 51
 ** MISO - pin 50
 ** CLK - pin 52
 ** CS - pin 53


  TODO:
  -Set up Real Time Clock for everything to use.
  -Set up Tracksoar & 2ndary
  -Set up Altimeter
  -Actuation Code w/ conditions
  -Wire diagram


  by Kireet Agrawal - 2018
*/

#include <SPI.h>
#include <SD.h>
#include <Wire.h> // I2C library
#include "Adafruit_SI1145.h"  // UV sensor library
#include "Adafruit_MAX31855.h"  // Thermocouple library
#include <Servo.h>
#include <RTClib.h>
#include <Stream.h>
#include <Time.h>
#include "IntersemaBaro.h"

// Arduino Mega SPI Pins
#define MISO 50
#define MOSI 51
#define CLK 52

// Arduino Mega I2C Pins
#define SCL 21
#define SDA 20



RTC_DS1307 RTC; // define the Real Time Clock object

#define DATA_HEADERS "Date, Time, UV, IR, Visible, ThermoCouple Internal Temp (C), ThermoCouple Temp (C), Altitude (m), Pressure (Pa), Altitude Temp (C),Servo1 Extended, Servo2 Extended"
int sd_card_pin = 47;  // SD card CS pin
String delimiter = ",";  // Data string delimiter for SD logging b/w sensors
File sd_card_file;  // filesystem object
String curr_data = "";

Intersema::BaroPressure_MS5607B baro(true);

Adafruit_SI1145 uv_sensor = Adafruit_SI1145();  // uv sensor object declaration

// Thermo Couple
int thermoCS = 48;
Adafruit_MAX31855 thermocouple(CLK, thermoCS, MISO);  // Initializes the Thermocouple

// Conversion constants
double ft_to_m = 0.3048;

// Experiment Specific Code
int servo1_pin = 2;
int servo2_pin = 3;

int servo_min = 45; // Value to retract servo to
int servo_max = 130; // Value to extend servo to (changed from 180 for servo1 and 165 for servo2

// 35-45K feet altitude
int servo1_start_alt = 35000 * ft_to_m; // Feet converted to meters
// 90-100K feet altitude
int servo2_start_alt = 90000 * ft_to_m;

// Timer 900 seconds
unsigned long timeout = 900000; // Milliseconds TODO FIXME

// Globals for servos
Servo servo1;
Servo servo2;
bool servo1_extended = false;
bool servo2_extended = false;
unsigned long exp1_start_time = 0;
unsigned long exp2_start_time = 0;
bool exp1_complete = false;
bool exp2_complete = false;

void setup() {
  
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  setup_rtc();

  setup_SD();
  setup_UV();
  setup_thermo();
  baro.init();
  
  write_to_sd("test.csv", DATA_HEADERS);
  setup_servos();  // Experiment specific linear actuator setup, takes up to 20 seconds
}

void loop() {
  
  // put your main code here, to run repeatedly:
  // fetch the time
   String curr_time = get_rtc();
   Serial.print("Current Time: ");
   Serial.println(curr_time);
   curr_data += curr_time;
   curr_data += delimiter;

   String uv_data = get_uv_data();
   Serial.print("UV Data: ");
   Serial.println(uv_data);
   curr_data += uv_data;
   curr_data += delimiter;

  String thermo_data = get_thermo_data();
  Serial.print("Thermocouple Temp: ");
  Serial.println(thermo_data);
  curr_data += thermo_data;
  curr_data += delimiter;

  double alt = baro.getHeightMeters(2);
  int32_t alt_pressure = baro.getP(2);
  double alt_temp = (double)(baro.getT(2))/100;
  Serial.print("Meters: ");
  Serial.print((float)(alt));
  Serial.print(", Feet: ");
  Serial.println((float)(alt) * 3.2808);
  Serial.print("Pressure (Pa): ");
  Serial.println(alt_pressure);
  Serial.print("Altimeter Temp: ");
  Serial.println(alt_temp);
  curr_data += String(alt, 2);
  curr_data += delimiter;
  curr_data += String(alt_pressure);
  curr_data += delimiter;
  curr_data += String(alt_temp);
  curr_data += delimiter;
  

  // Run Experiment Code
 
  // Starts experiment 1
  if (!exp1_complete && !servo1_extended && alt > servo1_start_alt) {
    extend_servo(1);
    exp1_start_time = millis();
    Serial.print("Started Experiment 1 at ");
    Serial.print(alt/ft_to_m);
    Serial.println("ft");
  }

  // Stops experiment 1
  if (!exp1_complete && servo1_extended && time_elapsed(exp1_start_time, timeout)) {
    return_servo(1);
    exp1_complete = true;
    Serial.print("Experiment 1 Complete at ");
    Serial.print(alt/ft_to_m);
    Serial.println("ft");
  }

  // Starts experiment 2
  if (!exp2_complete && !servo2_extended && alt > servo2_start_alt) {
    extend_servo(2);
    exp2_start_time = millis();
    Serial.print("Started Experiment 2 at ");
    Serial.print(alt/ft_to_m);
    Serial.println("ft");
  }

  // Stops experiment 2
  if (!exp2_complete && servo2_extended && time_elapsed(exp2_start_time, timeout)) {
    return_servo(2);
    exp2_complete = true;
    Serial.print("Experiment 2 Complete at ");
    Serial.print(alt/ft_to_m);
    Serial.println("ft");
  }
  
  curr_data += servo1_extended;
  curr_data += delimiter;
  curr_data += servo2_extended;
  // TODO: FIXME: no delimiter at end?
  
  write_to_sd("test.csv", curr_data);
  curr_data = "";
  
  delay(1500);
}

/*
   Experiment's Actuator Specific
*/
void setup_servos() {
  Serial.print("Initializiation Servos...");
  servo1.attach(servo1_pin);
  servo2.attach(servo2_pin);
  return_servo(1);
  return_servo(2);
  delay(20000);
  Serial.println("Servo initialization done");
}

void extend_servo(int servo_id) {
  if (servo_id == 1) {
      servo1.write(servo_max);
      servo1_extended = true;
  } else if (servo_id == 2) {
      servo2.write(servo_max);
      servo2_extended = true;
  }
}

void return_servo(int servo_id) {
  if (servo_id == 1) {
      servo1.write(servo_min);
      servo1_extended = false;
  } else if (servo_id == 2) {
      servo2.write(servo_min);
      servo2_extended = false;
  }
}

bool time_elapsed(unsigned long exp_start, unsigned long time_diff) {
  // Compares milliseconds since startup to milliseconds since startup for experiment start plus time_diff
  if (millis() > exp_start + time_diff) {
    Serial.println("Time elapsed");
    return true;
  }
  return false;
}

/*
   UV sensor Specific
*/
void setup_UV() {
  // Set up UV Sensor (SI1145)
  Serial.print("Initializing UV Sensor...");
  if (! uv_sensor.begin()) {
    Serial.println("Failed: Could not find Si1145 UV sensor");
    return;
  }
  Serial.println("UV Sensor initialization done.");
}
String get_uv_data() {
  String uv_data = "";
  uv_data += String(uv_sensor.readUV());
  uv_data += delimiter;
  uv_data += String(uv_sensor.readIR());
  uv_data += delimiter;
  uv_data += String(uv_sensor.readVisible());
  return uv_data;
}


/*
   RTC (Real Time Clock) Specific
*/
void setup_rtc() {
  // connect to RTC
  Wire.begin();
  RTC.adjust(DateTime(__DATE__, __TIME__));
  if (!RTC.begin()) Serial.println("RTC failed");
}
String get_rtc() {
  DateTime now = RTC.now();
  char buffer[25] = "";
  sprintf(buffer, "%04d/%02d/%02d, %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return buffer;
}

/*
   Thermo Couple Sensor Setup
*/
void setup_thermo() {
  // Thermo couple is setup when creating the object.
  Serial.println("Initializing Thermo Couple...");
  delay(500);  // wait for MAX thermo chip to stabilize
}
String get_thermo_data() {
  SPI.end();
  String thermo_data = "";
  thermo_data += thermocouple.readInternal();
  thermo_data += delimiter;
  double c = thermocouple.readCelsius();
  double f = thermocouple.readFarenheit();
  if (isnan(c)) {
    Serial.println("Something wrong with thermocouple!");
  } else {
    thermo_data += String(c);
  }
  thermo_data += delimiter;
  if (isnan(f)) {
    Serial.println("Something wrong with thermocouple!");
  } else {
    thermo_data += String(f);
  }
  return thermo_data;
}


/*
   SD Card Specific
*/
void setup_SD() {
  Serial.print("Initializing SD card...");
  if (!SD.begin(sd_card_pin)) {
    Serial.println("SD initialization failed!");
    return;
  }
  Serial.println("SD initialization done.");
}
void write_to_sd(String filename, String data) {
  // The following is the code to write to sd card
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  sd_card_file = SD.open(filename, FILE_WRITE);

  // if the file opened, write to it:
  if (sd_card_file) {
    Serial.print("Writing to file ...");
    sd_card_file.println(data);
    // close the file:
    sd_card_file.close();
    Serial.println("done writing to file");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening file");
  }
}

