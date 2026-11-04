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
#define SLIDE_SWITCH_PIN 3
#define ROCKER_SWITCH_PIN 4
#define GREEN_LED_PIN 5
#define YELLOW_LED_PIN 6
#define RED_LED_PIN 7
#define RELAY_PIN 9
#define CHIP_SELECT_PIN 10

// Define global macros
#define FILE_NAME "data.csv"
#define ALARM_DELAY 4000
#define ALARM_CODE 1
#define SETUP_CODE 2

// Declare component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 ds3231;
SensirionI2cScd4x scd41;

// Initialize flags
volatile byte isAuto;

/////////////////////////////////////
// Main Program Loop
/////////////////////////////////////

void setup()
{
  // Initialize components
  initializeArduino();
  initializeComponents();

  // Wait 3 seconds to simulate startup
  delay(3000);

  // Set the alarm (for testing purposes)
  setAlarm(1);
}

void loop()
{
  // Initialize necessary variables
  bool dataReady = false;
  uint16_t co2Concentration = 0;
  float temperature = 0.0f;
  float relativeHumidity = 0.0f;
  char time[10] = "hh:mm:ss";

  // Store current time
  ds3231.now().toString(time);

  // Set data ready status
  if (scd41.getDataReadyStatus(dataReady) != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: getDataReadyStatus failed."));
  }

  // Loop while data is not ready
  while (!dataReady)
  {
    delay(100);
    if (scd41.getDataReadyStatus(dataReady) != 0)
    {
      // Display error messages
      displayError(F("SCD41 failed"), F("Error: getDataReadyStatus failed."));
    }
  }

  // Read and set measurement values
  if (scd41.readMeasurement(co2Concentration, temperature, relativeHumidity) != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: readMeasurement() failed."));
  }

  // Handle measured data
  handleEvent(temperature, relativeHumidity, co2Concentration, time);

  // Display measured data
  displayData(temperature, relativeHumidity, co2Concentration, time);
}

/////////////////////////////////////
// Handler Function
/////////////////////////////////////

// Handles events
void handleEvent(float temperature, float relativeHumidity, uint16_t co2Concentration, char time[10])
{
  /*
  // Check if alarm 1 is ringing
  if (ds3231.alarmFired(1))
  {
    // Store current time in milliseconds
    unsigned long time = millis();

    // Loops until ALARM_DELAY is reached in milliseconds
    do
    {
      lcd.clear();
      delay(250);
      lcd.print(F("Alarm ringing!"));
      delay(250);
    } while ((millis() - time) < ALARM_DELAY);

    // Clear existing alarm 1
    ds3231.clearAlarm(1);
    setAlarm(1);
  }
    */

  // Check if alarm 1 is ringing
  if (ds3231.alarmFired(1))
  {
    // Declare necessary variables
    char tempText[7], humidityText[7], co2Text[7];

    // Convert values to strings
    dtostrf(temperature, 3, 2, tempText);
    dtostrf(relativeHumidity, 3, 2, humidityText);
    itoa(co2Concentration, co2Text, 10);

    // Write to file in sd card
    writeToCard(tempText, humidityText, co2Text, time);
    readFromCard();

    // Clear existing alarm 1
    ds3231.clearAlarm(1);
  }

  if (isAuto == LOW)
  {
    Serial.println(F("Now in Auto mode..."));
    Serial.println(digitalRead(SLIDE_SWITCH_PIN));

    // Turn on yellow LED for Auto indication
    digitalWrite(YELLOW_LED_PIN, HIGH);

    // Check if temperature, humidity, and co2 are above normal level
    if (temperature > 26 && relativeHumidity > 60 && co2Concentration > 1000)
    {
      // Turn relay, yellow/green ON with red OFF
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(YELLOW_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
    }
    else
    {
      // Turn relay, yellow/red ON with green OFF
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
    }
  }
  else
  {
    Serial.println(F("Now in Manual mode..."));
    Serial.println(digitalRead(SLIDE_SWITCH_PIN));

    bool isOn = digitalRead(ROCKER_SWITCH_PIN);

    // Turn off yellow LED for Manual indication
    digitalWrite(YELLOW_LED_PIN, LOW);

    // Check if rocker switch is on and turn relay on
    if (isOn == LOW)
    {
      Serial.println(F("Switch ON..."));
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
    }

    // Check if rocker switch is off and turn relay off
    else
    {
      Serial.println(F("Switch OFF..."));
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
    }
  }
}

/////////////////////////////////////
// Display Functions
/////////////////////////////////////

// Display sensory data
void displayData(float temperature, float relativeHumidity, uint16_t co2Concentration, char time[10])
{
  // Display time to serial
  Serial.print(F("Time: "));
  Serial.println(time);

  // Display temperature to serial
  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("C"));

  // Display humidity to serial
  Serial.print(F("Humidity: "));
  Serial.print(relativeHumidity);
  Serial.println(F("%"));

  // Display CO2 to serial
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

  // Wait 3 seconds
  delay(5000);

  // Display CO2 concentration to LCD
  lcd.clear();
  lcd.print(F("CO2: "));
  lcd.print(co2Concentration);
  lcd.print(F("PPM"));

  // Display time to LCD
  lcd.setCursor(0, 1);
  lcd.print(F("Time: "));
  lcd.print(time);

  // Wait 3 seconds
  delay(5000);
}

// Display error messages
void displayError(String lcdMessage, String serialMessage)
{
  Serial.println(serialMessage);
  lcd.clear();
  lcd.print(lcdMessage);
  while (1)
    ;
}

// Display messages for setup
void displayMessages(String row1Message, String row2Message, int milliseconds)
{
  lcd.clear();
  lcd.print(row1Message);
  lcd.setCursor(0, 1);
  lcd.print(row2Message);
  delay(milliseconds);
}

/////////////////////////////////////
// Initialize Functions
/////////////////////////////////////

// Initialize all components
void initializeComponents()
{
  initializeLCD();
  initializeSwitches();
  initializeRelay();
  initializeLED();
  initializeSCD41();
  initializeDS3231();
  initializeSDCard();
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

// Prepare DS3231 on startup
void initializeDS3231()
{
  Serial.println(F("Initializing DS3231..."));

  // Test I2C connection
  if (!ds3231.begin())
  {
    displayError(F("DS3231 failed"), F("Error: DS3231 failed to initialize."));
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

  // Set alarm 2
  if (!ds3231.setAlarm2(ds3231.now() + TimeSpan(3600), DS3231_A2_Hour))
  {
    // Display error messages
    displayError(F("DS3231 failed"), F("Error: alarm 2 is not set."));
  }
  */

  displayMessages(F("DS3231 is"), F("working..."), 2000);
}

// Prepare LCD on startup
void initializeLCD()
{
  Serial.println(F("Initializing LCD1602..."));

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  displayMessages(F("Testing"), F("components..."), 5000);
  displayMessages(F("Arduino is"), F("working..."), 2000);
  displayMessages(F("LCD1602 is"), F("working..."), 2000);
}

// Prepare LEDs on startup
void initializeLED()
{
  Serial.println(F("Initializing LEDs..."));

  // Configure pin modes
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Set all LEDs on initially
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);

  displayMessages(F("LEDs are"), F("working..."), 2000);
}

// Prepare relay on startup
void initializeRelay()
{
  Serial.println(F("Initializing relay..."));

  // Configure pin mode
  pinMode(RELAY_PIN, OUTPUT);

  // Set relay HIGH initially
  digitalWrite(RELAY_PIN, HIGH);

  displayMessages(F("Relay is"), F("working..."), 2000);
}

// Prepare SCD41 on startup
void initializeSCD41()
{
  Serial.println(F("Initializing SCD41..."));

  // Declare necessary variables
  uint64_t serialNumber = 0;
  uint16_t testResult = 0;

  // Connect to I2C
  scd41.begin(Wire, SCD41_I2C_ADDR_62);

  delay(30); // Wait for the sensor

  // Wake up the sensor
  if (scd41.wakeUp() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: wakeUp() failed."));
  }

  // Stop measuring the environment
  if (scd41.stopPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement() failed."));
  }

  // Reinitialize SCD41
  if (scd41.reinit() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: reinit() failed."));
  }
  /*
    // Test the component's functionality
    if (scd41.performSelfTest(testResult) != 0)
    {
      // Display error messages
      displayError(F("SCD41 failed"), F("Error: performSelfTest() failed."));
    }

    // Check result of self-test
    if (testResult != 0)
    {
      // Display error messages
      displayError(F("SCD 41 failed"), F("Error: self-test failed."));
    }
      */

  // Read out information about the sensor
  if (scd41.getSerialNumber(serialNumber) != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: getSerialNumber() failed."));
  }

  // Print serial number
  Serial.print("Serial Number: ");
  printUint64(serialNumber);

  // Start measuring the environment
  if (scd41.startPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startPeriodicMeasurement() failed."));
  }

  displayMessages(F("SCD41 is"), F("working..."), 2000);
}

// Prepare SD Card on startup
void initializeSDCard()
{
  Serial.println(F("Initializing SD card..."));

  // Check if there is no connection with sd card
  delay(2000);
  if (!SD.begin(CHIP_SELECT_PIN))
  {
    // Display error messages
    displayError(F("SD card failed"), F("Error: SD card failed to initialize."));
  }

  displayMessages(F("SD card is"), F("working..."), 0);
}

// Prepare switches on startup
void initializeSwitches()
{
  pinMode(SLIDE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(ROCKER_SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SLIDE_SWITCH_PIN), toggleAuto, CHANGE);
  isAuto = digitalRead(SLIDE_SWITCH_PIN);
}

/////////////////////////////////////
// Other Functions
/////////////////////////////////////

// Helper function for alarm
void onAlarm()
{
  return;
}

// Helper function to print serial number
void printUint64(uint64_t &value)
{
  Serial.print("0x");
  Serial.print((uint32_t)(value >> 32), HEX);
  Serial.println((uint32_t)(value & 0xFFFFFFFF), HEX);
}

// Read sd card contents
void readFromCard()
{
  // Initialize File object
  File dataFile;

  // Open file for reading
  dataFile = SD.open(FILE_NAME, FILE_READ);

  // Check if file does not exist
  if (!dataFile)
  {
    // Print error message
    displayError(F("SD card failed"), F("Error: data.csv failed to open while trying to read."));
  }

  // Loop while data exists in file
  while (dataFile.available())
  {
    // Read file and print to Serial
    Serial.write(dataFile.read());
  }
  dataFile.close(); // Close file
}

void setAlarm(int alarmNum)
{
  switch (alarmNum)
  {
  case 1:
    // Set alarm 1
    if (!ds3231.setAlarm1(ds3231.now() + TimeSpan(30), DS3231_A1_Second))
    {
      // Display error messages
      displayError(F("DS3231 failed"), F("Error: alarm 1 is not set."));
    }
    break;

  default:
    displayError(F("DS3231 failed"), F("Error: invalid alarm number."));
    break;
  }
}

// Toggle Auto to Manual and vice versa
void toggleAuto()
{
  isAuto = digitalRead(SLIDE_SWITCH_PIN);
  isAuto = !isAuto;
}

// Write data to sd card
void writeToCard(char tempText[7], char humidityText[7], char co2Text[7], char time[10])
{
  // Initialize File object
  File dataFile;

  // Open file for writing
  dataFile = SD.open(FILE_NAME, FILE_WRITE);

  // Check if file does not exist
  if (!dataFile)
  {
    // Print error message
    displayError(F("SD card failed"), F("Error: data.csv failed to open while trying to write."));
  }

  Serial.println(F("Writing to file..."));

  // Write time to file
  dataFile.print(time);
  dataFile.print(F(", "));

  // Write temperature to file
  dataFile.print(tempText);
  dataFile.print(F(", "));

  // Write humidity to file
  dataFile.print(humidityText);
  dataFile.print(F(", "));

  // Write co2 to file
  dataFile.println(co2Text);
  dataFile.close();

  Serial.println(F("Done."));
}