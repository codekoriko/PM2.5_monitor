auto lastRead = millis(); 
int sampling_time = 0;
void loop() {
/*   Serial.printf("\npress anykey when ready to send temp and humidity to Gdoc");
  String site;
  do {
    site = Serial.readString();
  } while (site == ""); */
  static const char * pm2dot5;
  const auto n = Pmsx003::Reserved;
  pms.write(Pmsx003::cmdWakeup);
  pms.waitForData(Pmsx003::wakeupTime);
	Pmsx003::pmsData data[n];
	Pmsx003::PmsStatus status = pms.read(data, n);
	switch (status) {
		case Pmsx003::OK:
		{
			Serial.println("_________________");
			auto newRead = millis();
			sampling_time = newRead - lastRead;
			lastRead = newRead;
      pm2dot5 = Pmsx003::getMetrics(4);
		}
		case Pmsx003::noData:
      Serial.println("_________________");
      Serial.println("No Data received! Please Check the wiring!");
			break;
		default:
			Serial.println("_________________");
			Serial.println(Pmsx003::errorMsg[status]);
	};

  if (sampling_time != 0) {
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();

    String buf;
    buf += F("Temperature=");
    buf += String(temperature, 3);
    buf += F("&Humidity=");
    buf += String(humidity, 3);
    buf += F("&PM2.5=");
    buf += pm2dot5;
    buf += F("&Sampling%20time=");
    buf += String(sampling_time, 3);
    
    BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
    // Integrate the cert store with this connection
    bear->setCertStore(&certStore);
    POST_data(bear, "script.google.com", 443, "/macros/s/AKfycbxUjBnKnfZZgr2jZw2yLCmDdXdZl6BAxd3pF-jQfVrHiWbYD-M/exec", buf);
    delete bear;
  }
  Serial.println("end of loop, putting to sleep pms7003, waiting 30s");
  pms.write(Pmsx003::cmdSleep);
  delay(30000);
}