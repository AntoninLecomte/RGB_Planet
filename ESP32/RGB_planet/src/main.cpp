#include "Arduino.h"
#include "FastLED.h"
#include <TimeLib.h>

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

// FASTLED_Leds configuration:
const double maxlatitudes = 70.0;
const int STRIPS = 1; //10;
const int LEDS_BY_STRIP = 120;//38;

// Coordinates and sun data:
double longitudes[STRIPS][LEDS_BY_STRIP]; // Stores longitude associated to each LED
double latitudes[STRIPS][LEDS_BY_STRIP];// Stores latitude associated to each LED
int sunElevations[STRIPS][LEDS_BY_STRIP]; // Stores calculated sun elevations

// Time variables
time_t currentTime;
long frameRefreshTime = 50; //estimated time between two frames when animating in ms

// FASTLED objects list
CRGB FASTLED_Leds[STRIPS][LEDS_BY_STRIP];


// DATA FUNCTIONS ///////////////////////////////////////////////////
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
}

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
    float redScale   = 255.0 * pow(scale, 0.05);
    float greenScale = 255.0 * pow(scale, 0.6);
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
    int RGBValues[3];
    float scale;
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
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


// Animate sun position from current date to targetDate, running intermediate animation positions during duration ms.
void animateToDate(int duration, int year, int month, int day, int hour, int minute){
    time_t animatedTime = currentTime; // Store start date, to be animated
    setTime(hour,minute,0,day,month,year);
    currentTime = now();

    long n_steps = (long) ((float) duration / (float) frameRefreshTime); // Number of steps that will be executed
    long totalSeconds = currentTime - animatedTime; // Total duration in seconds
    long dateStep = (long) ((float) totalSeconds / (float) n_steps); // Date gap to be crossed at each animation frame

    while (animatedTime < currentTime){
        computeElevations(animatedTime,sunElevations);
        elevationsToColors(sunElevations);
        animatedTime += dateStep;
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


void setup() {
    // Power-up safety delay
    delay(1000);
    Serial.begin(9600);

    // Generate coordinates
    // for (int stripID=0; stripID < STRIPS; stripID++){
    //     for (int ledID=0; ledID < LEDS_BY_STRIP/2; ledID++){
    //         // Standard hemisphere
    //         longitudes[stripID][ledID] = -180.0 + (double) stripID * (180.0 / (double) STRIPS);
    //         latitudes[stripID][ledID] = -maxlatitudes + (double) ledID * maxlatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
    //     }
    //     for (int ledID=LEDS_BY_STRIP/2; ledID < LEDS_BY_STRIP; ledID++){
    //         // Mirror hemisphere
    //         longitudes[stripID][ledID] = 0.0 + (double) stripID * (180.0 / (double) STRIPS);
    //         latitudes[stripID][ledID] = -maxlatitudes + (double) (ledID-(double) LEDS_BY_STRIP/2.0) * maxlatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
    //     }
    // }
    // {DEV} Set the first strip as the equator
    for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
        latitudes[0][ledID] = 70.0;
        longitudes[0][ledID] = -180.0 + (double) ledID * (360.0 / (double) LEDS_BY_STRIP);
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

    // Show all leds
    // stripTest();

    // Test a computation and check elevation results
    setTime(12,0,0,11,4,2026);
    currentTime = now();

    // Animate a day:
    for (int i; i<10;i++){
        animateToDate(20000, 2026+i,4,11,12,0);
    }
    

    delay(1000);
    switchLEDSOff();

    // Check setting date and adding a day to it

    // Check display of a state

    // Check state animation
}

void loop() {
    delay(1000);    
}

