#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <DHT.h>
#include <time.h>
#include <Wire.h>
#include "config.h"  // copier config.h.example en config.h et renseigner les valeurs


// Définition des pins
#define PUMP_PIN 32
#define LED1_PIN 23
#define LED2_PIN 25
#define LED3_PIN 26
#define LED4_PIN 27
#define LED5_PIN 14
#define FAN_PIN 18
#define WATER_LEVEL_PIN 19  // Capteur de niveau d'eau
#define PH_SENSOR_PIN 39
#define TDS_SENSOR_PIN 34
#define DHT_PIN 21  // Capteur DHT pour température et humidité
#define LIGHT_SENSOR_PIN 36

// Déclaration des variables pour le calibrage de la LDR
#define MAX_ADC_READING           4095
#define ADC_REF_VOLTAGE           3.3
#define REF_RESISTANCE            9700  // measure this for best results
#define LUX_CALC_SCALAR           1545832
#define LUX_CALC_EXPONENT         -1.049


// Déclaration des servos
Servo servo1; // Servo pour le pH
//Servo servo2; // Servo pour le TDS


// Déclaration du capteur DHT
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);


// Variables pour la gestion des intervalles de capture
unsigned long lastDHTRead_temp = 0;
unsigned long lastDHTRead_humidity = 0;
unsigned long lastPHRead = 0;
unsigned long lastLDRRead = 0;
unsigned long lastTDSRead = 0;
unsigned long lastWaterLevelRead = 0;
bool lastWaterLevelState = LOW;  // Initialisation du niveau d'eau


// Variables pour le délai d'attente de capture après activation de servo
//unsigned long lastPHCaptureTime = 0;
//unsigned long lastTDSCaptureTime = 0;
//unsigned long lastFanActivationTime = 0;
unsigned long lastPumpActivationTime = 0 ;


// Délais entre les captures (en millisecondes)
const unsigned long dhtInterval = 60000;  // 5 minutes pour DHT22 (300000)
const unsigned long dhtInterval_hum = 65000;  // 5 minutes pour DHT22 (300000)
const unsigned long phInterval = 120000;   // 15 minutes pour pH (900000)
const unsigned long ldrInterval = 30000;   // 30 secondes pour LDR (30000)
const unsigned long tdsInterval = 140000;  // 15 minutes pour TDS (900000)
const unsigned long waterInterval = 4000;   // 4 secondes pour le niveau d'eau


// Variables pour les seuils
float temp_min = 15.0;
float temp_max = 25.0;
float ph_min = 5.5;
float ph_max = 7.0;
int tds_min = 600;
int tds_max = 1000;
int humidity_min = 40;
int humidity_max = 80;
int light_min = 120;
int light_max = 200;


// Variables pour l'état automatique ou non des captures et actions
//bool pompeAutomatique = false;  // Variable pour garder la trace si la pompe est activée automatiquement
//bool manual_pump_on = false;   // Variable pour suivre si la pompe est allumée manuellement
bool captureEnCours_pH = false;
bool captureEnCours_TDS = false;
bool captureEnCours_Temp = false;
bool captureEnCours_Luminosite = false;
bool captureEnCours_Hum = false;
bool fanOn = false;
bool pumpOn = false;

// Variable pour le ph
float voltage;
#define slope  6.52 // Pente calculée
#define offset  -0.36 // Offset calculé

// Connexion WiFi et MQTT
const char* ssid = WIFI_SSID;           // défini dans config.h (non versionné)
const char* password = WIFI_PASSWORD;   // défini dans config.h (non versionné)
const char* mqtt_server = MQTT_SERVER;  // défini dans config.h (non versionné)
WiFiClient espClient;
PubSubClient client(espClient);


// Fonctions pour la validation des données
bool validateData(String sensorType, float value) {
  if (sensorType == "pH") {
    // pH entre 0 et 14
    if (isnan(value) || value < 0 || value > 14) {
      Serial.println("Invalid pH value: " + String(value));
      return false;
    }
  } else if (sensorType == "TDS") {
      // TDS : valeur brute du capteur (non convertie en ppm), plage de validation 0-2000
      if (isnan(value) || value < 0 || value > 2000) {
        Serial.println("Invalid TDS value: " + String(value));
        return false;
      }
  } else if (sensorType == "Temperature") {
      // Température entre -40°C et 80°C
      if (isnan(value) || value < -40 || value > 80) {
        Serial.println("Invalid Temperature value: " + String(value));
        return false;
      }
  } else if (sensorType == "Humidity") {
      // Humidité entre 0% et 100%
      if (isnan(value) || value < 0 || value > 100) {
        Serial.println("Invalid Humidity value: " + String(value));
        return false;
      }
  } else if (sensorType == "Luminosity") {
      // La luminosité doit être positive
      if (isnan(value) || value < 0) {
        Serial.println("Invalid Luminosity value: " + String(value));
        return false;
      }
  }
  // Si toutes les vérifications passent, la valeur est valide
  return true;
}


// Connexion au WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print(" Connexion au wifi... ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("Connexion en cours...");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}


// Envoi des données via MQTT avec horodatage
void send_mqtt(const char* topic, String message) {
  if (!client.connected()) return;  // Vérifier la connexion MQTT
  client.publish(topic, message.c_str());
}


// Contrôle du mouvement des servos
void moveServo(Servo &servo, int startPos, int endPos, int delayTime) {
  int step = (startPos < endPos) ? 1 : -1;  // Détermine le sens du mouvement
  int pos = startPos;  
  unsigned long previousMillis = millis();  

  while (pos != endPos + step) {  
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= delayTime) {
      previousMillis = currentMillis;
      servo.write(pos);
      pos += step;
    }
  }
}


// Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Commandes pour ajuster les seuils manuellement
  if (String(topic) == "hydroponics/control/setTempMin") {
    temp_min = message.toFloat();
    send_mqtt("hydroponics/config/temp_min", String(temp_min));
    Serial.println("Température minimale ajustée : " + String(temp_min));
  } else if (String(topic) == "hydroponics/control/setTempMax") {
      temp_max = message.toFloat();
      send_mqtt("hydroponics/config/temp_max", String(temp_max));
      Serial.println("Température maximale ajustée : " + String(temp_max));
  } else if (String(topic) == "hydroponics/control/setPHMin") {
      ph_min = message.toFloat();
      send_mqtt("hydroponics/config/ph_min", String(ph_min));
      Serial.println("pH minimal ajusté : " + String(ph_min));
  } else if (String(topic) == "hydroponics/control/setPHMax") {
      ph_max = message.toFloat();
      send_mqtt("hydroponics/config/ph_max", String(ph_max));
      Serial.println("pH maximal ajusté : " + String(ph_max));
  } else if (String(topic) == "hydroponics/control/setTDSMin") {
      tds_min = message.toFloat();
      send_mqtt("hydroponics/config/tds_min", String(tds_min));
      Serial.println("TDS minimal ajustée : " + String(tds_min));
  } else if (String(topic) == "hydroponics/control/setTDSMax") {
      tds_max = message.toFloat();
      send_mqtt("hydroponics/config/tds_max", String(tds_max ));
      Serial.println("TDS maximale ajustée : " + String(tds_max));
  } else if (String(topic) == "hydroponics/control/setHumidityMin") {
      humidity_min = message.toFloat();
      send_mqtt("hydroponics/config/hum_min", String(humidity_min));
      Serial.println("Humidité minimal ajusté : " + String(humidity_min));
  } else if (String(topic) == "hydroponics/control/setHumidityMax") {
      humidity_max = message.toFloat();
      send_mqtt("hydroponics/config/hum_max", String(humidity_max));
      Serial.println("Humidité maximal ajusté : " + String(humidity_max));
  } else if (String(topic) == "hydroponics/control/setLightMin") {
      light_min = message.toFloat();
      send_mqtt("hydroponics/config/light_min", String(light_min));
      Serial.println("Lumière minimal ajusté : " + String(light_min));
  } else if (String(topic) == "hydroponics/control/setLightMax") {
      light_max = message.toFloat();
      send_mqtt("hydroponics/config/light_max", String(light_max));
      Serial.println("lumière maximal ajusté : " + String(light_max));
  }

  // Activation manuelle de la pompe
  else if (String(topic) == "hydroponics/control/manualPump") {
    if (message == "on") {
      // Vérifier si la pompe n'est pas déjà activée automatiquement
      if (pumpOn == false) {
        // Allumer la pompe manuellement
        pumpOn = true;
        send_mqtt("hydroponics/status/pump", pumpOn ? "true" : "false");
        //manual_pump_on = true;
        digitalWrite(PUMP_PIN, HIGH);  // Allumer la pompe
        Serial.println("Pompe activée manuellement.");
        lastPumpActivationTime = millis();
      } else {
          Serial.println("Pompe déjà activée automatiquement.");
      }
    } else if (message == "off") {
        // Vérifier si la pompe est actuellement allumée (manuellement ou automatiquement)
        if (pumpOn == true) {
          // Éteindre la pompe manuellement
          pumpOn = false;
          send_mqtt("hydroponics/status/pump", pumpOn ? "true" : "false");
          //manual_pump_on = false;
          digitalWrite(PUMP_PIN, LOW);  // Éteindre la pompe
          Serial.println("Pompe désactivée manuellement.");
          lastPumpActivationTime = millis();
       } else {
          Serial.println("Pompe déjà désactivée.");
      }
    }
  }

  // Activation manuelle des capteurs
  else if (String(topic) == "hydroponics/control/manualCapture/ph") {
    if (message == "on"){
        if (!captureEnCours_pH) {  // Vérifier si aucune capture automatique n'est en cours
          Serial.println("Capture manuelle du pH.");
          captureEnCours_pH = true;
          send_mqtt("hydroponics/status/ph", captureEnCours_pH ? "true" : "false");
          // Tourner le servo 1
          moveServo(servo1, 0, 180, 15);
          //lastPHCaptureTime = millis();
          //if (millis() - lastPHCaptureTime > 30000) {  // 2 minutes écoulées 120000
          int rawValue = analogRead(PH_SENSOR_PIN); // Lecture brute
          voltage = rawValue * 3.3 / 4095; // Conversion en tension
          float ph_value = slope * voltage + offset;
          if (!validateData("pH", ph_value)) {
            ph_value = 0;  // Si la valeur est invalide, la mettre à zéro
          }
          send_mqtt("hydroponics/sensors/ph", String(ph_value));
          // Ramener le servo à la position initiale
          moveServo(servo1, 180, 0, 15);
          captureEnCours_pH = false;
          send_mqtt("hydroponics/status/ph", captureEnCours_pH ? "true" : "false");
        //}  
        } else {
            Serial.println("Capture automatique du pH déjà en cours, la capture manuelle est ignorée.");
        }
      }else if (message == "off") {
        // Vérifier si la capture manuelle est en cours
        if (captureEnCours_pH) {
            captureEnCours_pH = false;
            send_mqtt("hydroponics/status/ph", "false");  // Indiquer que la capture est arrêtée
            Serial.println("Capture manuelle du pH désactivée.");
            
            // Ramener le servo à la position initiale
            moveServo(servo1, 180, 0, 15);
        } else {
            Serial.println("Aucune capture manuelle du pH n'est en cours.");
        }
    } 
  } else if (String(topic) == "hydroponics/control/manualCapture/tds") {
      if (message == "on") {
        // Vérifier si aucune capture automatique n'est en cours
        if (!captureEnCours_TDS) {
            Serial.println("Capture manuelle du TDS activée.");
            captureEnCours_TDS = true;
            send_mqtt("hydroponics/status/tds", "true");
            // Tourner le servo 2
            //moveServo(servo2, 0, 180, 15);
            //lastTDSCaptureTime = millis();
            // Délai pour la capture de la valeur du TDS
            //if (millis() - lastTDSCaptureTime > 30000) {
            int tds_value = analogRead(TDS_SENSOR_PIN);
            if (!validateData("TDS", tds_value)) {
                tds_value = 0;
            }
            send_mqtt("hydroponics/sensors/tds", String(tds_value));
            // Ramener le servo à la position initiale
            //moveServo(servo2, 180, 0, 15);
            captureEnCours_TDS = false;
            send_mqtt("hydroponics/status/tds", "false");
            //}
        } else {
            Serial.println("Capture automatique du TDS déjà en cours, la capture manuelle est ignorée.");
        }
    } else if (message == "off") {
        if (captureEnCours_TDS) {
            captureEnCours_TDS = false;
            send_mqtt("hydroponics/status/tds", "false");
            Serial.println("Capture manuelle du TDS désactivée.");
            //moveServo(servo2, 180, 0, 15);
        } else {
            Serial.println("Aucune capture manuelle du TDS n'est en cours.");
        }
    }
  } else if (String(topic) == "hydroponics/control/manualCapture/temp") {
    if (message == "on") {
        if (!captureEnCours_Temp) {
            Serial.println("Capture manuelle de la Température activée.");
            captureEnCours_Temp = true;
            send_mqtt("hydroponics/status/temp", captureEnCours_Temp ? "true" : "false");
            float temp_value = dht.readTemperature();
            if (!validateData("Temperature", temp_value)) {
                temp_value = 0;
            }
            send_mqtt("hydroponics/sensors/temperature", String(temp_value));
            captureEnCours_Temp = false;
            send_mqtt("hydroponics/status/temp", captureEnCours_Temp ? "true" : "false");
        } else {
            Serial.println("Capture automatique de la Température déjà en cours, la capture manuelle est ignorée.");
        }
    }
  } else if (String(topic) == "hydroponics/control/manualCapture/humidity") {
    if (message == "on") {
        if (!captureEnCours_Hum) {
            Serial.println("Capture manuelle de l'humidité activée.");
            captureEnCours_Hum = true;
            send_mqtt("hydroponics/status/hum", captureEnCours_Hum ? "true" : "false");
            float humidity_value = dht.readHumidity();
            if (!validateData("Humidity", humidity_value)) {
                humidity_value = 0;
            }
            send_mqtt("hydroponics/sensors/humidity", String(humidity_value));
            captureEnCours_Hum = false;
            send_mqtt("hydroponics/status/hum", captureEnCours_Hum ? "true" : "false");
        } else {
            Serial.println("Capture automatique de l'humidité déjà en cours, la capture manuelle est ignorée.");
        }
    }
  } else if (String(topic) == "hydroponics/control/manualCapture/light") {
      if (message == "on") {
        if (!captureEnCours_Luminosite) {  // Vérifier si aucune capture automatique n'est en cours
          Serial.println("Capture manuelle de la Luminosité.");
          captureEnCours_Luminosite = true;
          send_mqtt("hydroponics/status/light", captureEnCours_Luminosite ? "true" : "false");
          int   ldrRawData;
          float resistorVoltage, ldrVoltage;
          float ldrResistance;
          float light_value;
          ldrRawData = analogRead(LIGHT_SENSOR_PIN);
          resistorVoltage = (float)ldrRawData / MAX_ADC_READING * ADC_REF_VOLTAGE;
          ldrVoltage = ADC_REF_VOLTAGE - resistorVoltage;
          ldrResistance = ldrVoltage/resistorVoltage * REF_RESISTANCE;
          light_value = LUX_CALC_SCALAR * pow(ldrResistance, LUX_CALC_EXPONENT);
          if (!validateData("Luminosity", light_value)) {
            light_value = 0;  // Si la valeur est invalide, la mettre à zéro
          }
          send_mqtt("hydroponics/sensors/light", String(light_value));
          captureEnCours_Luminosite = false;
          send_mqtt("hydroponics/status/light", captureEnCours_Luminosite ? "true" : "false");
        } else {
            Serial.println("Capture automatique de la luminosité déjà en cours, la capture manuelle est ignorée.");
        }
    }
  }
}


// Configuration de MQTT
void setup_mqtt() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.print("Tentative de connexion à MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("connecté au serveur MQTT");
      client.subscribe("hydroponics/control/setTempMin");
      client.subscribe("hydroponics/control/setTempMax");
      client.subscribe("hydroponics/control/setPHMin");
      client.subscribe("hydroponics/control/setPHMax");
      client.subscribe("hydroponics/control/setTDSMin");
      client.subscribe("hydroponics/control/setTDSMax");
      client.subscribe("hydroponics/control/setHumidityMin");
      client.subscribe("hydroponics/control/setHumidityMax");
      client.subscribe("hydroponics/control/setLightMin");
      client.subscribe("hydroponics/control/setLightMax");
      client.subscribe("hydroponics/control/manualPump");
      client.subscribe("hydroponics/control/manualCapture/ph");
      client.subscribe("hydroponics/control/manualCapture/tds");
      client.subscribe("hydroponics/control/manualCapture/temp");
      client.subscribe("hydroponics/control/manualCapture/humidity");
      client.subscribe("hydroponics/control/manualCapture/light");
    } else {
        Serial.print("failed, code erreur=");
        Serial.print(client.state());
        delay(5000);
    }
  }
}


// Capture des données des capteurs
void capture_ph() {
  if (!captureEnCours_pH) {  // Vérifier si aucune capture manuelle n'est en cours
      Serial.println("Capture automatique du pH.");
      captureEnCours_pH = true;
      send_mqtt("hydroponics/status/ph", captureEnCours_pH ? "true" : "false");
      // Tourner le servo 1
      moveServo(servo1, 0, 180, 15);
      //lastPHCaptureTime = millis();
      //if (millis() - lastPHCaptureTime > 30000) {  // 2 minutes écoulées
      int rawValue = analogRead(PH_SENSOR_PIN); // Lecture brute
      voltage = rawValue * 3.3 / 4095; // Conversion en tension
      float ph_value = slope * voltage + offset;
      if (!validateData("pH", ph_value)) {
        ph_value = 0;  // Si la valeur est invalide, la mettre à zéro
      }
      send_mqtt("hydroponics/sensors/ph", String(ph_value));
      // Ramener le servo à la position initiale
      moveServo(servo1, 180, 0, 15);
      captureEnCours_pH = false;
      send_mqtt("hydroponics/status/ph", captureEnCours_pH ? "true" : "false");
      if (ph_value < ph_min || ph_value > ph_max) {
        digitalWrite(LED1_PIN, HIGH);
      }else{
        digitalWrite(LED1_PIN, LOW);
       }
     //}
    } else {
        Serial.println("Capture manuelle du pH en cours");
    }
}


void capture_tds() {
  if (!captureEnCours_TDS) {  // Vérifier si aucune capture manuelle n'est en cours
      Serial.println("Capture automatique du TDS.");
      captureEnCours_TDS = true;
      send_mqtt("hydroponics/status/tds", captureEnCours_TDS ? "true" : "false");
      // Tourner le servo 2
      //moveServo(servo2, 0, 180, 15);
      //lastTDSCaptureTime = millis();
      //if (millis() - lastTDSCaptureTime > 30000) {
      int tds_value = analogRead(TDS_SENSOR_PIN);
      if (!validateData("TDS", tds_value)) {
        tds_value = 0;  // Si la valeur est invalide, la mettre à zéro
      }
      send_mqtt("hydroponics/sensors/tds", String(tds_value));
      // Ramener le servo à la position initiale
      //moveServo(servo2, 180, 0, 15);
      captureEnCours_TDS = false;
      send_mqtt("hydroponics/status/tds", captureEnCours_TDS ? "true" : "false");
      if (tds_value < tds_min || tds_value > tds_max) {
        digitalWrite(LED2_PIN, HIGH);
      }else{
        digitalWrite(LED2_PIN, LOW);
       }
      //}
    } else {
        Serial.println("Capture manuelle du TDS en cours.");
    }
}


void capture_temperature() {
  captureEnCours_Temp = true;
  send_mqtt("hydroponics/status/temp", captureEnCours_Temp ? "true" : "false");
  float temp_value = dht.readTemperature();
  if (!validateData("Temperature", temp_value)) {
    temp_value = 0;  // Si la valeur est invalide, la mettre à zéro
  }
  send_mqtt("hydroponics/sensors/temperature", String(temp_value));
  captureEnCours_Temp = false;
  send_mqtt("hydroponics/status/temp", captureEnCours_Temp ? "true" : "false");
  if (temp_value < temp_min || temp_value > temp_max) {
    fanOn = true;
    send_mqtt("hydroponics/status/fan", fanOn ? "true" : "false");
    digitalWrite(FAN_PIN,HIGH);
    //lastFanActivationTime = millis();
  }else{
    fanOn = false;
    send_mqtt("hydroponics/status/fan", fanOn ? "true" : "false");
    digitalWrite(FAN_PIN,LOW);
    }
}


void capture_humidity() {
  captureEnCours_Hum = true;
  send_mqtt("hydroponics/status/hum", captureEnCours_Hum ? "true" : "false");
  float humidity_value = dht.readHumidity();
  if (!validateData("Humidity", humidity_value)) {
    humidity_value = 0;  // Si la valeur est invalide, la mettre à zéro
  }
  send_mqtt("hydroponics/sensors/humidity", String(humidity_value));
  captureEnCours_Hum = false;
  send_mqtt("hydroponics/status/hum", captureEnCours_Hum ? "true" : "false");
  if (humidity_value < humidity_min || humidity_value > humidity_max) {
    digitalWrite(LED3_PIN, HIGH);
  }else{
    digitalWrite(LED3_PIN, LOW);
    }
}


void capture_light() {
  captureEnCours_Luminosite = true;
  send_mqtt("hydroponics/status/light", captureEnCours_Luminosite ? "true" : "false");
  int   ldrRawData;
  float resistorVoltage, ldrVoltage;
  float ldrResistance;
  float light_value;
  ldrRawData = analogRead(LIGHT_SENSOR_PIN);
  resistorVoltage = (float)ldrRawData / MAX_ADC_READING * ADC_REF_VOLTAGE;
  ldrVoltage = ADC_REF_VOLTAGE - resistorVoltage;
  ldrResistance = ldrVoltage/resistorVoltage * REF_RESISTANCE;
  light_value = LUX_CALC_SCALAR * pow(ldrResistance, LUX_CALC_EXPONENT);
  if (!validateData("Luminosity", light_value)) {
    light_value = 0;  // Si la valeur est invalide, la mettre à zéro
  }
  send_mqtt("hydroponics/sensors/light", String(light_value));
  captureEnCours_Luminosite = false;
  send_mqtt("hydroponics/status/light", captureEnCours_Luminosite ? "true" : "false");
  if (light_value < light_min || light_value > light_max) {
    digitalWrite(LED4_PIN, HIGH);
  }else{
      digitalWrite(LED4_PIN, LOW);
    }
}


// Automatisation de la pompe
/*void automate_pump() {
  if (digitalRead(WATER_LEVEL_PIN) == LOW) {
    //send_mqtt("hydroponics/actuators/pump", "water level low, pump off");
    digitalWrite(PUMP_PIN, LOW);
    pompeAutomatique = false;  // Désactiver l'automatisme si niveau bas
    Serial.println("Pompe éteinte automatiquement car niveau d'eau bas.");
  } else {
      digitalWrite(PUMP_PIN, HIGH);
      pompeAutomatique = true;
      pumpOn = true;
      send_mqtt("hydroponics/status/pump", pumpOn ? "true" : "false");
      lastPumpActivationTime = millis();
  }
}*/


// Boucle principale
void loop() {
  if (!client.connected()) {
    setup_mqtt();
  }
  client.loop();

   unsigned long now = millis();

   // Lecture du capteur DHT22 (température et humidité)
  if (now - lastDHTRead_temp > dhtInterval) {
    lastDHTRead_temp = now;
    capture_temperature();
  }

  if (now - lastDHTRead_humidity > dhtInterval_hum) {
    lastDHTRead_humidity = now;
    capture_humidity();
  }

  // Lecture du capteur pH
  if (now - lastPHRead > phInterval) {
    lastPHRead = now;
    capture_ph();
  }

  // Lecture du capteur TDS
  if (now - lastTDSRead > tdsInterval) {
    lastTDSRead = now;
    capture_tds();
  }

  // Lecture de la Photocellule LDR (luminosité)
  if (now - lastLDRRead > ldrInterval) {
    lastLDRRead = now;
    capture_light();
  }

  // Lecture du capteur de niveau d'eau à flotteur
  if (now - lastWaterLevelRead > waterInterval) {
    lastWaterLevelRead = now;
    bool currentWaterLevelState = digitalRead(WATER_LEVEL_PIN);
    if (currentWaterLevelState != lastWaterLevelState) {
      lastWaterLevelState = currentWaterLevelState;
      send_mqtt("hydroponics/sensors/water_level", String(currentWaterLevelState));
    }
    // Allumer LED5 si niveau d'eau bas
    if (currentWaterLevelState == LOW) {
      digitalWrite(LED5_PIN, LOW);
    }else{
      digitalWrite(LED5_PIN, HIGH);
    }
  }
  

  // Gérer l'extinction du ventilateur après 5 minutes
  /*if (fanOn && (now - lastFanActivationTime) > 60000) { // 5 minutes 300000
    digitalWrite(FAN_PIN, LOW);
    fanOn = false;
    send_mqtt("hydroponics/status/fan", fanOn ? "true" : "false");
  }*/

  // Activation de la pompe toutes les heures 3600000
  if (!pumpOn && (now - lastPumpActivationTime >= 420000)) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpOn = true;
    send_mqtt("hydroponics/status/pump", pumpOn ? "true" : "false");
    lastPumpActivationTime = now;
  }
  
  // Gérer l'extinction de la pompe après 8 minutes
  if (pumpOn && (now - lastPumpActivationTime >= 60000)) { // 8 minutes 480000
    digitalWrite(PUMP_PIN, LOW);
    pumpOn = false;
    send_mqtt("hydroponics/status/pump", pumpOn ? "true" : "false");
    lastPumpActivationTime = now;
  }
  
}



// Setup initial
void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  pinMode(LED5_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(WATER_LEVEL_PIN, INPUT_PULLUP);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(PH_SENSOR_PIN, INPUT);
  pinMode(TDS_SENSOR_PIN, INPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);

  servo1.setPeriodHertz(50); // Fréquence PWM 
  //servo2.setPeriodHertz(50); // Fréquence PWM 
  servo1.attach(33); // Largeur minimale et maximale de l'impulsion (en µs) pour aller de 0° à 180°
  //servo2.attach(22, 500, 2400);
  
  dht.begin();
  setup_wifi();
  setup_mqtt();

  // Publier l'état initial de certains composants
  send_mqtt("hydroponics/sensors/water_level", String(lastWaterLevelState));
  send_mqtt("hydroponics/status/fan", "false");
  send_mqtt("hydroponics/status/pump", "false");
  send_mqtt("hydroponics/status/ph", "false");
  send_mqtt("hydroponics/status/tds", "false");
  send_mqtt("hydroponics/status/temp", "false");
  send_mqtt("hydroponics/status/hum", "false");
  send_mqtt("hydroponics/status/light", "false");
  send_mqtt("hydroponics/config/temp_min", String(temp_min));
  send_mqtt("hydroponics/config/temp_max", String(temp_max));
  send_mqtt("hydroponics/config/ph_min", String(ph_min));
  send_mqtt("hydroponics/config/ph_max", String(ph_max));
  send_mqtt("hydroponics/config/tds_min", String(tds_min));
  send_mqtt("hydroponics/config/tds_max", String(tds_max ));
  send_mqtt("hydroponics/config/hum_min", String(humidity_min));
  send_mqtt("hydroponics/config/hum_max", String(humidity_max));
  send_mqtt("hydroponics/config/light_min", String(light_min));
  send_mqtt("hydroponics/config/light_max", String(light_max));

}
