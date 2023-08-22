#include <Arduino.h>
#include <WiFiManager.h> 
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

/*----------------------------------------------------------------*/
#define MAX_STRING_LENGTH 100
String dbpath = "";
String cusfcmtoken;
String pathquery;
const int switch_in  = 18;
int box_open = HIGH;
bool taskCompleted = false;
int numberup = 1;
int lastBoxState = HIGH;
int boxState; 
unsigned long lastDebounceTime1 = 0;
unsigned long debounceDelay = 50;

const int WiFi_rst = 0; //nut boot, reset wifi eeprom
int boot_press = LOW;
int lastbootpress = HIGH;
int bootpressState; 
unsigned long lastDebounceTime0 = 0;
unsigned long releasedTime  = 0;
unsigned long debounceDelay0 = 3000; 
/*----------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////
//                                                                  //
//                         CHANGE DATA HERE                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////
#define DATABASE_URL "/"
#define FIREBASE_PROJECT_ID "e"
#define FIREBASE_CLIENT_EMAIL ""
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY---

/*----------------------------------------------------------------*/
FirebaseData esp32box;
FirebaseAuth auth;
FirebaseConfig config;
/*----------------------------------------------------------------*/
#define JSON_CONFIG_FILE "/save_config.json" //save user name of customer, save token ?
bool shouldSaveConfig = false;
char customer[100] = "email cus";

/*----------------------------------------------------------------*/
void saveConfigFile()// Save Config in JSON format
{
  Serial.println("  Saving configuration...");
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["customer"] = customer;
  //json["fcmtoken"] = fcmtoken;
 
  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    // Error, file did not open
    Serial.println("  failed to open config file for writing");
  }
 
  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F(" Failed to write to file"));
  }
  // Close file
  configFile.close();
}
bool loadConfigFile()  // Load existing configuration file
{

  // Read configuration from FS json
  //Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    //Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
     // Serial.println("  Reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        //Serial.println("  Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("  Loaded and saved username");
 
         // strcpy(fcmtoken, json["fcmtoken"]);
          strcpy(customer, json["customer"]);
 
          return true;
        }
        else
        {
          // Error loading JSON data
          Serial.println("  Failed to load username");
        }
      }
    }
  }
  else
  {
    // Error mounting file system
    Serial.println("  Failed to mount FS");
  }
 
  return false;
}
void saveConfigCallback()// Callback notifying us of the need to save configuration
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}
/*----------------------------------------------------------------*/
// Gets called when WiFiManager enters configuration mode

void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
  //  if you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());
  //     entered config mode, make led toggle faster
  //    ticker.attach(1, tick);
}
/*-----------------------------------------------------------------------------------------------------*/

void tokenStatusCallback1(TokenInfo info)
{
    /** fb_esp_auth_token_status enum
     * token_status_uninitialized,
     * token_status_on_initialize,
     * token_status_on_signing,
     * token_status_on_request,
     * token_status_on_refresh,
     * token_status_ready,
     * token_status_error
     */
    if (info.status == token_status_error)
    {
        Serial_Printf("Token info: type = %s, status = %s\n", getTokenType(info), getTokenStatus(info));
        Serial_Printf("Token error: %s\n", getTokenError(info).c_str());
    }
    else
    {
        Serial_Printf("Token info: type = %s, status = %s\n", getTokenType(info), getTokenStatus(info));
    }
}

/*-----------------------------------------------------------------------------------------------------*/
void setup()
{
  Serial.begin(115200);
    //load data from json saved in fs
  bool forceConfig = false;
  Serial.println("0 - Check data config saved");
  bool spiffsSetup = loadConfigFile();
  
/*----------------------------------------------------------------*/
//1 connect wifi
    WiFi.mode(WIFI_OFF);  // The Wifi is started either by the WifiManager or by user invoking "begin"  
   // pinMode(SS_ENABLE_PIN, OUTPUT);
    //digitalWrite(SS_ENABLE_PIN, HIGH); 
    WiFi.persistent(true);
    WiFiManager wfm;
    Serial.println("1 - WifiManager enabled.");
   
   // wfm.setDebugOutput(false);
   // wfm.resetSettings();
    WiFiManagerParameter custom_email("Email", "EmailUser", "User Email", 50);
   // WiFiManagerParameter custom_token("Token", "TokenUser", "User Token", 200);
    wfm.addParameter(&custom_email);
    //wfm.addParameter(&custom_token);

    wfm.setAPCallback(configModeCallback);
    wfm.setSaveConfigCallback(saveConfigCallback);
    bool res = wfm.autoConnect(); // password protected ap
 
    if(!res) {
        Serial.println("  Failed to connect");
        delay(3000);
        ESP.restart();
    }
        //if you get here you have connected to the WiFi    
    Serial.println("  WiFi connected");
    Serial.print("  IP address: ");
    Serial.println(WiFi.localIP());
/*----------------------------------------------------------------*/
//AP  
  if (!spiffsSetup)
  {
    Serial.println("  Forcing config mode as there is no saved config");
    forceConfig = true;
  }
  if (forceConfig)
    // Run if we need a configuration
  {
    if (!wfm.startConfigPortal())
    {
      Serial.println("  failed to connect smart config and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    if (!wfm.autoConnect())
    {
      Serial.println("  failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }
  strncpy(customer, custom_email.getValue(), sizeof(customer));
  Serial.print("  Email customer: ");
  Serial.println(customer);
//  strncpy(fcmtoken, custom_token.getValue(), sizeof(fcmtoken));
 // Serial.print("  fcmtoken: ");
//  Serial.println(fcmtoken);

  if (shouldSaveConfig)
  {
    saveConfigFile();
  }

/*----------------------------------------------------------------*/
//2 Connect to Firebase
    Serial.printf("2 - Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
    config.database_url = DATABASE_URL;
    config.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
    config.service_account.data.project_id = FIREBASE_PROJECT_ID;
    config.service_account.data.private_key = PRIVATE_KEY;
    config.token_status_callback = tokenStatusCallback1; 

    esp32box.setResponseSize(4096);
    // Firebase.begin(DATABASE_URL, DATABASE_SECRET);
    Firebase.begin(&config, &auth);
    delay(300);
    Firebase.reconnectWiFi(false);
     if (Firebase.ready()){
        Serial.println("  Firebase ready");
     }else{
        Serial.println("  Firebase not ready");
     }
  
/*----------------------------------------------------------------*/ 
//3 Load data path RTDB Firebase, path = user email
    Serial.println("3 - Save username");
    pinMode(switch_in, INPUT_PULLUP);
    pinMode(WiFi_rst, INPUT);
    bool spiffsSetup1 = loadConfigFile();
   
    char buf_cus[100];
    strcpy(buf_cus, customer);
    String path = String(buf_cus);
    //cusfcmtoken = String(fcmtoken);
    
    Serial.println("4 - Monitoring box status");

/*----------------------------------------------------------------*/

/*----------------------------------------------------------------*/

/*----------------------------------------------------------------*/   
}
void loop()
{
   box_open = digitalRead(switch_in);
   boot_press = digitalRead(WiFi_rst);
  /*-------------------------------------------------------------*/
  lastDebounceTime0 = millis();
  while (digitalRead(boot_press) == LOW) {
    // Wait till boot button is pressed 
  }
   long pressDuration = releasedTime - lastDebounceTime0;

    if ( millis() - lastDebounceTime0 >= 3000 ) {
      Serial.println("Delete config file");
      deleteFile(SPIFFS, "/save_config.json");
      delay(500);
    }

  /*-------------------------------------------------------------*/
   if (box_open != lastBoxState ){
          lastDebounceTime1 = millis();
   }
   if ((millis() - lastDebounceTime1) > debounceDelay) {
      if (box_open != boxState){ //box is open
          
          boxState = box_open;
          if(boxState == 0){
              Serial.println("* The box is open *");
              send_data_to_firebase();
              delay(100);    
              querydata();
          }
       }      
    }
    lastBoxState = box_open;

}
