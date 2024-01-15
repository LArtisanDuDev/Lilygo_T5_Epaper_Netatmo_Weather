// Decomment to DEBUG
//#define DEBUG_NETATMO
//#define DEBUG_GRID

// Customize with your settings
#include "TOCUSTOMIZE.h"

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h> 
#include <TimeLib.h>
#include <math.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Org_01.h>

const int   PIN_BAT           = 35;      //adc for bat voltage
const float VOLTAGE_100       = 4.2;     // Full battery curent li-ion
const float VOLTAGE_0         = 3.5;     // Low battery curent li-ion

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16);
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);

int currentLinePos = 0;

int batteryPercentage = 0;
float batteryVoltage = 0.0;

struct module_struct {
  String name = "";
  String temperature = "";
  String min = "";
  String max = "";
  String trend = "";
  unsigned long timemin = 0;
  unsigned long timemax = 0;
  unsigned long timeupdate = 0;
} NAMain, NAModule1, NAModule4[3];

// put function declarations here:
bool connectToWiFi();
void updateBatteryPercentage( int &percentage, float &voltage );
void displayLine(String text);
void displayInfo();
bool getStationsData();
void goToDeepSleepUntilNextWakeup();
bool getRefreshToken();
void drawDebugGrid();

void setup() {

    setlocale(LC_TIME, "fr_FR.UTF-8");

    Serial.begin(115200);
    Serial.println("Starting...\n");

    // Gathering battery level
    updateBatteryPercentage(batteryPercentage, batteryVoltage) ;
  
    // Initialize display
    display.init();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    Serial.println("Starting...");
    Serial.println("MAC Adress:");
    Serial.println(WiFi.macAddress().c_str());
    Serial.println("Battery:");
    char line[24];
    sprintf(line, "%5.3fv (%d%%)",batteryVoltage,batteryPercentage);
    Serial.println(line);
    
    // Connecter au WiFi
    if (!connectToWiFi()) {
      displayLine("Error connecting wifi");  
      display.update();
    } else {
    
      // Gathering Netatmo datas
      if (getRefreshToken()) {
        if(getStationsData()) {
          Serial.println("Start display");
          display.fillScreen(GxEPD_WHITE);
#ifdef DEBUG_GRID
          drawDebugGrid();
#endif
          displayInfo();
          display.update();
        } else {
          displayLine("GetStationsData Error");  
          display.update();
        }
      } else {
        displayLine("Refresh token Error");  
        display.update();
      }
    }
    
    goToDeepSleepUntilNextWakeup();
}

bool connectToWiFi()
{

  // WiFi setup
  WiFi.mode(WIFI_STA);

  // SSID & Password are in toCustomize.h
  WiFi.begin(wifi_ssid, wifi_key);

  int max_try = 0;
  while (WiFi.status() != WL_CONNECTED && max_try < 10) {
    Serial.print(".");
    delay(3000);
    max_try++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected");
    return true;
  } else {
    Serial.println("Error connecting WiFi.");
    return false;
  }
}

void updateBatteryPercentage( int &percentage, float &voltage ) {
  // Lire la tension de la batterie
  voltage = analogRead(PIN_BAT) / 4096.0 * 7.05;
  percentage = 0;
  if (voltage > 1) { // Afficher uniquement si la lecture est valide
      percentage = static_cast<int>(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
      // Ajuster le pourcentage en fonction des seuils de tension
      if (voltage >= VOLTAGE_100) {
          percentage = 100;
      } else if (voltage <= VOLTAGE_0) {
          percentage = 0;
      }
  }
}

void displayLine(String text)
{
  if (currentLinePos > 150) {
      currentLinePos = 0;
      display.fillScreen(GxEPD_WHITE);
  }
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10,currentLinePos);
  display.print(text);
  currentLinePos += 10; 
}

void drawBatteryLevel(int batteryTopLeftX, int batteryTopLeftY)
{
  const int leftMargin = 2;

  // Draw battery Level
  const int batteryTopMargin = 10;
  const int nbBars = 4;
  const int barWidth = 3;
  const int batteryWidth = (barWidth + 1) * nbBars + 2;
  const int barHeight = 4;
  const int batteryHeight = barHeight + 4;

  // Horizontal
  display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY, GxEPD_BLACK);
  display.drawLine(batteryTopLeftX, batteryTopLeftY + batteryHeight, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  // Vertical
  display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  display.drawLine(batteryTopLeftX + batteryWidth, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  // + Pole
  display.drawLine(batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);
  display.drawLine(batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);

  int i, j;
  int nbBarsToDraw = round(batteryPercentage / 25.0);
  for (j = 0; j < nbBarsToDraw; j++) {
    for (i = 0; i < barWidth; i++) {
      display.drawLine(batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2, batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2 + barHeight, GxEPD_BLACK);
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

  if (module.trend == "up") {
    display.fillTriangle(35, y + 31, 51, y + 31, 43, y + 21, GxEPD_BLACK); // x1,y1,x2,y2,x3,y3,color
  }

  if (module.trend == "down") {
    display.fillTriangle(35, y + 21, 51, y + 21, 43, y + 31, GxEPD_BLACK); // x1,y1,x2,y2,x3,y3,color
  }
}

void displayInfo() {
    displayModule(NAMain,0);
    displayModule(NAModule1,50);
    displayModule(NAModule4[0],100);
    displayModule(NAModule4[1],150);
    displayModule(NAModule4[2],200);
    drawBatteryLevel(90,5);
}

void dumpModule(module_struct module) {
    Serial.println("Name :" + module.name);
    Serial.println("Temperature :" + module.temperature);
    Serial.println("Min :" + module.min);
    Serial.println("Max :" + module.max);
    Serial.println("Trend :" + module.trend);
    Serial.println("Time Min :");
    Serial.println(module.timemin);
    Serial.println("Time Max :");
    Serial.println(module.timemax);
    Serial.println("Time Update :");
    Serial.println(module.timeupdate);
}

bool getStationsData() {
  String tmp_device_id = device_id;
  tmp_device_id.replace(":", "%3A");

  bool retour = false;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    String netatmoGetStationsData = "https://api.netatmo.com/api/getstationsdata?device_id=" + tmp_device_id + "&get_favorites=false";
    Serial.println(netatmoGetStationsData);
          
    http.begin(netatmoGetStationsData);

    char bearer[66] = "Bearer ";
    strcat(bearer,access_token);

    http.addHeader("Authorization", bearer); // Adding Bearer token as HTTP header
    int httpCode = http.GET();

    if (httpCode > 0) {
        DynamicJsonDocument doc(12288);
        String payload = http.getString();
#ifdef DEBUG_NETATMO
        Serial.println("body :");
        Serial.println(payload);
#endif
        deserializeJson(doc, payload);

        JsonArray stations = doc["body"]["devices"].as<JsonArray>();
        for (JsonObject station : stations) {
          NAMain.name = station["module_name"].as<String>();
          NAMain.min = station["dashboard_data"]["min_temp"].as<String>();
          NAMain.max = station["dashboard_data"]["max_temp"].as<String>();
          NAMain.temperature = station["dashboard_data"]["Temperature"].as<String>();
          NAMain.trend = station["dashboard_data"]["temp_trend"].as<String>();
          NAMain.timemin = DELAYUTC_YOURTIMEZONE + station["dashboard_data"]["date_min_temp"].as<unsigned long>();
          NAMain.timemax = DELAYUTC_YOURTIMEZONE + station["dashboard_data"]["date_max_temp"].as<unsigned long>();
          NAMain.timeupdate = DELAYUTC_YOURTIMEZONE + station["dashboard_data"]["time_utc"].as<unsigned long>();
          JsonArray modules = station["modules"].as<JsonArray>();
          int module4counter = 0;
          for(JsonObject module : modules) {
            if(module["type"].as<String>() == "NAModule1") {
              NAModule1.name = module["module_name"].as<String>();
              NAModule1.min = module["dashboard_data"]["min_temp"].as<String>();
              NAModule1.max = module["dashboard_data"]["max_temp"].as<String>();
              NAModule1.temperature = module["dashboard_data"]["Temperature"].as<String>();
              NAModule1.trend = module["dashboard_data"]["temp_trend"].as<String>();
              NAModule1.timemin = DELAYUTC_YOURTIMEZONE + module["dashboard_data"]["date_min_temp"].as<unsigned long>();
              NAModule1.timemax = DELAYUTC_YOURTIMEZONE + module["dashboard_data"]["date_max_temp"].as<unsigned long>();
              NAModule1.timeupdate = DELAYUTC_YOURTIMEZONE + station["dashboard_data"]["time_utc"].as<unsigned long>();
            }
            if(module["type"].as<String>() == "NAModule4") {
              NAModule4[module4counter].name = module["module_name"].as<String>();
              NAModule4[module4counter].min = module["dashboard_data"]["min_temp"].as<String>();
              NAModule4[module4counter].max = module["dashboard_data"]["max_temp"].as<String>();
              NAModule4[module4counter].temperature = module["dashboard_data"]["Temperature"].as<String>();
              NAModule4[module4counter].trend = module["dashboard_data"]["temp_trend"].as<String>();
              NAModule4[module4counter].timemin = DELAYUTC_YOURTIMEZONE + module["dashboard_data"]["date_min_temp"].as<unsigned long>();
              NAModule4[module4counter].timemax = DELAYUTC_YOURTIMEZONE + module["dashboard_data"]["date_max_temp"].as<unsigned long>();
              NAModule4[module4counter].timeupdate = DELAYUTC_YOURTIMEZONE + station["dashboard_data"]["time_utc"].as<unsigned long>();
              module4counter++;
            }
          }
        }
#ifdef DEBUG_NETATMO        
        dumpModule(NAMain);
        dumpModule(NAModule1);
        dumpModule(NAModule4[0]);
        dumpModule(NAModule4[1]);
        dumpModule(NAModule4[2]);
#endif
        retour = true;
              
    } else {
        Serial.println("getStationsData Error : " + http.errorToString(httpCode));
    }
    http.end();   
  }
  return retour;
}

bool getRefreshToken() {
  String tmp_refresh_token = refresh_token;
  tmp_refresh_token.replace("|", "%7C");
  
  bool retour = false;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String netatmoRefreshTokenPayload = "client_secret=" + client_secret + "&grant_type=refresh_token&client_id=" + client_id + "&refresh_token=" + tmp_refresh_token;
  
#ifdef DEBUG_NETATMO
    Serial.println(netatmoRefreshTokenPayload);
#endif

    http.begin("https://api.netatmo.com/oauth2/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST(netatmoRefreshTokenPayload);
    if (httpCode > 0) {
        DynamicJsonDocument doc(1024);
        String payload = http.getString();
#ifdef DEBUG_NETATMO
        Serial.println("body :");
        Serial.println(payload);
#endif
        deserializeJson(doc, payload);
        if (doc.containsKey("access_token")) {
          String str_access_token = doc["access_token"].as<String>();
#ifdef DEBUG_NETATMO
          char buffer_token[77] = "Old access token :";
          strcat(buffer_token,access_token);
          Serial.println(buffer_token);
          Serial.println("New access token :" + str_access_token);
#endif
          strcpy(access_token, str_access_token.c_str());
          retour = true;
        } else {
          Serial.println("No access_token");
        }

        if (doc.containsKey("refresh_token")) {
          String str_refresh_token = doc["refresh_token"].as<String>();
#ifdef DEBUG_NETATMO
          char buffer_token[79] = "Old refresh token : ";
          strcat(buffer_token,refresh_token);
          Serial.println(buffer_token);
          Serial.println("New refresh token : " + str_refresh_token);
#endif
          strcpy(refresh_token, str_refresh_token.c_str()); 
        } else {
          Serial.println("No refresh_token");
          retour = false;
        } 
    } else {
        Serial.println("refreshToken Error : " + http.errorToString(httpCode));
    }
    http.end();   
  }
  return retour;
}

void drawDebugGrid()
{
    int gridSpacing = 10; // Espacement entre les lignes de la grille
    int screenWidth = 122;
    int screenHeight = 250;

    // Dessiner des lignes verticales
    for (int x = 0; x <= screenWidth; x += gridSpacing)
    {
        display.drawLine(x, 0, x, screenHeight, GxEPD_BLACK);
    }

    // Dessiner des lignes horizontales
    for (int y = 0; y <= screenHeight; y += gridSpacing)
    {
        display.drawLine(0, y, screenWidth, y, GxEPD_BLACK);
    }
}

void goToDeepSleepUntilNextWakeup() {
  time_t sleepDuration = WAKEUP_INTERVAL;
  Serial.print("Sleeping duration (seconds): ");
  Serial.println(sleepDuration);

  // Configure wake up
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {
  // put your main code here, to run repeatedly:
}
