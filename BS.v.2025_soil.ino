//Инициализация библиотек
#include <SPI.h> //библиотека для работы с SPI
#include <LoRa.h> //библиотека для работы с LoRa
#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiMulti.h>
  #include <HTTPClient.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
  #include <ESP8266HTTPClient.h>
  #include <ESP8266mDNS.h>  
  #include <ESPAsyncTCP.h>
#endif
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h> //библиотека для работы с bmp280
#include "Adafruit_HTU21DF.h" //библиотека для работы с htu21
//#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
//#include <AsyncElegantOTA.h>
#include <Wire.h> //библиотека i2c
#include <iarduino_OLED.h>  
iarduino_OLED myOLED(0x3C); 

//Инициализация пинов и параметры входа в сеть
#define NSS 5 //пин Chip select (slave select) lora   esp32  5     es8266 15
#define RST 14 //пин сброса lora   esp32 14  es8266 2
#define DIO0 13 // цифровой пин для lora    esp32  13    es8266 16
#define SDA 21
#define SCL 22

#ifndef STASSID
#define STASSID "Kaer Morhen" //название сети Wi-Fi   RT-GPON-5EA1   HUAWEI_D1D3
#define STAPSK "mustlightvk" //пароль от сети Wi-Fi   rb7AZEGiBR     AJ2Y491QNQR
#endif

//Инициализация порта для работы с локальным монитором
AsyncWebServer server(80);
WiFiServer wifiserver(80); 
AsyncEventSource events("/events");

Adafruit_BMP280 bmp; //объявляем класс для bmp280
Adafruit_HTU21DF htu = Adafruit_HTU21DF(); //объявляем класс для htu21

//Инициализация различных переменных
float CHR, bmpT, bmpP, htuH;
extern uint8_t SmallFontRus[]; 
int i = 0;
int BSid = 1000; //ID B, при создании карточки на сайте для БС, токен указываем от 1000 для БС. Следующий можно 1001 указать.
uint32_t id;
uint8_t n;
int rssi, packetSize;
static uint8_t mydata[400];
uint8_t txbuf[1];
static uint8_t PROTOCOL_1 = 0x7F; //номер первого протокола
char buffer1[500];
char buffer2[500];
char buffer3[200];
const char* ssid = STASSID;
const char* password = STAPSK;
uint32_t ID;

// HTML, CSS, JavaScript код для веб-интерфейса (монитора).
const char* htmlContent = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Local web-interface</title>
  <style>
    body {
      margin: 0;
      padding: 0;

      font-family: Arial, sans-serif;
      text-align: center;

      background-color: #f5f5f5;
    }

    * {
      box-sizing: border-box;
    }

    .container {
      height: 90vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
    }

    h1 {
      margin: 15px 0; 
      padding: 0;

      font-size: 24px;
      font-weight: bold;

      color: #333;
    }

    table {
      width: 90%;
      max-width: 500px;

      border-collapse: collapse;
      background-color: #f5f5f5;
      border-radius: 10px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }

    th, td {
      padding: 15px;

      text-align: center;
      font-size: 16px;
      
      color: #777;
    }

    th {
      border-top-left-radius: 10px;
      border-top-right-radius: 10px;
      font-weight: 500;
      background-color: #4682B4;
      color: #fff;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }
    .btn_download{
        margin-top: 15px;
        padding: 10px 20px; 
        background-color: #4682B4; 
        color: #fff; 
        border: none; 
        border-radius: 5px; 
        cursor: pointer;
    }
    .btn_download:active {
    background-color: #1e6ab0;
    }
    .developed{
      font-size: 16px;
      font-weight: 400;
      
      color: #777;
    }
  </style>
</head>
<body>
  <section class="table-section">
    <div class="container">
      <h1>Local web-interface</h1>
      <table class="table">
        <tr>
          <th>ID</th>
          <th>Air Temperature, °C</th>
          <th>Atm. Pressure, Pa</th>
          <th>Humidity, %</th>
          <th>Soil Temperature, °C</th>
          <th>RSSI</th>
          <th>Voltage, V</th>
          <th>Charge, %</th>
          <th>CO2, ppm</th>
          <th>BS Air Temperature, °C</th>
          <th>BS Atm. Pressure, Pa</th>
          <th>BS Humidity, %</th>
        </tr>
        <tr>
          <td id="measurement1">-</td>
          <td id="measurement2">-</td>
          <td id="measurement3">-</td>
          <td id="measurement4">-</td>
          <td id="measurement5">-</td>
          <td id="measurement6">-</td>
          <td id="measurement7">-</td>
          <td id="measurement8">-</td>
          <td id="measurement9">-</td>
          <td id="measurement10">-</td>
          <td id="measurement11">-</td>
          <td id="measurement12">-</td>
        </tr>
      </table>
      <button class="btn_download" id="downloadButton">Download</button>
    </div>
  </section>
  <footer>
    <p class="developed">Developed by V.A. Kazanin</p>
  </footer>

  <script>
    const measurementsArray = []; // Массив для хранения измерений
  
    // Обработчик событий для Server-Sent Events (SSE)
    const eventSource = new EventSource('/events');
    eventSource.onmessage = function (event) {
      const data = JSON.parse(event.data);
      measurementsArray.push(data);
  
      // Обновить значения в таблице
      document.getElementById("measurement1").textContent = data.measurement1;
      document.getElementById("measurement2").textContent = data.measurement2;
      document.getElementById("measurement3").textContent = data.measurement3;
      document.getElementById("measurement4").textContent = data.measurement4;
      document.getElementById("measurement5").textContent = data.measurement5;
      document.getElementById("measurement6").textContent = data.measurement6;
      document.getElementById("measurement7").textContent = data.measurement7;
      document.getElementById("measurement8").textContent = data.measurement8;
      document.getElementById("measurement9").textContent = data.measurement9;
      document.getElementById("measurement10").textContent = data.measurement10;
      document.getElementById("measurement11").textContent = data.measurement11;
      document.getElementById("measurement12").textContent = data.measurement12;
    };
  
    // Обработчик события для кнопки "Скачать данные"
    const downloadButton = document.getElementById("downloadButton");
    downloadButton.addEventListener("click", () => {
      // Создать CSV-подобную строку для экспорта
      let csvData = "ID, Air Temperature, Atm. Pressure, Humidity, Soil Temperature, RSSI, Voltage, Charge, CO2, BS Air Temperature, BS Atm. Pressure, BS Humidity,\n";
      measurementsArray.forEach((measurement) => {
        csvData += `${measurement.measurement1}, ${measurement.measurement2}, ${measurement.measurement3}, ${measurement.measurement4}, ${measurement.measurement5}, ${measurement.measurement6}, ${measurement.measurement7}, ${measurement.measurement8}, ${measurement.measurement9},${measurement.measurement10}, ${measurement.measurement11}, ${measurement.measurement12},\n`;
      });
  
      // Создать временный элемент для скачивания
      const blob = new Blob([csvData], { type: "text/csv" });
      const url = window.URL.createObjectURL(blob);
  
      const a = document.createElement("a");
      a.style.display = "none";
      a.href = url;
      a.download = "measurements.csv";
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
    });
  </script>

</body>
</html>
)HTML";

//Далее прописана работа кода
void setup() {
  Serial.begin(115200); //инициализация Serial, i2c, ЖК-дисплея
  Wire.begin(SDA, SCL);

  myOLED.begin();                                                    // Инициируем работу с дисплеем.
  myOLED.setFont(SmallFontRus);
  myOLED.setCoding(TXT_UTF8);  
  myOLED.print("РЭКС-Т Баз. станция", 5, 7);

  Serial.println("РЭКС-Т Баз. станция");;

  Serial.print("Connecting to "); //подключение к сети Wi-Fi
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Wi-Fi Ok!");
  myOLED.print("Wi-Fi     готов", 0, 20);
  
  LoRa.setPins(NSS, RST, DIO0); //конфигурация lora
  if (LoRa.begin(868E6)) 
  {
    Serial.println("LoRa Ok!");
    myOLED.print("Приёмник готов", 0, 30);

  } else
  {
    Serial.println("LoRa not Ok!");
    myOLED.print("Трансивер NOK", 0, 30);

  }
  
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(1);
  for (i = 0; i <= sizeof(mydata) - 1; i++) mydata[i] = 0x00;
  delay(1000);
  myOLED.clrScr();
  myOLED.print("Консоль логов:", 25, 7);
  myOLED.print("IP адрес:", 5, 30);
  myOLED.print(WiFi.localIP().toString(), 5, 40);
  if (WiFi.status() == WL_CONNECTED) myOLED.print("W", 120, 7);  

//-------Ниже обязательный код для прошивки "по воздуху"-------
 // AsyncElegantOTA.begin(&server);    // Старт ElegantOTA
 // server.addHandler(&events);
  //server.begin();
  //Serial.println("HTTP server started");

  // Маршрут для обработки корневой страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", htmlContent);
  });

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("HTTP/1.1 200 OK");
    client->send("Content-Type: text/event-stream");
    client->send("Cache-Control: no-cache");
    client->send("");
  });

//  // ArduinoOTA.onStart([]() 
//  // {
//    // String type;
//     //if (ArduinoOTA.getCommand() == U_FLASH)
//       type = "sketch";
//     else // U_SPIFFS
//       type = "filesystem";

//     // Примечание:  NOTE: при обновлении SPIFFS это место для размонтирования SPIFFS с помощью SPIFFS.end()
//     Serial.println("Start updating " + type);
//   });
//   ArduinoOTA.onEnd([]() 
//   {
//     Serial.println("\nEnd");
//   });
//   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
//   {
//     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//   });
//   ArduinoOTA.onError([](ota_error_t error) 
//   {
//     Serial.printf("Error[%u]: ", error);
//     if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
//     else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//     else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//     else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//     else if (error == OTA_END_ERROR) Serial.println("End Failed");
//   });
//   ArduinoOTA.begin();
//   Serial.println("Ready");
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());
// }

void loop()
{
  // ArduinoOTA.handle();
  // AsyncElegantOTA.loop();
  
  packetSize = LoRa.parsePacket(); //прием данных от агрозонда
  if (packetSize)
  {
  i = 0;
  while (LoRa.available())
  {
    mydata[i] = LoRa.read();
    Serial.print(mydata[i], HEX);
    i++;
  }
  Serial.println();
  Serial.println(mydata[0], HEX);
  
  if (mydata[0] == 0x9B) //АРУ
  {
    rssi = LoRa.packetRssi();
    if (rssi >= -130) 
      {
        txbuf[0] = 0x9A;
        Serial.print("RSSI: ");
        Serial.println(rssi);   
        LoRa.beginPacket(); 
        LoRa.write(txbuf, 1);
        LoRa.endPacket();
        Serial.println("Отправлено");
        //отправка команды на регулировку мощности
      } else
      {
        txbuf[0] = 0x9B;
        Serial.print("RSSI_END: ");
        Serial.println(rssi);
        LoRa.beginPacket(); 
        LoRa.write(txbuf, 1);
        LoRa.endPacket();
        Serial.println("Power Ok!");
        //отправка команды, что мощность настроена
      }
  }
  else
  {
  Serial.println();
  Serial.print("PROT="); Serial.print(mydata[0], HEX);
  Serial.println();
  
  signed short convert = 0xFFFF;
  convert = mydata[11];
  convert = convert << 8;
  convert |= mydata[10];
  float temp_rtc = static_cast<float>(convert) / 100;
  
  convert = 0xFFFF;
  convert = mydata[13];
  convert = convert << 8;
  convert |= mydata[12];
  float temp_bmp = static_cast<float>(convert) / 100;
  
  convert = 0xFFFF;
  convert = mydata[15];
  convert = convert << 8;
  convert |= mydata[14];
  float press_bmp = static_cast<float>(convert) / 10;
  
  convert = 0xFFFF;
  convert = mydata[17];
  convert = convert << 8;
  convert |= mydata[16];
  float temp_htu = static_cast<float>(convert) / 100;
  
  convert = 0xFFFF;
  convert = mydata[19];
  convert = convert << 8;
  convert |= mydata[18];
  float hum_htu = static_cast<float>(convert) / 100;
  
  convert = 0xFFFF;
  convert = mydata[21];
  convert = convert << 8;
  convert |= mydata[20];
  float temp_ds18b20 = static_cast<float>(convert) / 100;
  
  convert = 0xFFFF;
  convert = mydata[23];
  convert = convert << 8;
  convert |= mydata[22];
  float voltage = static_cast<float>(convert) / 100;
  Serial.print("Напряжение АКБ: ");
  Serial.print(voltage);
  Serial.print(" В ");
  Serial.print(mydata[22], HEX);
  Serial.println(mydata[23], HEX);

  convert = 0xFFFF;
  convert = mydata[24];

  Serial.print("Заряд: ");
  Serial.print(mydata[24], DEC);
  Serial.print("% ");
  Serial.println(mydata[24], HEX);
  CHR = mydata[24], DEC;
  
  convert = 0xFFFF;
  convert = mydata[26];
  convert = convert << 8;
  convert |= mydata[25];
  float current = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[28];
    convert = convert << 8;
    convert |= mydata[27];
    float t_sens_10cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[30];
    convert = convert << 8;
    convert |= mydata[29];
    float t_sens_20cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[32];
    convert = convert << 8;
    convert |= mydata[31];
    float t_sens_30cm = static_cast<float>(convert) / 100;
    
    convert = 0xFFFF;
    convert = mydata[34];
    convert = convert << 8;
    convert |= mydata[33];
    float t_sens_40cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[36];
    convert = convert << 8;
    convert |= mydata[35];
    float t_sens_50cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[38];
    convert = convert << 8;
    convert |= mydata[37];
    float t_sens_60cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[40];
    convert = convert << 8;
    convert |= mydata[39];
    float t_sens_70cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[42];
    convert = convert << 8;
    convert |= mydata[41];
    float t_sens_80cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[44];
    convert = convert << 8;
    convert |= mydata[43];
    float t_sens_90cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[46];
    convert = convert << 8;
    convert |= mydata[45];
    float t_sens_100cm = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[49];
    convert = convert << 8;
    convert |= mydata[48];
    float co2 = static_cast<float>(convert) / 10;

    convert = 0xFFFF;
    convert = mydata[27];
    float v_hum1 = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[28];
    float v_hum2 = static_cast<float>(convert) / 100;

    convert = 0xFFFF;
    convert = mydata[29];
    float v_hum3 = static_cast<float>(convert) / 100;

  rssi = LoRa.packetRssi(); //считывание индекса силы сигнала
  float sensorT = read_bmp280_temp();
  float sensorP = read_bmp280_press();
  float sensorH = read_htu21_hum();
  
  id = *reinterpret_cast<uint32_t *>(mydata+1);

  char temp_bmpStr[10];
  char press_bmpStr[10];
  char temp_ds18b20Str[10];
  char voltageStr[10];
  char co2Str[10];
  char bmp280_tempStr[10];
  char bmp280_pressStr[10];
  char htu21_humStr[10];
  
  snprintf(temp_bmpStr, sizeof(temp_bmpStr), "%.2f", temp_bmp);
  snprintf(press_bmpStr, sizeof(press_bmpStr), "%.2f", press_bmp);
  snprintf(temp_ds18b20Str, sizeof(temp_ds18b20Str), "%.2f", temp_ds18b20);
  snprintf(voltageStr, sizeof(voltageStr), "%.2f", voltage);
  snprintf(bmp280_tempStr, sizeof(bmp280_tempStr), "%.2f", sensorT);
  snprintf(bmp280_pressStr, sizeof(bmp280_pressStr), "%.2f", sensorP);
  snprintf(htu21_humStr, sizeof(htu21_humStr), "%.2f", sensorH);

  // Выделяем целую часть чисел 
  int rssiInt = int(rssi);
  int CHRInt = int(CHR);
  int humInt = int(hum_htu);
  int co2Int = int(co2);
  int idInt = int(id);

  // Создаем JSON-документ и добавляем округленные измерения
  StaticJsonDocument<400> jsonDocument;
  jsonDocument["measurement1"] = idInt;
  jsonDocument["measurement2"] = temp_bmpStr;
  jsonDocument["measurement3"] = press_bmpStr;
  jsonDocument["measurement4"] = humInt;
  jsonDocument["measurement5"] = temp_ds18b20Str;
  jsonDocument["measurement6"] = rssiInt;
  jsonDocument["measurement7"] = voltageStr;
  jsonDocument["measurement8"] = CHRInt;
  jsonDocument["measurement9"] = co2Int;
  jsonDocument["measurement10"] = bmp280_tempStr;
  jsonDocument["measurement11"] = bmp280_pressStr;
  jsonDocument["measurement12"] = htu21_humStr;
  
  char buffer[400];
  serializeJson(jsonDocument, buffer);
  
  // Отправляем JSON через Server-Sent Events (SSE)
  events.send(buffer);

  //Формируем JSON строку. Она использовалась в УСКД и эта же строка использузется для вывода на OLED-дисплей
  memset(buffer1, '\0', 200);
    sprintf(buffer1, R"({"id":%d,"datetime":"20%02d-%02d-%02d %02d:%02d:00","temp_rtc":%.2f,"atemp":%.2f,"press_bmp":%.1f,"temp_htu":%.2f,"hum_htu":%.2f,"stemp":%.2f,"rssi":%d,"vacc":%.2f,"spc":%.2f,"charge":%d,"sta0":[%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f]})""\n",
      id, mydata[7], mydata[6], mydata[5], mydata[8], mydata[9], temp_rtc, temp_bmp, press_bmp, temp_htu, hum_htu, temp_ds18b20, rssi, voltage, current, mydata[24], t_sens_10cm, t_sens_20cm, t_sens_30cm, t_sens_40cm, t_sens_50cm, t_sens_60cm, t_sens_70cm, t_sens_80cm, t_sens_90cm, t_sens_100cm);

  //Формируем JSON строку для нового веб-интерфейса REX-T.
  //sprintf(buffer2, R"({"t":"%d","m":{"2":%.2f,"3":%.1f,"4":%.2f,"1":%.2f,"6":%d,"7":%.2f,"5":%d,"71":%.2f,"91":%.2f,"92":%.2f,"93":%.2f}})""\n",
  //           id, temp_bmp, press_bmp, hum_htu, temp_ds18b20, rssi, voltage, mydata[24], co2, v_hum3, v_hum3, v_hum3);  
  sprintf(buffer2, R"({"t":"%d","m":{"2":%.2f,"3":%.1f,"4":%.2f,"6":%d,"7":%.2f,"5":%d,"91":%.2f,"92":%.2f,"93":%.2f}})""\n",
             id, temp_bmp, press_bmp, hum_htu, rssi, voltage, mydata[24], v_hum1, v_hum2, v_hum3);
  sprintf(buffer3, R"({"t":"%d","m":{"2":%.2f,"3":%.1f,"4":%.2f}})""\n",
             BSid, sensorT, sensorP, sensorH);
  Serial.print("\n\nbuffer2: ");
  Serial.println(buffer2);
  
  HTTPClient http;
    Serial.println("ok!");
    Serial.print("request begin...\n");
    http.begin("https://infotis-official.fvds.ru/api/v1/r"); //esp32
    // http.begin(client,"https://infotis-official.fvds.ru/api/v1/r"); //esp8266 не работает. Кто сделает - тот герой!
    int httpCode = http.POST(buffer2);
    delay(2000);
    int httpCode2 = http.POST(buffer3);
    Serial.printf("request code: %d\n", httpCode);
    Serial.printf("request code: %d\n", httpCode2);
    Serial.println(http.getString());
  http.end();
  Serial.print("\n-------------------------------------\n");
  for(int i=0; i<sizeof(mydata); i++){
    Serial.print(mydata[i]);
  }
  Serial.println("");
  for(int i=0; i<sizeof(mydata); i++){
    Serial.print((char)mydata[i]);
  }
  Serial.print("\n-------------------------------------\n");
  myOLED.print("W", 120, 7);
  
  }
  memset(buffer1, '\0', 300);
  sprintf(buffer1, "%d %d:%d R %dдБ %d%s", id, mydata[8], mydata[9], rssi, mydata[24], "%");
  myOLED.print(buffer1, 0, 20+n*10);
  n++;
  if (n > 5) 
  {
    myOLED.clrScr();
    myOLED.print("Консоль логов:", 25, 7); 
    if (WiFi.status() == WL_CONNECTED) myOLED.print("W", 120, 7); else  myOLED.print("X", 120, 7);
    sprintf(buffer1, "%d %d:%d R %ddB %d%s", id, mydata[8], mydata[9], rssi, mydata[24], "%");
    myOLED.print(buffer1, 0, 20);
    
    n = 1;
  }
  }
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    myOLED.print("X", 120, 7);
    delay(500);
    myOLED.clrScr();
  }
  if (WiFi.status() == WL_CONNECTED) myOLED.print("W", 120, 7); else  myOLED.print("X", 120, 7);
  
 }

float read_bmp280_temp(void)
{
  bmp.begin(0x76);
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X1,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X1,      /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_OFF,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  bmpT = bmp.readTemperature();
  return bmpT;
}

float read_bmp280_press(void)
{
  bmpP = bmp.readPressure()/133.332;
  long int result = round(bmpP);
  return result;
}

float read_htu21_hum(void)
{
  htuH = htu.readHumidity();
  return htuH;
}
