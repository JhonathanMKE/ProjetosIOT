
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <SPI.h>
#include <MFRC522.h>

#include <WiFi.h>
#include <PubSubClient.h>

#define TEMP_TOPIC "esp32/local/uid"
#define LED_TOPIC "esp32/led"
const char* ssid = "brisa-1441146";
const char* password = "########";

const char* mqttserver = "broker.hivemq.com";
const int mqttport = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

#define SS_PIN 5
#define RST_PIN 2

MFRC522 rfid(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key; 

byte nuidPICC[4];

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup_wifi()
{
  delay(10);
  // Inicia a conexão com a Rede Wi-Fi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String topicStr;
  String messageStr;
  topicStr = topic; //Converte o tópico em String
  for (int i = 0; i < length; i++) { //Converte o payload em String
  messageStr += (char)payload[i];
  }
  Serial.print("Messagem recebida [");
  Serial.print(topicStr);
  Serial.print("] ");
  Serial.println();
  if(topicStr == LED_TOPIC)
  {
  // Liga o LED se o tópico LIGADO == "ON"
  if (messageStr == "ON") {
  //digitalWrite(LED_PIN, HIGH); // Liga o LED
  Serial.println("LED Ligado");
  } else {
  //digitalWrite(LED_PIN, LOW); //Desliga o LED
  Serial.println("LED Desligado");
  }
  }
}

void reconnect()
{
  // Loop até que a conexão seja bem sucedida
  while (!client.connected()) {
  Serial.print("Tentando conexão MQTT...");
  // Cria um ID aleatório para o cliente
  String clientId = "ESP32 - Sensores";
  clientId += String(random(0xffff), HEX);
  // Se conectado
  if (client.connect(clientId.c_str())) {
  Serial.println("conectado");
  // Depois de conectado, publique um anúncio ...
  //client.publish(TEMP_TOPIC, "0.0");
  client.publish(LED_TOPIC, "OFF");
  //... e subscribe.
  client.subscribe(LED_TOPIC); // <<<<----- mudar aqui
  } else {
  Serial.print("Falha, rc=");
  Serial.print(client.state());
  Serial.println(" Tentando novamente em 5s");
  delay(5000);
  }
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  Serial.print(F("Using the following key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);

  lcd.init();
  lcd.backlight();
  lcd.print("Inicializando...");

  setup_wifi();
  client.setServer(mqttserver, 1883); // Publicar
  client.setCallback(callback); // Receber mensagem
}

void loop() {

  if (!client.connected()) {
  reconnect();
  }

  client.loop(); //O processamento das mensagens é assíncrono.

  unsigned long now = millis();
  if (now - lastMsg > 10000) { //Intervalo de 10s entre as publicacoes
  lastMsg = now;
  float temperature;
  temperature = 10;
  char s_temp[8];
  dtostrf(temperature,1,2,s_temp);
  client.publish(TEMP_TOPIC, s_temp);
  Serial.print("Temperatura: ");
  Serial.println(s_temp);
  }

  if ( ! rfid.PICC_IsNewCardPresent())
    return;
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

    if (rfid.uid.uidByte[0] != nuidPICC[0] || 
    rfid.uid.uidByte[1] != nuidPICC[1] || 
    rfid.uid.uidByte[2] != nuidPICC[2] || 
    rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
   
    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();


}

void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(' ');
    Serial.print(buffer[i], DEC);
  }
}
