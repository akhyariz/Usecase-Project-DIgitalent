#include <WiFi.h> //apabila menggunakan board ESP32 maka menggunakan library WiFi.h
#include "ThingsBoard.h"
#include <ESP32Servo.h>

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define BUTTON_RED 2 // GIOP21 pin connected to button
#define BUTTON_BLUE 4
#define LED_BLUE 18
#define LED_RED 5
#define LED_OL 19
#define LED_GROW 21
#define LIGHT_SENSOR_PIN 36
#define RAINDROP_SENSOR_PIN 39
#define SERVO_PIN 15

const float gama = 0.7;
const float rl10 = 50;


#define WIFI_SSID           "Wokwi-GUEST" //deklarasi ssid wifi yang digunakan, disesuiakan dengan wifi yang digunakan
#define WIFI_PASSWORD       "" //deklarasi password wifi yang digunakan

#define TOKEN               "IF0svkxcabM4EYNB62KUx" //deklarasi token device yang digunakan untuk menguhubungkan esp32 dengan device pada thingsboard, tokennya disesuaikan dengan device yang digunakan bisa di cek langsung ketika membuka device pada thingsboard
#define THINGSBOARD_SERVER  "thingsboard.cloud" //alamat server dari thingsboard, jika menggunakan server probadi alamatnya bisa disesuaikan, apabila menggunakan server yang disediakan thingsboard alamatnya adalah thingsboar.cloud

Servo myservo;
WiFiClient espClient;
ThingsBoard tb(espClient);


int status = WL_IDLE_STATUS;

//RPC parameters
// deklarasi array untuk pin led yang dikontrol melalui thingsboard
uint8_t leds_control[] = { 2, 4 };

// Main application loop delay
int quant = 20;

// Set to true if application is subscribed for the RPC messages.
bool subscribed = false;
// LED number that is currenlty ON.
int current_led = 0;


// Variables will change:
int jumlahTekananBlue = 0;
int jumlahTekananRed = 0;
int servo_pos = 0;
int mode = 1;
int rainValue;

int lightValue;
float lux;


RPC_Response processSetGpioState(const RPC_Data &data) //perintah untuk menyalakan atau mematikan (perintah kontrol) outputnya
{
  Serial.println("Received the set GPIO RPC method");

  int pin = data["pin"];
  bool enabled = data["enabled"];

  if (pin < COUNT_OF(leds_control)) {
    Serial.print("Setting LED ");
    Serial.print(pin);
    Serial.print(" to state ");
    Serial.println(enabled);

    digitalWrite(leds_control[pin], enabled);
  }

  return String("{\"" + String(pin) + "\": " + String(enabled?"true":"false") + "}");
}

RPC_Response processGetGpioState(const RPC_Data &data) //membaca kondisi dari gpio
{
  Serial.println("Received the get GPIO RPC method");
  String respStr = "{";
  
  for (size_t i = 0; i < COUNT_OF(leds_control); ++i) {
    int pin = leds_control[i];
    Serial.print("Getting LED ");
    Serial.print(pin);
    Serial.print(" state ");
    bool ledState = digitalRead(pin);
    Serial.println(ledState);

    respStr += String("\"" + String(i) + "\": " + String(ledState?"true":"false") + ", ");
  }  
  respStr = respStr.substring(0, respStr.length() - 2);
  respStr += "}";
  return respStr;
}

// konversi perintah menjadi perintah yang dikenal oleh rpc
RPC_Callback callbacks[] = {
  { "setGpioStatus",    processSetGpioState },
  { "getGpioStatus",    processGetGpioState },
};



void setup() {
  Serial.begin(115200);
  myservo.attach(SERVO_PIN);
  myservo.write(servo_pos);
  // initialize the pushbutton pin as an pull-up input

  pinMode(BUTTON_RED, INPUT);
  pinMode(BUTTON_BLUE, INPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_OL, OUTPUT);
  pinMode(LED_GROW, OUTPUT); 
  digitalWrite(LED_OL, LOW);
  Serial.println();
  connect();
}



void loop() {
  //  delay(10);
  //  modeManual();
  //  modeAuto();

  if (WiFi.status() != WL_CONNECTED) { //perintah untuk reconnect apabila esp32 disconnect dengan wifi
    reconnect();
  }

  connectTb();
  //rpcConnect();
  //delay(1000);

  //
  int switchVal = digitalRead(BUTTON_BLUE);
  if (switchVal == HIGH)
  {
    delay(500);
    mode ++;
    //Reset count if over max mode number
    if (mode == 3)
    {
      mode = 1;
    }
  }

  else
    //Change mode
    switch (mode) {
      case 1:
        digitalWrite(LED_BLUE, HIGH);
        digitalWrite(LED_RED, LOW);
        //Serial.print("Counter = ");
        //Serial.println(counter);
        Serial.println("Mode: AUTO");
        tb.sendTelemetryString("Mode", "AUTO");

        modeAuto();
        break;
      case 2:
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_BLUE, LOW);
        //Serial.print("Counter = ");
        //Serial.println(counter);
        Serial.println("Mode: MANUAL");
        tb.sendTelemetryString("Mode", "MANUAL");

        modeManual();
        break;
    }
  //
}




void connect() //void mengoneksikan esp dengan wifi pertama kali
{
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
  digitalWrite(LED_OL, HIGH);
}

void reconnect() { //peintah recconect wifi apabila esp terputus dengan jaringan wifinya
  // Loop until we're reconnected
  status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");

  }
}


void readSensor() {
  // reads the input on analog pin (value between 0 and 4095)
  rainValue = analogRead(RAINDROP_SENSOR_PIN);
  rainValue = map(rainValue, 0, 4095, 0, 100);


  lightValue = analogRead(LIGHT_SENSOR_PIN);
  lightValue = map(lightValue, 4063, 32, 1015, 8);
  float volt = lightValue / 1024.*5;
  float R = 2000 * volt / (1 - volt / 5);
  lux = pow(rl10 * 1e3 * pow(10, gama) / R, (1 / gama));

}

  
void sendData() {
  readSensor();
  Serial.println("Sending data Telemetry");
  tb.sendTelemetryFloat("Lux", lux); //perintah kirim data temperature ke thingsboard dengan menggunakan tipe data integer, key yang digunakan adalah suhu data yang dikirimkan adalah data.temperature, jika ingin mengirim data dari sensor lain bisa disesuaikan
  tb.sendTelemetryInt("Rain", rainValue);
}

void printSensor() {
  readSensor();
  Serial.print("Lux = ");
  Serial.println(lux);
  Serial.print("Rain = ");
  Serial.println(rainValue);
  
}

void modeAuto() {
  readSensor();
  // We'll have a few threshholds, qualitatively determined
  if (lux <= 100) { //tidak ada cahaya matahari
    //Serial.println(" >> Masih gelap, atap ditutup");
    servo_pos = 0;
    myservo.write(servo_pos);
    tb.sendTelemetryString("Atap", "TUTUP");
    digitalWrite(LED_GROW, LOW);
    readSensor();
    sendData();
    printSensor();
  }
  else if (lux >= 200 && rainValue >= 50) { //matahari bersinar & hujan
    //Serial.println(" >> Hujan, atap ditutup");
    servo_pos = 0;
    myservo.write(servo_pos);
    tb.sendTelemetryString("Atap", "TUTUP");
    tb.sendTelemetryString("Cuaca", "HUJAN");
    digitalWrite(LED_GROW, HIGH);
    readSensor();
    sendData();
    printSensor();
  }
  else if (lux >= 200 && rainValue < 50) { //matahari bersinar & tidak hujan
    //Serial.println(" >> Cerah, atap dibuka");
    servo_pos = 180;
    myservo.write(servo_pos);
    tb.sendTelemetryString("Atap", "BUKA");
    tb.sendTelemetryString("Cuaca", "CERAH");
    digitalWrite(LED_GROW, LOW);
    readSensor();
    sendData();
    printSensor();
  }
  Serial.println();

}


void modeManual() {

  int statusTombolRed = digitalRead(BUTTON_RED); //membaca sinyal digital yang dikirim oleh tombol ke pin dan dimasukkan ke dalam variabel statusTombol
      readSensor();
      sendData();
      printSensor();

  if (statusTombolRed == HIGH) {
    jumlahTekananRed ++; //menambahkan nilai +1 ke dalam variabel jumlahTekanan untuk menghitung berapa kali tekanan diberikan
    delay(200); //delay agar tidak terjadi pembacaan tekanan dua kali
    if (jumlahTekananRed == 1) {
      servo_pos = 180;
      myservo.write(servo_pos);
      tb.sendTelemetryString("Atap", "BUKA");
    }
    else if (jumlahTekananRed == 2) {
      servo_pos = 0;
      myservo.write(servo_pos);
      tb.sendTelemetryString("Atap", "TUTUP");
      jumlahTekananRed = 0;
    }
  }
}


void connectTb() {
  if (!tb.connected()) { //perintah untuk koneksi ke server thingsboard/device thingsboard
    // Connect to the ThingsBoard
    digitalWrite(LED_OL, HIGH);
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      digitalWrite(LED_OL, LOW);
      return;
    }
  }
}

//RPC function start here

void rpcConnect() {
  // perintah menghubungkan dengan rpc
  if (!subscribed) {
    Serial.println("Subscribing for RPC... ");

    // Perform a subscription. All consequent data processing will happen in
    // callbacks as denoted by callbacks[] array.
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    Serial.println("Subscribe done");
    subscribed = true;
  }  
}

