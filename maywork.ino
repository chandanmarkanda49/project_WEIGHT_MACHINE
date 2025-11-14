// #include <LiquidCrystal_I2C.h>
// #include "HX711.h"
#include <LiquidCrystal.h>
#include <HX711_ADC.h>
#include <EEPROM.h>
// define pins connected to HX711
const int HX711_dout = 6;  // DOUT pin of HX711 connected to pin D6
const int HX711_sck = 7;   // SCK pin of HX711 connected to pin D7

// create HX711 object with data and clock pins
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;  // EEPROM address (starting point) for calibration value
unsigned long t = 0;                // variable for storing time (used for print intervals)
uint8_t refreshCounter = 0;


const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal_I2C lcd(0x27, 16, 2);
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
int push1 = 10;  //motor on

int push3 = 13;  //teir
int push4 = 9;   //setweight
int relay = 8;
int weight = 50;
int reading = 0;
uint8_t state = 0;
float calVal = 396.24;
void printToLcd(String data, uint8_t dataLength, uint8_t column, uint8_t row) {
  for (int i = column; i <= (dataLength + column); i++) {
    lcd.setCursor(i, row);
    lcd.print(" ");
  }
  lcd.setCursor(column, row);
  lcd.print(data);
}

void calibrate() {
  // wdt_reset();
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell an a level stable surface.");
  // Serial.println("Send 't' from serial monitor to set the tare offset.");

  // LoadCell.update();
  LoadCell.tareNoDelay();
  delay(500);

  boolean _resume = false;
  Serial.println("state 1");
  while (_resume == false) {
    Serial.println("state 2");
    LoadCell.update();
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      _resume = true;
    }
  }

  float lastWeight = LoadCell.getData();

  Serial.println("Now, place your known mass on the loadcell.");
  Serial.println("Then send the weight of phone 2 A (i.e. 100.0) from serial monitor.");
  printToLcd("Place 100g wgt", 16, 0, 1);

  while (1) {
    if (!digitalRead(push4))
      break;
    delay(50);
    // wdt_reset();
  }

  printToLcd("Please wait...", 16, 0, 1);


  float known_mass = 100;

  LoadCell.refreshDataSet();                                           //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);  //get the new calibration value

  Serial.print("New calibration value has been set to: ");
  Serial.print(newCalibrationValue);
  Serial.println(", use this as calibration value (calFactor) in your project sketch.");
  Serial.print("Save this value to EEPROM adress ");
  Serial.print(calVal_eepromAdress);

  EEPROM.put(calVal_eepromAdress, newCalibrationValue);
  EEPROM.get(calVal_eepromAdress, newCalibrationValue);
  Serial.print("Value ");
  Serial.print(newCalibrationValue);
  Serial.print(" saved to EEPROM address: ");
  Serial.println(calVal_eepromAdress);

  Serial.println("End calibration");
  Serial.println("***");
}

void changeSavedCalFactor() {                           //for testing purpose only {
  float oldCalibrationValue = LoadCell.getCalFactor();  // get current factor
  boolean _resume = false;
  Serial.println("*");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);  // print existing value
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");

  float newCalibrationValue;
  while (_resume == false) {  // wait until user enters valid new value
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);  // apply new calibration
        _resume = true;
      }
    }
  }

  _resume = false;
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");  // ask user if they want to save

  while (_resume == false) {  // wait for user decision
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);  // save to EEPROM
#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);  // verify saved value
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("End change calibration value");
  Serial.println("*");
}

void refresh(){
  printToLcd(String(reading), 11, 0, 0);
  printToLcd("", 16, 0, 1);
  printToLcd(String(weight), 4, 12, 0);
}

void setup() {
  pinMode(push1, INPUT_PULLUP);
  pinMode(push3, INPUT_PULLUP);
  pinMode(push4, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  Serial.begin(9600);
  delay(10);
  Serial.println("  started");

  // lcd.init();
  // lcd.backlight();
  lcd.begin(16, 2);
  lcd.clear();

  Serial.print("\nWelcome\n");

  lcd.setCursor(3, 0);
  lcd.print("Welcome");
  Serial.println("50 weight limit");

  EEPROM.get(calVal_eepromAdress, calVal);
  Serial.println(calVal);

  // check if the value is valid (not NaN or too small/large)
  if (isnan(calVal) || calVal < 0.1 || calVal > 10000) {
    Serial.println("Invalid EEPROM value! Using default calibration.");
    calVal = 396.24;                          // fallback
    EEPROM.put(calVal_eepromAdress, calVal);  // save correct value
  }
  LoadCell.setCalFactor(calVal);

  LoadCell.begin();  // initialize HX711 ADC module
  // LoadCell.setReverseOutput();       // optional: invert output if weight reads negative

  unsigned long stabilizingtime = 5000;  // time (ms) for load cell to stabilize (10 seconds)
  boolean _tare = true;                  // perform tare (zero) operation on start

  LoadCell.start(stabilizingtime, _tare);  // start HX711 with stabilization and tare

  // check for timeout or bad connection
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");


    while (1)
      lcd.setCursor(1, 0);
    lcd.println("LOAD CELL ERR");
    ;  // infinite loop (halt program) if HX711 not responding


  } else {
    float calVal = 396.24;                    // default calibration value
    EEPROM.get(calVal_eepromAdress, calVal);  // read calibration factor from EEPROM
    LoadCell.setCalFactor(calVal);            // set calibration factor in HX711 object
    // LoadCell.setCalFactor(1.0);      // can also manually set initial factor
    Serial.println("Startup is complete");  // print success message
  }

  while (!LoadCell.update())
    ;  // wait until first valid HX711 reading
  // calibrate();  // start calibration procedure automatically

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(reading);
  lcd.setCursor(12, 0);
  lcd.print(weight);
}

void loop() {

  if (LoadCell.update()) {
    reading = LoadCell.getData();  // calibrated units (grams if scale.set_scale() sahi hai)
    printToLcd(String(reading), 11, 0, 0);

    refreshCounter++;
    if(refreshCounter >= 5){
      refresh();
      refreshCounter = 0;
    }
  }

  // --- handle user commands from serial monitor ---
  if (Serial.available() > 0) {                      // if user typed something
    char inByte = Serial.read();                     // read the first character
    if (inByte == 't') LoadCell.tareNoDelay();       // start tare (zero) process
    else if (inByte == 'r') calibrate();             // restart calibration procedure
    else if (inByte == 'c') changeSavedCalFactor();  // edit calibration value manually
  }

  // print message when tare finishes
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  if (digitalRead(push3) == 0) {  //Tare button
    unsigned long lastTime = millis();
    while (!digitalRead(push3)) {
      if (millis() - lastTime >= 2000) {
        Serial.print("Tared");
        printToLcd("SCALE TARING..", 16, 0, 1);
        LoadCell.tareNoDelay();
        delay(700);
        printToLcd("", 16, 0, 1);
        return;
        delay(50);
      }
    }

    weight++;
    Serial.print("Weight: ");
    Serial.println(weight);
    printToLcd(String(weight), 3, 12, 0);
    delay(200);
  }

  if (digitalRead(push4) == 0) {  //Weight selector
    unsigned long lastTime = millis();
    while (!digitalRead(push4)) {
      if (millis() - lastTime >= 2000) {
        printToLcd("CALIBRATING...", 16, 0, 1);
        while (!digitalRead(push4))
          ;
        calibrate();  // ya fir calibrayion wala code
        printToLcd("CALIBRATION DONE", 16, 0, 1);
        delay(700);
        printToLcd("", 16, 0, 1);
        return;
        delay(50);
      }
    }


    if (state == 5) {
      state = 0;
    } else {
      state++;
    }

    switch (state) {
      case 0:
        weight = 50;
        break;
      case 1:
        weight = 100;
        break;
      case 2:
        weight = 200;
        break;
      case 3:
        weight = 250;
        break;
      case 4:
        weight = 500;
        break;
      default:
        weight = 50;
    }
    printToLcd(String(weight), 4, 12, 0);
    Serial.print(weight);
    delay(200);
  }

  if (!digitalRead(push1)) {
    printToLcd("BOX DETECTED", 16, 0, 1);
    unsigned long lastTime = millis();

    while (reading >= -3 && reading <= 5 && millis() - lastTime <= 2000) {
      reading = LoadCell.getData();
    }

    if (millis() - lastTime < 2000) {
      Serial.println("Over weight or Under weight error");
      printToLcd("ER OVER/UNDER", 16, 0, 1);
      delay(1000);
      printToLcd("", 16, 0, 1);
      return;
    }

    digitalWrite(relay, HIGH);
    printToLcd("MOTOR ON", 16, 0, 1);

    while (reading <= weight) {
      if (LoadCell.update()) {
        reading = LoadCell.getData();
        Serial.println(reading);
        printToLcd(String(reading), 11, 0, 0);
      }

      if (reading < -3) {
        digitalWrite(relay, LOW);
        Serial.println("Box is removed!");
        printToLcd("ER BOX REMOVED", 16, 0, 1);
        delay(1000);
        printToLcd("", 16, 0, 1);
        return;
      }
    }

    digitalWrite(relay, LOW);
    printToLcd("DONE", 16, 0, 1);
    delay(1000);
    lcd.begin(16, 2);
    printToLcd("", 16, 0, 1);
    printToLcd(String(weight), 4, 12, 0);
  }
}
