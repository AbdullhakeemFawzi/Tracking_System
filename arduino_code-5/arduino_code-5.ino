/*
   ESP32 GPS Tracker with Offline Data Storage and Wi-Fi Upload
   Features:
   - Connects to a GPS module via UART1 (RX:16, TX:17)
   - Reads GPS data (latitude, longitude, altitude, timestamp, satellites)
   - Connects to Wi-Fi and uploads GPS data to a cloud server
   - Stores GPS data locally when Wi-Fi is unavailable and uploads later
   - Uses LEDs to indicate GPS signal and Wi-Fi connection status
   - Automatically reconnects to Wi-Fi if disconnected
   Functions:
   - setup(): Initializes Wi-Fi, GPS module, and status LEDs
   - loop(): Continuously reads GPS data, processes it, and uploads at intervals
   - processGPSData(): Extracts GPS information from raw NMEA sentences
   - sendGPSData(): Sends formatted GPS data to the cloud server via HTTP POST
   - parseGPGGA(), parseGPRMC(): Extracts relevant GPS details from NMEA sentences
   - convertAndPrintLocalDateTime(): Converts UTC time to local format
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>  // Include vector library for storing offline GPS data
#include <WiFiClientSecure.h>
#include <math.h>
// WiFi credentials
const char* ssid = "Fawzi";      // WiFi SSID - Replace your own
const char* password = "715502962";  // WiFi Password - Replace your own
// Server details for sending GPS data
const char* serverUrl =  "https://www.circuitdigest.cloud/geolinker";   // Server URL
const char* apiKey = "kGegKPqW4Kof";                                     //12 character API key for authentication
//GPS module connection using UART1
const int RXPin = 16;  // GPS TX connected to ESP32 RX
const int TXPin = 17;  // GPS RX connected to ESP32 TX
#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial    //for  serial montionr
#define gpsSerial Serial1     //for commincate with gps modual
#define SerialAT Serial2       // for  commincate with sim800l modual
String number ="+967778086270";
String textMessage;
// ESP32 and SIM800l pins
#define gsm_TX 26
#define gsm_RX 27
// Structure to hold detailed GPS data
struct GPSRawData {
  double latitude;
  char latitudeDir;
  double longitude;
  char longitudeDir;
  int satellites;
  double altitude;
  int hours, minutes, seconds;
  int day, month, year;
  String timestamp;  // Combined timestamp with date and time
};
// Structure to store simplified GPS data for transmission
struct GPSData {
  double latitude;
  double longitude;
  String timestamp;
};
String phoneNumber = "";
String message = "";
bool isReceivingMessage = false;
// Upload interval and last upload timestamp
const unsigned long uploadInterval = 30000;  // Data sent every 30 seconds
unsigned long lastUploadTime = 0;
// LED indicators for network and GPS status
bool gpsDataValid = false;    // Flag to check if GPS data is valid
GPSData latestGPSData;        // Stores the latest valid GPS data
GPSRawData latestGPSRawData;  // Stores raw GPS data
// Buffer to store GPS data when offline
std::vector<GPSData> offlineData;
// Add these defines/variables near your WiFi credentials
const char* apiEndpoint = "https://ajyalyemen.site/car_system/api/track.php"; // Change to your server URL
const char* carIdentifier = "1"; // Unique identifier for this car
const char* apiKeyWebsite = "YemenCars123"; // Must match the PHP API
// Add these variables to track the last sent coordinates
double lastSentLatitude = 0.0;
double lastSentLongitude = 0.0;
bool hasSentBefore = false;
// Returns distance in meters between two lat/lng points
double haversine(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000; // Earth radius in meters
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}
void setup() {
  SerialMon.begin(115200);
  delay(1000);
  SerialMon.println("Wait ...");
  gpsSerial.begin(9600, SERIAL_8N1, RXPin, TXPin);  // Initialize GPS module
  SerialAT.begin(9600, SERIAL_8N1, gsm_TX,gsm_RX);  //Initialize GSM module
  delay(7000);
  SerialMon.println(" success");
  SerialAT.println("AT+CMGF=1\r\n"); //Sets the GSM Module in Text Mode
  delay(1000);
  SerialAT.println("AT+CNMI=2,2,0,0,0\r\n");
  delay(1000);
  //SerialAT.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
   //delay(1000);
   //String smsMessage = "GPS TRACKING RADY";SSSSSSS
   //SerialAT.println(smsMessage);
    //delay(100);
    //SerialAT.println((char)26); // ASCII code of CTRL+Z
     //SerialAT.write(0x1a);
    //delay(1000);
  // Connect to WiFi
  WiFi.begin(ssid, password);
  SerialMon.print("Connecting to WiFi");
 //rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
 while (WiFi.status() != WL_CONNECTED) {
    SerialMon.print(" not connect");
  }
  SerialMon.println("\nWiFi connected!");
}
void loop() {
 while (SerialAT.available() > 0) {
  textMessage = SerialAT.readString();
  Serial.println(textMessage);
     // delay(15);
  }
if (textMessage.indexOf("LOC") >= 0) { 
  // delay(100); 
    textMessage = "";
    String text;
   String latitudee = String(latestGPSData.latitude, 6);
   String longitudee = String(latestGPSData.longitude, 6);
    text += "http://maps.google.com/maps?q=loc:" + latitudee + "," + longitudee;

   SerialAT.println("AT+CMGS=\"" + number + "\"\r");
    delay(1000);
    SerialAT.println(text);
    delay(100);
    //SerialAT.println((char)26); // ASCII code of CTRL+Z
     SerialAT.write(0x1a);
    delay(1000);
    SerialAT.println("AT+CMGD=1,4");
  SerialMon.println("sent");
  }
 static String gpsData = "";
  // Read and process GPS data from the module
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gpsData += c;
    if (c == '\n') {  // When a full GPS sentence is received
      processGPSData(gpsData);
      gpsData = "";
    }
  }

  // Periodically send GPS data to the server
  if (millis() - lastUploadTime >= uploadInterval) {
    lastUploadTime = millis();
    if (gpsDataValid) {  // Only send if GPS data is valid
      // Skip sending if distance is less than threshold
      const double MIN_DISTANCE_METERS = 30.0; // Change as needed
      if (hasSentBefore) {
        double dist = haversine(latestGPSData.latitude, latestGPSData.longitude, lastSentLatitude, lastSentLongitude);
        if (dist < MIN_DISTANCE_METERS) {
          SerialMon.print("Skipping send: distance (meters) = ");
          SerialMon.println(dist);
          return;
        }
      }
      if (WiFi.status() == WL_CONNECTED) {
        // Send any stored offline data first
        bool offlineUploadSuccess = true;
        for (auto& data : offlineData) {
          if (!sendGPSData(data)) {  // Stop if sending fails
            offlineUploadSuccess = false;
            break;
          }
        }

        if (offlineUploadSuccess) {
          offlineData.clear();  // Clear offline buffer if all data is uploaded
        }

        // Send the latest GPS data
        if (!sendGPSData(latestGPSData)) {
          SerialMon.println("Off line mode . Storing  data locally.");
          offlineData.push_back(latestGPSData);  // Store data for later upload
        } else {
          SerialMon.println("Live MODE ON,SHOW ON MAP.");
          // Update last sent coordinates
          lastSentLatitude = latestGPSData.latitude;
          lastSentLongitude = latestGPSData.longitude;
          hasSentBefore = true;
        }
      } else {
        // If WiFi is offline, store GPS data for later upload
        SerialMon.println("WiFi not connected. Storing data locally.");
        offlineData.push_back(latestGPSData);
        WiFi.disconnect();
        WiFi.reconnect();
      }
    } else {
      SerialMon.println("Invalid GPS data. Skipping upload.");
    }
  }
  
}

// Function to send GPS data to the server
bool sendGPSData(GPSData data) {
  HTTPClient http;

  // Build the full URL with parameters for GET
  String fullUrl = String(apiEndpoint) + "?car_identifier=" + String(carIdentifier) +
                   "&latitude=" + String(data.latitude, 6) +
                   "&longitude=" + String(data.longitude, 6) +
                   "&api_key=" + String(apiKeyWebsite);

  SerialMon.println("Full GET URL: " + fullUrl);

  http.begin(fullUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
  
  // Set timeout to prevent hanging
  http.setTimeout(10000); // 10 seconds timeout

  int httpResponseCode = http.GET();
  SerialMon.println("Final HTTP Response code: " + String(httpResponseCode));

  if (httpResponseCode > 0) {
    if(httpResponseCode == 301 || httpResponseCode == 302 || httpResponseCode == 307 || httpResponseCode == 308){
        String newUrl = http.getLocation();
        SerialMon.println("Redirected to: " + newUrl);
    }
    String response = http.getString();
    SerialMon.println("Response: " + response);
  } else {
    SerialMon.println("Error on HTTP request: " + http.errorToString(httpResponseCode));
    SerialMon.println("Error code: " + String(httpResponseCode));
  }

  http.end();
  return (httpResponseCode == 200 || httpResponseCode == 201);
}
// Function to process received GPS data
void processGPSData(String raw) {
  if (raw.startsWith("$GNGGA")) {  // GPGGA contains location and satellite data
    parseGPGGA(raw);
    convertAndPrintLocalDateTime();       // Convert UTC to local time
  } else if (raw.startsWith("$GNRMC")) {  // GPRMC contains date and speed info
    parseGPRMC(raw);
  }
}

// Function to parse GPGGA data and extract latitude, longitude, and altitude
void parseGPGGA(String gpgga) {
  String tokens[15];
  int tokenIndex = 0;

  // Split GPGGA string into individual tokens
  int startIndex = 0;
  for (int i = 0; i < gpgga.length(); i++) {
    if (gpgga[i] == ',' || gpgga[i] == '*') {
      tokens[tokenIndex++] = gpgga.substring(startIndex, i);
      startIndex = i + 1;
    }
  }

  // Extract meaningful GPS data
  if (tokenIndex > 1) {
    String utcTime = tokens[1];
    latestGPSRawData.hours = utcTime.substring(0, 2).toInt();
    latestGPSRawData.minutes = utcTime.substring(2, 4).toInt();
    latestGPSRawData.seconds = utcTime.substring(4, 6).toInt();

    latestGPSRawData.latitude = nmeaToDecimal(tokens[2]);
    latestGPSData.latitude = nmeaToDecimal(tokens[2]);
    latestGPSRawData.latitudeDir = tokens[3].charAt(0);
    latestGPSRawData.longitude = nmeaToDecimal(tokens[4]);
    latestGPSData.longitude = nmeaToDecimal(tokens[4]);
    latestGPSRawData.longitudeDir = tokens[5].charAt(0);

    latestGPSRawData.satellites = tokens[7].toInt();
    latestGPSRawData.altitude = tokens[9].toDouble();

    if (latestGPSRawData.satellites >= 4) {  // Ensure enough satellites are available
      if (latestGPSData.latitude != 0 || latestGPSData.longitude != 0) {
        gpsDataValid = true;
      } else {
        gpsDataValid = false;
      }
    } else {
      gpsDataValid = false;
    }
  }
}

// Function to parse GPRMC data and extract the date
void parseGPRMC(String gprmc) {
  String tokens[15];
  int tokenIndex = 0;

  // Split GPRMC string into tokens
  int startIndex = 0;
  for (int i = 0; i < gprmc.length(); i++) {
    if (gprmc[i] == ',' || gprmc[i] == '*') {
      tokens[tokenIndex++] = gprmc.substring(startIndex, i);
      startIndex = i + 1;
    }
  }
  
  // Extract date information
  if (tokenIndex > 9) {
    String utcDate = tokens[9];
    latestGPSRawData.day = utcDate.substring(0, 2).toInt();
    latestGPSRawData.month = utcDate.substring(2, 4).toInt();
    latestGPSRawData.year = 2000 + utcDate.substring(4, 6).toInt();
  }
}

// Function to convert UTC time and date to local time and update the timestamp
void convertAndPrintLocalDateTime() {
  int offsetHours = 3;
  int offsetMinutes = 1;
  latestGPSRawData.minutes += offsetMinutes;
  latestGPSRawData.hours += offsetHours;
  if (latestGPSRawData.minutes >= 60) {
    latestGPSRawData.minutes -= 60;
    latestGPSRawData.hours++;
  }
  if (latestGPSRawData.hours >= 24) {
    latestGPSRawData.hours -= 24;
    latestGPSRawData.day++;
  }
  char timeBuffer[20];
  snprintf(timeBuffer, sizeof(timeBuffer), "%04d-%02d-%02d %02d:%02d:%02d",
           latestGPSRawData.year, latestGPSRawData.month, latestGPSRawData.day,
           latestGPSRawData.hours, latestGPSRawData.minutes, latestGPSRawData.seconds);
  latestGPSRawData.timestamp = String(timeBuffer);
  latestGPSData.timestamp = String(timeBuffer);

  //SerialMon.print("TimeStamp: ");
  //SerialMon.println(latestGPSRawData.timestamp);
}
// Function to determine the number of days in a given month
int daysInMonth(int month, int year) {
  if (month == 2) {
    // Check for leap year
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  }
  return (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
}
// Function to convert NMEA coordinates to decimal degrees
double nmeaToDecimal(String nmeaCoord) {
  if (nmeaCoord == "") return 0.0;
  double raw = nmeaCoord.toDouble();
  int degrees = int(raw / 100);
  double minutes = raw - (degrees * 100);
  return degrees + (minutes / 60.0);
}