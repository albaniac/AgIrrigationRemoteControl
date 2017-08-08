/******************************************************************************************************************//**
 * @file UserControl.cpp
 * @brief Manages Input->Output Control Processes ( Read-InPin-Value > Control Process > Set-OutPin-Value )
 * @authors 
 *    tgit23        8/2017       Original
 **********************************************************************************************************************/
#include "UserControl.h"

unsigned int  UserControl::NextEpromOffset = 0;
char          UserControl::OnControls[16] = {};
int           UserControl::ObjectCount = 0;

#if DEBUG>0                             // Activate Debug Messages ( DEBUG defined in PeerRemoteMenu.h )
  #define DBL(x) Serial.println x
  #define DB(x) Serial.print x
  #define DBC Serial.print(", ")
#else                                   // ELSE - Clear Debug Messages
  #define DBL(x)
  #define DB(x)
  #define DBC  
#endif
/******************************************************************************************************************//**
 * @brief Creates a new Control ( Constructor )
 * @remarks
 * @code
 *   exmaple code
 * @endcode
**********************************************************************************************************************/
UserControl::UserControl(PinPoint * _InPin, eControlType _ControlType, PinPoint * _OutPin, char _ID, PinPoint * _StorePin = NULL) {
  DB(("UserControl::UserControl("));DB((_InPin->Name));DBC;DB((_OutPin->Name));DBC;DB((_ID));DB((")"));

  InPin = _InPin;
  ControlType = _ControlType;
  OutPin = _OutPin;
  ID = _ID;
  StorePin = _StorePin;
  
  if ( ControlType == PID_SET ) {
    PIDControl = new PID(&PIDInput, &PIDOutput, &PIDSetpoint, Kp, Ki, Kd, POn, PIDDirection );
    PIDControl->SetOutputLimits(1,1023);
    PIDControl->SetMode(MANUAL);
  }

  //------------ Assign EEPROM Offset for this User Control and Retreive values ---------------------
  EpromOffset = NextEpromOffset;
  DB((" @EpromOffset="));DB((EpromOffset));
  byte LoByte = EEPROM.read( EpromOffset );                           // Read Eprom when Offset is assigned
  byte HiByte = EEPROM.read( EpromOffset + 1 );
  if ( HiByte > 0x3F ) {                                              // Initialize EPROM to 0x00 if out-of-bounds
    EEPROM.update( EpromOffset, 0x00 );
    EEPROM.update( EpromOffset + 1, 0x00 );
    Setpoint = 0;
  } else {                                                            // ELSE assign eprom values
    Status = ((HiByte & 0x30) >> 4);                                    // Bit 12 & 13 make Status
    Setpoint = (int)(((HiByte & 0x0F) << 8) | LoByte);                  // 0x0FFF 0->11-bits for value
  }
  NextEpromOffset = EpromOffset + 2;                             // Set the next available offset
  if ( ControlType == SET_PIN ) Status = OKAY;
  
  //------------ Assign an OnControls [Index] and set to ID if ON -------------------------------------
  ObjectIndex = ObjectCount;
  if (ObjectCount<15) ObjectCount++;                  // Stay within boundaries
  if ( Status == ISON ) OnControls[ObjectIndex] = ID;
  DB((" @ObjectIndex="));DBL((ObjectIndex));
}

/******************************************************************************************************************//**
 * @brief Applies the Control - Output ( Driven ) Pin
 * @remarks
 * @code
 *   exmaple code
 * @endcode
**********************************************************************************************************************/
void UserControl::Apply() {

  if ( ControlType != SET_PIN && Status != ISON ) return;                   // If control is not ON return
  if ( InPin->GetStatus() == WAIT || InPin->GetStatus() == ERR ) return;    // If read value isn't available then return

  int InputValue = InPin->GetRawValue();
  switch ( ControlType ) {
    
    case LESS_THAN:
      if ( InputValue < Setpoint ) { OutPin->SetTo(1000); }
      else { OutPin->SetTo(0); }
      break;
      
    case GREATER_THAN:
      if ( InputValue > Setpoint ) { OutPin->SetTo(1000); }
      else { OutPin->SetTo(0); }
      break;
      
    case EQUAL_TO:
      if ( InputValue == Setpoint ) { OutPin->SetTo(1000); }
      else { OutPin->SetTo(0); }
      break;
      
    case NOT_EQUAL_TO:
      if ( InputValue != Setpoint ) { OutPin->SetTo(1000); }
      else { OutPin->SetTo(0); }
      break;
      
    case SET_PIN:
      OutPin->SetTo(Setpoint, Status);
      break;
      
    case PID_SET:
      if ( PIDControl != NULL ) {
        PIDInput = double(InputValue);
        PIDSetpoint = double(Setpoint);
        PIDControl->Compute();
        OutPin->SetTo(PIDOutput);
      }
      break;
  }
}

/******************************************************************************************************************//**
 * @brief Sets/Gets a User Setpoint
 * @remarks
 * @code
 *   exmaple code
 * @endcode
**********************************************************************************************************************/
void UserControl::SetPoint(int _Set) {
  Setpoint = _Set;
  if ( StorePin != NULL ) StorePin->SetTo(Setpoint);
}
int UserControl::SetPoint() {
  return Setpoint;
}

/******************************************************************************************************************//**
 * @brief Adds or Subtracts to the Setpoint using the Modified Display Value
 * @remarks
 * @code
 *   exmaple code
 * @endcode
**********************************************************************************************************************/
void UserControl::SetPointAdd(int AddValue) {
  if ( InPin->IsOnOff ) {
    if ( Setpoint != 0 ) { Setpoint = 0; }
    else { Setpoint = 1; }
  } else {
    // Adjust Setpoint according to Modified Value
    int ModStart = InPin->ModifyValue(Setpoint);
    if ( AddValue < 0 ) {
      while ( InPin->ModifyValue(Setpoint) > ModStart + AddValue ) { Setpoint--; }
    } else if ( AddValue > 0 ) {
      while ( InPin->ModifyValue(Setpoint) < ModStart + AddValue ) { Setpoint++; }
    }
  }
}

/******************************************************************************************************************//**
 * @brief IsOn Get/Set wrappers
 * @remarks
 * - If 'StorePin' is set the status is retreived from the Virtual Pin
 * - Else the Status is stored locally
 * @code
 *   exmaple code
 * @endcode
**********************************************************************************************************************/
void UserControl::IsOn(bool _IsOn) {
  DB(("UserControl::IsOn("));DB((_IsOn));DBL((")"));

  if ( _IsOn ) { Status = ISON; } else { Status = ISOFF; }
  if ( StorePin != NULL ) OutPin->SetTo(Setpoint, Status);
  if ( Status == ISON && ID != NULL ) { OnControls[ObjectIndex] = ID; } 
  else { OnControls[ObjectIndex] = ' '; }
  //DB(("OnControls["));DB((ObjectIndex));DB(("]="));DBL((OnControls[ObjectIndex]));
}
bool UserControl::IsOn() {
  if ( StorePin != NULL ) return (OutPin->GetStatus() == ISON);
  return ( Status == ISON );
}

/******************************************************************************************************************//**
 * @brief  Save the Controls 'Setpoint' to Eprom and VirtualPins
 * @remarks
 *  @code
 *    exmaple code
 *  @endcode
**********************************************************************************************************************/
void UserControl::Save() {
  DBL(("UserControl::Save()"));
    
  // EEPROM - Save; Store the Value and Status as two bytes in Eprom
  byte LoByte = ((Setpoint >> 0) & 0xFF);
  byte HiByte = ((Status << 4 & 0x30) | (Setpoint >> 8 & 0x0F));
  EEPROM.update( EpromOffset, LoByte );
  EEPROM.update( (EpromOffset + 1), HiByte );
  DB(("EEPROM.update( "));DB((EpromOffset));DBC;DB((LoByte, HEX));DBL((")"));
  DB(("EEPROM.update( "));DB((EpromOffset + 1));DBC;DB((HiByte, HEX));DBL((")"));

  // VirtualPin - Save; If 'StorePin' Exists - Save to a Virtual Pin
  if ( StorePin != NULL ) { StorePin->SetTo(Setpoint, Status); }
}
