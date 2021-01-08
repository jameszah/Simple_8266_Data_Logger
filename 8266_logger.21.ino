/*------------------------------------------------------------------------------

  Simple Data Logger

  Use a Wemos d1 mini and a sensor, and this program logs data without internet, SD card, ...
  or anything -- just the Wemos and the sensor, and it serves up graphs to your computer or
  your phone of the following data logging intervals

  - 3 minutes of 1-Second samples
  - 3 hours of 1-Minute samples
  - 1 day of 5-Minute samples
  - 10 days of 1-hour averages
  - 180 days of daily averages
  - and 100 samples of live moving graph at up to 1/10 of a second samples

  It logs to memory that san be served as a graph, or a data file, and it saves the data to the
  internal Wemos 8266 flash in case of power failure or reboot.

  It uses WiFiManager to get ssid name and password with normal 192.168.4.1 startup.

  Set Arduino board to LOLIN(WEMOS) D1 R2 & mini

  by James Zahary Dec 18, 2020
     jamzah.plc@gmail.com

  v20 - Dec 30, 2020 - reformat text output with Exceltime
  v21 - Jan  8, 2021 - fix the fake data function
  
  https://github.com/jameszah/Simple_8266_Data_Logger


    Simple_8266_Data_Logger is licensed under the
    GNU General Public License v3.0

Using library ESP8266WiFi at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\ESP8266WiFi 
Using library DNSServer at version 1.1.1 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\DNSServer 
Using library ESP8266WebServer at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\ESP8266WebServer 
Using library WebSockets at version 2.3.1 in folder: C:\Users\James\Documents\Arduino\libraries\WebSockets 
Using library WiFiManager at version 2.0.3-alpha in folder: C:\Users\James\Documents\Arduino\libraries\WiFiManager 
Using library Ticker at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\Ticker 
Using library LittleFS at version 0.1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\LittleFS 
Using library BME280 at version 2.3.0 in folder: C:\Users\James\Documents\Arduino\libraries\BME280 
Using library Wire at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\Wire 
Using library Hash at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\Hash 
Using library SPI at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.7.4\libraries\SPI 


   Starting point of this program was from Acrobatic in 2018 below:
   - I think it was the chart.js stuff with a moving graph
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  10/20/2018
  Author: Cisco • A C R O B O T I C
  Platforms: ESP8266
  Language: C++/Arduino
  File: bmp180_gui.ino
  ------------------------------------------------------------------------------
  Description:
  Code for YouTube video demonstrating how to plot sensor data from a BMP180 tem-
  perature, and barometric pressure sensor. A web server running on the ESP8266
  serves a web page that uses Chart.js and websockets to plot the data.
  https://youtu.be/lEVoRJSsEa8
  ------------------------------------------------------------------------------
  Do you like my work? You can support me:
  https://patreon.com/acrobotic
  https://paypal.me/acrobotic
  https://buymeacoff.ee/acrobotic
  ------------------------------------------------------------------------------
  Please consider buying products and kits to help fund future Open-Source
  projects like this! We'll always put our best effort in every project, and
  release all our design files and code for you to use.
  https://acrobotic.com/
  https://amazon.com/shops/acrobotic
  ------------------------------------------------------------------------------
  License:
  Please see attached LICENSE.txt file for details.
  ------------------------------------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <Ticker.h>

#include "FS.h"
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED true

#include <time.h>
//int timezone = -6; // -6 for daylight savings, -7 regular
int dst = 0;
struct tm * timeinfo;
time_t boottime;
time_t now;
unsigned long unixtime;


void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("  FILE: ");
    Serial.print(root.fileName());
    Serial.print("  SIZE: ");
    Serial.print(file.size());
    time_t cr = file.getCreationTime();
    time_t lw = file.getLastWrite();
    file.close();
    struct tm * tmstruct = localtime(&cr);
    Serial.printf("    CREATION: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    tmstruct = localtime(&lw);
    Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
  }
}


Ticker timer;
Ticker timer1Sec;
Ticker timer1Min;
int counter = 0;
int FiveMin;
int OneHour;
int OneDay;

#define Max_1_Sec 180
#define Max_1_Min 180
#define Max_5_Min 288
#define Max_1_Hour 240
#define Max_1_Day 180

typedef struct {
  float f1;
  float f2;
  float f3;
  unsigned long t;
} dp;

dp Buf_1_Sec  [Max_1_Sec];
dp Buf_1_Min  [Max_1_Min];
dp Buf_5_Min  [Max_5_Min];
dp Buf_1_Hour [Max_1_Hour];
dp Buf_1_Day [Max_1_Day];

int Buf_In_1_Sec = 0;
int Buf_Out_1_Sec = 0;
int Buf_In_1_Min = 0;
int Buf_Out_1_Min = 0;
int Buf_In_5_Min = 0;
int Buf_Out_5_Min = 0;
int Buf_In_1_Hour = 0;
int Buf_Out_1_Hour = 0;
int Buf_In_1_Day = 0;
int Buf_Out_1_Day = 0;

bool get_data_1Sec = false;
bool get_data_1Min = false;
bool get_data_5Min = false;
bool get_data_1Hour = false;
bool get_data_1Day = false;
bool get_data_live = false;

bool get_data = false;
bool do_a_backup = false;

unsigned long fake = 0;

//
//  BME280 sensor stuff
//
#include <BME280I2C.h>
#include <Wire.h>

BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
// Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
BME280::PresUnit presUnit(BME280::PresUnit_Pa);

float temp(NAN), hum(NAN), pres(NAN);

/*
  #include <OneWire.h>
  #include <DallasTemperature.h>
  #define DS_PIN 0  // gpio 0 - D3
  OneWire oneWire(DS_PIN);
  DallasTemperature sensors(&oneWire);
  DeviceAddress  DS18B20[3];
*/

/*
  Wemos d1 mini

  Pin  ESP-8266 Pin Function
  D0  GPIO16
  D1  GPIO5  SCL                       -- bme
  D2  GPIO4  SDA                       -- bme
  D3  GPIO0  10k Pull-up               -- ds18b20
  D4  GPIO2  10k Pull-up, BUILTIN_LED
  D5  GPIO14 SCK
  D6  GPIO12 MISO
  D7  GPIO13 MOSI
  D8  GPIO15 10k Pull-down, SS
*/

// Connecting to the Internet

// Running a web server
ESP8266WebServer server;

// Adding a websocket to the server
WebSocketsServer webSocket = WebSocketsServer(81);

// Serving a web page (from flash memory)
// formatted as a string literal!
//<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js'></script>
char webpage[] PROGMEM = R"=====(
<html>
<!-- Adding a data chart using Chart.js -->
<head>
  <script src='https://cdn.jsdelivr.net/npm/chart.js@2.9.3'></script>
  <script src='https://cdn.jsdelivr.net/npm/moment@2.27.0'></script>
  <script src='https://cdn.jsdelivr.net/npm/chartjs-adapter-moment@0.1.1'></script>
  
</head>
<body onload="javascript:init()">

<hr />
<div>
  <canvas id="line-chart" width="800" height="450"></canvas>
</div>
<br>
  <button onClick="f_seconds()" id="seconds">Seconds</button>
  <button onClick="f_minutes()" id="minutes">Minutes</button>
  <button onClick="f_5minutes()" id="5minutes">5 Minutes</button>
  <button onClick="f_hours()" id="hours">Hours</button>
  <button onClick="f_days()" id="days">Days</button>
  <button onClick="f_live()" id="live">Live</button>
  
  <!-- Adding a slider for controlling data rate -->

  <input type="range" min="1" max="30" value="5" id="dataRateSlider" oninput="sendDataRate()" />
  <label for="dataRateSlider" id="dataRateLabel">Update: 5 /10 Sec</label>
         
  <button onClick="f_write()" id="write">Write Data to Flash</button>
  
<hr>
<div id="p2"></div>  
<div id="danger">
 <hr>
 <h1>These are dangerous buttons!</h1>
  <button style=" background-color:red; border-color:blue; color:white" onClick="f_erase()" id="era">Erase all current Data</button>
  <button style=" background-color:blue; border-color:red; color:white" onClick="f_fake()" id="fake">Make 3 days of fake data</button>
  <button style=" background-color:red; border-color:blue; color:white" onClick="f_delete()" id="del">Delete Data from Flash</button>
  <hr>
  </div>

<!-- Adding a websocket to the client (webpage) -->
<script>
  var webSocket, dataPlot;
  var maxDataPoints = 100;
  
  function removeData(){
    dataPlot.data.labels.shift();
    dataPlot.data.datasets[0].data.shift();
  }
  function addData(label, data) {
    if(dataPlot.data.labels.length > maxDataPoints) removeData();
    dataPlot.data.labels.push(label);
    dataPlot.data.datasets[0].data.push(data);
    dataPlot.update();
  }
  
  function init() {
    webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
    dataPlot = new Chart(document.getElementById("line-chart"), {
      type: 'line',
      data: {
        labels: [],
        datasets: [
        {
          data: [],
          label: "BME Temperature (C)",
          yAxisID: "A",
          borderColor: "red",
          fill: false
        },
        {
          data: [],
          label: "Pressure (mPa)",
          yAxisID: "B",                  // humid and pressure same axis
          borderColor: "blue",
          fill: false
        },
        {
          data: [],
          label: "Humidity (%)",
          yAxisID: "B",
          borderColor: "green",
          fill: false,
          display: false
        }
        ]
      },
      options: {
        title: {
          display: true,
          text: 'Logger 21 - Click Seconds, Minutes, ... and Click Temperature, Pressure, ... to add or remove it'
        },
        scales: {
           xAxes: [{
                type: 'time',
                distribution: 'series',
                ticks: {
                   autoSkip: true,
                   maxTicksLimit: 20
                        }
            }],
           yAxes: [{
             id: 'A',
              type: 'linear',
              position: 'left',
            }, {
              id: 'B',
              type: 'linear',
               position: 'right',
        
      }]
    }
  }
    });

    webSocket.onclose = async (event) => {
        console.error(event);    
        dataPlot.options.title.text = " Logger21 - Hit Refresh - Socket has closed " ; 
   }

    webSocket.onmessage = function(event) {
      var mydata = JSON.parse(event.data);
      var mytype = mydata.type;
      var myLabel = mydata.myLabel;
      var nowLabel = new Date();
      
      var mydataarray = mydata.value;
      
      //console.log(mydataarray.length, mydataarray);

      if (mytype == "live") {
         var data = JSON.parse(event.data);
         var today = new Date();
         //var t =  today.getSeconds();
         //console.log('live',mydataarray);
         //addData('', mydataarray);
           //function addData(label, data) {
           if(dataPlot.data.labels.length > maxDataPoints) {
                dataPlot.data.labels.shift();
                dataPlot.data.datasets[0].data.shift();
                dataPlot.data.datasets[1].data.shift();
                dataPlot.data.datasets[2].data.shift();
           }
           //dataPlot.data.labels.push('');
           dataPlot.data.labels.push(mydataarray.y*1000 + mydataarray.y2);
           dataPlot.data.datasets[0].data.push(mydataarray.x);
           dataPlot.data.datasets[1].data.push(mydataarray.x2);
           dataPlot.data.datasets[2].data.push(mydataarray.x3);
            
           dataPlot.update();
           
           if (dataPlot.data.labels.length < maxDataPoints) {             
             document.getElementById("p2").innerHTML += (25569 + (mydataarray.y + mydataarray.y2/1000)/ 86400) +  "  ,  " + mydataarray.x + "  ,  " + mydataarray.x2 + "  ,  " + mydataarray.x3  + "<br>";
           } else {
              document.getElementById("p2").innerHTML = "Too many points!"; 
           }
          
      } else if (mytype == "LIVE") {
        //dataPlot.data.datasets[0].label = myLabel + " - " + nowLabel;  // data.push(mydataarray[i].x);
        dataPlot.options.title.text = myLabel + " - " + nowLabel; 
        document.getElementById("p2").innerHTML = "<p>&nbsp;</p>  ExcelTime  ,  Temperature (C)  ,  Pressure (mPa)  ,  Humidity (%)  <br>";
        //console.log('LIVE',dataPlot.data.datasets[0].data.length); //jz
        var len = dataPlot.data.datasets[0].data.length;
        for(var i = 0; i <= len ; i++ ) {
          dataPlot.data.labels.pop();
          dataPlot.data.datasets[0].data.pop();
          dataPlot.data.datasets[1].data.pop();
          dataPlot.data.datasets[2].data.pop();
          
         }
         dataPlot.update();
      
      } else {
          dataPlot.options.title.text = myLabel + " - " + nowLabel;
          //dataPlot.data.datasets[0].label = myLabel; // + "  -  " + nowLabel;;  // data.push(mydataarray[i].x);
          document.getElementById("p2").innerHTML = "<p>&nbsp;</p>ExcelTime  ,  Temperature (C)  ,  Pressure (mPa)  ,  Humidity (%)   ,  Browser Time<br>";
          var len = dataPlot.data.datasets[0].data.length;
          for(var i = 0; i <= len ; i++ ) {
            dataPlot.data.labels.pop();
            dataPlot.data.datasets[0].data.pop();
            dataPlot.data.datasets[1].data.pop();
            dataPlot.data.datasets[2].data.pop();
            
           }
           dataPlot.update();
       

       for(var i = 0; i < mydataarray.length; i++ ) {   
      
          var t = new Date(mydataarray[i].y*1000);
          
          dataPlot.data.labels.push(mydataarray[i].y*1000);

          dataPlot.data.datasets[0].data.push(mydataarray[i].x);
          dataPlot.data.datasets[1].data.push(mydataarray[i].x2);
          dataPlot.data.datasets[2].data.push(mydataarray[i].x3);
          
          document.getElementById("p2").innerHTML += (25569 + mydataarray[i].y / 86400)  + "  ,  " + mydataarray[i].x + "  ,  " + mydataarray[i].x2 + "  ,  " + mydataarray[i].x3 + "  ,  "  + t + "<br>";
       }
      
      dataPlot.update();
     
      }
    }
  }

  function f_seconds(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else {
      console.log('got seconds click');
      document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
      document.getElementById("dataRateSlider").value = 5;
      webSocket.send("s");
    }
  }

  function f_minutes(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else {
      console.log('got minutes click');
      document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
      document.getElementById("dataRateSlider").value = 5;
      webSocket.send("m");
    }
  }
  
  function f_5minutes(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else {
      console.log('got 5 minutes click');
      document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
      document.getElementById("dataRateSlider").value = 5;
      webSocket.send("f");
    }
  }
  function f_hours(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else {
      console.log('got hours click');
      document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
      document.getElementById("dataRateSlider").value = 5;
      webSocket.send("h");
    }
  }
  function f_days(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else { 
      console.log('got days click');
      document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
      document.getElementById("dataRateSlider").value = 5;
      webSocket.send("y");
    }
  }
  function f_fake(){
    console.log('got fake click');
    document.getElementById("dataRateLabel").innerHTML = "Update: 5 /10 Sec";
    document.getElementById("dataRateSlider").value = 5;
    webSocket.send("X");
  }

  function f_live(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Logger21 - Hit Refresh - Socket has closed " ; 
     } else {  
      console.log('got live click');
      webSocket.send("l");
    }
  }
  function f_write(){
     if (webSocket.readyState === WebSocket.CLOSED) {
       dataPlot.options.title.text = " Hit Refresh - Socket has closed " ; 
     } else {  
      console.log('got write click');
      webSocket.send("w");
    }
  }

  function f_delete(){
    console.log('got delete click');
    webSocket.send("d");
  }
  function f_erase(){
    console.log('got erase click');
    webSocket.send("e");
  }
  
  function sendDataRate(){
    var dataRate = document.getElementById("dataRateSlider").value;
    webSocket.send(dataRate);
    dataRate = dataRate ;
    document.getElementById("dataRateLabel").innerHTML = "Update: " + dataRate + " /10 Sec" ; //"Update: " + dataRate + " sec";
  }
</script>
</body>
</html>
)=====";


void setup() {

  Serial.begin(115200);
  Serial.println("             ");
  Serial.println("Logger21");

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.autoConnect("Logger");
  Serial.println("connected...yeey :)");

  WiFi.mode(WIFI_STA);
  WiFi.hostname("Logger");

  // put your setup code here, to run once:
  //  WiFi.begin(ssid, password);
  //  Serial.begin(115200);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  //#define TIMEZONE "GMT0BST,M3.5.0/01,M10.5.0/02"             // your timezone  -  this is GMT

  setenv("TZ", "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00", 1);  // mountain time zone
  tzset();


  server.on("/", []() {
    server.send_P(200, "text/html", webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Wire.begin();

  while (!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch (bme.chipModel())
  {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }

  /*
     sensors.begin();

    int available = sensors.getDeviceCount();
    Serial.print("DS Device count: ");
    Serial.println( available);

    for(int x = 0; x!= available; x++)
    {
      if(sensors.getAddress(DS18B20[x], x))
      {
        //Serial.println(DS18B20[x]);
        sensors.setResolution(DS18B20[x], 12);
      }
    }
  */

  Serial.println("Mount LittleFS");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    Serial.println("Formatting LittleFS filesystem");
    LittleFS.format();
    if (!LittleFS.begin()) {
      Serial.println("LittleFS mount failed");
      return;
    }
  }

  listDir("/");

  ReadFile2();

  Serial.println("\nWaiting for time");
  //delay(10000);

  while (time(nullptr) < 1000) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("");
  now = time(nullptr);
  Serial.println(ctime(&now));
  Serial.println(now);

  boottime = now;
  boottime = boottime - millis() / 1000;
  Serial.printf("boottime correctmod  %d\n", boottime);

  unsigned long rightnow = time(nullptr);

  int OneMin = (60 - (rightnow % 60) + 30) % 60;
  Serial.printf("Let us just delay until we get to the middle of minute: %d seconds\n", OneMin);
  delay (OneMin * 1000);

  rightnow = time(nullptr);
  counter = 0;
  FiveMin = 5 - (rightnow % 300 ) / 60;
  OneHour = 60 - (rightnow % 3600 ) / 60;
  OneDay =  1440 - (rightnow % (1440 * 60) ) / 60;
  OneDay = (OneDay - 9 * 60 + 1440 ) % 1440;           // 5pm or GMT midnight to = 8 AM Mountain Time


  Serial.printf("Right Now = %d, in Minutes %d\n", rightnow, rightnow % 60);
  Serial.printf("Five Min index = %d\n", FiveMin);
  Serial.printf("One Hour index = %d\n", OneHour);
  Serial.printf("One Day index = %d\n", OneDay);

  timer.attach(5, getData);
  timer1Sec.attach(1, Proc1Sec);
  timer1Min.attach(60, Proc1Min);
}



bool first = true;
bool try_again_hour = false;
bool try_again_day = false;
int warmup = 5;
float f1, f2, f3;
float ds_temp = -100;
float fakef1, fakef2, fakef3;
unsigned long f1f2f3time;
unsigned long nowtime;
long loopcount = 0;

void loop() {
  // put your main code here, to run repeatedly:
  webSocket.loop();
  server.handleClient();
  loopcount++;

  delay(50);
  bme.read(pres, temp, hum, tempUnit, presUnit);
  nowtime = time(nullptr);
  f1f2f3time = 60 * (int) ((nowtime / 60 ) + 0.5);

  f1 = temp;
  f2 = pres / 1000;  // from 88000 kpa to 88 mpa to put on same scale as humidity
  f3 = hum;

  /*
        if (loopcount % 100 == 0) {
            long star = millis();
            sensors.requestTemperatures();
            Serial.println(millis()-star);
            ds_temp = sensors.getTempC(DS18B20[0]);
            Serial.println(millis()-star);
            f2 = ds_temp;
        }

        if (isnan(ds_temp)) {
          ds_temp = 0;
        } else if (ds_temp < -99){
            ds_temp = 0;
        }
  */



  if (fake > 0 ) {
    f1f2f3time = fake;
    float mod =  1.0 ; // + ( abs((counter % 360) - 180)) / 360.0;
    f1 = fakef1 * mod;
    f2 = fakef2 * mod;
    f3 = fakef3 * mod;
  }

  if (get_data_live && get_data) {

    String json = "{\"type\":\"live\",\"value\":{\"x\":";
    json += f1;
    json += ",\"x2\":";
    json += f2;
    json += ",\"x3\":";
    json += f3;
    json += ",\"y\":";             // arduino gives seconds since 1970
    json += boottime ;             // but javascript time needs milli-seconds since 1970
    json += ",\"y2\":";            // and we need a long long int to send that
    json += millis() ;             // which doesn't seem to work nicely
    json += "}}";                  // so send the seconds at boot, and the milli-seconds since boot
    //Serial.printf("tempnum %d, bootime %d, nowtime %d, millis %d\n", tempnum, boottime,nowtime,millis());
    webSocket.broadcastTXT(json.c_str(), json.length());

    //Serial.println(json.c_str());
    //Serial.println(json.length());

    get_data = false;
  }


  if (get_data_1Hour) {

    int len = (Buf_In_5_Min - Buf_Out_5_Min + Max_5_Min ) % Max_5_Min;
    if (len < 12) {
      //Serial.println("Not enogh data for a 1 Hour average");
      try_again_hour = true;
    } else {
      try_again_hour = false;

      if ( ((Buf_In_1_Hour + 1) % Max_1_Hour) == Buf_Out_1_Hour) {
        Buf_Out_1_Hour = (Buf_Out_1_Hour + 1) % Max_1_Hour;
      }

      Buf_In_1_Hour = (Buf_In_1_Hour + 1) % Max_1_Hour;

      float sumf1 = 0;
      float sumf2 = 0;
      float sumf3 = 0;

      for (int i = 0; i < 12; i++) {
        int ind = (Buf_In_5_Min - i + Max_5_Min ) % Max_5_Min;
        sumf1 += Buf_5_Min[ind].f1;
        sumf2 += Buf_5_Min[ind].f2;
        sumf3 += Buf_5_Min[ind].f3;
      }

      Buf_1_Hour[Buf_In_1_Hour].f1 = sumf1 / 12;
      Buf_1_Hour[Buf_In_1_Hour].f2 = sumf2 / 12;
      Buf_1_Hour[Buf_In_1_Hour].f3 = sumf3 / 12;
      Buf_1_Hour[Buf_In_1_Hour].t = f1f2f3time;


      Serial.printf("1Hour Index, In %d, Out %d, f1 = %f\n", Buf_In_1_Hour, Buf_Out_1_Hour, f1);
    }
    get_data_1Hour = false;
  }


  if (get_data_1Day) {

    int len = (Buf_In_1_Hour - Buf_Out_1_Hour + Max_1_Hour ) % Max_1_Hour;
    if (len < 24) {
      Serial.println("Tried to save 1 day, but not enough data, try again tomorrow");
      try_again_day = true;
    } else {
      try_again_day = false;
      if ( ((Buf_In_1_Day + 1) % Max_1_Day) == Buf_Out_1_Day) {
        Buf_Out_1_Day = (Buf_Out_1_Day + 1) % Max_1_Day;
      }

      Buf_In_1_Day = (Buf_In_1_Day + 1) % Max_1_Day;

      float sumf1 = 0;
      float sumf2 = 0;
      float sumf3 = 0;

      for (int i =  0; i < 24; i++) {
        int ind = (Buf_In_1_Hour - i + Max_1_Hour ) % Max_1_Hour;
        sumf1 += Buf_1_Hour[ind].f1;
        sumf2 += Buf_1_Hour[ind].f2;
        sumf3 += Buf_1_Hour[ind].f3;
      }

      Buf_1_Day[Buf_In_1_Day].f1 = sumf1 / 24;
      Buf_1_Day[Buf_In_1_Day].f2 = sumf2 / 24;
      Buf_1_Day[Buf_In_1_Day].f3 = sumf3 / 24;
      Buf_1_Day[Buf_In_1_Day].t = f1f2f3time;

      Serial.printf("1Day Index, In %d, Out %d, f1 = %f\n", Buf_In_1_Day, Buf_Out_1_Day, Buf_1_Day[Buf_In_1_Day].f1);

    }
    get_data_1Day = false;
  }

  if (do_a_backup) {
    do_a_backup = false;
    WriteFile2();
  }


} // end of loop

void getData() {
  get_data = true;
}


void Proc1Sec() {

  //Serial.printf("Bef 1Sec Index, In %d, Out %d, f1 = %f\n",Buf_In_1_Sec,Buf_Out_1_Sec, f1);
  if ( ((Buf_In_1_Sec + 1) % Max_1_Sec) == Buf_Out_1_Sec) {
    Buf_Out_1_Sec = (Buf_Out_1_Sec + 1) % Max_1_Sec;
  }
  Buf_In_1_Sec = (Buf_In_1_Sec + 1) % Max_1_Sec;
  Buf_1_Sec[Buf_In_1_Sec].f1 = f1;
  Buf_1_Sec[Buf_In_1_Sec].f2 = f2;
  Buf_1_Sec[Buf_In_1_Sec].f3 = f3;
  Buf_1_Sec[Buf_In_1_Sec].t = boottime + millis() / 1000; //nowtime;   // use nowtime that has not been rounded to the minute

  //Serial.printf("Aft 1Sec Index, In %d, Out %d, f1 = %f\n",Buf_In_1_Sec,Buf_Out_1_Sec, f1);
  get_data_1Sec = false;


  /*
    if (warmup < 0 ) {
    get_data_1Sec = true;
    } else if (warmup == 0 ){
    Proc1Min();
    warmup--;
    } else {
    warmup--;  }
  */

}

void Proc1Min() {


  if ( ((Buf_In_1_Min + 1) % Max_1_Min) == Buf_Out_1_Min) {
    Buf_Out_1_Min = (Buf_Out_1_Min + 1) % Max_1_Min;
  }
  Buf_In_1_Min = (Buf_In_1_Min + 1) % Max_1_Min;
  Buf_1_Min[Buf_In_1_Min].f1 = f1;
  Buf_1_Min[Buf_In_1_Min].f2 = f2;
  Buf_1_Min[Buf_In_1_Min].f3 = f3;
  Buf_1_Min[Buf_In_1_Min].t = f1f2f3time;


  //Serial.printf("1Min Index, In %d, Out %d, f1 = %f\n", Buf_In_1_Min, Buf_Out_1_Min, f1);
  //get_data_1Min = false;


  //get_data_1Min = true;
  
  unsigned long rightnow = time(nullptr);
  unsigned long rightnowmin = rightnow -  (rightnow % 60 );
  if (fake > 0){
    rightnow = fake;
    rightnowmin = rightnow -  (rightnow % 60 );
  }  
  int xmin = (rightnow % 300 ) / 60;
  int xhour = (rightnow % 3600 ) / 60;
  int xday = (rightnow % (1440 * 60)) / 60;

  //Serial.printf("Now %d, NowMin %d, Min %d, Hour %d, Day %d\n",rightnow,rightnowmin,xmin,xhour,xday);

  if (xmin == 0) {
    //get_data_5Min = true;
    if ( ((Buf_In_5_Min + 1) % Max_5_Min) == Buf_Out_5_Min) {
      Buf_Out_5_Min = (Buf_Out_5_Min + 1) % Max_5_Min;
    }
    Buf_In_5_Min = (Buf_In_5_Min + 1) % Max_5_Min;
    Buf_5_Min[Buf_In_5_Min].f1 = f1;
    Buf_5_Min[Buf_In_5_Min].f2 = f2;
    Buf_5_Min[Buf_In_5_Min].f3 = f3;
    Buf_5_Min[Buf_In_5_Min].t = f1f2f3time;
  }

  if (xhour == 0 || try_again_hour) {
    get_data_1Hour = true;

    if (xday == 0 || try_again_day) get_data_1Day = true;
  }


  /*
    if (counter % 5 == FiveMin) {
      get_data_5Min = true;

      if (counter % 60 == OneHour || try_again_hour) {
        get_data_1Hour = true;
        if (counter % 1440 == OneDay || try_again_day) get_data_1Day = true;
      }
    }
  */


  if (fake == 0 && counter % 240 == 120) do_a_backup = true; //  WriteFile2();    // every 4 hours - not during fake

    if (fake >  0) {
      if (fake < time(nullptr)) {
        fake = fake + 60;
      } else {
        Serial.println("Done making fake data.");
        fake = 0;
        timer1Min.detach();
        timer1Min.attach(60, Proc1Min);
      }
  }

  counter++;
}

void sendSeconds() {
  int len = (Buf_In_1_Sec - Buf_Out_1_Sec + Max_1_Sec ) % Max_1_Sec;

  String json = "{\"type\":\"seconds\",  \"myLabel\":\" 180 Seconds - 3 Minutes\",  \"value\":[";

  for (int i = 1 ; i < len; i++) {
    json += "{\"x\":";
    json += Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f1;

    json += ",\"x2\":";
    json += Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f2;
    json += ",\"x3\":";
    json += Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f3;

    json += ",\"y\":";
    json += Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].t;
    json += "},";
  }
  json += "{\"x\":";
  json += Buf_1_Sec[(Buf_Out_1_Sec + len) % Max_1_Sec].f1;
  json += ",\"x2\":";
  json += Buf_1_Sec[(Buf_Out_1_Sec + len) % Max_1_Sec].f2;
  json += ",\"x3\":";
  json += Buf_1_Sec[(Buf_Out_1_Sec + len) % Max_1_Sec].f3;

  json += ",\"y\":";
  json += Buf_1_Sec[(Buf_Out_1_Sec + len) % Max_1_Sec].t;
  json += "}";
  json += "]}";

  //Serial.println(json.c_str());  Serial.println(json.length());
  webSocket.broadcastTXT(json.c_str(), json.length());
}
void sendMinutes() {
  int len = (Buf_In_1_Min - Buf_Out_1_Min + Max_1_Min ) % Max_1_Min;

  String json = "{\"type\":\"minutes\",  \"myLabel\":\" 180 Minutes - 3 Hours\",  \"value\":[";

  for (int i = 1 ; i < len; i++) {
    json += "{\"x\":";
    json += Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f1;

    json += ",\"x2\":";
    json += Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f2;
    json += ",\"x3\":";
    json += Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f3;

    json += ",\"y\":";
    json += Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].t;
    json += "},";
  }
  json += "{\"x\":";
  json += Buf_1_Min[(Buf_Out_1_Min + len) % Max_1_Min].f1;
  json += ",\"x2\":";
  json += Buf_1_Min[(Buf_Out_1_Min + len) % Max_1_Min].f2;
  json += ",\"x3\":";
  json += Buf_1_Min[(Buf_Out_1_Min + len) % Max_1_Min].f3;

  json += ",\"y\":";
  json += Buf_1_Min[(Buf_Out_1_Min + len) % Max_1_Min].t;
  json += "}";
  json += "]}";

  //Serial.println(json.c_str());  Serial.println(json.length());
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void send5Minutes() {
  int len = (Buf_In_5_Min - Buf_Out_5_Min + Max_5_Min ) % Max_5_Min;

  String json = "{\"type\":\"5minutes\",  \"myLabel\":\" 288 x 5Min - 1 Day\",  \"value\":[";

  for (int i = 1 ; i < len; i++) {
    json += "{\"x\":";
    json += Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f1;

    json += ",\"x2\":";
    json += Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f2;
    json += ",\"x3\":";
    json += Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f3;

    json += ",\"y\":";
    json += Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].t;
    json += "},";
  }
  json += "{\"x\":";
  json += Buf_5_Min[(Buf_Out_5_Min + len) % Max_5_Min].f1;
  json += ",\"x2\":";
  json += Buf_5_Min[(Buf_Out_5_Min + len) % Max_5_Min].f2;
  json += ",\"x3\":";
  json += Buf_5_Min[(Buf_Out_5_Min + len) % Max_5_Min].f3;

  json += ",\"y\":";
  json += Buf_5_Min[(Buf_Out_5_Min + len) % Max_5_Min].t;
  json += "}";
  json += "]}";

  //Serial.println(json.c_str());  Serial.println(json.length());
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void sendHours() {
  int len = (Buf_In_1_Hour - Buf_Out_1_Hour + Max_1_Hour ) % Max_1_Hour;
  String json;
  if (len == 0) {
    json = "{\"type\":\"hours\",  \"myLabel\":\" no hour data yet\",  \"value\":[]}";
  } else {

    json = "{\"type\":\"hours\",  \"myLabel\":\" 240 Hours - 10 Days\",  \"value\":[";

    for (int i = 1 ; i < len; i++) {
      json += "{\"x\":";
      json += Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f1;

      json += ",\"x2\":";
      json += Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f2;
      json += ",\"x3\":";
      json += Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f3;

      json += ",\"y\":";
      json += Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].t;
      json += "},";
    }
    json += "{\"x\":";
    json += Buf_1_Hour[(Buf_Out_1_Hour + len) % Max_1_Hour].f1;
    json += ",\"x2\":";
    json += Buf_1_Hour[(Buf_Out_1_Hour + len) % Max_1_Hour].f2;
    json += ",\"x3\":";
    json += Buf_1_Hour[(Buf_Out_1_Hour + len) % Max_1_Hour].f3;

    json += ",\"y\":";
    json += Buf_1_Hour[(Buf_Out_1_Hour + len) % Max_1_Hour].t;
    json += "}";
    json += "]}";
  }
  //Serial.println(json.c_str());  Serial.println(json.length());
  webSocket.broadcastTXT(json.c_str(), json.length());

}

void sendDays() {
  int len = (Buf_In_1_Day - Buf_Out_1_Day + Max_1_Day ) % Max_1_Day;
  String json;
  if (len == 0) {
    json = "{\"type\":\"days\",  \"myLabel\":\" no day data yet\",  \"value\":[]}";
  } else {

    json = "{\"type\":\"days\",  \"myLabel\":\" 180 Days - 6 Months\",  \"value\":[";

    for (int i = 1 ; i < len; i++) {
      json += "{\"x\":";
      json += Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f1;

      json += ",\"x2\":";
      json += Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f2;
      json += ",\"x3\":";
      json += Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f3;

      json += ",\"y\":";
      json += Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].t;
      json += "},";
    }
    json += "{\"x\":";
    json += Buf_1_Day[(Buf_Out_1_Day + len) % Max_1_Day].f1;
    json += ",\"x2\":";
    json += Buf_1_Day[(Buf_Out_1_Day + len) % Max_1_Day].f2;
    json += ",\"x3\":";
    json += Buf_1_Day[(Buf_Out_1_Day + len) % Max_1_Day].f3;

    json += ",\"y\":";
    json += Buf_1_Day[(Buf_Out_1_Day + len) % Max_1_Day].t;
    json += "}";
    json += "]}";
  }
  //Serial.println(json.c_str());  Serial.println(json.length());
  webSocket.broadcastTXT(json.c_str(), json.length());

}

void ReadFile2() {

  float myfloat1, myfloat2, myfloat3;
  unsigned long mytime;
  File file;
  int i;
  /*
    File file = LittleFS.open("/seconds2.txt", "r");
    if (!file) {
      Serial.println("Failed to open file for reading");
      return;
    }
    Serial.print("Reading Seconds Buffer ...");

    for ( i = 0; i < Max_1_Sec; i++) {
      if (!file.available()) break;
      myfloat1 = file.parseFloat();
      myfloat2 = file.parseFloat();
      myfloat3 = file.parseFloat();
      mytime = file.parseInt();

      //Serial.print(i);  Serial.print(" ");  Serial.println(myfloat);

      Buf_In_1_Sec = (Buf_In_1_Sec + 1) % Max_1_Sec;
      Buf_1_Sec[Buf_In_1_Sec].f1 = myfloat1;
      Buf_1_Sec[Buf_In_1_Sec].f2 = myfloat2;
      Buf_1_Sec[Buf_In_1_Sec].f3 = myfloat3;
      Buf_1_Sec[Buf_In_1_Sec].t = mytime;
    }

    Serial.print("Total Samples : ");
    Serial.println(i - 1);
    file.close();
    if ( i == 0) {
    } else {
      Buf_In_1_Sec = (Buf_In_1_Sec - 1 + Max_1_Sec) % Max_1_Sec ;
    }
    Serial.println(" done.");
  */

  file = LittleFS.open("/minutes2.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Reading Minutes Buffer ...");

  for ( i = 0; i < Max_1_Min; i++) {
    if (!file.available()) break;
    myfloat1 = file.parseFloat();
    myfloat2 = file.parseFloat();
    myfloat3 = file.parseFloat();
    mytime = file.parseInt();

    //myfloat = file.parseFloat();
    //Serial.print(i);  Serial.print(" ");  Serial.println(myfloat);

    Buf_In_1_Min = (Buf_In_1_Min + 1) % Max_1_Min;
    Buf_1_Min[Buf_In_1_Min].f1 = myfloat1;
    Buf_1_Min[Buf_In_1_Min].f2 = myfloat2;
    Buf_1_Min[Buf_In_1_Min].f3 = myfloat3;
    Buf_1_Min[Buf_In_1_Min].t = mytime;
  }

  Serial.print("Total Samples : ");
  Serial.println(i - 1);
  file.close();
  if (i == 0) {
  } else {
    Buf_In_1_Min = (Buf_In_1_Min - 1 + Max_1_Min) % Max_1_Min ;
  }
  Serial.println(" done.");


  file = LittleFS.open("/5minutes2.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Reading 5Minutes Buffer ...");

  for ( i = 0; i < Max_5_Min; i++) {
    if (!file.available()) break;
    //myfloat = file.parseFloat();
    myfloat1 = file.parseFloat();
    myfloat2 = file.parseFloat();
    myfloat3 = file.parseFloat();
    mytime = file.parseInt();

    //Serial.print(i);  Serial.print(" ");  Serial.println(myfloat);

    Buf_In_5_Min = (Buf_In_5_Min + 1) % Max_5_Min;
    Buf_5_Min[Buf_In_5_Min].f1 = myfloat1;
    Buf_5_Min[Buf_In_5_Min].f2 = myfloat2;
    Buf_5_Min[Buf_In_5_Min].f3 = myfloat3;
    Buf_5_Min[Buf_In_5_Min].t = mytime;
  }

  Serial.print("Total Samples : ");
  Serial.println(i - 1);
  file.close();
  if (i == 0 ) {
  } else {
    Buf_In_5_Min = (Buf_In_5_Min - 1 + Max_5_Min) % Max_5_Min ;
  }
  Serial.println(" done.");


  file = LittleFS.open("/hours2.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Reading Hours Buffer ...");

  for ( i = 0; i < Max_1_Hour; i++) {
    if (!file.available()) break;
    //myfloat = file.parseFloat();
    myfloat1 = file.parseFloat();
    myfloat2 = file.parseFloat();
    myfloat3 = file.parseFloat();
    mytime = file.parseInt();

    //Serial.print(i);  Serial.print(" ");  Serial.println(myfloat1); Serial.println(myfloat2); Serial.println(myfloat3);

    Buf_In_1_Hour = (Buf_In_1_Hour + 1) % Max_1_Hour;
    Buf_1_Hour[Buf_In_1_Hour].f1 = myfloat1;
    Buf_1_Hour[Buf_In_1_Hour].f2 = myfloat2;
    Buf_1_Hour[Buf_In_1_Hour].f3 = myfloat3;
    Buf_1_Hour[Buf_In_1_Hour].t = mytime;
  }

  Serial.print("Total Samples : ");
  Serial.println(i - 1);
  file.close();

  if (i == 0 ) {
  } else {
    Buf_In_1_Hour = (Buf_In_1_Hour - 1 + Max_1_Hour) % Max_1_Hour ;
  }

  Serial.println(" done.");

  file = LittleFS.open("/days2.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Reading Days Buffer ...");

  for ( i = 0; i < Max_1_Day; i++) {
    if (!file.available()) break;
    //myfloat = file.parseFloat();
    myfloat1 = file.parseFloat();
    myfloat2 = file.parseFloat();
    myfloat3 = file.parseFloat();
    mytime = file.parseInt();

    //Serial.print(i);  Serial.print(" ");  Serial.println(myfloat1); Serial.println(myfloat2); Serial.println(myfloat3);

    Buf_In_1_Day = (Buf_In_1_Day + 1) % Max_1_Day;
    Buf_1_Day[Buf_In_1_Day].f1 = myfloat1;
    Buf_1_Day[Buf_In_1_Day].f2 = myfloat2;
    Buf_1_Day[Buf_In_1_Day].f3 = myfloat3;
    Buf_1_Day[Buf_In_1_Day].t = mytime;
  }

  Serial.print("Total Samples : ");
  Serial.println(i - 1);
  file.close();

  if (i == 0 ) {
  } else {
    Buf_In_1_Day = (Buf_In_1_Day - 1 + Max_1_Day) % Max_1_Day ;
  }

  Serial.println(" done.");

}


void WriteFile2() {

  File file;
  int len;

  /*
    Serial.println("Writing seconds ... ");
    file = LittleFS.open("/seconds2.txt", "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    len = (Buf_In_1_Sec - Buf_Out_1_Sec + Max_1_Sec ) % Max_1_Sec;

    for (int i = 1; i <= len; i++) {
      //Serial.print(i);  Serial.print(" "); Serial.println(Buf_1_Sec[(Buf_Out_1_Sec+i) % Max_1_Sec]);
      file.printf("%f,%f,%f,%lu\n",Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f1,
                                   Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f2,
                                   Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].f3,
                                   Buf_1_Sec[(Buf_Out_1_Sec + i) % Max_1_Sec].t);
    }
    file.close();
    Serial.println(" done.");
  */

  Serial.println("Writing minutes ... ");
  file = LittleFS.open("/minutes2.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  len = (Buf_In_1_Min - Buf_Out_1_Min + Max_1_Min ) % Max_1_Min;

  for (int i = 1; i <= len; i++) {
    //Serial.print(i);  Serial.print(" "); Serial.println(Buf_1_Min[(Buf_Out_1_Min+i) % Max_1_Min]);
    file.printf("%f,%f,%f,%lu\n", Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f1,
                Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f2,
                Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].f3,
                Buf_1_Min[(Buf_Out_1_Min + i) % Max_1_Min].t);
  }
  file.close();
  Serial.println(" done.");


  Serial.println("Writing 5minutes ... ");
  file = LittleFS.open("/5minutes2.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  len = (Buf_In_5_Min - Buf_Out_5_Min + Max_5_Min ) % Max_5_Min;

  for (int i = 1; i <= len; i++) {
    //Serial.print(i);  Serial.print(" "); Serial.println(Buf_5_Min[(Buf_Out_5_Min+i) % Max_5_Min]);
    file.printf("%f,%f,%f,%lu\n", Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f1,
                Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f2,
                Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].f3,
                Buf_5_Min[(Buf_Out_5_Min + i) % Max_5_Min].t);
  }
  file.close();
  Serial.println(" done.");


  Serial.println("Writing hours ... ");
  file = LittleFS.open("/hours2.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  len = (Buf_In_1_Hour - Buf_Out_1_Hour + Max_1_Hour ) % Max_1_Hour;

  for (int i = 1; i <= len; i++) {
    //Serial.print(i);  Serial.print(" "); Serial.println(Buf_1_Hour[(Buf_Out_1_Hour+i) % Max_1_Hour]);
    file.printf("%f,%f,%f,%lu\n", Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f1,
                Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f2,
                Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].f3,
                Buf_1_Hour[(Buf_Out_1_Hour + i) % Max_1_Hour].t);
  }
  file.close();
  Serial.println(" done.");


  Serial.println("Writing days ... ");
  file = LittleFS.open("/days2.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  len = (Buf_In_1_Day - Buf_Out_1_Day + Max_1_Day ) % Max_1_Day;

  for (int i = 1; i <= len; i++) {
    //Serial.print(i);  Serial.print(" "); Serial.println(Buf_1_Day[(Buf_Out_1_Day+i) % Max_1_Day]);
    file.printf("%f,%f,%f,%lu\n", Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f1,
                Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f2,
                Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].f3,
                Buf_1_Day[(Buf_Out_1_Day + i) % Max_1_Day].t);
  }
  file.close();
  Serial.println(" done.");

}



void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Do something with the data from the client
  //Serial.printf("Get Event  %d,%d\n",num,length);
  if (type == WStype_TEXT) {
    get_data_live = false;
    timer.detach();
    timer.attach(0.5 , getData);
    if (payload[0] == 's') {
      Serial.println("sending seconds");
      sendSeconds();

    } else if (payload[0] == 'm') {
      Serial.println("sending minutes");
      sendMinutes();
      //sendMinutes2();


    } else if (payload[0] == 'f') {
      Serial.println("sending 5minutes");
      send5Minutes();

    } else if (payload[0] == 'h') {
      Serial.println("sending hours");
      sendHours();

    } else if (payload[0] == 'y') {
      Serial.println("sending days");
      sendDays();

    } else if (payload[0] == 'l') {
      Serial.println("got live command");

      String json = "{\"type\":\"LIVE\",  \"myLabel\":\"Temperature (C) - 100 Live Samples\", \"value\":";
      json += 0;
      json += "}";

      webSocket.broadcastTXT(json.c_str(), json.length());

      get_data_live = true;

    } else if (payload[0] == 'w') {

      WriteFile2();

    } else if (payload[0] == 'X') {

      fake = time(nullptr) - 60 * 60 * 24 * 3;
      timer1Min.detach();
      timer1Min.attach(0.02, Proc1Min);
      fakef1 = f1;
      fakef2 = f2;
      fakef3 = f3;

      for (int i = 0; i < Max_1_Sec; i++) {
        Buf_1_Sec[i].f1 = f1;
        Buf_1_Sec[i].f2 = f2;
        Buf_1_Sec[i].f3 = f3;
      }
      for (int i = 0; i < Max_1_Min; i++) {
        Buf_1_Min[i].f1 = f1;
        Buf_1_Min[i].f2 = f2;
        Buf_1_Min[i].f3 = f3;
      }
      for (int i = 0; i < Max_5_Min; i++) {
        Buf_5_Min[i].f1 = f1;
        Buf_5_Min[i].f2 = f2;
        Buf_5_Min[i].f3 = f3;
      }
      for (int i = 0; i < Max_1_Hour; i++) {
        Buf_1_Hour[i].f1 = f1;
        Buf_1_Hour[i].f2 = f2;
        Buf_1_Hour[i].f3 = f3;
      }
      for (int i = 0; i < Max_1_Day; i++) {
        Buf_1_Day[i].f1 = f1;
        Buf_1_Day[i].f2 = f2;
        Buf_1_Day[i].f3 = f3;
      }

      Serial.println("Start making fake data ...");
    } else if (payload[0] == 'e') {
      Buf_In_1_Sec = 0;
      Buf_Out_1_Sec = 0;
      Buf_In_1_Min = 0;
      Buf_Out_1_Min = 0;
      Buf_In_5_Min = 0;
      Buf_Out_5_Min = 0;
      Buf_In_1_Hour = 0;
      Buf_Out_1_Hour = 0;
      Buf_In_1_Day = 0;
      Buf_Out_1_Day = 0;

    } else if (payload[0] == 'd') {


      LittleFS.remove("/seconds2.txt");
      LittleFS.remove("/minutes2.txt");
      LittleFS.remove("/5minutes2.txt");
      LittleFS.remove("/hours2.txt");
      LittleFS.remove("/dayss2.txt");

    } else {
      get_data_live = true;
      float dataRate = (float) atof((const char *) &payload[0]);


      timer.detach();
      timer.attach(dataRate / 10 , getData);
    }
  }
}
