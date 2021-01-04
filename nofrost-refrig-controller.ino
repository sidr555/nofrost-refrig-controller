/*********
  Dmitry Sidorov
  Complete project details at https://RandomNerdTutorials.com  
  This version is not server controlled
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

#define FAN  2              // Freezer fan pinout
#define COMPRESSOR 12       // Compressor pinout
#define COMPRESSOR_FAN  13  // Compressor pinout
#define HEATER  14          // Heater pinout
#define WIFI_BUTTON  15     // Wifi button


//#define LED  2       // Blinking led pinout
#define BLINK_LED  5       // Blinking led pinout

//#define LED_WIFI  13
//#define LED_COMP  2   
#define LED_HEAT  5
#define LED_ERROR  0

#define WIFI_WAIT  500
#define WIFI_TRIES  20


#define BEEPS_LOOP  1

#define BEEPS_REFRIG  2
#define BEEPS_TEMP  3
#define BEEPS_WIFI 4
#define BEEPS_ERROR  5
#define BEEPS_COMPRESSOR  6
#define BEEPS_START_COMPRESSOR_WITH_HEATER  7
#define BEEPS_START_HEATER_WITH_COMPRESSOR  8


#define LAST_ERROR_COUNT 5

uint8_t addrFreeze[8] =     { 0x28, 0x2C, 0x21, 0x96, 0x5E, 0x14, 0x01, 0x0E };
uint8_t addrRefrig[8] =     { 0x28, 0x0D, 0x53, 0x95, 0x5E, 0x14, 0x01, 0xE9 };
uint8_t addrCompressor[8] = { 0x28, 0xDB, 0x29, 0x79, 0x97, 0x11, 0x03, 0x8A };
uint8_t addrPCB[8] =        { 0x28, 0x05, 0x16, 0x11, 0x04, 0x00, 0x00, 0x3C };


float tempFreeze = 0;
float tempRefrig = 0;
float tempCompressor = 0;
float tempPCB = 0;

float tempMinFreeze = -20;    //порог выключения компрессора
float tempMaxFreeze = -10;    // порог включения компрессора
float tempMaxCompressor = 90; // максимальная температура компрессора
float tempMaxRefrig = 5;      // минимальная температура в холодильнике, после которого начинаем пищать ошибку


bool stateCompressor = false;
bool stateHeater = false;
bool stateWiFi = false;
bool stateFan = false;
bool stateCompressorFan = false;
//bool stateDoor1 = false;
//bool stateDoor2 = false;

unsigned long timeNow = 0;

unsigned long timeStartCompressor = 0;
unsigned long timeStopCompressor = 0;
unsigned long timeWorkCompressor = 2 * 60 * 60 * 1000;  // работать компрессору не более 2 часов
unsigned long timeRestCompressor = 30 * 60 * 1000;      // после выключения компрессора дать ему отдохнуть не менее 30 минут
unsigned long timeWorkCompressorFan = 15 * 60 * 1000;   // после выключения компрессора дать вентилятору поработать не менее 15 минут

unsigned long timeStartHeater = 0;
unsigned long timeStopHeater = 0;
unsigned long timeWorkHeater = 2 * 60 * 1000;           // работать тэнке не более 2 минут
unsigned long timeRestHeater = 4 * 60 * 60 * 1000;      // после чего не включать 4 часа

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
void error(const String& message, uint8_t beeps);


//struct Error {
//  unsigned long time;
//  uint8_t beeps;
//  String msg;
//}
//Error lastErrors[LAST_ERROR_COUNT]; 




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
    <span id="temperature_freeze">%TEMP_FREEZE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Refrigerator</span> 
    <span id="temperature_refrig">%TEMP_REFRIG%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Compressor</span> 
<span id="temperature_heater">%TEMP_COMPRESSOR%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="ds-labels">Controller</span> 
    <span id="temperature_heater">%TEMP_OUT%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <button class="btn %COMPRESSOR_BUTTON_CLASS%" id="compressor" onClick="switch(this)">Compressor (%COMPRESSOR_TIME%)</btn>
  </p>
  <p>
    <button class="btn %HEATER_BUTTON_CLASS%" id="heater" onClick="switch(this)">Heater %HEATER_TIME%</btn>
  </p>
  
<!--  <p>
    <h2>Errors</h2>
    <ul>
      <li class="err-msg"><div class="err-time">%errtime0%</div>%errmsg0%</li>
      <li class="err-msg"><div class="err-time">%errtime1%</div>%errmsg1%</li>
      <li class="err-msg"><div class="err-time">%errtime2%</div>%errmsg2%</li>
      <li class="err-msg"><div class="err-time">%errtime3%</div>%errmsg3%</li>
      <li class="err-msg"><div class="err-time">%errtime4%</div>%errmsg4%</li>
      <li class="err-msg"><div class="err-time">%errtime5%</div>%errmsg5%</li>
      <li class="err-msg"><div class="err-time">%errtime6%</div>%errmsg6%</li>
      <li class="err-msg"><div class="err-time">%errtime7%</div>%errmsg7%</li>
      <li class="err-msg"><div class="err-time">%errtime8%</div>%errmsg8%</li>
      <li class="err-msg"><div class="err-time">%errtime9%</div>%errmsg9%</li>
    </ul>
  </p>
  -->
</body>
<script type="text/javascript">
function switch(el) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        //el.innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/switch/" + el.id, true);
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


String getMinRange(unsigned long from, unsigned long to) {
  if (from == 0) {
    return String("0-");
  } else if (to == 0) {
    return String("-0");
  }
  return String((int)((to - from)/1000/60));
}

// Replaces placeholder with DHT values
String processor(const String& var){
  if (var == "TEMP_FREEZE") return String(tempFreeze);
  if (var == "TEMP_REFRIG") return String(tempRefrig);
  if (var == "TEMP_COMPRESSOR") return String(tempCompressor);
  if (var == "TEMP_OUT") return String(tempPCB);
  if (var == "COMPRESSOR_BUTTON_CLASS"  && stateCompressor) return String("btn-active");
  if (var == "HEATER_BUTTON_CLASS" && stateHeater) return String("btn-active");
  if (var == "COMPRESSOR_TIME") {
    if (stateCompressor) {
      return String("w:") + getMinRange(timeStartCompressor, timeNow) + String("|r:") + getMinRange(timeStopCompressor,timeStartCompressor);
    } else {
      return String("r:") + getMinRange(timeStopCompressor, timeNow) + String("|w:") + getMinRange(timeStartCompressor,timeStopCompressor);
    }
  }
  if (var == "HEATER_TIME") {
    if (stateHeater) {
      return String("w:") + getMinRange(timeStartHeater, timeNow) + String("|r:") + getMinRange(timeStopHeater,timeStartHeater);
    } else {
       return String("r:") + getMinRange(timeStopHeater, timeNow) + String("|w:") + getMinRange(timeStartHeater,timeStopHeater);
    }
  }
//  if (var == "errtime0") return String(lastErrors[0].time);
//  if (var == "errtime1") return String(lastErrors[1].time);
//  if (var == "errtime2") return String(lastErrors[2].time);
//  if (var == "errtime3") return String(lastErrors[3].time);
//  if (var == "errtime4") return String(lastErrors[4].time);
//  if (var == "errmsg0") return String(lastErrors[0].msg);
//  if (var == "errmsg1") return String(lastErrors[1].msg);
//  if (var == "errmsg2") return String(lastErrors[2].msg);
//  if (var == "errmsg3") return String(lastErrors[3].msg);
//  if (var == "errmsg4") return String(lastErrors[4].msg);
  return String();
}



void blinkDot() {
//  Serial.println("BLINK DOT");
  for (int i = 0; i<3; i++) {
      delay(500);      
      digitalWrite(BLINK_LED, LOW);
      delay(100);      
      digitalWrite(BLINK_LED, HIGH);
      delay(500);      
  }
}

void blinkNum(int num) {
//  Serial.println(String("BLINK NUM") + num);
  if (num == 0) {
    for (int i = 0; i<3; i++) {
      delay(50);      
      digitalWrite(BLINK_LED, LOW);
      delay(50);      
      digitalWrite(BLINK_LED, HIGH);
    }
    delay(200);      
  }
  while (num--) {
      digitalWrite(BLINK_LED, LOW);
      delay(100);      
      digitalWrite(BLINK_LED, HIGH);
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

//  webserver.on("/compressor/start", HTTP_GET, [](AsyncWebServerRequest *request){
//    switchCompressor(true);
//    request->send_P(200, "text/plain", String(stateCompressor).c_str());
//    timeLastRequest = timeNow;
//  });
//  webserver.on("/compressor/stop", HTTP_GET, [](AsyncWebServerRequest *request){
//    switchCompressor(false);
//    request->send_P(200, "text/plain", String(stateCompressor).c_str());
//    timeLastRequest = timeNow;
//  });
  webserver.on("/switch/compressor", HTTP_GET, [](AsyncWebServerRequest *request){
    switchCompressor(!stateCompressor);
    request->send_P(200, "text/plain", String(stateCompressor).c_str());
    timeLastRequest = timeNow;
  });
//  webserver.on("/heater/start", HTTP_GET, [](AsyncWebServerRequest *request){
//    switchHeater(true);
//    request->send_P(200, "text/plain", String(stateHeater).c_str());
//    timeLastRequest = timeNow;
//  });
//  webserver.on("/heater/stop", HTTP_GET, [](AsyncWebServerRequest *request){
//    switchHeater(false);
//    request->send_P(200, "text/plain", String(stateHeater).c_str());
//    timeLastRequest = timeNow;
//  });
  webserver.on("/switch/heater", HTTP_GET, [](AsyncWebServerRequest *request){
    switchHeater(!stateHeater);
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
        //Serial.println(WiFi.localIP());
        //blinkIP(WiFi.localIP());
      } else {
        error(String("Could not connect with SSID ") + ssid + String(" and password ") + password, BEEPS_WIFI);
        
      }
    
      runWebServer();
    }
  } else {
    if (stateWiFi) {
      blinkIP(WiFi.localIP());

//      Serial.println("Disconnecting WiFi");
//      webserver.end();
//      // Disconnect Wi-Fi
//      WiFi.disconnect();
//      stateWiFi = false;
    }
  }
}


void error(const String& message, uint8_t beeps) {
/*  
  for (int i = LAST_ERROR_COUNT-1; i>0; i--) {
    if (lastErrors[i-1].time > 0) {
      lastErrors[i].time = lastErrors[i-1].time;  
      lastErrors[i].beeps = lastErrors[i-1].beeps;  
      lastErrors[i].msg = lastErrors[i-1].msg;  
    }
  }
  lastErrors[0].time = timeNow;
  lastErrors[0].beeps = beeps;
  lastErrors[0].msg = message;
 */ 
  for (int i = 0; i<BEEPS_LOOP; i++) {
    for (int j = 0; j<beeps; j++) {
      digitalWrite(LED_ERROR, HIGH);
      delay(30);
      digitalWrite(LED_ERROR, LOW);
      delay(150);
    }
    delay(3000);
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
  if (on) {
    if (stateHeater) {
      return error("Compressor start when heater is working", BEEPS_START_COMPRESSOR_WITH_HEATER);
    }
    digitalWrite(COMPRESSOR, LOW);
//    digitalWrite(LED_COMP, HIGH);
    timeStartCompressor = timeNow;
  } else {
    digitalWrite(COMPRESSOR, HIGH);
//    digitalWrite(LED_COMP, LOW);
    timeStopCompressor = timeNow;
  }
  stateCompressor = on;
  Serial.print("Compressor state: ");
  Serial.println(String(stateCompressor));
}
//
//void switchCompressorFan(bool on) {
//  if (on) {
//    digitalWrite(COMPRESSOR_FAN, LOW);
//  } else {
//    digitalWrite(COMPRESSOR_FAN, HIGH);
//  }
//  stateCompressorFan = on;
////  Serial.print("Compressor state: ");
////  Serial.println(String(stateCompressor));
//}


void switchHeater(bool on) {
  if (on) {
    if (stateCompressor) {
      return error("Heater start when compressor is working", BEEPS_START_HEATER_WITH_COMPRESSOR);
    }
    digitalWrite(HEATER, LOW);
    digitalWrite(LED_HEAT, HIGH);
    timeStartHeater = timeNow;
  } else {
    digitalWrite(HEATER, HIGH);
    digitalWrite(LED_HEAT, LOW);
    timeStopHeater = timeNow;
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

//  pinMode(FAN, OUTPUT);
//  pinMode (COMPRESSOR_FAN, OUTPUT );
//  digitalWrite(COMPRESSOR_FAN, HIGH); // HIGH LEVEL IS OFF

  pinMode (COMPRESSOR, OUTPUT );
  pinMode(HEATER, OUTPUT);

//  digitalWrite(FAN, HIGH);            // HIGH LEVEL IS OFF
//  digitalWrite(COMPRESSOR_FAN, LOW); // HIGH LEVEL IS OFF
  digitalWrite(COMPRESSOR, HIGH);     // HIGH LEVEL IS OFF
  digitalWrite(HEATER, HIGH);         // HIGH LEVEL IS OFF

  pinMode (WIFI_BUTTON, INPUT);  

//  digitalWrite(LED_WIFI, HIGH);
//
//  digitalWrite(LED_COMP, HIGH);

  pinMode(LED_HEAT, OUTPUT);
//  pinMode(LED_COMP, OUTPUT);
//  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  // Start up the DS18B20 library
  sensors.begin();

//  DeviceAddress addr;
//  Serial.println(String("Scan dev addresses: ") + String(sensors.getDeviceCount()));
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


  // Нас тарте моргнем светиками
  for(int i=0; i<3; i++) {
//    digitalWrite(LED_WIFI, HIGH);
//    delay(20);
//    digitalWrite(LED_COMP, HIGH);
//    delay(20);
    digitalWrite(LED_HEAT, HIGH);
    delay(20);
    digitalWrite(LED_ERROR, HIGH);
    delay(20);
    
//    digitalWrite(LED_WIFI, LOW);
//    delay(20);
//    digitalWrite(LED_COMP, LOW);
//    delay(20);
    digitalWrite(LED_HEAT, LOW);
    delay(20);
    digitalWrite(LED_ERROR, LOW);
    delay(20);
  }

//  for (int i=0; i<100; i++) {
//    digitalWrite(COMPRESSOR_FAN, LOW); // HIGH LEVEL IS OFF
////    switchCompressorFan(true);
//    delay(500);
//    digitalWrite(COMPRESSOR_FAN, HIGH); // HIGH LEVEL IS OFF
////    switchCompressorFan(false);
//    delay(1000);
//  }     


  switchWiFi(true);
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
  Serial.println("LOOP");

    if (digitalRead(WIFI_BUTTON)) {
      if (stateWiFi) {
        Serial.println("Wifi button OFF");
        switchWiFi(false);
      } else {
        Serial.println("Wifi button ON");
        switchWiFi(true);
      }
    }


    sensors.requestTemperatures(); 
    tempFreeze = sensors.getTempC(addrFreeze);
    tempRefrig = sensors.getTempC(addrRefrig);
    tempCompressor = sensors.getTempC(addrCompressor);
    tempPCB = sensors.getTempC(addrPCB);
    checkTemperatureSensor("Freeze", tempFreeze);
//    checkTemperatureSensor("Refrig", tempRefrig);
//    checkTemperatureSensor("Compressor", tempCompressor);
//    checkTemperatureSensor("PCB", tempPCB);

    // Проверим компрессор на перегрев
    if (tempCompressor > tempMaxCompressor) {
      switchCompressor(false);
      error("Compressor overheat!", BEEPS_COMPRESSOR);
      return;
    }


    
    timeNow = millis();
    if (stateCompressor) {
      switchHeater(false);
      // проверим, что температура не ниже минимальной, иначе отключаем компрессор  
      // либо когда он отработал положенное время
      if ((tempFreeze < tempMinFreeze && tempRefrig < tempMaxRefrig) 
        || timeNow - timeStartCompressor > timeWorkCompressor ) {
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
      if (tempFreeze > tempMaxFreeze && (timeStopCompressor == 0 || timeNow - timeStopCompressor > timeRestCompressor)) {
        return switchCompressor(true);
      }
    }

////    // Включим вентилятор компрессора
//    if (stateCompressor || (timeStopCompressor > 0 && timeNow - timeStopCompressor <= timeWorkCompressorFan)) {
//      switchCompressorFan(true);
//    } else {
//      switchCompressorFan(false);
//    }


    // Проверим температуру в холодильнике, возможно открыта дверца
    if (!stateCompressor && tempRefrig > tempMaxRefrig ) {
      error("Refrig temperature too high. Is door open?", BEEPS_REFRIG);
    }

//    if (timeNow - timeLastRequest > timeMaxWaitRequest) {
//      switchWiFi(false);
//    }

    delay( 30000 );
}
