#include <DisplayManager.h>
#include <FastLED_NeoMatrix.h>
#include "MatrixDisplayUi.h"
#include <TJpg_Decoder.h>
#include "icons.h"
#include "Globals.h"
#include <ArduinoJson.h>
#include "PeripheryManager.h"
#include "MQTTManager.h"
#include "GifPlayer.h"
#include <Ticker.h>
#include "Functions.h"
#include "ServerManager.h"
#include "MenuManager.h"
#include "Frames.h"

Ticker AlarmTicker;
Ticker TimerTicker;

#define MATRIX_PIN 32
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

bool appIsSwitching;

GifPlayer gif;

CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
FastLED_NeoMatrix matrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);

MatrixDisplayUi ui(&matrix);

DisplayManager_ &DisplayManager_::getInstance()
{
    static DisplayManager_ instance;
    return instance;
}

DisplayManager_ &DisplayManager = DisplayManager.getInstance();

void DisplayManager_::setBrightness(uint8_t bri)
{
    if (MATRIX_OFF && !ALARM_ACTIVE)
    {
        matrix.setBrightness(0);
    }
    else
    {
        matrix.setBrightness(bri);
    }
}

void DisplayManager_::setFPS(uint8_t fps)
{
    ui.setTargetFPS(fps);
}

void DisplayManager_::setTextColor(uint16_t color)
{
    matrix.setTextColor(color);
}

void DisplayManager_::MatrixState(bool on)
{
    MATRIX_OFF = !on;
    setBrightness(BRIGHTNESS);
}

bool DisplayManager_::setAutoTransition(bool active)
{
    if (active && AUTO_TRANSITION)
    {
        ui.enablesetAutoTransition();
        return true;
    }
    else
    {
        ui.disablesetAutoTransition();
        return false;
    }
}

void DisplayManager_::drawGIF(uint16_t x, uint16_t y, fs::File gifFile)
{
    gif.setFile(gifFile);
    gif.drawFrame(x, y);
}

void DisplayManager_::drawJPG(uint16_t x, uint16_t y, fs::File jpgFile)
{
    TJpgDec.drawFsJpg(x, y, jpgFile);
}

void DisplayManager_::applyAllSettings()
{
    ui.setTargetFPS(MATRIX_FPS);
    ui.setTimePerApp(TIME_PER_APP);
    ui.setTimePerTransition(TIME_PER_TRANSITION);
    setBrightness(BRIGHTNESS);
    setTextColor(TEXTCOLOR_565);
    setAutoTransition(AUTO_TRANSITION);
}

void DisplayManager_::resetTextColor()
{
    matrix.setTextColor(TEXTCOLOR_565);
}

void DisplayManager_::clearMatrix()
{
    matrix.clear();
    matrix.show();
}

bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    uint16_t bitmapIndex = 0;

    for (uint16_t row = 0; row < h; row++)
    {
        for (uint16_t col = 0; col < w; col++)
        {
            uint16_t color = bitmap[bitmapIndex++];
            uint8_t r = ((color & 0xF800) >> 11) << 3;
            uint8_t g = ((color & 0x07E0) >> 5) << 2;
            uint8_t b = (color & 0x001F) << 3;
            matrix.drawPixel(x + col, y + row, matrix.Color(r, g, b));
        }
    }
    return 0;
}

void DisplayManager_::printText(int16_t x, int16_t y, const char *text, bool centered, bool ignoreUppercase)
{
    if (centered)
    {
        uint16_t textWidth = getTextWidth(text, ignoreUppercase);
        int16_t textX = ((32 - textWidth) / 2);
        matrix.setCursor(textX, y);
    }
    else
    {
        matrix.setCursor(x, y);
    }

    if (UPPERCASE_LETTERS && !ignoreUppercase)
    {
        size_t length = strlen(text);
        char upperText[length + 1]; // +1 for the null terminator

        for (size_t i = 0; i < length; ++i)
        {
            upperText[i] = toupper(text[i]);
        }

        upperText[length] = '\0'; // Null terminator
        matrix.print(utf8ascii(upperText));
    }
    else
    {
        matrix.print(utf8ascii(text));
    }
}

void DisplayManager_::HSVtext(int16_t x, int16_t y, const char *text, bool clear)
{
    if (clear)
        matrix.clear();
    static uint8_t hueOffset = 0;
    uint16_t xpos = 0;
    for (uint16_t i = 0; i < strlen(text); i++)
    {
        uint8_t hue = map(i, 0, strlen(text), 0, 255) + hueOffset;
        uint32_t textColor = hsvToRgb(hue, 255, 255);
        matrix.setTextColor(textColor);
        const char *myChar = &text[i];

        matrix.setCursor(xpos + x, y);
        if (UPPERCASE_LETTERS)
        {
            matrix.print(utf8ascii((char)toupper(text[i])));
        }
        else
        {
            matrix.print(utf8ascii(&text[i]));
        }
        char temp_str[2] = {'\0', '\0'};
        temp_str[0] = text[i];
        xpos += getTextWidth(temp_str, false);
    }
    hueOffset++;
    if (clear)
        matrix.show();
}

void pushCustomFrame(String name)
{

    if (customFrames.count(name) == 0)
    {
        ++customPagesCount;
        void (*customFrames[10])(FastLED_NeoMatrix *, MatrixDisplayUiState *, int16_t, int16_t, bool, bool) = {CFrame1, CFrame2, CFrame3, CFrame4, CFrame5, CFrame6, CFrame7, CFrame8, CFrame9, CFrame10};
        if (customFrames[customPagesCount] != NULL)
        {
            Apps.push_back(std::make_pair(name, customFrames[customPagesCount]));
            ui.setApps(Apps); // Add frames
        }
        else
        {
            ++customPagesCount;
        }
    }
}

void removeCustomFrame(const String &name)
{
    // Suchen Sie nach dem Element, das dem Namen entspricht
    auto it = std::find_if(Apps.begin(), Apps.end(), [&name](const std::pair<String, AppCallback> &appPair)
                           { return appPair.first == name; });

    // Wenn das Element gefunden wurde, entfernen Sie es aus dem Vektor
    if (it != Apps.end())
    {
        Apps.erase(it);
        ui.setApps(Apps); // Aktualisieren Sie die Frames
    }
}

void DisplayManager_::generateCustomPage(String name, String payload)
{

    if (payload == "" && customFrames.count(name))
    {
        customFrames.erase(customFrames.find(name));
        removeCustomFrame(name);
        return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
        return;

    CustomFrame customFrame;

    if (doc.containsKey("sound"))
    {
        customFrame.sound = ("/" + doc["sound"].as<String>() + ".txt");
    }
    else
    {
        customFrame.sound = "";
    }

    customFrame.rainbow = doc.containsKey("rainbow") ? doc["rainbow"] : false;
    customFrame.pushIcon = doc.containsKey("pushIcon") ? doc["pushIcon"] : 0;
    customFrame.name = name;
    customFrame.text = doc["text"].as<String>();

    customFrame.color = doc.containsKey("color") ? doc["color"].is<String>() ? hexToRgb565(doc["color"]) : doc["color"].is<JsonArray>() ? hexToRgb565(doc["color"].as<String>())
                                                                                                                                        : TEXTCOLOR_565
                                                 : TEXTCOLOR_565;

    if (currentCustomFrame != name)
    {
        customFrame.scrollposition = 9;
    }

    customFrame.repeat = doc.containsKey("repeat") ? doc["repeat"].as<uint8_t>() : -1;

    if (doc.containsKey("icon"))
    {
        String iconFileName = String(doc["icon"].as<String>());

        if (LittleFS.exists("/ICONS/" + iconFileName + ".jpg"))
        {
            customFrame.isGif = false;
            customFrame.icon = LittleFS.open("/ICONS/" + iconFileName + ".jpg");
        }
        else if (LittleFS.exists("/ICONS/" + iconFileName + ".gif"))
        {
            customFrame.isGif = true;
            customFrame.icon = LittleFS.open("/ICONS/" + iconFileName + ".gif");
        }
    }

    pushCustomFrame(name);
    customFrames[name] = customFrame;
}

void DisplayManager_::generateNotification(String payload)
{
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, payload);

    notify.duration = doc.containsKey("duration") ? doc["duration"].as<int>() * 1000 : TIME_PER_APP;
    notify.text = doc["text"].as<String>();
    notify.repeat = doc.containsKey("repeat") ? doc["repeat"].as<uint16_t>() : -1;
    notify.rainbow = doc.containsKey("rainbow") ? doc["rainbow"].as<bool>() : false;
    notify.hold = doc.containsKey("hold") ? doc["hold"].as<bool>() : false;
    notify.pushIcon = doc.containsKey("pushIcon") ? doc["pushIcon"] : 0;
    notify.flag = true;
    notify.startime = millis();
    notify.scrollposition = 9;
    notify.iconWasPushed = false;
    notify.iconPosition = 0;

    if (doc.containsKey("sound"))
    {
        PeripheryManager.playFromFile("/MELODIES/" + doc["sound"].as<String>() + ".txt");
    }

    notify.color = doc.containsKey("color") ? doc["color"].is<String>() ? hexToRgb565(doc["color"]) : doc["color"].is<JsonArray>() ? hexToRgb565(doc["color"].as<String>())
                                                                                                                                   : TEXTCOLOR_565
                                            : TEXTCOLOR_565;

    if (doc.containsKey("icon"))
    {
        String iconFileName = doc["icon"].as<String>();
        if (LittleFS.exists("/ICONS/" + iconFileName + ".jpg"))
        {
            notify.isGif = false;
            notify.icon = LittleFS.open("/ICONS/" + iconFileName + ".jpg");
        }
        else if (LittleFS.exists("/ICONS/" + iconFileName + ".gif"))
        {
            notify.isGif = true;
            notify.icon = LittleFS.open("/ICONS/" + iconFileName + ".gif");
        }
    }
    else
    {
        File f;
        notify.icon = f;
    }
}

void DisplayManager_::loadApps()
{
    Apps.clear();
    Apps.push_back(std::make_pair("time", TimeFrame));
    if (SHOW_DATE)
        Apps.push_back(std::make_pair("date", DateFrame));
    if (SHOW_TEMP)
        Apps.push_back(std::make_pair("temp", TempFrame));
    if (SHOW_HUM)
        Apps.push_back(std::make_pair("hum", HumFrame));
    if (SHOW_BATTERY)
        Apps.push_back(std::make_pair("bat", BatFrame));
    // if (SHOW_WEATHER)
    //     Apps.push_back(std::make_pair(5, WeatherFrame));
    nativeAppsCount = Apps.size();
    ui.setApps(Apps); // Add frames
    if (AUTO_TRANSITION && nativeAppsCount == 1)
        setAutoTransition(false);
    StartAppUpdater();
}

void DisplayManager_::setup()
{
    TJpgDec.setCallback(jpg_output);
    FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);
    gif.setMatrix(&matrix);
    ui.setAppAnimation(SLIDE_DOWN);
    ui.setOverlays(overlays, 4);
    setAutoTransition(AUTO_TRANSITION);
    ui.init();
}

void DisplayManager_::tick()
{
    if (AP_MODE)
    {
        HSVtext(2, 6, "AP MODE", true);
    }
    else
    {
        ui.update();
        if (ui.getUiState()->frameState == IN_TRANSITION && !appIsSwitching)
        {
            appIsSwitching = true;
            MQTTManager.setCurrentApp(CURRENT_APP);
        }
        else if (ui.getUiState()->frameState == FIXED && appIsSwitching)
        {
            appIsSwitching = false;
            MQTTManager.setCurrentApp(CURRENT_APP);
        }
    }
}

void DisplayManager_::clear()
{
    matrix.clear();
}

void DisplayManager_::show()
{
    matrix.show();
}

void DisplayManager_::leftButton()
{
    if (!MenuManager.inMenu)
        ui.previousApp();
}

void DisplayManager_::rightButton()
{
    if (!MenuManager.inMenu)
        ui.nextApp();
}

void DisplayManager_::nextApp()
{
    if (!MenuManager.inMenu)
        ui.nextApp();
}

void DisplayManager_::previousApp()
{
    if (!MenuManager.inMenu)
        ui.previousApp();
}

void snozzeTimerCallback()
{
    ALARM_ACTIVE = true;
    AlarmTicker.detach();
}

void DisplayManager_::selectButton()
{
    if (!MenuManager.inMenu)
    {
        if (notify.flag && notify.hold)
        {
            DisplayManager.getInstance().dismissNotify();
        }
        if (ALARM_ACTIVE && SNOOZE_TIME > 0)
        {
            PeripheryManager.stopSound();
            ALARM_ACTIVE = false;
            AlarmTicker.once(SNOOZE_TIME * 60, snozzeTimerCallback);
        }

        if (TIMER_ACTIVE)
        {
            PeripheryManager.stopSound();
            TIMER_ACTIVE = false;
        }
    }
}

void DisplayManager_::selectButtonLong()
{
    if (ALARM_ACTIVE)
    {
        PeripheryManager.stopSound();
        ALARM_ACTIVE = false;
    }
}

void DisplayManager_::dismissNotify()
{
    notify.hold = false;
    notify.flag = false;
}

void timerCallback()
{
    TIMER_ACTIVE = true;
    TimerTicker.detach();
}

void DisplayManager_::gererateTimer(String Payload)
{
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, Payload);
    if (error)
        return;
    int hours = doc["hours"] | 0;
    int minutes = doc["minutes"] | 0;
    int seconds = doc["seconds"] | 0;
    TIMER_SOUND = doc.containsKey("sound") ? doc["sound"].as<String>() : "";
    time_t now = time(nullptr);
    struct tm futureTimeinfo = *localtime(&now);
    futureTimeinfo.tm_hour += hours;
    futureTimeinfo.tm_min += minutes;
    futureTimeinfo.tm_sec += seconds;
    time_t futureTime = mktime(&futureTimeinfo);
    int interval = difftime(futureTime, now) * 1000;
    TimerTicker.attach_ms(interval, timerCallback);
}

void DisplayManager_::switchToApp(String Payload)
{
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, Payload);
    if (error)
        return;
    String name = doc["name"].as<String>();
    bool fast = doc["fast"] | false;
    int index = findAppIndexByName(name);
    if (index > -1)
        ui.transitionToApp(index);
}

void DisplayManager_::setNewSettings(String Payload)
{
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, Payload);
    if (error)
        return;
    TIME_PER_APP = doc.containsKey("apptime") ? doc["apptime"] : TIME_PER_APP;
    TIME_PER_TRANSITION = doc.containsKey("transition") ? doc["transition"] : TIME_PER_TRANSITION;
    TEXTCOLOR_565 = doc.containsKey("textcolor") ? hexToRgb565(doc["textcolor"]) : TEXTCOLOR_565;
    MATRIX_FPS = doc.containsKey("fps") ? doc["fps"] : MATRIX_FPS;
    BRIGHTNESS = doc.containsKey("brightness") ? doc["brightness"] : BRIGHTNESS;
    AUTO_BRIGHTNESS = doc.containsKey("autobrightness") ? doc["autobrightness"] : AUTO_BRIGHTNESS;
    AUTO_TRANSITION = doc.containsKey("autotransition") ? doc["autotransition"] : AUTO_TRANSITION;
    applyAllSettings();
    saveSettings();
}

void DisplayManager_::drawProgressBar(int cur, int total)
{
    matrix.clear();
    int progress = (cur * 100) / total;
    char progressStr[5];
    snprintf(progressStr, 5, "%d%%", progress); // Formatieren des Prozentzeichens
    printText(0, 6, progressStr, true, false);
    int leds_for_progress = (progress * MATRIX_WIDTH * MATRIX_HEIGHT) / 100;
    matrix.drawFastHLine(0, 7, MATRIX_WIDTH, matrix.Color(100, 100, 100));
    matrix.drawFastHLine(0, 7, leds_for_progress / MATRIX_HEIGHT, matrix.Color(0, 255, 0));
    matrix.show();
}

void DisplayManager_::drawMenuIndicator(int cur, int total)
{
    int menuItemWidth = 2;
    int totalWidth = total * menuItemWidth + (total - 2);
    int leftMargin = (MATRIX_WIDTH - totalWidth) / 2;
    int pixelSpacing = 1;
    for (int i = 0; i < total; i++)
    {
        int x = leftMargin + i * (menuItemWidth + pixelSpacing);
        if (i == cur)
        {
            matrix.drawLine(x, 7, x + menuItemWidth - 1, 7, matrix.Color(255, 0, 0));
        }
        else
        {
            matrix.drawLine(x, 7, x + menuItemWidth - 1, 7, matrix.Color(100, 100, 100));
        }
    }
}