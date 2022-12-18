#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ThingSpeak.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#define MAIN_SSID "Galaxy M21848C"
#define MAIN_PASS "jutm5054"
#define CSE_IP "192.168.52.204"                        //
#define CSE_PORT 5089                                
#define OM2M_ORGIN "admin:admin"
#define OM2M_MN "/~/in-cse/in-name/"
#define OM2M_AE1 "DHT11"
#define OM2M_AE2 "MQ-2"
#define OM2M_DATA_CONT "Node-1/Data"
#define INTERVAL 2000L

#define DHTPIN 5 
DHT dht(DHTPIN, DHT11);

#define Threshold 80
#define MQ2pin 35
#define ledPin 25
float humidity;
float temp;
float gas_concentration;

const char * ntpServer = "pool.ntp.org";

long randNumber;
long int prev_millis = 0;
unsigned long epochTime;

long myChannelNumber = 1976303;
const char myWriteAPIKey[] = "FJEHLZ5JKI9P2NU8";

HTTPClient http;
WiFiClient client;

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime( & timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time( & now);
  return now;
}

AsyncWebServer server(80);
void setup() {
  Serial.begin(115200);
  //delay(1000);
 
  dht.begin();
  pinMode(ledPin,OUTPUT);
 
  
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  WiFi.begin(MAIN_SSID, MAIN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("#");
  }
  
  configTime(0, 0, ntpServer);
  ThingSpeak.begin(client);

   Serial.println(WiFi.localIP());
   
   Serial.println("MQ2 warming up!");
  pinMode(MQ2pin , INPUT);
  delay(20000);
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(temp).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(humidity).c_str());
  });
  server.on("/gas_concentration", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(gas_concentration).c_str());
  });

  // Start server
  server.begin();
  
}


void loop() {
  humidity = dht.readHumidity();
  temp = dht.readTemperature();
  gas_concentration = analogRead(MQ2pin); 

  if(gas_concentration > Threshold)
  {
    Serial.print("Gas detected");
    digitalWrite(ledPin,HIGH);
  }
  else
  {
    digitalWrite(ledPin,LOW);
  }
  if (isnan(humidity) || isnan(temp)) {
  Serial.println("Failed to read from DHT sensor!");
    return;
  }
 
  if (millis() - prev_millis >= INTERVAL) {
    epochTime = getTime();
    String data;

    Serial.print("Temperature: " + (String) temp);
    Serial.print(" ,Humidity: " + (String) humidity);
    Serial.println(" ,Gas Concentration: " + (String) gas_concentration);
    //Serial.println(gas_concentration);
    String server = "http://" + String() + CSE_IP + ":" + String() + CSE_PORT + String() + OM2M_MN;

    http.begin(server + String() + OM2M_AE1 + "/" + OM2M_DATA_CONT + "/");

    http.addHeader("X-M2M-Origin", OM2M_ORGIN);
    http.addHeader("Content-Type", "application/json;ty=4");
    http.addHeader("Content-Length", "100");

    data = "[" + String(epochTime) + ", " + String(temp) + ", " + String(humidity) +   + "]"; 
    String req_data = String() + "{\"m2m:cin\": {"

      +
      "\"con\": \"" + data + "\","

      +
      "\"lbl\": \"" + "V1.0.0" + "\","

      //+ "\"rn\": \"" + "cin_"+String(i++) + "\","

      +
      "\"cnf\": \"text\""

      +
      "}}";
    int code = http.POST(req_data);
    http.end();
    Serial.println(code);

    http.begin(server + String() + OM2M_AE2 + "/" + OM2M_DATA_CONT + "/");

    http.addHeader("X-M2M-Origin", OM2M_ORGIN);
    http.addHeader("Content-Type", "application/json;ty=4");
    http.addHeader("Content-Length", "100");

    data = "[" + String(epochTime) + ", " + String(gas_concentration) +   + "]"; 
    req_data = String() + "{\"m2m:cin\": {"

      +
      "\"con\": \"" + data + "\","

      +
      "\"lbl\": \"" + "V1.0.0" + "\","

      //+ "\"rn\": \"" + "cin_"+String(i++) + "\","

      +
      "\"cnf\": \"text\""

      +
      "}}";

      
    int code1 = http.POST(req_data);
    http.end();
    Serial.println(code1);

    ThingSpeak.setField(1,temp);
    ThingSpeak.setField(2,humidity);
    ThingSpeak.setField(3,gas_concentration);
    ThingSpeak.writeFields(myChannelNumber,myWriteAPIKey);
    prev_millis = millis();
  }
  delay(500);
}
