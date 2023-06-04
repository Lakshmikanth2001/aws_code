#include <Arduino.h>
#include <cstring>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ESP8266HTTPClient.h>

const char *websockets_connection_string = "wss://teraiot.in/ws"; // Enter server adress

String teraiot_ssl_fingerprint = "";
String api_ssl_finger_print = "";

// GPIO Pins for relay trigger
#define SW1 D1
#define SW2 D2
#define SW3 D5
#define SW4 D6

// GPIO Pins for input
#define ISW1 D3
#define ISW2 D4
#define ISW3 D7
#define ISW4 D8

// External RESET
#define EX_RST 3
#define LED D0

#define DEVICE_ID "800"
#define SWITCH_COUNT 4
#define RECONNECTION_DELAY 1200
#define REQUEST_DELAY 400

const String cloud_url = "https://tfesgx8tv3.execute-api.us-east-2.amazonaws.com/Production/control/";
const String device_id = DEVICE_ID;
String external_bits_string = "";

const int *switches = new int[SWITCH_COUNT]{SW1, SW2, SW3, SW4}; // GPIO pins for the switches
const int *input_switches = new int[SWITCH_COUNT]{ISW1, ISW2, ISW3, ISW4};
uint8_t *external_bits = new uint8_t[SWITCH_COUNT]{HIGH, HIGH, HIGH, HIGH};

const char *default_device_id = device_id.c_str();

bool register_mac_id = false;
bool avaliable_finger_prints = false;
// id - label - default value - length
WiFiManager wm; // to configure wifi from 192.168.4.1 by default
int global_http_code = 0;
unsigned int bit_count = 0;

websockets::WebsocketsClient websocket_client;

String bits_array_to_string(uint8_t *array, uint8_t size)
{
    String bits_string = "";
    for (size_t i = 0; i < size; i++)
    {
        bits_string = bits_string + String(array[i]);
    }
    return bits_string;
}

String makeGetRequest(std::unique_ptr<BearSSL::WiFiClientSecure> &http_client, String url, String parameters)
{
    // Serial.print("[HTTPS] GET Request begin...\n");

    HTTPClient https;
    String response;
    if (https.begin(*http_client, url + parameters))
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
    // Serial.print("[HTTPS] GET Request Ends...\n");
    return response;
}

String makePostRequest(std::unique_ptr<BearSSL::WiFiClientSecure> &http_client, String url, String payload)
{
    Serial.print("[HTTPS] POST Request begin...\n");

    HTTPClient https;
    String response;
    if (https.begin(*http_client, url))
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

String fetchFingerPrints(std::unique_ptr<BearSSL::WiFiClientSecure> &http_client, String url)
{
    http_client->setInsecure();
    HTTPClient https;
    String response;
    if (https.begin(*http_client, url))
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

IRAM_ATTR void read_external_bits()
{
    for (int i = 0; i < bit_count; i++)
    {
        external_bits[i] = digitalRead(input_switches[i]);
    }
}

void onMessageCallback(websockets::WebsocketsMessage message)
{
    Serial.print("Got Message: ");
    Serial.println(message.data());
}

void onEventsCallback(websockets::WebsocketsEvent event, String data)
{
    if (event == websockets::WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("Connnection Opened");
    }
    else if (event == websockets::WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("Connnection Closed");
    }
    else if (event == websockets::WebsocketsEvent::GotPing)
    {
        Serial.println("Got a Ping!");
    }
    else if (event == websockets::WebsocketsEvent::GotPong)
    {
        Serial.println("Got a Pong!");
    }
}

void webSocketSetup()
{
    // run callback when messages are received
    websocket_client.onMessage(onMessageCallback);

    // run callback when events are occuring
    websocket_client.onEvent(onEventsCallback);

    // Before connecting, set the ssl fingerprint of the server
    if (teraiot_ssl_fingerprint != "")
    {
        websocket_client.setFingerprint(teraiot_ssl_fingerprint.c_str());
    }

    // Connect to server
    websocket_client.connect(websockets_connection_string);

    // Send a message
    websocket_client.send("Hello Server");

    // Send a ping
    websocket_client.ping();

    return;
}

bool fetchFingerPrints()
{

    Serial.println("Fetching certificates");

    std::unique_ptr<BearSSL::WiFiClientSecure> http_client(new BearSSL::WiFiClientSecure);

    String finger_print_url = cloud_url + "0000" + "?fetch=finger_print";

    String fingerprints = fetchFingerPrints(http_client, finger_print_url);

    if (fingerprints == "-1")
    {
        Serial.println("Failed to fetch certificates");
        return false;
    }

    String finger_print_1, finger_print_2;

    bool breakFlag = false;

    // iterate over the fingerprints string
    for (int i = 0; i < fingerprints.length(); i++)
    {
        if (fingerprints[i] == '|')
        {
            breakFlag = true;
            continue;
        }
        if (!breakFlag)
        {
            api_ssl_finger_print += fingerprints[i];
        }
        else
        {
            teraiot_ssl_fingerprint += fingerprints[i];
        }
    }
    return true;
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
    pinMode(LED, OUTPUT);
    pinMode(EX_RST, INPUT);

    pinMode(ISW1, INPUT);
    pinMode(ISW2, INPUT);
    pinMode(ISW3, INPUT);
    pinMode(ISW4, INPUT);

    attachInterrupt(digitalPinToInterrupt(ISW1), read_external_bits, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ISW2), read_external_bits, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ISW3), read_external_bits, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ISW4), read_external_bits, CHANGE);

    for (uint8_t t = 4; t > 0; t--)
    {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    if (WiFi.SSID() == "")
    {
        Serial.println("We haven't got any access point credentials, so get them now");
    }
    else
    {
        WiFi.mode(WIFI_STA); // configure wifi in station mode

        Serial.println("Previous WiFi SSID " + WiFi.SSID());

        WiFi.begin(WiFi.SSID(), WiFi.psk());

        Serial.println("Trying for previous WiFi : " + WiFi.SSID());

        while (true)
        {
            int wiFiStatus = WiFi.status();
            if (wiFiStatus == WL_CONNECTED)
            {
                Serial.print("Connected to router with IP: ");
                Serial.println(WiFi.localIP());

                avaliable_finger_prints = fetchFingerPrints();
                if (avaliable_finger_prints)
                {
                    webSocketSetup();
                }
                return;
            }
            if (wiFiStatus == WL_WRONG_PASSWORD)
            {
                Serial.print(WiFi.SSID() + " password is changed so re-enter credentials ");
                break;
            }
            if (digitalRead(EX_RST) == LOW)
            {
                Serial.println("EX_RST PIN TRIGGERED");
                break;
            }
            delay(100);
        }
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

        avaliable_finger_prints = fetchFingerPrints();
        if (avaliable_finger_prints)
        {
            webSocketSetup();
        }

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

void loop()
{
    if ((WiFi.status() == WL_CONNECTED))
    {
        // make recoonection_delay_counter = 0 as we are now conneted to a network
        // client->setFingerprint(fingerprint);
        // Or, if you happy to ignore the SSL certificate, then use the following line instead:
        // client->setInsecure();
        // HTTPClient https;
        String parameters;
        if (bit_count == 0)
        {
            parameters = device_id + "?hardware=esp8266";
        }
        else
        {
            external_bits_string = bits_array_to_string(external_bits, bit_count);
            parameters = device_id + "?hardware=esp8266&external_bits=" + external_bits_string;
        }

        std::unique_ptr<BearSSL::WiFiClientSecure> http_client(new BearSSL::WiFiClientSecure);

        if (!avaliable_finger_prints)
        {
            http_client->setInsecure();
        }
        else
        {
            http_client->setFingerprint(api_ssl_finger_print.c_str());
        }

        if (!register_mac_id)
        {
            Serial.println("finger prints fetched => " + api_ssl_finger_print + "|" + teraiot_ssl_fingerprint);
            Serial.println("ESP8266 MAC Address : " + WiFi.macAddress());

            String post_responce = makePostRequest(http_client, cloud_url + device_id, WiFi.macAddress() + "|" + WiFi.SSID());
            register_mac_id = true;
        }
        // websocket_client.send("greeeting from esp8266");
        // websocket_client.poll();

        String response = makeGetRequest(http_client, cloud_url, parameters);
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
            }
            bit_count = response.length();
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
                    // Serial.println("External Power Supply is turned off for " + String(i) + " switch at " + String(input_switches[i]));
                }
            }
            digitalWrite(LED, HIGH);
        }
        else
        {
            Serial.println("[ERROR] HTTP code: " + String(global_http_code));
            if (global_http_code < 0)
            {
                // network issue
                Serial.println("[HTTPError]: " + String(global_http_code));
            }
            else
            {
                for (int i = 0; i < response.length(); i++)
                {
                    // make all digital pins low if there is any error on the server side
                    digitalWrite(switches[i], LOW);
                }
                // turn on the internal led if there is any error on the server side
                digitalWrite(LED, LOW);
            }
        }
    }
    else if (WiFi.status() == WL_WRONG_PASSWORD)
    {
        wm.resetSettings();
        ESP.restart();
    }
    else
    {
        Serial.println("Disconnected from wifi");
    }
    delay(REQUEST_DELAY);
}

void serialEvent() {
    Serial.println("serial event");
    while(Serial.available()) {
        String command = Serial.readStringUntil('\n');
        Serial.println(command);

        if(command.indexOf(DEVICE_ID) >= 0){
            websocket_client.send(command);
        }
        else{
            Serial.println("invalid command");
        }
    }
    Serial.flush();
    Serial.end();
    Serial.begin(115200);
}