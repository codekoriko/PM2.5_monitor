// Demonstrate the CertStore object with WiFiClientBearSSL
//
// Before running, you must download the set of certs using
// the script "certs-from-mozilla.py" (no parameters)
// and then uploading the generated .AR file to SPIFFS or SD.
//
// You do not need to generate the ".IDX" file listed below,
// it is generated automatically when the CertStore object
// is created and written to SD or SPIFFS by the ESP8266.
//
// Why would you need a CertStore?
//
// If you know the exact server being connected to, or you
// are generating your own self-signed certificates and aren't
// allowing connections to HTTPS/TLS servers out of your
// control, then you do NOT want a CertStore.  Hardcode the
// self-signing CA or the site's x.509 certificate directly.
//
// However, if you don't know what specific sites the system
// will be required to connect to and verify, a
// CertStore can allow you to select from among
// 10s or 100s of CAs against which you can check the
// target's X.509, without taking any more RAM than a single
// certificate.  This is the same way that standard browsers
// and operating systems verify SSL connections.
//
// About the chosen certs:
// The certificates are scraped from the Mozilla.org current
// list, but please don't take this as an endorsement or a
// requirement:  it is up to YOU, the USER, to specify the
// certificate authorities you will use as trust bases.
//
// Mar 2018 by Earle F. Philhower, III
// Released to the public domain

#include <ESP8266WiFi.h>
#include <CertStoreBearSSL.h>
#include <time.h>

#include <pms.h>
#include <DHTesp.h>

Pmsx003 pms(D6, D7); //rx, tx

DHTesp dht;

#ifndef STASSID
#define STASSID "Mike Homestay 4"
#define STAPSK  "vietnam1"
#endif

const char *ssid = STASSID;
const char *pass = STAPSK;

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
BearSSL::CertStore certStore;


// NOTE: The CertStoreFile virtual class may migrate to a templated
// model in a future release. Expect some changes to the interface,
// no matter what, as the SD and SPIFFS filesystem get unified.

#include <FS.h>
class SPIFFSCertStoreFile : public BearSSL::CertStoreFile {
  public:
    SPIFFSCertStoreFile(const char *name) {
      _name = name;
    };
    virtual ~SPIFFSCertStoreFile() override {};

    // The main API
    virtual bool open(bool write = false) override {
      _file = SPIFFS.open(_name, write ? "w" : "r");
      return _file;
    }
    virtual bool seek(size_t absolute_pos) override {
      return _file.seek(absolute_pos, SeekSet);
    }
    virtual ssize_t read(void *dest, size_t bytes) override {
      return _file.readBytes((char*)dest, bytes);
    }
    virtual ssize_t write(void *dest, size_t bytes) override {
      return _file.write((uint8_t*)dest, bytes);
    }
    virtual void close() override {
      _file.close();
    }

  private:
    File _file;
    const char *_name;
};

SPIFFSCertStoreFile certs_idx("/certs.idx"); // Generated by the ESP8266
SPIFFSCertStoreFile certs_ar("/certs.ar"); // Uploaded by the user



// ###########################################################
// #########################FONCTIONS#########################
// ###########################################################

// retrieve the temperature and the humidity from the DHT11
std::pair<int, int> getAveragePm2dot5(int nb_measurement) {
  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];
  float measurement_data[10] = {};
  float current_measurement = 0;
  auto prior_measurement_timestamp = 0;
  int measurement_duration = 0;

  Serial.println("Waking-up the sensor...");
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
  delay(5000);
	
  Serial.println("starting measurement...");
  int j = 0;
  
  for (int i = 0; i < 10; ++i) {
   prior_measurement_timestamp = millis();
    for (int k = 0; k < 200 ; k++){
      Pmsx003::PmsStatus status = pms.read(data, n);
      delay(200);
      switch (status) {
        case Pmsx003::OK:
        {
          measurement_duration = millis() - prior_measurement_timestamp;
          Serial.print((String)"measurement "+i+"succeed in "+measurement_duration+" ms : ");
          k = 200;
          // For loop starts from 3
          // Skip the first three data (PM1dot0CF1, PM2dot5CF1, PM10CF1)
          for (size_t i = Pmsx003::PM1dot0; i < n-6; ++i)
            Serial.print((String)data[i]+" "+Pmsx003::dataNames[i]+" | ");
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
    
    // Check if current measurement is not too far off
    if (current_measurement != 0){
      if ( j == 0 ){
        measurement_data[j] = current_measurement;
        j++;
      } else if ( fabs(measurement_data[j-1]-current_measurement) < 20 ) { 
        Serial.println((String)"Epsilon measurement in the acceptable range: "+(measurement_data[j-1]-current_measurement));
        measurement_data[j] = current_measurement;
        j++;
      } else
          Serial.println((String)"Epsilon measurement out of the acceptable range: "+(measurement_data[j-1]-current_measurement));
    } else
      Serial.println("Couldn't get any data from the sensor");
    delay(3000);
  }

  int nb_element = sizeof(measurement_data) / sizeof(measurement_data[0]); 
  if (nb_element > 2){
    float sum = 0;
    double pm2dot5_avg = 0;
    for (int i = 0; i < nb_element; i++){
      Serial.println((String)"measurement "+i+": "+measurement_data[i]);
      sum += measurement_data[i];
    }
    pm2dot5_avg = (int)round(sum / nb_element);

    return std::make_pair(pm2dot5_avg, measurement_duration);
  }
}

// Set time via NTP, as required for x.509 validation
void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

// Try and connect using a WiFiClientBearSSL to specified host:port and dump URL
void POST_data(BearSSL::WiFiClientSecure *client, String host, const uint16_t port, String path, String payload) {
  if (!path) {
    path = "/";
  }

  Serial.println("\nTrying: " + host + "");
  client->connect(host, port);
  if (!client->connected()) {
    Serial.printf("*** Can't connect. ***\n-------\n");
    return;
  }
  client->println("POST " + path + " HTTP/1.0");
  client->println("Host: " + host);
  client->println("User-Agent: ESP8266");
  client->print("Content-Length: ");
  client->println(payload.length());
  client->println("Content-type: application/x-www-form-urlencoded");
  client->println();
  client->println(payload);

  uint32_t to = millis() + 5000;
  if (client->connected()) {
    do {
      char tmp[32];
      memset(tmp, 0, 32);
      int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
      yield();
      if (rlen < 0) {
        break;
      }
      // Only print out first line up to \r, then abort connection
      char *nl = strchr(tmp, '\r');
      if (nl) {
        *nl = 0;
        Serial.print(tmp);
        break;
      }
      Serial.print(tmp);
    } while (millis() < to);
  }
  client->stop();
  Serial.printf("\n-------\n");
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
	
	Serial.println("Initializing DHT11 on PIN D5...");
	dht.setup(D5, DHTesp::DHT11);

  SPIFFS.begin();


  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Initializing the certification store
  setClock(); // Required for X.509 validation
  int numCerts = certStore.initCertStore(&certs_idx, &certs_ar);
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozill.py and upload the SPIFFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }

}


// ###########################################################
// #########################LOOP##############################
// ###########################################################
bool measurement_sucessful = 0;
void loop(void) {
  int sec_wait_main_loop = 50;
	
  std::pair<int, int> measurement_data = getAveragePm2dot5(10); 

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  String buf;
  buf += F("Temperature=");
  buf += String(temperature, 3);
  buf += F("&Humidity=");
  buf += String(humidity, 3);
  buf += F("&PM2.5=");
  buf += String(measurement_data.first);
  buf += F("&Meas.%20duration=");
  buf += String(measurement_data.second);

  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);
  POST_data(bear, "script.google.com", 443, "/macros/s/AKfycbxUjBnKnfZZgr2jZw2yLCmDdXdZl6BAxd3pF-jQfVrHiWbYD-M/exec", buf);
  delete bear;


  Serial.println((String)"end of loop, putting the sensor to sleep for: "+sec_wait_main_loop);
  pms.write(Pmsx003::cmdSleep);
  delay(sec_wait_main_loop*1000);
}