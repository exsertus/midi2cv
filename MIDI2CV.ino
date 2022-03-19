
/*============================================================
  Name : MIDI2CV
  Author : exsertus.com (Steve Bowerman)
 
  Summary
  MIDI to CV convertor:
  - Single channel (works in omni or channel 1-15, via dip switches, all off = 0 = omni)
  - 1V/Octave CV note output (0-12V)
  - Clock gate (selectable quarter, half or sixteenth note resolution) (5V)
  - Note on/off gate (5V)
  - Start gate (5V) - useful for asserting reset
  - Velocity CV (12V)
  
  
  Version : 0.1

  Credits
  - Core MIDI-2 logic adapted from https://github.com/elkayem/midi2cv/blob/master/midi2cv.ino

  Physical mapping:
  - Based around Atega328 running at 16Mhz (ext crystal)
  
============================================================*/

#include <MIDI.h>
#include <SPI.h>

#define GATE 2
#define CLOCK 3
#define RESET 4
#define CS 9

#define CH_BIT4 5
#define CH_BIT3 6
#define CH_BIT2 7
#define CH_BIT1 8

#define RES_24 A0
#define RES_6 A1

#define NOTE_SF 47.069f

MIDI_CREATE_DEFAULT_INSTANCE();

int clock_count = 0;
int res = 12; // 6=16th, 12=Quarter, 24=Beat
int channel = 0;

bool notes[88] = {0}; 
int8_t noteOrder[20] = {0}, orderIndx = {0};


void setDAC(bool ch, unsigned int value, bool buffered = 0, bool gain = 1,  bool enabled = 1) {
  
  // CONF bits are channel, buffer, gain, enable
  byte msb = ch << 7 | buffered << 6 | gain << 5 | enabled << 4 | (value & 0xF00) >> 8;
  byte lsb = value & 0xFF; // assigning to byte retains last 8 bits of 12, looses first 4 (which are included in the msb)
  
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS, LOW);
  SPI.transfer(msb);
  SPI.transfer(lsb);
  digitalWrite(CS, HIGH);
  SPI.endTransaction(); 
}


void setup() { 
  pinMode(CLOCK,OUTPUT);
  digitalWrite(CLOCK,LOW);
  
  pinMode(RESET,OUTPUT);
  digitalWrite(RESET,LOW);

  pinMode(GATE,OUTPUT);

  pinMode(CS, OUTPUT);   
  digitalWrite(CS, HIGH);

  pinMode(CH_BIT4, INPUT_PULLUP);
  pinMode(CH_BIT3, INPUT_PULLUP);
  pinMode(CH_BIT2, INPUT_PULLUP);
  pinMode(CH_BIT1, INPUT_PULLUP);

  pinMode(RES_24, INPUT_PULLUP);
  pinMode(RES_6, INPUT_PULLUP);
  
  channel = !digitalRead( CH_BIT1 ) + ( !digitalRead( CH_BIT2 ) * 2 ) + ( !digitalRead( CH_BIT3 ) * 4 ) + ( !digitalRead( CH_BIT4 ) * 8 );
  
  SPI.begin();  
  
  delay(100);
  setDAC(0,0);
  setDAC(1,0);

  if (channel == 0) {
    digitalWrite(GATE,HIGH);
    delay(2000);
    digitalWrite(GATE,LOW);
    delay(2000);
  } else {
    for (int i=0; i<channel; i++) {
      digitalWrite(GATE,HIGH);
      delay(100);
      digitalWrite(GATE,LOW);
      delay(500);
    }
  }

  MIDI.begin(channel);

  //setDAC(0,4095);
  //while (true) {}
  
}

void loop() {
  int noteMsg, channel, velocity, d1, d2;
  static unsigned long clock_timer=0, clock_timeout=0, reset_timer=0, reset_timeout=0;

  if ((clock_timer > 0) && (millis() - clock_timer > 50)) { 
    digitalWrite(CLOCK,LOW); // Set clock pulse low after 50 msec 
    clock_timer = 0;  
  }

  if ((reset_timer > 0) && (millis() - reset_timer > 50)) { 
    digitalWrite(RESET,LOW); // Set trigger low after 50 msec 
    reset_timer = 0;  
  }
  
  if (MIDI.read()) {
    byte type = MIDI.getType();
    switch(type) {
      
      /*
       * 
       *  NoteOn / NoteOff
       * 
       */
        
      case midi::NoteOn:
      case midi::NoteOff:
        noteMsg = MIDI.getData1() - 21; // A0 = 21, Top Note = 108
        channel = MIDI.getChannel();
        
        if ((noteMsg < 0) || (noteMsg > 87)) break;

        if (type == midi::NoteOn) velocity = MIDI.getData2();
        else velocity = 0;  

        if (velocity == 0)  {
          notes[noteMsg] = false;
          digitalWrite(GATE,LOW);
          //setDAC(0,0);
          //setDAC(1,0);
        }
        else {
          notes[noteMsg] = true;
          // velocity range from 0 to 4095 mV  
          // Note scaling to 1V/Oct, Vcc = 5v, so 1/12v per note 0.0833, DAC res 4096 over 5v so 0.8192
          // 0v=0 1v=1024 2v=2048 3v=3072 4v=4096
          // Left shift velocity by 5 to scale from 0-127 0 to 0-4095
          // 0-87notes = 0-4095 = 0-5v = 46.54
          // Trimmer on TL072 A circuit scales max voltage to 7.33v), hence V/Octave
                   
          setDAC(1,velocity<<5);
       
        }

        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx+1) % 20;
          noteOrder[orderIndx] = noteMsg;  
          int8_t noteIndx;
  
          for (int i=0; i<20; i++) {
            noteIndx = noteOrder[ mod(orderIndx-i, 20) ];
            if (notes[noteIndx]) {
              unsigned int mV = (unsigned int) ((float) noteIndx * NOTE_SF + 0.5); 
              digitalWrite(GATE,HIGH);
              setDAC(0,mV);
              return;
            }
          }
          digitalWrite(GATE,LOW);  // All notes are off
                 
        }
        
        break;

      /* 
       *  
       *  Clock tick
       *  
       */
       
      case midi::Clock:
        // Set clock resolution based on 3x way switch (On1-off-on2), where On1 = 24 res, On2 = 6. Off defaults to 12 (middle)
        
        clock_timeout = millis();
        
        if (clock_count == 0) {
          digitalWrite(CLOCK,HIGH); // Start clock pulse
          clock_timer=millis();    
        }

        clock_count++;
        
        if (clock_count == res) clock_count = 0;  
       
        break;


      /*  
       *   
       *   Start / Stop
       *   
       */
       
      case midi::Stop:
      case midi::Start:
        res = 12;
        if (digitalRead(RES_24) == LOW) res = 24;
        if (digitalRead(RES_6) == LOW) res = 6;
  
        digitalWrite(RESET,HIGH);
        reset_timer = millis();
        clock_count = 0;
        setDAC(0,0);
        setDAC(1,0);
        digitalWrite(GATE,LOW);
        break;

      // Not implemented yet
      
      case midi::PitchBend:
        d1 = MIDI.getData1();
        d2 = MIDI.getData2();
        break;

      case midi::ControlChange: 
        d1 = MIDI.getData1();
        d2 = MIDI.getData2(); // From 0 to 127
        break;
   
      default:
        d1 = MIDI.getData1();
        d2 = MIDI.getData2();
        break;
     }
  }
}

int mod(int a, int b) {
    int r = a % b;
    return r < 0 ? r + b : r;
}
