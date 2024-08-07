// Bugs to fix:
// X TODO: Status LED changes to yellow during established conection
// 

#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <esp_now.h>
#include <WiFi.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>


// Structure to send data, Must match the receiver structure
typedef struct struct_message {
    int actionID;
    float value;
} struct_message;
struct_message TXdata; // Create a struct_message called data


typedef struct temp {
  int actionID;
  float sens1;
  float sens2;
  float sens3;
  float sens4;
  float sens5;
  float sens6;
  float sens7;
  float sens8;
  float sens9;
} temp;
temp tempData; // Create a struct_message called data

//MARK: USER VARIABLES
int GCTID               = 1;            //1 to 4
char fileName[25]       = "/data_GCT1.csv";   //file name for the data file on the SD card
int pingInterval        = 1000;               //500 to inf
uint8_t masterAdress[]  = {0x48, 0xE7, 0x29, 0x8C, 0x6B, 0x5C};

//MARK: PIN DEFINITIONS
#define oneWireBus  4
#define SD_CS       5
#define LED_PIN     2

int SD_CS_PIN   = 5;    // Chip Select pin //MARK: SPI_PIN
int SCK_PIN     = 18;   // Clock pin
int MISO_PIN    = 19;   // Master In Slave Out pin
int MOSI_PIN    = 23;   // Master Out Slave In pin

//clock SDA = 21
//clock SCL = 22


//MARK: SYSTEM VARIABLES
//Do not touch these!!!
char filename[25] = "";
#define NUM_SENSORS 9
char timestamp[19];
char line[1000];
#define numMasters 1
File file;
unsigned long sinceLastConnection = 0;
bool loggingStatus = false;
bool callbackEnabled = true;

Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800);  // Create an instance of the Adafruit_NeoPixel class

esp_now_peer_info_t peerInfo[numMasters];

RTC_DS3231 rtc;


struct SensorData {
  float temperature;
};

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Create an array of SensorData structures
SensorData sensorData[NUM_SENSORS];


void blinkLED(int red, int green, int blue, int blinkIntervall) {
    static unsigned long previousMillis = 0;
    static bool ledState = false;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= blinkIntervall) {
        previousMillis = currentMillis;
        ledState = !ledState;

        if (ledState) {
            strip.setPixelColor(0, strip.Color(red, green, blue));
        } else {
            strip.setPixelColor(0, strip.Color(0, 0, 0));
        }

        strip.show();
    }
}


void updateStatusLED(int status, int blinkIntervall = 1000){ //MARK: Update status LED
    switch (status)
    {

    case 0:
        strip.setPixelColor(0, strip.Color(0, 0, 0));       // Turn off the LED
        break;

    case 1:
        strip.setPixelColor(0, strip.Color(255, 100, 0));   // Constant yellow
        break;
    
    case 2:
        blinkLED(0, 255, 0, blinkIntervall);                // Blink the LED in green
        break;
    
    case 3:
        strip.setPixelColor(0, strip.Color(0, 255, 0));     // Constant green
        break;

    case 4:
        strip.setPixelColor(0, strip.Color(255, 0, 0));     // Constant red
        break;

    case 5:
        blinkLED(255, 0, 0, blinkIntervall);                // Blink the LED in red
        break;

    case 6:
        blinkLED(255, 100, 0, blinkIntervall);              // Blink the LED in yellow
        break;
    
    case 7://TODO: check, why it is not working (blinking red...) 
        for (int i = 0; i < 10; i++) {
          strip.setPixelColor(0, strip.Color(255, 0, 0));
          strip.show();
          delay(100);
          strip.setPixelColor(0, strip.Color(0, 0, 0));
          strip.show();
          delay(100);
        }
        break;
    
    default:
        break;
    }

    strip.show();
}


void get_temperature() {
  sensors.requestTemperatures();
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensorData[i].temperature = sensors.getTempCByIndex(i);
  }
}


const char* get_timestamp() {
    DateTime now = rtc.now();
    sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    return timestamp;
}

void print_temperature() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(" temperature: ");
        Serial.println(sensorData[i].temperature);
    }
    Serial.println();
}


void send_data(int actionID, float value){
    TXdata.actionID = actionID;
    TXdata.value = value;
    esp_err_t result = esp_now_send(masterAdress, (uint8_t *) &TXdata, sizeof(TXdata));
    Serial.print("Send: ");
    Serial.print(actionID);
    Serial.print(", ");
    Serial.println(value);
    //delay(1000);
}


String tempToString(String timestamp) {//MARK: To String
    String data = "";
    data += timestamp + "," + String(GCTID) + ",1," + String(tempData.sens1) + "\n";
    data += timestamp + "," + String(GCTID) + ",2," + String(tempData.sens2) + "\n";
    data += timestamp + "," + String(GCTID) + ",3," + String(tempData.sens3) + "\n";
    data += timestamp + "," + String(GCTID) + ",4," + String(tempData.sens4) + "\n";
    data += timestamp + "," + String(GCTID) + ",5," + String(tempData.sens5) + "\n";
    data += timestamp + "," + String(GCTID) + ",6," + String(tempData.sens6) + "\n";
    data += timestamp + "," + String(GCTID) + ",7," + String(tempData.sens7) + "\n";
    data += timestamp + "," + String(GCTID) + ",8," + String(tempData.sens8) + "\n";
    data += timestamp + "," + String(GCTID) + ",9," + String(tempData.sens9) + "\n";
    return data;
    Serial.println(data);
}


void writeToSD(String dataString) { //MARK: Write to SD
    //Serial.print (dataString);
    file = SD.open(fileName, FILE_APPEND); // Open the file in append mode

  if (!file){       
    Serial.println("Failed to write to file");

    while (true)
    {
      callbackEnabled = false;
      updateStatusLED(5); //MARK: Implement error handling, currently, the will still acationally display readiness of operation
    }
  }

  file.print(dataString);
  file.close();
}


void sendTempData(){ //MARK: SEND TEMPERATURE DATA
    get_temperature();
    tempData.actionID = 2001;
    tempData.sens1 = sensorData[0].temperature;
    tempData.sens2 = sensorData[1].temperature;
    tempData.sens3 = sensorData[2].temperature;
    tempData.sens4 = sensorData[3].temperature;
    tempData.sens5 = sensorData[4].temperature;
    tempData.sens6 = sensorData[5].temperature;
    tempData.sens7 = sensorData[6].temperature;
    tempData.sens8 = sensorData[7].temperature;
    tempData.sens9 = sensorData[8].temperature;
    
    if (loggingStatus){
      writeToSD(tempToString(get_timestamp()));
    }
    
    esp_err_t result = esp_now_send(masterAdress, (uint8_t *) &tempData, sizeof(tempData));
}


void checkActionID(int actionID){
  switch (actionID) {
    case 3001://Full temperature data request
      Serial.println("Full temperature data request");
      sendTempData();
      break;

    case 1001:
      Serial.println("Connection test");
      sinceLastConnection = millis(); // reset the timer for the last connection
      break;

    case 1002:
      Serial.println("Logging in process");
      loggingStatus = true;
      break;
    
    case 1003:
      Serial.println("Not logging");
      loggingStatus = false;
      break;

    default:
      Serial.println("Action not found");
      break;
  }
}


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {  //registered callback
    if(!callbackEnabled){return;} //if the callback is disabled, return
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) { //registered callback
    if(!callbackEnabled){return;} //if the callback is disabled, return
    int receivedData;
    memcpy(&receivedData, incomingData, sizeof(receivedData));
    Serial.print("Received: ");
    Serial.println(receivedData);
    checkActionID(receivedData);
}


void setup() { //MARK: SETUP
  Serial.begin(115200);   // Start the Serial Monitor

  Serial.println("\n\n\nSELF CHECK:\n");

  //------------------ NEOPIXEL - INIT - BEGIN ------------------
  strip.begin(); // Initialize the NeoPixel library
  strip.show();  // Initialize all pixels to 'off'
  //------------------ NEOPIXEL - INIT - END ------------------
  updateStatusLED(1); 

  
  //--------------- DS18B20 - INIT - START -----------------
  sensors.begin();        // Start the DS18B20 sensors
  sensors.setResolution(12);
  


  //--------------- DS18B20 - INIT - END -----------------
  sensors.begin();        // Start the DS18B20 sensors
  sensors.setResolution(12);
  
  // Loop over the sensors and display the temperature for each one
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(i);
    if (temperature == -127.00) {
      Serial.print("Init Sensor #");
      Serial.print(i+1);
      Serial.println(":\t\tFailed");
      
      while(temperature == -127.00){
        updateStatusLED(5);
        temperature = sensors.getTempCByIndex(i);
      }

    } else {
      Serial.print("Init Sensor #");
      Serial.print(i+1);
      Serial.print(":\t\tSuccess (");
      Serial.print(temperature);
      Serial.println(" C)");
    }
  }

  //--------------- RTC - INIT - START -----------------
  if (! rtc.begin()) {
    Serial.println("Init RTC:\t\tFailed");
    updateStatusLED(4);
    while (true){}
    } else {
      Serial.print("Init RTC:\t\tSuccess (");
      Serial.print(get_timestamp());
      Serial.println(")"); 
  }

DateTime now = rtc.now(); // Declare "now" here
if (now.year() < 2024) {
  Serial.print("WARNING: RCT compromised");
  updateStatusLED(7);
}
    
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //uncomment to set the RTC to the compile time //Set RTC time
  
//--------------- RTC - INIT - END -----------------

//--------------- SD CARD - INIT - START -----------------
  while (!SD.begin(SD_CS_PIN)){
    updateStatusLED(5);
    Serial.println("SD Card Mount:\t\tFailed");
  }
  Serial.println("SD Card Mount:\t\tSuccess");

  strncpy(fileName, "/data_GCT1.csv", sizeof(fileName));
  File file = SD.open(fileName, FILE_APPEND);
    
  while(!file){
      Serial.println("Writing to file:\tFailed");
      updateStatusLED(5);
  }
  Serial.println("Writing to file:\tSuccess");
    
    file.close();
  //--------------- SD CARD - INIT - END  ------------------

  //--------------- ESP NOW - INIT - BEGIN -----------------
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Initialization:\tFailed");
        return;
    }else{
        Serial.println("ESP-NOW Initialization:\tSuccess");
    }

    esp_now_register_send_cb(OnDataSent); // Register callbacks
    esp_now_register_recv_cb(OnDataRecv);

    for (int i = 0; i < numMasters; i++) {
        memcpy(peerInfo[i].peer_addr, masterAdress, 6);
        peerInfo[i].channel = 0;  
        peerInfo[i].encrypt = false;
        
        if (esp_now_add_peer(&peerInfo[i]) != ESP_OK){
            Serial.println("ESP-NOW Peer Addition:\tFailed");
            return;
        }else{
            Serial.println("ESP-NOW Peer Addition:\tSuccess");
        }
    }
  //--------------- ESP NOW - INIT - END -----------------
  Serial.println("\nSELF-CHECK COMPLET\n\n\n");

  updateStatusLED(0);
}

void loop(){
  unsigned long currentConection = millis();
  if (currentConection - sinceLastConnection > pingInterval + 2000) {
    updateStatusLED(6); // Blink yellow
  }else{
    if (loggingStatus)
    {
      updateStatusLED(3); // Constant green
    }else{
      updateStatusLED(2); // Blink green
    }
  }
}