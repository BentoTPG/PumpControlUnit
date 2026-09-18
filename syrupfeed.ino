#include <Arduino.h>
#include <ArduinoJson.h>
#define MAX_PUMP 8 //pump amount
// =========================
// Relay pins
// =========================

const int relayPins[MAX_PUMP] = {
    25,  // CH1 GPIO25
    26,  // CH2 GPIO26
    27,  // CH3 GPIO27
    14,  // CH4 GPIO14
    16,  // CH5 GPIO16
    17,  // CH6 GPIO17
    18,  // CH7 GPIO18
    19   // CH8 GPIO19
};

const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;


// =========================
// Pump configuration
// =========================

struct Pump {

    String name;

    bool running;

    unsigned long startTime;
    unsigned long duration;

    float flowRate;   // mL per second
};


// Change these according to your actual calibration
Pump pumps[MAX_PUMP] = {

    {"syrup", false, 0, 0, 1.0},

    {"limejuice",    false, 0, 0, 1.0},

    {"vodka",  false, 0, 0, 1.0},

    {"gin",    false, 0, 0, 1.0},

    {"pineapple",    false, 0, 0, 1.0},

    {"cranberry",    false, 0, 0, 1.0},

    {"butterflypea",    false, 0, 0, 1.0},

    {"sprite",    false, 0, 0, 1.0},
};


// =========================
// Serial buffer
// =========================

String serialBuffer;


// =========================
// Start pump
// =========================

void startPump(int pumpIndex, float amountML)
{
    if (pumpIndex < 0 || pumpIndex >= MAX_PUMP) {
        return;
    }

    if (amountML <= 0) {
        return;
    }

    Pump &pump = pumps[pumpIndex];

    // Convert mL -> milliseconds
    unsigned long duration =
        (unsigned long)((amountML / pump.flowRate) * 1000.0);

    pump.running = true;
    pump.startTime = millis();
    pump.duration = duration;

    digitalWrite(relayPins[pumpIndex], RELAY_ON);

    Serial.print("{\"status\":\"started\"");
    Serial.print(",\"pump\":\"");
    Serial.print(pump.name);
    Serial.print("\"");
    Serial.print(",\"amount_ml\":");
    Serial.print(amountML);
    Serial.println("}");
}


// =========================
// Stop pump
// =========================

void stopPump(int pumpIndex)
{
    if (pumpIndex < 0 || pumpIndex >= MAX_PUMP) {
        return;
    }

    Pump &pump = pumps[pumpIndex];

    digitalWrite(relayPins[pumpIndex], RELAY_OFF);

    pump.running = false;
    pump.duration = 0;

    Serial.print("{\"status\":\"stopped\"");
    Serial.print(",\"pump\":\"");
    Serial.print(pump.name);
    Serial.println("\"}");
}


// =========================
// Find pump by name
// =========================

int findPump(String name)
{
    for (int i = 0; i < MAX_PUMP; i++) {

        if (name.equalsIgnoreCase(pumps[i].name)) {
            return i;
        }
    }

    return -1;
}


// =========================
// Process JSON
// =========================

void processCommand(String json)
{
    StaticJsonDocument<1024> doc;

    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println(
            "{\"status\":\"error\",\"message\":\"invalid_json\"}"
        );
        return;
    }

    // Check command
    String command = doc["command"] | "";

    if (command != "dispense") {
        Serial.println(
            "{\"status\":\"error\",\"message\":\"unknown_command\"}"
        );
        return;
    }

    // Check pumps array
    if (!doc.containsKey("pumps") ||
        !doc["pumps"].is<JsonArray>()) {

        Serial.println(
            "{\"status\":\"error\",\"message\":\"missing_pumps_array\"}"
        );
        return;
    }

    JsonArray pumpArray = doc["pumps"].as<JsonArray>();

    // Process every pump
    for (JsonObject pumpData : pumpArray) {

        String pumpName =
            pumpData["pump"] | "";

        float amountML =
            pumpData["amount_ml"] | 0.0;

        // Check data
        if (pumpName == "" || amountML <= 0) {

            Serial.println(
                "{\"status\":\"error\",\"message\":\"invalid_pump_data\"}"
            );

            continue;
        }

        // Find corresponding pump
        int pumpIndex = findPump(pumpName);

        if (pumpIndex == -1) {

            Serial.print(
                "{\"status\":\"error\",\"message\":\"unknown_pump\",\"pump\":\""
            );

            Serial.print(pumpName);

            Serial.println("\"}");

            continue;
        }

        // Start this pump
        startPump(pumpIndex, amountML);
    }
}

// =========================
// Read Serial
// =========================

void readSerial()
{
    while (Serial.available()) {

        char c = Serial.read();

        if (c == '\n') {

            serialBuffer.trim();

            if (serialBuffer.length() > 0) {

                processCommand(serialBuffer);

            }

            serialBuffer = "";
        }

        else {

            serialBuffer += c;

            if (serialBuffer.length() > 512) {

                serialBuffer = "";

                Serial.println(
                    "{\"status\":\"error\",\"message\":\"buffer_overflow\"}"
                );
            }
        }
    }
}


// =========================
// Update pumps
// =========================

void updatePumps()
{
    unsigned long now = millis();

    for (int i = 0; i < MAX_PUMP; i++) {

        if (!pumps[i].running) {
            continue;
        }


        if (now - pumps[i].startTime >=
            pumps[i].duration) {

            stopPump(i);
        }
    }
}


// =========================
// Setup
// =========================

void setup()
{
    Serial.begin(115200);

    for (int i = 0; i < MAX_PUMP; i++) {

        pinMode(relayPins[i], OUTPUT);

        digitalWrite(
            relayPins[i],
            RELAY_OFF
        );
    }

    Serial.print("{\"status\":\"ready\",\"pumps\":");
    Serial.print(MAX_PUMP);
    Serial.println("}");
}


// =========================
// Loop
// =========================

void loop()
{
    readSerial();

    updatePumps();
}