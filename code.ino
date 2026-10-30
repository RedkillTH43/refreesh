// Import necessary modules
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SD.h>
#include <SensirionI2cScd4x.h>
#include <SPI.h>
#include <Wire.h>

// Define component pins
#define CLOCK_INTERRUPT_PIN 2
#define RED_LED_PIN 7
#define YELLOW_LED_PIN 6
#define GREEN_LED_PIN 5
#define RELAY_PIN 9
#define CHIP_SELECT_PIN 10
#define ALARM_DELAY 4000
#define DEFAULT_CODE 0
#define ALARM_CODE 1
#define SETUP_CODE 2

// Declare component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 ds3231;
Sd2Card card;
SdVolume volume;
SdFile root;
SensirionI2cScd4x scd41;

// Initialize global variables
bool alarmExists = false;

/////////////////////////////////////
// Main Program Loop
/////////////////////////////////////

void setup()
{
  // Initialize components
  initializeArduino();
  initializeLCD();
  initializeRelay(); 
  initializeLED();
  initializeSCD41();
  initializeDS3231();
  initializeSDCard();

  // Display success messages to LCD
  renderLCD(SETUP_CODE);

  delay(3000); // Wait 3 seconds to simulate startup

  // Set the alarm (for testing purposes)
  setAlarm();
}

void loop()
{
  update();
  // renderLCD(DEFAULT_CODE);
}

/////////////////////////////////////
// Program Functions
/////////////////////////////////////

// Handles events
void handleEvent(float temperature, float relativeHumidity, uint16_t co2Concentration, char date[])
{
  // Checks if there is an alarm
  if (alarmExists)
  {
    // Check if alarm is ringing
    if (ds3231.alarmFired(1))
    {
      renderLCD(ALARM_CODE);
    }
  }
  
  if (temperature > 26 && relativeHumidity > 60 && co2Concentration > 1000) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(YELLOW_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  }
  else
  {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
  }
}

// Display data to LCD
void renderLCD(int code)
{
  // Call render function according to code
  switch (code)
  {
  case ALARM_CODE:
    displayAlarm();
    break;

  case SETUP_CODE:
    displaySetup();
    break;

  case DEFAULT_CODE:
    // displayData();
    break;

  default:
    Serial.println(F("Invalid code."));
    break;
  }
}

// Updates data
void update()
{
  lcd.clear();
  lcd.print(F("Measuring..."));
  measureEnvironment();
}

// Display setup messages
void displaySetup()
{
  displayMessages(F("Testing"), F("components..."), 5000);
  displayMessages(F("Arduino is"), F("working..."), 2000);
  displayMessages(F("LCD1602 is"), F("working..."), 2000);
  displayMessages(F("Relay is"), F("working..."), 2000);
  displayMessages(F("LEDs are"), F("working..."), 2000);
  displayMessages(F("SCD41 is"), F("working..."), 2000);
  displayMessages(F("DS3231 is"), F("working..."), 2000);
  displayMessages(F("SD card is"), F("working..."), 2000);
}

// Display sensory data
void displayData(float temperature, float relativeHumidity, uint16_t co2Concentration, char date[])
{
  Serial.print(F("Time: "));
  Serial.println(date);

  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("C"));

  Serial.print(F("Humidity: "));
  Serial.print(relativeHumidity);
  Serial.println(F("%"));

  Serial.print(F("CO2 Level: "));
  Serial.print(co2Concentration);
  Serial.println(F("PPM"));

  // Display temperature to LCD
  lcd.clear();
  lcd.print(F("Temp: "));
  lcd.print(temperature);
  lcd.print(char(223));
  lcd.print(F("C"));

  // Display humidity to LCD
  lcd.setCursor(0, 1);
  lcd.print(F("Humidity: "));
  lcd.print(relativeHumidity);
  lcd.print(F("%"));

  delay(3000);

  // Display CO2 concentration to LCD
  lcd.clear();
  lcd.print(F("CO2: "));
  lcd.print(co2Concentration);
  lcd.print(F("PPM"));

  // Display time to LCD
  lcd.setCursor(0, 1);
  lcd.print(F("Time: "));
  lcd.print(date);

  delay(3000);
}

void displayMessages(String row1Message, String row2Message, int milliseconds)
{
  lcd.clear();
  lcd.print(row1Message);
  lcd.setCursor(0, 1);
  lcd.print(row2Message);
  delay(milliseconds);
}

// Helper function for alarm
void onAlarm()
{
  return;
}

void PrintUint64(uint64_t &value)
{
  Serial.print("0x");
  Serial.print((uint32_t)(value >> 32), HEX);
  Serial.println((uint32_t)(value & 0xFFFFFFFF), HEX);
}

// Displays alarm ringing
void displayAlarm()
{
  unsigned long time = millis(); // Store current time in milliseconds
  // Loops until ALARM_DELAY is reached in milliseconds
  do
  {
    lcd.clear();
    delay(250);
    lcd.print(F("Alarm ringing!"));
    delay(250);
  } while ((millis() - time) < ALARM_DELAY);
  ds3231.clearAlarm(1);
  alarmExists = false; // Sets alarm as false
}

void setAlarm()
{
  // Set alarm 1
  if (!ds3231.setAlarm1(ds3231.now() + TimeSpan(30), DS3231_A1_Second))
  {
    Serial.println(F("Error: alarm wasn't set!"));
  }
  else
  {
    Serial.println(F("Alarm will happen in 10 seconds!"));
    alarmExists = true; // Sets alarm as true
  }
}

// Prepare SD Card on startup
void initializeSDCard()
{
  Serial.println(F("Initializing SD card..."));

  if (!card.init(SPI_HALF_SPEED, CHIP_SELECT_PIN))
  {
    Serial.println(F("Error: SD Card failed to initialize."));
    lcd.clear();
    lcd.print(F("SD Card failed"));
    while (1)
      ;
  }
  else
  {
    Serial.println(F("Wiring is correct and a card is present."));
  }
}

// Prepare DS3231 on startup
void initializeDS3231()
{
  Serial.println(F("Initializing DS3231..."));

  // Test I2C connection
  if (!ds3231.begin())
  {
    lcd.clear();
    lcd.print(F("DS3231 failed"));
    Serial.println(F("Error: DS3231 failed to initialize."));
    while (1)
      ;
  }

  // Checks if stopped then reset time
  if (ds3231.lostPower())
  {
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
void initializeLCD()
{
  Serial.println(F("Initializing LCD1602..."));
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
}

// Prepare Arduino on startup
void initializeArduino()
{
  Serial.begin(9600);
  while (!Serial)
  {
    delay(100);
  }
  Serial.println(F("Initializing Arduino..."));
  Wire.begin();
}

// Prepare SCD41 on startup
void initializeSCD41()
{
  Serial.println(F("Initializing SCD41..."));

  // Connect to I2C
  scd41.begin(Wire, SCD41_I2C_ADDR_62);
  uint64_t serialNumber = 0;

  delay(30); // Wait for the sensor

  // Ensure sensor is in clean state
  if (scd41.wakeUp() != 0)
  {
    Serial.print("Error trying to execute wakeUp(): ");
    while (1)
      ;
  }

  if (scd41.stopPeriodicMeasurement() != 0)
  {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    while (1)
      ;
  }

  if (scd41.reinit() != 0)
  {
    Serial.print("Error trying to execute reinit(): ");
    while (1)
      ;
  }

  // Read out information about the sensor
  if (scd41.getSerialNumber(serialNumber) != 0)
  {
    Serial.print("Error trying to execute getSerialNumber(): ");
    while (1)
      ;
  }
  Serial.print("Serial Number: ");
  PrintUint64(serialNumber);

  if (scd41.startPeriodicMeasurement() != 0)
  {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    while (1)
      ;
  }
}

void measureEnvironment()
{
  bool dataReady = false;
  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0;
  char date[10] = "hh:mm:ss";

  delay(5000);

  ds3231.now().toString(date);

  if (scd41.getDataReadyStatus(dataReady) != 0)
  {
    Serial.print("Error trying to execute getDataReadyStatus(): ");
    while (1)
      ;
  }

  while (!dataReady)
  {
    delay(100);
    if (scd41.getDataReadyStatus(dataReady) != 0)
    {
      Serial.print("Error trying to execute getDataReadyStatus(): ");
      while (1)
        ;
    }
  }

  if (scd41.readMeasurement(co2Concentration, temperature, relativeHumidity) != 0)
  {
    Serial.print("Error trying to execute readMeasurement(): ");
    while (1)
      ;
  }

  displayData(temperature, relativeHumidity, co2Concentration, date);
  handleEvent(temperature, relativeHumidity, co2Concentration, date);
}

void initializeRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
}

void initializeLED() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
}
