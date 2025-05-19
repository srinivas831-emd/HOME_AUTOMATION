#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>



//DEFINATION OF PINS
#define ONE_WIRE_BUS D6  
#define DOOR_PIN D5  
#define AOUT_PIN A0 
#define BUZZER_PIN D7


//WIFI CREDENTIALS
const char* ssid = "S2VAUTOMATION_1"; //--> Your wifi name or SSID.
const char* password = "S2v12345"; //--> Your wifi password.
 
 
//HOST & HTTPSPORTS
const char* host = "script.google.com";
const int httpsPort = 443;
String GAS_ID = "AKfycbypPaCgJvl-d5FC7wWSQfte3lLFzN9ex_tJpaXCARa2mcG7FPc5njIK1oiHnUBsrivQ";
const char* path = "/macros/s/AKfycbypPaCgJvl-d5FC7wWSQfte3lLFzN9ex_tJpaXCARa2mcG7FPc5njIK1oiHnUBsrivQ/exec";


WiFiClientSecure client; //Create a WiFiClientSecure object.

 

//DATA FROM GOOGLE_SHEET
struct SheetValues
{
  int A2;
  int B2;
  int C2;
  bool success;
}values;


//DATA TO GOOGLE_SHEET
struct data
{
  unsigned long previousReadMillis = 0;
  unsigned long previousSendMillis = 0;
  char door_condition[16];
  unsigned long previousMillis = 0;
  float light_sensor;
  float temperature_sensor;
  float moisture_sensor;
}d;


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
BH1750 lightMeter;
bool int_flag = 0;//FLAG USED FOR INTERRUPT 


// FUNCTION PROTOTYPE OR DECLARATION
SheetValues readGoogleSheet();
float light_intensity();
float temperature();
void door_status();
float moisture();
void collect_data();
void sendData();
void read();
void IRAM_ATTR handleInterrupt();
void buzzer();
void sendDoorData();
void sendTemperatureData();


// FUNCTION FOR READING FROM DATA GOOGLE_SHEET
SheetValues readGoogleSheet()
{
  SheetValues result = {0, 0, false};
  
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected");
    return result;
  }

  String url = "https://" + String(host) + String(path);
  Serial.println("Requesting URL: " + url);
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) 
  {
    String payload = http.getString();
    Serial.println("Received payload:");
    Serial.println(payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) 
    {
      result.A2 = doc["A2"];
      result.B2 = doc["B2"];
      result.C2 = doc["C2"];
      result.success = true;
    } 
    else 
    {
      Serial.print("JSON error: ");
      Serial.println(error.c_str());
    }
  } 
  else 
  {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return result;
}


//FUNCTION FOR ASSIGNING VALUE TO VARIABLES RECEIVED FOR GOOGLE_SHEET
void read()
{
  values = readGoogleSheet();
  
  if (values.success) 
  {
    Serial.print("A2: "); Serial.println(values.A2);
    Serial.print("B2: "); Serial.println(values.B2);
    Serial.print("C2: "); Serial.println(values.C2);

  } else 
  {
    Serial.println("Failed to read from Google Sheet");
  }
}


 //FUNCTION FOR LIGHT SENSOR 
float light_intensity()
{
  float lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  return lux;
}


//FUNCTION FOR TEMPERATURE SENSOR
float temperature()
{
  float tempC = 0;
  sensors.requestTemperatures();
  tempC = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");
  return tempC;
}


//FUNCTION FOR DOOR SENSOR
void door_status()
{
  int doorState = digitalRead(DOOR_PIN);  // LOW = closed, HIGH = open

  if (doorState == LOW) 
  {
    Serial.println("Door is CLOSED");
    strcpy(d.door_condition,"CLOSED");
  } 
  else if (doorState == HIGH) 
  {
    Serial.println("Door is OPEN");
    strcpy(d.door_condition,"OPEN");
  }
}


//FUNCTION FOR SOIL MOISTURE SENSOR
float moisture()
{
  float mos = analogRead(AOUT_PIN);
  Serial.print("moisture: ");
  Serial.println(mos);
  return mos;
}


//FUNCTION FOR COLLECTING DATA FROM SENSOR
void collect_data()
{
  d.temperature_sensor=temperature();
  d.moisture_sensor=moisture();
  d.light_sensor=light_intensity();
  door_status();
  buzzer();
}


//FUNCTION FOR SENDIND SENSOR DATA TO GOOGLE_SHEET
void sendData() 
{
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);
  
  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) 
  {
    Serial.println("connection failed");
    return;
  }
  //----------------------------------------
 
  //----------------------------------------Processing data and sending data
  String string_temperature =  String(d.temperature_sensor,2);
  // String string_temperature =  String(tem, DEC); 
  String string_moisture =  String(d.moisture_sensor, 2);
  String string_light =  String(d.light_sensor, 2); 
  buzzer();
  
  String url = "/macros/s/" + GAS_ID + "/exec?Temperature=" + string_temperature + "&Soil_Moisture=" + string_moisture + "&Light_Intensity=" + string_light + "&Door_Condition=" + d.door_condition ;
  Serial.print("requesting URL: ");
  Serial.println(url);
 
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");
 
  Serial.println("request sent");
  //----------------------------------------
 
  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) 
  {
    String line = client.readStringUntil('\n');
    if (line == "\r") 
    {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
  
} 

//FUNCTION FOR SENDIND DOOR DATA
void sendDoorData()
{
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);

  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) 
  {
    Serial.println("connection failed");
    return;
  }
  //----------------------------------------
  
  String url = "/macros/s/" + GAS_ID + "/exec?Door_Condition=" + d.door_condition ;
  Serial.print("requesting URL: ");
  Serial.println(url);
 
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");
 
  Serial.println("request sent");
  //----------------------------------------
 
  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) 
  {
    String line = client.readStringUntil('\n');
    if (line == "\r") 
    {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
}


//FUNCTION FOR SENDIND TEMPERATURE DATA
void sendTemperatureData()
{
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);
  
  
  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) 
  {
    Serial.println("connection failed");
    return;
  }
  //----------------------------------------
  String string_temperature =  String(d.temperature_sensor,2);
  String url = "/macros/s/" + GAS_ID + "/exec?Temperature=" + string_temperature;
  Serial.print("requesting URL: ");
  Serial.println(url);
 
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");
 
  Serial.println("request sent");
  //----------------------------------------
 
  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) 
  {
    String line = client.readStringUntil('\n');
    if (line == "\r") 
    {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
}


//FUNCTION FOR BUZZER
void buzzer()
{
  if(d.temperature_sensor >= values.A2)
  {
    sendTemperatureData();
    digitalWrite(BUZZER_PIN, HIGH);  // Turn ON once
    delay(values.B2 * 1000);       // Wait for full duration
    digitalWrite(BUZZER_PIN, LOW);   // Turn OFF
  }
}


//INTERRUPT SERVICE ROUTINE
void IRAM_ATTR handleInterrupt() 
{
    int_flag = 1; 
}



void setup() 
{
  Serial.begin(9600);
  pinMode(BUZZER_PIN,OUTPUT);
  attachInterrupt(DOOR_PIN, handleInterrupt, CHANGE); // SSetting up interrupt
  Wire.begin(D2, D1); // SDA, SCL
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  sensors.begin();
  delay(500);
  pinMode(DOOR_PIN, INPUT_PULLUP);  // Enable internal pull-up resistor
  delay(500);
  WiFi.begin(ssid, password); //--> Connect to your WiFi router
  Serial.println("");
  Serial.print("Connecting");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) 
  {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    client.setInsecure();
    
    // Immediate first transmission
    read();
    collect_data();
    sendData();
    
    // Set previousMillis to now to properly time the next interval
    d.previousMillis = millis();
  } 
  else 
  {
    Serial.println("\nFailed to connect to WiFi");
  }
}


void loop()
 {
  unsigned long currentMillis = millis();
  unsigned long readInterval = 3000;          // 3 seconds for reading
  unsigned long sendInterval = values.C2 * 60 * 1000;  // C2 minutes for sending
  
  // Handle interrupt flag (highest priority)
  if (int_flag == 1) 
  {
    int_flag = 0;
    door_status();
    sendDoorData();
  }

  // Independent timing for reading/collecting data
  if (currentMillis - d.previousReadMillis >= readInterval) 
  {
    d.previousReadMillis = currentMillis;  // Reset the timer
    read();
    collect_data();
  }  

  // Independent timing for sending data
  if (currentMillis - d.previousSendMillis >= sendInterval) 
  {
    d.previousSendMillis = currentMillis;  // Reset the timer
    sendData();
  }    
} 
 
