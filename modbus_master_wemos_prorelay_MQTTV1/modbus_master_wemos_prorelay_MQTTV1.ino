#include <SoftwareSerial.h>
// modbus PINs: RX - 5, TX - 4, enPin - 2
SoftwareSerial Serial100(5, 4);
#define enPin 2

#include <ESP8266WiFi.h>
#include "PubSubClient.h"
// WIFI
const char *ssid = "xxxx"; // Имя вайфай точки доступа
const char *pass = "xxxx"; // Пароль от точки доступа
WiFiClient wclient; 

// MQTT
const char *mqtt_server = "xxx.xxxxxxxxxxxx.xxx"; // Имя сервера MQTT
const int mqtt_port = xxxx; // Порт для подключения к серверу MQTT
const char *mqtt_user = "xxxx"; // Логин от сервер
const char *mqtt_pass = "xxxx"; // Пароль от сервера

PubSubClient client(wclient, mqtt_server, mqtt_port);

byte _gtv1 = 0;
byte _gtv2 = 0;
byte temp_gtv1 = 0;
byte temp_gtv2 = 0;

// Функция получения данных от MQTT сервера
void callback(const MQTT::Publish& pub)
{

//Serial.print(pub.topic()); // выводим в сериал порт название топика
//Serial.print(" => ");
//Serial.println(pub.payload_string()); // выводим в сериал порт значение полученных данных

String payload = pub.payload_string();

//if(String(pub.topic()) == "smartbus/inputs") // проверяем из нужного ли нам топика пришли данные 
//{
//    Serial.print(" ok ");
//    Serial.println(payload);
//}

if(String(pub.topic()) == "smartbus/outputs") // проверяем из нужного ли нам топика пришли данные 
{
//    Serial.print(" ok ");
//    Serial.println(payload);
    _gtv2 = payload.toInt(); // преобразуем полученные данные в тип integer
}
}
/////////////////////////////////////////////

int _modbusMasterDataTable_4_reg_1[2];
int _modbusMasterAddressTable_4_reg_1[2] = {5, 7};
byte _modbusMasterBufferSize = 0;
byte _modbusMasterState = 1;
long _modbusMasterSendTime;
byte _modbusMasterLastRec = 0;
long _modbusMasterStartT35;
byte _modbusMasterBuffer[64];
byte _modbusMasterCurrentReg = 0;
byte _modbusMasterCurrentVariable = 0;
struct _modbusMasterTelegramm {
  byte slaveId;        
  byte function;        
  int startAddres;   
  int numbeRegs;   
  int valueIndex;
};
_modbusMasterTelegramm _modbusTelegramm;
long _startTimeMasterRegs[1];
long _updateTimeMasterRegsArray[] = {1000};
byte _readWriteMasterVars[] = {3};
const unsigned char _modbusMaster_fctsupported[] = {3, 6, 16};


void setup()
{
Serial.begin(9600);
//Serial.println();
Serial100.begin(9600);
pinMode(enPin, OUTPUT);
digitalWrite(enPin, LOW);
for(int i=0; i<1; i++) {_startTimeMasterRegs[i] =  millis();}
}

byte temp1 = 255;
byte temp2 = 255;
char incomingByte;   // переменная для хранения полученного байта

void loop()
{
byte _tempVariable_byte;

_gtv1 = (_modbusMasterDataTable_4_reg_1[0]);
_tempVariable_byte = _gtv2;
if (! (_tempVariable_byte == _modbusMasterDataTable_4_reg_1[1])) {_readWriteMasterVars[0] = 6;};
_modbusMasterDataTable_4_reg_1[1] = _tempVariable_byte;

/////////////////////////////////////
switch ( _modbusMasterState ) {
    case 1:
      _nextModbusMasterQuery();
      break;
    case 2:
      pollModbusMaster();
      break;
  }
 
// подключаемся к wi-fi
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("...");
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
        return;
    Serial.println("WiFi connected");
}

// подключаемся к MQTT серверу
if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
        Serial.println("Connecting to MQTT server");
        if (client.connect(MQTT::Connect("smartBUSClient").set_auth(mqtt_user, mqtt_pass))) {
            Serial.println("Connected to MQTT server");
            client.set_callback(callback);
            client.subscribe("smartbus/inputs"); // подписывааемся на топик smartbus/temp с данными о температуре
            client.subscribe("smartbus/outputs"); // подписывааемся на топик smartbus/led с данными для светодиода
        } 
//        else {
//            Serial.println("Could not connect to MQTT server"); 
//        }
    }
    if (client.connected()){
        client.loop();
        if (temp_gtv1 != _gtv1) {
            temp_gtv1 = _gtv1;
            //Serial.println(String(_gtv1));
            client.publish("smartbus/inputs",String(_gtv1)); // отправляем в топик значение входов
        }
        if (temp_gtv2 != _gtv2) {
            temp_gtv2 = _gtv2;
            //Serial.println(String(_gtv2));
            client.publish("smartbus/outputs",String(_gtv2)); // отправляем в топик значение выходы
        }
    }
}

}


bool _isTimer(unsigned long startTime, unsigned long period )
  {
  unsigned long currentTime;
currentTime = millis();
if (currentTime>= startTime) {return (currentTime>=(startTime + period));} else {return (currentTime >=(4294967295-startTime+period));}
  }
int modbusCalcCRC(byte length, byte bufferArray[])
{
  unsigned int temp, temp2, flag;
  temp = 0xFFFF;
  for (unsigned char i = 0; i < length; i++) {
    temp = temp ^ bufferArray[i];
    for (unsigned char j = 1; j <= 8; j++) {
      flag = temp & 0x0001;
      temp >>= 1;
      if (flag)   temp ^= 0xA001;
    }
  }
  temp2 = temp >> 8;
  temp = (temp << 8) | temp2;
  temp &= 0xFFFF;
  return temp;
}
void _nextModbusMasterQuery()
{
_selectNewModbusMasterCurrentReg(_modbusMasterCurrentReg, _modbusMasterCurrentVariable);
if (_modbusMasterCurrentReg == 0)  return;
_createMasterTelegramm();
_modbusMasterSendQuery();
}
void _selectNewModbusMasterCurrentReg(byte oldReg, byte oldVar)
{
bool isNeeded = 1;
if (oldReg == 0) {_selectNewModbusMasterCurrentReg(1, 0); return;}
if (!(_isTimer  ((_startTimeMasterRegs[oldReg - 1]),(_updateTimeMasterRegsArray[oldReg -1])))) {isNeeded = 0;}
if( ! isNeeded ) {if(oldReg < 1) {_selectNewModbusMasterCurrentReg(oldReg+1, 0); return;} else {_modbusMasterCurrentReg = 0; _modbusMasterCurrentVariable = 0; return;}}
if (oldVar == 0) {_modbusMasterCurrentReg = oldReg; _modbusMasterCurrentVariable = 1; return;}
byte temp;
switch (oldReg) {
case 1:
temp = 2;
 break;
 }
if (oldVar < temp) {_modbusMasterCurrentReg = oldReg; _modbusMasterCurrentVariable = oldVar +1; return;}
_startTimeMasterRegs[oldReg -1] = millis();
if(oldReg < 1) { _selectNewModbusMasterCurrentReg(oldReg+1, 0); return;} 
_modbusMasterCurrentReg = 0; _modbusMasterCurrentVariable = 0; return;
}
void _createMasterTelegramm()
{
switch (_modbusMasterCurrentReg) {
case 1:
_modbusTelegramm.slaveId = 1;
switch (_modbusMasterCurrentVariable) {
case 1:
_modbusTelegramm.function = 3;
_modbusTelegramm.startAddres = 5;
_modbusTelegramm.numbeRegs = 1;
_modbusTelegramm.valueIndex = 0;
break;
case 2:
_modbusTelegramm.function = _readWriteMasterVars[0];
_modbusTelegramm.startAddres = 7;
_modbusTelegramm.numbeRegs = 1;
_modbusTelegramm.valueIndex = 1;
_readWriteMasterVars[0] = 3;
break;
}
break;
}
}
void _modbusMasterSendQuery()
{
int intTemp;
byte currentIndex = _modbusTelegramm.valueIndex;
  _modbusMasterBuffer[0]  = _modbusTelegramm.slaveId;
  _modbusMasterBuffer[1] = _modbusTelegramm.function;
  _modbusMasterBuffer[2] = highByte(_modbusTelegramm.startAddres );
  _modbusMasterBuffer[3] = lowByte( _modbusTelegramm.startAddres );
  switch ( _modbusTelegramm.function ) {
case 3:
 _modbusMasterBuffer[4] = highByte(_modbusTelegramm.numbeRegs );
      _modbusMasterBuffer[5] = lowByte( _modbusTelegramm.numbeRegs );
      _modbusMasterBufferSize = 6;
      break;
case 6:
 switch ( _modbusMasterCurrentReg ) {
case 1 :
intTemp = _modbusMasterDataTable_4_reg_1[currentIndex];
break;
}
      _modbusMasterBuffer[4]      = highByte(intTemp);
      _modbusMasterBuffer[5]      = lowByte(intTemp);
      _modbusMasterBufferSize = 6;
      break;
}
  _modbusMasterSendTxBuffer();
  _modbusMasterState = 2;
}
void _modbusMasterSendTxBuffer()
{
 byte i = 0;
int crc = modbusCalcCRC( _modbusMasterBufferSize, _modbusMasterBuffer );
  _modbusMasterBuffer[ _modbusMasterBufferSize ] = crc >> 8;
_modbusMasterBufferSize++;
 _modbusMasterBuffer[ _modbusMasterBufferSize ] = crc & 0x00ff;
 _modbusMasterBufferSize++;
digitalWrite(enPin, 1 );
delay(5);
Serial100.write( _modbusMasterBuffer, _modbusMasterBufferSize );
digitalWrite(enPin, 0 );
Serial100.flush();
  _modbusMasterBufferSize = 0;
  _modbusMasterSendTime = millis();
}
void pollModbusMaster()
{
if (_modbusTelegramm.slaveId == 0) {   _modbusMasterState = 1;   return;}
  if (_isTimer(_modbusMasterSendTime, 1000)) {
    _modbusMasterState = 1;
    return;
  }
  byte avalibleBytes = Serial100.available();
  if (avalibleBytes == 0) return;
  if (avalibleBytes != _modbusMasterLastRec) {
    _modbusMasterLastRec = avalibleBytes;
    _modbusMasterStartT35 = millis();
    return;
  }
  if (!(_isTimer(_modbusMasterStartT35, 5 ))) return;
  _modbusMasterLastRec = 0;
  byte readingBytes = _modbusMasterGetRxBuffer();
  if (readingBytes < 5) {
    _modbusMasterState = 1;
    return ;
  }
byte exeption = validateAnswer();
  if (exeption != 0) {
 _modbusMasterState = 1;
    return;
  }
 switch ( _modbusMasterBuffer[1] ) {
 case 3:
 get_FC3(4);
break;
}
  _modbusMasterState = 1;
  return;
}
byte _modbusMasterGetRxBuffer()
{
boolean bBuffOverflow = false;digitalWrite(enPin, LOW );
 _modbusMasterBufferSize = 0;
  while (Serial100.available() ) {
    _modbusMasterBuffer[ _modbusMasterBufferSize ] = Serial100.read();
    _modbusMasterBufferSize ++;
    if (_modbusMasterBufferSize >= 64) bBuffOverflow = true;
  }
  if (bBuffOverflow) {return -3;}
  return _modbusMasterBufferSize;
}
byte validateAnswer()
{
uint16_t u16MsgCRC =    ((_modbusMasterBuffer[_modbusMasterBufferSize - 2] << 8) | _modbusMasterBuffer[_modbusMasterBufferSize - 1]);
  if ( modbusCalcCRC( _modbusMasterBufferSize - 2,_modbusMasterBuffer ) != u16MsgCRC ) { return 255; }
  if ((_modbusMasterBuffer[1] & 0x80) != 0) {return _modbusMasterBuffer[2] ;}
  boolean isSupported = false;
  for (byte i = 0; i < sizeof( _modbusMaster_fctsupported ); i++) {
    if (_modbusMaster_fctsupported[i] == _modbusMasterBuffer[1]) {
      isSupported = 1;

      break;
    }
  }
  if (!isSupported) {return 1;}
  return 0;
}
void get_FC3(byte table)
{
int currentIndex = _modbusTelegramm.valueIndex;
  byte currentByte = 3;
int value;
  for (int i = 0; i < _modbusTelegramm.numbeRegs; i++) {
   value = word( _modbusMasterBuffer[ currentByte],   _modbusMasterBuffer[ currentByte + 1 ]);
switch ( _modbusMasterCurrentReg ) {
case 1 :
if(table == 3) {} else {_modbusMasterDataTable_4_reg_1[currentIndex + i] =value;}
break;
}
    currentByte += 2;
  } 
}
