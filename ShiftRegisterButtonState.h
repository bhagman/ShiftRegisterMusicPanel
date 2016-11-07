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

#ifndef SHIFTREGISTERBUTTONSTATE_H
#define SHIFTREGISTERBUTTONSTATE_H

#include "ShiftRegisterButtonManager.h"

typedef void (*buttonEventHandler)();

template <int registerCount>
class ShiftRegisterButtonState
{
  public:
    ShiftRegisterButtonState(ShiftRegisterButtonManager<registerCount> &buttons, uint8_t reg, uint8_t pos);
    void run();
    void setHoldThreshold(uint16_t holdTime);
    void setHoldRepeat(uint16_t repeatTime);
    void pressHandler(buttonEventHandler handler);
    //void releaseHandler(buttonEventHandler handler);
    void holdHandler(buttonEventHandler handler, uint16_t holdTime = 0);
  private:
    uint32_t _lastTime;
    uint8_t _register;
    uint8_t _position;
    bool _checkHeld;
    bool _holding;
    ShiftRegisterButtonManager<registerCount> *_buttons;
    uint16_t _holdEventThresholdTime;
    uint16_t _holdEventRepeatTime;
    buttonEventHandler _cb_onPress;
    //buttonEventHandler _cb_onRelease;
    buttonEventHandler _cb_onHold;
};


template <int registerCount>
ShiftRegisterButtonState<registerCount>::ShiftRegisterButtonState(ShiftRegisterButtonManager<registerCount> &buttons, uint8_t reg, uint8_t pos)
{
  _buttons = &buttons;
  _register = reg;
  _position = pos;
  _checkHeld = false;
  _holding = false;
  _lastTime = 0;
}


template <int registerCount>
void ShiftRegisterButtonState<registerCount>::run()
{
  if (_buttons->pressed(_register, _position))
  {
    // Check if it is being held
    _lastTime = millis();
    _checkHeld = true;
    _holding = false;
  }
  else if (_checkHeld)
  {
    if (_buttons->held(_register, _position))
    {
      if ((millis() - _lastTime) > _holdEventThresholdTime)
      {
        // hit hold threshold

        // Execute "hold" duties
        if (_cb_onHold)
          _cb_onHold();

        _checkHeld = false;
        // We don't need to go into "holding" state, if there is no repeat interval
        if (_holdEventRepeatTime > 0)
        {
          _holding = true;
          _lastTime = millis();
        }
        else
        {
          _holding = false;
        }
      }
      // otherwise, we wait
    }
    else
    {
      // Button was released before hold time
      // this is a "press"
      _holding = false;
      _checkHeld = false;
      // Execute "press" duties
      if (_cb_onPress)
        _cb_onPress();
    }
  }
  else if (_holding)
  {
    // we are in "holding" state
    if (_buttons->held(_register, _position))
    {
      if ((millis() - _lastTime) > _holdEventRepeatTime)
      {
        // Execute "hold" duties
        if (_cb_onHold)
          _cb_onHold();
        _lastTime = millis();
      }
    }
    else
    {
      // Button was released
      _holding = false;
      _checkHeld = false;
      //if (_cb_onRelease)
      //  _cb_onRelease();
    }
  }
}


template <int registerCount>
void ShiftRegisterButtonState<registerCount>::setHoldThreshold(uint16_t holdTime)
{
  _holdEventThresholdTime = holdTime;
}


template <int registerCount>
void ShiftRegisterButtonState<registerCount>::setHoldRepeat(uint16_t repeatTime)
{
  _holdEventRepeatTime = repeatTime;
}


template <int registerCount>
void ShiftRegisterButtonState<registerCount>::pressHandler(buttonEventHandler handler)
{
  _cb_onPress = handler;
}

//void ShiftRegisterButtonState::releaseHandler(buttonEventHandler handler)
//{
//  _cb_onRelease = handler;
//}


template <int registerCount>
void ShiftRegisterButtonState<registerCount>::holdHandler(buttonEventHandler handler, uint16_t holdTime)
{
  _cb_onHold = handler;
  setHoldThreshold(holdTime);
}


#endif
//SHIFTREGISTERBUTTONSTATE_H
