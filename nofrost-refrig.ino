/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com  
*********/

// Import required libraries
#ifdef ESP32
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
#else
  #include <Arduino.h>
  #include <ESP8266WiFi.h>
  #include <Hash.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
#endif
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 4

#define COMPRESSOR 12 // Compressor pinout
//#define FAN  2       // Freezer fan pinout
#define HEATER  14       // Heater pinout
#define WIFI_BUTTON  15       // Wifi button

#define LED  2       // Heater pinout

#define LED_WIFI  13
#define LED_COMP  2   
#define LED_HEAT  5
#define LED_ERROR  0

#define WIFI_WAIT  500
#define WIFI_TRIES  20


#define BEEPS_LOOP  3
#define BEEPS_ERROR  5
#define BEEPS_TEMP  3
#define BEEPS_WIFI 4
#define BEEPS_COMPRESSOR  7
#define BEEPS_REFRIG  2

#define LAST_ERROR_COUNT 10

//String lastErrors[LAST_ERROR_COUNT]; 

uint8_t addrFreeze[8] = { 0x28, 0x2C, 0x21, 0x96, 0x5E, 0x14, 0x01, 0x0E };
uint8_t addrRefrig[8] = { 0x28, 0x0D, 0x53, 0x95, 0x5E, 0x14, 0x01, 0xE9 };
uint8_t addrCompressor[8] = { 0x28, 0xDB, 0x29, 0x79, 0x97, 0x11, 0x03, 0x8A };


float tempFreeze = 0;
float tempRefrig = 0;
float tempCompressor = 0;

float tempMinFreeze = -16;    //порог выключения компрессора
float tempMaxFreeze = -12;    // порог включения компрессора
float tempMaxCompressor = 60; // максимальная температура компрессора
float tempMaxRefrig = 0;      // минимальная температура в холодильнике, после которого начинаем пищать ошибку


bool stateCompressor = false;
bool stateHeater = false;
bool stateWiFi = false;
//bool stateFan = false;
//bool stateDoor1 = false;
//bool stateDoor2 = false;

unsigned long timeNow = 0;

unsigned long timeStartCompressor = 0;
unsigned long timeStopCompressor = 0;
unsigned long timeWorkCompressor = 2 * 60 * 60 * 1000;
unsigned long timeRestCompressor = 15 * 60 * 1000;

unsigned long timeStartHeater = 0;
unsigned long timeStopHeater = 0;
unsigned long timeWorkHeater = 2 * 60 * 1000;
unsigned long timeRestHeater = 4 * 60 * 60 * 1000;

unsigned long timeLastRequest = 0;
unsigned long timeMaxWaitRequest = 60 * 60 * 1000; // после часа без запросов отключим Wifi, чтобы не свистел зазря


// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);


// Replace with your network credentials
const char* ssid = "Keenetic-0186";
const char* password = "R838rPfr";


void switchCompressor(bool on);
void switchHeater(bool on);
void error(const String& message, int beeps);




// Create AsyncWebServer object on port 80
AsyncWebServer webserver(80);



const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .ds-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    .btn {
      font-size: 1.5rem;
      padding: 5px 10px;
      margin: 0 10px;
      background-color: lightgray;
      color: black;
      border: 1px solid black;
      width: 200px;
      height: 50px;
      vertical-align: bottom;
      cursor: pointer;
    }
    .btn-active {
      background-color: green;
      color: white;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <h2>Wi-Daewoo</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Freezer</span> 
    <span id="temperature_freeze">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Refrigerator</span> 
    <span id="temperature_refrig">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Heater</span> 
    <span id="temperature_heater">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <button class="btn" id="compressor" onClick="toggle(this)">Compressor</btn>
  </p>
  <p>
    <button class="btn btn-active" id="fan" onClick="toggle(this)">Fan</btn>
  </p>
  <p>
    <button class="btn btn-active" id="heater" onClick="toggle(this)">Heater</btn>
  </p>
</body>
<script type="text/javascript">
function toggle(el) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        //el.innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/toggle/" + el.id, true);
    xhttp.send();
}
setInterval(function ( ) {
  for (let type of ['freeze', 'refrig', 'heater']) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("temperature_" + type).innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/temperature/" + type, true);
    xhttp.send();
  }
}, 3000);
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
//  if(var == "TEMPERATURE"){
//    return readDSTemperature();
//  }
  return String();
}



void blinkDot() {
//  Serial.println("BLINK DOT");
  for (int i = 0; i<3; i++) {
      delay(500);      
      digitalWrite(LED, LOW);
      delay(100);      
      digitalWrite(LED, HIGH);
      delay(500);      
  }
}

void blinkNum(int num) {
//  Serial.println(String("BLINK NUM") + num);
  if (num == 0) {
    for (int i = 0; i<3; i++) {
      delay(50);      
      digitalWrite(LED, LOW);
      delay(50);      
      digitalWrite(LED, HIGH);
    }
    delay(200);      
  }
  while (num--) {
      digitalWrite(LED, LOW);
      delay(100);      
      digitalWrite(LED, HIGH);
      delay(200);      
  }
}

void blinkIP(IPAddress ip) {
//  Serial.print("BLINK IP ");
//  Serial.println(ip);

  for (int i=0; i<4; i++) {
    delay(1000);
    if (ip[i] >= 100) {
      blinkNum(floor(ip[i] / 100));
      delay(200);
    }
    if (ip[i] >= 10) {
      blinkNum(floor((ip[i]%100)/10));
      delay(200);
    }
    blinkNum(floor(ip[i] % 10));
  }
  
}



void runWebServer() {

  // Route for root / web page
  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
    timeLastRequest = timeNow;
  });
  webserver.on("/temperature/freeze", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(tempFreeze).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/temperature/refrig", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(tempRefrig).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/temperature/heater", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(tempCompressor).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/state/compressor", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(stateCompressor).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/state/heater", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(stateHeater).c_str());
    timeLastRequest = timeNow;
  });
//  webserver.on("/state/door1", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/plain", String(stateDoor1).c_str());
//    timeLastRequest = timeNow;
//  });
//  webserver.on("/state/door2", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/plain", String(stateDoor2).c_str());
//    timeLastRequest = timeNow;
//  });

  webserver.on("/compressor/start", HTTP_GET, [](AsyncWebServerRequest *request){
    switchCompressor(true);
    request->send_P(200, "text/plain", String(stateCompressor).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/compressor/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    switchCompressor(false);
    request->send_P(200, "text/plain", String(stateCompressor).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/heater/start", HTTP_GET, [](AsyncWebServerRequest *request){
    switchHeater(true);
    request->send_P(200, "text/plain", String(stateHeater).c_str());
    timeLastRequest = timeNow;
  });
  webserver.on("/heater/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    switchHeater(false);
    request->send_P(200, "text/plain", String(stateHeater).c_str());
    timeLastRequest = timeNow;
  });

  webserver.begin();
}


void switchWiFi(bool on) {
  if (on) {
    if (!stateWiFi) {
    
        // Connect to Wi-Fi
      WiFi.begin(ssid, password);
      Serial.println("Connecting to WiFi");
      for (int i=0; i<WIFI_TRIES && !stateWiFi; i++) {
        //delay(WIFI_WAIT);
        Serial.print(".");
        blinkDot();
        stateWiFi = WiFi.status() == WL_CONNECTED;
      }
      Serial.println();
    
      if (stateWiFi) {
        Serial.println(WiFi.localIP());
        blinkIP(WiFi.localIP());
      } else {
        error(String("Could not connect with SSID ") + ssid + String(" and password ") + password, BEEPS_WIFI);
        
      }
    
      runWebServer();
    }
  } else {
    if (stateWiFi) {
      Serial.println("Disconnecting WiFi");
      webserver.end();
      // Disconnect Wi-Fi
      WiFi.disconnect();
      stateWiFi = false;
    }
  }
}


void error(const String& message, int beeps) {
//  for (int i = LAST_ERROR_COUNT-1; i>0; i--) {
//    lastErrors[i] = lastErrors[i-1];  
//  }
//  lastErrors[0] = String(timeNow) + message;
//  
  for (int i = 0; i<BEEPS_LOOP; i++) {
    for (int j = 0; j<beeps; j++) {
      digitalWrite(LED_ERROR, HIGH);
      delay(200);
      digitalWrite(LED_ERROR, LOW);
      delay(300);
    }
    delay(5000);
  }
  Serial.println("----- ERROR -----");
  Serial.print("| ");
  Serial.println(message);
  Serial.println("----- ERROR -----");

  if (beeps != BEEPS_WIFI) {
   // switchWiFi(true);
  }
}

void switchCompressor(bool on) {
  if (stateHeater) {
    return error("Compressor start when heater is working", BEEPS_ERROR);
  }
  if (on) {
    digitalWrite(COMPRESSOR, LOW);
    timeStartCompressor = timeNow;
    timeStopCompressor = 0;
  } else {
    digitalWrite(COMPRESSOR, HIGH);
    timeStartCompressor = 0;
    timeStopCompressor = timeNow;
  }
  stateCompressor = on;
  Serial.print("Compressor state: ");
  Serial.println(String(stateCompressor));
}

void switchHeater(bool on) {
  if (stateCompressor) {
    return error("Heater start when compressor is working", BEEPS_ERROR);
  }
  if (on) {
      digitalWrite(HEATER, LOW);
  } else {
      digitalWrite(HEATER, HIGH);
  }
  stateHeater = on;
  Serial.print("Heater state: ");
  Serial.println(String(stateHeater));
}
//
//void printSensorAddress(DeviceAddress deviceAddress) {
//  for (uint8_t i = 0; i < 8; i++) {
//    if (deviceAddress[i] < 16) Serial.print("0");
//    Serial.print(deviceAddress[i], HEX);
//  }
//}


void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println("INIT CONTROLLER");

  pinMode (COMPRESSOR, OUTPUT );
  digitalWrite(COMPRESSOR, HIGH);
  pinMode(HEATER, OUTPUT);
  digitalWrite(HEATER, HIGH);

  pinMode (WIFI_BUTTON, INPUT);  

//  digitalWrite(LED_WIFI, HIGH);
//
//  digitalWrite(LED_COMP, HIGH);

  pinMode(LED_HEAT, OUTPUT);
  pinMode(LED_COMP, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  // Start up the DS18B20 library
  sensors.begin();

//  DeviceAddress addr;
//  for (int i=0; i<sensors.getDeviceCount(); i++) {
//    if (sensors.getAddress(addr, i)) {
//      Serial.print("String(DS18B20 )" + String(i) + String(" addr: "));
//      for (uint8_t i = 0; i < 8; i++) {
//        if (addr[i] < 16) Serial.print("0");
//        Serial.print(addr[i], HEX);
//      }
//      Serial.println();
//    }
//  }


  for(int i=0; i<3; i++) {
    break;
    digitalWrite(LED_WIFI, HIGH);
    delay(200);
    digitalWrite(LED_COMP, HIGH);
    delay(200);
    digitalWrite(LED_HEAT, HIGH);
    delay(200);
    digitalWrite(LED_ERROR, HIGH);
  
    delay(200);
    
    digitalWrite(LED_WIFI, LOW);
    delay(200);
    digitalWrite(LED_COMP, LOW);
    delay(200);
    digitalWrite(LED_HEAT, LOW);
    delay(200);
    digitalWrite(LED_ERROR, LOW);
    delay(200);
  }

  //enableWiFi();
}



void checkTemperatureSensor(const String& id, float temperature) {
    if (temperature == -127.00) {
      error(String("Failed to read from DS18B20 sensor: " + id), BEEPS_TEMP);
    } else {
      Serial.print(id);
      Serial.print(" temperature: ");
      Serial.println(temperature);
    }
}


void loop(){
    if (digitalRead(WIFI_BUTTON)) {
      if (!stateWiFi) {
        for (int i=0; i<3; i++) {
          error("TEST ERROR", BEEPS_WIFI);
          stateWiFi = true;
          delay(2000);
        }
      }
      return;
      if (stateWiFi) {
        Serial.println("Wifi button OFF");
        switchWiFi(false);
      } else {
        Serial.println("Wifi button ON");
        switchWiFi(true);
      }
    }

    return;

    sensors.requestTemperatures(); 
    tempFreeze = sensors.getTempC(addrFreeze);
    tempRefrig = sensors.getTempC(addrRefrig);
    tempCompressor = sensors.getTempC(addrCompressor);
    checkTemperatureSensor("Freeze", tempFreeze);
    checkTemperatureSensor("Refrig", tempRefrig);
    checkTemperatureSensor("Compressor", tempCompressor);

    // Проверим компрессор на перегрев
    if (tempCompressor > tempMaxCompressor) {
      switchCompressor(false);
      error("Compressor overheat!", BEEPS_COMPRESSOR);
      return;
    }

    // Проверим температуру в холодильнике, возможно открыта дверца
    if (tempRefrig > tempMaxRefrig) {
      error("Refrig temperature too high. Is door open?", BEEPS_REFRIG);
    }

    timeNow = millis();
    if (stateCompressor) {
      switchHeater(false);
      // проверим, что температура не ниже минимальной, иначе отключаем компрессор  
      if (tempFreeze < tempMinFreeze) {
        return switchCompressor(false);
      }
    } else {
      if (stateHeater) {
        // проверим, что не работает тенка
        if (timeNow - timeStartHeater > timeWorkHeater) {
          return switchHeater(false);
        }
      } else if (timeNow - timeStopHeater > timeRestHeater) {
          // или не пришло время для ее запуска
          return switchHeater(true);
      }

      // проверим, что температура не выше максимальной, иначе включаем компрессор  
      if (tempFreeze > tempMaxFreeze) {
        return switchCompressor(true);
      }
    }

    if (timeNow - timeLastRequest > timeMaxWaitRequest) {
      switchWiFi(false);
    }

    delay( 5000 );
}
