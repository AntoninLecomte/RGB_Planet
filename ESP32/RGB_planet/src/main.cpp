#include "Arduino.h"
#include "SolarCalculator.h"
#define LED 2

const int STRIPS = 10;
const int LEDS_BY_STRIP = 38;
const float maxLatitude = 70;

float longitude[STRIPS][LEDS_BY_STRIP];
float latitude[STRIPS][LEDS_BY_STRIP];
int sunElevation[STRIPS][LEDS_BY_STRIP];

void setup() {
    // Set pin mode
    pinMode(LED,OUTPUT);

    Serial.begin(9600);
    // Generate coordinates
    for (int STRIP_ID=0; STRIP_ID < STRIPS; STRIP_ID++){
        for (int LED_ID=0; LED_ID < LEDS_BY_STRIP/2; LED_ID++){
            // Standard hemisphere
            longitude[STRIP_ID][LED_ID] = -180.0 + (float) STRIP_ID * (180.0 / (float) STRIPS);
            latitude[STRIP_ID][LED_ID] = -maxLatitude + (float) LED_ID * maxLatitude*2.0 / (((float) LEDS_BY_STRIP/2.0)-1.0);
        }
        for (int LED_ID=LEDS_BY_STRIP/2; LED_ID < LEDS_BY_STRIP; LED_ID++){
            // Mirror hemisphere
            longitude[STRIP_ID][LED_ID] = 0.0 + (float) STRIP_ID * (180.0 / (float) STRIPS);
            latitude[STRIP_ID][LED_ID] = -maxLatitude + (float) (LED_ID-(float) LEDS_BY_STRIP/2.0) * maxLatitude*2.0 / (((float) LEDS_BY_STRIP/2.0)-1.0);
        }
    }

    // // Show coordinates table
    // for (int STRIP_ID=0; STRIP_ID < STRIPS; STRIP_ID++){
    //     for (int LED_ID=0; LED_ID < LEDS_BY_STRIP; LED_ID++){
    //         delay(50);
    //         Serial.print(STRIP_ID);
    //         Serial.print(";");
    //         Serial.print(LED_ID);
    //         Serial.print(" - ");
    //         Serial.print(longitude[STRIP_ID][LED_ID]);
    //         Serial.print(";");
    //         Serial.println(latitude[STRIP_ID][LED_ID]);
    //     }
    // }
}

void loop() {
    
}