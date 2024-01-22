#include <Servo.h>
#include <DHT.h>

// Constants
#define DHTPIN 7         // DHT sensor pin
#define DHTTYPE DHT22    // DHT 22 (AM2302)
#define ECHO_PIN 12      // Echo pin for ultrasonic sensor
#define TRIG_PIN 6       // Trigger pin for ultrasonic sensor
#define SERVO_PIN 5      // Servo pin
#define RELAY_PIN 8      // Relay pin for fan
#define PIR_LED_PIN 4    // LED pin for PIR sensor
#define PIR_INPUT_PIN 2  // Input pin for PIR sensor
#define LDR_PIN A1       // Light sensor pin
#define BUTTON_PIN 10    // Button pin

// Global variables
unsigned long pirLastActivated = 0, lastPirTriggerTime = 0, lastLDRReadTime = 0, lastDHTReadTime = 0;
const long pirActivationTime = 10000, pirDebounceTime = 3000, LDRReadInterval = 2000, DHTReadInterval = 2000;
bool pirActivated = false, autoPirMode = true, autoFanMode = true, garageAutoMode = true, autoLightMode = true;
bool lastButtonState = HIGH;
int pirState = LOW;
const float GAMMA = 0.7, RL10 = 50;

DHT dht(DHTPIN, DHTTYPE);
Servo myservo;

void setup() {
  initializePins();
  Serial.begin(9600);
  myservo.attach(SERVO_PIN);
  myservo.write(0);  // Initial position of servo
  dht.begin();
  digitalWrite(RELAY_PIN, HIGH);  // Initially, the fan is off
}

void loop() {
  if (Serial.available() > 0) {
    handleSerialCommands();
  }

  float temperature = handleDHT22Sensor();
  float distance = readDistanceCM();
  float lux = readLightLevel();
  handlePIRSensor();
  controlFan(temperature);
  handleGarageDoor(distance);
  controlLED(lux);

  // Send data over serial every 2 seconds
  static unsigned long lastSerialUpdate = 0;
  if (millis() - lastSerialUpdate > 2000) {
    sendSerialData(temperature, distance, lux);
    lastSerialUpdate = millis();
  }
}

void sendSerialData(float temperature, float distance, float lux) {
  String data = "Temperature:" + String(temperature) + ",Distance:" + String(distance) +
                ",LUX:" + String(lux) + ",Garage:" + (distance < 10 ? "open" : "closed") +
                ",PIRMotion:" + (pirActivated ? "detected" : "ended");
  Serial.println(data);
}

void initializePins() {
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIR_LED_PIN, OUTPUT);
  pinMode(PIR_INPUT_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
}

void handleSerialCommands() {
  String command = Serial.readStringUntil('\n');
  if (command == "AUTO LIGHT") {
    autoLightMode = !autoLightMode;
  } else if (command == "AUTO FAN") {
    autoFanMode = !autoFanMode;
    controlFan(handleDHT22Sensor()); // Update the fan status immediately
  } else if (command == "AUTO GARAGE") {
    garageAutoMode = !garageAutoMode;
    handleGarageDoor(readDistanceCM()); // Update the garage door status immediately
  } else if (command == "AUTO PIR") {
    autoPirMode = !autoPirMode;
    if (!autoPirMode) {
      pirActivated = false;
      digitalWrite(PIR_LED_PIN, LOW); // Turn off LED when PIR mode is disabled
    }
  }
}

float handleDHT22Sensor() {
  unsigned long currentMillis = millis();
  static float lastTemperature = 0; // To store the last read temperature
  
  if (currentMillis - lastDHTReadTime >= DHTReadInterval) {
    lastDHTReadTime = currentMillis;
    float temperature = dht.readTemperature();
    if (!isnan(temperature)) {
      lastTemperature = temperature;
    }
  }
  return lastTemperature;
}

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;  // Calculate and return the distance
}

void controlFan(float temperature) {
  if (temperature > 30) {
    digitalWrite(RELAY_PIN, LOW); // Turn on the fan
  } else if (temperature < 24) {
    digitalWrite(RELAY_PIN, HIGH);  // Turn off the fan
  }
}

void handleGarageDoor(float distance) {
  if (garageAutoMode && distance < 10) {
    myservo.write(96); // Open the garage door
  } else if (garageAutoMode) {
    myservo.write(0); // Close the garage door
  }
}

void controlLED(float lux) {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    autoLightMode = false;
  }
  lastButtonState = currentButtonState;
  if (autoLightMode && lux < 500) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (autoLightMode) {
    digitalWrite(LED_BUILTIN, LOW);
  }
}
float readLightLevel() {
  unsigned long currentMillis = millis();
  static float lastLux = 0; // To store the last read lux value

  if (currentMillis - lastLDRReadTime >= LDRReadInterval) {
    lastLDRReadTime = currentMillis;
    int lanalogValue = analogRead(LDR_PIN);
    float voltage = lanalogValue / 1024.0 * 5.0;
    float resistance = 2000 * voltage / (5.0 - voltage);
    lastLux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, 1 / GAMMA);
  }
  return lastLux; // Return the most recent LUX value instead of 0
}

void handlePIRSensor() {
  if (!autoPirMode) {
    return; // Exit if PIR mode is disabled
  }

  int sensorValue = digitalRead(PIR_INPUT_PIN);
  unsigned long currentMillis = millis();
  if (currentMillis - lastPirTriggerTime >= pirDebounceTime) {
    if (sensorValue == HIGH) {
      if (!pirActivated) {
        pirLastActivated = currentMillis;
        pirActivated = true;
        digitalWrite(PIR_LED_PIN, HIGH); // Turn on LED
      }
      lastPirTriggerTime = currentMillis;
    } else if (sensorValue == LOW && pirActivated && currentMillis - pirLastActivated > pirActivationTime) {
      pirActivated = false;
      digitalWrite(PIR_LED_PIN, LOW); // Turn off LED
    }
  }
}

