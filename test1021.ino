// Import necessary modules
#include <RtcDS1302.h>
#include <RtcDateTime.h>
#include <ThreeWire.h>
#include <DHT.h>
#include <DHT_U.h>
#include <LiquidCrystal_I2C.h>

// Declare component pins
#define DHT22_PIN 8
#define RELAY_PIN 9
#define INTERRUPT_PIN 2
#define RED_PIN 10
#define BLUE_PIN 11

// Create component objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht22(DHT22_PIN, DHT22);
ThreeWire myWire(4, 5, 6);
RtcDS1302<ThreeWire> rtc(myWire);
bool isOn = false;

void setup() {
  // Initialize components
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  dht22.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!rtc.IsDateTimeValid()) {
    Serial.println("Invalid time");
    rtc.SetDateTime(compiled);
  }
  if (rtc.GetIsWriteProtected()) {
    Serial.println("RTC is write protected");
    rtc.SetIsWriteProtected(false);
  }
  if (!rtc.GetIsRunning()) {
    Serial.println("RTC is not running");
    rtc.SetIsRunning(true);
  }
  
  RtcDateTime now = rtc.GetDateTime();
  if (now < compiled) {
    rtc.SetDateTime(compiled);
  }
}

void loop() {
  RtcDateTime now = rtc.GetDateTime();
  if (!now.IsValid()) {
    Serial.println("Invalid time");
  }

  float temperature = dht22.readTemperature(); // Read temperature
  float humidity = dht22.readHumidity(); // Read humidity

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 failed!");
    return;
  }
  float heat_index = dht22.computeHeatIndex(temperature, humidity, false); // Compute heat index
  
  // Display temperature
  lcd.clear();
  lcd.print("Temp: "); 
  lcd.print(temperature);
  lcd.print(char(223));
  lcd.print("C");
  Serial.print("Temperature: ");
  Serial.println(temperature);

  // Display humidity
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print("%");
  Serial.print("Humidity: ");
  Serial.println(humidity);
  if (isOn == false) {
    delay(1000);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BLUE_PIN, HIGH);
    digitalWrite(RED_PIN, LOW);
    Serial.println("Relay ON");
    isOn = true;
  }

  delay(10000);

  // Display heat index
  lcd.clear();
  lcd.print("Index: ");
  lcd.print(heat_index);
  lcd.print(char(223));
  lcd.print("C");

  // Display time
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(now.Hour());
  lcd.print(":");
  lcd.print(now.Minute());
  if (isOn == true) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BLUE_PIN, LOW);
    digitalWrite(RED_PIN, HIGH);
    Serial.println("Relay OFF");
    isOn = false;
  }
  delay(5000);
}