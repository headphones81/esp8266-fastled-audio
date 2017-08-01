/*
   ESP8266 + FastLED + IR Remote: https://github.com/jasoncoon/esp8266-fastled-audio
   Copyright (C) 2015-2016 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Portions of this file are adapted from the work of Stefan Petrick:
// https://plus.google.com/u/0/115124694226931502095

// Portions of this file are adapted from RGB Shades Audio Demo Code by Garrett Mace:
// https://github.com/macetech/RGBShadesAudio

// Pin definitions
#define MSGEQ7_AUDIO_PIN A0
#define MSGEQ7_STROBE_PIN D0
#define MSGEQ7_RESET_PIN  D3

#define AUDIODELAY 0

// Smooth/average settings 
#define SPECTRUMSMOOTH 0.08 //0.08
#define PEAKDECAY 0.05 //0.01
#define NOISEFLOOR 575 //575

// AGC settings
#define AGCSMOOTH 0.004 //0.004
#define GAINUPPERLIMIT 15.0 //15.0
#define GAINLOWERLIMIT 0.1 //0.1

// Global variables
unsigned int spectrumValue[7];  // holds raw adc values
float spectrumDecay[7] = {0};   // holds time-averaged values
float spectrumPeaks[7] = {0};   // holds peak values
float audioAvg = 270.0;
float gainAGC = 0.0;

uint8_t spectrumByte[7];        // holds 8-bit adjusted adc values

uint8_t spectrumAvg;

unsigned long currentMillis; // store current loop's millis value
unsigned long audioMillis; // store time of last audio update

void initializeAudio() {
  pinMode(MSGEQ7_AUDIO_PIN, INPUT);
  pinMode(MSGEQ7_RESET_PIN, OUTPUT);
  pinMode(MSGEQ7_STROBE_PIN, OUTPUT);

  digitalWrite(MSGEQ7_RESET_PIN, LOW);
  digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
}

void readAudio() {
  static PROGMEM const byte spectrumFactors[7] = {10, 11, 11, 11, 11, 11, 10}; // 9 11 13 13 12 12 13

  // reset MSGEQ7 to first frequency bin
  digitalWrite(MSGEQ7_RESET_PIN, HIGH);
  delayMicroseconds(39); //5 default
  digitalWrite(MSGEQ7_RESET_PIN, LOW);

  // store sum of values for AGC
  int analogsum = 0;

  // cycle through each MSGEQ7 bin and read the analog values
  for (int i = 0; i < 7; i++) {

    // set up the MSGEQ7
    digitalWrite(MSGEQ7_STROBE_PIN, LOW);
    delayMicroseconds(39); // to allow the output to settle

    // read the analog value
    spectrumValue[i] = analogRead(MSGEQ7_AUDIO_PIN);
    digitalWrite(MSGEQ7_STROBE_PIN, HIGH);

    // noise floor filter
   // spectrumValue[i] = (spectrumValue[i] * pgm_read_byte_near(spectrumFactors + i)) / 10;
    if (spectrumValue[i] < NOISEFLOOR) {
      spectrumValue[i] = 0;
    } else {
      spectrumValue[i] -= NOISEFLOOR;
    }

    // apply correction factor per frequency bin
    spectrumValue[i] = (spectrumValue[i] * pgm_read_byte_near(spectrumFactors + i)) / 10;

    // prepare average for AGC
    analogsum += spectrumValue[i];

    // apply current gain value
    spectrumValue[i] *= gainAGC;

    // process time-averaged values
    spectrumDecay[i] = (int(spectrumDecay[i] * 7) + spectrumValue[i]) >> 3; //Just for test
    spectrumDecay[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecay[i] + SPECTRUMSMOOTH * spectrumValue[i];

    // process peak values
    if (spectrumPeaks[i] < spectrumDecay[i]) spectrumPeaks[i] = spectrumDecay[i];
    spectrumPeaks[i] = spectrumPeaks[i] * (1.0 - PEAKDECAY);

    spectrumByte[i] = spectrumValue[i] / 4;
  }

  // Calculate audio levels for automatic gain
  audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 7.0);

  spectrumAvg = (analogsum / 7.0) / 4;

  // Calculate gain adjustment factor
  gainAGC = 270.0 / audioAvg;
  if (gainAGC > GAINUPPERLIMIT) gainAGC = GAINUPPERLIMIT;
  if (gainAGC < GAINLOWERLIMIT) gainAGC = GAINLOWERLIMIT;
}

// Attempt at beat detection
byte beatTriggered = 0;
#define beatLevel 20.0
#define beatDeadzone 30.0
#define beatDelay 50
float lastBeatVal = 0;
byte beatDetect() {
  static float beatAvg = 0;
  static unsigned long lastBeatMillis;
  float specCombo = (spectrumDecay[0]*2.5 + spectrumDecay[1]) / 2.0;
  beatAvg = (1.0 - AGCSMOOTH) * beatAvg + AGCSMOOTH * specCombo;

  if (lastBeatVal < beatAvg) lastBeatVal = beatAvg;
  if ((specCombo - beatAvg) > beatLevel && beatTriggered == 0 && currentMillis - lastBeatMillis > beatDelay) {
    beatTriggered = 1;
    lastBeatVal = specCombo;
    lastBeatMillis = currentMillis;
    return 1;
  } else if ((lastBeatVal - specCombo) > beatDeadzone) {
    beatTriggered = 0;
    return 0;
  } else {
    return 0;
  }
}

void fade_down(uint8_t value) {
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i].fadeToBlackBy(value);
  }
}

void spectrumPaletteWaves()
{
//  fade_down(1);

  CRGB color6 = ColorFromPalette(gCurrentPalette, spectrumByte[5], spectrumByte[6]);
  CRGB color5 = ColorFromPalette(gCurrentPalette, spectrumByte[5] / 8, spectrumByte[6] / 8);
  CRGB color1 = ColorFromPalette(gCurrentPalette, spectrumByte[1] / 2, spectrumByte[2] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[CENTER_LED] = color;
  leds[CENTER_LED].fadeToBlackBy(spectrumByte[3] / 12);

  leds[CENTER_LED - 1] = color;
  leds[CENTER_LED - 1].fadeToBlackBy(spectrumByte[3] / 12);

  //move to the left
  EVERY_N_MILLISECONDS(25) {
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];
  }
  }
}

void spectrumPaletteWaves2()
{
//  fade_down(1);

  CRGBPalette16 palette = palettes[currentPaletteIndex];

  CRGB color6 = ColorFromPalette(palette, 255 - spectrumByte[4], spectrumByte[4]);
  CRGB color5 = ColorFromPalette(palette, 255 - spectrumByte[5] / 8, spectrumByte[5] / 8);
  CRGB color1 = ColorFromPalette(palette, 255 - spectrumByte[1] / 2, spectrumByte[1] / 2);

  CRGB color = nblend(color6, color5, 256 / 8);
  color = nblend(color, color1, 256 / 2);

  leds[CENTER_LED] = color;
  leds[CENTER_LED].fadeToBlackBy(spectrumByte[3] / 12);

  leds[CENTER_LED - 1] = color5;
  leds[CENTER_LED - 1].fadeToBlackBy(spectrumByte[3] / 12);
EVERY_N_MILLISECONDS(50){
  //move to the left
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];
  }
  }
}

void spectrumWaves()
{
 fade_down(2);
 blur1d( leds, NUM_LEDS, map(spectrumByte[0],0,255,64,172)); 
 CRGB bass = CRGB(spectrumByte[1],spectrumByte[0]/5, 0 );
 CRGB high = CRGB(0, spectrumByte[6]/5, spectrumByte[5] );
leds[map(spectrumByte[1],0,255,0,CENTER_LED/2)] = bass;
leds[map(spectrumByte[5],0,255,CENTER_LED,CENTER_LED/2)] = high;
EVERY_N_MILLISECONDS(37){
  //move to the left
  for (int i = CENTER_LED; i > 0; i--) {
    leds[i] = leds[i - 1];
     leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
for (int i = CENTER_LED; i < NUM_LEDS - 1; i++) {
    leds[i] = leds[NUM_LEDS - i];
     leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
 }
}

void spectrumWavesP()
{
 fade_down(2);
  blur1d( leds, NUM_LEDS, map(spectrumByte[0],0,255,64,172)); 
CRGB bass = ColorFromPalette(palettes[currentPaletteIndex], spectrumByte[1], spectrumByte[1],LINEARBLEND);
 //CRGB bass = CRGB(spectrumByte[1],spectrumByte[0]/5, 0 );
 //CRGB high = CRGB(0, spectrumByte[6]/5, spectrumByte[5] );
 CRGB high = ColorFromPalette(palettes[currentPaletteIndex], 255 - spectrumByte[5], spectrumByte[5],LINEARBLEND);
leds[map(spectrumByte[1],0,255,0,CENTER_LED/2)] = bass;
leds[map(spectrumByte[5],0,255,CENTER_LED-1,CENTER_LED/2)] = high;
EVERY_N_MILLISECONDS(25){
  //move to the left
  for (int i = CENTER_LED ; i > 0; i--) {
    leds[i] = leds[i - 1];
     leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
for (int i = CENTER_LED ; i < NUM_LEDS - 1; i++) {
    leds[i] = leds[NUM_LEDS - i];
     leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
 }
}
void spectrumWaves2()
{
  //fade_down(1);

  CRGB color = CRGB(spectrumByte[1],spectrumByte[3], spectrumByte[5] );
  CRGB color2 = CRGB(spectrumByte[0],spectrumByte[2], spectrumByte[4] );
  leds[CENTER_LED] = (color);
 // leds[CENTER_LED].fadeToBlackBy(spectrumByte[3] / 2);
 
  leds[CENTER_LED - 1] = (color2);
 // leds[CENTER_LED - 1].fadeToBlackBy(spectrumByte[3] / 2);
 EVERY_N_MILLISECONDS(37){
  //move to the left
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
 
    leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];

    leds[i].fadeToBlackBy(spectrumByte[3] / 2);
  }
  }
}

void spectrumWaves4()
{
  fade_down(2);
 blur1d( leds, NUM_LEDS, map(spectrumByte[0],0,255,21,186)); 
  CRGB color = CRGB(spectrumByte[1], 0, spectrumByte[0]);
  CRGB color2 = CRGB(0, spectrumByte[6]/2, spectrumByte[5]);
  leds[0] = color;
  //leds[0].fadeToBlackBy(cooling);
leds[CENTER_LED-spectrumByte[3]/30] = color2;
leds[CENTER_LED+spectrumByte[3]/30] = color2;
leds[CENTER_LED-spectrumByte[3]/30,CENTER_LED+spectrumByte[3]/30].fadeToBlackBy(spectrumByte[4]/2);
  leds[NUM_LEDS - 1] = color;
 // leds[NUM_LEDS - 1].fadeToBlackBy(cooling);

  //move to the left
  EVERY_N_MILLISECONDS(10){
  for (int i = CENTER_LED - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = CENTER_LED + 1; i < NUM_LEDS - 1; i++) {
    leds[i] = leds[i + 1];
   }
  }
 /*  EVERY_N_MILLISECONDS(35){
  for (int i = CENTER_LED; i <= NUM_LEDS ; i++) {
    leds[i].green = color2;
    leds[i].green = leds[i - 1,i + spectrumByte[3]/24 ];
  }
 }*/
yield();
}

void spectrumWaves3()
{
  //fade_down(1);
CRGB color = CRGB(spectrumByte[1], spectrumByte[0], 0);
CRGB color2 = CRGB(0, spectrumByte[3]/2, spectrumByte[4]/5);
CRGB color3 = CRGB(spectrumByte[6]/2, 0, spectrumByte[5]/1.5); 
  leds[NUM_LEDS/6+spectrumByte[1]/28] = color;
  leds[NUM_LEDS - NUM_LEDS/6-spectrumByte[1]/28] = color;
  leds[NUM_LEDS/3+spectrumByte[3]/30] = color2;
  leds[NUM_LEDS - NUM_LEDS/3-spectrumByte[3]/30] = color2;
  leds[NUM_LEDS/2+spectrumByte[5]/29] = color3;
  leds[NUM_LEDS - NUM_LEDS/2-spectrumByte[5]/29] = color3;
 EVERY_N_MILLISECONDS(30){
  //move to the left
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
     leds[i].fadeToBlackBy(cooling);
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];
     leds[i].fadeToBlackBy(cooling);
  }
  }
yield();
}

//RADIATE
void radiate() {
 
int  zero  = constrain((spectrumByte[0]+spectrumByte[1])/2,0,255);
int  three = constrain((spectrumByte[4]/2+spectrumByte[3])/2,0,127);
int  six   = constrain((spectrumByte[5]+spectrumByte[6])/2,0,255);
 leds[CENTER_LED] = CRGB(zero, three, six);
// leds[45] = CRGB(zero, three, six);
  // high frequencies = blue = FAST SPEED
  EVERY_N_MILLISECONDS(25) { //25
    for (int i = NUM_LEDS; i > CENTER_LED; i--) {
      leds[i].blue = leds[i - 1].blue;
    leds[i].fadeToBlackBy( (six+1)/2.5 );
    }
  }
 
  // mid frequencies = green = MEDIUM SPEED
  EVERY_N_MILLISECONDS(38) { //38
    for (int i = NUM_LEDS; i > CENTER_LED; i--) {
      leds[i].green = leds[i - 1].green;
      
    }
  }
 
  // low frequencies = red = SLOW SPEED
  EVERY_N_MILLISECONDS(43) { //43
    for (int i = NUM_LEDS; i > CENTER_LED; i--) {
      leds[i].red = leds[i - map(spectrumByte[0],0,255,0,3)].red;
       leds[i].red = leds[i - 1].red;
      //leds[i].nscale8((six+1)/1.6 );
     // leds[i].fadeToBlackBy( (spectrumByte[0]) );
    }
  }
 
  // for mirroring to the other half of the strip
  for (int i = NUM_LEDS; i > CENTER_LED; i--) {
    leds[CENTER_LED - (i - CENTER_LED)] = leds[i - 1];
    
  }
  yield();
 // FastLED.show();
}
void radiate2() {

int  zero  = constrain((spectrumValue[0]+spectrumByte[1]+spectrumByte[2])/3,0,255);
int  three = constrain((spectrumByte[4]/2+spectrumByte[3])/2,0,127);
int  six   = constrain((spectrumByte[5]/1.25+spectrumByte[6])/2,0,255);
 leds[NUM_LEDS-1] = CRGB(zero, three, six);
 leds[0]= CRGB(zero, three, six);
// leds[45] = CRGB(zero, three, six);
  // high frequencies = blue = FAST SPEED
  EVERY_N_MILLISECONDS(25) { //25
    for (int i = CENTER_LED -1 ;i >0; i--) {
      
      leds[i].blue = CRGB(0, 0, six+i);
      leds[i].blue = leds[i-1].blue;
    }
  }
 
  // mid frequencies = green = MEDIUM SPEED
  EVERY_N_MILLISECONDS(38) { //38
    for (int i = CENTER_LED -1 ;i >0; i--) {
      leds[i].green = leds[i - 1].green;
      
    }
  }
 
  // low frequencies = red = SLOW SPEED
  EVERY_N_MILLISECONDS(43) { //43
  for (int i = CENTER_LED -1 ;i >0; i--) {
      //leds[i].red = leds[i - 1,i-2].red;
       leds[i].red = CRGB(zero, 0, 0);
       leds[i].red = leds[i-1].red;
      //leds[i].nscale8((six+1)/1.6 );
     leds[i].fadeToBlackBy( (six+1)/2 );
 
    }
  }
 
  // for mirroring to the other half of the strip
  for (int i = NUM_LEDS -1 ; i > CENTER_LED +1 ; i--) {
    leds[i] = leds[NUM_LEDS  - i];
     
  }
 blur1d( leds, CENTER_LED/7, map(zero,0,255,0,3)); 
}

void radiateBass() {
 int  six   = constrain((spectrumByte[5]/1.25+spectrumByte[6])/2,0,255);
int  zero  = constrain((spectrumValue[0]*1.25+spectrumByte[1])/3,0,255);
 leds[0] = CRGB(zero, 0, 0);
 leds[NUM_LEDS-1] = CRGB(zero, 0, 0);



  EVERY_N_MILLISECONDS(38) { //43
  
  for (int i = CENTER_LED -1 ;i >0; i--) {
     if (beatDetect()) {
 leds[ i ] = CRGB(zero/2, 0, zero);
 leds[ CENTER_LED - spectrumByte[0]/25 ] = CRGB(zero/2, 0, zero);
  leds[CENTER_LED - spectrumByte[1]/25  ] = CRGB(zero/2, 0, zero/4);
  leds[i].fadeToBlackBy(cooling);
 }
      
      else{ leds[i].red = CRGB(zero, 0,0);
       leds[i].red = leds[i-1].red;
       leds[i].blue = leds[i-1].blue;}
      //leds[i].nscale8((six+1)/1.6 );
    leds[i].fadeToBlackBy( (six+1)/2 );
  }
    
  }
 
  // for mirroring to the other half of the strip
  for (int i = NUM_LEDS -1 ; i > CENTER_LED ; i--) {
    leds[i] = leds[NUM_LEDS  - i];
     
  }
}

void radiateTreb() {
 
int  six   = constrain((spectrumByte[5]+spectrumByte[6])/2,0,255);
 leds[CENTER_LED - spectrumByte[5]/25 ] = CRGB(spectrumByte[6]/6, spectrumByte[6]/6, spectrumByte[5]);
 leds[0] = CRGB(0, 0, six);
  EVERY_N_MILLISECONDS(43) { //43
  for (int i = CENTER_LED;i >0; i--) {
       leds[i].blue = CRGB(0, 0, six);
       leds[i].blue = leds[i-1].blue;
      //leds[i].nscale8((six+1)/1.6 );
   leds[i].fadeToBlackBy( (six+1)/2.5);
 
    }
  }
 
  // for mirroring to the other half of the strip
  for (int i = NUM_LEDS -1 ; i > CENTER_LED ; i--) {
    leds[i] = leds[NUM_LEDS  - i];
     
  }
}
void radiateNoise() {

  radiateBass();
  radiateTreb();
  EVERY_N_MILLISECONDS(25) { 
  if (beatDetect()) {
  pnoise2(); 
    blur1d( leds, NUM_LEDS, map(spectrumByte[0],0,255,64,172)); 
  }
  }
}
//RADIATE
void analyzerColumns()
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  const uint8_t columnSize = NUM_LEDS / 7;

  for (uint8_t i = 0; i < 7; i++) {
    uint8_t columnStart = i * columnSize;
    uint8_t columnEnd = columnStart + columnSize;

    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

    uint8_t columnHeight = map8(spectrumByte[i], 1, columnSize);

    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
      if (j >= NUM_LEDS || j >= columnEnd)
        continue;

      leds[j] = CHSV(i * 40, 255, 255);
    }
  }
}

void analyzerPeakColumns()
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  const uint8_t columnSize = NUM_LEDS / 7;

  for (uint8_t i = 0; i < 7; i++) {
    uint8_t columnStart = i * columnSize;
    uint8_t columnEnd = columnStart + columnSize;

    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;

    uint8_t columnHeight = map(spectrumValue[i], 0, 1023, 0, columnSize);
    uint8_t peakHeight = map(spectrumPeaks[i], 0, 1023, 0, columnSize);
if ( peakHeight > columnEnd ) { peakHeight = columnEnd -1;}
    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
      if (j < NUM_LEDS && j <= columnEnd) {
        leds[j] = CHSV(i * 40, 255, 128);
      }
    }

    uint8_t k = columnStart + peakHeight;
    if (k < NUM_LEDS && k <= columnEnd)
      leds[k] = CHSV(i * 40, 255, 255);
  }
}

void beatWaves()
{
  fade_down(2);

  if (beatDetect()) {
    leds[CENTER_LED] = CRGB::Red;
  }

  //move to the left
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];
  }
}

void beatWaves2()
{
  fade_down(2);
CRGB color = ColorFromPalette(palettes[currentPaletteIndex], spectrumByte[1], 255);
  if (beatDetect()) {
    leds[CENTER_LED] = color;
  }
  //move to the left
  for (int i = NUM_LEDS - 1; i > CENTER_LED; i--) {
    leds[i] = leds[i - 1];
  }
  // move to the right
  for (int i = 0; i < CENTER_LED; i++) {
    leds[i] = leds[i + 1];
  }
}


#define VUFadeFactor 5
#define VUScaleFactor 2.5
#define VUPaletteFactor 1.7
void drawVU() {
  CRGB pixelColor;

  const float xScale = 255.0 / (NUM_LEDS / 2);
  float specCombo = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0;

  for (byte x = 0; x < NUM_LEDS / 2; x++) {
    int senseValue = specCombo / VUScaleFactor - xScale * x;
    int pixelBrightness = senseValue * VUFadeFactor;
    if (pixelBrightness > 255) pixelBrightness = 255;
    if (pixelBrightness < 0) pixelBrightness = 0;

    int pixelPaletteIndex = senseValue / VUPaletteFactor - 15;
    if (pixelPaletteIndex > 240) pixelPaletteIndex = 240;
    if (pixelPaletteIndex < 0) pixelPaletteIndex = 0;

    pixelColor = ColorFromPalette(palettes[currentPaletteIndex], pixelPaletteIndex, pixelBrightness);

    leds[x] = pixelColor;
    leds[NUM_LEDS - x - 1] = pixelColor;
  }
}

void drawVU2() {
  uint8_t avg = map8(spectrumAvg*2, NUM_LEDS/2 - 1,NUM_LEDS -1 );
 
  for (uint8_t i = NUM_LEDS/2; i < NUM_LEDS; i++) {
    if(i <= avg) {
      leds[i] = ColorFromPalette(palettes[currentPaletteIndex], (480 / NUM_LEDS) * i);
    }
    else {
      leds[i] = CRGB::Black;
    }
 for (int i = NUM_LEDS; i > NUM_LEDS/2; i--) {
    leds[NUM_LEDS/2 - (i - NUM_LEDS/2)] = leds[i - 1];
 }
  }
}

void Ajuggle()
{
  static uint8_t    numdots =   7; // Number of dots in use.
  static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
  static uint8_t   basebeat =   5;
 uint8_t blurAmount = dim8_raw( beatsin8(3,64, 172) );       // A sinewave at 3 Hz with values ranging from 64 to 192.
  blur1d( leds, NUM_LEDS, blurAmount);                        // Apply some blurring to whatever's already on the strip, which will eventually go black.
  
  static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch (secondHand) {
      case  0:  basebeat = 7;  faderate = 12;  break; // You can change values here, one at a time , or altogether.
      case 10:  basebeat = 5;  faderate = 24;  break;
      case 20:  basebeat =  3;  faderate = 32;  break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
 // curhue = thishue; // Reset the hue values.
  fadeToBlackBy(leds, NUM_LEDS, faderate);
  for ( int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[beatsin16(basebeat + i + map(spectrumByte[i],0,255,1,5), 0, NUM_LEDS)] += CHSV(i*40 + map(spectrumByte[i],0,255,1,20), 255, map(spectrumByte[i],0,255,30,255));
  }
}

void drawVU3() {
  CRGB pixelColor;

  const float xScale = 255.0 / (NUM_LEDS / 2);
  float specCombo = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0;

  for (byte x = 0; x < NUM_LEDS / 3; x++) {
    int senseValue = specCombo / VUScaleFactor - xScale * x;
    int pixelBrightness = senseValue * VUFadeFactor;
    if (pixelBrightness > 255) pixelBrightness = 255;
    if (pixelBrightness < 0) pixelBrightness = 0;

    int pixelPaletteIndex = senseValue / VUPaletteFactor - 15;
    if (pixelPaletteIndex > 240) pixelPaletteIndex = 240;
    if (pixelPaletteIndex < 0) pixelPaletteIndex = 0;

    pixelColor = ColorFromPalette(palettes[currentPaletteIndex], pixelPaletteIndex, pixelBrightness);

    leds[x] = pixelColor;
    leds[NUM_LEDS - x - 1] = pixelColor;
  }
for (int i = NUM_LEDS/3 ; i < NUM_LEDS- NUM_LEDS/3 ; i++){
  leds[i] = CRGB::Black;
}
}

void analyzerPeakColumns2()
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  const uint8_t columnSize = CENTER_LED / 7;

  for (uint8_t i = 0; i < 7; i++) {
    uint8_t columnStart = i * columnSize;
    uint8_t columnEnd = columnStart + columnSize;

    if (columnEnd >= CENTER_LED) columnEnd = CENTER_LED - 1;

    uint8_t columnHeight = map(spectrumValue[i], 0, 1023, 0, columnSize);
    uint8_t peakHeight = map(spectrumPeaks[i], 0, 1023, 0, columnSize);

    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
      if (j < CENTER_LED && j <= columnEnd) {
        leds[j] = CHSV(i * 40+gHue, 255, 128);
      }
    }

    uint8_t k = columnStart + peakHeight;
    if (k < NUM_LEDS && k <= columnEnd)
      leds[k] = CHSV(i * 40-gHue, 255,127 + spectrumByte[i]/2);
  }
  for (int i = CENTER_LED; i < NUM_LEDS-1; i++) {
    leds[i] = leds[NUM_LEDS - i];
    
  }
   //uint8_t blurAmount = dim8_raw( beatsin8(3,64, 172) );       // A sinewave at 3 Hz with values ranging from 64 to 192.
  blur1d( leds, NUM_LEDS, map(spectrumByte[0],0,255,64,172));                        // Apply some blurring to whatever's already on the strip, which will eventually go black.
  
}
void VUPeak()
{
  fill_solid(leds, NUM_LEDS-1, CRGB::Black);
  const uint8_t columnSize = NUM_LEDS /3;
    uint8_t columnStart = 0;
    uint8_t columnEnd = columnStart + columnSize;
    if (columnEnd >= NUM_LEDS/3) columnEnd = NUM_LEDS/3;
    //uint8_t columnHeight = map(spectrumValue[0], 0, 1023, 0, columnSize);
 //uint8_t   columnHeight = ((columnHeight * 7) + int(spectrumPeaks[0]+spectrumPeaks[1])/2) >> 3;
 uint8_t   columnHeight = map((spectrumPeaks[0]+spectrumPeaks[1])/2, 0, 1023, 0, columnSize);
    uint8_t peakHeight = map((spectrumPeaks[0]+spectrumPeaks[1])/2, 0, 1023, 0, columnSize);
if (peakHeight < columnHeight )  { peakHeight = columnHeight+1;}
else if (peakHeight > NUM_LEDS/3)  { peakHeight = columnHeight;}
    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
      if (j < NUM_LEDS/3 && j <= columnEnd) {
        leds[j] = CHSV(30-j*3, 255, 128);
      }
    }

    uint8_t k = columnStart + peakHeight;
    if (k < NUM_LEDS/3 && k <= columnEnd) {
      leds[k] = CHSV(30+k*4, 255, 255);}
   else k =  columnEnd; 
       
       for (int i = CENTER_LED; i < NUM_LEDS; i++) {
    leds[i] = leds[NUM_LEDS - i];
  }
}

void VUPeak2()
{
  fill_solid(leds, NUM_LEDS-1, CRGB::Black);
  const uint8_t columnSize = NUM_LEDS /3;
    uint8_t columnStart = 0;
    uint8_t columnEnd = columnStart + columnSize;
    if (columnEnd >= NUM_LEDS/3) columnEnd = NUM_LEDS/3;
    //uint8_t columnHeight = map(spectrumValue[0], 0, 1023, 0, columnSize);
 //uint8_t   columnHeight = ((columnHeight * 7) + int(spectrumPeaks[0]+spectrumPeaks[1])/2) >> 3;
 uint8_t   columnHeight = map((spectrumPeaks[0]+spectrumPeaks[1])/2, 0, 1023, 0, columnSize);
    uint8_t peakHeight = map((spectrumPeaks[0]+spectrumPeaks[1])/2, 0, 1023, 0, columnSize);
if (peakHeight < columnHeight )  { peakHeight = columnHeight+1;}
else if (peakHeight > NUM_LEDS/3)  { peakHeight = columnHeight;}
    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
      if (j < NUM_LEDS/3 && j <= columnEnd) {
        leds[j] = CHSV(gHue-j*3, 255, 128);
      }
    }

    uint8_t k = columnStart + peakHeight;
    if (k < NUM_LEDS/3 && k <= columnEnd) {
      leds[k] = CHSV(gHue+k*3, 255, 255);}
   else k =  columnEnd; 
       
       for (int i = CENTER_LED; i < NUM_LEDS; i++) {
    leds[i] = leds[NUM_LEDS - i];
  }
}


void VUPeak3()
{ 
  //fill_solid(leds, NUM_LEDS, CRGB::Black);
  const uint8_t columnSize = NUM_LEDS / 2;
    uint8_t columnStart = 0;
    uint8_t columnEnd = columnStart + columnSize;
    if (columnEnd >= NUM_LEDS) columnEnd = NUM_LEDS - 1;
    //uint8_t columnHeight = map(spectrumValue[0], 0, 1023, 0, columnSize);
 uint8_t   columnHeight = ((columnHeight * 7) + (spectrumByte[0]+spectrumByte[1])/2) >> 3;
    uint8_t peakHeight = map((spectrumPeaks[0]+spectrumPeaks[1])/2, 0, 1023, 0, columnSize);
if (peakHeight <= columnHeight ) { peakHeight = columnHeight + 1;}
    for (uint8_t j = columnStart; j < columnStart + columnHeight; j++) {
    EVERY_N_MILLISECONDS(17) {//43
      if (j < NUM_LEDS && j <= columnEnd) {
        leds[j] = CHSV(gHue-j*3, 255, 128);
      }
    
    }

    uint8_t k = columnStart + peakHeight;
    if (k < NUM_LEDS/2 && k <= columnEnd){
      leds[k] = CHSV(gHue+k*4, 255, 255);}
    else { k = columnEnd; } 
       for (int i = CENTER_LED; i < NUM_LEDS; i++) {
    leds[i] = leds[NUM_LEDS - i];
  }
}
 fadeToBlackBy( leds, NUM_LEDS, 2);
}

void RadiatePeaks()
{
   
 //radiateBass();
radiateTreb();
VUPeak3();

fadeToBlackBy( leds, NUM_LEDS, 5);
}


