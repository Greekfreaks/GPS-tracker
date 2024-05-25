#include <TinyGPS++.h>
#include <WiFiNINA.h>

#include "Secret.h"

char ssid[] = SECRET_SSID;        // My SSID stored in the secret.h file
char pass[] = SECRET_PASS;        // network password also stored in the secret.h file
int status = WL_IDLE_STATUS;      // The WiFi radio's status
WiFiClient client;

// IFTTT webhooks
char HOST_NAME[] = "maker.ifttt.com";
String DeliveryOutUrl = "/trigger/Delivery_out/with/key/0RtZT3s81ZWSpbs__GHYa?value1={lat}&value2={long}&value3={map}";
String CloseUrl = "/trigger/Close_by/with/key/0RtZT3s81ZWSpbs__GHYa?value1={lat}&value2={long}&value3={map}";
String DeliveredUrl = "/trigger/Delivered/with/key/0RtZT3s81ZWSpbs__GHYa?value1={lat}&value2={long}&value3={map}";


#define SerialGPS Serial1
int GPSBaud = 9600;
TinyGPSPlus gps;

// Destination coordinates
// home
float destLat = -37.874300;
float destLong = 145.224288;

// monash caulfield
// float destLat = -37.877392;
// float destLong = 145.044700;

// flags to ensure emails are only sent once and not every time the gps receives location
bool deliveryOutSent = false;
bool closeSent = false;
bool deliveredSent = false;
bool programStarted = false;

void setup()
{
  Serial.begin(9600);
  SerialGPS.begin(GPSBaud);

  // Prompt the user to type "Send" to start the program
  Serial.println("Type 'Send' to start the program.");

  // Connect to WiFi
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  Serial.println("Connected to WiFi");
}

void loop()
{
  // Check for user command via Serial Monitor
  if (!programStarted && Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove leading/trailing whitespaces
    if (command.equals("Send")) {
      programStarted = true;
      Serial.println("Program started.");
    }
  }

  // If the program has started
  if (programStarted) {
    while (SerialGPS.available() > 0) {
      if (gps.encode(SerialGPS.read())) {
        displayInfo();
      }
    }

    if (millis() > 5000 && gps.charsProcessed() < 10) {
      Serial.println("No GPS detected");
      while (true);
    }

    // Send the DeliveryOut webhook when valid GPS data is available and not sent yet
    if (!deliveryOutSent && gps.location.isValid()) {
      Serial.println("Sending DeliveryOut email");
      sendWebhook(DeliveryOutUrl, gps.location.lat(), gps.location.lng());
      deliveryOutSent = true;
    }

    // Check if the package is close to the destination
    if (!closeSent && isCloseToDestination(gps.location.lat(), gps.location.lng())) {
      Serial.println("Sending CloseBy email");
      sendWebhook(CloseUrl, gps.location.lat(), gps.location.lng());
      closeSent = true;
    }

    // Check if the package has reached the destination
    if (!deliveredSent && isDelivered(gps.location.lat(), gps.location.lng())) {
      Serial.println("Sending Delivered email");
      sendWebhook(DeliveredUrl, gps.location.lat(), gps.location.lng());
      deliveredSent = true;
    }
  }
}

void displayInfo()
{
  // Check if the program has started
  if (programStarted) {
    if (gps.location.isValid()) {
      Serial.print("Latitude: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: ");
      Serial.println(gps.location.lng(), 6);
      Serial.print("Altitude: ");
      Serial.println(gps.altitude.meters());
    } else {
      Serial.println("Location: Not Available");
    }

    // Display date and time
    Serial.print("Date: ");
    if (gps.date.isValid()) {
      Serial.print(gps.date.month());
      Serial.print("/");
      Serial.print(gps.date.day());
      Serial.print("/");
      Serial.println(gps.date.year());
    } else {
      Serial.println("Not Available");
    }

    Serial.print("Time: ");
    if (gps.time.isValid()) {
      if (gps.time.hour() < 10) Serial.print(F("0"));
      Serial.print(gps.time.hour());
      Serial.print(":");
      if (gps.time.minute() < 10) Serial.print(F("0"));
      Serial.print(gps.time.minute());
      Serial.print(":");
      if (gps.time.second() < 10) Serial.print(F("0"));
      Serial.print(gps.time.second());
      Serial.print(".");
      if (gps.time.centisecond() < 10) Serial.print(F("0"));
      Serial.println(gps.time.centisecond());
    } else {
      Serial.println("Not Available");
    }

    Serial.println();
    Serial.println();
    delay(1000);
  }
}

// Haversine formula to calculate distance between two points on the Earth's surface
float calculateDistance(float lat1, float long1, float lat2, float long2) {
  const float R = 6371.0; // Radius of the Earth in kilometers
  float latRad1 = radians(lat1);
  float latRad2 = radians(lat2);
  float deltaLat = radians(lat2 - lat1);
  float deltaLong = radians(long2 - long1);

  float a = sin(deltaLat / 2) * sin(deltaLat / 2) +
            cos(latRad1) * cos(latRad2) *
            sin(deltaLong / 2) * sin(deltaLong / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  float distance = R * c;
  return distance;
}

bool isCloseToDestination(float currentLat, float currentLong) {
  // Calculate distance to destination
  float distance = calculateDistance(currentLat, currentLong, destLat, destLong);
  return distance < 1.0; // 1 km threshold
}

bool isDelivered(float currentLat, float currentLong) {
  // Calculate distance to destination
  float distance = calculateDistance(currentLat, currentLong, destLat, destLong);
  return distance < 0.05; // 50 meters threshold
}

void sendWebhook(String url, float latitude, float longitude) {
  String formattedUrl = url;
  String latStr = String(latitude, 6);
  String longStr = String(longitude, 6);

  formattedUrl.replace("{lat}", latStr);
  formattedUrl.replace("{long}", longStr);

  String mapLink = "https://maps.google.com/?q=" + latStr + "," + longStr;
  formattedUrl.replace("{map}", mapLink);

  if (client.connect(HOST_NAME, 80)) {
    Serial.println("Connected to server, sending HTTP request...");

    // Make a HTTP request
    client.print(String("GET ") + formattedUrl + " HTTP/1.1\r\n" +
                 "Host: " + HOST_NAME + "\r\n" +
                 "Connection: close\r\n\r\n");
    delay(1000); // Wait for server to respond

    // Read and print server response
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();

    // Disconnect from server
    client.stop();
    Serial.println("Disconnected from server");
  } else {
    Serial.println("Connection failed");
  }
}