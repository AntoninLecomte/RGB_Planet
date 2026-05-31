#include "Arduino.h"
#include "FastLED.h"
#include "TimeLib.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Network credentials
const char* ssid = "bahnhof2_4Ghz-226625";
const char* password = "soSTow6NaRiqu";

// NTP server
const char* timeAPIURL = "https://time.now/developer/api/ip";

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
const float availableAmps = 1.0; // A

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

// Time variables
unsigned long lastTimeUpdate; // Millis stamps for last synchronization with the time server
unsigned long timeSyncInterval = 60000*10; // Delay in milliseconds between two synchronisations with the time server

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

// TIME FUNCTIONS ///////////////////////////////////////////////////
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

        lastTimeUpdate = millis(); // Log time stamp to check for next occurence in loop.
    }
    else{
        Serial.print("Time api call failed. Response code: ");
        Serial.println(httpResponseCode);
    }
    
}

// ASTRODYNAMICS FUNCTIONS

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

    rgbArray[0] = (int) redScale;
    rgbArray[1] = (int) greenScale;
    rgbArray[2] = (int) blueScale;
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
void displayColorScale(int stripID){
    for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
        int RGBValues[3];
        float scale = (float) ledID / (float) LEDS_BY_STRIP;
        scaleToColor(scale, RGBValues);
        FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
    }
    dimToCapPower();
    FastLED.show();
}



// Light strip by strip and led by led sequentially
void stripTest() {
    Serial.println("Starting strips tests");
    int RGBValues[3];
    float scale;
    for (int stripID=0; stripID < STRIPS; stripID++){
        Serial.print("   Testing strip ");
        Serial.println(stripID);
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            // Switch ON
            scale = (float) ledID / (float) LEDS_BY_STRIP;
            scaleToColor(scale, RGBValues);
            FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
            FastLED.show();
            delay(10);

            // // Print strip test colors
            // Serial.print("   ");
            // Serial.print(stripID);
            // Serial.print("/");
            // Serial.print(ledID);
            // Serial.print(" ");
            // Serial.print(RGBValues[0]);
            // Serial.print(";");
            // Serial.print(RGBValues[1]);
            // Serial.print(";");
            // Serial.println(RGBValues[2]);

            // Switch OFF
            FASTLED_Leds[stripID][ledID] = CRGB::Black; 
            FastLED.show();
            delay(10);
        }
        // Reverse light wave test back to start:
        for (int ledID=LEDS_BY_STRIP-1; ledID >= 0; ledID--){
            // Switch ON
            scale = (float) ledID / (float) LEDS_BY_STRIP;
            scaleToColor(scale, RGBValues);
            FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
            FastLED.show();
            delay(10);

            // Switch OFF
            FASTLED_Leds[stripID][ledID] = CRGB::Black;
            FastLED.show();
            delay(10);
        }
    }
    Serial.println("Strips tests ended");
}

// Display a spiraling wave going up around the globe
void spiralTest(){
    int RGBValues[3];
    float scale;
    for (int ledID=0; ledID < LEDS_BY_STRIP/2; ledID++){
        // Color for this latitude:
        scale = (float) ledID / (float) LEDS_BY_STRIP * 2.0;
        scaleToColor(scale, RGBValues);

        // Standard hemisphere
        for (int stripID=0; stripID < STRIPS; stripID++){
            // Show coordinates
            // Serial.print(stripID);
            // Serial.print(";");
            // Serial.print(ledID);
            // Serial.print(" - ");
            // Serial.print(longitudes[stripID][ledID]);
            // Serial.print(";");
            // Serial.println(latitudes[stripID][ledID]);

            // Switch ON
            FASTLED_Leds[stripID][ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
            FastLED.show();
            delay(20);

            // Switch OFF
            FASTLED_Leds[stripID][ledID] = CRGB::Black;
            FastLED.show();
            delay(20);
        }
        // Mirror hemisphere
        for (int stripID=0; stripID < STRIPS; stripID++){
            // Show coordinates
            // Serial.print(stripID);
            // Serial.print(";");
            // Serial.print(ledID);
            // Serial.print(" - ");
            // Serial.print(longitudes[stripID][LEDS_BY_STRIP-ledID]);
            // Serial.print(";");
            // Serial.println(latitudes[stripID][LEDS_BY_STRIP-ledID]);

            // Switch ON
            FASTLED_Leds[stripID][LEDS_BY_STRIP-ledID].setRGB(RGBValues[0],RGBValues[1],RGBValues[2]);
            FastLED.show();
            delay(20);

            // Switch OFF
            FASTLED_Leds[stripID][LEDS_BY_STRIP-ledID] = CRGB::Black;
            FastLED.show();
            delay(20);
        }
    }
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

// Animate sun position from current date to targetDate, running intermediate animation positions during duration ms.
void animateToDate(int duration, time_t targetDate){
    time_t animatedTime = now(); // Store start date, to be animated

    long n_steps = (long) ((float) duration / (float) frameRefreshTime); // Number of steps that will be executed
    long totalSeconds = targetDate - animatedTime; // Total duration in seconds
    long dateStep = (long) ((float) totalSeconds / (float) n_steps); // Date gap to be crossed at each animation frame

    while (animatedTime < targetDate){
        displayDate(animatedTime);
        animatedTime += dateStep;
    }
}




void setup() {
    // Power-up safety delay
    delay(1000);
    Serial.begin(9600);

    // Connect to WiFi:
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected. IP Address: ");
    Serial.println(WiFi.localIP());

    // Get hour:
    getDateTimeFromServer();

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
}

void loop() {
    // Check time and update if necessary:
    if ((millis() - lastTimeUpdate) > timeSyncInterval){
        getDateTimeFromServer();
    }

    Serial.print("Running new loop: ");
    printTime(now());
    Serial.println();

    // Show current time:
    displayDate(now());
    delay(10000);

    // Animate a year:
    animateToDate(2000,now()+365*24*60*60);
}

