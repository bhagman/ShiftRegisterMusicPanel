/*
||
|| @author         Brett Hagman <bhagman@roguerobotics.com>
|| @url            http://roguerobotics.com/
|| @url            http://wiring.org.co/
||
|| @description
|| |
|| | Music panel using multiple shift registers to select an album.
|| |
|| #
||
|| @license
|| |
|| | Copyright (c) 2016 - Brett Hagman
|| |
|| | Permission is hereby granted, free of charge, to any person obtaining a copy of
|| | this software and associated documentation files (the "Software"), to deal in
|| | the Software without restriction, including without limitation the rights to
|| | use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
|| | the Software, and to permit persons to whom the Software is furnished to do so,
|| | subject to the following conditions:
|| |
|| | The above copyright notice and this permission notice shall be included in all
|| | copies or substantial portions of the Software.
|| |
|| | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
|| | IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
|| | FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
|| | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
|| | IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
|| | CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
|| #
||
|| @notes
|| |
|| |
|| #
||
*/

// Libraries
#include <RogueSD.h>
#include <RogueMP3.h>
#include <SoftwareSerial.h>
#include "ShiftRegisterButtonManager.h"
#include "ShiftRegisterButtonState.h"

// Constants
#define VERSIONSTR "1.0"

#define LEDPIN            13

#define MAXALBUMS          3

#define SR_REGISTER_COUNT  2
#define SR_DATA_PIN       19
#define SR_SH_LD_PIN      18
#define SR_CLK_PIN        17
#define SR_CINH_PIN       16

#define RMP3_RX_PIN        2
#define RMP3_TX_PIN        3

#define B_CTRL_REGISTER    0
#define B_V_UP             7
#define B_FWD              6
#define B_PLAYPAUSE        5
#define B_REV              4
#define B_V_DOWN           3

#define B_HOLDTIME       500
#define B_HOLDREPEAT     100

#define MAXVOLUMEVALUE    32

// Tables

uint8_t vLookup[MAXVOLUMEVALUE] = { 254, 121, 110, 101, 93, 86, 79, 73,
                                     68,  63,  58,  54, 50, 46, 42, 39,
                                     36,  33,  30,  27, 24, 22, 19, 17,
                                     14,  12,  10,   8,  6,  4,  2,  0 };


// Objects
SoftwareSerial rmp3_serial = SoftwareSerial(RMP3_RX_PIN, RMP3_TX_PIN);
RogueMP3 rmp3 = RogueMP3(rmp3_serial);
RogueSD filecommands = RogueSD(rmp3_serial);
ShiftRegisterButtonManager<SR_REGISTER_COUNT> buttons = ShiftRegisterButtonManager<SR_REGISTER_COUNT>(SR_SH_LD_PIN, SR_CINH_PIN, SR_CLK_PIN, SR_DATA_PIN);

ShiftRegisterButtonState<SR_REGISTER_COUNT> fwdButton = ShiftRegisterButtonState<SR_REGISTER_COUNT>(buttons, B_CTRL_REGISTER, B_FWD);
ShiftRegisterButtonState<SR_REGISTER_COUNT> revButton = ShiftRegisterButtonState<SR_REGISTER_COUNT>(buttons, B_CTRL_REGISTER, B_REV);
ShiftRegisterButtonState<SR_REGISTER_COUNT> vupButton = ShiftRegisterButtonState<SR_REGISTER_COUNT>(buttons, B_CTRL_REGISTER, B_V_UP);
ShiftRegisterButtonState<SR_REGISTER_COUNT> vdownButton = ShiftRegisterButtonState<SR_REGISTER_COUNT>(buttons, B_CTRL_REGISTER, B_V_DOWN);

// Variables

uint8_t volume;
bool playing = false;
int8_t trackNumber = 0;
int8_t albumNumber = 0;
uint8_t albumTrackCount = 0;
// TODO: random mode
bool randomMode = false;
// NOTES: When random mode is selected (how?), store the total number of tracks by using fileCount on the album folder for x.mp3.
// (maybe do that for every time an album is selected?).  Then play a random track from the current album.  (can't play all files
// from the album randomly, as it would require keeping a complete track list?)
// Maybe random mode should be playing random tracks from ALL albums?
// Make sure we don't repeat the same track back to back when in random mode.


void printBits(uint8_t b)
{
  for (uint8_t i = 0; i < 8; i++)
    Serial.print(b >> (7 - i) & 1, BIN);
}


void handleFWDPress()
{
  Serial.println(F("Next track"));
  if (playing)
  {
    rmp3.setVolume(vLookup[volume]);  // In case we were paused.
    playNextTrack();
  }
}


void handleFWDHold()
{
  playbackInfo info;

  if (rmp3.isPlaying())
  {
    info = rmp3.getPlaybackInfo();

    rmp3.jump(info.position + 5);
  }

  Serial.println(F(">>"));
}


void handleREVPress()
{
  Serial.println(F("Previous track"));
  if (playing)
  {
    int8_t newTrack = 0;

    if (trackNumber > 0)
      newTrack = trackNumber - 1;

    if (playTrackInAlbum(newTrack))
    {
      rmp3.setVolume(vLookup[volume]);  // In case we were paused.
      trackNumber = newTrack;
    }
    else
    {
      // TODO: go to previous album?
      Serial.print(F("REV -- Couldn't find album/track: "));
      Serial.print(albumNumber);
      Serial.print('/');
      Serial.println(newTrack);
    }
  }
}


void handleREVHold()
{
  playbackInfo info;

  if (rmp3.isPlaying())
  {
    info = rmp3.getPlaybackInfo();

    if (info.position > 5)
      rmp3.jump(info.position - 5);
    else
      rmp3.jump(0);
  }

  Serial.println(F("<<"));
}


void handleVUPHold()
{
  if (volume < (MAXVOLUMEVALUE - 1))
  {
    volume++;
    rmp3.setVolume(vLookup[volume]);
    Serial.print(F("VOL++ "));
    Serial.println(volume);
  }
}


void handleVDOWNHold()
{
  if (volume > 0)
  {
    volume--;
    rmp3.setVolume(vLookup[volume]);
    Serial.print(F("VOL-- "));
    Serial.println(volume);
  }
}


int8_t scanAlbumButtons()
{
  // The album buttons are on registers > 0 (register 0 is dedicated to playback controls)
  // This function scans all those registers and gives the album number that has been pressed,
  // or returns -1 if nothing has been pressed.

  buttons.scan();

  for (int i = 1; i < SR_REGISTER_COUNT; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      if (buttons.pressed(i, j, true))   // don't scan
      {
        Serial.print(F("Album pressed: "));
        Serial.println((i - 1) * 8 + j);
        return (i - 1) * 8 + j;
      }
    }
  }
  return -1;
}


bool playTrackInAlbum(int8_t newTrack)
{
  char path[64];

  sprintf(path, "/%d/%d.mp3", albumNumber, newTrack);

  Serial.print(F("Looking for "));
  Serial.println(path);
  
  if (filecommands.exists(path))
  {
    // File found.
    rmp3.playFile(path);
    trackNumber = newTrack;
    playing = true;
    return true;
  }
  else
  {
    return false;
  }
}


bool playNextTrack()
{
  trackNumber++;

  if (!playTrackInAlbum(trackNumber))
  {
    Serial.print(F("playNextTrack -- Couldn't find album/track: "));
    Serial.print(albumNumber);
    Serial.print('/');
    Serial.println(trackNumber);

    albumNumber++;
    if (albumNumber >= MAXALBUMS)
      albumNumber = 0;
    // Reset the track number
    trackNumber = 0;

    if (!playTrackInAlbum(trackNumber))
    {
      Serial.print(F("Missing Album: "));
      Serial.println(albumNumber);
      playing = false;
    }
  }
  return playing;
}


void setup()
{
  pinMode(LEDPIN, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("Music Panel V" VERSIONSTR " Started"));
  Serial.print(F("Latches Configured: "));
  Serial.println(SR_REGISTER_COUNT);

  buttons.begin();
  fwdButton.pressHandler(handleFWDPress);
  fwdButton.holdHandler(handleFWDHold);
  fwdButton.setHoldThreshold(B_HOLDTIME);
  fwdButton.setHoldRepeat(B_HOLDREPEAT);

  revButton.pressHandler(handleREVPress);
  revButton.holdHandler(handleREVHold);
  revButton.setHoldThreshold(B_HOLDTIME);
  revButton.setHoldRepeat(B_HOLDREPEAT);

  vupButton.pressHandler(handleVUPHold);
  vupButton.holdHandler(handleVUPHold);
  vupButton.setHoldThreshold(B_HOLDTIME);
  vupButton.setHoldRepeat(B_HOLDREPEAT);

  vdownButton.pressHandler(handleVDOWNHold);
  vdownButton.holdHandler(handleVDOWNHold);
  vdownButton.setHoldThreshold(B_HOLDTIME);
  vdownButton.setHoldRepeat(B_HOLDREPEAT);

  rmp3_serial.begin(9600);

  if (!rmp3.sync())
  {
    Serial.println(F("rMP3 synchronization failure."));
    Serial.println(F("Please check connections and restart."));
    for (;;);
  }
  else
  {
    filecommands.sync();
    Serial.println(F("rMP3 synchronized."));
    Serial.print(F("rMP3 firmware version: "));
    Serial.println(rmp3.version(), DEC);
  }

  rmp3.stop();
  volume = 20;
  rmp3.setVolume(vLookup[20]);

  Serial.println(F("Setup Complete"));
}


void loop()
{
  char status;
  int8_t newAlbum;

  status = rmp3.getPlaybackStatus();

  // First, let's keep playing, if we can.
  if (playing && status == 'S')
  {
    playNextTrack();
  }

  // Next, let's see if a new album button has been pressed.
  newAlbum = scanAlbumButtons();

  if ((newAlbum >= 0))
  {
    if (playing && (newAlbum == albumNumber))
    {
      // Don't restart
    }
    else
    {
      // An album has been pressed.  Start playing, regardless of current state.
      albumNumber = newAlbum;
      trackNumber = 0;
      // Let's see if there are tracks there, and start playing.
      if (playTrackInAlbum(trackNumber))
      {
        playing = true;
      }
      else
      {
        Serial.print(F("Album Button -- Couldn't find album/track: "));
        Serial.print(albumNumber);
        Serial.print('/');
        Serial.println(trackNumber);
        playing = false;
      }
    }
  }

  // Now, let's check the play/pause button.
  if (buttons.pressed(B_CTRL_REGISTER, B_PLAYPAUSE))
  {
    Serial.println(F("Play/Pause"));

    if (playing)
    {
      // We need to update the status here, since we may have started a new track above.
      status = rmp3.getPlaybackStatus();

      if (status == 'P')
      {
        // We are already playing
        rmp3.fade(150, 400);
        rmp3.playPause();
      }
      else if (status == 'D')
      {
        // Paused - let's resume
        rmp3.playPause();
        rmp3.fade(vLookup[volume], 400);
      }
      else if (status == 'S')
      {
        // We should never get here, because of the above check on status.  Let's just spit something out.
        Serial.println(F("Odd. Stopped, but we should be playing?"));
      }
    }
    else
    {
      // System is stopped.  Start playing the first song of the current album.
      trackNumber = 0;
      if (playTrackInAlbum(trackNumber))
      {
        playing = true;
      }
      else
      {
        Serial.print(F("Play/Pause -- Couldn't find album/track: "));
        Serial.print(albumNumber);
        Serial.print('/');
        Serial.println(trackNumber);
        playing = false;
      }
    }
  }

  // Finally, process the other control buttons.
  fwdButton.run();
  revButton.run();
  vupButton.run();
  vdownButton.run();
}

