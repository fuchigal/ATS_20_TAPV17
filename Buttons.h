// Buttons controllers
#define MODE_SWITCH      0x10   // 4 = PD4 Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 0x20   // 5 = PD5 Used to select the banddwith.
#define VOL_UP           0x40   // 6 = PD6 Volume Up
#define VOL_DOWN         0x80   // 7 = PD7 Volume Down
#define BAND_BUTTON_UP   0x01   // 8 = PB0 Next band
#define BAND_BUTTON_DOWN 0x02   // 9 = PB1 Previous band
#define STEP_SWITCH      0x04   // 10= PB2 Select frequency step (1, 5, 10 or 100 kHz)
#define AGC_SWITCH       0x08   // 11= PB3 Switch AGC ON/OF
#define BFO_SWITCH       0x100  // 14= PC0 Select the enconder control (BFO or VFO) 

void InitButtons();
uint8_t ReadVcc();              // return 0...100 (%)
