#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <stdlib.h>


const char WIFI_SSID[] = "AAAAAAAAAA!!";
const char WIFI_PASSWORD[] = "Fuckyou!";

String HOST_NAME   = "https://waterservices.usgs.gov";
String PATH_NAME   = "/nwis/iv/?format=json&sites=02335880&siteStatus=all";

JsonDocument doc;


#include <SPI.h>

#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define TFT_GREY 0x5AEB

float ltx = 0;    // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update

int old_analog =  -999; // Value last displayed
int old_digital = -999; // Value last displayed

int value[6] = {0, 0, 0, 0, 0, 0};
int old_value[6] = { -1, -1, -1, -1, -1, -1};
int d = 0;


// #########################################################################
// Update needle position
// This function is blocking while needle moves, time depends on ms_delay
// 10ms minimises needle flicker if text is drawn within needle sweep area
// Smaller values OK if text not in sweep area, zero for instant movement but
// does not look realistic... (note: 100 increments for full scale deflection)
// #########################################################################
void plotNeedle(int value, byte ms_delay)
{
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  char buf[8]; dtostrf(value, 4, 0, buf);
  tft.drawRightString(buf, 80, 119 - 20, 2);

  if (value < -10) value = -10; // Limit value to emulate needle end stops
  if (value > 110) value = 110;

  // Move the needle util new value reached
  while (!(value == old_analog)) {
    if (old_analog < value) old_analog++;
    else old_analog--;

    if (ms_delay == 0) old_analog = value; // Update immediately id delay is 0

    float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
    // Calculate tip of needle coords
    float sx = cos(sdeg * 0.0174532925);
    float sy = sin(sdeg * 0.0174532925);

    // Calculate x delta of needle start (does not start at pivot point)
    float tx = tan((sdeg + 90) * 0.0174532925);

    // Erase old needle image
    tft.drawLine(120 + 20 * ltx - 1, 140 - 20, osx - 1, osy, TFT_WHITE);
    tft.drawLine(120 + 20 * ltx, 140 - 20, osx, osy, TFT_WHITE);
    tft.drawLine(120 + 20 * ltx + 1, 140 - 20, osx + 1, osy, TFT_WHITE);

    // Re-plot text under needle
    tft.setTextColor(TFT_BLACK);
    tft.drawCentreString("%RH", 120, 70, 4); // // Comment out to avoid font 4

    // Store new needle end coords for next erase
    ltx = tx;
    osx = sx * 98 + 120;
    osy = sy * 98 + 140;

    // Draw the needle in the new postion, magenta makes needle a bit bolder
    // draws 3 lines to thicken needle
    tft.drawLine(120 + 20 * ltx - 1, 140 - 20, osx - 1, osy, TFT_RED);
    tft.drawLine(120 + 20 * ltx, 140 - 20, osx, osy, TFT_MAGENTA);
    tft.drawLine(120 + 20 * ltx + 1, 140 - 20, osx + 1, osy, TFT_RED);

    // Slow needle down slightly as it approaches new postion
    if (abs(old_analog - value) < 10) ms_delay += ms_delay / 5;

    // Wait before next update
    delay(ms_delay);
  }
}

// #########################################################################
//  Draw the analogue meter on the screen
// #########################################################################
void analogMeter()
{
  // Meter outline
  tft.fillRect(0, 0, 479, 126, TFT_GREY);
  tft.fillRect(5, 3, 230, 119, TFT_WHITE);

  tft.setTextColor(TFT_BLACK);  // Text colour

  // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
  for (int i = -50; i < 51; i += 5) {
    // Long scale tick length
    int tl = 15;

    // Coordinates of tick to draw
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (100 + tl) + 120;
    uint16_t y0 = sy * (100 + tl) + 140;
    uint16_t x1 = sx * 100 + 120;
    uint16_t y1 = sy * 100 + 140;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * 0.0174532925);
    float sy2 = sin((i + 5 - 90) * 0.0174532925);
    int x2 = sx2 * (100 + tl) + 120;
    int y2 = sy2 * (100 + tl) + 140;
    int x3 = sx2 * 100 + 120;
    int y3 = sy2 * 100 + 140;

    // Yellow zone limits
    //if (i >= -50 && i < 0) {
    //  tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_YELLOW);
    //  tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    //}

    // Green zone limits
    if (i >= 0 && i < 25) {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }

    // Orange zone limits
    if (i >= 25 && i < 50) {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_ORANGE);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_ORANGE);
    }

    // Short scale tick length
    if (i % 25 != 0) tl = 8;

    // Recalculate coords incase tick lenght changed
    x0 = sx * (100 + tl) + 120;
    y0 = sy * (100 + tl) + 140;
    x1 = sx * 100 + 120;
    y1 = sy * 100 + 140;

    // Draw tick
    tft.drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Check if labels should be drawn, with position tweaks
    if (i % 25 == 0) {
      // Calculate label positions
      x0 = sx * (100 + tl + 10) + 120;
      y0 = sy * (100 + tl + 10) + 140;
      switch (i / 25) {
        case -2: tft.drawCentreString("0", x0, y0 - 12, 2); break;
        case -1: tft.drawCentreString("25", x0, y0 - 9, 2); break;
        case 0: tft.drawCentreString("50", x0, y0 - 6, 2); break;
        case 1: tft.drawCentreString("75", x0, y0 - 9, 2); break;
        case 2: tft.drawCentreString("100", x0, y0 - 12, 2); break;
      }
    }

    // Now draw the arc of the scale
    sx = cos((i + 5 - 90) * 0.0174532925);
    sy = sin((i + 5 - 90) * 0.0174532925);
    x0 = sx * 100 + 120;
    y0 = sy * 100 + 140;
    // Draw scale arc, don't draw the last part
    if (i < 50) tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
  }

  tft.drawString("%RH", 5 + 230 - 40, 119 - 20, 2); // Units at bottom right
  tft.drawCentreString("%RH", 120, 70, 4); // Comment out to avoid font 4
  tft.drawRect(5, 3, 230, 119, TFT_BLACK); // Draw bezel line

  plotNeedle(0, 0); // Put meter needle at 0
}



// #########################################################################
//  Draw a linear meter on the screen
// #########################################################################
void plotLinear(char *label, int x, int y)
{
  int w = 72;
  tft.drawRect(x, y, w, 155, TFT_GREY);
  tft.fillRect(x+2, y + 19, w-3, 155 - 38, TFT_WHITE);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString(label, x + w / 2, y + 2, 2);

  for (int i = 0; i < 110; i += 10)
  {
    tft.drawFastHLine(x + 20, y + 27 + i, 6, TFT_BLACK);
  }

  for (int i = 0; i < 110; i += 50)
  {
    tft.drawFastHLine(x + 20, y + 27 + i, 9, TFT_BLACK);
  }
  
  tft.fillTriangle(x+3, y + 127, x+3+16, y+127, x + 3, y + 127 - 5, TFT_RED);
  tft.fillTriangle(x+3, y + 127, x+3+16, y+127, x + 3, y + 127 + 5, TFT_RED);
  
  tft.drawCentreString("---", x + w / 2, y + 155 - 18, 2);
}

// #########################################################################
//  Adjust 6 linear meter pointer positions
// #########################################################################
void plotPointer(void)
{
  int dy = 187;
  byte pw = 16;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  // Move the 6 pointers one pixel towards new value
  for (int i = 0; i < 4; i++)
  {
    char buf[8]; dtostrf(value[i], 4, 0, buf);
    tft.drawRightString(buf, i * 80 + 72 - 5, 187 - 27 + 155 - 18, 2);

    int dx = 3 + 80 * i;
    if (value[i] < 0) value[i] = 0; // Limit value to emulate needle end stops
    if (value[i] > 100) value[i] = 100;

    while (!(value[i] == old_value[i])) {
      dy = 187 + 100 - old_value[i];
      if (old_value[i] > value[i])
      {
        tft.drawLine(dx, dy - 5, dx + pw, dy, TFT_WHITE);
        old_value[i]--;
        tft.drawLine(dx, dy + 6, dx + pw, dy + 1, TFT_RED);
      }
      else
      {
        tft.drawLine(dx, dy + 5, dx + pw, dy, TFT_WHITE);
        old_value[i]++;
        tft.drawLine(dx, dy - 6, dx + pw, dy - 1, TFT_RED);
      }
    }
  }
}


const char* Temp;
const char* Flowrate;
const char* Turbidity;
const char* E_Coli;


void TitleBox() {
  tft.setTextColor(TFT_PINK, TFT_BLACK);
  tft.setCursor(270,40,4);
  tft.println("Welcome to the");
  tft.setCursor(270,65,4);
  tft.println("Chattahoochee");
  tft.setCursor(270,90,4);
  tft.println("Scanner!");

  // String title = "Welcome to Chattahooche Scanner";
  // tft.drawString(title,300,80,3);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.println("Starting up");


  

  updateTime = millis(); // Next update time
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
      // Serial.println(payload);

      DeserializationError error = deserializeJson(doc,payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }

      JsonArray value_timeSeries = doc["value"]["timeSeries"];

      JsonObject value_timeSeries_0 = value_timeSeries[0];
      // JsonObject value_timeSeries_0_variable = value_timeSeries_0["variable"];
      JsonObject value_timeSeries_0_values_0 = value_timeSeries_0["values"][0];
      JsonObject value_timeSeries_0_values_0_value_0 = value_timeSeries_0_values_0["value"][0];
      Temp = value_timeSeries_0_values_0_value_0["value"];

      JsonObject value_timeSeries_2 = value_timeSeries[2];
      JsonObject value_timeSeries_2_values_0 = value_timeSeries_2["values"][0];
      JsonObject value_timeSeries_2_values_0_value_0 = value_timeSeries_2_values_0["value"][0];
      Flowrate = value_timeSeries_2_values_0_value_0["value"];

      JsonObject value_timeSeries_5 = value_timeSeries[5];
      JsonObject value_timeSeries_5_values_0 = value_timeSeries_5["values"][0];
      JsonObject value_timeSeries_5_values_0_value_0 = value_timeSeries_5_values_0["value"][0];
      Turbidity = value_timeSeries_5_values_0_value_0["value"];

      JsonObject value_timeSeries_6 = value_timeSeries[6];
      JsonObject value_timeSeries_6_values_0 = value_timeSeries_6["values"][0];
      JsonObject value_timeSeries_6_values_0_value_0 = value_timeSeries_6_values_0["value"][0];
      E_Coli = value_timeSeries_6_values_0_value_0["value"];


      Serial.printf("Temp: %s\n", Temp);
      Serial.printf("Flowrate: %s\n", Flowrate);
      Serial.printf("Turbidity: %s\n", Turbidity);
      Serial.printf("E.Coli: %s\n", E_Coli);


    } else {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();


  tft.init();
  tft.setRotation(1);
  Serial.begin(57600); // For debug
  tft.fillScreen(TFT_BLACK);

  analogMeter(); // Draw analogue meter
  TitleBox();

  // Draw 6 linear meters
  byte d = 80;
  plotLinear("Temp", 0, 160);
  plotLinear("Flow", 1 * d, 160);
  plotLinear("Turb", 2 * d, 160);
  plotLinear("E.Coli", 3 * d, 160);
  
  
}

void loop() {
  if (updateTime <= millis()) {
    updateTime = millis() + 25; // Delay to limit speed of update
 
    d += 4; if (d >= 360) d = 0;

    //value[0] = map(analogRead(A0), 0, 1023, 0, 100); // Test with value form Analogue 0
    //value[1] = map(analogRead(A1), 0, 1023, 0, 100); // Test with value form Analogue 1
    //value[2] = map(analogRead(A2), 0, 1023, 0, 100); // Test with value form Analogue 2
    //value[3] = map(analogRead(A3), 0, 1023, 0, 100); // Test with value form Analogue 3
    //value[4] = map(analogRead(A4), 0, 1023, 0, 100); // Test with value form Analogue 4
    //value[5] = map(analogRead(A5), 0, 1023, 0, 100); // Test with value form Analogue 5

    // Create a Sine wave for testing
    // value[0] = 50 + 50 * sin((d + 0) * 0.0174532925);
    // value[1] = 50 + 50 * sin((d + 60) * 0.0174532925);
    // value[2] = 50 + 50 * sin((d + 120) * 0.0174532925);
    // value[3] = 50 + 50 * sin((d + 180) * 0.0174532925);
    // value[4] = 50 + 50 * sin((d + 240) * 0.0174532925);
    // value[5] = 50 + 50 * sin((d + 300) * 0.0174532925);


    value[0] = atoi(Temp);
    value[1] = atoi(Flowrate);
    value[2] = atoi(Turbidity);
    value[3] = atoi(E_Coli);
    // value[4] = 50 + 50 * sin((d + 240) * 0.0174532925);
    // value[5] = 50 + 50 * sin((d + 300) * 0.0174532925);
    
    //unsigned long t = millis(); 
    plotPointer(); // It takes aout 3.5ms to plot each gauge for a 1 pixel move, 21ms for 6 gauges
     
    plotNeedle(value[0], 0); // It takes between 2 and 12ms to replot the needle with zero delay
    //Serial.println(millis()-t); // Print time taken for meter update
  }
}

