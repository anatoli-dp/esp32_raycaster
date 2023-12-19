#include <mutex>

#include "./display.h"
#include "../images/splash/images.h"

#define BACKLIGHT_PIN 3
#define BACKLIGHT_CHANNEL 0

TFT_eSPI tft = TFT_eSPI();

static TFT_eSprite* displayBuffer[2];
static bool bufferSel = 0;

static std::mutex updateLock;
static bool updateChange = false;

static TaskHandle_t updateDisplayTask;
static void updateDisplay (void* parameter)
{
    tft.startWrite();
    bool lastUpdateChange = updateChange;
    while (true)
    {
        updateLock.lock();
        if (lastUpdateChange != updateChange)
        {
            displayBuffer[!bufferSel]->pushSprite(0, 0);
            lastUpdateChange = updateChange;
        }
        updateLock.unlock();
        delay(1);
    }
    tft.endWrite();
}

static void drawSplash ()
{
    {
        //splash images
        bb_sprload(-3, splash00, sizeof(splash00));
        bb_sprload(-4, splash01, sizeof(splash01));
        bb_sprload(-5, splash02, sizeof(splash02));
        bb_sprload(-6, splash03, sizeof(splash03));
        bb_sprload(-7, splash04, sizeof(splash04));
        bb_sprload(-8, splash05, sizeof(splash05));
        bb_sprload(-9, splash06, sizeof(splash06));
        bb_sprload(-10, splash07, sizeof(splash07));
        bb_sprload(-11, splash08, sizeof(splash08));
        bb_sprload(-12, splash09, sizeof(splash09));
        bb_sprload(-13, splash10, sizeof(splash10));
        bb_sprload(-14, splash11, sizeof(splash11));
        bb_sprload(-15, splash12, sizeof(splash12));
        bb_sprload(-16, splash13, sizeof(splash13));
        bb_sprload(-17, splash14, sizeof(splash14));
        bb_sprload(-18, splash15, sizeof(splash15));
        bb_sprload(-19, splash16, sizeof(splash16));
        bb_sprload(-20, splash17, sizeof(splash17));
        bb_sprload(-21, splash18, sizeof(splash18));
        bb_sprload(-22, splash19, sizeof(splash19));
        bb_sprload(-23, splash20, sizeof(splash20));
        bb_sprload(-24, splash21, sizeof(splash21));
        bb_sprload(-25, splash22, sizeof(splash22));
        bb_sprload(-26, splash23, sizeof(splash23));
        bb_sprload(-27, splash24, sizeof(splash24));
        bb_sprload(-28, splash25, sizeof(splash25));
        bb_sprload(-29, splash26, sizeof(splash26));
    }

    bb_display_brightness(100);

    for (int f = -3; f > -30; f--)
    {
        bb_spr(f, 0, 0);
        bb_sprfree(f);
        bb_flip();

        if (f == -3) delay(1500);
        if (f == -11 || f == -14 || f == -20) delay(250);
        if (f == -29) delay(1000);

        delay(85);
    }
}

void display_init ()
{
    ledcSetup(BACKLIGHT_CHANNEL, 5000, 10);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);

    bb_display_brightness(0);

    tft.init();
    tft.setRotation(3);

    bb_sprnew(-1, BB_DISPLAY_WIDTH, BB_DISPLAY_HEIGHT);
    bb_sprnew(-2, BB_DISPLAY_WIDTH, BB_DISPLAY_HEIGHT);

    displayBuffer[0] = bb_sprget(-1);
    displayBuffer[1] = bb_sprget(-2);

    xTaskCreatePinnedToCore(updateDisplay, "Update Display", 1500, NULL, 0, &updateDisplayTask, 0);

    //drawSplash();
    bb_display_brightness(100);
}

TFT_eSprite* bb_displaybuffer (bool current)
{
    return (current ? displayBuffer[bufferSel] : displayBuffer[!bufferSel]);
}

void bb_flip (bool copy)
{
    updateLock.lock();
    if (copy) {
        /*for (int x = 0; x < BB_DISPLAY_WIDTH; x++)
        {
            for (int y = 0; y < BB_DISPLAY_HEIGHT; y++)
            {
                displayBuffer[!bufferSel]->drawPixel(x, y, displayBuffer[bufferSel]->readPixel(x, y));
            }
        }*/
        displayBuffer[bufferSel]->pushToSprite(displayBuffer[!bufferSel], 0, 0);
    } else {
        bufferSel = !bufferSel;
    }
    updateChange = !updateChange;
    updateLock.unlock();
}

void bb_clear (bb_color color)
{
    displayBuffer[bufferSel]->fillSprite(convertColor(color));
}

void bb_display_brightness (int percentage)
{
    if (percentage > 100)
        percentage = 100;
    else if (percentage < 0)
        percentage = 0;

    uint32_t duty_cycle = (1023 * percentage) / 100;
    ledcWrite(BACKLIGHT_CHANNEL, duty_cycle);
}