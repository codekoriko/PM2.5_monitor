// Example Arduino/ESP8266 code to upload data to Google Sheets
// Follow setup instructions found here:
// https://github.com/StorageB/Google-Sheets-Logging
// reddit: u/StorageB107
// email: StorageUnitB@gmail.com


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "HTTPSRedirect.h"

#include <pms.h>
// #include <DHTesp.h>

// ###########################################################
// #####################SETUP Gapp Script#####################
// ###########################################################

// Enter network credentials:
const char* ssid     = "TANG 8";
const char* password = "88888888";

// Enter Google Script Deployment ID:
const char *GScriptId = "AKfycbzxYrJd6lzyuP90IjHdNftWQAiq4AXKB7g540niBL2tg8aWfLybOJZ9mnejdLcBvGxw";

// Enter command (insert_row or append_row) and your Google Sheets sheet name (default is Sheet1):
String payload_base =  "{\"command\": \"append_row\", \"sheet_name\": \"Sheet1\", \"values\": ";
String payload = "";

// Google Sheets setup (do not edit)
const char* host = "script.google.com";
const int httpsPort = 443;
const char* fingerprint = "";
String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;

// ###########################################################
// #########################SETUP PMS#########################
// ###########################################################

#define PMS7001_TX D6 // blue
#define PMS7001_RX D7 // black
// #define DHT_OUT D5
Pmsx003 pms(PMS7001_TX, PMS7001_RX); //PMS7001-tx_pin, PMS7001-rx_pin
int sec_wait_main_loop = 600;

// DHTesp dht;

// Example Arduino/ESP8266 code to upload data to Google Sheets
// Follow setup instructions found here:
// https://github.com/StorageB/Google-Sheets-Logging
// reddit: u/StorageB107
// email: StorageUnitB@gmail.com


// ###########################################################
// ######################MEASURE FUNCT########################
// ###########################################################


std::pair<uint16_t, uint16_t> getAveragePm2dot5(int nb_measurement) {
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
  for (int i = 0; i < 10; ++i) {
    prior_measurement_timestamp = millis();
    for (int j = 0; j < 200 ; j++){
      Pmsx003::PmsStatus status = pms.read(data, n);
      delay(200);
      switch (status) {
        case Pmsx003::OK:
        {
          current_measurement_duration = millis() - prior_measurement_timestamp;
          measurement_duration += current_measurement_duration;
          Serial.print((String)"measurement "+i+" succeeded in "+measurement_duration+" ms : ");
          j = 200;
          // For loop starts from 3
          // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
          for (size_t i = Pmsx003::PM1dot0; i < n-6; ++i)
            Serial.print((String)"\t"+data[i]+" "+Pmsx003::dataNames[i]);
          Serial.println();
          current_measurement = data[4];
        }
        case Pmsx003::noData:
          break;
        default:
          Serial.println((String)"measurement "+i+" failed ("+Pmsx003::errorMsg[status]+"), retrying...");
          //Serial.println(Pmsx003::errorMsg[status]);
      };
    }

    if (current_measurement != 0){
      measurement_data[i] = current_measurement;
    }else{
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
  for (int i = 0; i < nb_element; i++){
    int epsilon_sum = 0;
    for (int j = 0; j < nb_element; j++)
      epsilon_sum += abs(measurement_data[i] - measurement_data[j]);
    epsilon_avg[i] = (int)round(epsilon_sum / nb_element);
  }

  uint16_t reference = measurement_data[0];
  int smallest_epsilon = epsilon_avg[0];
  for (int i = 0; i < nb_element; i++){
    if (epsilon_avg[i] < smallest_epsilon){
      smallest_epsilon = epsilon_avg[i];
      reference = measurement_data[i];
    }
  }
  Serial.println((String)"Measurement value "+reference+" has the smallest averaged Epsilon: "+smallest_epsilon);

  // if the measurement epsilon is less than 20 off from the reference, then,
  // take into the avg calculation
  uint16_t sum = 0;
  uint16_t pm2dot5_avg = 0;
  int nb_valid_measurement = 0;
  for (int i = 0; i < nb_element; i++){
    if (epsilon_avg[i] < 15){
      sum += measurement_data[i];
      nb_valid_measurement++;
    }
    else
      Serial.println((String)"measurement "+i+" was rejected! the value "+measurement_data[i]+" has an averaged Epsilon of "+epsilon_avg[i]);
  }
  pm2dot5_avg = (uint16_t)round(sum / nb_valid_measurement);

  return std::make_pair(pm2dot5_avg, measurement_duration);

}


// ###########################################################
// #########################SETUP#############################
// ###########################################################


void setup() {
  // Starting Software Serial and waiting its "ready state"
  Serial.begin(115200);
  while (!Serial) {};

	Serial.println(" Initializing Pms7003...");
	pms.begin();
	pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
	pms.write(Pmsx003::cmdModeActive);

	// Serial.println("Initializing DHT11 on PIN D5...");
	// dht.setup(DHT_OUT, DHTesp::DHT11);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1){
       flag = true;
       Serial.println("Connected");
       break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag){
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    return;
  }
  delete client;    // delete HTTPSRedirect object
  client = nullptr; // delete HTTPSRedirect object

}


// ###########################################################
// #########################LOOP##############################
// ###########################################################
bool measurement_sucessful = 0;
void loop(void) {

  std::pair<int, int> measurement_data = getAveragePm2dot5(10);

  // float humidity = dht.getHumidity();
  // float temperature = dht.getTemperature();

  static bool flag = false;
  if (!flag){
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr){
    if (!client->connected()){
      client->connect(host, httpsPort);
    }
  }
  else{
    Serial.println("Error creating client object!");
  }

  // Create json object string to send to Google Sheets
  payload = payload_base + "\"" + measurement_data.first + "," + measurement_data.second + "\"}";

  // Publish data to Google Sheets
  Serial.println("Publishing data...");
  Serial.println(payload);
  if(client->POST(url, host, payload)){
    // do stuff here if publish was successful
  }
  else{
    // do stuff here if publish was not successful
    Serial.println("Error while connecting");
  }


  Serial.println((String)"end of loop, putting the sensor to sleep for: "+sec_wait_main_loop);
  pms.write(Pmsx003::cmdSleep);
  delay(sec_wait_main_loop*1000);
}