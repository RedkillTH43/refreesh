// Import necessary modules
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

// Define component pins
#define CLOCK_INTERRUPT_PIN 2
#define ALARM_DELAY 5000
#define DEFAULT_CODE 0
#define ALARM_CODE 1
#define COUNTDOWN_CODE 2

// Declare component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 ds3231;
//SensirionI2cScd4x scd41;

bool alarmExists = false;

void setup() {
  // Initialize components
  initializeArduino();
  initializeDS3231();
  initializeLCD();

  delay(3000);  // Wait 3 seconds to simulate startup

  // Set the alarm (for testing purposes)
  setAlarm();
  //initializeSCD41();
}

void loop() {
  //measureEnvironment();
  renderLCD(DEFAULT_CODE);
  handleEvent();
}

/////////////////////////////////////
// Separator
/////////////////////////////////////

void renderLCD(int code) {
  switch (code) {
    case ALARM_CODE:
      alarm();
      break;

    case COUNTDOWN_CODE:
      countdown();
      break;

    case DEFAULT_CODE:
      measureTime();
      break;

    default:
      Serial.println("Invalid code.");
      break;
  }
}

void handleEvent() {
  if (alarmExists) {
    if (ds3231.alarmFired(1)) {
      renderLCD(ALARM_CODE);
    } else {
      renderLCD(COUNTDOWN_CODE);
    }
  }
  return;
}

// Measure and display time
void measureTime() {
  // Display time
  char date[10] = "hh:mm:ss";
  ds3231.now().toString(date);
  Serial.print("Time: ");
  Serial.print(date);

  if (alarmExists) {
    DateTime alarm1 = ds3231.getAlarm1();
    char alarm1Date[10] = "hh:mm:ss";
    alarm1.toString(alarm1Date);
    Serial.print(" Alarm: ");
    Serial.print(alarm1Date);
    return;
  }

  Serial.println(" ");
  lcd.clear();
  lcd.print("Time: ");
  lcd.print(date);
  delay(1000);
}

void onAlarm() {
  return;
}

void alarm() {
  unsigned long time = millis();
  do {
    lcd.clear();
    delay(250);
    lcd.print("Alarm ringing!");
    delay(250);
  } while ((millis() - time) < ALARM_DELAY);
  ds3231.clearAlarm(1);
  alarmExists = false;
}

void setAlarm() {
  // Set alarm 1
  if (!ds3231.setAlarm1(ds3231.now() + TimeSpan(10), DS3231_A1_Second)) {
    Serial.println("Error: alarm wasn't set!");
  } else {
    Serial.println("Alarm will happen in 10 seconds!");
    alarmExists = true;
  }
}

void countdown() {
  uint8_t alarmSecond = ds3231.getAlarm1().second();
  uint8_t currentSecond = ds3231.now().second();

  lcd.clear();
  lcd.print("Alarm in: ");
  lcd.print(alarmSecond - currentSecond);
  lcd.print("s");

  Serial.print(" Alarm in: ");
  Serial.print(alarmSecond - currentSecond);
  Serial.println("s");
  delay(1000);
}

// Prepare DS3231 on startup
void initializeDS3231() {
  // Test I2C connection
  if (!ds3231.begin()) {
    lcd.clear();
    lcd.print("Error: can't find DS3231.");
    return;
  }

  // Checks if stopped then reset time
  if (ds3231.lostPower()) {
    ds3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Disable unnecessary pin
  ds3231.disable32K();

  // Set interrupt pin
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);

  // Clear alarms
  ds3231.clearAlarm(1);
  ds3231.clearAlarm(2);
  ds3231.disableAlarm(2);

  // Disable SQW initially
  ds3231.writeSqwPinMode(DS3231_OFF);
  /*
  // Set alarm 1
  if (!ds3231.setAlarm1(ds3231.now() + TimeSpan(10), DS3231_A1_Second)) {
    Serial.println("Error: alarm wasn't set!");
  }
  else {
    Serial.println("Alarm will happen in 10 seconds!");
    alarmExists = true;
  }
  */
}

// Prepare LCD on startup
void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
}

// Prepare Arduino on startup
void initializeArduino() {
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }
  Wire.begin();
  pinMode(LED_BUILTIN, OUTPUT);
}

/*
void measureEnvironment() {
  bool dataReady = false;
  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0;

  delay(5000);

  success = scd41.getDataReadyStatus(dataReady);
  if (!success) {
      Serial.print("Error trying to execute getDataReadyStatus(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  while (!dataReady) {
      delay(100);
      success = scd41.getDataReadyStatus(dataReady);
      if (!success) {
          Serial.print("Error trying to execute getDataReadyStatus(): ");
          errorToString(success, errorMessage, sizeof errorMessage);
          Serial.println(errorMessage);
          return;
      }
  }

  success = scd41.readMeasurement(co2Concentration, temperature, relativeHumidity);
  if (!success) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }

  Serial.print("CO2 concentration [ppm]: ");
  Serial.print(co2Concentration);
  Serial.println();
  Serial.print("Temperature [°C]: ");
  Serial.print(temperature);
  Serial.println();
  Serial.print("Relative Humidity [RH]: ");
  Serial.print(relativeHumidity);
  Serial.println();
}


static char errorMessage[64];
static int16_t success;

void PrintUint64(uint64_t& value) {
    Serial.print("0x");
    Serial.print((uint32_t)(value >> 32), HEX);
    Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}


// Prepare SCD41 on startup
void initializeSCD41() {
  scd41.begin(Wire, SCD41_I2C_ADDR_62);

  uint64_t serialNumber = 0;
  delay(30);

  // Ensure sensor is in clean state
  success = scd41.wakeUp();
  if (!success) {
      Serial.print("Error trying to execute wakeUp(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
  }
  
  success = scd41.stopPeriodicMeasurement();
  if (!success) {
      Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
  }

  success = scd41.reinit();
  if (!success) {
      Serial.print("Error trying to execute reinit(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
  }

  // Read out information about the sensor
  success = scd41.getSerialNumber(serialNumber);
  if (!success) {
      Serial.print("Error trying to execute getSerialNumber(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }

  Serial.print("serial number: ");
  PrintUint64(serialNumber);
  Serial.println();

  success = scd41.startPeriodicMeasurement();
  if (!success) {
      Serial.print("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(success, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
}
*/