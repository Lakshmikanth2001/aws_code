#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define PWM_PIN D4
#define DEVICE_ID "600"
#define PWM_FREQ 500
#define WIFI_SERVER_PASSWORD "teraiot@143"

const String cloud_url = "https://tfesgx8tv3.execute-api.us-east-2.amazonaws.com/Production/control/";
const String device_id = DEVICE_ID;
WiFiManager wm;
String api_finger_print = "-1";
bool register_mac_id = false;
int global_http_code = 0;

String makeGetRequest(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url, String parameters)
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

String makePostRequest(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url, String payload)
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

String fetchFingerPrints(std::unique_ptr<BearSSL::WiFiClientSecure> &client, String url)
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

void wifi_setup()
{
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

    if (!wm.autoConnect(wifiName.c_str(), WIFI_SERVER_PASSWORD))
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

void set_api_finger_print(std::unique_ptr<BearSSL::WiFiClientSecure> &http_client)
{
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
        String finger_print_fetched = fetchFingerPrints(http_client, cloud_url + device_id + "?fetch=finger_print");
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
        String post_responce = makePostRequest(http_client, cloud_url + device_id, WiFi.macAddress());
        register_mac_id = true;
    }
}

void setup()
{
    // put your setup code here, to run once:

    // setting up wifi using WiFiManager external library
    wifi_setup();

    // PWM PIN in analog OUTPUT Mode
    pinMode(PWM_PIN, OUTPUT);

    analogWriteFreq(PWM_FREQ);
}

void loop()
{
    // put your main code here, to run repeatedly:

    if ((WiFi.status() == WL_CONNECTED))
    {
        String parameters = device_id + "?hardware=esp8266";
        std::unique_ptr<BearSSL::WiFiClientSecure> http_client(new BearSSL::WiFiClientSecure);

        // cloud URL finger print
        set_api_finger_print(http_client);

        String response = makeGetRequest(http_client, cloud_url, parameters);

        if (global_http_code == 200 || global_http_code == 201)
        {
            analogWrite(PWM_PIN, response.toInt());
        }
    }
}
