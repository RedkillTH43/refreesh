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

enum Pins : byte
{
  CLOCK_INTERRUPT_PIN = 2,
  SLIDE_SWITCH_PIN = 3,
  ROCKER_SWITCH_PIN = 4,
  GREEN_LED_PIN = 5,
  YELLOW_LED_PIN = 6,
  RED_LED_PIN = 7,
  RELAY_PIN = 9,
  CHIP_SELECT_PIN = 10
};

// Define global macros
const char SENSING_FILE_NAME[] = "sdata.csv";
const char OPERATION_FILE_NAME[] = "odata.csv";
const int SENSING_ARRAY_LENGTH = 5;
const int OPERATION_ARRAY_LENGTH = 3;
const int timeOffset = 15;
const unsigned long DISPLAY_INTERVAL = 5000L;
const unsigned long MEASURE_INTERVAL = DISPLAY_INTERVAL * 2;
const unsigned long DELAY_INTERVAL = 100L;

// Initialize flags
bool isInactive;
bool hasDisplay = false;
bool alarm1Set = false;
bool initialMeasurement = true;
bool initialOnTracking = true;
bool initialOffTracking = true;
unsigned long previousMeasurement = 0L;
unsigned long previousMillis = 0L;
unsigned long previousTest = 0L;
unsigned long previousOnTime = 0L;
unsigned long previousOffTime = 0L;
uint16_t co2Concentration = 0;
float temperature = 0.0f;
float relativeHumidity = 0.0f;
float timeOn = 0.0f;
float timeOff = 0.0f;

// Declare component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 ds3231;
SensirionI2cScd4x scd41;

/////////////////////////////////////
// Main Program Loop
/////////////////////////////////////

void setup()
{
  // Initialize all components
  initializeComponents();

  // Display success message
  displayMessages(F("All components"), F("are working!"));
}

void loop()
{
  // Initialize necessary variables
  uint8_t currentHour = ds3231.now().hour();
  isInactive = (currentHour < 6 || currentHour >= 20);

  // Check if current time is not between 6AM and 8PM
  if (isInactive)
  {
    char timeOnText[7], timeOffText[7];
    char currentDate[] = "YYYY-MM-DD";
    ds3231.now().toString(currentDate);

    dtostrf(timeOn, 3, 2, timeOnText);
    dtostrf(timeOff, 3, 2, timeOffText);
    char *content[] = {currentDate, timeOnText, timeOffText};
    writeToCard(OPERATION_FILE_NAME, content);

    while (isInactive)
    {
      // Put Arduino and components to sleep
      goToSleep();

      // Reset necessary variables
      previousMeasurement = 0;

      // Begin measuring data
      beginMeasurement();
      handleEvent();

      uint8_t currentHour = ds3231.now().hour();
      isInactive = (currentHour < 6 || currentHour >= 20);
    }

    // Reset all components after waking up
    resetComponents();
  }

  // Begin measuring data
  beginMeasurement();

  // Handle measured data
  handleEvent();

  // Display measured data
  displayData();
}

/////////////////////////////////////
// Handler Function
/////////////////////////////////////

// Handles events
void handleEvent()
{
  // Check if alarm 1 for 1-hour interval writing to SD card is ringing
  if (ds3231.alarmFired(1) && alarm1Set)
  {
    // Declare necessary variables
    char tempText[7], humidityText[7], co2Text[7];
    char currentDate[] = "YYYY-MM-DD";
    char currentTime[] = "hh:mm:ss";
    ds3231.now().toString(currentTime);
    ds3231.now().toString(currentDate);

    // Convert values to strings
    dtostrf(temperature, 3, 2, tempText);
    dtostrf(relativeHumidity, 3, 2, humidityText);
    itoa(co2Concentration, co2Text, 10);

    char *content[] = {currentDate, currentTime, tempText, humidityText, co2Text};

    // Write to file in sd card
    writeToCard(SENSING_FILE_NAME, content);

    // Clear this alarm
    ds3231.disableAlarm(1);
    ds3231.clearAlarm(1);
    alarm1Set = false;

    // Set another alarm for 1-hour interval
    setAlarm(ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), DS3231_A1_Minute);
    alarm1Set = true;
  }

  if (!isInactive)
  {
    byte isAuto = digitalRead(SLIDE_SWITCH_PIN);

    // Turn yellow LED on or off for Auto indication
    digitalWrite(YELLOW_LED_PIN, !isAuto);

    // Check if in Auto Mode
    if (isAuto == LOW)
    {
      unsigned long currentTime = millis();

      // Check if temperature, humidity, and co2 are above normal level
      if (temperature > 26 && relativeHumidity > 60 && co2Concentration > 1000)
      {
        initialOffTracking = true;

        if (initialOnTracking)
        {
          previousOnTime = millis();
          initialOnTracking = false;
        }

        unsigned long elapsedTime = currentTime - previousOnTime;

        if (elapsedTime >= 1000)
        {
          timeOn += elapsedTime / 1000;
          previousOnTime = currentTime;
        }

        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
      }

      // Check if temperature, humidity, and co2 are within normal level
      else
      {
        initialOnTracking = true;

        if (initialOffTracking)
        {
          previousOffTime = millis();
          initialOffTracking = false;
        }

        unsigned long elapsedTime = currentTime - previousOffTime;

        if (elapsedTime >= 1000)
        {
          timeOff += elapsedTime / 1000;
          previousOffTime = currentTime;
        }

        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
      }
    }

    // Check if in Manual Mode
    else
    {
      // Store the current state of rocker switch
      unsigned long currentTime = millis();

      byte isOn = digitalRead(ROCKER_SWITCH_PIN);

      if (isOn == LOW)
      {
        initialOffTracking = true;
        if (initialOnTracking)
        {
          previousOnTime = millis();
          initialOnTracking = false;
        }

        unsigned long elapsedTime = currentTime - previousOnTime;

        if (elapsedTime >= 1000)
        {
          timeOn += elapsedTime / 1000;
          previousOnTime = currentTime;
        }
      }

      if (isOn == HIGH)
      {
        initialOnTracking = true;
        unsigned long elapsedTime = currentTime - previousOffTime;

        if (initialOffTracking)
        {
          previousOffTime = millis();
          initialOffTracking = false;
        }

        if (elapsedTime >= 1000)
        {
          timeOff += elapsedTime / 1000;
          previousOffTime = currentTime;
        }
      }

      // Turn relay, red led, and green led on or off
      digitalWrite(RELAY_PIN, isOn);
      digitalWrite(RED_LED_PIN, isOn);
      digitalWrite(GREEN_LED_PIN, !isOn);
    }
  }
}

/////////////////////////////////////
// Display Functions
/////////////////////////////////////

// Display sensory data
void displayData()
{
  // Initialize necessary variables
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - previousMillis;
  char currentTime[] = "hh:mm";
  ds3231.now().toString(currentTime);

  // Check if elapsed time is 5 seconds
  if (elapsedTime >= DISPLAY_INTERVAL)
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
      lcd.print(currentTime);
    }

    // Reset necessary variables
    previousMillis = currentMillis;
    hasDisplay = !hasDisplay;
  }
}

// Display error messages
void displayError(String lcdMessage, String serialMessage)
{
  // Print error message to LCD
  lcd.clear();
  lcd.print(lcdMessage);

  // Print error message to Serial
  Serial.println(serialMessage);

  // Turn off relay
  digitalWrite(RELAY_PIN, HIGH);

  // Infinite loop to stop the program
  while (1)
    ;
}

// Display messages for setup
void displayMessages(String row1Message, String row2Message)
{
  // Display to first row of LCD
  lcd.clear();
  lcd.print(row1Message);

  // Display to second row of LCD
  lcd.setCursor(0, 1);
  lcd.print(row2Message);
}

/////////////////////////////////////
// Initialize Functions
/////////////////////////////////////

// Initialize all components
void initializeComponents()
{
  initializeArduino();
  initializeLCD();
  displayMessages(F("Testing"), F("components..."));
  initializeSwitches();
  initializeRelay();
  initializeLED();
  initializeSCD41();
  initializeSDCard();
  initializeDS3231();
}

// Prepare Arduino on startup
void initializeArduino()
{
  Serial.begin(9600);
  Wire.begin();
}

// Prepare DS3231 on startup
void initializeDS3231()
{
  DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
  // Test I2C connection
  if (!ds3231.begin())
  {
    displayError(F("DS3231 failed"), F("Error: DS3231 failed to initialize."));
  }

  // Checks if stopped due to power loss
  if (ds3231.lostPower())
  {
    // Adjust time to compile time
    ds3231.adjust(compileTime + TimeSpan(timeOffset));
  }

  // Disable unnecessary pin
  ds3231.disable32K();

  // Set interrupt pin
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);

  // Clear alarms
  ds3231.disableAlarm(1);
  ds3231.disableAlarm(2);
  ds3231.clearAlarm(1);
  ds3231.clearAlarm(2);
  alarm1Set = false;

  // Disable SQW pin to allow for alarm interrupt
  ds3231.writeSqwPinMode(DS3231_OFF);

  setAlarm(ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), DS3231_A1_Minute);
  alarm1Set = true;
}

// Prepare LCD on startup
void initializeLCD()
{
  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
}

// Prepare LEDs on startup
void initializeLED()
{
  // Configure pin modes
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Set all LEDs ON initially
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
}

// Prepare relay on startup
void initializeRelay()
{
  // Configure pin mode
  pinMode(RELAY_PIN, OUTPUT);

  // Set relay HIGH, turning it OFF
  digitalWrite(RELAY_PIN, HIGH);
}

// Prepare SCD41 on startup
void initializeSCD41()
{
  // Declare necessary variables
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

  // Start measuring the environment
  if (scd41.startPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startPeriodicMeasurement() failed."));
  }
}

// Prepare SD Card on startup
void initializeSDCard()
{
  bool status;
  bool displayed = false;

  // Check if there is no connection with sd card
  do
  {
    status = SD.begin(CHIP_SELECT_PIN);
    if (!displayed)
    {
      lcd.clear();
      lcd.print(F("Insert SD card.."));
    }
    displayed = true;
  } while (!status);
}

// Prepare switches on startup
void initializeSwitches()
{
  // Configure pins for switches
  pinMode(SLIDE_SWITCH_PIN, INPUT_PULLUP);
  pinMode(ROCKER_SWITCH_PIN, INPUT_PULLUP);
}

/////////////////////////////////////
// Other Functions
/////////////////////////////////////

// Start measuring the environment
void beginMeasurement()
{
  bool dataReady = false;
  unsigned long currentMeasurement = millis();
  unsigned long interval = (isInactive) ? 0L : MEASURE_INTERVAL;

  // Check if data is ready to be measured
  if ((currentMeasurement - previousMeasurement) >= interval || initialMeasurement)
  {
    previousMeasurement = currentMeasurement;
    initialMeasurement = false;

    // Set data ready status
    if (scd41.getDataReadyStatus(dataReady) != 0)
    {
      // Display error messages
      displayError(F("SCD41 failed"), F("Error: getDataReadyStatus() failed."));
    }

    while (!dataReady)
    {
      if ((currentMeasurement - previousMeasurement) >= DELAY_INTERVAL)
      {
        previousMeasurement = currentMeasurement;
        if (scd41.getDataReadyStatus(dataReady) != 0)
        {
          // Display error messages
          displayError(F("SCD41 failed"), F("Error: getDataReadyStatus failed."));
        }
      }
      currentMeasurement = millis();
    }

    // Read and set measurement values
    if (scd41.readMeasurement(co2Concentration, temperature, relativeHumidity) != 0)
    {
      // Display error messages
      displayError(F("SCD41 failed"), F("Error: readMeasurement() failed."));
    }
  }
}

// Power down Arduino and reset components
void goToSleep()
{
  // Reset alarms
  ds3231.clearAlarm(1);
  ds3231.disableAlarm(1);
  alarm1Set = false;

  setAlarm(ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), DS3231_A1_Minute);
  alarm1Set = true;

  // Reset relay
  digitalWrite(RELAY_PIN, HIGH);

  // Stop SCD41 current periodic measurement
  if (scd41.stopPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement failed."));
  }

  // Start SCD41 new low power periodic measurement
  if (scd41.startLowPowerPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startLowPowerPeriodicMeasurement failed."));
  }

  // Reset LEDs
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  // Display message
  lcd.clear();
  lcd.print(F("Going to sleep.."));
  delay(3000);

  // Reset LCD1602
  lcd.noBacklight();
  lcd.noDisplay();

  // Power down Arduino
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

// Helper function for alarm
void onAlarm()
{
  return;
}

// Read sd card contents
void readFromCard(const char FILE_NAME[])
{
  // Initialize File object
  File dataFile;
  bool status;
  bool displayed = false;

  dataFile = SD.open(FILE_NAME, FILE_READ);

  // Check if file does not exist
  while (!dataFile)
  {
    // Check if there is no connection with sd card
    do
    {
      status = SD.begin(CHIP_SELECT_PIN);
      if (!displayed)
      {
        digitalWrite(RELAY_PIN, HIGH);
        lcd.clear();
        lcd.print(F("Insert SD card.."));
      }
      displayed = true;
    } while (!status);

    dataFile = SD.open(FILE_NAME, FILE_READ);
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
  timeOn = 0.0f;
  timeOff = 0.0f;
  hasDisplay = false;
  isInactive = false;

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
  if (scd41.stopPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: stopPeriodicMeasurement failed."));
  }

  // Start SCD41 new periodic measurement
  if (scd41.startPeriodicMeasurement() != 0)
  {
    // Display error messages
    displayError(F("SCD41 failed"), F("Error: startPeriodicMeasurement failed."));
  }
}

// Set alarm based on specified details
void setAlarm(DateTime alarmTime, Ds3231Alarm1Mode alarmMode)
{
  if (!ds3231.setAlarm1(alarmTime, alarmMode))
  {
    // Display error messages
    displayError(F("DS3231 failed"), F("Error: alarm 1 is not set."));
  }
}

// Write data to sd card
void writeToCard(const char FILE_NAME[], char *content[])
{
  // Initialize File object
  File dataFile;
  bool status;
  bool displayed = false;
  int length = (FILE_NAME == SENSING_FILE_NAME) ? SENSING_ARRAY_LENGTH : OPERATION_ARRAY_LENGTH;

  dataFile = SD.open(FILE_NAME, FILE_WRITE);

  // Check if file does not exist
  while (!dataFile)
  {
    // Check if there is no connection with sd card
    do
    {
      status = SD.begin(CHIP_SELECT_PIN);
      if (!displayed)
      {
        digitalWrite(RELAY_PIN, HIGH);
        lcd.clear();
        lcd.print(F("Insert SD card.."));
      }
      displayed = true;
    } while (!status);

    dataFile = SD.open(FILE_NAME, FILE_WRITE);
  }

  for (int i = 0; i < length; i++)
  {
    if (i == (length - 1))
    {
      dataFile.println(content[i]);
    }
    else
    {
      dataFile.print(content[i]);
      dataFile.print(F(", "));
    }
  }

  dataFile.close();
}