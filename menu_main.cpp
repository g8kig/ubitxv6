#include "menu_main.h"
#include "menu_main_buttons.h"

#include <avr/pgmspace.h>
#include <Arduino.h>

#include "button.h"
#include "color_theme.h"
#include "menu_utils.h"
#include "morse.h"
#include "nano_gui.h"
#include "settings.h"
#include "ubitx.h"//THRESHOLD_USB_LSB
#include "utils.h"

//The startup menu stuff is a little hack to render the main screen on boot
MenuReturn_e runStartupMenu(const ButtonPress_e tuner_button,
                            const ButtonPress_e touch_button,
                            const Point touch_point,
                            const int16_t knob);
Menu_t startupMenu = {
  runStartupMenu,
  nullptr
};

MenuReturn_e runMainMenu(const ButtonPress_e tuner_button,
                         const ButtonPress_e touch_button,
                         const Point touch_point,
                         const int16_t knob);
Menu_t mainMenu = {
  runMainMenu,
  &startupMenu
};

Menu_t* const rootMenu = &mainMenu;

bool mainMenuSelecting = false;//Tracks if we're selecting buttons with knob, or adjusting frequency
int16_t mainMenuSelectedItemRaw = 0;//Allow negative only for easier checks on wrap around

void drawMainMenu(void)
{
  displayClear(COLOR_BACKGROUND);
  Button button;
  for(uint8_t i = 0; i < MAIN_MENU_NUM_BUTTONS; ++i){
    memcpy_P(&button, &(mainMenuButtons[i]), sizeof(Button));
    displayText(button.text, button.x, button.y, button.w, button.h, COLOR_INACTIVE_TEXT, COLOR_INACTIVE_BACKGROUND, COLOR_INACTIVE_BORDER);
    Serial.println(button.text);
  }
}

MenuReturn_e runStartupMenu(const ButtonPress_e,
                            const ButtonPress_e,
                            const Point,
                            const int16_t)
{
  return MenuReturn_e::ExitedRedraw;
}

void mainMenuTune(int16_t knob)
{
  static uint32_t current_freq = 0;

  if((0 == knob) && (GetActiveVfoFreq() == current_freq)){
    //Nothing to do - we're already set!
    return;
  }

  current_freq = GetActiveVfoFreq();
  uint32_t new_freq = current_freq + (50 * knob);
  
  //Transition from below to above the traditional threshold for USB
  if(current_freq < THRESHOLD_USB_LSB && new_freq >= THRESHOLD_USB_LSB){
    SetActiveVfoMode(VfoMode_e::VFO_MODE_USB);
  }
  
  //Transition from above to below the traditional threshold for USB
  if(current_freq >= THRESHOLD_USB_LSB && new_freq < THRESHOLD_USB_LSB){
    SetActiveVfoMode(VfoMode_e::VFO_MODE_LSB);
  }

  setFrequency(new_freq);
  current_freq = new_freq;

  Button button;
  memcpy_P(&button, &(mainMenuButtons[0]), sizeof(Button));
  displayText(button.text, button.x, button.y, button.w, button.h, COLOR_INACTIVE_TEXT, COLOR_INACTIVE_BACKGROUND, COLOR_INACTIVE_BORDER);
}

MenuReturn_e runMainMenu(const ButtonPress_e tuner_button,
                         const ButtonPress_e touch_button,
                         const Point touch_point,
                         const int16_t knob)
{
  if(runSubmenu(&mainMenu,
                drawMainMenu,
                tuner_button,
                touch_button,
                touch_point,
                knob)){
    //Submenu processed the input, so return now
    mainMenuSelectedItemRaw = 0;
    mainMenuSelecting = false;
    return MenuReturn_e::StillActive;//main menu always returns StillActive
  }//end submenu

  //Submenu didn't run, so handle the inputs ourselves

  //Check tuner_button
  if(ButtonPress_e::NotPressed != tuner_button){
    switch(tuner_button){
      default://Fallthrough intended
      case ButtonPress_e::NotPressed:
      {
        //Nothing to do
        break;
      }
      case ButtonPress_e::ShortPress:
      {
        if(mainMenuSelecting){
          uint8_t menu_index = mainMenuSelectedItemRaw/MENU_KNOB_COUNTS_PER_ITEM;
          Button button;
          memcpy_P(&button,&mainMenuButtons[menu_index],sizeof(button));
          endSelector(&button);

          //TODO: activate button
          Serial.print(F("Select button "));
          Serial.print(menu_index);
          Serial.print(F(":"));
          Serial.println(button.text);
        }
        else{
          initSelector(&mainMenuSelectedItemRaw,
                       mainMenuButtons,
                       MAIN_MENU_NUM_BUTTONS,
                       MorsePlaybackType_e::PlayChar);
        }
        mainMenuSelecting = !mainMenuSelecting;

        //Don't handle touch or knob on this run
        return MenuReturn_e::StillActive;//main menu always returns StillActive
        break;
      }
      case ButtonPress_e::LongPress:
      {
        if(!globalSettings.morseMenuOn){
            globalSettings.morseMenuOn = true;//set before playing
            morseLetter(2);
        }
        else{
            morseLetter(4);
            globalSettings.morseMenuOn = false;//unset after playing
        }
        SaveSettingsToEeprom();
        //Don't handle touch or knob on this run
        return MenuReturn_e::StillActive;//main menu always returns StillActive
        break;
      }
    }//switch
  }//tuner_button

  else if(ButtonPress_e::NotPressed != touch_button){
    //We treat long and short presses the same, so no need to have a switch
    Button button;
    if(findPressedButton(mainMenuButtons,MAIN_MENU_NUM_BUTTONS,&button,touch_point)){
      //TODO: activate button
      Serial.print(F("Touch button "));
      Serial.println(button.text);
    }
    else{
      //Touch detected, but not on our buttons, so ignore
      Serial.println(F("Touch not on button"));
    }
  }//touch_button

  else{//Neither button input type found, so handle the knob
    if(mainMenuSelecting){
      adjustSelector(&mainMenuSelectedItemRaw,
                     knob,
                     mainMenuButtons,
                     MAIN_MENU_NUM_BUTTONS,
                     MorsePlaybackType_e::PlayChar);
    }
    else{
      mainMenuTune(knob);
    }
  }

  //

  return MenuReturn_e::StillActive;//main menu always returns StillActive
}