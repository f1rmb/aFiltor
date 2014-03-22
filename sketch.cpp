/*
  Copyright (C) 2014 F1RMB, Daniel Caujolle-Bert <f1rmb.daniel@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <Arduino.h>
#include <avr/pgmspace.h>

#include <DFR_Keypad.h>
#include <afnAttenuator.h>
#include <dhwFilters.h>

#include "MemoryFree.h"

static const String     PROGRAM         = "aFiltor";
static const uint8_t    VERSION_MAJOR   = 0;
static const uint8_t    VERSION_MINOR   = 1;
static const uint8_t    VERSION_SUB     = 0;


static const uint8_t    GLYPH_MARKER    = 0;
static const uint8_t    GLYPH_FOCUS     = 1;
static const uint8_t    GLYPH_SPACE     = 32;
static const uint8_t    GLYPH_MINUS     = 45;

static const uint8_t    _glyphs[2][8]   =
{
    {
        B11111,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
    },
    {
        B11000,
        B11100,
        B11110,
        B11111,
        B11110,
        B11100,
        B11000,
        B00000
    }
};

// Keypad + LCD object
DFR_Keypad              mKeypad(16, 2, A0); // Analog port 0
// SV1AFN attenuator object
afnAttenuator           mAtt;
// WB6DHW filters board object
dhwFilters              mFilters(0, 1, 2);//12, 11, 10);

// States
static bool             mAttUnity       = true;
static bool             mAttFocused     = true;
static uint8_t          mFilterMaxLen;

//
// Display Attenuator value
//
void displayAtt()
{
    uint8_t a = mAtt.GetValue();

    mKeypad.setCursor(0, 0);
    mKeypad.write(mAttFocused ? GLYPH_FOCUS : GLYPH_SPACE);
    mKeypad.write(GLYPH_SPACE);

    if (a < 10)
        mKeypad.write(GLYPH_SPACE);

    mKeypad.write(a > 0 ? GLYPH_MINUS : GLYPH_SPACE);
    mKeypad.print(a, DEC);
    mKeypad.print(" dB");

    mKeypad.setCursor(3, 1);

    if (!mAttFocused)
        mKeypad.print("  ");
    else
    {
        if (mAttUnity)
        {
          mKeypad.write(GLYPH_SPACE);
          mKeypad.write(GLYPH_MARKER);
        }
        else
        {
          mKeypad.write(GLYPH_MARKER);
          mKeypad.write(GLYPH_SPACE);
        }
    }
}

//
// Display selected filter
//
void displayFilter()
{
    String str = mFilters.GetFilterName();

    mKeypad.setCursor(mKeypad.getCols() - mFilterMaxLen - 3, 1);
    mKeypad.write(mAttFocused ? GLYPH_SPACE : GLYPH_FOCUS);
    mKeypad.write(GLYPH_SPACE);

    if (str.length() < mFilterMaxLen)
    {
        for (uint8_t i = 0; i < mFilterMaxLen - str.length(); i++)
            mKeypad.write(GLYPH_SPACE);
    }

    mKeypad.print(str);
}

//
// Update attenuator and filter displayed values, take care of calling order (avoid display messup).
//
void displayUpdate()
{
    mKeypad.clear();
    if (mAttFocused)
    {
        displayFilter();
        displayAtt();
    }
    else
    {
        displayAtt();
        displayFilter();
    }
}

//
// Display software banner
//
void displayBanner(unsigned long timeout = 1500)
{
    char buf[17];

    snprintf(buf, sizeof(buf), "%s %u.%u.%u", PROGRAM.c_str(), VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);

    mKeypad.setCursor(0, 0);
    mKeypad.printCenter(buf);
    mKeypad.setCursor(0, 1);
    mKeypad.printCenter("f1rmb - 2014");
    delay(timeout);
    mKeypad.clear();
}

//
// Display memory information
//
void displayInfo()
{
    char    buf[mKeypad.getCols() + 1];
    int     f = freeMemory();

    snprintf(buf, sizeof(buf), "%d %s", f, (f ? "bytes" : "byte"));

    mKeypad.clear();
    mKeypad.setCursor(0, 0);
    mKeypad.print("Free Memory:");
    mKeypad.setCursor(mKeypad.getCols() - strlen(buf), 1);
    mKeypad.print(buf);
    delay(2500);
    mKeypad.clear();

    displayBanner(2500);
}

//
// Arduino's setup function (called once at startup).
//
void setup()
{
    mFilters.SetUserFilterName(dhwFilters::FILTER_USER_1, " Bypass ");
    mFilters.SetUserFilterEnabled(dhwFilters::FILTER_USER_1);

    mFilterMaxLen = mFilters.GetMaxNameLength();

    for (uint8_t i = 0; i < sizeof(_glyphs) / sizeof(_glyphs[0]); i++)
        mKeypad.createChar(i, (uint8_t *)_glyphs[i]);

    displayBanner();
    displayUpdate();
}

//
// Arduino's main loop (never ending)
//
void loop()
{
    while (true)
    {
        DFR_Key_t k = mKeypad.getKey();

        // No button was pressed, next pass.
        if (k == KEY_WAIT)
        {
            delay(50);
            continue;
        }

        // Five buttons handling
        switch (k)
        {
            case KEY_UP:
                if (mAttFocused)
                {
                    mAtt.Inc(!mAttUnity ? 10 : 1); // Increment by 10 if tenth is selected, other by 1

                    displayAtt();
                }
                else
                {
                    mFilters.Next(); // Select next filter
                    displayFilter();
                }
                break;

            case KEY_DOWN:
                if (mAttFocused)
                {
                    mAtt.Dec(!mAttUnity ? 10 : 1); // Decrement by 10 if tenth is selected, other by 1

                    if ((mAttUnity == false) && mAtt.GetValue() < 10)
                        mAttUnity = true;

                    displayAtt();
                }
                else
                {
                    mFilters.Previous(); // Seletec previous filter
                    displayFilter();
                }
                break;

            case KEY_LEFT:
                if (mAttFocused)
                {
                    if (mAtt.GetValue() >= 10) // Switch from unity and tenth, if attenuation value is >= 10
                        mAttUnity = !mAttUnity;
                    else
                        mAtt.Dec(10); // Quick go to Attenuation 0 is value is < 10

                    displayAtt();
                }
                break;

            case KEY_RIGHT:
                if (mAttFocused)
                {
                    if (mAtt.GetValue() >= 10) // Switch from unity and tenth, if attenuation value is >= 10
                        mAttUnity = !mAttUnity;
                    else
                    {
                        mAtt.Inc(10); // Quickly increment +10 if attenuation is < 10
                    }
                    displayAtt();
                }
                else
                {
                    displayInfo(); // Display informations
                    displayUpdate();
                }
                break;

            case KEY_SELECT:
                mAttFocused = !mAttFocused; // Toggle between attenuator and filter
                displayUpdate();
                break;

            case KEY_NO:
            default: // NOOP
                break;
        }

        // Take some rest
        delay(150);
    }
}
