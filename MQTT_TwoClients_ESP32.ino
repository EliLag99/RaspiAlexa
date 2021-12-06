#include <WiFi.h>
#include <PubSubClient.h>  // Download and install this library first from: https://www.arduinolibraries.info/libraries/pub-sub-client
#include <WiFiClient.h>

#define SSID_NAME "VM8104350"               // Your Wifi Network name
#define SSID_PASSWORD "qtcx7hpZzfs7"        // Your Wifi network password
#define MQTT_BROKER "smartnest.cz"          // Broker host
#define MQTT_PORT 1883
#define MQTT_USERNAME "ILZ008"                    // Username from Smartnest
#define MQTT_PASSWORD "Bayern4ever"               // Password from Smartnest (or API key)
#define MQTT_CLIENT1 "605fd14bd0ec45783ea739fb"   // Device Id from smartnest
#define MQTT_CLIENT2 "605fd1acd0ec45783ea739fc"   // Device Id from smartnest
#define FIRMWARE_VERSION "Elias-Roomlights"       // Custom name for this program

WiFiClient espClient1;
WiFiClient espClient2;
PubSubClient client1(espClient1);
PubSubClient client2(espClient2);

int lightPin1 = 2;
int lightPin2 = 4;

int L1State = 255;
int L2State = 255;

int PWMChannel0 = 0;
int PWMChannel1 = 1;

void startWifi();
void startMqtt(PubSubClient, char*);
void checkMqtt(PubSubClient, char*);
void connectClient(PubSubClient, char*);
int  splitTopic(char*, char* tokens[], int);
void callback(char*, byte*, unsigned int);
void sendToBroker(PubSubClient, char*, char*, char*);



void setup() {
  ledcSetup(PWMChannel0, 25000, 8);
  ledcSetup(PWMChannel1, 25000, 8);
  ledcAttachPin(lightPin1, PWMChannel0);
  ledcAttachPin(lightPin2, PWMChannel1);

  Serial.begin(115200);
  startWifi();
  Serial.println("Finishing setup");
}

void loop() {
  if(WiFi.status()!= WL_CONNECTED)
    startWifi();
  checkMqtt(client1,MQTT_CLIENT1);
  client1.loop();
  checkMqtt(client2,MQTT_CLIENT2);
  client2.loop();
}


void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  Serial.println("Connecting ...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    attempts++;
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println('\n');
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());

  } else {
    Serial.println('\n');
    Serial.println('I could not connect to the wifi network after 10 attempts \n');
  }

  delay(500);
}

void checkMqtt(PubSubClient currentClient, char* mqtt) {
  if (!currentClient.connected()) {
    startMqtt(currentClient, mqtt);
  }
}

void startMqtt(PubSubClient currentClient, char* mqtt) {
  connectClient(currentClient, MQTT_CLIENT1);
    
  char subscribeTopic[100];
  sprintf(subscribeTopic, "%s/#", mqtt);
  currentClient.subscribe(subscribeTopic);  //Subscribes to all messages send to the device

  sendToBroker(currentClient, mqtt, "report/online", "true");  // Reports that the device is online
  delay(100);
  sendToBroker(currentClient, mqtt, "report/firmware", FIRMWARE_VERSION);  // Reports the firmware version
  delay(100);
  sendToBroker(currentClient, mqtt, "report/ip", (char*)WiFi.localIP().toString().c_str());  // Reports the ip
  delay(100);
  sendToBroker(currentClient, mqtt, "report/network", (char*)WiFi.SSID().c_str());  // Reports the network name
  delay(100);

  char signal[5];
  sprintf(signal, "%d", WiFi.RSSI());
  sendToBroker(currentClient, mqtt, "report/signal", signal);  // Reports the signal strength
  delay(100);
}

void connectClient(PubSubClient clientToConnect, char* mqtt){
  clientToConnect.setServer(MQTT_BROKER, MQTT_PORT);
  clientToConnect.setCallback(callback);

  while (!clientToConnect.connected()) {
    Serial.println("Connecting Client to MQTT...");

    if (clientToConnect.connect(mqtt, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("Client connected");
    } else {
      if (clientToConnect.state() == 5) {
        Serial.println(mqtt);
        Serial.println("Connection not allowed by broker, possible reasons:");
        Serial.println("- Device is already online. Wait some seconds until it appears offline for the broker");
        Serial.println("- Wrong Username or password. Check credentials");
        Serial.println("- Client Id does not belong to this username, verify ClientId");

      } else {
        Serial.println("Not possible to connect to Broker. Error code:");
        Serial.print(clientToConnect.state());
      }
        delay(0x7530);
    }
  }
}

void sendToBroker(PubSubClient currentClient, char* mqtt, char* topic, char* message) {
  if (currentClient.connected()) {
    char topicArr[100];
    sprintf(topicArr, "%s/%s", mqtt, topic);
    currentClient.publish(topicArr, message);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {  //A new message has been received
  Serial.print("Topic:");
  Serial.println(topic);
  int tokensNumber = 10;
  char* tokens[tokensNumber];
  char message[length + 1];
  splitTopic(topic, tokens, tokensNumber);
  sprintf(message, "%c", (char)payload[0]);
  for (int i = 1; i < length; i++) {
    sprintf(message, "%s%c", message, (char)payload[i]);
  }
  Serial.print("Message:");
  Serial.println(message);

  //------------------ACTIONS HERE---------------------------------
  if (strcmp(tokens[1], "directive") == 0 && (strcmp(tokens[2], "powerState") == 0)) {
      if (strcmp(message, "ON") == 0) {
        if(strcmp(tokens[0], MQTT_CLIENT1) == 0){
          ledcWrite(PWMChannel0, L1State);
          sendToBroker(client1, MQTT_CLIENT1, "report/powerState", "ON");
        }
        else{
          ledcWrite(PWMChannel1, L2State); 
          sendToBroker(client2, MQTT_CLIENT2, "report/powerState", "ON");
        }
      } else if (strcmp(message, "OFF") == 0) {
        if(strcmp(tokens[0], MQTT_CLIENT1) == 0){
          ledcWrite(PWMChannel0, 0);
          sendToBroker(client1, MQTT_CLIENT1, "report/powerState", "OFF");
        }
        else{
          ledcWrite(PWMChannel1, 0);
          sendToBroker(client2, MQTT_CLIENT2, "report/powerState", "OFF");
        }
      } 
    }
  
   if (strcmp(tokens[1], "directive") == 0 && (strcmp(tokens[2], "percentage") == 0)) {
        int pwm = atoi(message)*2.55;
        if(strcmp(tokens[0], MQTT_CLIENT1) == 0){
          ledcWrite(PWMChannel0, pwm);
          L1State = pwm;
          sendToBroker(client1, MQTT_CLIENT1, "report/percentage", message);
        }
        else{
          ledcWrite(PWMChannel1, pwm);
          L2State = pwm;
          sendToBroker(client2, MQTT_CLIENT2, "report/percentage", message);
        }
    }
}

int splitTopic(char* topic, char* tokens[], int tokensNumber) {
  const char s[2] = "/";
  int pos = 0;
  tokens[0] = strtok(topic, s);

  while (pos < tokensNumber - 1 && tokens[pos] != NULL) {
    pos++;
    tokens[pos] = strtok(NULL, s);
  }

  return pos;
}
