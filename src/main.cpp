// Example Arduino/ESP8266 code to upload data to Google Sheets
// Follow setup instructions found here:
// https://github.com/StorageB/Google-Sheets-Logging
// reddit: u/StorageB107
// email: StorageUnitB@gmail.com

#include <Arduino.h>
#include <ArduinoJson.h> // Include the ArduinoJson library
// #include <ESP8266WiFi.h>
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
// const char *ssid = "Mike Homestay 4 2.4GHz";
// const char *password = "vietnam1";

// Enter Google Script Deployment ID:
const char *GScriptId = "AKfycbxA5gwluyL3Nw2FK_tnUW2ix276BEaCVnsFYEc3JJIDYNHr_sQJUNh3bzOdgZyVIiI";
// Create a JSON document
StaticJsonDocument<200> doc;

// Enter command (insert_row or append_row) and your Google Sheets sheet name (default is Sheet1):
String payload_basev1 = "{\"command\": \"append_row\", \"sheet_name\": \"Sheet1\", \"values\": ";

// Google Sheets setup (do not edit)
const char *host = "script.google.com";
String url = String("/macros/s/") + GScriptId + "/exec";

// columns name must match otherwise get (not helpful error) Error! Not connected to host.
// Timestamp col get filled by the script
// order do matter
const char *columns[] = {
    "Temperature",
    "Humidity",
    "PM2.5",
    "Meas. duration"};

// set https
const int httpsPort = 443;
HTTPSRedirect *client = nullptr;

// ###########################################################
// #########################SETUP PMS#########################
// ###########################################################

#define PMS7001_TX D6 // blue
#define PMS7001_RX D7 // black
#define DHT_IN D5
Pmsx003 pms(PMS7001_TX, PMS7001_RX); // PMS7001-tx_pin, PMS7001-rx_pin

// ####measurement frequency (in sec)####
int measurement_freq = 600;

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
  // wait for it to wake-up. 5000 seems the be the minimum time for it wakeup
  // but waitForData is not working where delay with same amount of time does
  delay(5000);
  // pms.waitForData(5000);

  Serial.println("starting measurement...");
  for (int i = 0; i < 10; ++i)
  {
    prior_measurement_timestamp = millis();
    for (int j = 0; j < 200; j++)
    {

      Pmsx003::PmsStatus status = pms.read(data, n);
      pms.waitForData(2000, n);
      switch (status)
      {
      case Pmsx003::OK:
      {
        current_measurement_duration = millis() - prior_measurement_timestamp;
        measurement_duration += current_measurement_duration;
        Serial.print((String) "measurement " + i + " succeeded in " + measurement_duration + " ms : ");
        // For loop starts from 3
        // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
        for (size_t i = Pmsx003::PM1dot0; i < n - 6; ++i)
          Serial.print((String) "\t" + data[i] + " " + Pmsx003::dataNames[i]);
        Serial.println();
        current_measurement = data[4];
        j = 200;
        break;
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

  Serial.println((String) "Putting the sensor to sleep");
  pms.write(Pmsx003::cmdSleep);
  return std::make_pair(pm2dot5_avg, measurement_duration);
}

// ###########################################################
// #########################SETUP#############################
// ###########################################################

void setup()
{
  // Starting Software Serial and waiting its "ready state"
  Serial.begin(9600);
  while (!Serial)
  {
  };

  Serial.println("Connect to wifi...");
  // Connect to WiFi
  WiFiManager wifiManager;
  wifiManager.autoConnect();

  Serial.println("Setting up HTTPS connexion >>>");

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  for (int i = 0; i < 5; i++)
  {
    if (client->connect(host, httpsPort))
    {
      Serial.print("Connected to ");
      Serial.println(host);
      break;
    }
    else
      Serial.println((String) "Connection failed. Retrying " + i + "/5 ...");
  }

  delete client;    // delete HTTPSRedirect object
  client = nullptr; // delete HTTPSRedirect object

  Serial.println(" Initializing Pms7003...");
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModeActive);

  Serial.println("Initializing DHT11 on PIN D5...");
  dht.setup(DHT_IN, DHTesp::DHT11);

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
  // make sure our PM2.5 sensor sleep
  pms.write(Pmsx003::cmdSleep);

  if (client == nullptr)
  {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client == nullptr)
  {
    Serial.println("Error creating client object!");
  }
  else
  {
    if (!client->connected())
    {
      if (client->connect(host, httpsPort))
      {
        Serial.print("Connected to ");
        Serial.println(host);
      }
      else
      {
        Serial.print("Could not connect to server: ");
        Serial.println(host);
        return;
      }
    }
  }

  // ###get the measurements###

  // get pm2.5 measurement
  std::pair<int, int> measurement_data = getAveragePm2dot5(10);
  // std::pair<int, int> measurement_data;
  // measurement_data.first = 12;
  // measurement_data.second = 12;

  // if has no measurement time, meausrement succeeded and we push gspread
  if (measurement_data.second)
  {
    // get humidity and temperature from DHT11 sensor
    int humidity = dht.getHumidity();
    float temperature = dht.getTemperature();

    char values_string[50];
    snprintf(
        values_string,
        sizeof(values_string),
        "%.1f,%d,%d,%d",
        temperature,
        humidity,
        measurement_data.first,
        measurement_data.second);
    doc["values"] = values_string;

    // Serialize the JSON document to a string
    String payload;
    serializeJson(doc, payload);

    // Create json object string to send to Google Sheets
    // payload = payload_basev1 + "\"" + measurement_data.first + "," + measurement_data.second + "\"}";

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

    //  we wait measurement_freq before next loop
    Serial.println((String) "end of loop, Next loop in " + measurement_freq + "s");
    delay(measurement_freq * 1000);
  }
  else
  {
    // measurement fails we wait a bit before trying again
    delay(5000);
  }
}
