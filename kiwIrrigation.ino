#define BLYNK_TEMPLATE_ID "TMPL4-hmWzt71"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "iRuxWZTiWUNRcNzI56ZtzMYgUrDM-vCS"


/**************************************************************
 *
 * For this example, you need to install Blynk library:
 *   https://github.com/blynkkk/blynk-library/releases/latest
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 **************************************************************
 *
 * Blynk is a platform with iOS and Android apps to control
 * Arduino, Raspberry Pi and the likes over the Internet.
 * You can easily build graphic interfaces for all your
 * projects by simply dragging and dropping widgets.
 *
 * Blynk supports many development boards with WiFi, Ethernet,
 * GSM, Bluetooth, BLE, USB/Serial connection methods.
 * See more in Blynk library examples and community forum.
 *
 *                http://www.blynk.io/
 *
 * Change GPRS apm, user, pass, and Blynk auth token to run :)
 **************************************************************/

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space

// Default heartbeat interval for GSM is 60
// If you want override this value, uncomment and set this option:
// #define BLYNK_HEARTBEAT 30

// Please select the corresponding model

// #define SIM800L_IP5306_VERSION_20190610
#define SIM800L_AXP192_VERSION_20200327
// #define SIM800C_AXP192_VERSION_20200609
// #define SIM800L_IP5306_VERSION_20200811

#include "utilities.h"

// Select your modem:
#define TINY_GSM_MODEM_SIM800

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);


// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char auth[] = "iRuxWZTiWUNRcNzI56ZtzMYgUrDM-vCS";

// Sensor pins
#define VIBRATION_PIN_ONE 34
#define VIBRATION_PIN_TWO 35
#define VIBRATION_PIN_THREE 32 //?

// Control pins
#define RELAY_VALVE_PIN 14
#define RELAY_STARTER_PIN 33

// Thresholds
#define VIBRATION_THRESHOLD 10

// Virtual Pins
#define VIRT_STARTER V0
#define VIRT_VALVE V1
#define VIRT_RESET V2
#define VIRT_STARTENG V3
#define VIRT_VIBRATION V4
#define VIRT_KILLENG V5
#define VIRT_LOG V6 
#define VIRT_DIAL V7
#define VIRT_COMMAND V8
#define VIRT_SUBMIT_COMMAND V9

#define MAX_ENGINE_RUNTIME 2 //hours

// Control values
int timerID;
int engineStatus = 0;
int valveStatus = 0;
int timestamp = 0;
int disableVib1 = 0;
int disableVib2 = 0;
int disableVib3 = 1;
BlynkTimer timer;
uint64_t sleepTime= 65760; //18 hours in seconds + 16 min
uint64_t awakeTime= 21600000; //6 hours in ms
int dial = 0;
int command = 0; //locked and loaded
    // 0 is no command
    // 1 set sleep time according to dial
    // 2 set awake time according to dial
    // 3 check vibration - general status
    // 4 disable sensor 1
    // 5 disable sensor 2
    // 6 disable sensor 3


void logStatus(char* message){
  Serial.printf(message);
  Blynk.virtualWrite(VIRT_LOG, message);
}


int readVibration() {
  char buf[100];
  int oneBuf = 0;
  int twoBuf = 0;
  int threeBuf = 0;
  for(int j=0; j<100; j++){
    oneBuf += digitalRead(VIBRATION_PIN_ONE);
    twoBuf += digitalRead(VIBRATION_PIN_TWO);
    threeBuf += digitalRead(VIBRATION_PIN_THREE);
    delay(5);
  }
  sprintf(buf,"Vibration read1: (%d)%d, read2: (%d)%d, read3: (%d)%d\n",
                disableVib1, oneBuf, disableVib2, twoBuf, disableVib3, threeBuf);
  logStatus(buf);
  oneBuf = oneBuf>VIBRATION_THRESHOLD ? 1 : 0;
  twoBuf = twoBuf>VIBRATION_THRESHOLD ? 1 : 0;
  threeBuf = threeBuf>VIBRATION_THRESHOLD ? 1 : 0;
  return (oneBuf||disableVib1) && 
        (twoBuf||disableVib2) && 
        (threeBuf||disableVib3);
}


void openValve(){
  logStatus("Opening valve\n");
  digitalWrite(RELAY_VALVE_PIN, 1);
  Blynk.virtualWrite(VIRT_VALVE, 1);
  valveStatus = 1;
}

void closeValve(){
  logStatus("Closing valve\n");
  digitalWrite(RELAY_VALVE_PIN, 0);
  Blynk.virtualWrite(VIRT_VALVE, 0);
  valveStatus = 0;
}

void tryStarter(int actuationTime, int pause){
  logStatus("Trying starter\n");
  Blynk.virtualWrite(VIRT_STARTER, 1);
  digitalWrite(RELAY_STARTER_PIN, 1);
  delay(actuationTime);
  Blynk.virtualWrite(VIRT_STARTER, 0);
  digitalWrite(RELAY_STARTER_PIN, 0);
  delay(pause);
}

int startEngine() {
  logStatus("---Starting-Engine---\n");
  openValve();
  delay(10000); //wait for fuel pressure to equalise
  tryStarter(2000, 5000);
  if(readVibration()!=1){ //retrying if first try was not successful
    tryStarter(3000,3000);
  } else {
    return 1;
  }
  Blynk.logEvent("enginestarted");
  return readVibration();
}

void killEngine() {
  logStatus("Killing engine\n");
  closeValve();
  delay(2000);
  //start new timer to run every half a minute to 
  //check whether the engine has stopped running
  timer.deleteTimer(timerID);
  timerID = timer.setInterval(15000, engineChoke);
}

void reset() {
  logStatus("Setting all pins to closed state (reset)\n");
  Blynk.virtualWrite(VIRT_STARTER, 0); //zero starter
  digitalWrite(RELAY_STARTER_PIN,0);
  engineStatus = 0;
  timer.deleteTimer(timerID);
  timerID = timer.setInterval(1800000L, timerCheckupFunction); //half an hour
  Blynk.virtualWrite(VIRT_VIBRATION, 0);
  Blynk.virtualWrite(VIRT_VALVE, 0); //close valve
  digitalWrite(RELAY_VALVE_PIN,0);
  valveStatus = 0;
}

void goToSleepFor(int seconds){
  uint64_t timeToSleep = seconds * 1000000ull; 
  esp_sleep_enable_timer_wakeup(timeToSleep);
  logStatus("Goodnight fellas\n");
  delay(1000);
  esp_deep_sleep_start();
}

void setAwakeDurationTime(int time){
    char buf[100];
    sprintf(buf, "Awake duration set to %d hours\n",time);
    logStatus(buf);
    awakeTime = time*3600*1000; 
}

void setSleepDurationTime(int time){
    char buf[100];
    sprintf(buf, "Sleep duration set to %d hours\n",time);
    logStatus(buf);
    sleepTime = time*3600;
}

void goToSleep(){
    char buf[100];
    if(engineStatus==1 || readVibration()==1) {
      logStatus("Can't go to sleep when engine is running\n");
      return;
    }else{
      sprintf(buf, "Going to sleep now for %d hours\n",sleepTime/3600);
      logStatus(buf);
      goToSleepFor(sleepTime);
    }
}

void engineChoke() {
  logStatus("Waiting for vibrations to stop\n");
  if(readVibration()!=1 && readVibration()!=1){ //double check
    timer.deleteTimer(timerID);
    openValve();
    logStatus("Waiting for fuel pressure to equalize\n");
    delay(10000);
    closeValve();
    Blynk.logEvent("engineclosed");
    if(readVibration()==1){
        killEngine();
        return;
    }
    engineStatus = 0;
    Blynk.virtualWrite(VIRT_VIBRATION, 0);
    timerID = timer.setInterval(1800000L, timerCheckupFunction);
  }
}

void statusCheck(){
  readVibration();
}

void timerCheckupFunction() {
  char buf[100];
  sprintf(buf,"Uptime %d\n",millis());
  logStatus(buf);
  if(engineStatus==1){
    if(readVibration()==0){ //double check
    delay(2000);
      if(readVibration()==0){
        logStatus("Unexpected Shutdown\n");
        reset();
        return;
      }
    }
  }
  Blynk.virtualWrite(VIRT_VIBRATION, readVibration());
  printf("Uptime since timestamp: %d\n",int(millis()-timestamp));
  if((int(millis()-timestamp)>3600000*MAX_ENGINE_RUNTIME) && (engineStatus==1)){ // 2 hours in ms
    logStatus("Time limit passed, shutting down engine");
    killEngine();
  }
  if(Blynk.connected()==false && (engineStatus == 1)){
    logStatus("Unexpected disconnection, shutting down engine");
    killEngine();
    reset();
  }
  if(millis()>awakeTime) {
    logStatus("Reached max awake time, going to sleep now\n");
    goToSleep();
  }
}


BLYNK_WRITE(VIRT_STARTER) // Starter direct control
{
  if(param.asInt() == 1)
  {
    if(engineStatus == 1){
      logStatus("Cant use that while engine is running\n");
    } else {
      logStatus("Trying Starter\n");
      digitalWrite(RELAY_STARTER_PIN, 1);
    }
  }
  else
  {
    logStatus("Closing Starter\n");
    digitalWrite(RELAY_STARTER_PIN,0);
  }
}


BLYNK_WRITE(VIRT_VALVE) // Valve direct control
{
  if(param.asInt() == 1)
  {
    if(valveStatus == 1){
      logStatus("Valve already open\n");
    } else {
      logStatus("Opening Valve\n");
      digitalWrite(RELAY_VALVE_PIN, 1);
      valveStatus=1;
    }
  }
  else
  {
    if(valveStatus == 0){
      logStatus("Valve already closed\n");
    } else {
      logStatus("Closing Valve\n");
      digitalWrite(RELAY_VALVE_PIN, 0);
      valveStatus=0;
    }
  }
}

BLYNK_WRITE(VIRT_RESET){ //reset pins
  if(param.asInt() == 1){
    reset();
    logStatus("Restarting...\n");
    ESP.restart();
  }
}

BLYNK_WRITE(VIRT_STARTENG){ //start engine procedure
  if(param.asInt()==1){
    if(engineStatus==1) {
      logStatus("Engine already running\n");
    }else{
      if(startEngine()==0){
        logStatus("Engine start failed\n");
        engineStatus=0;
        Blynk.virtualWrite(VIRT_VIBRATION, 0);
      }else{
        logStatus("Engine start successful\n");
        engineStatus=1;
        timestamp = millis();
        Blynk.virtualWrite(VIRT_VIBRATION, 1);
      }
    }
  }
}

BLYNK_WRITE(VIRT_KILLENG){ //kill engine
  if(param.asInt()==1){
    if(engineStatus==1) {
      logStatus("Killing Engine\n");
      killEngine();
    }else{
      logStatus("Engine not running\n");
    }
  }
}

BLYNK_WRITE(VIRT_DIAL) {
  dial = param.asInt();
  char buf[100];
  sprintf(buf,"Dial set to %d\n",dial);
  logStatus(buf);
  return;
}

BLYNK_WRITE(VIRT_COMMAND) {
  command = param.asInt();
  char buf[100];
  switch (command) {
    case 0:
      sprintf(buf,"Command set to \"no command\"\n");
      break;
    case 1:
      sprintf(buf,"Command set to \"set sleep time\"\n");
      break;
    case 2:
      sprintf(buf,"Command set to \"set awake time\"\n");
      break;
    case 3:
      sprintf(buf,"Command set to \"check vibration\"\n");
      break;
    case 4:
      sprintf(buf,"Command set to \"disable sensor 1\"\n");
      break;
    case 5:
      sprintf(buf,"Command set to \"disable sensor 2\"\n");
      break;
    case 6:
      sprintf(buf,"Command set to \"disable sensor 3\"\n");
      break;
    default:
      sprintf(buf,"Command set to \"unknown\"\n");
  }
  logStatus(buf);
  return;
}

BLYNK_WRITE(VIRT_SUBMIT_COMMAND){ 
    if(param.asInt()==0){
      return;
    }
    if(command==0){
        logStatus("Doing nothing often leads to the very best something\n");
    } else if(command==1){ //set sleep time
        setSleepDurationTime(dial);
    } else if(command==2){ //set awake time
        setAwakeDurationTime(dial);
    } else if(command==3){ //status check
        statusCheck();
    } else if(command==4){ //deactivate sensor 1
        disableVib1 = 1;
        logStatus("Deactivating sensor 1\n");
    } else if(command==5){ //deactivate sensor 2
        disableVib2 = 1;
        logStatus("Deactivating sensor 2\n");
    } else if(command==6){ //deactivate sensor 3
        disableVib3 = 1;
        logStatus("Deactivating sensor 3\n");
    } else if(command==7){ //Gotosleep
        logStatus("Attempting to sleep\n");
        goToSleep();
    } else {
        logStatus("Unknown command. Try crying for help\n");
    }
    return;

}

BLYNK_CONNECTED() //startup init reset
{
  reset();
}

void setup()
{
    // Set console baud rate
    SerialMon.begin(115200);

    delay(10);

    // Setup input pins
    pinMode(VIBRATION_PIN_ONE, INPUT);
    pinMode(VIBRATION_PIN_TWO, INPUT);

    // Setup output pins
    pinMode(RELAY_STARTER_PIN,OUTPUT);
    pinMode(RELAY_VALVE_PIN,OUTPUT);

    // Set outputs to high (hope nothing starts prematurely)
    digitalWrite(RELAY_STARTER_PIN, LOW); 
    digitalWrite(RELAY_VALVE_PIN, LOW);

    setupModem();

    SerialMon.println("Wait...");

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    delay(6000);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();
    // modem.init();

    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem Info: ");
    SerialMon.println(modemInfo);

    // Unlock your SIM card with a PIN
    //modem.simUnlock("1234");

    Blynk.begin(auth, modem, apn, gprsUser, gprsPass);

    //timerID = timer.setInterval(300000L, timerCheckupFunction); //happens on reset


}

void loop()
{
    Blynk.run();

    timer.run(); 
    
    }
