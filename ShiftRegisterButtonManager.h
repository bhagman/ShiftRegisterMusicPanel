/*
||
|| @author         Brett Hagman <bhagman@wiring.org.co>
|| @url            http://wiring.org.co/
|| @url            http://roguerobotics.com/
|| @url            http://loftypremises.com/
||
|| @description
|| |
|| #
||
|| @license Please see license.txt.
||
*/

#ifndef SHIFTREGISTERBUTTONMANAGER_H
#define SHIFTREGISTERBUTTONMANAGER_H


template <int registerCount>
class ShiftRegisterButtonManager
{
  public:
    ShiftRegisterButtonManager(uint8_t shLdPin, uint8_t clkInhibitPin, uint8_t clkPin, uint8_t dataPin);
    void begin();
    void scan();
    bool pressed(uint8_t reg, uint8_t position, bool suppressScan = false);
    bool released(uint8_t reg, uint8_t position, bool suppressScan = false);
    bool held(uint8_t reg, uint8_t position, bool suppressScan = false);
    uint8_t getRegister(uint8_t reg, bool suppressScan = false);

  private:
    uint8_t _shLdPin;
    uint8_t _clkInibitPin;
    uint8_t _clkPin;
    uint8_t _dataPin;
    uint8_t _buttonsLastState[registerCount];
    uint8_t _buttonsCurrentState[registerCount];
    uint8_t _buttonsPressed[registerCount];
    uint8_t _buttonsReleased[registerCount];
};


template <int registerCount>
ShiftRegisterButtonManager<registerCount>::ShiftRegisterButtonManager(uint8_t shLdPin, uint8_t clkInhibitPin, uint8_t clkPin, uint8_t dataPin)
{
  _shLdPin = shLdPin;
  _clkInibitPin = clkInhibitPin;
  _clkPin = clkPin;
  _dataPin = dataPin;
}


template <int registerCount>
void ShiftRegisterButtonManager<registerCount>::begin()
{
  pinMode(_dataPin, INPUT);
  pinMode(_shLdPin, OUTPUT);
  pinMode(_clkPin, OUTPUT);
  pinMode(_clkInibitPin, OUTPUT);

  // Required initial states of these two pins according to the datasheet timing diagram
  digitalWrite(_clkPin, HIGH);
  digitalWrite(_shLdPin, HIGH);
}


template <int registerCount>
void ShiftRegisterButtonManager<registerCount>::scan()
{
  // '165 requires a "load" before shifting data out
  digitalWrite(_shLdPin, LOW);
  delayMicroseconds(5);                 // Shift register timing
  digitalWrite(_shLdPin, HIGH);
  delayMicroseconds(5);                 // Shift register timing

  // From TI 'HC165 datasheet:
  // "Clocking is accomplished by a low-to-high transition of the clock (CLK)
  // input while SH/LD is held high and CLK INH is held low."
  digitalWrite(_clkInibitPin, LOW);

  for (int i = 0; i < registerCount; i++)
  {
    _buttonsLastState[i] = _buttonsCurrentState[i];  // Save the last state
    _buttonsCurrentState[i] = shiftIn(_dataPin, _clkPin, MSBFIRST);
  }

  // From TI 'HC165 datasheet:
  // "CLK INH should be changed to the high level only while CLK is high."
  digitalWrite(_clkPin, HIGH);
  digitalWrite(_clkInibitPin, HIGH);

  // Do some state processing for buttons
  for (int i = 0; i < registerCount; i++)
  {
    uint8_t newState;

    newState = ~_buttonsLastState[i] & _buttonsCurrentState[i];
    _buttonsPressed[i] = _buttonsPressed[i] | newState;

    newState = _buttonsLastState[i] & ~_buttonsCurrentState[i];
    _buttonsReleased[i] = _buttonsReleased[i] | newState;
  }
}


template <int registerCount>
bool ShiftRegisterButtonManager<registerCount>::pressed(uint8_t reg, uint8_t position, bool suppressScan)
{
  bool isPressed;

  if (!suppressScan)
    scan();

  isPressed = bitRead(_buttonsPressed[reg], position);

  // now clear the state
  bitWrite(_buttonsPressed[reg], position, false);
  
  return isPressed;
}


template <int registerCount>
bool ShiftRegisterButtonManager<registerCount>::released(uint8_t reg, uint8_t position, bool suppressScan)
{
  bool isReleased;

  if (!suppressScan)
    scan();

  isReleased = bitRead(_buttonsReleased[reg], position);

  // now clear the state
  bitWrite(_buttonsReleased[reg], position, false);
  
  return isReleased;
}


template <int registerCount>
bool ShiftRegisterButtonManager<registerCount>::held(uint8_t reg, uint8_t position, bool suppressScan)
{
  if (!suppressScan)
    scan();

  return (bitRead(_buttonsCurrentState[reg], position));
}


template <int registerCount>
uint8_t ShiftRegisterButtonManager<registerCount>::getRegister(uint8_t reg, bool suppressScan)
{
  if (!suppressScan)
    scan();

  if (reg >= 0 && reg < registerCount)
    return _buttonsCurrentState[reg];
  else
    return 0;
}

#endif
// SHIFTREGISTERBUTTONMANAGER_H
