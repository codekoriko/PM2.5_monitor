#include <Arduino.h>
#include <unity.h>

using namespace fakeit;
#include "gs_publisher.h"

// Mocks and other setup might be needed here

void test_publish_method(void)
{
    // Create a mock of HTTPSRedirect and Stream
    // MockHTTPSRedirect client;
    // MockStream serial;

    // Instantiate GsPublisher with mock objects
    GsPublisher publisher("DEPLOYMENT_ID", serial);

    // Define your test paragmeters
    float temperature = 25.0;
    int humidity = 50;
    int firstData = 10;
    int secondData = 20;

    // Call the publish method
    bool result = publisher.publish(temperature, humidity, firstData, secondData);

    // Check the expected result
    TEST_ASSERT_TRUE(result);
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_publish_method);
    UNITY_END();
}

void loop()
{
    // Not used in this test
}
