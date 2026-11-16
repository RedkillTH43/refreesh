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

enum Tests : bool
{
  SCD41_SELF_TEST = false,
  ONE_MIN_WRITE = true,
  DELETE_FILE = true,
  READ_SD_CARD = true,
  ADJUST_TIME = false,
  LIGHT_LED = true,
  ONE_MIN_SLEEP = false,
  PRINT_TIME = false
};

// Define global macros
const char SENSING_FILE_NAME[] = "sdata.csv";
const char OPERATION_FILE_NAME[] = "odata.csv";
const int SENSING_ARRAY_LENGTH = 5;
const int OPERATION_ARRAY_LENGTH = 7;
const int timeOffset = 15;
const unsigned long DISPLAY_INTERVAL = 5000L;
const unsigned long MEASURE_INTERVAL = DISPLAY_INTERVAL * 2;
const unsigned long DELAY_INTERVAL = 100L;

// Initialize flags
bool isInactive = true;
bool hasDisplay = false;
bool alarm1Set = false;
bool alarm2Set = false;
bool initialMeasurement = true;
unsigned long previousMeasurement = 0L;
unsigned long previousMillis = 0L;
unsigned long previousTest = 0L;
uint16_t co2Concentration = 0;
float temperature = 0.0f;
float relativeHumidity = 0.0f;

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
  bool dataReady = false;
  uint8_t currentHour = ds3231.now().hour();
  unsigned long currentMeasurement = millis();

  isInactive = (ONE_MIN_SLEEP && isInactive) ? true : (currentHour <= 6 || currentHour >= 20);

  // Check if current time is not between 6AM and 8PM
  if (isInactive)
  {
    Serial.println(F("Device is going to sleep..."));
    // Put Arduino and components to sleep
    goToSleep();

    // Reset all components after waking up
    Serial.println(F("Device is waking up..."));
    resetComponents();
  }

  // Check if data is ready to be measured
  if ((currentMeasurement - previousMeasurement) >= MEASURE_INTERVAL || initialMeasurement)
  {
    Serial.println(F("10 seconds had passed, measuring environment..."));
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
      Serial.println(F("Data is not ready, trying again..."));
      if ((currentMeasurement - previousMeasurement) >= DELAY_INTERVAL)
      {
        Serial.println(F("100 milliseconds had passed, measuring environment..."));
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
  // Check if alarm 1 for 10-hour interval of sleep is ringing
  if (ds3231.alarmFired(1) && alarm1Set)
  {
    // Clear and disable this alarm
    ds3231.clearAlarm(1);
    ds3231.disableAlarm(1);
    alarm1Set = false;

    if (ONE_MIN_WRITE)
    {
      setAlarm(2, ds3231.now() + TimeSpan(0, 0, 1, 0), 'm');
    }
    else
    {
      setAlarm(2, ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), 'm');
    }

    alarm2Set = true;

    char currentTime[] = "hh:mm:ss";
    char targetTime[] = "hh:mm:ss";

    Serial.print(F("Current Time: "));
    ds3231.now().toString(currentTime);
    Serial.println(currentTime);

    Serial.print(F("Target Time: "));
    ds3231.getAlarm2().toString(targetTime);
    Serial.println(targetTime);
  }

  // Check if alarm 2 for 1-hour interval writing to SD card is ringing
  if (ds3231.alarmFired(2) && alarm2Set)
  {
    // Declare necessary variables
    char tempText[7], humidityText[7], co2Text[7];
    char currentDate[] = "YYYY-MM-DD";
    char currentTime[] = "hh:mm:ss";
    char targetTime[] = "hh:mm:ss";
    ds3231.now().toString(currentTime);
    ds3231.now().toString(currentDate);

    // Convert values to strings
    dtostrf(temperature, 3, 2, tempText);
    dtostrf(relativeHumidity, 3, 2, humidityText);
    itoa(co2Concentration, co2Text, 10);

    char *content[] = {currentDate, currentTime, tempText, humidityText, co2Text};

    // Write to file in sd card
    Serial.println(F("Writing data to SD card..."));
    writeToCard(SENSING_FILE_NAME, content);

    if (READ_SD_CARD)
    {
      Serial.println(F("Reading data from SD card..."));
      readFromCard(SENSING_FILE_NAME);
    }

    // Clear this alarm
    ds3231.disableAlarm(2);
    ds3231.clearAlarm(2);
    alarm2Set = false;

    // Set another alarm for 1-hour interval

    if (ONE_MIN_WRITE)
    {
      setAlarm(2, ds3231.now() + TimeSpan(0, 0, 1, 0), 'm');
    }
    else
    {
      setAlarm(2, ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), 'm');
    }

    alarm2Set = true;

    Serial.print(F("Current Time: "));
    Serial.println(currentTime);

    ds3231.getAlarm2().toString(targetTime);
    Serial.print(F("Target Time: "));
    Serial.println(targetTime);
  }

  byte isAuto = digitalRead(SLIDE_SWITCH_PIN);

  // Turn yellow LED on or off for Auto indication
  digitalWrite(YELLOW_LED_PIN, !isAuto);

  // Check if in Auto Mode
  if (isAuto == LOW)
  {
    // Check if temperature, humidity, and co2 are above normal level
    if (temperature > 26 && relativeHumidity > 60 && co2Concentration > 1000)
    {
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }

    // Check if temperature, humidity, and co2 are within normal level
    else
    {
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }

  // Check if in Manual Mode
  else
  {
    // Store the current state of rocker switch
    byte isOn = digitalRead(ROCKER_SWITCH_PIN);

    // Turn relay, red led, and green led on or off
    digitalWrite(RELAY_PIN, isOn);
    digitalWrite(RED_LED_PIN, isOn);
    digitalWrite(GREEN_LED_PIN, !isOn);
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
  char testTime[] = "hh:mm:ss";
  ds3231.now().toString(currentTime);

  if (PRINT_TIME)
  {
    ds3231.now().toString(testTime);
    if ((currentMillis - previousTest) >= 1000)
    {
      previousTest = currentMillis;
      Serial.println(testTime);
    }
  }

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
  Serial.println(F("Initializing LCD1602..."));
  initializeLCD();
  displayMessages(F("Testing"), F("components..."));
  initializeSwitches();
  initializeRelay();
  initializeLED();
  Serial.println(F("Initializing SCD41..."));
  initializeSCD41();
  Serial.println(F("Initializing SD card..."));
  initializeSDCard();
  Serial.println(F("Initializing DS3231..."));
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

  if (ADJUST_TIME)
  {
    ds3231.adjust(compileTime + TimeSpan(timeOffset));
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
  alarm2Set = false;

  // Disable SQW pin to allow for alarm interrupt
  ds3231.writeSqwPinMode(DS3231_OFF);

  // Set alarm 2 for 1-hour interval of writing to SD card
  if (DELETE_FILE)
  {
    SD.remove(SENSING_FILE_NAME);
    SD.remove(OPERATION_FILE_NAME);
  }

  if (ONE_MIN_WRITE)
  {
    setAlarm(2, ds3231.now() + TimeSpan(0, 0, 1, 0), 'm');
  }
  else
  {
    setAlarm(2, ds3231.now() + TimeSpan(0, 1, -ds3231.now().minute(), -ds3231.now().second()), 'm');
  }

  alarm2Set = true;

  char currentTime[] = "YYYY-MM-DD hh:mm:ss";
  char targetTime[] = "YYYY-MM-DD hh:mm:ss";

  Serial.print(F("Current Time: "));
  ds3231.now().toString(currentTime);
  Serial.println(currentTime);

  Serial.print(F("Target Time: "));
  ds3231.getAlarm2().toString(targetTime);
  Serial.println(targetTime);
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
  if (LIGHT_LED)
  {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(YELLOW_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
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

  if (SCD41_SELF_TEST)
  {
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
      displayError(F("SCD41 failed"), F("Error: self-test failed."));
    }
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

  // Check if there is no connection with sd card
  do
  {
    status = SD.begin(CHIP_SELECT_PIN);
    lcd.clear();
    lcd.print(F("Insert SD card.."));
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

// Power down Arduino and reset components
void goToSleep()
{
  // Set alarm to wake up Arduino after 10 hours
  char time[] = "YYYY-MM-DD hh:mm:ss";
  char targetTime[] = "YYYY-MM-DD hh:mm:ss";

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

  // Reset alarms
  ds3231.clearAlarm(1);
  ds3231.clearAlarm(2);
  ds3231.disableAlarm(1);
  ds3231.disableAlarm(2);
  alarm1Set = false;
  alarm2Set = false;

  if (ONE_MIN_SLEEP)
  {
    setAlarm(1, ds3231.now() + TimeSpan(0, 0, 1, 0), 'm');
  }
  else
  {
    setAlarm(1, ds3231.now() + TimeSpan(1, 6 - ds3231.now().hour(), -ds3231.now().minute(), -ds3231.now().second()), 'h');
  }
  alarm1Set = true;

  ds3231.now().toString(time);
  Serial.print(F("Current Time: "));
  Serial.println(time);

  ds3231.getAlarm1().toString(targetTime);
  Serial.print(F("Wakeup Time: "));
  Serial.println(targetTime);

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
void setAlarm(int alarmNum, DateTime alarmTime, char alarmMode)
{
  // Check if alarm mode is seconds or hours
  switch (alarmMode)
  {

  // Set alarm in minutes
  case 'm':
    // Set alarm 1
    if (alarmNum == 1)
    {
      if (!ds3231.setAlarm1(alarmTime, DS3231_A1_Minute))
      {
        // Display error messages
        displayError(F("DS3231 failed"), F("Error: alarm 1 is not set."));
      }
    }

    // Set alarm 2
    else
    {
      if (!ds3231.setAlarm2(alarmTime, DS3231_A2_Minute))
      {
        // Display error messages
        displayError(F("DS3231 failed"), F("Error: alarm 2 is not set."));
      }
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
    displayError(F("DS3231 failed"), F("Error: invalid alarm mode set."));
    break;
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