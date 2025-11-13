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


const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal_I2C lcd(0x27, 16, 2);
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
int push1 = 10;  //motor on

int push3 = 13;  //teir
int push4 = 9;   //setweight
int relay = 8;
int weight = 50;
float reading = 0;
uint8_t state = 0;

void calibrate() {
  Serial.println("*");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell on a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  LoadCell.update();                       // keep updating HX711 data
  LoadCell.tareNoDelay();                  // start tare
  if (LoadCell.getTareStatus() == true) {  // if tare done
    Serial.println("Tare complete");
  }

  Serial.println("Now, place your known mass on the loadcell.");
  Serial.println("Place the weight of 65w charger (i.e. 100.0) from serial monitor.");

  float known_mass = 100;  // variable to store user-entered known weight // change this value to for user


  LoadCell.refreshDataSet();                                           // refresh HX711 readings for accurate result
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);  // calculate calibration factor

  Serial.print("New calibration value has been set to: ");
  Serial.print(newCalibrationValue);  // print calculated calibration factor
  Serial.println(", use this as calibration value (calFactor) in your project sketch.");
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);


#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512);  // start EEPROM (for ESP boards)
#endif
  EEPROM.put(calVal_eepromAdress, newCalibrationValue);  // save calibration factor
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();  // finish write (for ESP)
#endif
  EEPROM.get(calVal_eepromAdress, newCalibrationValue);  // verify by reading back
  Serial.print("Value ");
  Serial.print(newCalibrationValue);
  Serial.print(" saved to EEPROM address: ");
  Serial.println(calVal_eepromAdress);
  // exit loop
}



void printToLcd(String data, uint8_t dataLength, uint8_t column, uint8_t row) {
  for (int i = column; i <= (dataLength + column); i++) {
    lcd.setCursor(i, row);
    lcd.print(" ");
  }
  lcd.setCursor(column, row);
  lcd.print(data);
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
  LoadCell.begin();  // initialize HX711 ADC module
  // LoadCell.setReverseOutput();       // optional: invert output if weight reads negative

  unsigned long stabilizingtime = 5000;  // time (ms) for load cell to stabilize (10 seconds)
  boolean _tare = true;                  // perform tare (zero) operation on start

  LoadCell.start(stabilizingtime, _tare);  // start HX711 with stabilization and tare

  // check for timeout or bad connection
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1)
      ;  // infinite loop (halt program) if HX711 not responding
  } else {
    float calVal = 1.0;                       // default calibration value
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


  // if (Serial.available()) {
  //   String data = Serial.readStringUntil('\n');
  //   data.trim();

  // }

  // static boolean newDataReady = 0;    // flag for new data availability
  // const int serialPrintInterval = 0;  // control speed of serial printing (0 = no delay)

  if (LoadCell.update()) {
    reading = LoadCell.getData();  // calibrated units (grams if scale.set_scale() sahi hai)
    printToLcd(String(reading), 3, 0, 0);
  }

  // if (newDataReady) {                          // if new data available
  //   if (millis() > t + serialPrintInterval) {  // check if enough time passed
  //     float i = LoadCell.getData();            // read filtered load cell data
  //     Serial.print("Load_cell output val: ");
  //     Serial.println(i);  // print current load cell reading
  //     newDataReady = 0;   // reset flag
  //     t = millis();       // update timestamp
  //   }
  // }

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
        calibrate();  // ya fir calibrayion wala code
        printToLcd("CALIBRATING", 16, 0, 1);
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
    printToLcd(String(weight), 3, 12, 0);
    Serial.print(weight);
    delay(200);
  }

  // if (digitalRead((push4) == 0 && millis() - lastTime >= 2000)) {  //Weight incremental
  //   weight++;
  //   Serial.print("Weight: ");
  //   Serial.println(weight);
  //   printToLcd(String(weight), 3, 12, 0);
  //   delay(250);
  // }

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

    while (reading <= weight) {
      LoadCell.update();
      reading = LoadCell.getData();
      Serial.println(reading);
      printToLcd(String(reading), 4, 1, 0);

      digitalWrite(relay, HIGH);
      lcd.begin(16, 2);
       printToLcd(String(reading), 3, 0, 0);
      printToLcd("MOTOR ON", 16, 0, 1);

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
    lcd.begin(16, 2) ;
    printToLcd(String(weight), 3, 12, 0);
    printToLcd("", 16, 0, 1);
  }
}