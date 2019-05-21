// #include <ESP8266WiFi.h>
// #include <ESP8266HTTPClient.h>
// #include <../lib/ESP8266Ping.impl.h>
// // Network SSID
// const char* ssid = "Mike Homestay 4";
// const char* password = "vietnam1";
 
// void setup() {
  
//   Serial.begin(9600);
//   delay(10);
 
//   // Connect WiFi
//   Serial.println();
//   Serial.println();
//   Serial.print("Connecting to ");
//   Serial.println(ssid);
//   WiFi.hostname("Name");
//   WiFi.begin(ssid, password);
 
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("");
//   Serial.println("WiFi connected");
 
//   // Print the IP address
//   Serial.print("IP address: ");
//   Serial.print(WiFi.localIP());

//   const IPAddress remote_ip(8, 8, 8, 8); // Remote host

// }
 
// void PostString() {
//  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

//     HTTPClient http;    //Declare object of class HTTPClient

//     http.begin("https://ptsv2.com/t/345vr-1558372486/post");      //Specify request destination
//     http.addHeader("Content-Type", "text/plain");  //Specify content-type header

//     int httpCode = http.POST("Message from ESP8266");   //Send the request
//     String payload = http.getString();                  //Get the response payload

//     Serial.println("http code:");
//     Serial.println(httpCode);   //Print HTTP return code
//     Serial.println("payload:");
//     Serial.println(payload);    //Print request response payload

//     http.end();  //Close connection

//   }else{

//     Serial.println("Error in WiFi connection");   

//   }

//   delay(30000);  //Send a request every 30 seconds
// }
 
// void ping () {
//   Serial.print("Pinging ip ");
//   Serial.println(remote_ip);
  
//   // Ping
//   if (Ping.ping(remote_ip)) {
//   Serial.println("Success!!");
//   } else {
//   Serial.println("Error :(");
//   }
// }

// void loop() {
//   ping();
//   delay (10000);
// }