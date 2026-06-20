#include "Arduino.h"
#include "FastLED.h"
#include "TimeLib.h"
#include <atomic>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Network credentials
const char* ssid = "bahnhof2_4Ghz-226625";
const char* password = "soSTow6NaRiqu";

// NTP server
const char* timeAPIURL = "https://time.now/developer/api/ip";

// Web server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Time variables
unsigned long lastTimeServerUpdate; // Millis stamps for last synchronization with the time server
unsigned long timeSyncInterval = 60000*10; // Delay in milliseconds between two synchronisations with the time server
time_t clientTime; // Time received in real time by the client
long clientTimeMultiplier; // Time multiplier factor received in real time from the client

// LED strips pins:
#define STRIP_PIN_0 16
#define STRIP_PIN_1 17
#define STRIP_PIN_2 18
#define STRIP_PIN_3 19
#define STRIP_PIN_4 21
#define STRIP_PIN_5 22
#define STRIP_PIN_6 23
#define STRIP_PIN_7 25
#define STRIP_PIN_8 26
#define STRIP_PIN_9 27

// Current configuration
const float currentPerLEDColor = 0.02; // mA
const float availableAmps = 0.1; // A

// Physical LED bands:
const double LEDS_DIAMETER = 233.427; // mm
const double LEDS_SPACING = 16.667; // mm
const int STRIPS = 10;
const int LEDS_BY_STRIP = 40;
double maxLatitudes = 0.0; // Max latitude in degrees

// Coordinates and sun data:
double longitudes[STRIPS][LEDS_BY_STRIP]; // Stores longitude associated to each LED
double latitudes[STRIPS][LEDS_BY_STRIP]; // Stores latitude associated to each LED
int sunElevations[STRIPS][LEDS_BY_STRIP]; // Stores calculated sun elevations

long frameRefreshTime = 50; //estimated time between two frames when animating in ms

// FASTLED objects list
CRGB FASTLED_Leds[STRIPS][LEDS_BY_STRIP];

// LOG FUNCTIONS ///////////////////////////////////////////////////
void printTime(time_t t){
    Serial.print(year(t));
    Serial.print("/");
    Serial.print(month(t));
    Serial.print("/");
    Serial.print(day(t));
    Serial.print(" - ");
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.print(second(t));
}

// ASTRODYNAMICS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////

// Fills an sunElevations array with computed elevation for all LEDS for a given timeToCompute (UTC)
void computeElevations(time_t timeToCompute, int sunElevations[STRIPS][LEDS_BY_STRIP]){
    // Time related astronomic parameters (only one time):
    const int doy =  (275 * month(timeToCompute)) / 9 - ((month(timeToCompute) + 9) / 12) * (1 + (year(timeToCompute) - 4 * (year(timeToCompute) / 4) + 2) / 3) + day(timeToCompute) - 30; // Day of year, January 1 is 1
    double fractionalTime = hour(timeToCompute) + minute(timeToCompute) / 60.0; // Fractional hour
    double fractionalYear = 2.0 * M_PI / 365.0 * (doy - 1 + (fractionalTime - 12.0) / 24.0); // Fractional year (radians)

        // --- Equation of time (minutes)
    double eqtime = 229.18 * (
        0.000075
        + 0.001868 * cos(fractionalYear)
        - 0.032077 * sin(fractionalYear)
        - 0.014615 * cos(2 * fractionalYear)
        - 0.040849 * sin(2 * fractionalYear)
    );

    // --- Solar declination (radians)
    double decl = 
    0.006918
    - 0.399912 * cos(fractionalYear)
    + 0.070257 * sin(fractionalYear)
    - 0.006758 * cos(2 * fractionalYear)
    + 0.000907 * sin(2 * fractionalYear)
    - 0.002697 * cos(3 * fractionalYear)
    + 0.00148  * sin(3 * fractionalYear);
    
    // Space related astronomic parameters (looped)
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            // --- Time offset (minutes)
            double timeOffset = eqtime + 4.0 * longitudes[stripID][ledID];

            // --- True solar time (minutes)
            double tst = fractionalTime * 60.0 + timeOffset;

            // --- Hour angle (degrees → radians)
            double ha = (tst / 4.0 - 180.0) * DEG_TO_RAD;

            // --- Solar elevation
            double elevation = asin(
                sin(latitudes[stripID][ledID]*DEG_TO_RAD) * sin(decl)
                + cos(latitudes[stripID][ledID]*DEG_TO_RAD) * cos(decl) * cos(ha)
            ) * RAD_TO_DEG;

            sunElevations[stripID][ledID] = elevation;
            
            // // Print elevation:
            // Serial.print("Sun elevation on ");
            // printTime(timeToCompute);
            // Serial.print(" at long ");
            // Serial.print(longitudes[stripID][ledID]);
            // Serial.print("; lat ");
            // Serial.print(latitudes[stripID][ledID]);
            // Serial.print(": ");
            // Serial.print(elevation);
            // Serial.println(" deg");
        }
    }
}


// LED utilities fonctions ///////////////////////////////////////////////////

// Serial display of all LED colors for a given strip
void printStripColorsArray(int stripID){
    for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
        Serial.print("LED ");
        Serial.print(ledID);
        Serial.print(": r");
        Serial.print(FASTLED_Leds[stripID][ledID].r);
        Serial.print(" g");
        Serial.print(FASTLED_Leds[stripID][ledID].g);
        Serial.print(" b");
        Serial.println(FASTLED_Leds[stripID][ledID].b);
    }
}

// Takes a linear scale 0-1 and fill rgbArray from corresponding colors 
void scaleToColor(float scale, int rgbArray[3]){
    float redScale   = 255.0 * pow(scale, 0.2);
    float greenScale = 255.0 * pow(scale, 0.8);
    float blueScale  = 255.0 * pow(scale, 2.0);
    float intensityScale = pow(scale,0.5);

    // Display scale conversion through serial:
    // Serial.print("Scale: ");
    // Serial.print(scale);
    // Serial.print(" Color: ");
    // Serial.print(redScale);
    // Serial.print(";");
    // Serial.print(greenScale);
    // Serial.print(";");
    // Serial.println(blueScale);
    // delay(500);

    rgbArray[0] = (int) redScale*intensityScale;
    rgbArray[1] = (int) greenScale*intensityScale;
    rgbArray[2] = (int) blueScale*intensityScale;
}

// Dims all lights to cap power consumption
void dimToCapPower(){
    float total_current = 0.0;
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            total_current += (float) FASTLED_Leds[stripID][ledID].r/255.0 * currentPerLEDColor;
            total_current += (float) FASTLED_Leds[stripID][ledID].g/255.0 * currentPerLEDColor;
            total_current += (float) FASTLED_Leds[stripID][ledID].b/255.0 * currentPerLEDColor;
        }
    }
    // Serial.print("Total amps before cap:");
    // Serial.println(total_current);

    float capRatio = availableAmps / total_current;
    // Serial.print("Cap ratio:");
    // Serial.println(capRatio);

    total_current = 0.0;
    if (capRatio < 1.00){
        for (int stripID=0; stripID < STRIPS; stripID++){
            for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
                float newR = (float) FASTLED_Leds[stripID][ledID].r * capRatio;
                float newG = (float) FASTLED_Leds[stripID][ledID].g * capRatio;
                float newB = (float) FASTLED_Leds[stripID][ledID].b * capRatio;

                FASTLED_Leds[stripID][ledID].setRGB(
                    (int) newR,
                    (int) newG,
                    (int) newB
                );

                // Update new current value
                total_current += newR/255.0*currentPerLEDColor;
                total_current += newG/255.0*currentPerLEDColor;
                total_current += newB/255.0*currentPerLEDColor;
            }
        }
        // Serial.print("Total amps after cap:");
        // Serial.println(total_current);
    }
}


// Display the sun color scale on a given strip
void displayColorScale(int stripID, float intensity){
    for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
        int RGBValues[3];
        float scale = (float) ledID / (float) LEDS_BY_STRIP * intensity;
        scaleToColor(scale, RGBValues);
        FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
    }
    dimToCapPower();
    FastLED.show();
}

std::atomic<bool> waitAnimationFlag(true);
// Display a spiraling wave going up around the globe, spacing lights with interDelay ms
void waitAnimation(void *parameters){
    // Runs forever until the task is stopped:
    while (waitAnimationFlag.load()){
        for (int strip=0; strip<STRIPS;strip++){
            if (waitAnimationFlag.load() == false){
                break;
            }
            for (float intensity=0.0; intensity<1.0; intensity+=0.02){
                displayColorScale(strip,pow(intensity,2));
            }
            for (float intensity=1.0; intensity>0.0; intensity-=0.02){
                displayColorScale(strip,pow(intensity,2));
            }
        }
    }
    
    vTaskDelete(NULL);
}

// Switches all LEDS off
void switchLEDSOff(){
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            FASTLED_Leds[stripID][ledID] = CRGB::Black;
        }
    }
    FastLED.show();
}

// Set FASTLED_Leds colors depending on their elevations
void elevationsToColors(int sunElevations[STRIPS][LEDS_BY_STRIP]) {
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            // Cut light at night:
            if(sunElevations[stripID][ledID] < 0){
                FASTLED_Leds[stripID][ledID] = CRGB::Black;
            }
            else{
                float intensity = 0.1; // LED intensity,for power and max current management
                float sunPhaseScale = (float) sunElevations[stripID][ledID] / 90.0; // Normalized sun phase value, 0 is horizon, 1 is zenith

                int RGBValues[3];
                scaleToColor(sunPhaseScale, RGBValues);

                FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0] * intensity, RGBValues[1] * intensity, RGBValues[2] * intensity);
            }
        }
    }
    dimToCapPower();
    FastLED.show();
}

// Display the given date on the globe
void displayDate(time_t date){
    computeElevations(date,sunElevations);
    elevationsToColors(sunElevations);
}

// animateToDate function arguments
struct animateToDateParams{
    int duration;
    time_t targetDate;
    long forcedDateStep;
};
// Animate sun position from current date to targetDate in duration milliseconds. If forcedDateStep is specified, goes to the closest round number of iterations to match the duration.
void animateToDate(void *parameters){
    animateToDateParams *config = (animateToDateParams*) parameters;
    int duration = config->duration;
    time_t targetDate = config->targetDate;
    long forcedDateStep = config->forcedDateStep;

    time_t animatedTime = now(); // Store start date, to be animated
    time_t startTime = now(); // Keep track of starting time
    long step = 0; // Steps counter for animation
    float ratio; // For animation, 0 to 1 linear
    float animatedProgress; // 0 to 1 cubic

    long n_steps = (long) ((float) duration / (float) frameRefreshTime); // Number of steps that will be executed
    long totalSeconds = targetDate - animatedTime; // Total duration between current date and target date in seconds
    long dateStep;
    if (forcedDateStep != 0){
        dateStep = round(totalSeconds/n_steps/forcedDateStep)*forcedDateStep;
        if (dateStep == 0){dateStep = forcedDateStep;}
    }

    while (animatedTime < targetDate){
        ratio = (float) step / (float) n_steps;
        // animatedProgress =  -(cos(PI * ratio) - 1) / 2; // Sine
        animatedProgress = ratio; // Linear

        if (forcedDateStep == 0){
            animatedTime = startTime + animatedProgress*totalSeconds;
        }else{
            animatedTime += dateStep;
        }

        displayDate(animatedTime);
        step++;
    }
    vTaskDelete(NULL);
}

// INTERFACE FUNCTIONS ///////////////////////////////////////////////////////

// Connect to the wifi network
void connectWiFi(){    
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected. Server running on ");
    Serial.println(WiFi.localIP());
}
// Gets time stamp from time server and sets the board time accordingly
void getDateTimeFromServer(){
    HTTPClient http;
    http.begin(timeAPIURL);
    // Send HTTP GET request
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200){
        JsonDocument doc;
        deserializeJson(doc, http.getString());
        const char* dateTime = doc["datetime"];
        String dateTimeString = String(dateTime);

        setTime(
            (int) dateTimeString.substring(11,13).toInt(),
            (int) dateTimeString.substring(14,16).toInt(),
            (int) dateTimeString.substring(17,19).toInt(),
            (int) dateTimeString.substring(8,10).toInt(),
            (int) dateTimeString.substring(5,7).toInt(),
            (int) dateTimeString.substring(0,4).toInt()
        );
        Serial.print("Time set to ");
        printTime(now());
        Serial.println();

        lastTimeServerUpdate = millis(); // Log time stamp to check for next occurence in loop.
    }
    else{
        Serial.print("Time api call failed. Response code: ");
        Serial.println(httpResponseCode);
    }
}

// Handle incoming WebSocket messages
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0; // Null-terminate incoming byte payload safely
        char* message = (char*)data;
        Serial.print(message);
        Serial.print(" ");
        char* token1 = strtok(message, ",");
        if (token1 != NULL) {
            char* token2 = strtok(NULL, ",");
            if (token2 != NULL){
                int64_t ms_timestamp = strtoll(token1, NULL, 10); // Going through int64_t to avoid overflow
                clientTime =  (time_t)(ms_timestamp / 1000);
                clientTimeMultiplier = strtol(token2, NULL, 10);
                Serial.print(clientTime);
                Serial.print(" ");
                Serial.println(clientTimeMultiplier);
            }
        }
    }
}
// WebSocket event handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void setup() {
    // Power-up safety delay
    delay(500);
    Serial.begin(9600);

    // Generate coordinates
    maxLatitudes = LEDS_SPACING * (float) LEDS_BY_STRIP / LEDS_DIAMETER * RAD_TO_DEG/2.0;
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP/2; ledID++){
            // Standard hemisphere
            longitudes[stripID][ledID] = -180.0 + (double) stripID * (180.0 / (double) STRIPS);
            latitudes[stripID][ledID] = -maxLatitudes + (double) ledID * maxLatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
        }
        for (int ledID=LEDS_BY_STRIP/2; ledID < LEDS_BY_STRIP; ledID++){
            // Mirror hemisphere
            longitudes[stripID][ledID] = 0.0 + (double) stripID * (180.0 / (double) STRIPS);
            latitudes[stripID][ledID] = maxLatitudes - (double) (ledID-(double) LEDS_BY_STRIP/2.0) * maxLatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
        }
    }


    // // Show coordinates table
    // Serial.begin(9600);
    // for (int stripID=0; stripID < STRIPS; stripID++){
    //     for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
    //         delay(50);
    //         Serial.print(stripID);
    //         Serial.print(";");
    //         Serial.print(ledID);
    //         Serial.print(" - ");
    //         Serial.print(longitudes[stripID][ledID]);
    //         Serial.print(";");
    //         Serial.println(latitudes[stripID][ledID]);
    //     }
    // }

    // Initialize fastLED objects
    FastLED.addLeds<WS2812B, STRIP_PIN_0, GRB> (FASTLED_Leds[0], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_1, GRB> (FASTLED_Leds[1], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_2, GRB> (FASTLED_Leds[2], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_3, GRB> (FASTLED_Leds[3], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_4, GRB> (FASTLED_Leds[4], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_5, GRB> (FASTLED_Leds[5], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_6, GRB> (FASTLED_Leds[6], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_7, GRB> (FASTLED_Leds[7], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_8, GRB> (FASTLED_Leds[8], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_9, GRB> (FASTLED_Leds[9], LEDS_BY_STRIP);

    // Run spiral animation to wait
    xTaskCreatePinnedToCore(waitAnimation,"waitAnimation",10000,NULL,0,NULL,0);

    connectWiFi();
    getDateTimeFromServer();
    // Initialize WebSocket
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    // Start server
    server.begin();

    waitAnimationFlag.store(false);
    delay(2000);
}

// Main loop is run on core 1 and handles all LED control
void loop() {
    ws.cleanupClients(); // Clean up heap memory
    delay(1000);
    // Animate a year:
    // animateToDateParams animateParams;
    // animateParams.duration = 5000;
    // animateParams.targetDate = now()+360*24*60*60;
    // animateParams.forcedDateStep = 24*60*60;
    // xTaskCreatePinnedToCore(animateToDate,"animateToDate",10000,&animateParams,0,NULL,0);

}