//
// Projeto IrrigSmart
// Irrigador inteligente microcontrolado
// https://irrigsmart.com.br
//
// 1° Semestre - Engenharia de Computação - Universidade São Francisco
// Prática Profissional Extensionista - Introdução aos Projetos de Engenharia
// Professor Orientador: Débora Meyhofer Ferreira
//
// Feito por Pedro Marcondes - RA 202327824
// Guilherme Paupitz - RA 202314314
// Valber Batista - RA 202331049
//

// Inclui as bibliotecas básicas
#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// Bibliotecas do Firebase 
// para comunicação com banco de dados
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// Definição de pinos e valores
#define pinoSU A0
#define pinoSN A3
#define pinoRele 23
#define pino5V 4
#define pino5V2 5

#define WIFI_SSID "Pedro"
#define WIFI_PASSWORD "pedro*1313"
#define API_KEY "AIzaSyC6ENp5kcG9gEss7kN7qpHml6GnPSBJ4sg"
#define FIREBASE_PROJECT_ID "irrigsmart-fe662"
#define USER_EMAIL "teste@teste.com"
#define USER_PASSWORD "teste123"

// Definição de variáveis
int ValAnalogUmidade; // Leitura do Sensor de Umidade
int ValAnalogNivel; // Leitura do Sensor de Nível
bool taskcomplete = false;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long dataMillis = 0;
int count = 0;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif


// Função de callback do upload do Firebase
void fcsUploadCallback(CFS_UploadStatusInfo info)
{
    if (info.status == fb_esp_cfs_upload_status_init)
    {
        Serial.printf("\nEnviando dados (%d)...\n", info.size);
    }
    else if (info.status == fb_esp_cfs_upload_status_upload)
    {
        Serial.printf("Enviado %d%s\n", (int)info.progress, "%");
    }
    else if (info.status == fb_esp_cfs_upload_status_complete)
    {
        Serial.println("Dados enviados com sucesso ");
    }
    else if (info.status == fb_esp_cfs_upload_status_process_response)
    {
        Serial.print("Processando resposta... ");
    }
    else if (info.status == fb_esp_cfs_upload_status_error)
    {
        Serial.printf("Falha no envio de dados, %s\n", info.errorMsg.c_str());
    }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Projeto IrrigSmart");

  Serial.println("[I] Ativando pinos");
  pinMode(pinoRele, OUTPUT);
  pinMode(pino5V, OUTPUT);
  pinMode(pino5V2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(pino5V, HIGH);
  digitalWrite(pino5V2, HIGH);
  Serial.println("[I] Pinos ativados");

  Serial.println("[I] Iniciando conexão Wi-Fi");
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
    multi.run();
  #else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  #endif

  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
    multi.run();
  #else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  #endif

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
      if (millis() - ms > 10000)
        break;
      #endif
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // The WiFi credentials are required for Pico W
  // due to it does not have reconnect feature.
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    config.wifi.clearAP();
    config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
  #endif

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  #if defined(ESP8266)
    // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
    fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
  #endif

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  // For sending payload callback
  // config.cfs.upload_callback = fcsUploadCallback;
}

void loop() {

  bool irrigado = false;

  if (millis() - dataMillis > 20000 || dataMillis == 0)
  {
    dataMillis = millis();

    digitalWrite(LED_BUILTIN, HIGH); 
    ValAnalogUmidade = analogRead(pinoSU); 
    int PorcentoUmidade = map(ValAnalogUmidade, 4095, 0, 0, 100);
    Serial.print("Nivel da umidade: ");
    Serial.print(PorcentoUmidade);
    Serial.print("%");
    Serial.println(" ");

    int PorcentoNivel = analogRead(pinoSN); 
    Serial.print("Nivel do reservatorio: ");
    Serial.print(PorcentoNivel);
    Serial.println(" ");
    
    if (PorcentoUmidade <= 45) {
      if(PorcentoNivel >= 300) {
        Serial.println("Irrigando a planta ..."); 
        digitalWrite(pinoRele, LOW); 
        irrigado = true;
      } else {
        irrigado = false;
        Serial.println("Reservatorio vazio, nao irrigar ..."); 
        digitalWrite(pinoRele, HIGH);
      }
    }
    else {
      irrigado = false;
      Serial.println("Planta Irrigada ...");
      digitalWrite(pinoRele, HIGH);
    }
    delay (1000);
    digitalWrite(LED_BUILTIN, LOW); 

    if (Firebase.ready()) {
      FirebaseJson content;

      int timestamp;
      timestamp = gettimeofday();

      String documentPath = "/0/";
      documentPath += auth.token.uid.c_str();
      
      content.set("fields/umidade/integerValue", String(PorcentoUmidade));
      content.set("fields/reservatorio/integerValue", String(PorcentoNivel));

      if (irrigado) {
        content.set("fields/datairrigado/integerValue", timestamp);
      }
      content.set("fields/dataleitura/integerValue", timestamp);
      if (!taskcomplete)
      {
        taskcomplete = true;
        Serial.print("Criando documento no Firebase... ");
        if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())) 
        {
          Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        }
        else
        {
          Serial.println(fbdo.errorReason());
        }
      } 
      else
      {
        Serial.print("Atualizando documento no Firebase... ");
        if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "umidade,reservatorio,datairrigado,dataleitura")) 
        {
          Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        }
        else
        {
          Serial.println(fbdo.errorReason());
        }
      }
    }
  }
}