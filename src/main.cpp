// Decomment to DEBUG
#define DEBUG_NETATMO
// #define DEBUG_GRID
//#define DEBUG_WIFI
//#define DEBUG_SERIAL
//#define FORCE_NVS

// Customize with your settings
#include "TOCUSTOMIZE.h"

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <MyDumbWifi.h>
#include <NetatmoWeatherAPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TimeLib.h>
#include <math.h>
#include <Preferences.h>


Preferences nvs;
char previous_access_token[58];
char previous_refresh_token[58];

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Org_01.h>

const int PIN_BAT = 35;        // adc for bat voltage
const float VOLTAGE_100 = 4.2; // Full battery curent li-ion
const float VOLTAGE_0 = 3.5;   // Low battery curent li-ion

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16);
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);

int currentLinePos = 0;

int batteryPercentage = 0;
float batteryVoltage = 0.0;

// put function declarations here:
void drawLine(int x0, int y0, int y1, int y2);
void updateBatteryPercentage(int &percentage, float &voltage);
void displayLine(String text);
void displayInfo(NetatmoWeatherAPI myAPI);
int getDataFromAPI(NetatmoWeatherAPI *myAPI);

void goToDeepSleepUntilNextWakeup();
void drawDebugGrid();

void drawLine(int x0, int y0, int x1, int y1)
{
  display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
}
void setup()
{

  setlocale(LC_TIME, "fr_FR.UTF-8");

  Serial.begin(115200);
#ifdef DEBUG_SERIAL
  // time to plug serial
  for (int i = 0; i < 20; i++)
  {
    Serial.println(i);
    delay(1000);
  }
#endif

  Serial.println("Starting...\n");

  // Gathering battery level
  updateBatteryPercentage(batteryPercentage, batteryVoltage);

  // Initialize display
  display.init();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  Serial.println("Starting...");
  Serial.println("MAC Adress:");
  Serial.println(WiFi.macAddress().c_str());
  Serial.println("Battery:");

  char line[24];
  sprintf(line, "%5.3fv (%d%%)", batteryVoltage, batteryPercentage);
  Serial.println(line);
  // Connect to WiFi
  MyDumbWifi mdw;
#ifdef DEBUG_WIFI
  mdw.setDebug(true);
#endif
  if (!mdw.connectToWiFi(wifi_ssid, wifi_key))
  {
    displayLine("Error connecting wifi");
  }
  else
  {
    nvs.begin("netatmoCred", false);
    NetatmoWeatherAPI myAPI;
	
#ifdef FORCE_NVS
    nvs.putString("access_token", access_token);
    nvs.putString("refresh_token", refresh_token);
#endif
    if(!nvs.isKey("access_token") || !nvs.isKey("refresh_token")) {
      Serial.println("NVS : init namespace");
      nvs.putString("access_token", access_token);
      nvs.putString("refresh_token", refresh_token);
    } else {
      Serial.println("NVS : get value from namespace");
      nvs.getString("access_token", access_token, 58);
      nvs.getString("refresh_token", refresh_token, 58);     
    }
    Serial.print("access_token : ");
    Serial.println(access_token);
    Serial.print("refresh_token : ");
    Serial.println(refresh_token); 
    memcpy(previous_access_token, access_token, 58);
    memcpy(previous_refresh_token, refresh_token, 58); 

    
    #ifdef DEBUG_NETATMO
      myAPI.setDebug(true);
    #endif
    int res = getDataFromAPI(&myAPI); 
    if (res == VALID_ACCESS_TOKEN)
    {
      Serial.println("Start display");
#ifdef DEBUG_GRID
      drawDebugGrid();
#endif
      displayInfo(myAPI);
    } else {
      displayLine("API Error");
      switch (res)
        {
          case 3:
            displayLine("Expired Token");
          break;
          case 2:
            displayLine("Invalid Token");
          break;
          case 1:
            displayLine("Other");
          break;
          case 0:
            displayLine("Ok ? WTF ?");
          break;
        }
        displayLine("Msg " + myAPI.errorMessage);
        displayLine("LastBody " + myAPI.lastBody);
    }    
  }
  display.update();

  goToDeepSleepUntilNextWakeup();
}

void updateBatteryPercentage(int &percentage, float &voltage)
{
  // Lire la tension de la batterie
  voltage = analogRead(PIN_BAT) / 4096.0 * 7.05;
  percentage = 0;
  if (voltage > 1)
  { // Afficher uniquement si la lecture est valide
    percentage = static_cast<int>(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
    // Ajuster le pourcentage en fonction des seuils de tension
    if (voltage >= VOLTAGE_100)
    {
      percentage = 100;
    }
    else if (voltage <= VOLTAGE_0)
    {
      percentage = 0;
    }
  }
}


int getDataFromAPI(NetatmoWeatherAPI *myAPI) {
  #ifdef DEBUG_NETATMO
      myAPI->setDebug(true);
    #endif

    int result = myAPI->getStationsData(access_token, device_id, DELAYUTC_YOURTIMEZONE); 

    if (result == EXPIRED_ACCESS_TOKEN || result == INVALID_ACCESS_TOKEN) {
      if (myAPI->getRefreshToken(access_token, refresh_token, client_secret, client_id))
      {
        
        // compare cred with previous
        if(strncmp(previous_access_token, access_token, 58) != 0){
          nvs.putString("access_token", access_token);
          Serial.println("NVS : access_token updated");   
        } else {
          Serial.println("NVS : same access_token");
          
        }

        if(strncmp(previous_refresh_token, refresh_token, 58) != 0){
          nvs.putString("refresh_token", refresh_token);
          Serial.println("NVS : refresh_token updated");
            
        } else {
          Serial.println("NVS : same refresh_token");
          
        }
        result = myAPI->getStationsData(access_token, device_id, DELAYUTC_YOURTIMEZONE);
        
      }
    }
    return result;
}


void displayLine(String text)
{
  if (currentLinePos > 150)
  {
    currentLinePos = 0;
    display.fillScreen(GxEPD_WHITE);
  }
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, currentLinePos);
  display.print(text);
  currentLinePos += 10;
}

void drawBatteryLevel(int batteryTopLeftX, int batteryTopLeftY, int percentage)
{
  // Draw battery Level
  const int nbBars = 4;
  const int barWidth = 3;
  const int batteryWidth = (barWidth + 1) * nbBars + 2;
  const int barHeight = 4;
  const int batteryHeight = barHeight + 4;

  // Horizontal
  drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY);
  drawLine(batteryTopLeftX, batteryTopLeftY + batteryHeight, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight);
  // Vertical
  drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX, batteryTopLeftY + batteryHeight);
  drawLine(batteryTopLeftX + batteryWidth, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight);
  // + Pole
  drawLine(batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + (batteryHeight - 1));
  drawLine(batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + (batteryHeight - 1));

  int i, j;
  int nbBarsToDraw = round(percentage / 25.0);
  for (j = 0; j < nbBarsToDraw; j++)
  {
    for (i = 0; i < barWidth; i++)
    {
      drawLine(batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2, batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2 + barHeight);
    }
  }
}

void displayModule(module_struct module, int y)
{
  // 250 x 122
  const int rectWidth = 118;
  const int rectHeight = 45;
  const int borderRadius = 8;
  const int nameOffsetX = 10;
  const int nameOffsetY = 17;
  const int tempOffsetX = 55;
  const int tempOffsetY = 40;

  const int maxTempOffsetX = 15;
  const int maxTempOffsetY = 25;

  const int minTempOffsetX = 15;
  const int minTempOffsetY = 32;

  const int timeupdateOffsetX = 15;
  const int timeupdateOffsetY = 39;

  display.drawRoundRect(2, y, rectWidth, rectHeight, borderRadius, GxEPD_BLACK);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(nameOffsetX, y + nameOffsetY);
  display.print(module.name);

  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(tempOffsetX, y + tempOffsetY);
  display.print(module.temperature);

  display.setFont(&Org_01);
  display.setCursor(minTempOffsetX, y + minTempOffsetY);
  display.print(module.min);

  display.setCursor(maxTempOffsetX, y + maxTempOffsetY);
  display.print(module.max);

  display.setCursor(timeupdateOffsetX, y + timeupdateOffsetY);
  char line[9];
  sprintf(line, "%02d:%02d", hour(module.timeupdate), minute(module.timeupdate));
  display.print(line);

  if (module.trend == "up")
  {
    display.fillTriangle(35, y + 31, 51, y + 31, 43, y + 21, GxEPD_BLACK); // x1,y1,x2,y2,x3,y3,color
  }

  if (module.trend == "down")
  {
    display.fillTriangle(35, y + 21, 51, y + 21, 43, y + 31, GxEPD_BLACK); // x1,y1,x2,y2,x3,y3,color
  }
}

void displayInfo(NetatmoWeatherAPI myAPI)
{
  myAPI.dumpModule(myAPI.NAMain);
        
  displayModule(myAPI.NAMain, 0);
  // esp32 batterie level
  drawBatteryLevel(90, 5, batteryPercentage);

  displayModule(myAPI.NAModule1, 50);
  drawBatteryLevel(90, 55, myAPI.NAModule1.battery_percent);

  displayModule(myAPI.NAModule4[0], 100);
  drawBatteryLevel(90, 105, myAPI.NAModule4[0].battery_percent);

  displayModule(myAPI.NAModule4[1], 150);
  drawBatteryLevel(90, 155, myAPI.NAModule4[1].battery_percent);

  displayModule(myAPI.NAModule4[2], 200);
  drawBatteryLevel(90, 205, myAPI.NAModule4[2].battery_percent);
}

void drawDebugGrid()
{
  int gridSpacing = 10; // Espacement entre les lignes de la grille
  int screenWidth = 122;
  int screenHeight = 250;

  Serial.print("Width : ");
  Serial.print(screenWidth);
  Serial.print(" Eight : ");
  Serial.println(screenHeight);

  // Dessiner des lignes verticales
  for (int x = 0; x <= screenWidth; x += gridSpacing)
  {
    drawLine(x, 0, x, screenHeight);
  }

  // Dessiner des lignes horizontales
  for (int y = 0; y <= screenHeight; y += gridSpacing)
  {
    drawLine(0, y, screenWidth, y);
  }
}

void goToDeepSleepUntilNextWakeup()
{
  time_t sleepDuration = WAKEUP_INTERVAL;
  Serial.print("Sleeping duration (seconds): ");
  Serial.println(sleepDuration);

  // Configure wake up
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void loop()
{
  // put your main code here, to run repeatedly:
}

