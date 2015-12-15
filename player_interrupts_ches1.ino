/*************************************************** 
  This is an example for the Adafruit VS1053 Codec Breakout

  Designed specifically to work with the Adafruit VS1053 Codec Breakout 
  ----> https://www.adafruit.com/products/1381

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

/****************************************************
 * Define filelist linked list implementation
 ****************************************************/

class filelist { 
  char* _name = NULL;
  filelist* _next;
  filelist* _tail;
  int _length = 1;
  public:
  ~filelist(){
    delete _name;
  }
  filelist(char* name){
    _tail = this;
    _name = new char[strlen(name) + 1];
    strcpy(_name, name);
  }
  filelist* add(char* nextname){
    _length++;
    filelist* next = new filelist(nextname);
    _tail->_next = next;
    _tail = next;
    return this;
  }

  filelist* remove(int position){
    int i = 0;
    filelist* pos = this;
    filelist* followptr = NULL;
    while(pos->_next != NULL && i < position){
      followptr = pos;
      pos = pos->_next;
      i++;
    }
    if(i < position){
      //out of bounds
      return NULL;
    }
    _length--;
    if(pos->_next == NULL){
      //removing tail
      followptr->_next = NULL;
    } else {
      followptr->_next = pos->_next;
    }
    delete pos;
    return this;
  }

  int length(){
    Serial.println(_length);
    return _length;
  }

  char* getFileAt(int position){
    int i = 0;
    filelist* pos = this;
    while(pos->_next != NULL && i < position){
      pos = pos->_next;
      i++;
    }
    if(i < position){
      //out of bounds
      return NULL;
    }
    return pos->_name;
  }
};

enum ControlState {
  PAUSED,
  PLAYING
};

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>



//Seven Segment Display driver libraries
// Enable one of these two #includes and comment out the other.
// Conditional #include doesn't work due to Arduino IDE shenanigans.
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
//#include <TinyWireM.h> // Enable this line if using Adafruit Trinket, Gemma, etc.
// IMPORTANT WIRING NOTE FOR 7 SEGMENT DISPLAY:  CLK is the I2C clock, connect to Analog 5 (on the UNO)
//                                               DAT is the I2C data, connect ot Analog 4 (on the UNO)

#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);


//Song names array
char* songArray[3];
//Song names linked-list
filelist* songList = NULL;
ControlState state = PLAYING;
//// These are variables for volume control
int attenPotPin = A0;
int fadePotPin = A1;
int attenPotRead, fadePotRead;
int attenLeft = 255;    //volume is a uint8_t, so between 0 - 255, attenuation so lower == louder.
int attenRight = 255;
int attenMax;

// These are variables for the playPause, nextSong and prevSong buttons.
int playPausePin = 8;
int nextSongPin = 9;
int prevSongPin = 10;
int songNum = 1;  //the number of the currently selected song. this gets passed to the 7seg display.



//Using a custom timer instead of delay()

unsigned long timerStart = 0UL;
unsigned long timerA = 0UL;
unsigned long timerB = 0UL;
unsigned long timerC = 0UL;
unsigned long timerD = 0UL;

Adafruit_7segment matrix = Adafruit_7segment();

void setup() {
  Serial.begin(9600);
  matrix.begin(0x70);     //start the seven segment display
  Serial.println("Adafruit VS1053 Library Test");
  
  // Array adding
  
  songArray[0] = ("track001.mp3");
  songArray[1] = ("track002.mp3");
  
  //setup the button pins
  pinMode(playPausePin, INPUT_PULLUP);    //they will be pulled to ground when activated, hence the switch
  pinMode(nextSongPin, INPUT_PULLUP);
  pinMode(prevSongPin, INPUT_PULLUP);
  
  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }
  Serial.println(F("VS1053 found"));

  //musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // list files
  printDirectory(SD.open("/"), 0);
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(255,255);

  /***** Two interrupt options! *******/ 
  // This option uses timer0, this means timer1 & t2 are not required
  // (so you can use 'em for Servos, etc) BUT millis() can lose time
  // since we're hitchhiking on top of the millis() tracker
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT);
  
  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT))
    Serial.println(F("DREQ pin is not an interrupt pin"));
}

void loop() {  
  // Alternately, we can just play an entire file at once
  // This doesn't happen in the background, instead, the entire
  // file is played and the program will continue when it's done!
  //musicPlayer.playFullFile("track001.ogg");
   
  //Start playing the specified file!
  if(!musicPlayer.playingMusic && state == PLAYING){
    // Start playing a file, then we can do stuff while waiting for it to finish
    if (! musicPlayer.startPlayingFile("track001.mp3")) {
      Serial.println("Could not open file track001.mp3");
      songNum = 1;
      musicPlayer.startPlayingFile(songArray[songNum - 1]);
    }
    Serial.println(F("Started playing"));
    
  } else {
    // file is now playing in the 'background' so now's a good time
    // to do something else like handling LEDs or buttons :)
    
    handlePlayPause();    
    if(digitalRead(nextSongPin) == LOW){
      songNum = playNextSong(songNum);
    }
    
    if(digitalRead(prevSongPin) == LOW){
      songNum = playPrevSong(songNum);
    }
    matrix.print(songNum, DEC);
    matrix.writeDisplay();
    
    /*
    matrix.print(1, DEC);
    matrix.writeDisplay();
    delay(500);
    matrix.print(10, DEC);
    matrix.writeDisplay();
    delay(500);
    matrix.print(100, DEC);
    matrix.writeDisplay();
    delay(500);
    matrix.print(1000, DEC);
    matrix.writeDisplay();
    delay(500);  */
    
    /*
    matrix.writeDigitNum(0,songNum);
    matrix.writeDisplay();
    delay(100);
    matrix.writeDigitRaw(0,0);
    matrix.writeDigitNum(1,songNum);
    matrix.writeDisplay();
    delay(100);
    matrix.writeDigitRaw(1,0);
    matrix.writeDigitNum(3,songNum);
    matrix.writeDisplay();
    delay(100);
    matrix.writeDigitRaw(3,0);
    matrix.writeDigitNum(4,songNum);
    matrix.writeDisplay();
    delay(100);
    matrix.writeDigitRaw(4,0);
    */
    
    attenPotRead = analogRead(attenPotPin);    //"volume" reading from attenPot
    fadePotRead = analogRead(fadePotPin);    //balance reading from fadePot
    attenPotRead /= 10;    //been playing around with this divisor to try to get a reasonable volume range on a linear pot
    attenPotRead += 1;    //to prevent div/0 errors when using log function
    //float taper0read = log(attenPotRead)*35.0;
    //attenPotRead = (int)taper0read;    //casting the float back to an int, or more accurately an uint8_t
    
    //The next section of code handles Left/Right balance or "fade", NOT Front/Back
    
    attenMax = 102;  //the max attenuation value, speakers should be silent at this level. value btw 0 - 255. 
                     //the "setVolume" method below takes a uint_8 as attenuation, not volume, so 0 = max loudness.
    
    if(fadePotRead <= 512){    //this value is the halfway point in the fadePot analogRead range 
      attenLeft = attenPotRead;
      attenRight = attenMax - ((float)fadePotRead/512)*(attenMax - (float)attenPotRead);
      attenRight = (int)attenRight;
    }
    else{
      attenRight = attenPotRead;
      attenLeft = attenPotRead + (((float)fadePotRead - 512)/512)*(attenMax - (float)attenPotRead);
      attenLeft = (int)attenLeft;
    }
    
    musicPlayer.setVolume(attenLeft,attenRight);
    
    /*DEBUG LINES
    Serial.print("attenPot: ");
    Serial.print(attenPotRead);
    Serial.print("  fadePot: ");
    Serial.print(fadePotRead);
    Serial.print("  attenLeft: ");
    Serial.print(attenLeft);
    Serial.print("  attenRight: ");
    Serial.println(attenRight);
    //Serial.print("The taper0read reading is: ");
    //Serial.println(taper0read);
    */
    Serial.println(songArray[songNum - 1]);

    
    delay(100);
  }
  if(!musicPlayer.playingMusic && state == PAUSED){
    Serial.println("paused");
  } else if (!musicPlayer.playingMusic && state == PLAYING){
    Serial.println("uhhhh");
  }
}

int playNextSong(int songNum){
  int nextSong = songNum + 1;
  char* songname = songArray[nextSong - 1];
  if(nextSong <= songList->length()){
    songname = songList->getFileAt(nextSong - 1);
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile(songname);
    state = PLAYING;
    return nextSong;
  }
  return songNum;
}

int playPrevSong(int songNum){
  int nextSong = songNum > 1 ? songNum - 1 : songNum;
  char* songname = songArray[nextSong -1];
  musicPlayer.stopPlaying();
  musicPlayer.startPlayingFile(songname);
  state = PLAYING;
  return nextSong;
}

void handlePlayPause(){ 
  //This section handles the buttons 
  if(digitalRead(playPausePin) == LOW){
    if(!musicPlayer.paused() && state == PLAYING) {
      musicPlayer.pausePlaying(true);
      state = PAUSED;
    }
    else{
      musicPlayer.pausePlaying(false);
      state = PLAYING;
    }  
  }
}

bool isMp3File(char* filename, uint32_t size){
  int filenameLength = strlen(filename); //we know this will be 12
  char* extension = &filename[filenameLength - 3];
  Serial.println(strcmp("MP3", extension));
  Serial.println(size);
  return size > 4096 && strcmp("MP3", extension) == 0; 
}

/// File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.println(entry.name());
     if(isMp3File(entry.name(), entry.size()) && numTabs < 1){
      Serial.print("adding file " );
      Serial.println(entry.name());
      if(songList == NULL){
        songList = new filelist(entry.name());
      } else {
        songList->add(entry.name());
      }
     } else {
      Serial.println(entry.size());
     }
     
     entry.close();
   }
}

