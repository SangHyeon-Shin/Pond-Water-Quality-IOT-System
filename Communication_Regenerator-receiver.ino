#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6Dr0phLeZ"
#define BLYNK_TEMPLATE_NAME "Pond Water Quality"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#include <SoftwareSerial.h>
#include <SPI.h> 
#include <nRF24L01.h>
#include <RF24.h>
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>

SoftwareSerial EspSerial(2, 3); // RX, TX
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";
BlynkTimer timer;

struct SensorData {
    float temperature;
    float pH;
    float turbidity;
    float TDS;
};
SensorData dataReceived;

#define ESP8266_BAUD 9600

ESP8266 wifi(&EspSerial);


RF24 radio(7, 8); // SPI 버스에 nRF24L01 라디오를 설정하기 위해 CE, CSN 선언.
const byte address[6] = "00001"; //주소값을 5가지 문자열로 변경할 수 있으며, 송신기과 수신기가 동일한 주소로 해야됨.
void setup() {
  Serial.begin(9600);
  EspSerial.begin(ESP8266_BAUD);
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW); //전원공급에 관한 파워레벨을 설정합니다. 모듈 사이가 가까우면 최소로 설정합니다.

//거리가 가까운 순으로 RF24_PA_MIN / RF24_PA_LOW / RF24_PA_HIGH / RF24_PA_MAX 등으로 설정할 수 있습니다.

//높은 레벨(거리가 먼 경우)은 작동하는 동안 안정적인 전압을 가지도록 GND와 3.3V에 바이패스 커패시터 사용을 권장함
  radio.startListening(); //모듈을 수신기로 설정
  Blynk.begin(BLYNK_AUTH_TOKEN, wifi, ssid, pass);
  timer.setInterval(1000L, updateBlynk);
}

void updateBlynk() {
  if (radio.available()) {
    radio.read(&dataReceived, sizeof(SensorData));
    Blynk.virtualWrite(V0, dataReceived.temperature);
    Blynk.virtualWrite(V1, dataReceived.pH);
    Blynk.virtualWrite(V2, dataReceived.turbidity);
    Blynk.virtualWrite(V3, dataReceived.TDS);
    Serial.print(dataReceived.temperature);
    Serial.print(",\t");
    Serial.print(dataReceived.pH);
    Serial.print(",\t");
    Serial.print(dataReceived.turbidity);
    Serial.print(",\t");
    Serial.println(dataReceived.TDS);

  }
}

void loop() {
  Blynk.run();
  timer.run();
}