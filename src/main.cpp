// Example Arduino/ESP8266 code to upload data to Google Sheets
// Follow setup instructions found here:
// https://github.com/StorageB/Google-Sheets-Logging
// reddit: u/StorageB107
// email: StorageUnitB@gmail.com

#include <Arduino.h>
#include <NTPClient.h>
#include <ArduinoJson.h> // Include the ArduinoJson library
#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include <WiFiManager.h>

#include <pms.h>
#include <DHTesp.h>

// ###########################################################
// #####################SETUP Gapp Script#####################
// ###########################################################

// Enter network credentials:
// const char* ssid     = "TANG 8";
// const char* password = "88888888";
const char *ssid = "Mike Homestay 4 2.4GHz";
const char *password = "vietnam1";

// Enter Google Script Deployment ID:
const char *GScriptId = "AKfycbxUjBnKnfZZgr2jZw2yLCmDdXdZl6BAxd3pF-jQfVrHiWbYD-M";
// Create a JSON document
StaticJsonDocument<200> doc;

// Google Sheets setup (do not edit)
const char *host = "script.google.com";
String url = String("/macros/s/") + GScriptId + "/exec";

// set https
const int httpsPort = 443;
HTTPSRedirect *client = nullptr;
const char *ntpServer = "pool.ntp.org"; // NTP server address
// keep time correct
long gmtOffset_sec = 0;     // Offset from GMT in seconds
int daylightOffset_sec = 0; // Daylight saving time offset in seconds


// ###########################################################
// #########################SETUP PMS#########################
// ###########################################################

#define PMS7001_TX D6 // blue
#define PMS7001_RX D7 // black
#define DHT_IN D5
Pmsx003 pms(PMS7001_TX, PMS7001_RX); // PMS7001-tx_pin, PMS7001-rx_pin
int sec_wait_main_loop = 600;

DHTesp dht;

// Example Arduino/ESP8266 code to upload data to Google Sheets
// Follow setup instructions found here:
// https://github.com/StorageB/Google-Sheets-Logging
// reddit: u/StorageB107
// email: StorageUnitB@gmail.com

// ###########################################################
// ######################MEASURE FUNCT########################
// ###########################################################

std::pair<uint16_t, uint16_t> getAveragePm2dot5(int nb_measurement)
{
  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];
  uint16_t measurement_data[10] = {};
  uint16_t current_measurement = 0;
  auto prior_measurement_timestamp = 0;
  uint16_t current_measurement_duration = 0;
  uint16_t measurement_duration = 0;

  Serial.println("Waking-up the sensor...");
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
  delay(5000);

  Serial.println("starting measurement...");
  for (int i = 0; i < 10; ++i)
  {
    prior_measurement_timestamp = millis();
    for (int j = 0; j < 200; j++)
    {
      Pmsx003::PmsStatus status = pms.read(data, n);
      delay(200);
      switch (status)
      {
      case Pmsx003::OK:
      {
        current_measurement_duration = millis() - prior_measurement_timestamp;
        measurement_duration += current_measurement_duration;
        Serial.print((String) "measurement " + i + " succeeded in " + measurement_duration + " ms : ");
        j = 200;
        // For loop starts from 3
        // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
        for (size_t i = Pmsx003::PM1dot0; i < n - 6; ++i)
          Serial.print((String) "\t" + data[i] + " " + Pmsx003::dataNames[i]);
        Serial.println();
        current_measurement = data[4];
      }
      case Pmsx003::noData:
        break;
      default:
        Serial.println((String) "measurement " + i + " failed (" + Pmsx003::errorMsg[status] + "), retrying...");
        // Serial.println(Pmsx003::errorMsg[status]);
      };
    }

    if (current_measurement != 0)
    {
      measurement_data[i] = current_measurement;
    }
    else
    {
      Serial.println("Couldn't get any data from the sensor");
      return std::make_pair(0, 0);
    }
    // delay between each 10 measurements
    delay(2000);
  }
  Serial.println();

  Serial.println("measurement finish");

  int nb_element = sizeof(measurement_data) / sizeof(measurement_data[0]);
  int epsilon_avg[10] = {};
  // get the average epsilon for each measurement
  for (int i = 0; i < nb_element; i++)
  {
    int epsilon_sum = 0;
    for (int j = 0; j < nb_element; j++)
      epsilon_sum += abs(measurement_data[i] - measurement_data[j]);
    epsilon_avg[i] = (int)round(epsilon_sum / nb_element);
  }

  uint16_t reference = measurement_data[0];
  int smallest_epsilon = epsilon_avg[0];
  for (int i = 0; i < nb_element; i++)
  {
    if (epsilon_avg[i] < smallest_epsilon)
    {
      smallest_epsilon = epsilon_avg[i];
      reference = measurement_data[i];
    }
  }
  Serial.println((String) "Measurement value " + reference + " has the smallest averaged Epsilon: " + smallest_epsilon);

  // if the measurement epsilon is less than 20 off from the reference, then,
  // take into the avg calculation
  uint16_t sum = 0;
  uint16_t pm2dot5_avg = 0;
  int nb_valid_measurement = 0;
  for (int i = 0; i < nb_element; i++)
  {
    if (epsilon_avg[i] < 15)
    {
      sum += measurement_data[i];
      nb_valid_measurement++;
    }
    else
      Serial.println((String) "measurement " + i + " was rejected! the value " + measurement_data[i] + " has an averaged Epsilon of " + epsilon_avg[i]);
  }
  pm2dot5_avg = (uint16_t)round(sum / nb_valid_measurement);

  return std::make_pair(pm2dot5_avg, measurement_duration);
}

// ###########################################################
// #####################HTTPS FUNCTIONS#######################
// ###########################################################

// Function to connect to the server using HTTPSRedirect
bool connectToServer(HTTPSRedirect *&client, const char *host, int httpsPort)
{

  // Initialize client if null
  if (client == nullptr)
  {
    // Use HTTPSRedirect class to create a new TLS connection
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
    Serial.print("Connecting to ");
    Serial.println(host); // try to connect with "script.google.com"
  }

  // Attempt to connect multiple times
  for (int attempt = 0; attempt < 5; ++attempt)
  {
    Serial.print("Connection attempt ");
    Serial.println(attempt + 1);

    if (client->connect(host, httpsPort))
    {
      Serial.println("Connected successfully.");
      return true;
    }
    else
    {
      Serial.println("Connection failed, retrying...");
      delay(1000); // Wait before retrying
    }
  }

  Serial.print("Failed to connect to server: ");
  Serial.println(host);
  return false;
}

// ###########################################################
// #########################SETUP#############################
// ###########################################################

void setup()
{
  // Starting Software Serial and waiting its "ready state"
  Serial.begin(115200);
  while (!Serial)
  {
  };

  Serial.println(" Initializing Pms7003...");
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);

  Serial.println("Initializing DHT11 on PIN D5...");
  dht.setup(DHT_IN, DHTesp::DHT11);

  Serial.println("Connect to wifi...");
  // Connect to WiFi
  WiFiManager wifiManager;
  wifiManager.autoConnect();

  Serial.println("Setting up HTTPS connexion >>>");

  Serial.println("Initialize the NTPClient...");
  // Initialize the NTPClient
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);
  Serial.println("Synchronize time...");
  // Synchronize time
  timeClient.begin();

  // Use HTTPSRedirect class to create a new TLS connection
  if (!connectToServer(client, host, httpsPort))
  {
    Serial.println("Exiting setup due to connection failure.");
    return;
  }

  // Add data to the JSON document
  doc["command"] = "append_row";
  doc["sheet_name"] = "Sheet1";
}

// ###########################################################
// #########################LOOP##############################
// ###########################################################
bool measurement_sucessful = 0;
void loop(void)
{
  // Ensure client is connected before attempting to POST data
  if (client == nullptr || !client->connected())
  {
    if (!connectToServer(client, host, httpsPort))
    {
      Serial.println("Failed to connect in loop. Retrying in next iteration.");
      return;
    }
  }
  // get pm2.5 measurement
  std::pair<int, int> measurement_data = getAveragePm2dot5(10);

  // get humidity and temperature from DHT11 sensor
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  static bool flag = false;
  if (!flag)
  {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr)
  {
    if (!client->connected())
    {
      client->connect(host, httpsPort);
    }
  }
  else
  {
    Serial.println("Error creating client object!");
  }

  // Create a nested JSON object for values
  JsonObject values = doc.createNestedObject("values");
  values["pm2dot5_avg"] = measurement_data.first;
  values["measurement_duration"] = measurement_data.second;

  // Add temperature and humidity data
  values["temperature"] = temperature;
  values["humidity"] = humidity;

  // Serialize the JSON document to a string
  String payload;
  serializeJson(doc, payload);

  // Publish data to Google Sheets
  Serial.println("Publishing data...");
  Serial.println(payload);
  if (client->POST(url, host, payload))
  {
    // do stuff here if publish was successful
    Serial.println("Data successfully uploaded to spreadsheet");
  }
  else
  {
    // do stuff here if publish was not successful
    Serial.println("Error while connecting");
  }

  Serial.println((String) "end of loop, putting the sensor to sleep for: " + sec_wait_main_loop);
  pms.write(Pmsx003::cmdSleep);
  delay(sec_wait_main_loop * 1000);
}
