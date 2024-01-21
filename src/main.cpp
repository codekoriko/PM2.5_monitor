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

#include <gs_publisher.h>

#include <pms.h>
#include <DHTesp.h>

#include <vector>
#include <cstdint> // For uint16_t

// ###########################################################
// #####################SETUP Gapp Script#####################
// ###########################################################

const char *DEPLOYMENT_ID = "AKfycbxA5gwluyL3Nw2FK_tnUW2ix276BEaCVnsFYEc3JJIDYNHr_sQJUNh3bzOdgZyVIiI";

// Create a JSON document
StaticJsonDocument<200> doc;

GsPublisher *publisher;

// columns name must match otherwise get (not helpful error) Error! Not connected to host.
// Timestamp col get filled by the script
// order do matter
const char *columns[] = {
    "Temperature",
    "Humidity",
    "PM2.5",
    "Meas. duration"};

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
// ##########################Helper###########################
// ###########################################################

bool resetPms()
{
    Serial.println("Resetting PMS7003...");
    pms.flushInput();
    pms.end();
    delay(2000);
    pms.begin();
    delay(2000);
    pms.write(Pmsx003::cmdModeActive);
    delay(2000);
    pms.flushInput();
    return true;
}

int averageWithoutOutliers(const uint16_t measurement_data[10])
{
    // Convert array to vector
    std::vector<int> data(measurement_data, measurement_data + 10);

    if (data.empty())
    {
        return 0; // Return 0 if the array is empty
    }

    // Calculate the initial average
    float sum = 0.0;
    for (int value : data)
    {
        sum += value;
    }
    float average = sum / data.size();

    // Filter out outliers and recalculate sum and count
    sum = 0.0;
    int count = 0;
    for (int value : data)
    {
        if (abs(value - average) <= 20)
        {
            sum += value;
            ++count;
        }
    }

    // Calculate the new average with outliers removed
    // Cast to int to return an integer average
    return (count > 0) ? static_cast<int>(sum / count) : 0;
}

// ###########################################################
// ######################MEASURE FUNCT########################
// ###########################################################

std::pair<uint16_t, uint16_t> getAveragePm2dot5(int nb_measurement)
{
    const auto n = Pmsx003::Reserved;
    Pmsx003::pmsData data[n];

    uint16_t measurement_data[10] = {};
    uint16_t current_measurement = 0;
    auto sample_start_timestamp = 0;
    auto measurement_start_timestamp = 0;
    uint16_t current_measurement_duration = 0;
    uint16_t measurement_duration = 0;

    Serial.println("Waking-up the sensor...");
    pms.flushInput();
    pms.write(Pmsx003::cmdWakeup);
    // wait for it to wake-up. 5000 seems the be the minimum time for it wakeup
    // but waitForData is not working where delay with same amount of time does
    delay(5000);
    pms.flushInput();
    // pms.waitForData(5000);

    Serial.println("starting measurement...");
    delay(5000); // initial delay for sensor to stabilize
    measurement_start_timestamp = millis();
    for (int i = 0; i < 11; ++i)
    {
        sample_start_timestamp = millis();
        for (int j = 0; j < 200; j++)
        {
            pms.flushInput();
            Pmsx003::PmsStatus status = pms.read(data, n);
            pms.waitForData(2000, n);
            switch (status)
            {
            case Pmsx003::OK:
            {
                current_measurement_duration = millis() - sample_start_timestamp;
                Serial.print((String) "measurement " + i + " succeeded in " + current_measurement_duration + " ms : ");
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
            // we discard first measurement (always a bit off)
            if (i != 0){
                measurement_data[i] = current_measurement;
            }
        }
        else
        {
            Serial.println("Couldn't get any data from the sensor, resetting the sensor...");
            resetPms();
            return std::make_pair(0, 0);
        }
        // delay between each 10 measurements
        delay(2000);
    }
    Serial.println();

    Serial.println("measurement finish");
    measurement_duration = static_cast<int>((millis() - measurement_start_timestamp) / 1000);
    Serial.println((String) "measurement duration : " + measurement_duration + " s");
    uint16_t pm2dot5_avg = averageWithoutOutliers(measurement_data);

    Serial.println((String) "Putting the sensor to sleep");
    pms.write(Pmsx003::cmdSleep);
    delay(1000);
    pms.flushInput();
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

    publisher = new GsPublisher(DEPLOYMENT_ID, Serial); // Initialize the variable "publisher"

    Serial.println(" Initializing Pms7003...");
    pms.begin();
    pms.flushInput();
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

void loop(void)
{
    // ###get the measurements###

    // get pm2.5 measurement
    std::pair<int, int> measurement_data = getAveragePm2dot5(10);
    int humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    // std::pair<int, int> measurement_data;
    // measurement_data.first = 12;
    // measurement_data.second = 12;

    // if has measurement time, meausrement succeeded and we push gspread
    if (measurement_data.first != 0 && measurement_data.second != 0)
    {
        if (publisher->publish(temperature, humidity, measurement_data.first, measurement_data.second))
        {
            Serial.println("Measurements published successfully.");
        }
        else
        {
            Serial.println("Failed to publish measurements.");
        }
        //  we wait measurement_freq before next loop
        Serial.println((String) "end of loop, Next loop in " + measurement_freq + "s");
        delay(measurement_freq * 1000);
    }
    else
    {
        Serial.println("Measurement was unsuccessful, not publishing.");
        delay(5000);
    }
}
