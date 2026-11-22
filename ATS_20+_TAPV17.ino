//=============== modification by TAP 18.11.2026 ============


// added switch "STEREO / MONO"  ( button BW )
// improved work stability RDS
// added switch  "MUTE"          ( button MODE)
// AUTO changing MODE ( AM / LSB / USB )
// changing default setting 
// added maxAVC cotrol in SSB mode
// incrementing volume in SSB mode (volume_ssb = volume + 5 )

//=================================================================

// Enconder PINs
#define ENCODER_PIN_A 2  // PD2
#define ENCODER_PIN_B 3  // PD3

// OLED Diaplay constants
#define I2C_ADDRESS 0x3C // 0x3C or 0x3D
#define RST_PIN -1       // Define proper RST_PIN if required.

// Reset pin Si4732
#define RESET_PIN 12 

// For read buttons in interrupts
volatile uint16_t StateBut = 0;      // Button state
volatile uint16_t IntTimePush = 0;   // Push button Time counter
// volatile uint16_t IntTimeFree = 0;   // Free button Time counter

uint16_t OldBut = 0;                 // Old button state
uint16_t TimePush, button;           // For button
uint16_t temp16;
// uint16_t TimeFree;

// Encoder control variables
volatile int8_t encoderCount = 0;

// StateBut:
// 0 0 0 0 0 0 0  x   x     x     x   x    x   x    x      x
//               PC0 PD7   PD6   PD5 PD4  PB3 PB2  PB1    PB0
//               BFO VOL_D VOL_U BW  MODE AGC STEP BAND_D BAND_U
// Analog input for battery test - A2 (PC2)
// Digital output for on/off RF amplifier A1 (PC1) =0 for AM,SSB; =1 for FM

// avr-libc library includes
#include <avr/io.h>
#include <Wire.h>
#include <EEPROM.h>
#include "Rotary.h"
#include "Buttons.h"
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#include <SI4735.h>
#include <patch_ssb_compressed.h>    // Compressed SSB patch version (saving almost 1KB)

const uint16_t size_content = sizeof ssb_patch_content; // See ssb_patch_content.h
const uint16_t cmd_0x15_size = sizeof cmd_0x15;         // Array of lines where the 0x15 command occurs in the patch content.
 
#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

#define FM 0
#define LSB 1
#define USB 2
#define AM 3

#define MIN_ELAPSED_RSSI_TIME 150

#define DEFAULT_VOLUME 40    // change it for your favorite sound volume

const uint8_t app_id = 0x5A; // Useful to check the EEPROM content before processing useful data
const int eeprom_address = 0;
long elapsedBatt = millis();
long elapsedSave = millis();
bool needSave = false;


const char *bandModeDesc[] = {"FM ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;
uint8_t seekDirection = 1;

bool ssbLoaded = false;
bool  mode_mute = false;

bool fmStereo = true;


bool SSBmode = false;

int currentBFO = 0;
long countRSSI = 0;
long elapsedRSSI = millis();

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint16_t previousFrequency;
uint8_t currentStep = 1;

uint8_t currentBFOStep = 1;  //100;

uint16_t realBFOStep = 1000; //100;

uint8_t s_m=0; 
uint8_t new_s_m=0;

uint16_t realSSBFRQ;


// Datatype to deal with bandwidth on AM, SSB and FM in numerical order.
// Ordering by bandwidth values. 

//============================================================  
typedef struct
{
  uint8_t idx;      // SI473X device bandwidth index value
  const char *desc; // bandwidth description
} Bandwidth;

int8_t bwIdxSSB = 4;
Bandwidth bandwidthSSB[] = {
  {4, "0.5"},  // 0  
  {5, "1.0"},  // 1
  {0, "1.2"},  // 2
  {1, "2.2"},  // 3
  {2, "3.0"},  // 4  - default
  {3, "4.0"}   // 5
}; // 3 = 4kHz

int8_t bwIdxAM = 5;
const int maxFilterAM = 15;
Bandwidth bandwidthAM[] = {
  {4, "1.0"},   // 0
  {5, "1.8"},   // 1
  {3, "2.0"},   // 2
  {6, "2.5"},   // 3
  {2, "3.0"},   // 4 
  {1, "4.0"},   // 5 - default
  {0, "6.0"}    // 6 
};
/*
 int8_t bwIdxFM = 0;
 Bandwidth bandwidthFM[] = {
  {0, "AUT"}, // Automatic - default
  {1, "110"}, // Force wide (110 kHz) channel filter.
  {1, " 84"},
  {2, " 60"},
  {3, " 40"}
 };
*/
// Atenuator and AGC
uint8_t agcIdx = 0;
uint8_t disableAgc = 0;
uint8_t agcNdx = 0;
// AVC
uint8_t avcIdx = 0;
uint8_t max_avc_gain = 48;
uint8_t avcNdx = 1; // max AVC

//======================================================

// Band data structure
typedef struct
{
  uint8_t bandType;     // Band type (FM, MW or SW)
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Default step (increment and decrement)
  int8_t  bandwidth;  // Bandwidth local table index.


} Band;

/*
   Band table
   To add a new band, all you have to do is insert a new line in the table below.
   No extra code will be needed.
   Remove or comment a line if you do not want a given band
   You have to RESET the eeprom after modiging this table. 
   Turn your receiver on with the encoder push button pressed at first time
   to RESET the eeprom content.
*/
Band band[] = {

  {FM_BAND_TYPE, 6400, 10800, 8750, 10, 0},

   //{LW_BAND_TYPE, 150, 520, 300, 5, 5},

  {MW_BAND_TYPE, 150, 1710, 522, 9, 5},   // LW + MW (150 a 1710 kHz) for Europe, Africa and Asia

  
// SW BANDS AM mode

  {SW_BAND_TYPE, 1710, 2900, 2300, 5, 5},    // 120 meters
  {SW_BAND_TYPE, 2900, 3400, 3200, 5, 5},    // 90 meters
  {SW_BAND_TYPE, 3800, 4100, 3900, 5, 5},    // 75 meters
  {SW_BAND_TYPE, 4700, 5100, 4700, 5, 5},    // 60 meters
  {SW_BAND_TYPE, 5600, 6300, 5600, 5, 5},    // 49 meters
  {SW_BAND_TYPE, 7100, 7800, 7200, 5, 5},    // 41 meters
  {SW_BAND_TYPE, 9300, 10100, 9300, 5, 5},   // 31 meters
  {SW_BAND_TYPE, 11300, 12200, 11300, 5, 5}, // 25 meters
  {SW_BAND_TYPE, 13500, 13900, 13500, 5, 5}, // 22 meters
  {SW_BAND_TYPE, 15000, 16000, 15000, 5, 5}, // 19 meters
  {SW_BAND_TYPE, 17400, 18000, 17400, 5, 5}, // 16 meters
  {SW_BAND_TYPE, 21350, 22000, 21350, 5, 5}, // 13 meters
  {SW_BAND_TYPE, 25600, 27500, 26965, 5, 5}, // 11 meters + CB (27 MHz AM)

  
// HAMRADIO BANDS SSB mode

  {SW_BAND_TYPE, 1700, 3400, 1900, 1, 4},     // 160m (LSB)
 {SW_BAND_TYPE, 3400, 6900, 3700, 1, 4},     // 80m (LSB)
 {SW_BAND_TYPE, 6900, 10000, 7100, 1, 4},    // 40m (LSB)
 {SW_BAND_TYPE, 10000, 13900, 10100, 1, 4},  // 30m (USB)
 {SW_BAND_TYPE, 13900, 18000, 14200, 1, 4},  // 20m (USB)
 {SW_BAND_TYPE, 18000, 20900, 18100, 1, 4},  // 17m (USB)
 {SW_BAND_TYPE, 20900, 24800, 21200, 1, 4},  // 15m (USB)
 {SW_BAND_TYPE, 24800, 26965, 24900, 1, 4},  // 12m (USB)
 {SW_BAND_TYPE, 26965, 30000, 27385, 1, 4}   // CB 27 MHz USB (Canal 38) + 10m (USB)


}; 

const int lastBand = (sizeof band / sizeof(Band)) - 1;

int bandIdx = 0;

uint8_t rssi = 0;
uint8_t aux;
uint8_t volume = DEFAULT_VOLUME;  
uint8_t vol_save;

uint8_t volume_mute = 0 ;
//===========================================
uint8_t volume_ssb = DEFAULT_VOLUME + 5;


// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735 si4735;
SSD1306AsciiWire oled;

//==================================================================
//==================================================================
// ===================== SETUP =====================================

void setup()
{
  InitButtons();
  
// Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  Wire.begin();
  Wire.setClock(400000L);

//++++ UNcomment for SSD1306, COMMEMT for SH1106 ++++

#if RST_PIN >= 0
  oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
#else // RST_PIN < 0
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
#endif 

//++++ UNcomment for SH1106, COMMENT for SSD1306 ++++

//#if RST_PIN >= 0
//  oled.begin(&SH1106_128x64, I2C_ADDRESS, RST_PIN);
//#else // RST_PIN < 0
//  oled.begin(&SH1106_128x64, I2C_ADDRESS);
//#endif
//+++++ End SSD1306 or SH1106 +++++++

// Splash - Change it for your introduction text.
  oled.clear();
  oled.setFont(X11fixed7x14B);
  oled.setCursor(10, 0);
  oled.print("ATS-20+ (Si4732)");
  oled.setCursor(16, 2);
  oled.set2X();
  oled.println(" RADIO");
  oled.set1X(); 
  oled.setCursor(21, 6);
  oled.print(" -FM-AM-SSB-");
  delay(2000);
// end Splash
//====================================
  //====================================

// If you want to reset the eeprom, keep the BFO_SWITCH button pressed during statup
 if (StateBut & BFO_SWITCH)
  {
    oled.clear();
    EEPROM.update(eeprom_address, 0);
    oled.setCursor(15, 1);
    oled.print("EEPROM RESETED");
    delay(2000);
  }
 while (StateBut); // wait for off all buttons
 oled.clear();

//====================================
//====================================

//End EEPROM reset

 si4735.getDeviceI2CAddress(RESET_PIN); // Looks for the I2C bus address and set it.  Returns 0 if error
 si4735.setup(RESET_PIN, MW_BAND_TYPE);

 delay(300);
// Checking the EEPROM content

 if (EEPROM.read(eeprom_address) == app_id)
  {
    readAllReceiverInformation();
  } 

// Set up the radio for the current band (see index table variable bandIdx )
  useBand();
  currentFrequency = previousFrequency = si4735.getFrequency();
  //si4735.setVolume(volume);
  if (currentMode == LSB || currentMode == USB)
   {
     si4735.setVolume(volume_ssb);
    }
    else
    {
      si4735.setVolume(volume);
    }
  oled.clear();
  showStatus(); 
  showBattery();
 
}

// ===================== END of SETUP ==============================
//==================================================================
//==================================================================

void saveAllReceiverInformation()
 {
   int addr_offset;
   EEPROM.update(eeprom_address, app_id); // stores the app id;
  //EEPROM.update(eeprom_address + 1, si4735.getVolume()); // stores the current Volume
   EEPROM.update(eeprom_address + 1, volume);
   EEPROM.update(eeprom_address + 2, bandIdx); // Stores the current band
   EEPROM.update(eeprom_address + 3, currentMode); // Stores the current Mode (FM / AM / SSB)
   EEPROM.update(eeprom_address + 4, currentBFO >> 8);
   EEPROM.update(eeprom_address + 5, currentBFO &  0XFF);

   addr_offset = 6;
   band[bandIdx].currentFreq = currentFrequency;

   for (int i = 0; i <= lastBand; i++ )
    {
      EEPROM.update(addr_offset++, (band[i].currentFreq >> 8) );   // stores the current Frequency HIGH byte for the band
      EEPROM.update(addr_offset++, (band[i].currentFreq & 0xFF));  // stores the current Frequency LOW byte for the band
      EEPROM.update(addr_offset++, band[i].currentStep);          // Stores current step of the band
      EEPROM.update(addr_offset++, band[i].bandwidth);           // table index (direct position) of bandwidth
    }    
}

//==================================================================
//==================================================================

void readAllReceiverInformation()
{
  int addr_offset;
  int bwIdx;
  volume = EEPROM.read(eeprom_address + 1); // Gets the stored volume;
  volume_ssb = volume + 5;
  if ( volume_ssb > 63 ) volume_ssb = 63 ;
  bandIdx = EEPROM.read(eeprom_address + 2);
  currentMode = EEPROM.read(eeprom_address + 3);
  currentBFO = EEPROM.read(eeprom_address + 4) << 8;
  currentBFO |= EEPROM.read(eeprom_address + 5);

  addr_offset = 6;
  for (int i = 0; i <= lastBand; i++ )
  {
    band[i].currentFreq = EEPROM.read(addr_offset++) << 8;
    band[i].currentFreq |= EEPROM.read(addr_offset++);
    band[i].currentStep = EEPROM.read(addr_offset++);
    band[i].bandwidth = EEPROM.read(addr_offset++);
  }

  previousFrequency = currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;
  bwIdx = band[bandIdx].bandwidth;

  if (currentMode == LSB || currentMode == USB)
  {
      
      loadSSB();
      bwIdxSSB = (bwIdx > 5)? 5: bwIdx;
      si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
      if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
          si4735.setSSBSidebandCutoffFilter(0);
       else
          si4735.setSSBSidebandCutoffFilter(1);
          
   }
   else if (currentMode == AM)
   {
        bwIdxAM = bwIdx;
        si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
   } else
      {
//        bwIdxFM = 0;   // bwIdx;
        fmStereo = true;  // set stereo
        
        si4735.setFmBandwidth(0);      // 1=>110 kHz   0 =>auto
        SetStereoMono();
//        si4735.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
     }    
} 

//==================================================================
//==================================================================

// Switch the radio to current band.
// The bandIdx variable points to the current band.
// This function change to the band referenced by bandIdx (see table band).
// Show some basic information on display
void showStatus()
{
  showFrequency();
  oled.setFont(X11fixed7x14B);
  showSTEP();
  showBandwidth();
  ShowAGC();
  showRSSI();
  if (mode_mute == false) showVolume();
  
}

//==================================================================
//==================================================================

// Converts a number to a char string and places leading zeros.
// It is useful to mitigate memory space used by sprintf or generic similar function
void convertToChar(uint16_t value, char *strValue, uint8_t len, uint8_t dot)
{
  char d;
  for (char i = (len - 1); i >= 0; i--)
  {
    d = value % 10;
    value = value / 10;
    strValue[i] = d + 48;
  }
  strValue[len] = '\0';
  if ( dot > 0 )
   {
     for (char i = len; i >= dot ; i-- )
      {
       strValue[i + 1] = strValue[i];
      }
     strValue[dot] = '.';
  }

    for (uint8_t i = 0; i < (len - 1); i++)
   {
    if (strValue[i] == '0') strValue[i] = '-';
    else break;
   }

}

//==================================================================
//==================================================================

// Show current frequency
void showFrequency()
{
  uint16_t tmp;
  uint8_t d;
  char *bandMode;
  char freqDisplay[10];

  oled.setFont(lcdnums14x24);
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    oled.setCursor(0, 2);
    oled.print("  ");              // Limpia los primeros 28 pixeles de la izquierda al venir de AM/SW
    convertToChar(currentFrequency / 10, freqDisplay, 4, 3);
    strcat(freqDisplay, "--");    // Limpia cualquier rastro a la derecha
    oled.setCursor(22, 2);        // Centra la frecuencia en pantalla (ej. "98.5" o "104.2")
    oled.print(freqDisplay);
  }
  else        // AM, LSB or USB
  {
    realSSBFRQ = currentFrequency;
    tmp = 0;
    if (currentMode == LSB || currentMode == USB)
    {
      // Convertimos todo a Hz totales para evitar errores con numeros negativos
      uint32_t totalHz = ((uint32_t)currentFrequency * 1000UL) + currentBFO;
      realSSBFRQ = totalHz / 1000UL;
      tmp = (totalHz % 1000UL) / 10UL;
    }
    convertToChar(realSSBFRQ, freqDisplay, 5, 0);
    oled.setCursor(0, 2);
    oled.print(freqDisplay);
    d = tmp % 10;
    tmp = tmp / 10;
    freqDisplay[2] = d + 48;
    d = tmp % 10;
    tmp = tmp / 10;
    freqDisplay[1] = d + 48;
    freqDisplay[0] = '.';
    freqDisplay[3] = '\0';
    oled.print(freqDisplay);
  }

  oled.setFont(X11fixed7x14B);
  bandMode = (char *) bandModeDesc[currentMode];
  oled.setCursor(0, 0);
  oled.print(bandMode);

  if (band[bandIdx].bandType != FM_BAND_TYPE)
  {
    oled.setCursor(113, 3);
    oled.print("  ");
  }
}


//==================================================================
//==================================================================

// Show Step
void showSTEP()
 {
   oled.setCursor(32, 0);
   oled.print("   ");         // Borra solo 3 caracteres (hasta pixel 59) para no tocar los corchetes
   oled.setCursor(32, 0);
   if (currentMode == AM)
    {
      oled.print(currentStep);
      oled.print("k");
    }
   if (currentMode == LSB || currentMode == USB)
    {
      oled.print(currentBFOStep);
      if (realBFOStep >= 1000)
      {
        oled.print("k");
      }
    }
 }


//==================================================================
//==================================================================

// Show AGC Information
void ShowAGC()
  {
	  if (currentMode == AM || currentMode == FM )
   {
    si4735.getAutomaticGainControl();
    oled.setCursor(0, 6);
    if (agcIdx == 0 )
     {
       oled.print("AGC");
     }
    else
     {
       oled.print('-');
       if (agcNdx < 10)
         oled.print('0');
       oled.print(agcNdx);
     }
	}
//=================================================================
	 if (currentMode == LSB || currentMode == USB )
	 {
	  oled.setCursor(0, 6);
	  switch (avcNdx)
       {
         case 1:
          {
            oled.print("MAX");
            break;
          }
         case 2:
          {
            oled.print("-6 ");
            break;
          }
         case 3:
          {
            oled.print("-12");
            break;
          }
		     case 4:
          {
            oled.print("-18");
            break;
          }
	   }
	 }
  }         

//==================================================================
// Shows RSSI/RSN status
// Shows RSSI/RSN status calibrated to real S-Meter
void showRSSI()
{
  oled.setCursor(27, 6);
  oled.print("<");
  oled.setCursor(90, 6);
  oled.print(">");
  
  int bars = 0;

  if (currentMode == FM)
  {
    // En FM el chip Si4732 suele dar entre 5 (muy debil) y 65 (a tope)
    if (rssi >= 5)  bars = 1;
    if (rssi >= 12) bars = 2;
    if (rssi >= 20) bars = 3;
    if (rssi >= 28) bars = 4;
    if (rssi >= 36) bars = 5;
    if (rssi >= 44) bars = 6;
    if (rssi >= 52) bars = 7;
    if (rssi >= 60) bars = 8;
  }
  else
  {
    // Escala real S-Meter para AM / SW / SSB (basada en tabla dBuV)
    if (rssi >= 4)  bars = 1; // S4 (señal minima audible)
    if (rssi >= 10) bars = 2; // S5
    if (rssi >= 16) bars = 3; // S6
    if (rssi >= 22) bars = 4; // S7
    if (rssi >= 28) bars = 5; // S8
    if (rssi >= 34) bars = 6; // S9 (señal fuerte)
    if (rssi >= 44) bars = 7; // S9 +10dB
    if (rssi >= 54) bars = 8; // S9 +20dB o mas
  }

  oled.setCursor(34, 6);
  for (int i = 1; i <= 8; i++)
  {
    if (bars >= i) oled.print("|");
    else           oled.print(".");
  }

  if (currentMode == FM)
  {
    oled.setCursor(113, 3);
    if (si4735.getCurrentPilot() && fmStereo)
    { 
      oled.print("ST");
    }
    else
    {
      if (fmStereo) oled.print("  ");
      else          oled.print("MN");
    }
  }
}

//==================================================================
//==================================================================


// Shows the volume level on OLED
void showVolume()
{
  if (volume < 10)
  {
   oled.setCursor(113, 0);
   oled.print("  ");
   oled.setCursor(113, 0);
   //oled.print(si4735.getCurrentVolume());
   oled.print (volume);
  }
  else
  {
  oled.setCursor(113, 0);
  //oled.print(si4735.getCurrentVolume());
  oled.print (volume);
  }
}

//==================================================================
//==================================================================

// Shows battery on OLED
void showBattery()
{
  uint8_t n, m;
  char strBatt[5];

  m = ReadVcc();

  if (m == 0)
  {
    oled.setCursor(100, 6);
    oled.print("BATT");
    return;
  }

  n = m % 10;
  m = m / 10;
  strBatt[2] = n + 48;
  n = m % 10;
  m = m / 10;
  strBatt[1] = n + 48;
  n = m % 10;
  strBatt[0] = n + 48;
  strBatt[3] = '%';
  strBatt[4] = '\0';
  if (strBatt[0] == '0') strBatt[0] = ' ';
  oled.setCursor(100, 6);
  oled.print(strBatt);
}


//==================================================================
//==================================================================

// Show bandwidth on AM,SSB and FM mode
void showBandwidth()
{
  char bw[7];

  strcpy(bw, "[]");
  if (currentMode == LSB || currentMode == USB)
  {
   if (bwIdxSSB > 5) bwIdxSSB = 5;
   strcat(bw, bandwidthSSB[bwIdxSSB].desc);
  }
  else if (currentMode == AM)
  {
   strcat(bw, bandwidthAM[bwIdxAM].desc);
  }
  else
  {        
   strcpy(bw, "      ");
  }
  oled.setCursor(66, 0);      // Empieza justo despues de donde acaba "50k"
  oled.print(bw);
}


//==================================================================
//==================================================================
void useBand()
{
  cleanBfoRdsInfo();

  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    agcIdx = 0;
    disableAgc = 0; // Turns AGC ON
    agcNdx = 0;
    currentMode = FM;
    si4735.setTuneFrequencyAntennaCapacitor(0);
    si4735.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
    si4735.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    si4735.setSeekFmSpacing(band[bandIdx].currentStep);
    si4735.setSeekFmRssiThreshold(6);
    si4735.setSeekFmSNRThreshold(2);
    si4735.setFmSoftMuteMaxAttenuation(0);
    ssbLoaded = false; currentBFO = 0;
    si4735.setAutomaticGainControl(disableAgc, agcNdx);
    si4735.setRdsConfig(3, 3, 3, 3, 3);
    si4735.setFifoCount(1);
    si4735.setFmBandwidth(0);
    SetStereoMono();
    checkRDS();
  }
  else
  {
    // Si vamos a SSB y el parche no está cargado aún, lo cargamos aquí
    if ((currentMode == LSB || currentMode == USB) && !ssbLoaded)
    {
      loadSSB();
    }

    si4735.setTuneFrequencyAntennaCapacitor(1);

    if (ssbLoaded && (currentMode == LSB || currentMode == USB))
    {
      si4735.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      si4735.setSSBAutomaticVolumeControl(1);
      si4735.setSsbSoftMuteMaxAttenuation(0);
      si4735.setAutomaticGainControl(disableAgc, agcNdx);
      bwIdxSSB = band[bandIdx].bandwidth;
      if (bwIdxSSB > 5) bwIdxSSB = 5;
      si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      si4735.setSSBBfo(-currentBFO);
      si4735.setAvcAmMaxGain(max_avc_gain);
      si4735.setSsbIfAgcAttackRate(4);
      si4735.setSsbIfAgcReleaseRate(140);
    }
    else
    {
      currentMode = AM;
      si4735.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      si4735.setAutomaticGainControl(disableAgc, agcNdx);
      si4735.setAmAgcReleaseRate(40);
      si4735.setAmSoftMuteMaxAttenuation(0);
      bwIdxAM = band[bandIdx].bandwidth;
      si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      si4735.setAvcAmMaxGain(48);
    }
    si4735.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    si4735.setSeekAmSpacing((band[bandIdx].currentStep > 10) ? 10 : band[bandIdx].currentStep);
    si4735.setSeekAmRssiThreshold(6);
    si4735.setSeekAmSNRThreshold(2);
  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;

  showStatus();
}

//==================================================================
//==================================================================

// Goes to the next band (see Band table)
void bandUp()
{
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;

  if (bandIdx < lastBand)
    bandIdx++;
  else
    bandIdx = 0;

  if (bandIdx == 0)
  {
    currentMode = FM;
    if (mode_mute == false) si4735.setVolume(volume);
  }
  else if (bandIdx >= 1 && bandIdx <= 14)
  {
    currentMode = AM;
    ssbLoaded = false;
    if (mode_mute == false) si4735.setVolume(volume);
  }
  else if (bandIdx >= 15 && bandIdx <= 17) // 160m, 80m, 40m -> LSB
  {
    currentMode = LSB;
  }
  else if (bandIdx >= 18 && bandIdx <= 23) // 30m, 20m, 17m, 15m, 12m, 10m -> USB
  {
    currentMode = USB;
  }

  if (currentMode == LSB || currentMode == USB)
  {
    volume_ssb = volume + 5;
    if (volume_ssb > 63) volume_ssb = 63;
    if (mode_mute == false) si4735.setVolume(volume_ssb);
  }

  fmStereo = true;
  useBand();
}

//==================================================================
//==================================================================

// Goes to the previous band (see Band table)
void bandDown()
{
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;

  if (bandIdx > 0)
    bandIdx--;
  else
    bandIdx = lastBand;

  if (bandIdx == 0)
  {
    currentMode = FM;
    if (mode_mute == false) si4735.setVolume(volume);
  }
  else if (bandIdx >= 1 && bandIdx <= 14)
  {
    currentMode = AM;
    ssbLoaded = false;
    if (mode_mute == false) si4735.setVolume(volume);
  }
  else if (bandIdx >= 15 && bandIdx <= 17) // 160m, 80m, 40m en LSB
  {
    currentMode = LSB;
  }
  else if (bandIdx >= 18 && bandIdx <= 23) // 30m, 20m, 17m, 15m, 12m, 10m -> USB
  {
    currentMode = USB;
  }

  if (currentMode == LSB || currentMode == USB)
  {
    volume_ssb = volume + 5;
    if (volume_ssb > 63) volume_ssb = 63;
    if (mode_mute == false) si4735.setVolume(volume_ssb);
  }

  fmStereo = true;
  useBand();
}

//==================================================================
//==================================================================

// This function loads the contents of the ssb_patch_content array into the
// CI (Si4735) and starts the radio on SSB mode.
void loadSSB()
{
  oled.setCursor(24, 0);
  oled.print("Loading SSB ");
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.downloadCompressedPatch(ssb_patch_content, size_content, cmd_0x15, cmd_0x15_size);
  si4735.setSSBConfig(bandwidthSSB[bwIdxSSB].idx, 1, 0, 1, 0, 1);    

  ssbLoaded = true;
  cleanBfoRdsInfo();
  currentBFOStep = 1;
  realBFOStep = 1000;
} 
//==================================================================
//==================================================================

char *stationName;
char *programInfo;
long delayStationName = millis();
long delayProgramInfo = millis();
long delayMsgTurn = millis();
bool bShowStationName = true;
int progInfoIdx = 0;

// Clean RDS
void cleanBfoRdsInfo()
{
  oled.setFont(X11fixed7x14B);
  oled.setCursor(24, 0);
  oled.print("            "); // Limpia toda la zona superior sin tocar el volumen
}

// Muestra el texto largo (RadioText) desplazándose sin invadir "FM" ni el volumen
void showProgramInfo()
{
  char txtAux[12]; // 11 caracteres visibles + '\0' (ocupa 77 px: de X=24 a X=101)
  if (programInfo == NULL || (millis() - delayProgramInfo) < 500) return;

  cleanBfoRdsInfo();
  programInfo[60] = '\0';

  for (byte i = 0; i < 11; i++)
  {
    char c = programInfo[progInfoIdx + i];
    if (c == '\0')
    {
      txtAux[i] = ' '; // Rellena con espacios si llega al final
    }
    else if (c < 32 || c > 126)
    {
      // Traduce los códigos RDS europeos más comunes en España a letras legibles
      switch ((uint8_t)c)
      {
        case 0x81: case 0x91: case 0xE1: case 0xC1: txtAux[i] = 'A'; break; // á, Á
        case 0x82: case 0x92: case 0xE9: case 0xC9: txtAux[i] = 'E'; break; // é, É
        case 0x83: case 0x93: case 0xED: case 0xCD: txtAux[i] = 'I'; break; // í, Í
        case 0x84: case 0x94: case 0xF3: case 0xD3: txtAux[i] = 'O'; break; // ó, Ó
        case 0x85: case 0x95: case 0x99: case 0xD9: case 0xFA: case 0xDA: txtAux[i] = 'U'; break; // ú, Ú, ü, Ü
        case 0x8A: case 0x9A: case 0xF1: case 0xD1: txtAux[i] = 'N'; break; // Ñ, ñ
        case 0x8B: case 0x9B: case 0xE7: case 0xC7: txtAux[i] = 'C'; break; // Ç, ç
        case 0x0D: case 0x0A: txtAux[i] = ' '; break; // Fin de línea RDS -> espacio
        default: txtAux[i] = '?'; break; // Cualquier otro símbolo desconocido

      }
    }
    else
    {
      txtAux[i] = c;
    }
  }
  txtAux[11] = '\0';

  progInfoIdx += 2;
  if (progInfoIdx > (60 - 11)) progInfoIdx = 0;

  oled.setFont(X11fixed7x14B);
  oled.setCursor(24, 0);
  oled.print(txtAux);
  delayProgramInfo = millis();
}

// Muestra el nombre corto de la emisora (8 caracteres)
void showRDSStation()
{
  static char lastStationName[9] = "";
  static bool wasShowingProgInfo = false;

  // Si estamos mostrando el texto largo, marcamos que habrá que limpiar al volver
  if (!bShowStationName)
  {
    wasShowingProgInfo = true;
    return;
  }

  if (stationName == NULL || (millis() - delayStationName) < 1000) return;

  // Solo borra y redibuja si el nombre es nuevo O si acabamos de volver del texto largo
  if (strncmp(lastStationName, stationName, 8) != 0 || wasShowingProgInfo)
  {
    cleanBfoRdsInfo();
    oled.setFont(X11fixed7x14B);

    byte x = 24;
    for (byte i = 0; i < 8; i++)
    {
      if (stationName[i] == '\0') break;
      oled.setCursor(x, 0);
      oled.print(stationName[i]);
      x += 10;
    }

    strncpy(lastStationName, stationName, 8);
    lastStationName[8] = '\0';
    wasShowingProgInfo = false;
  }

  delayStationName = millis();
}


// Comprueba si el texto tiene al menos un carácter visible (mayor que espacio ASCII 32)
bool hasValidProgramInfo(char *info)
{
  if (info == NULL) return false;
  for (byte i = 0; i < 64; i++)
  {
    if (info[i] == '\0') break;
    if (info[i] > 32) return true;
  }
  return false;
}

void checkRDS()
{
  si4735.getRdsStatus();
  if (si4735.getRdsReceived())
  {
    if (si4735.getRdsSync() && si4735.getNumRdsFifoUsed() > 0)
    {
      // Alterna el turno cada 30 segundos
      if ((millis() - delayMsgTurn) > 30000)
      {
        bShowStationName = !bShowStationName;
        progInfoIdx = 0;
        delayMsgTurn = millis();
      }

      if (bShowStationName)
      {
        stationName = si4735.getRdsStationName();
        showRDSStation();
      }
      else
      {
        // Llama siempre a getRdsProgramInformation() para que la librería procese la cola FIFO
        programInfo = si4735.getRdsProgramInformation();

        if (hasValidProgramInfo(programInfo))
        {
          // Hay texto real (o genérico) -> lo muestra con scroll
          showProgramInfo();
        }
        else
        {
          // Si esa emisora NO transmite texto, sigue mostrando el nombre en vez de dejarlo en blanco
          stationName = si4735.getRdsStationName();
          showRDSStation();
        }
      }
    }
  }
}


//-----------------------------------------------

//This function is called by the seek function process.
void showFrequencySeek(uint16_t freq)
  {
    currentFrequency = freq;
    showFrequency();
  }
//-----------------------------------------------

//   Checks the stop seeking criterias.
//   Returns true if the user press the touch or rotates the encoder.
bool checkStopSeeking()
  {
// Checks the BFO_SWITCH and encoder
    return (bool) (encoderCount || (StateBut & BFO_SWITCH));
  }

//==================================================================
//==================================================================
void SetStereoMono()
{
  if (fmStereo == true)
  {
    si4735.setFMDeEmphasis(1);                 // 50 mks
    si4735.setFmBlendStereoThreshold(0);
    si4735.setFmBlendMonoThreshold(0);
    si4735.setFmBlendRssiStereoThreshold(0);
    si4735.setFmBLendRssiMonoThreshold(0);
    si4735.setFmBlendSnrStereoThreshold(0);
    si4735.setFmBLendSnrMonoThreshold(0);
    si4735.setFmBlendMultiPathStereoThreshold(100);
    si4735.setFmBlendMultiPathMonoThreshold(100);
  }
  else
  {
    si4735.setFmBlendStereoThreshold(127);
    si4735.setFmBlendMonoThreshold(127);
    si4735.setFmBlendRssiStereoThreshold(127);
    si4735.setFmBLendRssiMonoThreshold(127);
    si4735.setFmBlendSnrStereoThreshold(127);
    si4735.setFmBLendSnrMonoThreshold(127);
    si4735.setFmBlendMultiPathStereoThreshold(0);
    si4735.setFmBlendMultiPathMonoThreshold(0);
  }
}

//==================================================================
//==================================================================

// Use Rotary.h and Rotary.cpp implementation to process encoder via interrupt
 void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
  {
    if (encoderStatus == DIR_CW)
    {
      if (encoderCount < 20) encoderCount++;
    }
    else
    {
      if (encoderCount > -20) encoderCount--;
    }
  }
}


// ===================== LOOP ======================================
//==================================================================
//==================================================================
void loop()
{
  int8_t steps = 0;
  noInterrupts();
  steps = encoderCount;
  encoderCount = 0;
  interrupts();

  // Check if the encoder has moved.
  if (steps != 0)
  {
    previousFrequency = 0; // Forces eeprom update, etc

    if (ssbLoaded)

    {
      if (realBFOStep >= 10000)
      {
        uint16_t stepKHz = realBFOStep / 1000;
        if (steps > 0 && (currentFrequency + stepKHz) <= band[bandIdx].maximumFreq)
          currentFrequency += stepKHz;
        else if (steps < 0 && (currentFrequency - stepKHz) >= band[bandIdx].minimumFreq)
          currentFrequency -= stepKHz;
        si4735.setFrequency(currentFrequency);
        si4735.setAutomaticGainControl(disableAgc, agcNdx);
      }
      else
      {
        currentBFO += (steps * realBFOStep);

        if (currentBFO >= 16000)
        {
          currentBFO -= 16000;
          currentFrequency += 16;
          si4735.setFrequency(currentFrequency);
          si4735.setAutomaticGainControl(disableAgc, agcNdx);
        }
        //========================================================
        if (currentBFO <= -16000)
        {
          currentBFO += 16000;
          currentFrequency -= 16;
          si4735.setFrequency(currentFrequency);
          si4735.setAutomaticGainControl(disableAgc, agcNdx);
        }
        si4735.setSSBBfo(-currentBFO);
      }
    }
    //===========================================================
    else
    {
      if ((steps > 0) && (currentFrequency < band[bandIdx].maximumFreq))
      {
        si4735.frequencyUp();
        seekDirection = 1;
      }
      if ((steps < 0) && (currentFrequency > band[bandIdx].minimumFreq))
      {
        si4735.frequencyDown();
        seekDirection = 0;
      }
      // Show the current frequency only if it has changed
      currentFrequency = si4735.getFrequency();
    }
    showFrequency();
  }


//==================================================================
//==================================================================
//==================================================================
//==================================================================

// -------- Check buttons --------

  TimePush = IntTimePush;
  if (TimePush != IntTimePush)  TimePush = IntTimePush; // Interrupt took place
//  TimeFree = IntTimeFree;
//  if (TimeFree != IntTimeFree)  TimeFree = IntTimeFree; // Interrupt took place

  button = StateBut;
  temp16 = ((OldBut ^ button) & button);     // Check if any -|_

  OldBut = button;                       // Update
  if (TimePush > 150)                    // Auto repeat
    {
      IntTimePush = 0;
      TimePush = IntTimePush;
      if (TimePush != IntTimePush)  TimePush = IntTimePush; // Interrupt took place
      temp16 = button;
    }

//==================================================================
//==================================================================

// Check BAND_BUTTON_UP
  if (temp16 & BAND_BUTTON_UP)
       bandUp();
// Check BAND_BUTTON_DOWN
  if (temp16 & BAND_BUTTON_DOWN)
       bandDown();
 
//==================================================================
//==================================================================

// Check VOL_UP
  if (temp16 & VOL_UP)
    {
      if (mode_mute == false )
      {
      IntTimePush = 100;      // Fast auto repeat
      volume++;
      if ( volume > 63 ) volume = 63 ;
       if (currentMode == AM || currentMode == FM) si4735.setVolume(volume);
      if (currentMode == LSB || currentMode == USB)
       {
	      volume_ssb = volume + 5 ;
	      if ( volume_ssb > 63 ) volume_ssb = 63 ;
	      si4735.setVolume(volume_ssb);
       }
       showVolume(); 
       }
    }
// Check VOL_DOWN
  if (temp16 & VOL_DOWN )
    {
      if (mode_mute == false )
      {
      IntTimePush = 100;      // Fast auto repeat
      volume--;
      if ( volume <= 1 ) volume = 1 ;
       if (currentMode == AM || currentMode == FM) si4735.setVolume(volume);
      if (currentMode == LSB || currentMode == USB)
       {
	      volume_ssb = volume + 5 ;
	      if ( volume_ssb > 63 ) volume_ssb = 63 ;
	      si4735.setVolume(volume_ssb);
       }
       showVolume(); 
       }
    
    }

//==================================================================
//==================================================================
// Check BANDWIDTH
  if (temp16 & BANDWIDTH_BUTTON)
 {
      
 //=============================================     
     if (currentMode == LSB || currentMode == USB)
      {
         bwIdxSSB++;
         if (bwIdxSSB > 5)
           bwIdxSSB = 0;
         band[bandIdx].bandwidth = bwIdxSSB;
         si4735.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
// If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
         if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
           si4735.setSSBSidebandCutoffFilter(0);
         else
           si4735.setSSBSidebandCutoffFilter(1);
          showStatus();
      }
//==========================================
//     else
      if (currentMode == AM)
       {
         bwIdxAM++;
         if (bwIdxAM > 6)
           bwIdxAM = 0;
           band[bandIdx].bandwidth = bwIdxAM;
           si4735.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
           showStatus();
       } 
//==========================================     
      if (currentMode == FM) 
       {

       fmStereo = !fmStereo;
        SetStereoMono();
        showRSSI();
        }

 } 
//==================================================================
//==================================================================

// Check AGC
  if (temp16 & AGC_SWITCH)
  {  if (currentMode == AM || currentMode == FM )
    {
      agcIdx += 1;
      if (agcIdx >4)  agcIdx = 0;
      switch (agcIdx)
       {
         case 0:
          {
            disableAgc = 0; // Turns AGC ON
            agcNdx = 0;
            break;
          }
         case 1:
          {
            disableAgc = 1; // Turns AGC OFF
            agcNdx = 0;     // Sets minimum attenuation
            break;
          }
         case 2:
          {
            disableAgc = 1; // Turns AGC OFF
            agcNdx = 6;    // Increases the attenuation AM/SSB AGC Index  = 6
            break;
          }
         case 3:
          {
            disableAgc = 1; // Turns AGC OFF
            agcNdx = 12;    // Increases the attenuation AM/SSB AGC Index  = 12
            break;
          }
         case 4:
          {
            disableAgc = 1; // Turns AGC OFF
            agcNdx = 18;    // Increases the attenuation AM/SSB AGC Index  = 18
          }
       }
// Sets AGC on/off and gain
      si4735.setAutomaticGainControl(disableAgc, agcNdx);
      showStatus();
       
    }
//==========================================================================
// Check AVC for SSB
	
	if (currentMode == LSB || currentMode == USB)
	{
		avcIdx += 1;
		if (avcIdx > 3)  avcIdx = 0;
        switch (avcIdx)
       {
         case 0:
          {
            max_avc_gain = 48; 
            avcNdx = 1;
            break;
          }
         case 1:
          {
            max_avc_gain = 42;
            avcNdx = 2; 
            break;
          }
         case 2:
          {
            max_avc_gain = 36;
            avcNdx = 3; 
            break;
          }
         case 3:
          {
            max_avc_gain = 30;
            avcNdx = 4; 
            break;
          }
         
       }
// Sets AVC gain
      si4735.setAvcAmMaxGain(max_avc_gain);
      showStatus();
	}
	
	
  }
  
//==================================================================
//==================================================================

// Check STEP
  if (temp16 & STEP_SWITCH)
    {
// This command should work only for SSB mode
      if (currentMode == LSB || currentMode == USB)
       {
         if (currentBFOStep == 25)
           {
             currentBFOStep = 100;
             realBFOStep = 100;
           }
         else if (currentBFOStep == 100)
           {
             currentBFOStep = 1;
             realBFOStep = 1000;
           }
                  else if (currentBFOStep == 1)
           {
             currentBFOStep = 5;
             realBFOStep = 5000;
           }
         else if (currentBFOStep == 5)
           {
             currentBFOStep = 10;
             realBFOStep = 10000;
           }
         else if (currentBFOStep == 10)
           {
             currentBFOStep = 50;
             realBFOStep = 50000;
           }
         else
           {
             currentBFOStep = 25;
             realBFOStep = 25;
           }

       }
      else
       if (currentMode == AM)
        {
          if (currentStep == 1)
            currentStep = 5;
          else if (currentStep == 5)
            currentStep = 9;
          else if (currentStep == 9)
            currentStep = 50;
          else
            currentStep = 1;
          si4735.setFrequencyStep(currentStep);
          band[bandIdx].currentStep = currentStep;
          si4735.setSeekAmSpacing((band[bandIdx].currentStep > 10) ? 10 : band[bandIdx].currentStep); // Max 10kHz for spacing
        }
      if (currentMode != FM)
       showStatus();
    }
//==================================================================
//==================================================================
    // Check MODE ( Pulsacion corta = MUTE | Pulsacion larga = AM / LSB / USB )
  static bool modeBtnPressed = false;
  static bool modeLongHandled = false;
  static unsigned long modePressTime = 0;

  if (button & MODE_SWITCH)
  {
    if (!modeBtnPressed)
    {
      modeBtnPressed = true;
      modeLongHandled = false;
      modePressTime = millis();
    }
    else if (!modeLongHandled && (millis() - modePressTime) >= 600)
    {
      modeLongHandled = true;
      if (band[bandIdx].bandType != FM_BAND_TYPE)
      {
        if (currentMode == AM)
        {
          currentMode = LSB;
          currentBFO = 0;
        }
        else if (currentMode == LSB)
        {
          currentMode = USB;
          currentBFO = 0;
        }
        else
        {
          currentMode = AM;
          ssbLoaded = false;
          currentBFO = 0;
        }

        if (mode_mute == false)
        {
          if (currentMode == LSB || currentMode == USB)
          {
            volume_ssb = volume + 5;
            if (volume_ssb > 63) volume_ssb = 63;
            si4735.setVolume(volume_ssb);
          }
          else
          {
            si4735.setVolume(volume);
          }
        }
        band[bandIdx].currentFreq = currentFrequency;
        useBand();
        if (mode_mute == true)
        {
          oled.setCursor(113, 0);
          oled.print("MT");
        }
      }
    }
  }
  else if (modeBtnPressed)
  {
    if (!modeLongHandled)
    {
      // Pulsacion corta: MUTE
      if (mode_mute == false)
      {
        si4735.setVolume(0);
        si4735.setAudioMute(true); // Corta la salida de audio por hardware
        mode_mute = true;
        oled.setCursor(113, 0);
        oled.print("MT");
      }
      else
      {
        si4735.setAudioMute(false); // Reactiva la salida de audio
        if (currentMode == LSB || currentMode == USB)
        {
          volume_ssb = volume + 5;
          if (volume_ssb > 63) volume_ssb = 63;
          si4735.setVolume(volume_ssb);
        }
        else
        {
          si4735.setVolume(volume);
        }
        mode_mute = false;
        showVolume();
      }
    }
    modeBtnPressed = false;
  }


//==================================================================
//==================================================================
// Check BFO_SWITCH
 if (temp16 & BFO_SWITCH)
  {
    if (currentMode == LSB || currentMode == USB)
      {
        if (realBFOStep <= 100)
          {
            currentBFOStep = 1;
            realBFOStep = 1000;
          }
        else
          {
            currentBFOStep = 100;
            realBFOStep = 100;
          }
        showSTEP();
      }
      else if (currentMode == FM || currentMode == AM)
       {
// Jumps up or down one space
        if (seekDirection)
          si4735.frequencyUp();
        else
          si4735.frequencyDown();
          
        si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, seekDirection);
        delay(100); //30
        currentFrequency = si4735.getFrequency();
        showFrequency();
      }
    } 

//==================================================================
//==================================================================

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 2) {
    si4735.getCurrentReceivedSignalQuality();
    int aux = si4735.getCurrentRSSI();
    if (rssi != aux) {
      rssi = aux;
      showRSSI();
    }

    if (countRSSI++ > 3) {
      //disableCommand(NULL, false, NULL);  // disable all command buttons
      countRSSI = 0;
    }
    elapsedRSSI = millis();
  }

  if (currentMode == FM)
  {
    if (currentFrequency != previousFrequency)
    {
      cleanBfoRdsInfo();
      previousFrequency = currentFrequency;
    }
      
    checkRDS();
  }

//==================================================================
//==================================================================

// Update battery status every 2 minutes
  if ( (millis() - elapsedBatt ) > 120000)
  {
     showBattery();
     elapsedBatt = millis();
  }  
//==================================================================
//==================================================================

// Check (VOL_UP + VOL_DOWN)
// Detectar si has cambiado de frecuencia, banda o volumen para marcar guardado
  static uint16_t lastSavedFreq = 0;
  static uint8_t lastSavedVol = 0;
  static int lastSavedBand = -1;
  static int lastSavedBFO = 0;

if (currentFrequency != lastSavedFreq || volume != lastSavedVol || bandIdx != lastSavedBand || currentBFO != lastSavedBFO)
{
  lastSavedFreq = currentFrequency;
  lastSavedVol = volume;
  lastSavedBand = bandIdx;
  lastSavedBFO = currentBFO;
  elapsedSave = millis();
  needSave = true;
}


// Guardar automaticamente tras 10 segundos sin cambios
/*if (needSave && (millis() - elapsedSave) > 10000)
  {
    saveAllReceiverInformation();
    needSave = false;
  }
*/
  
// Save all Receiver Information & invert display if pushed buttons (VOL_UP + VOL_DOWN) 
  
  if ((temp16 & VOL_UP) && (temp16 & VOL_DOWN))
    {
      saveAllReceiverInformation();
      
      // Muestra "SAV" en la pantalla
      oled.setCursor(100, 6);
      oled.print(" SAV");

      // Hace el destello invertido de medio segundo
      oled.invertDisplay(1);
      delay(500);                   // 0,5 sec
      oled.invertDisplay(0);

      while (StateBut);             // Espera a que sueltes los botones
      
      // Borra "SAV" y vuelve a mostrar el icono de la batería
      showBattery();
    }

  
//==================================================================
//==================================================================

  delay(10);
}

//================== End program ===============================
