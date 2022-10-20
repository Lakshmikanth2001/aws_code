#include <Arduino.h>
#include <cstring>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// #include <ESP8266WiFiMulti.h>
// #include <WiFiClientSecureBearSSL.h>
// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
// const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

#define SW1 D1
#define SW2 D2
#define SW3 D5
#define SW4 D6

// GPIO Pins
#define ISW1 D7
#define ISW2 3
#define ISW3 9
#define ISW4 10

#define DEVICE_ID "600"
#define WIFI_DOWN_COUNTER_OVERFLOW 200000
#define REQUEST_DELAY 200

const String cloud_url = "https://tfesgx8tv3.execute-api.us-east-2.amazonaws.com/Production/control/";
const String device_id = DEVICE_ID;
const int *switches = new int[4]{SW1, SW2, SW3, SW4}; // GPIO pins for the switches
const int *input_switches = new int[4]{ISW1, ISW2, ISW3, ISW4};
const char *default_device_id = device_id.c_str();

String api_finger_print = "-1";
bool register_mac_id = false;
// id - label - default value - length
WiFiManager wm; // to configure wifi from 192.168.4.1 by default
bool openWebPortal = false;
int global_http_code = 0;
int wifiDownCounter = WIFI_DOWN_COUNTER_OVERFLOW;

String make_get_request(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url, String parameters)
{
    Serial.print("[HTTPS] GET Request begin...\n");

    HTTPClient https;
    String response;
    if (https.begin(*client, url + parameters))
    {
        int httpCode = https.GET();
        global_http_code = httpCode;
        String payload = https.getString();

        if (httpCode == 200 || httpCode == 201)
        {
            response = payload;
        }
        else
        {
            Serial.printf("[HTTPS] GET Request failed, error code %d\n", httpCode);
            response = "[HTTPS] GET Request failed";
        }
    }
    else
    {
        response = "[HTTPS] GET Request failed";
    }
    https.end();
    Serial.print("[HTTPS] GET Request Ends...\n");
    return response;
}

String make_post_request(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url, String payload)
{

    Serial.print("[HTTPS] POST Request begin...\n");
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:
    // client->setInsecure();

    HTTPClient https;
    String response;
    if (https.begin(*client, url))
    {
        int httpCode = https.POST(payload);
        global_http_code = httpCode;
        String post_payload = https.getString();

        if (httpCode == 200 || httpCode == 201)
        {
            response = post_payload;
        }
        else
        {
            Serial.printf("[HTTPS] POST Request failed, error code %d\n", httpCode);
            response = "[HTTPS] POST Request failed";
        }
    }
    else
    {
        response = "[HTTPS] GET Request failed";
    }
    https.end();
    Serial.print("[HTTPS] POST Request Ends...\n");
    return response;
}

String fetch_finger_print(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url)
{
    client->setInsecure();
    HTTPClient https;
    String response;
    if (https.begin(*client, url))
    {
        int httpCode = https.GET();
        global_http_code = httpCode;
        String payload = https.getString();
        if (httpCode == 200 || httpCode == 201)
        {
            response = payload;
        }
        else
        {
            Serial.printf("[HTTPS] Failed to fetch finger print, error code %d\n", httpCode);
            response = "[HTTPS] GET Request failed";
        }
    }
    else
    {
        response = "-1";
    }
    https.end();
    Serial.print("[HTTPS] GET Request for fingerprint Ends...\n");
    return response;
}

void setup()
{

    Serial.begin(115200);
    // Serial.setDebugOutput(true);

    pinMode(SW1, OUTPUT);
    pinMode(SW2, OUTPUT);
    pinMode(SW3, OUTPUT);
    pinMode(SW4, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

    pinMode(ISW1, INPUT);
    pinMode(ISW2, INPUT);
    pinMode(ISW3, INPUT);
    pinMode(ISW4, INPUT);

    // pinMode(16, OUTPUT);

    for (uint8_t t = 4; t > 0; t--)
    {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    WiFi.mode(WIFI_STA); // configure wifi in station mode

    wm.setAPStaticIPConfig(IPAddress(80, 80, 80, 1), IPAddress(80, 80, 80, 1), IPAddress(255, 255, 255, 0));
    // wm.addParameter(&UserSpecifiedDevice);

    wm.setTitle("TeraIoT");

    // get default HTTP_HEAD_START

    // inject css
    const char *css = " \
        <style> \
        body {\
            color: white;\
            background: #232526;\
            background: -webkit-linear-gradient(to right,\
                    #414345,\
                    #232526);\
            background: linear-gradient(to right,\
                    #414345,\
                    #232526);\
        }\
        a {\
            color:white\
        }\
        .logo{\
            margin:15px;\
        }\
        </style>\
    ";
    wm.setCustomHeadElement(css);
    // commet it our during production
    // wm.resetSettings();

    String wifiName = "teraiot@" + device_id;

    if (!wm.autoConnect(wifiName.c_str(), "teraiot@143"))
    {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected to you local WiFi :");
    }
}

uint8_t *read_external_bits(int bits_count)
{
    uint8_t *external_bits = new uint8_t[bits_count];
    for (int i = 0; i < bits_count; i++)
    {
        external_bits[i] = digitalRead(input_switches[i]);
    }
    return external_bits;
}

void loop()
{
    if ((WiFi.status() == WL_CONNECTED))
    {
        // client->setFingerprint(fingerprint);
        // Or, if you happy to ignore the SSL certificate, then use the following line instead:
        // client->setInsecure();

        HTTPClient https;
        String parameters = device_id + "?hardware=esp8266";
        std::unique_ptr<BearSSL::WiFiClientSecure> http_client(new BearSSL::WiFiClientSecure);

        if (api_finger_print == "-1")
        {
            http_client->setInsecure();
        }
        else
        {
            http_client->setFingerprint(api_finger_print.c_str());
        }

        if (!register_mac_id)
        {
            String finger_print_fetched = fetch_finger_print(http_client, cloud_url + device_id + "?fetch=finger_print");
            if (finger_print_fetched == "-1")
            {
                http_client->setInsecure();
            }
            else
            {
                http_client->setFingerprint(finger_print_fetched.c_str());
                api_finger_print = finger_print_fetched;
            }
            Serial.println("finger print fetched = " + finger_print_fetched);
            Serial.println("ESP8266 MAC Address : " + WiFi.macAddress());
            String post_responce = make_post_request(http_client, cloud_url + device_id, WiFi.macAddress());
            register_mac_id = true;
        }

        String response = make_get_request(http_client, cloud_url, parameters);
        if (global_http_code == 200 || global_http_code == 201)
        {

            // we are explicitly clearing the wifi crendentials
            if (response == "-1")
            {
                Serial.println("Reseting Wifi Credentials");
                // clear out wifi credentials
                wm.resetSettings();

                // restart esp8266 in wifi station mode
                ESP.restart();
            }
            else
            {
                for (int i = 0; i < response.length(); i++)
                {
                    // state of the switches
                    if (response[i] == '1')
                    {
                        digitalWrite(switches[i], HIGH);
                    }
                    else if (response[i] == '0')
                    {
                        digitalWrite(switches[i], LOW);
                    }
                }
                digitalWrite(LED_BUILTIN, HIGH);
            }
            uint8_t *external_bits = read_external_bits(response.length());
            for (int i = 0; i < response.length(); i++)
            {
                // state of the switches
                if (response[i] == '1' && external_bits[i] == HIGH)
                {
                    // pass this condition rare condition internal hardware failure
                }
                else if (response[i] == '0' && external_bits[i] == LOW)
                {
                    // pass this condition
                    // external power supply is cut down
                }
                else
                {
                    Serial.println("External Power Supply is turned off for " + String(i) + " switch at " + String(input_switches[i]));
                }
            }
        }
        else
        {
            Serial.println("[ERROR] HTTP code: " + String(global_http_code));
            for (int i = 0; i < response.length(); i++)
            {
                // make all digital pins low if there is any error on the server side
                digitalWrite(switches[i], LOW);
            }
            // turn on the internal led if there is any error on the server side
            digitalWrite(LED_BUILTIN, LOW);
        }
    }
    else
    {
        if (!openWebPortal && wifiDownCounter > 0)
        {
            Serial.println("Dissconnected to Current Wifi");
            delay(100);
            wifiDownCounter--;
        }
        else if (wifiDownCounter == 0)
        {
            openWebPortal = true;
            wifiDownCounter = WIFI_DOWN_COUNTER_OVERFLOW;
        }
        else
        {
            openWebPortal = false;
            Serial.println("Reseting Wifi Credentials");
            // clear out wifi credentials
            wm.resetSettings();
            ESP.restart();
        }
    }
    delay(REQUEST_DELAY);
}