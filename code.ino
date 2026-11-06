// Import necessary modules
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <LowPower.h>
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

// Declare component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 ds3231;
SensirionI2cScd4x scd41;

// Initialize flags
volatile byte isAuto;
unsigned long previousMillis = 0;
unsigned long previousMeasurement = 0;
bool hasDisplay = false;

/////////////////////////////////////
// Main Program Loop
/////////////////////////////////////

void setup()
{
  // Initialize Arduino and components
  initializeArduino();
  initializeComponents();

  // Wait 3 seconds to simulate startup
  delay(3000);
}

void loop()
{
  // Initialize necessary variables
  bool dataReady = false;
  bool isActive = ds3231.now().hour() >= 6 && ds3231.now().hour() <= 18;
  uint16_t co2Concentration = 0;
  int measureInterval = 5000;
  unsigned long currentMeasurement = millis();
  float temperature = 0.0f;
  float relativeHumidity = 0.0f;
  char time[10] = "hh:mm:ss";

  // Check if current time is not between 6AM and 6PM
  if (!isActive)
  {
    // Put Arduino and components to sleep
    goToSleep();

    // Reset all components after waking up
    resetComponents();
  }

  // Store current time
  ds3231.now().toString(time);

  // Check if data is ready to be measured
  if ((currentMeasurement - previousMeasurement) >= measureInterval)
  {
    previousMeasurement = currentMeasurement;

    // Set data ready status
    if (scd41.getDataReadyStatus(dataReady) != 0)
    {
      // Display error messages
      displayError(F("SCD41 failed"), F("Error: getDataReadyStatus failed."));
    }

    if (dataReady)
    {
      // Read and set measurement values
      if (scd41.readMeasurement(co2Concentration, temperature, relativeHumidity) != 0)
      {
        // Display error messages
        displayError(F("SCD41 failed"), F("Error: readMeasurement() failed."));
      }
    }
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
  // Check if alarm 1 for 12-hour interval of sleep is ringing
  if (ds3231.alarmFired(1))
  {
    // Clear and disable this alarm
    ds3231.clearAlarm(1);
    ds3231.disableAlarm(1);

    // Set a new alarm for 1-hour interval of writing to SD card
    setAlarm(2, ds3231.now() + TimeSpan(0, 1, 0, 0), 'h');
  }

  // Check if alarm 2 for 1-hour interval writing to SD card is ringing
  if (ds3231.alarmFired(2))
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

    // Clear this alarm
    ds3231.clearAlarm(2);

    // Set another alarm for 1-hour interval
    setAlarm(2, ds3231.now() + TimeSpan(0, 1, 0, 0), 'h');
  }

  // Check if in Auto Mode
  if (isAuto == LOW)
  {
    // Turn on yellow LED for Auto indication
    digitalWrite(YELLOW_LED_PIN, HIGH);

    // Check if temperature, humidity, and co2 are above normal level
    if (temperature > 26 && relativeHumidity > 60 && co2Concentration > 1000)
    {
      // Turn relay ON, red OFF, and green ON
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }
    else
    {
      // Turn relay OFF, red ON, and green OFF
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }

  // Check if in Manual Mode
  else
  {
    bool isOn = digitalRead(ROCKER_SWITCH_PIN);

    // Turn off yellow LED for Manual indication
    digitalWrite(YELLOW_LED_PIN, LOW);

    // Check if rocker switch is on and turn relay on
    if (isOn == LOW)
    {
      // Turn relay ON, red OFF, and green ON
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }

    // Check if rocker switch is off and turn relay off
    else
    {
      // Turn relay OFF, red ON, and green OFF
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }
}

/////////////////////////////////////
// Display Functions
/////////////////////////////////////

// Display sensory data
void displayData(float temperature, float relativeHumidity, uint16_t co2Concentration, char time[10])
{
  // Initialize necessary variables
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - previousMillis;

  // Check if elapsed time is 5 seconds
  if (elapsedTime >= 5000)
  {
    // Check if LCD has not displayed first info
    if (!hasDisplay)
    {
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

      // Display temperature to serial
      Serial.print(F("Temperature: "));
      Serial.print(temperature);
      Serial.println(F("C"));

      // Display humidity to serial
      Serial.print(F("Humidity: "));
      Serial.print(relativeHumidity);
      Serial.println(F("%"));
    }

    // Check if LCD has displayed first info
    else
    {
      // Display CO2 concentration to LCD
      lcd.clear();
      lcd.print(F("CO2: "));
      lcd.print(co2Concentration);
      lcd.print(F("PPM"));

      // Display time to LCD
      lcd.setCursor(0, 1);
      lcd.print(F("Time: "));
      lcd.print(time);

      // Display CO2 to serial
      Serial.print(F("CO2 Level: "));
      Serial.print(co2Concentration);
      Serial.println(F("PPM"));

      // Display time to serial
      Serial.print(F("Time: "));
      Serial.println(time);
    }

    // Reset necessary variables
    previousMillis = currentMillis;
    hasDisplay = !hasDisplay;
  }
}

// Display error messages
void displayError(String lcdMessage, String serialMessage)
{
  // Print error message to Serial
  Serial.println(serialMessage);

  // Print error message to LCD
  lcd.clear();
  lcd.print(lcdMessage);

  // Infinite loop to stop the program
  while (1)
    ;
}

// Display messages for setup
void displayMessages(String row1Message, String row2Message, unsigned long milliseconds)
{
  // Display to first row of LCD
  lcd.clear();
  lcd.print(row1Message);

  // Display to second row of LCD
  lcd.setCursor(0, 1);
  lcd.print(row2Message);

  // Wait for specified milliseconds
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

  // Checks if stopped due to power loss
  if (ds3231.lostPower())
  {
    // Adjust time to compile time
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

  // Disable SQW pin to allow for alarm interrupt
  ds3231.writeSqwPinMode(DS3231_OFF);

  // Set alarm 2 for 1 hour interval of writing to SD card
  setAlarm(2, ds3231.now() + TimeSpan(0, 1, 0, 0), 'h');

  // Display success message to LCD
  displayMessages(F("DS3231 is"), F("working..."), 1000);
}

// Prepare LCD on startup
void initializeLCD()
{
  Serial.println(F("Initializing LCD1602..."));

  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  // Display success messages to LCD
  displayMessages(F("Testing"), F("components..."), 5000);
  displayMessages(F("Arduino is"), F("working..."), 1000);
  displayMessages(F("LCD1602 is"), F("working..."), 1000);
}

// Prepare LEDs on startup
void initializeLED()
{
  Serial.println(F("Initializing LEDs..."));

  // Configure pin modes
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Set all LEDs ON initially
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);

  // Display success message to LCD
  displayMessages(F("LEDs are"), F("working..."), 1000);
}

// Prepare relay on startup
void initializeRelay()
{
  Serial.println(F("Initializing relay..."));

  // Configure pin mode
  pinMode(RELAY_PIN, OUTPUT);

  // Set relay HIGH, turning it OFF
  digitalWrite(RELAY_PIN, HIGH);

  // Display success message to LCD
  displayMessages(F("Relay is"), F("working..."), 1000);
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
  delay(30); // Wait for the sensor

  // Stop measuring the environment
  if (scd41.stopPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement() failed."));
  }
  delay(500); // Wait for the sensor

  // Reinitialize SCD41
  if (scd41.reinit() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: reinit() failed."));
  }
  delay(30); // Wait for the sensor

  // Test the component's functionality
  if (scd41.performSelfTest(testResult) != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: performSelfTest() failed."));
  }
  delay(10000); // Wait for the sensor

  // Check result of self-test
  if (testResult != 0)
  {
    // Display error messages
    displayError(F("SCD 41 failed"), F("Error: self-test failed."));
  }

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

  // Display success message to LCD
  displayMessages(F("SCD41 is"), F("working..."), 0);
}

// Prepare SD Card on startup
void initializeSDCard()
{
  Serial.println(F("Initializing SD card..."));

  // Check if there is no connection with sd card
  if (!SD.begin(CHIP_SELECT_PIN))
  {
    // Display error messages
    displayError(F("SD card failed"), F("Error: SD card failed to initialize."));
  }

  // Display success message to LCD
  displayMessages(F("SD card is"), F("working..."), 2000);
}

// Prepare switches on startup
void initializeSwitches()
{
  // Configure pins for switches
  pinMode(SLIDE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(ROCKER_SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SLIDE_SWITCH_PIN), toggleAuto, CHANGE);

  // Set initial mode (Manual/Auto)
  isAuto = digitalRead(SLIDE_SWITCH_PIN);
}

/////////////////////////////////////
// Other Functions
/////////////////////////////////////

// Power down Arduino and reset components
void goToSleep()
{
  // Reset relay
  digitalWrite(RELAY_PIN, HIGH);

  // Stop SCD41 current periodic measurement
  if (!scd41.stopPeriodicMeasurement())
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement failed."));
  }

  delay(500); // Wait for the sensor

  // Start SCD41 new low power periodic measurement
  if (!scd41.startLowPowerPeriodicMeasurement())
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startLowPowerPeriodicMeasurement failed."));
  }

  // Reset LCD1602
  lcd.noBacklight();
  lcd.noDisplay();

  // Reset LEDs
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  // Reset alarms
  ds3231.clearAlarm(1);
  ds3231.clearAlarm(2);
  ds3231.disableAlarm(2);

  // Set alarm to wake up Arduino after 12 hours
  setAlarm(1, ds3231.now() + TimeSpan(0, 12, 0, 0), 'h');

  // Power down Arduino
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

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

  // Close file
  dataFile.close();
}

// Reset all components after waking up
void resetComponents()
{
  // Reset necessary variables
  previousMeasurement = 0;
  previousMillis = 0;

  // Initialize LCD1602
  lcd.backlight();
  lcd.display();
  lcd.clear();
  lcd.print(F("Waking up..."));

  // Initialize LEDs
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);

  // Stop SCD41 current periodic measurement
  if (!scd41.stopPeriodicMeasurement())
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement failed."));
  }
  delay(500); // Wait for the sensor

  // Start SCD41 new periodic measurement
  if (!scd41.startPeriodicMeasurement())
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startPeriodicMeasurement failed."));
  }

  // Wait 5 seconds for initial measurement
  delay(5000);
}

// Set alarm based on specified details
void setAlarm(int alarmNum, DateTime alarmTime, char alarmMode)
{
  // Check if alarm mode is seconds or hours
  switch (alarmMode)
  {
  // Set alarm in seconds
  case 's':
    // Set alarm 1
    if (!ds3231.setAlarm1(alarmTime, DS3231_A1_Second))
    {
      // Display error messages
      displayError(F("DS3231 failed"), F("Error: alarm 1 is not set."));
    }
    break;

  // Set alarm in hours
  case 'h':
    // Set alarm 1
    if (alarmNum == 1)
    {
      if (!ds3231.setAlarm1(alarmTime, DS3231_A1_Hour))
      {
        // Display error messages
        displayError(F("DS3231 failed"), F("Error: alarm 1 is not set."));
      }
    }

    // Set alarm 2
    else
    {
      if (!ds3231.setAlarm2(alarmTime, DS3231_A2_Hour))
      {
        // Display error messages
        displayError(F("DS3231 failed"), F("Error: alarm 2 is not set."));
      }
    }
    break;

  default:
    // Display error messages
    displayError(F("Invalid alarm."), F("Error: invalid alarm mode set."));
    break;
  }
}

// Toggle Auto to Manual and vice versa
void toggleAuto()
{
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