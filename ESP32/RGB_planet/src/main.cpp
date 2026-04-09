#include "Arduino.h"
#include "SolarCalculator.h"
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


const double maxlatitudes = 70.0;
const int STRIPS = 10;
const int LEDS_BY_STRIP = 38;

// Coordinates and sun data:
double longitudes[STRIPS][LEDS_BY_STRIP]; // Stores longitude associated to each LED
double latitudes[STRIPS][LEDS_BY_STRIP];// Stores latitude associated to each LED
double sunElevations[STRIPS][LEDS_BY_STRIP]; // Stores sun elevation of last date computed associated to each LED


// Color and values variables
boolean LEDUpdatedStatus[STRIPS][LEDS_BY_STRIP]; // Tracks the state of LEDs in regard to their value in memory, to only change LED instructions when needed
double ELEVATION_UPDATE_THRESHOLD = 1; // Threshold in degrees above wich the LED requires an update of color


// Time variables
unsigned long currentTime;
long frameRefreshTime = 50; //estimated time between two frames when animating in ms

// LEDs vars
CRGB leds[STRIPS][LEDS_BY_STRIP];

void setup() {
    // Power-up safety delay
    delay(1000);

    // Generate coordinates
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP/2; ledID++){
            // Standard hemisphere
            longitudes[stripID][ledID] = -180.0 + (double) stripID * (180.0 / (double) STRIPS);
            latitudes[stripID][ledID] = -maxlatitudes + (double) ledID * maxlatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
        }
        for (int ledID=LEDS_BY_STRIP/2; ledID < LEDS_BY_STRIP; ledID++){
            // Mirror hemisphere
            longitudes[stripID][ledID] = 0.0 + (double) stripID * (180.0 / (double) STRIPS);
            latitudes[stripID][ledID] = -maxlatitudes + (double) (ledID-(double) LEDS_BY_STRIP/2.0) * maxlatitudes*2.0 / (((double) LEDS_BY_STRIP/2.0)-1.0);
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
    FastLED.addLeds<WS2812B, STRIP_PIN_0> (leds[0], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_1> (leds[1], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_2> (leds[2], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_3> (leds[3], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_4> (leds[4], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_5> (leds[5], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_6> (leds[6], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_7> (leds[7], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_8> (leds[8], LEDS_BY_STRIP);
    FastLED.addLeds<WS2812B, STRIP_PIN_9> (leds[9], LEDS_BY_STRIP);
}

// Light strip by strip and led by led sequentially
void stripTest() {
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            // Switch ON
            leds[stripID][ledID] = CRGB::White; 
            FastLED.show();
            delay(100);

            // Switch OFF
            leds[stripID][ledID] = CRGB::Black; 
            FastLED.show();
            delay(100);
        }
    }
}

// Updates all points sun elevation angles at a given date
void updateElevations(time_t timeToCompute){
    for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            // Calculate the solar position, in degrees
            double azimuth; // Azimuths are not used, so not storedin arrays and not returned to save memory
            double elevation;
            calcHorizontalCoordinates(timeToCompute, latitudes[stripID][ledID], longitudes[stripID][ledID], azimuth, elevation);

            // Update value stored in memory
            if (abs(elevation - sunElevations[stripID][ledID]) > ELEVATION_UPDATE_THRESHOLD){
                // If threshold is crossed, flag the need for color change and replace value:
                LEDUpdatedStatus[stripID][ledID] = false;
                sunElevations[stripID][ledID] = elevation;
            }
            
        }
    }
}

// Set LEDs colors depending on their elevations
void updateLEDColors() {
     for (int stripID=0; stripID < STRIPS; stripID++){
        for (int ledID=0; ledID < LEDS_BY_STRIP; ledID++){
            if (LEDUpdatedStatus[stripID][ledID] == false){
                LEDUpdatedStatus[stripID][ledID] = true;

                // Cut light at night:
                if(sunElevations[stripID][ledID] < 0){
                    leds[stripID][ledID] = CRGB::Black;
                }
                else{
                    float intensity = 0.1; // LED intensity,for power and max current management
                    float sunPhaseScale = sunElevations[stripID][ledID] / 90.0; // Normalized sun phase value, 0 is horizon, 1 is zenith

                    float redScale = pow(sunPhaseScale, 0.1);
                    float greenScale = pow(sunPhaseScale, 0.4);
                    float blueScale = pow(sunPhaseScale, 2.0);
                    leds[stripID][ledID].setRGB(
                        intensity * 255.0 * redScale, 
                        intensity * 255.0*greenScale,
                        intensity * 255.0*blueScale
                    );
                }
            }
        }
    }
}


// Animate sun position from current date to targetDate, running intermediate animation positions during duration ms.
void animateToDate(time_t targetDate, int duration){
    long animatedDate = currentTime; // Date to be animated
    long n_steps = duration / frameRefreshTime; // Number of steps that will be executed
    long dateInterval = targetDate - currentTime;
    long dateStep = dateInterval / n_steps; // Date gap to be crossed at each animation frame

    while (animatedDate < targetDate){
        updateElevations(animatedDate);
        updateLEDColors();
        animatedDate += dateStep;
    }
}

void loop() {
    // Periodic tests to ensure the lights work
    stripTest();

    // Test a computation and check elevation results

    // Check setting date and adding a day to it

    // Check display of a state

    // Check state animation
}

