#include <IridiumSBD.h> // Using own fork of IridiumSBD @ https://github.com/appelflap/IridiumSBD
#include <SoftwareSerial.h>
#include <TinyGPS++.h> // NMEA parsing: http://arduiniana.org
#include <PString.h> // String buffer formatting: http://arduiniana.org
#include <Time.h> // https://www.pjrc.com/teensy/td_libs_Time.html
#include <EEPROM.h> // For writing settings

#define BEACON_INTERVAL 3600 // Time between transmissions
#define ROCKBLOCK_RX_PIN 7
#define ROCKBLOCK_TX_PIN 8
#define ROCKBLOCK_SLEEP_PIN 6
#define ROCKBLOCK_BAUD 19200
#define ROCKBLOCK_SENDRECEIVE_TIME 120
#define GPS_RX_PIN 2
#define GPS_TX_PIN 3
#define GPS_ENABLE_PIN 4
#define GPS_BAUD 9600
#define GPS_MAX_WAIT 120
#define CONSOLE_BAUD 115200

#define CONFIG_VERSION "ir1"
#define CONFIG_START 32

#define DIAGNOSTICS true

// Example settings structure
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[4];
  // The variables of your settings
  int interval, wait, timeout;
} mySettings = {
  CONFIG_VERSION,
  // The default values
  BEACON_INTERVAL, GPS_MAX_WAIT, ROCKBLOCK_SENDRECEIVE_TIME
};

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int t=0; t<sizeof(mySettings); t++)
      *((char*)&mySettings + t) = EEPROM.read(CONFIG_START + t);
      
     Serial.println("Found config in EEPROM!");
     Serial.print("interval = "); Serial.println(mySettings.interval);
     Serial.print("wait = "); Serial.println(mySettings.wait);
     Serial.print("timeout = "); Serial.println(mySettings.timeout);
   } else {
     Serial.println("No config found in EEPROM");     
   }
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(mySettings); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&mySettings + t));
}

SoftwareSerial ssIridium(ROCKBLOCK_RX_PIN, ROCKBLOCK_TX_PIN);
SoftwareSerial ssGPS(GPS_RX_PIN, GPS_TX_PIN);
IridiumSBD modem(ssIridium, ROCKBLOCK_SLEEP_PIN);
TinyGPSPlus tinygps;

uint8_t inBuffer[200];
char inBufferString[200];

void setup()
{
 
  // Start the serial ports
  Serial.begin(CONSOLE_BAUD);
  ssIridium.begin(ROCKBLOCK_BAUD);

  // Load config from memory
  loadConfig();
  
  // Set max Iridium WAIT
  modem.adjustSendReceiveTimeout(mySettings.timeout);
  
  modem.setPowerProfile(IridiumSBD::DEFAULT_POWER_PROFILE); // 0 = direct connect (default), 1 = USB
  
  // All unused pins to LOW for power saving
  for (int i=0;i<20;i++) {
    if(i != ROCKBLOCK_RX_PIN
       && i != ROCKBLOCK_TX_PIN
       && i != GPS_RX_PIN
       && i != GPS_TX_PIN)
    pinMode(i, OUTPUT);
  }
  
  //SETUP WATCHDOG TIMER
  WDTCSR = (24);//change enable and WDE - also resets
  WDTCSR = (33);//prescalers only - get rid of the WDE and WDCE bit
  WDTCSR |= (1<<6);//enable interrupt mode
  
  //Disable ADC - don't forget to flip back after waking up if using ADC in your application ADCSRA |= (1 << 7);
  ADCSRA &= ~(1 << 7);
  
  //ENABLE SLEEP - this enables the sleep mode
  SMCR |= (1 << 2); //power down mode
  SMCR |= 1;//enable sleep
}

void loop()
{
  bool fixFound = false;
  int err;
  unsigned long loopStartTime = millis();
  
  // Wake up GPS
  Serial.println("Enabling GPS chip...");
  digitalWrite(GPS_ENABLE_PIN, HIGH);
  
  // Step 0: Start the serial ports
  ssIridium.begin(ROCKBLOCK_BAUD);
  ssGPS.begin(GPS_BAUD);

  // Step 1: Reset TinyGPS++ and begin listening to the GPS
  Serial.println("Beginning to listen for GPS traffic...");
  tinygps = TinyGPSPlus();
  ssGPS.listen();

  // Step 2: Look for GPS signal for up to 3 minutes
  for (unsigned long now = millis(); !fixFound && millis() - now < GPS_MAX_WAIT * 1000UL;)
    if (ssGPS.available())
    {
      tinygps.encode(ssGPS.read());
      fixFound = tinygps.location.isValid() && tinygps.date.isValid() &&
        tinygps.time.isValid() && tinygps.altitude.isValid();
    }

  Serial.println(fixFound ? F("A GPS fix was found!") : F("No GPS fix was found."));

  // Disable GPS
  Serial.println("Disabling GPS chip...");
  digitalWrite(GPS_ENABLE_PIN, LOW);

  // Step 3: Start talking to the RockBLOCK and power it up
  Serial.println("Beginning to talk to the RockBLOCK...");
  ssIridium.listen();
  if (modem.begin() == ISBD_SUCCESS)
  {
    char outBuffer[60]; // Always try to keep message short
    size_t inBufferSize = sizeof(inBuffer);
        
    if (fixFound)
    {
      sprintf(outBuffer, "%d%02d%02d%02d%02d%02d,",
        tinygps.date.year(), tinygps.date.month(), tinygps.date.day(),
        tinygps.time.hour(), tinygps.time.minute(), tinygps.time.second());
      int len = strlen(outBuffer);
      PString str(outBuffer, sizeof(outBuffer) - len);
      str.print(tinygps.date.value());
      str.print(",");
      str.print(tinygps.time.value());
      str.print(",");
      str.print(tinygps.location.lat(), 6);
      str.print(",");
      str.print(tinygps.location.lng(), 6);
      str.print(",");
      str.print(tinygps.satellites.value());
      str.print(",");
      str.print(tinygps.altitude.meters());
      str.print(",");
      str.print(tinygps.speed.kmph());
      str.print(",");
      str.print(tinygps.course.deg());
      
      Serial.print("Transmitting message: ");
      Serial.println(outBuffer);
      err = modem.sendReceiveSBDText(outBuffer, inBuffer, inBufferSize);
    }
    else
    {
      // Empty the string in stead of saying "No GPS Fix"
      err = modem.sendReceiveSBDBinary(NULL, NULL, inBuffer, inBufferSize);
    }
    
    if (err != ISBD_SUCCESS)
    {
      Serial.print("sendReceiveSBD* failed: error ");
      Serial.println(err);
    }
    else // success!
    {
      Serial.print("Inbound buffer size is ");
      Serial.println(inBufferSize);
      for (int i=0; i<inBufferSize; ++i)
      {
        inBufferString[i] = inBuffer[i];
        Serial.print(inBuffer[i], HEX);
        if (isprint(inBuffer[i]))
        {
          Serial.print("(");
          Serial.write(inBuffer[i]);
          Serial.print(")");
        }
        Serial.print(" ");
      }
      Serial.println();
      
      // Parse the received settings
      int interval,wait,timeout;
      interval = atoi(strtok(inBufferString,","));
      wait = atoi(strtok(NULL,","));
      timeout = atoi(strtok(NULL,","));
      
      if(interval > 600 && interval <= 86400 * 7) {
        mySettings.interval = interval;
      }
      if(wait > 10 && wait <= 600) {
        mySettings.wait = wait;
      }
      if(timeout > 10 && timeout <= 600) {
        mySettings.timeout = timeout;
      }
     
      saveConfig();
      
      Serial.print("Messages remaining to be retrieved: ");
      Serial.println(modem.getWaitingMessageCount());
    }
  }

  // Sleep
  Serial.println("Going to sleep mode for about an hour...");

  modem.sleep();
  ssIridium.end();
  ssGPS.end();
  
  for(int i=0;i<mySettings.interval/8;i++) {
    
    //BOD DISABLE - this must be called right before the __asm__ sleep instruction
    MCUCR |= (3 << 5); //set both BODS and BODSE at the same time
    MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6); //then set the BODS bit and clear the BODSE bit at the same time
    __asm__  __volatile__("sleep");//in line assembler to go to sleep
    
  }
  
  Serial.println("Woke up from sleep!");  
}

void digitalInterrupt(){
  //needed for the digital input interrupt
}

ISR(WDT_vect){
  //DON'T FORGET THIS!  Needed for the watch dog timer.  This is called after a watch dog timer timeout - this is the interrupt function called after waking up
}// watchdog interrupt

#if DIAGNOSTICS
void ISBDConsoleCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}
#endif
