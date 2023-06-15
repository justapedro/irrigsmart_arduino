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

#include <HTTPClient.h>
#include <DNSServer.h> 
#include <WebServer.h>
#include <WiFiManager.h>

// Bibliotecas do Firebase 
// para comunicação com banco de dados
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// Definição de pinos e valores
#define pinoSU A0
#define pinoSN A3
#define pinoBoia 18
#define pinoRele 23
#define pino5V 4
#define pino5V2 5
#define pino5V3 19

#define API_KEY "AIzaSyC6ENp5kcG9gEss7kN7qpHml6GnPSBJ4sg"
#define FIREBASE_PROJECT_ID "irrigsmart-fe662"
#define USER_EMAIL "teste@teste.com"
#define USER_PASSWORD "teste123"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3600;
const int   daylightOffset_sec = 3600;
const int   refreshsec = 10000;
const int   watersec = 4000;

// Definição de variáveis
int ValAnalogUmidade; // Leitura do Sensor de Umidade
int ValAnalogNivel; // Leitura do Sensor de Nível
bool taskcomplete = true;

WiFiManager wifiManager;

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
        Serial.printf("\n[FB] Enviando dados (%d)...\n", info.size);
    }
    else if (info.status == fb_esp_cfs_upload_status_upload)
    {
        Serial.printf("[FB] Enviado %d%s\n", (int)info.progress, "%");
    }
    else if (info.status == fb_esp_cfs_upload_status_complete)
    {
        Serial.println("[FB] Dados enviados com sucesso ");
    }
    else if (info.status == fb_esp_cfs_upload_status_process_response)
    {
        Serial.print("[FB] Processando resposta... ");
    }
    else if (info.status == fb_esp_cfs_upload_status_error)
    {
        Serial.printf("[FB] Falha no envio de dados, %s\n", info.errorMsg.c_str());
    }
}

//callback que indica que o ESP entrou no modo AP
void configModeCallback (WiFiManager *myWiFiManager) {  
  Serial.println("[WM] Entrou no modo de configuração");
  Serial.println(WiFi.softAPIP()); //imprime o IP do AP
  Serial.println(myWiFiManager->getConfigPortalSSID()); //imprime o SSID criado da rede
}
 
//Callback que indica que salvamos uma nova rede para se conectar (modo estação)
void saveConfigCallback () {
  Serial.println("[WM] Configuração salva");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Projeto IrrigSmart");

  Serial.println("[I] Ativando pinos... ");
  pinMode(pinoRele, OUTPUT);
  pinMode(pino5V, OUTPUT);
  pinMode(pino5V2, OUTPUT);
  pinMode(pinoBoia, INPUT);
  pinMode(pino5V3, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(pinoRele, HIGH);
  digitalWrite(pino5V, HIGH);
  digitalWrite(pino5V2, HIGH);
  digitalWrite(pino5V3, LOW);
  Serial.print("OK");

  Serial.println("[WM] Inicializando Portal");
  wifiManager.setAPCallback(configModeCallback); 
  wifiManager.setSaveConfigCallback(saveConfigCallback); 
  wifiManager.resetSettings();
  if(!wifiManager.startConfigPortal("IrrigSmart") ){ //Nome da Rede e Senha gerada pela ESP
    Serial.println("[WM] Falha ao conectar, reiniciando..."); //Se caso não conectar na rede mostra mensagem de falha
    delay(2000);
    ESP.restart(); //Reinicia ESP após não conseguir conexão na rede
  }
  else{       //Se caso conectar 
    Serial.println("[WM] Conectado na Rede!");
  }

  if(WiFi.status()== WL_CONNECTED){ //Se conectado na rede
    
    digitalWrite(LED_BUILTIN, HIGH);

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
    Serial.print("Conectado com o IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Configuração do Firebase

    Serial.printf("[FB] Inicializando Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
      config.wifi.clearAP();
      config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
    #endif

    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    #if defined(ESP8266)
      fbdo.setBSSLBufferSize(2048, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
    #endif

    fbdo.setResponseSize(2048);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("[I] Conectando ao servidor de tempo online");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  }

}

void loop() {

  bool irrigado = false;

  if (millis() - dataMillis > refreshsec || dataMillis == 0)
  {
    dataMillis = millis();

    digitalWrite(LED_BUILTIN, HIGH); 
    ValAnalogUmidade = analogRead(pinoSU); 
    int PorcentoUmidade = map(ValAnalogUmidade, 4095, 0, 0, 100);
    Serial.print("Nivel da umidade (A): ");
    Serial.print(ValAnalogUmidade);    
    Serial.print("\nNivel da umidade (%): ");
    Serial.print(PorcentoUmidade);
    Serial.print("%");
    Serial.println(" ");

    Serial.print("Nivel do reservatorio: ");
    Serial.print(digitalRead(pinoBoia));
    Serial.println(" ");
    
    if (PorcentoUmidade <= 45) {
      if(digitalRead(pinoBoia) == 1) {
        Serial.println("Irrigando a planta ..."); 
        digitalWrite(pinoRele, LOW); 
        delay (watersec);
        digitalWrite(pinoRele, HIGH); 
        delay (1000);
        ValAnalogUmidade = analogRead(pinoSU); 
        PorcentoUmidade = map(ValAnalogUmidade, 4095, 1365, 0, 100);
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
    digitalWrite(LED_BUILTIN, LOW); 
    digitalWrite(pinoRele, HIGH);

    if (WiFi.status()== WL_CONNECTED && Firebase.ready()) {
      
      FirebaseJson content;

      // Obter tempo
      struct tm timeinfo;
      char buffer[30];
            
      if(!getLocalTime(&timeinfo)){
        Serial.println("Falha ao obter tempo");
      }
      else {
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        Serial.println(buffer);
      }      


      //Envia dados para o firebase
      String documentPath = "/0/";
      documentPath += auth.token.uid.c_str();
      
      content.set("fields/umidade/integerValue", String(PorcentoUmidade));
      content.set("fields/reservatorio/integerValue", String("1"));
      if (irrigado == true) {
        content.set("fields/datairrigado/timestampValue", buffer);
      }
      content.set("fields/dataleitura/timestampValue", buffer);

      if (!taskcomplete) 
      {
        taskcomplete = true;
        Serial.print("[FB] Criando documento... ");
        if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())) 
        {
          Serial.printf("ok");
        }
        else
        {
          Serial.println(fbdo.errorReason());
        }
      } 
      else
      {
        Serial.print("[FB] Atualizando documento... ");
        if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), irrigado ? "umidade,reservatorio,datairrigado,dataleitura" : "umidade,reservatorio,dataleitura")) 
        {
          Serial.printf("ok\n");
        }
        else
        {
          Serial.println(fbdo.errorReason());
        }
      }
    }
  }
}