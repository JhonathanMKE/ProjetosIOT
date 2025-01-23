
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <SPI.h>
#include <MFRC522.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wifi.h>


//#define UID_TOPIC //REMOVER DPS
String UID_TOPIC;
String ACCESS_TOPIC;

#define TEMP_TOPIC "esp32/local/uid"
#define LED_TOPIC "esp32/led"
const char* ssid = "brisa-1441146";
const char* password = "xbe6e0nx";


const char* mqttserver = "broker.hivemq.com"; //BROKER ADDRESS
const int mqttport = 1883; //BROKER PORT

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

#define SS_PIN 5
#define RST_PIN 2

MFRC522 rfid(SS_PIN, RST_PIN); //CRIANDO INSTÂNCIA DO mfrc522

MFRC522::MIFARE_Key key; 

byte nuidPICC[4];

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte name0x0[] = { B00100, B01010, B10001, B00100, B01010, B00000, B00100, B00000 };

void scrollText(String message, int row, int delayMs = 300) {
  int len = message.length();
  if (len <= 16) {
    // Se a mensagem couber no display, mostre normalmente
    lcd.setCursor(0, row);
    lcd.print(message);
    return;
  }

  // Se for maior, faz o scroll
  for (int i = 0; i < len - 15; i++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(i, i + 16)); // Mostra 16 caracteres por vez
    delay(delayMs);
  }
}

void setup_wifi() //CONFIGURAÇÃO DO WIFI E OBTENÇÃO DE MAC PARA TÓPICO
{
  delay(10);
  lcd.clear();
  lcd.print("Conectando WiFi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }
  lcd.clear();
  lcd.print("WiFi Conectado!");
  delay(2000);
  lcd.clear();
  lcd.print("IP: ");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());

  uint8_t mac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
  char macAddress[18];
  snprintf(macAddress, sizeof(macAddress), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  UID_TOPIC = String("esp32/uid/") + macAddress;
  ACCESS_TOPIC = String("esp32/access/") + macAddress;
  delay(2000);
  lcd.clear();
  lcd.print("MAC:");
  lcd.setCursor(0, 1);
  lcd.print(macAddress);
  delay(2000);
}

void callback(char* topic, byte* payload, unsigned int length) //FUNÇÃO PARA TRATAMENTO DO PAYLOAD RECEBIDO
{
  String topicStr;
  String messageStr;
  topicStr = topic;

  for (int i = 0; i < length; i++) {
    messageStr += (char)payload[i];
  }

  Serial.print("Messagem recebida [");
  Serial.print(topicStr);
  Serial.print("] ");
  Serial.println();

  if(topicStr == ACCESS_TOPIC)
  {
    if (messageStr.isEmpty()) {
      //digitalWrite(LED_PIN, HIGH); // DESLIGAR LED - NÃO LIBERAR ACESSO
      Serial.println("Acesso negado!");
      lcd.print("Acesso negado!");
      digitalWrite(35, HIGH);
      digitalWrite(33, LOW);
      delay(1000);
      digitalWrite(35, LOW);
      digitalWrite(33, LOW);
    } else {
      digitalWrite(35, LOW);
      digitalWrite(33, HIGH);
      scrollText(messageStr, 0); //CHAMAR FUNÇÃO DO LCD PARA MOSTRAR TEXTO PASSANDO
      digitalWrite(35, LOW);
      digitalWrite(33, LOW);
    }
  } else {
    if(topicStr == "esp32/access/all"){
      lcd.clear();
      lcd.print(messageStr);
      lcd.createChar(0, name0x0);
      lcd.createChar(1, name0x0);
      lcd.setCursor(15,1);
      lcd.write(1);
      lcd.setCursor(0,1);
      lcd.write(0);
    }
  }
}

void reconnect() //FUNÇÃO DE RECONEXÃO MQTT
{
  while (!client.connected()) {
    lcd.clear();
    lcd.print("Conectando MQTT");
    String clientId = "EPS32 - ";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      lcd.clear();
      lcd.print("MQTT Conectado");
      delay(2000);
    
      client.subscribe("esp32/access/#");
  } else {
    lcd.clear();
    lcd.print("Falha, rc=");
    lcd.setCursor(0,1);
    lcd.print(client.state());
    lcd.clear();
    lcd.print(" Tentando novamente em 5s");
    delay(5000);
  }
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

  pinMode(35, OUTPUT);
  pinMode(33, OUTPUT);

  digitalWrite(35, HIGH);
  digitalWrite(33, HIGH);
  delay(2000);
  digitalWrite(35, LOW);
  digitalWrite(33, LOW);

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  lcd.init();
  lcd.backlight();

  lcd.print("Inicializando...");
  delay(2000);

  setup_wifi();

  client.setServer(mqttserver, mqttport); 

  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop(); //O processamento das mensagens é assíncrono.

  if ( ! rfid.PICC_IsNewCardPresent())
    return;
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  lcd.clear();
  lcd.print("Verificando acesso...");

  String uidMessage = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidMessage += "0"; // Adicionar 0 para valores menores que 0x10
    uidMessage += String(rfid.uid.uidByte[i], HEX);
  }

  client.publish(UID_TOPIC.c_str(), uidMessage.c_str());

  lcd.clear();
  lcd.print("UID Publicado:");
  lcd.setCursor(0, 1);
  lcd.print(uidMessage);
  delay(2000);
  lcd.clear();

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

}
