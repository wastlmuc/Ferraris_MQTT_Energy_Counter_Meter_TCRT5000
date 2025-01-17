/***************************************************************************
  Part one:
  IR Ferraris Reader
  2019 Adlerweb//Bitbastelei
  Florian Knodt - www.adlerweb.info
  Part two:
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
  https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
  Part three:
  MQTT and combine other parts
  2020 Eisbaeeer
  This sketch allows you to connect an infrared sensor who detect the red mark on a ferraris energy counter.
  The data will be sent via MQTT to a server. The counter will be stored on the file-system as json.
  The ESP firmware update can be done via "Over-The-Air".
  
 * Used pins
 * Internal LED       (D0) GPIO 16
 * IR Pin Messure 1   (D1) GPIO 05
 * IR Pin Messure 2   (D2) GPIO 04
 * IR Pin Messure 3   (D3) GPIO 00
 * free               (D4) GPIO 02
 * IR Pin Messure 4   (D5) GPIO 14
 * free               (D6) GPIO 12
 * free               (D7) GPIO 13
 * free               (D8) GPIO 15
 * free               (SDD3) GPIO 10
 * 
*********/
#include <Arduino.h>
#include <PubSubClient.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "dashboard.h"
#include "timeSync.h"
#include <ArduinoJson.h>

// Infrared vars
#define IRPIN1 D1
#define IRPIN2 D2
#define IRPIN3 D3
#define IRPIN4 D5
#define RED1 LOW
#define SILVER1 HIGH
#define RED2 LOW
#define SILVER2 HIGH
#define RED3 LOW
#define SILVER3 HIGH
#define RED4 LOW
#define SILVER4 HIGH
#define MINTIME 2    //in 10ms = 20ms
#define MSG_BUFFER_SIZE	(20)
char result[MSG_BUFFER_SIZE];

bool lastState1 = 1;  // 0 = Silver->Red; 1 = Red->Silver
bool lastState2 = 1;  
bool lastState3 = 1; 
bool lastState4 = 1;  
bool saveConfig = false;
unsigned long lastmillis1 = 0;
unsigned long pendingmillis1 = 0;
unsigned long lastmillis2 = 0;
unsigned long pendingmillis2 = 0;
unsigned long lastmillis3 = 0;
unsigned long pendingmillis3 = 0;
unsigned long lastmillis4 = 0;
unsigned long pendingmillis4 = 0;
bool inbuf1[MINTIME];
bool inbuf2[MINTIME];
bool inbuf3[MINTIME];
bool inbuf4[MINTIME];
bool startup1=true;
bool startup2=true;
bool startup3=true;
bool startup4=true;
bool calcPower1Stat;
bool calcPower2Stat;
bool calcPower3Stat;
bool calcPower4Stat;
bool debStat1;
bool debStat2;
bool debStat3;
bool debStat4;
int loops_actual_1 = 0;
int loops_actual_2 = 0;
int loops_actual_3 = 0;
int loops_actual_4 = 0;

int mqttPublishTime;          // last publish time in seconds
int mqttReconnect;            // timeout for reconnecting MQTT Server

// MQTT
WiFiClient espClient;
PubSubClient MQTTclient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

String getTopicName(int meter, String measurement){
  String  topic = "Ferraris/";
  topic = topic + configManager.data.messure_place;
  topic = topic +"/meter";
  topic = topic + String(meter);
  topic = topic +"/";
  topic = topic + measurement;
  
  return topic;
}

String getHATopicName(String mqtt_type, char uniqueId[30]){
  String  topic = "homeassistant/";
  topic = topic + mqtt_type;
  topic = topic +"/Ferraris_";
  topic = topic + String(uniqueId);
  topic = topic +"/config";

  return topic;
}

String getSetTopicName(int meter, String measurement){
  String  topic = getTopicName(meter,measurement);
  topic = topic + "/set"; 
  
  return topic;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // copy payload into string compatible format
  char pl[length+1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    pl[i]=(char)payload[i];
  }
  pl[length] = '\0';
  Serial.println();

  String p = String(pl);
  String t = String(topic);

  String ukwhCmdTopic;
  String kwhCmdTopic;
  String debounceTimeCmdTopic;
  bool processed=false;
  for (int i=0;i<4;i++){
    ukwhCmdTopic=getSetTopicName(i+1,"UKWh");
    kwhCmdTopic=getSetTopicName(i+1,"Stand");
    debounceTimeCmdTopic=getSetTopicName(i+1,"Entprellzeit");

    if (t == ukwhCmdTopic){
      int16_t meters_per_loop = p.toInt();
      switch (i+1)
      {
        case 1:
          Serial.print("Setting configManager.data.meter_loops_count_1 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_1=meters_per_loop;
          saveConfig=true;
          processed=true;
          break;
        case 2:
          Serial.print("Setting configManager.data.meter_loops_count_2 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_2=meters_per_loop;
          saveConfig=true;
          processed=true;
          break;
        case 3:
          Serial.print("Setting configManager.data.meter_loops_count_3 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_3=meters_per_loop;
          saveConfig=true;
          processed=true;
          break;
        case 4:
          Serial.print("Setting configManager.data.meter_loops_count_4 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_4=meters_per_loop;
          saveConfig=true;
          processed=true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }

    if (t == kwhCmdTopic){
      int16_t meter_value = p.toInt();
      switch (i+1)
      {
        case 1:
          Serial.print("Setting configManager.data.meter_counter_reading_1 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_1=meter_value;
          saveConfig=true;
          processed=true;
          break;
        case 2:
          Serial.print("Setting configManager.data.meter_counter_reading_2 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_2=meter_value;
          saveConfig=true;
          processed=true;
          break;
        case 3:
          Serial.print("Setting configManager.data.meter_counter_reading_3 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_3=meter_value;
          saveConfig=true;
          processed=true;
          break;
        case 4:
          Serial.print("Setting configManager.data.meter_counter_reading_4 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_4=meter_value;
          saveConfig=true;
          processed=true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }

    if (t == debounceTimeCmdTopic){
      int16_t debounce_value = p.toInt();
      switch (i+1)
      {
        case 1:
          Serial.print("Setting configManager.data.debounce_1 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.debounce_1=debounce_value;
          saveConfig=true;
          processed=true;
          break;
        case 2:
          Serial.print("Setting configManager.data.debounce_2 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.debounce_2=debounce_value;
          saveConfig=true;
          processed=true;
          break;
        case 3:
          Serial.print("Setting configManager.data.debounce_3 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.debounce_3=debounce_value;
          saveConfig=true;
          processed=true;
          break;
        case 4:
          Serial.print("Setting configManager.data.debounce_4 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.debounce_4=debounce_value;
          saveConfig=true;
          processed=true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }
  }

  if(!processed){
    Serial.print("Could not process request!");
    Serial.println();
  }

}

// Tasks
struct task
{    
    unsigned long rate;
    unsigned long previous;
};

task taskA = { .rate = 1000, .previous = 0 };
task taskB = { .rate = 200, .previous = 0 };

unsigned long debouncePrevious1 = 0;
unsigned long debouncePrevious2 = 0;
unsigned long debouncePrevious3 = 0;
unsigned long debouncePrevious4 = 0;

// ### Begin Subroutines
// IR-Sensor Subs
bool getInput(uint8_t pin) {
  byte inchk=0;
  for(byte i=0; i < 5; i++) {
    inchk += digitalRead(pin);
    delay(2);
  }
  if(inchk >= 3) return 1;
  return 0;
}

bool procInput1(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf1[k+1] = inbuf1[k];
    inchk += inbuf1[k];
  }

  //New value
  inbuf1[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput2(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf2[k+1] = inbuf2[k];
    inchk += inbuf2[k];
  }

  //New value
  inbuf2[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput3(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf3[k+1] = inbuf3[k];
    inchk += inbuf3[k];
  }

  //New value
  inbuf3[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput4(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf4[k+1] = inbuf4[k];
    inchk += inbuf4[k];
  }

  //New value
  inbuf4[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

void IRAM_ATTR IRSensorHandle1(void) {
 
   // IR Sensors
  bool cur1 = getInput(IRPIN1);
  cur1 = procInput1(cur1);

  if (!debStat1) {
    switch(lastState1) {
      case 0: //Silver; Waiting for transition to red
        if(cur1 != SILVER1) {
          lastState1 = true;
          pendingmillis1 = millis();
          Serial.println("Silver detected; waiting for red");
          calcPower1Stat = true;
          debouncePrevious1 = millis();
          debStat1 = true;
        }
        break;
      case 1: //Red; Waiting for transition to silver
        if(cur1 != RED1) {
          lastState1=false;
          Serial.println("Red detected; Waiting for silver");
          debouncePrevious1 = millis();
          debStat1 = true;
        }
        break;
    }
  }
}

void IRAM_ATTR IRSensorHandle2(void) {
 
   // IR Sensors
  bool cur2 = getInput(IRPIN2);
  cur2 = procInput2(cur2);
  
  if (!debStat2) {
    switch(lastState2) {
    case 0: //Silver; Waiting for transition to red
      if(cur2 != SILVER2) {
        lastState2=true;
        pendingmillis2 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower2Stat = true;
        debouncePrevious2 = millis();
        debStat2 = true;
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur2 != RED2) {
        lastState2=false;
        Serial.println("Red detected; Waiting for silver");
        debouncePrevious2 = millis();
        debStat2 = true;
      }
      break;
  }
  }
}

void IRAM_ATTR IRSensorHandle3(void) {
 
   // IR Sensors
  bool cur3 = getInput(IRPIN3);
  cur3 = procInput3(cur3);
  
  if (!debStat3) {
  switch(lastState3) {
    case 0: //Silver; Waiting for transition to red
      if(cur3 != SILVER3) {
        lastState3=true;
        pendingmillis3 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower3Stat = true;
        debouncePrevious3 = millis();
        debStat3 = true;
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur3 != RED3) {
        lastState3=false;
        Serial.println("Red detected; Waiting for silver");
        debouncePrevious3 = millis();
        debStat3 = true;
      }
      break;
  }
  }
}

void IRAM_ATTR IRSensorHandle4(void) {
 
   // IR Sensors
  bool cur4 = getInput(IRPIN4);
  cur4 = procInput4(cur4);
  
  if (!debStat4) {
  switch(lastState4) {
    case 0: //Silver; Waiting for transition to red
      if(cur4 != SILVER4) {
        lastState4=true;
        pendingmillis4 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower4Stat = true;
        debouncePrevious4 = millis();
        debStat4 = true;
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur4 != RED4) {
        lastState4=false;
        Serial.println("Red detected; Waiting for silver");
        debouncePrevious4 = millis();
        debStat4 = true;
      }
      break;
  }
  }
}

void PublishMQTT(void) {
      
  detachInterrupt(digitalPinToInterrupt(IRPIN1));
  detachInterrupt(digitalPinToInterrupt(IRPIN2));
  detachInterrupt(digitalPinToInterrupt(IRPIN3));
  detachInterrupt(digitalPinToInterrupt(IRPIN4));

  String topic;
  String cmdTopic;
  String haTopic;
  if(configManager.data.home_assistant_auto_discovery){
    StaticJsonDocument<240> discoverDocument;
    char discoverJson[240];
    char uniqueId[30];
    String meterName;
    for (int i = 0; i < 4; i++) {
      // W
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));

      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "/w", i+1);
      topic = getTopicName(i+1, "power_W");
      meterName = "meter"+String(i+1)+"_power_W";

      discoverDocument["dev_cla"] = "power";
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "W";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 7000;

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("sensor", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)) {
        Serial.print("failed to publish w "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // kWh / Stand
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));

      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "/kwh", i+1);
      topic = getTopicName(i+1, "energy_kWh");
      meterName = "meter "+String(i+1)+"_energy_kWh";
      cmdTopic = getSetTopicName(i+1, "energy_kWh");

      discoverDocument["dev_cla"] = "energy";
      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "kWh";
      discoverDocument["val_tpl"] = "{{value}}";

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("sensor", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)){
        Serial.print("failed to publish kwh "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }
      
      // Impulse
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));
      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "/impulse", i+1);
      topic = getTopicName(i+1, "impulse_1");
      meterName = "meter "+String(i+1) + "_impulse_1";

      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "1";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 1;

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("sensor", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)){
        Serial.print("failed to publish impulse "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // Umdrehungen/kWh
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));
      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "/ukwh", i+1);
      topic = getTopicName(i+1, "rotation_R/kWh");
      meterName = "meter "+String(i+1)+"_rotation_R/kWh";
      cmdTopic = getSetTopicName(i+1, "rotation_R/kWh");

      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "rotation/kWh";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 512;

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("number", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)){
        Serial.print("failed to publish ukwh "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // Entprellzeit
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));
      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "/entprellzeit", i+1);
      topic = getTopicName(i+1, "debounce_ms");
      meterName = "meter "+String(i+1)+"_debounce_ms";
      cmdTopic = getSetTopicName(i+1, "debounce_ms");

      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "ms";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 200; // TODO: Is this a reasonable maximum value?

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("number", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)){
        Serial.print("failed to publish debounce time "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }
    }
  }

  // Meter #1
  topic = getTopicName(1,"Stand");
  dtostrf(configManager.data.meter_counter_reading_1, 9, 3, result);
  MQTTclient.publish(topic.c_str(), result, true);

  topic = getTopicName(1,"W");
  char char_Leistung_Zaehler1[6];
  dtostrf(dash.data.Leistung_Zaehler1, 4, 3, char_Leistung_Zaehler1);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler1, true);
  
  topic = getTopicName(1,"impulse");
  char char_meter_impulse1[1];
  dtostrf(dash.data.Impuls_Z1,1,0, char_meter_impulse1);
  MQTTclient.publish(topic.c_str(), char_meter_impulse1, true);

  topic = getTopicName(1,"UKWh");
  char char_meter_loop_counts1[5];
  dtostrf(configManager.data.meter_loops_count_1,4,0, char_meter_loop_counts1);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts1, true);
  
  topic = getTopicName(1,"Entprellzeit");
  char char_debounce_1[4];
  dtostrf(configManager.data.debounce_1,3,0, char_debounce_1);
  MQTTclient.publish(topic.c_str(), char_debounce_1, true);

  // Meter #2
  topic = getTopicName(2,"Stand");
  dtostrf(configManager.data.meter_counter_reading_2, 9, 3, result);
  MQTTclient.publish(topic.c_str(), result, true);

  topic = getTopicName(2,"W");
  char char_Leistung_Zaehler2[6];
  dtostrf(dash.data.Leistung_Zaehler2, 4, 3, char_Leistung_Zaehler2);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler2, true);

  topic = getTopicName(2,"impulse");
  char char_meter_impulse2[1];
  dtostrf(dash.data.Impuls_Z1,1,0, char_meter_impulse2);
  MQTTclient.publish(topic.c_str(), char_meter_impulse2, true);
  
  topic = getTopicName(2,"UKWh");
  char char_meter_loop_counts2[5];
  dtostrf(configManager.data.meter_loops_count_2,4,0, char_meter_loop_counts2);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts2, true);

  topic = getTopicName(2,"Entprellzeit");
  char char_debounce_2[4];
  dtostrf(configManager.data.debounce_2,3,0, char_debounce_2);
  MQTTclient.publish(topic.c_str(), char_debounce_2, true);

  // Meter #3
  topic = getTopicName(3,"Stand");
  dtostrf(configManager.data.meter_counter_reading_3, 9, 3, result);
  MQTTclient.publish(topic.c_str(), result, true);

  topic = getTopicName(3,"W");
  char char_Leistung_Zaehler3[6];
  dtostrf(dash.data.Leistung_Zaehler3, 4, 3, char_Leistung_Zaehler3);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler3, true);
  
  topic = getTopicName(3,"impulse");
  char char_meter_impulse3[1];
  dtostrf(dash.data.Impuls_Z3,1,0, char_meter_impulse3);
  MQTTclient.publish(topic.c_str(), char_meter_impulse3, true);

  topic = getTopicName(3,"UKWh");
  char char_meter_loop_counts3[5];
  dtostrf(configManager.data.meter_loops_count_3,4,0, char_meter_loop_counts3);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts3, true);

  topic = getTopicName(3,"Entprellzeit");
  char char_debounce_3[4];
  dtostrf(configManager.data.debounce_3,3,0, char_debounce_3);
  MQTTclient.publish(topic.c_str(), char_debounce_3, true);

  // Meter #4
  topic = getTopicName(4,"Stand");
  dtostrf(configManager.data.meter_counter_reading_4, 9, 3, result);
  MQTTclient.publish(topic.c_str(), result, true);

  topic = getTopicName(4,"W");
  char char_Leistung_Zaehler4[6];
  dtostrf(dash.data.Leistung_Zaehler4, 4, 3, char_Leistung_Zaehler4);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler4, true);
  
  topic = getTopicName(4,"impulse");
  char char_meter_impulse4[1];
  dtostrf(dash.data.Impuls_Z4,1,0, char_meter_impulse4);
  MQTTclient.publish(topic.c_str(), char_meter_impulse4, true);

  topic = getTopicName(4,"UKWh");
  char char_meter_loop_counts4[5];
  dtostrf(configManager.data.meter_loops_count_4,4,0, char_meter_loop_counts4);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts4, true);

  topic = getTopicName(4,"Entprellzeit");
  char char_debounce_4[4];
  dtostrf(configManager.data.debounce_4,3,0, char_debounce_4);
  MQTTclient.publish(topic.c_str(), char_debounce_4, true);

  attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
}

void reconnect(void) {
  if (mqttReconnect > 60) {
      detachInterrupt(digitalPinToInterrupt(IRPIN1));
      detachInterrupt(digitalPinToInterrupt(IRPIN2));
      detachInterrupt(digitalPinToInterrupt(IRPIN3));
      detachInterrupt(digitalPinToInterrupt(IRPIN4));
    mqttReconnect = 0;    // reset reconnect timeout
  // reconnect to MQTT Server
  if (!MQTTclient.connected()) {
    dash.data.MQTT_Connected = false;
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "FerrarisClient-";
    //clientId += String(random(0xffff), HEX);
    clientId += String(configManager.data.messure_place);
    // Attempt to connect
    if (MQTTclient.connect(clientId.c_str(),configManager.data.mqtt_user,configManager.data.mqtt_password)) {
      Serial.println("connected");
      dash.data.MQTT_Connected = true;
      // Once connected, publish an announcement...
      PublishMQTT();
      // ... and resubscribe
      // MQTTclient.subscribe("inTopic");
      String topic[3]={"UKWh","Stand","Entprellzeit"};
      for (int tId = 0; tId < 3; tId++) {
        for (int i = 0; i < 4; i++) {
          String t = getSetTopicName(i+1, topic[tId]);
          if (MQTTclient.subscribe(t.c_str())) {
            Serial.print("subscribed to ");
          } else {
            Serial.print("failed to subscribe to ");
          }
          Serial.print(t);
          Serial.println();
        }
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in one minute");
     }
    }
   }
      attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
}

void calcPower1(void)  {
  unsigned long took1 = pendingmillis1 - lastmillis1;
  lastmillis1 = pendingmillis1;

  if(!startup1) {
    dash.data.Leistung_Zaehler1 = 3600000.00 / took1 / configManager.data.meter_loops_count_1 * 1000.00;
    Serial.print(dash.data.Leistung_Zaehler1);
    Serial.print(" W @ ");
    Serial.print(took1);
    Serial.println("ms");

    /***
    // adding float to meter count
    float delta_meter1 = 1.0 / configManager.data.meter_loops_count_1;
    configManager.data.meter_counter_reading_1 += delta_meter1;
    ***/    
    
    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_1 :");
    Serial.print(loops_actual_1);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_1);
    
    // Check if this works!
    // I guess this is only called, wehn loop detected
    configManager.data.meter_counter_reading_1 = configManager.data.meter_counter_reading_1 + 1.0/ configManager.data.meter_loops_count_1;
    
    if(loops_actual_1 < configManager.data.meter_loops_count_1 * 10) {
      loops_actual_1++;
    } else {
   // configManager.data.meter_counter_reading_1++;
    loops_actual_1 = 1;
    saveConfig = true;
    }
    
    Serial.print("meter_counter_reading_1 :");
    Serial.print(configManager.data.meter_counter_reading_1);
    Serial.println(" KWh");
  
    } else {
    startup1=false;
  }
}

void calcPower2(void)  {
  unsigned long took2 = pendingmillis2 - lastmillis2;
  lastmillis2 = pendingmillis2;

  if(!startup2) {
    dash.data.Leistung_Zaehler2 = 3600000.00 / took2 / configManager.data.meter_loops_count_2 * 1000.00;
    Serial.print(dash.data.Leistung_Zaehler2);
    Serial.print(" W @ ");
    Serial.print(took2);
    Serial.println("ms");

    /***
    // adding float to meter count
    float delta_meter2 = 1.0 / configManager.data.meter_loops_count_2;
    configManager.data.meter_counter_reading_2 += delta_meter2;
    ***/

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_2 :");
    Serial.print(loops_actual_2);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_2);
    
    // Check if this works!
    // I guess this is only called, wehn loop detected
    configManager.data.meter_counter_reading_2 = configManager.data.meter_counter_reading_2 + 1.0/configManager.data.meter_loops_count_2;
    
    
    if(loops_actual_2 < configManager.data.meter_loops_count_2 * 10) {
       loops_actual_2++;
    } else {
//    configManager.data.meter_counter_reading_2++;
    loops_actual_2 = 1;
    saveConfig = true;
    }
    
    Serial.print("meter_counter_reading_2 :");
    Serial.print(configManager.data.meter_counter_reading_2);
    Serial.println(" KWh");
  
    } else {
    startup2=false;
  }
}

void calcPower3(void)  {
  unsigned long took3 = pendingmillis3 - lastmillis3;
  lastmillis3 = pendingmillis3;

  if(!startup3) {
    dash.data.Leistung_Zaehler3 = 3600000.00 / took3 / configManager.data.meter_loops_count_3*1000.00;
    Serial.print(dash.data.Leistung_Zaehler3);
    Serial.print(" W @ ");
    Serial.print(took3);
    Serial.println("ms");

    /***
    // adding float to meter count
    float delta_meter3 = 1.0 / configManager.data.meter_loops_count_3;
    configManager.data.meter_counter_reading_3 += delta_meter3;
    ***/

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_3 :");
    Serial.print(loops_actual_3);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_3);
    
    // Check if this works!
    // I guess this is only called, wehn loop detected
    configManager.data.meter_counter_reading_3 = configManager.data.meter_counter_reading_3 + 1.0/configManager.data.meter_loops_count_3;
    
    
    if(loops_actual_3 < configManager.data.meter_loops_count_3 * 10) {
      loops_actual_3++;
    } else {
    //configManager.data.meter_counter_reading_3++;
    loops_actual_3 = 1;
    saveConfig = true;
    }
    
    Serial.print("meter_counter_reading_3 :");
    Serial.print(configManager.data.meter_counter_reading_3);
    Serial.println(" KWh");
  
    } else {
    startup3=false;
  }
}

void calcPower4(void)  {
  unsigned long took4 = pendingmillis4 - lastmillis4;
  lastmillis4 = pendingmillis4;

  if(!startup4) {
    dash.data.Leistung_Zaehler4 = 3600000.00 / took4 / configManager.data.meter_loops_count_4*1000.00;
    Serial.print(dash.data.Leistung_Zaehler4);
    Serial.print(" W @ ");
    Serial.print(took4);
    Serial.println("ms");

    /***
    // adding float to meter count
    float delta_meter4 = 1.0 / configManager.data.meter_loops_count_4;
    configManager.data.meter_counter_reading_4 += delta_meter4;
    ***/

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_4 :");
    Serial.print(loops_actual_4);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_4);
    
    // Check if this works!
    // I guess this is only called, wehn loop detected
    configManager.data.meter_counter_reading_4 = configManager.data.meter_counter_reading_4 + 1.0/configManager.data.meter_loops_count_4;
    
    if(loops_actual_4 < configManager.data.meter_loops_count_4 * 10) {
      loops_actual_4++;
    } else {
    //configManager.data.meter_counter_reading_4++;
    loops_actual_4 = 1;
    saveConfig = true;
    }
    
    Serial.print("meter_counter_reading_4 :");
    Serial.print(configManager.data.meter_counter_reading_4);
    Serial.println(" KWh");
  
    } else {
    startup4=false;
  }
}

// ### End Subroutines


void setup() {
  Serial.begin(115200);

    LittleFS.begin();
    GUI.begin();
    configManager.begin();
    WiFiManager.begin(configManager.data.projectName);
    timeSync.begin();
    dash.begin(500);

  // WiFi
  WiFi.hostname(configManager.data.wifi_hostname);
  WiFi.begin();

  // IR-Sensor
  pinMode(IRPIN1, INPUT_PULLUP);
  pinMode(IRPIN2, INPUT_PULLUP);
  pinMode(IRPIN3, INPUT_PULLUP);
  pinMode(IRPIN4, INPUT_PULLUP);

  // Printout the IP address
  IPAddress ip;
  ip = WiFi.localIP();
  Serial.println("Connected.");
  Serial.print("IP-address : ");
  Serial.println(ip);

    String VERSION = F("v.0.92");
    int str_len = VERSION.length() + 1;
    VERSION.toCharArray(dash.data.Version,str_len);

    MQTTclient.setServer(configManager.data.mqtt_server, configManager.data.mqtt_port);
    MQTTclient.setCallback(callback);
    MQTTclient.setBufferSize(320); // TODO: maybe we can calculate this based on the largest assumed request + its parameters?

    dash.data.KWh_Zaehler1 = configManager.data.meter_counter_reading_1;
    dash.data.KWh_Zaehler2 = configManager.data.meter_counter_reading_2;
    dash.data.KWh_Zaehler3 = configManager.data.meter_counter_reading_3;
    dash.data.KWh_Zaehler4 = configManager.data.meter_counter_reading_4;

    attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
  
}

void loop() {
  // framework things
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();
  MQTTclient.loop();

//tasks
    if (taskA.previous == 0 || (millis() - taskA.previous > taskA.rate))
    {
        taskA.previous = millis();
        int rssi = 0;
        rssi = WiFi.RSSI();
        sprintf(dash.data.Wifi_RSSI, "%ld", rssi) ;

        dash.data.KWh_Zaehler1 = configManager.data.meter_counter_reading_1;
        dash.data.KWh_Zaehler2 = configManager.data.meter_counter_reading_2;
        dash.data.KWh_Zaehler3 = configManager.data.meter_counter_reading_3;
        dash.data.KWh_Zaehler4 = configManager.data.meter_counter_reading_4;
                 
        reconnect();
        mqttReconnect++;      

        if (mqttPublishTime <= configManager.data.mqtt_interval) {
          mqttPublishTime++;
        } else {
          PublishMQTT();
          mqttPublishTime = 0;

          /***
        Serial.println(F("Publish to MQTT Server"));
        Serial.print(F("meter_kw_1: "));
        Serial.print(dash.data.Leistung_Zaehler1);
        Serial.println(" KW");
        Serial.print("loops_actual_1: ");
        Serial.print(loops_actual_1);
        Serial.print(" / ");
        Serial.println(configManager.data.meter_loops_count_1);
        Serial.print("meter_counter_reading_1: ");
        Serial.print(configManager.data.meter_counter_reading_1);
        Serial.println(" KWh");
        ***/
        }
    }

    if (taskB.previous == 0 || (millis() - taskB.previous > taskB.rate))
    {
        taskB.previous = millis();
    }

    if (debouncePrevious1 == 0 || (millis() - debouncePrevious1 > configManager.data.debounce_1))
    {
        debouncePrevious1 = millis();
        if (debStat1) {
          debStat1 = false;
        }
    }

    if (debouncePrevious2 == 0 || (millis() - debouncePrevious2 > configManager.data.debounce_2))
    {
        debouncePrevious2 = millis();
        if (debStat2) {
          debStat2 = false;
        }
    }

    if (debouncePrevious3 == 0 || (millis() - debouncePrevious3 > configManager.data.debounce_3))
    {
        debouncePrevious3 = millis();
        if (debStat3) {
          debStat3 = false;
        }
    }

    if (debouncePrevious4 == 0 || (millis() - debouncePrevious4 > configManager.data.debounce_4))
    {
        debouncePrevious4 = millis();
        if (debStat4) {
          debStat4 = false;
        }
    }

    if (calcPower1Stat) {
      calcPower1();
      calcPower1Stat = false;
    } else if (calcPower2Stat) {
      calcPower2();
      calcPower2Stat = false;
    } else if (calcPower3Stat) {
      calcPower3();
      calcPower3Stat = false;
    } else if (calcPower4Stat) {
      calcPower4();
      calcPower4Stat = false;
    }
    
    if (saveConfig) {
      saveConfig = false;
      detachInterrupt(digitalPinToInterrupt(IRPIN1));
      detachInterrupt(digitalPinToInterrupt(IRPIN2));
      detachInterrupt(digitalPinToInterrupt(IRPIN3));
      detachInterrupt(digitalPinToInterrupt(IRPIN4));
      configManager.save();
      attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
    }

    dash.data.Impuls_Z1 = lastState1;
    dash.data.Impuls_Z2 = lastState2;
    dash.data.Impuls_Z3 = lastState3;
    dash.data.Impuls_Z4 = lastState4;

}
