#include "HTTPSRedirect.h"
#include <ArduinoJson.h> // Include the ArduinoJson library
#include <string>

class GsPublisher
{
private:
    Stream &serial;
    const char* url;
    const int httpsPort = 443; // Standard HTTPS port
    const char *host = "script.google.com";          // Hostname for the HTTPS connection
    HTTPSRedirect *client;

public:
    // Constructor
    GsPublisher(const std::string &deploymentId, Stream &serialRef)
        : serial(serialRef)
    {
        // Initialize HTTPSRedirect client
        client = new HTTPSRedirect(httpsPort);
        client->setInsecure();
        client->setPrintResponseBody(true);
        client->setContentTypeHeader("application/json");

        // Log the connection attempt
        serial.println("Connecting to ");
        serial.println(host);
        std::string urlString = "/macros/s/" + deploymentId + "/exec";
        url = strdup(urlString.c_str());
    }

    // Destructor to clean up the HTTPSRedirect client
    ~GsPublisher()
    {
        if (client != nullptr)
        {
            delete client;
            client = nullptr;
        }
    }

    // Attempt to connect with a specified number of retries
    bool connect(int retries) {
        if (client->connected()) {
            serial.println("Client already connected.");
            return true;  // Add this return statement
        }

        while (retries-- > 0) {
            if (client->connect(host, httpsPort)) {
                serial.println("Connected successfully.");
                return true;
            }
            serial.println("Connection failed. Retrying...");
            delay(1000); // Wait before retrying
        }
        serial.println("Failed to connect after all retries.");
        return false;
    }

    // Publish method
    bool publish(float temperature, int humidity, int firstData, int secondData) {
        if (!client->connected()) {
            if (!connect(3)) { // Attempt to connect with 3 retries
                return false; // Connection failed
            }
        }

        // Create the values string using snprintf
        char valuesString[50]; // Adjust the size as needed
        snprintf(
            valuesString,
            sizeof(valuesString),
            "%.1f,%d,%d,%d",
            temperature,
            humidity,
            firstData,
            secondData
        );

        // Create a JSON object
        StaticJsonDocument<200> doc;
        doc["command"] = "append_row";
        doc["sheet_name"] = "Sheet1";
        doc["values"] = valuesString;

        // Serialize JSON object to a String
        String jsonString;
        serializeJson(doc, jsonString);
        serial.println("jsonString:");
        serial.println(jsonString);
        // Send the JSON string to the server
        // Assuming client->POST takes the JSON string and sends it to the server
        if (client->POST(url, host, jsonString)) {
            serial.println("Data published successfully.");
            return true;
        } else {
            serial.println("Failed to publish data.");
            return false;
        }
    }
};
