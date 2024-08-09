#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>


const char WIFI_SSID[] = "AAAAAAAAAA!!";
const char WIFI_PASSWORD[] = "Fuckyou!";

String HOST_NAME   = "https://waterservices.usgs.gov";
String PATH_NAME   = "/nwis/iv/?format=json&sites=02335880&siteStatus=all";



void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.println("Starting up");

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  HTTPClient http;

  http.begin(HOST_NAME + PATH_NAME); //HTTP
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  
}

void loop() {
  // put your main code here, to run repeatedly:
}

