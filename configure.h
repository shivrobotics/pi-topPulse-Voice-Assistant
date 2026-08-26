#ifndef CONFIGURE_H
#define CONFIGURE_H

#include "Wire.h"
const uint8_t _device_addr = 0x24;
const uint8_t _register_addr = 0;

const int _speaker_bit = 0;
const int _mcu_bit = 1;
const int _eeprom_bit = 2;
const int _16khz_bit = 3;
#define ZERO_PADDING 4
char binstr[]="0000";

#define I2C_SDA 4
#define I2C_SCL 5
//Wire.begin(I2C_SDA,I2C_SCL);

int __get_addr_for_bit(int bit) {
    if (bit >= 0 && bit <= 3) {
        return pow(2, bit);
    } else {
        return -1;
    }
}

int __read_device_state() {
    Wire.begin(I2C_SDA,I2C_SCL);
    uint8_t bytesReceived = Wire.requestFrom(_device_addr, 1);
    if ((bool)bytesReceived) {  //If received more than zero bytes
      uint8_t temp[bytesReceived];
      Wire.readBytes(temp, bytesReceived);
      //Serial.println(temp[0],BIN);
      return temp[0] & 0x0F;
    }
    else
      return -1;
    Wire.end();
}

bool __verify_device_state(int expected_state) {
    int current_state = __read_device_state();
    return expected_state == current_state;
}

bool __write_device_state(int state) {
    Wire.begin(I2C_SDA,I2C_SCL);
    Wire.beginTransmission(_device_addr);
    uint8_t state_to_send = 0x0F & state;
    //Wire.printf("%d", state_to_send);
    Wire.write(state_to_send);
    uint8_t error = Wire.endTransmission(true);
    //Serial.printf("endTransmission: %u\n", error);
    Wire.end();

    bool result = __verify_device_state(state_to_send);
    if (result) {
        Serial.println("OK");
    } else {
        Serial.println(state_to_send);
    }
    //Serial.println(state_to_send,HEX);
    return result;
}

bool __update_device_state_bit(int bit, int value) {
    if (bit < 0 || bit > 3) {
        return false;
    }
    int current_state;
    current_state = __read_device_state();
    int new_state = __get_addr_for_bit(bit);
    
    if (value == 0)
        new_state = ~new_state;
    
    if ((value == 1 && (new_state & current_state) != 0) || (value == 0 && (~new_state & ~current_state) != 0)) {
        Serial.println("OK");
        return true;
    }
    if (value == 0) {
        new_state = new_state & current_state;
    } else {
        new_state = new_state | current_state;
    }
    
    return __write_device_state(new_state);
}

bool __reset_device_state(bool enable)
    {
    int clean_enable_state = __get_addr_for_bit(_eeprom_bit);
    int clean_disable_state = __get_addr_for_bit(_speaker_bit) | __get_addr_for_bit(_mcu_bit);
    int state_to_send = enable ? clean_enable_state : clean_disable_state;
    return __write_device_state(state_to_send);
    }

bool disable_device()
{ 
  __reset_device_state(0);
  return true;
}

bool set_microphone_sample_rate_to_16khz()
{
    //"""Set the appropriate I2C bits to enable 16,000Hz recording on the microphone"""
    return __update_device_state_bit(_16khz_bit, 1);
}


bool set_microphone_sample_rate_to_22khz()
{
  //  """Set the appropriate I2C bits to enable 22,050Hz recording on the microphone"""
    return __update_device_state_bit(_16khz_bit, 0);
}


//GET STATE

bool speaker_enabled()
{
  //  """Get whether the speaker is enabled"""
    return (__read_device_state() & __get_addr_for_bit(_speaker_bit)) == 0;
}


bool speaker_enable()
{
  //  """Get whether the speaker is enabled"""
    return __update_device_state_bit(_speaker_bit, 0);
}

bool mcu_enabled()
{
  //  """Get whether the onboard MCU is enabled"""
    return (__read_device_state() & __get_addr_for_bit(_mcu_bit)) == 0;
}

bool mcu_enable()
{
  //  """Get whether the onboard MCU is enabled"""
    return __update_device_state_bit(_mcu_bit, 0);
}


bool eeprom_enabled()
{
  //  """Get whether the eeprom is enabled"""
    return (__read_device_state() & __get_addr_for_bit(_eeprom_bit)) != 0;
}


bool microphone_sample_rate_is_16khz()
{
  //  """Get whether the microphone is set to record at a sample rate of 16,000Hz"""
    return (__read_device_state() & __get_addr_for_bit(_16khz_bit)) != 0;
}


bool microphone_sample_rate_is_22khz()
{
  //  """Get whether the microphone is set to record at a sample rate of 22,050Hz"""
    return (__read_device_state() & __get_addr_for_bit(_16khz_bit)) == 0;
}

#endif
