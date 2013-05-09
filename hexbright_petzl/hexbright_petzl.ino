/*

  Modified to cycle through modes but allow direct shutoff after 2 seconds of idle button time.

  This mimics the behavior of a petzl headlamp.

*/

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                340
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define APIN_TEMP               0
#define APIN_CHARGE             3
// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3

// State
byte          mode               = 0;
unsigned long btnTime            = 0;
boolean       btnDown            = false;
unsigned long lastModeTransition = 0;  // Used so we don't have to cycle thru all modes


void setup() {
    // We just powered on!  That means either we got plugged
    // into USB, or the user is pressing the power button.
    pinMode(DPIN_PWR, INPUT);
    digitalWrite(DPIN_PWR, LOW);

    // Initialize GPIO
    pinMode(DPIN_RLED_SW, INPUT);
    pinMode(DPIN_GLED, OUTPUT);
    pinMode(DPIN_DRV_MODE, OUTPUT);
    pinMode(DPIN_DRV_EN, OUTPUT);
    digitalWrite(DPIN_DRV_MODE, LOW);
    digitalWrite(DPIN_DRV_EN, LOW);

    // Initialize serial busses
    Serial.begin(9600);
    Wire.begin();

    btnTime            = millis();
    lastModeTransition = millis();
    btnDown            = digitalRead(DPIN_RLED_SW);
    mode               = MODE_OFF;

    Serial.println("Powered up!");
}


void loop() {
    static unsigned long lastTempTime;
    unsigned long        time = millis();

    checkChargeState(time);

    // Check temp sensors
    // TODO: Refactor this time logic out more
    if (time - lastTempTime > 1000) {
        checkTemperature();
        lastTempTime = time;
    }

    // Periodically pull down the button's pin, since
    // in certain hardware revisions it can float.
    pinMode(DPIN_RLED_SW, OUTPUT);
    pinMode(DPIN_RLED_SW, INPUT);

    // Check for mode changes
    byte newBtnDown = digitalRead(DPIN_RLED_SW);

    // Check for button click release
    if (btnDown && !newBtnDown && (time - btnTime) > 50) { // The button was pressed and released
        buttonPressed(time - lastModeTransition);  // Call button pressed function
    }

    // Remember button state so we can detect transitions
    if (newBtnDown != btnDown) {
        btnTime = time;
        btnDown = newBtnDown;
        delay(50);
    }
}


void checkChargeState(unsigned long time) {
    // Check the state of the charge controller
    int chargeState = analogRead(APIN_CHARGE);
    if (chargeState < 128) { // Low - charging
        digitalWrite(DPIN_GLED, (time & 0x0100) ? LOW : HIGH);
    }
    else if (chargeState > 768) { // High - charged
        digitalWrite(DPIN_GLED, HIGH);
    }
    else { // Hi-Z - shutdown
        digitalWrite(DPIN_GLED, LOW);
    }
}


// Handle overheating behavior and temp readouts
void checkTemperature() {
    // Check the temperature sensor
    int temperature = analogRead(APIN_TEMP);
    Serial.print("Temp: ");
    Serial.println(temperature);
    if (temperature > OVERTEMP && mode != MODE_OFF) {
        Serial.println("Overheating!");

        for (int i = 0; i < 6; i++) {
            digitalWrite(DPIN_DRV_MODE, LOW);
            delay(100);
            digitalWrite(DPIN_DRV_MODE, HIGH);
            delay(100);
        }
        digitalWrite(DPIN_DRV_MODE, LOW);

        mode = MODE_LOW;
    }
}


// Handle button press logic
void buttonPressed(unsigned long timeSinceLastPress) {
    // Check to see if we've pressed in the last 2 seconds, if not then shutdown
    if (mode != MODE_OFF && timeSinceLastPress > 2000) {
        switchMode(MODE_OFF);
    }
    else {
        // Cycle thru the available modes
        switch (mode) {
        case MODE_OFF:
            switchMode(MODE_LOW);
            break;
        case MODE_LOW:
            switchMode(MODE_MED);
            break;
        case MODE_MED:
            switchMode(MODE_HIGH);
            break;
        case MODE_HIGH:
            switchMode(MODE_OFF);
            break;
        }
    }
}


// Preform direct mode transitions
void switchMode(byte newMode) {
    // Do the mode transitions
    switch (newMode) {
    case MODE_OFF:
        Serial.println("Mode = off");
        pinMode(DPIN_PWR, OUTPUT);
        digitalWrite(DPIN_PWR, LOW);
        digitalWrite(DPIN_DRV_MODE, LOW);
        digitalWrite(DPIN_DRV_EN, LOW);
        break;
    case MODE_LOW:
        Serial.println("Mode = low");
        pinMode(DPIN_PWR, OUTPUT);
        digitalWrite(DPIN_PWR, HIGH);
        digitalWrite(DPIN_DRV_MODE, LOW);
        analogWrite(DPIN_DRV_EN, 64);
        break;
    case MODE_MED:
        Serial.println("Mode = medium");
        pinMode(DPIN_PWR, OUTPUT);
        digitalWrite(DPIN_PWR, HIGH);
        digitalWrite(DPIN_DRV_MODE, LOW);
        analogWrite(DPIN_DRV_EN, 255);
        break;
    case MODE_HIGH:
        Serial.println("Mode = high");
        pinMode(DPIN_PWR, OUTPUT);
        digitalWrite(DPIN_PWR, HIGH);
        digitalWrite(DPIN_DRV_MODE, HIGH);
        analogWrite(DPIN_DRV_EN, 255);
        break;
    }
    mode               = newMode;
    lastModeTransition = millis();  // Update our mode tracker
}
