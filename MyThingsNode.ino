
//-------------------------------------------------------------------
// My Things Node (SWy, 30.12.2020)
//
// This sketch is forked from
// https://github.com/ch2i/arduino-node-lib
// which was forked from the main 
// https://github.com/TheThingsNetwork/arduino-node-lib
// 
// Contact: wyss (AT) superspider (DOT) net
// 
// - It uses Ultra Low Power techniques and consumes about 40uA sleep 
//   current on a battery powered Node. 
// - Cayenne is used as data transmission format.
// - Upstream of measured temperature to the cloud each 15 minutes.    
// - Double-press the reset button to upload a new sketch.
//-------------------------------------------------------------------
#include <TheThingsNode.h>
#include <TheThingsNetwork.h>
#include <EEPROM.h>

//#define USE_CAYENNE
#include <CayenneLPP.h>

// Set your AppEUI and AppKey
const char *appEui = "<EDIT HERE>";
const char *appKey = "<EDIT HERE>";

// Replace REPLACE_ME with TTN_FP_EU868 or TTN_FP_US915
#define freqPlan TTN_FP_EU868

#define loraSerial Serial1
#define debugSerial Serial

#define PORT_SETUP         1
#define PORT_BTN_PRESS    10
#define PORT_BTN_RELEASE  11
#define PORT_MOTION_START 20
#define PORT_MOTION_END   21
#define PORT_WATCHDOG     30
#define PORT_INTERVAL     31
#define PORT_LORA         32
#define PORT_TEMPERATURE  33

// Interval between send in seconds, so 900s = 15min
//#define CONFIG_INTERVAL ((uint32_t)60)
uint32_t config_interval = 0;       // in seconds

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);
TheThingsNode *node;

CayenneLPP lpp(24);

void(* resetFunc) (void) = 0; //declare reset function @ address 0

uint8_t fport = PORT_SETUP; // LoRaWAN port used 

void sendData(uint8_t port=PORT_SETUP, uint32_t duration=0);

// This is called on each interval we defined so mainly
// this is where we need to do our job
void interval(uint8_t wakeReason)
{
  uint8_t fport = PORT_INTERVAL;

  debugSerial.print(F("-- SEND: INTERVAL 0x"));
  debugSerial.print(wakeReason, HEX);

  if (wakeReason&TTN_WAKE_LORA) 
  {
    fport = PORT_LORA;
  }

  sendData(fport);
}

// This is called on each wake, every 8S or by an sensor/button interrupt
// if it's watchdog, we mainly do nothing (core IRQ, we don't have choice)
// but if it's an interupt it will ne bone ine loop
void wake(uint8_t wakeReason)
{
  ttn_color ledcolor = TTN_BLACK;
  uint8_t ledblink = 0 ;

  debugSerial.print(F("-- WAKE: 0x"));
  debugSerial.println(wakeReason, HEX);
  
  if (wakeReason & (TTN_WAKE_WATCHDOG | TTN_WAKE_INTERVAL)) {
    ledcolor = TTN_YELLOW;
    ledblink = 1;
    if (wakeReason & TTN_WAKE_WATCHDOG) {
      debugSerial.print(F(" Watchdog"));
    }
    if (wakeReason & TTN_WAKE_INTERVAL) {
      debugSerial.print(F(" INTERVAL"));
      ledblink++;
    }
  }
  if (wakeReason & TTN_WAKE_LORA) {
    debugSerial.print(F(" LoRa RNxxxx"));
    ledblink = 1;
    ledcolor = TTN_GREEN;
  }
  if (wakeReason & TTN_WAKE_BTN_PRESS) {
    debugSerial.print(F(" PRESS"));
  }
  if (wakeReason & TTN_WAKE_BTN_RELEASE) {
    debugSerial.print(F(" RELEASE"));
  }
  if (wakeReason & TTN_WAKE_MOTION_START) {
    ledblink = 1;
    ledcolor = TTN_RED;
    debugSerial.print(F(" MOTION_START"));
  }
  if (wakeReason & TTN_WAKE_MOTION_STOP)  {
    ledblink = 2;
    ledcolor = TTN_RED;
    debugSerial.print(F(" MOTION_STOP"));
  }
  if (wakeReason & TTN_WAKE_TEMPERATURE) {
    debugSerial.print(F(" TEMPERATURE"));
  }
  
  debugSerial.println();

  // Just if you want to see this IRQ with a LED
  // just uncomment, but take care, not LOW power
  //while (ledblink--) {
  //  node->setColor(ledcolor);
  //  delay(50);
  //  node->setColor(TTN_BLACK);
  //  delay(333);
  //}
}

void sleep()
{
  node->setColor(TTN_BLACK);

  // Just in case, disable all sensors
  // this couldn't hurt except time to do job
  node->configMotion(false);
  node->configLight(false);
  node->configTemperature(false);
}

void onButtonRelease(unsigned long duration)
{
  uint32_t timepressed = (uint32_t) duration;

  debugSerial.print(F("-- SEND: BUTTON: " ));
  debugSerial.print(timepressed);
  debugSerial.println(F(" ms"));

  /*
  // If button was pressed for more then 2 seconds
  if (timepressed > 2000) {
    // blink yellow led for 60 seconds
    // this will let us to upload new sketch if needed
    for (uint8_t i=0 ; i<60 ; i++) {
      node->setColor(TTN_YELLOW);
      delay(30);
      node->setColor(TTN_BLACK);
      delay(470);
    }
  }
  */

  // then send data
  sendData(PORT_BTN_RELEASE, timepressed);
}

//-------------------------------------------------------------------
// sendData() - sending data to gateway
//-------------------------------------------------------------------
void sendData(uint8_t port, uint32_t value)
{
  uint16_t volt;
  
  byte *bytes;
  byte payload[4];

  // Wake RN2483 
  ttn.wake();

  // Prepare cayenne payload
  lpp.reset();

  // Add temperature
#ifdef USE_CAYENNE
  lpp.addTemperature(PORT_TEMPERATURE,node->getTemperatureAsFloat());
#else
  int16_t temperature = round(node->getTemperatureAsFloat() * 100);
  bytes = (byte *)&temperature;
  payload[0] = bytes[1];
  payload[1] = bytes[0];
#endif

  // Read battery voltage
  volt = node->getBattery();
  debugSerial.print(F("Bat:\t"));
  debugSerial.print(volt);
  debugSerial.println(F("mV"));
#ifdef USE_CAYENNE  
  lpp.addAnalogInput(4, volt/1000.0);
#else
  bytes = (byte *)&volt;
  payload[2] = bytes[1];
  payload[3] = bytes[0];
#endif

  /*
  // This one is usefull when battery < 2.5V  below reference ADC 2.52V
  // because in this case reading are wrong, but you can use it 
  // as soon as VCC < 3.3V, 
  // when above 3.3V, since regulator fix 3.3V you should read 3300mV
  volt = node->getVCC();
  debugSerial.print(F("Vcc:\t"));
  debugSerial.print(volt);
  debugSerial.println(F("mV"));
  lpp.addAnalogInput(5, volt/1000.0);

  // Read value returned by RN2483 module
  volt = ttn.getVDD();
  debugSerial.print(F("Vcc:\t"));
  debugSerial.print(volt);
  debugSerial.println(F("mV"));
  lpp.addAnalogInput(6, volt/1000.0);

  // If button pressed, send press duration
  // please myDeviceCayenne add counter value type to
  // avoid us using analog values to send counters
  if (value) 
  {
    debugSerial.print(F("Button pressed for "));
    debugSerial.print(value);
    debugSerial.println(F("ms"));
	  lpp.addAnalogInput(10, value/1000.0);
  }
  */
  
  node->setColor(TTN_BLUE);
  // Send data
#ifdef USE_CAYENNE  
  ttn.sendBytes(lpp.getBuffer(), lpp.getSize(), port);
#else
  ttn.sendBytes(payload, sizeof(payload), port);
#endif  
}

//-------------------------------------------------------------------
// setup()
//-------------------------------------------------------------------
void setup()
{
  loraSerial.begin(57600);
  debugSerial.begin(9600);

  // Wait a maximum of 5s for Serial Monitor
  while (!debugSerial && millis() < 5000) {
    node->setColor(TTN_RED);
    delay(20);
    node->setColor(TTN_BLACK);
    delay(480);
  };

  config_interval = 60 * EEPROM.read(0);  // config_interval in seconds
  debugSerial.print("EEPROM interval configuration: " + String(config_interval/60));
  debugSerial.println(" minutes");
  
  // Config Node, Disable all sensors except temperature sensor
  // Check node schematics here
  // https://github.com/TheThingsProducts/node
  node = TheThingsNode::setup();
  node->configTemperature(false); // enable temp. sensor without alarm

  node->onWake(wake);
  node->onInterval(interval);
  node->onSleep(sleep);

  // We monitor just button release
  node->onButtonRelease(onButtonRelease);

  // Test sensors 
  node->showStatus();

  debugSerial.println(F("-- TTN: STATUS"));
  ttn.showStatus();

  // Each interval (with Lora Module and Serial IRQ)
  // Take care this one need to be called after any
  // first call to ttn.* so object has been instancied
  // if not &ttn will be null and watchdog will wake us
  node->configInterval(&ttn, config_interval*1000); 

  // Set callback for incoming messages
  ttn.onMessage(message);

  // Magenta during join, is joined then green else red
  debugSerial.println(F("-- TTN: JOIN"));
  node->setColor(TTN_BLUE);
  if (ttn.join(appEui, appKey)) {
    node->setColor(TTN_GREEN);
  } else {
    node->setColor(TTN_RED);
  }

  debugSerial.println(F("-- SEND: SETUP"));
  sendData(PORT_SETUP);

  // Enable sleep even connected with USB cable
  // node->configUSB(true);
}

//-------------------------------------------------------------------
// message() - receiving messages from gateway
//-------------------------------------------------------------------
void message(const uint8_t *payload, size_t size, port_t port)
{
  debugSerial.println("-- MESSAGE");
  debugSerial.print("Received " + String(size) + " bytes on port " + String(port) + ":");

  for (int i = 0; i < size; i++)
  {
    debugSerial.print(" " + String(payload[i]));
  }
  debugSerial.println();

  if (size>0)
  {
    EEPROM.write(0,payload[0]); // first payload byte is config_interval in minutes
    resetFunc();  //call reset
  }  
}

//-------------------------------------------------------------------
// loop() 
//-------------------------------------------------------------------
void loop()
{
  node->loop();
}
