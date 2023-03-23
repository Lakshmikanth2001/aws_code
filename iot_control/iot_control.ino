#include <Arduino.h>
#include <cstring>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// #include <ESP8266WiFiMulti.h>
// #include <WiFiClientSecureBearSSL.h>
// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
// const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

// Root certificate for teraiot.in
// const char TERAIOT_ROOT [] PROGMEM = R"CERT(
// -----BEGIN CERTIFICATE-----
// MIIF9zCCBN+gAwIBAgIQBCp+Jd6bWJbAX2qhqLklrjANBgkqhkiG9w0BAQsFADA8
// MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRwwGgYDVQQDExNBbWF6b24g
// UlNBIDIwNDggTTAxMB4XDTIzMDMxNTAwMDAwMFoXDTIzMDczMDIzNTk1OVowMDEu
// MCwGA1UEAwwlKi5leGVjdXRlLWFwaS51cy1lYXN0LTIuYW1hem9uYXdzLmNvbTCC
// ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAPKcEGmOj0r1iw650IghA2e/
// UOnfTbCdZaNUsHlSD1cRpDL7ZobXUYD41pQNpgJuafcGhQQDz0jHXvkRMbXgBltq
// n6tGm7zAwL4ZKBaXZNuEdXGf1NjLVGxHv/8tQB2PAU+eM09dvIGzO/zluqlnjG6L
// R6xuyufbC/5EDFopnih+L5nDEgNVPhfeqCo8nd5dc49wj9dH5Mht3uIHc2G6Np8x
// sYWjHdrjbVPVd4tXCvhVvn3BduMKr9jyovPQG116rxl9KBw3ZNdJWP4RWW2RwT0W
// XYa5SbLyZH3v+3j1Oz8YzHJX2D6I0dxsXkYvD/+PiBA4b1iEi4XK1UH2ijJwBXsC
// AwEAAaOCAv8wggL7MB8GA1UdIwQYMBaAFIG4DmOKiRIY5fo7O1CVn+blkBOFMB0G
// A1UdDgQWBBTG3aEnSIUxjJUKzIC8sBKhi1dWzjAwBgNVHREEKTAngiUqLmV4ZWN1
// dGUtYXBpLnVzLWVhc3QtMi5hbWF6b25hd3MuY29tMA4GA1UdDwEB/wQEAwIFoDAd
// BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwOwYDVR0fBDQwMjAwoC6gLIYq
// aHR0cDovL2NybC5yMm0wMS5hbWF6b250cnVzdC5jb20vcjJtMDEuY3JsMBMGA1Ud
// IAQMMAowCAYGZ4EMAQIBMHUGCCsGAQUFBwEBBGkwZzAtBggrBgEFBQcwAYYhaHR0
// cDovL29jc3AucjJtMDEuYW1hem9udHJ1c3QuY29tMDYGCCsGAQUFBzAChipodHRw
// Oi8vY3J0LnIybTAxLmFtYXpvbnRydXN0LmNvbS9yMm0wMS5jZXIwDAYDVR0TAQH/
// BAIwADCCAX8GCisGAQQB1nkCBAIEggFvBIIBawFpAHcA6D7Q2j71BjUy51covIlr
// yQPTy9ERa+zraeF3fW0GvW4AAAGG52VYlQAABAMASDBGAiEAv3BK5EMteabcgwJ+
// TL2epY3KyBLPC3yB3kU+wFSz0B8CIQCbE7jpLGOqXc6TT57iMQeCU69pIDjVd1K4
// uX/dZMliJAB2ALNzdwfhhFD4Y4bWBancEQlKeS2xZwwLh9zwAw55NqWaAAABhudl
// WL4AAAQDAEcwRQIgIbvrfRMxRtkJ6gZjoxlJD5bnMizcfshjv2a1ZUKnzHUCIQC9
// JLeLd1toR/F/90ca2IdvCNM2u/iHUS0C3462vlmJXAB2ALc++yTfnE26dfI5xbpY
// 9Gxd/ELPep81xJ4dCYEl7bSZAAABhudlWHQAAAQDAEcwRQIgAJFLcu3MIIR4tJb+
// aqsCBn74he2OSDhvYJtX1gkZfNoCIQDh+Cq7W3U4x0mPDTPj3V6UkPlLu4o6SDGu
// VZHhwJGL5zANBgkqhkiG9w0BAQsFAAOCAQEAhphybwIFk/ed3QmkmAN40ir4Eg0F
// J+ULXvt7paotE96bQ5plgD7rztUB26aNlucSyVEQetvcuyALxk2LC65qFs6A+WIS
// iGy+f3EU6v3OZ0W7aVcWMCI0ikemCkfYkKTQjyoIxtNS3t4eaV7uEoImCulCXr9u
// AME3PsN4ETiIM/7zfceyldjOaiSYgXj1WuK6Hde0xa8OElqA6L9LwRez0GNlA3gk
// GZbaPuPzr9ilHByq7jxVFqi8feE6K3lHn0Opypkx4mkhbk58X7lfDLcT1phkdF+y
// a7GRpwSE/TWB586ynxpGiRBghc/Yei3SToH+9BMjX/rYvAn9Lz9WGAhhDw==
// -----END CERTIFICATE-----
// )CERT";

// // Create a list of certificates with the server certificate
// X509List cert(TERAIOT_ROOT);

#define SW1 D1
#define SW2 D2
#define SW3 D5
#define SW4 D6

// GPIO Pins
#define ISW1 D7
#define ISW2 3
#define ISW3 9
#define ISW4 10

#define DEVICE_ID "800"
#define RECONNECTION_DELAY 1200
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
// Indicates whether ESP has WiFi credentials saved from previous session
bool initialConfig = false;
int global_http_code = 0;
int reconnection_delay_upcounter = 0;

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
            Serial.printf("[HTTPS] GET Request failed, error code %d => %s \n", httpCode, https.errorToString(httpCode).c_str());
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
            Serial.printf("[HTTPS] POST Request failed, error code %d => %s \n", httpCode, https.errorToString(httpCode).c_str());
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

    int reconnection_response = 0;
    Serial.begin(115200);
    Serial.setDebugOutput(true);

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

    if (WiFi.SSID() == "")
    {
        Serial.println("We haven't got any access point credentials, so get them now");
        initialConfig = true;
    }
    else
    {
        WiFi.mode(WIFI_STA); // configure wifi in station mode

        Serial.println("Previous WiFi SSID " + WiFi.SSID());

        WiFi.begin(WiFi.SSID(), WiFi.psk());

        int up_counter = 0;

        while(up_counter < RECONNECTION_DELAY){
            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.print("Connected to router with IP: ");
                Serial.println(WiFi.localIP());
                return;
            }
            up_counter++;
            delay(100);
        }
    }

    Serial.println("failed to connect, finishing WiFi setup anyway");

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

        // Set time via NTP, as required for x.509 validation
        // configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

        // Serial.print("Waiting for NTP time sync: ");
        // time_t now = time(nullptr);

        // while (now < 8 * 3600 * 2) {
        //   delay(500);
        //   Serial.print(".");
        //   now = time(nullptr);
        // }
        // Serial.println("");

        // struct tm timeinfo;
        // gmtime_r(&now, &timeinfo);

        // Serial.print("Current time: ");
        // Serial.print(asctime(&timeinfo));
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
        // make recoonection_delay_counter = 0 as we are now conneted to a network
        reconnection_delay_upcounter = 0;
        // client->setFingerprint(fingerprint);
        // Or, if you happy to ignore the SSL certificate, then use the following line instead:
        // client->setInsecure();

        HTTPClient https;
        String parameters = device_id + "?hardware=esp8266";
        std::unique_ptr<BearSSL::WiFiClientSecure> http_client(new BearSSL::WiFiClientSecure);
        // http_client->setTrustAnchors(&cert);

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
                    // external power supply is cut
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
            if(global_http_code < 0){
                // network issue
                Serial.println("[HTTPError]: " + String(global_http_code));
            }
            else{
                for (int i = 0; i < response.length(); i++)
                {
                    // make all digital pins low if there is any error on the server side
                    digitalWrite(switches[i], LOW);
                }
                // turn on the internal led if there is any error on the server side
                digitalWrite(LED_BUILTIN, LOW);
            }
        }
    }
    else{
        reconnection_delay_upcounter++;
        Serial.println("Disconnected from wifi");
        if(reconnection_delay_upcounter >= RECONNECTION_DELAY){
            ESP.restart();
        }
    }
    delay(REQUEST_DELAY);
}