/*
Copyright (c) 2019 lewis he
This is just a demonstration. Most of the functions are not implemented.
The main implementation is low-power standby.
The off-screen standby (not deep sleep) current is about 4mA.
Select standard motherboard and standard backplane for testing.
Created by Lewis he on October 10, 2019.
*/

// Please select the model you want to use in config.h
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <soc/rtc.h>
#include "esp_wifi.h"
#include "esp_sleep.h"
#include <WiFi.h>
#include "gui.h"

#define G_EVENT_VBUS_PLUGIN         _BV(0)
#define G_EVENT_VBUS_REMOVE         _BV(1)
#define G_EVENT_CHARGE_DONE         _BV(2)

#define G_EVENT_WIFI_SCAN_START     _BV(3)
#define G_EVENT_WIFI_SCAN_DONE      _BV(4)
#define G_EVENT_WIFI_CONNECTED      _BV(5)
#define G_EVENT_WIFI_BEGIN          _BV(6)
#define G_EVENT_WIFI_OFF            _BV(7)

#define RTC_TIME_ZONE               "CST-8"

enum {
    Q_EVENT_WIFI_SCAN_DONE,
    Q_EVENT_WIFI_CONNECT,
    Q_EVENT_BMA_INT,
    Q_EVENT_AXP_INT,
} ;

#define DEFAULT_SCREEN_TIMEOUT      10*1000
#define DIM_SCREEN_TIMEOUT          5*1000
#define ACTIVE_BRIGHTNESS           150
#define DIM_BRIGHTNESS              50

#define RTC_SYNC_INTERVAL_MS        (60UL * 60UL * 1000UL)
#define WIFI_SYNC_INTERVAL_MS       (4UL * 60UL * 60UL * 1000UL)
#define WIFI_CONNECT_TIMEOUT_MS     8000UL

#define WATCH_FLAG_SLEEP_MODE   _BV(1)
#define WATCH_FLAG_SLEEP_EXIT   _BV(2)
#define WATCH_FLAG_BMA_IRQ      _BV(3)
#define WATCH_FLAG_AXP_IRQ      _BV(4)

QueueHandle_t g_event_queue_handle = NULL;
EventGroupHandle_t g_event_group = NULL;
bool lenergy = false;
TTGOClass *ttgo;
bool backlight_dimmed = false;
uint32_t last_rtc_sync_ms = 0;
uint32_t last_wifi_sync_ms = 0;


void periodicRTCSync()
{
    uint32_t now = millis();
    if (now - last_rtc_sync_ms >= RTC_SYNC_INTERVAL_MS) {
        ttgo->rtc->syncToSystem();
        last_rtc_sync_ms = now;
    }
}


void smartWiFiSyncChargingOnly()
{
    uint32_t now = millis();

    if (!ttgo->power->isChargeing()) {
        return;
    }

    if (now - last_wifi_sync_ms < WIFI_SYNC_INTERVAL_MS) {
        return;
    }

    bool connectedBefore = WiFi.isConnected();

    if (!connectedBefore) {
        WiFi.mode(WIFI_STA);
        WiFi.begin();

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        configTzTime(RTC_TIME_ZONE, "pool.ntp.org");
        ttgo->rtc->syncToSystem();
        last_rtc_sync_ms = now;
    }

    if (!connectedBefore) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifi_connect_status(false);
    }

    last_wifi_sync_ms = now;
}


void updatePowerPolicies()
{
    uint32_t inactive = lv_disp_get_inactive_time(NULL);

    if (inactive < DIM_SCREEN_TIMEOUT) {
        if (backlight_dimmed) {
            ttgo->setBrightness(ACTIVE_BRIGHTNESS);
            backlight_dimmed = false;
        }
        return;
    }

    if (inactive < DEFAULT_SCREEN_TIMEOUT) {
        if (!backlight_dimmed) {
            ttgo->setBrightness(DIM_BRIGHTNESS);
            backlight_dimmed = true;
        }
        return;
    }

    low_energy();
}


void setupNetwork()
{
    WiFi.mode(WIFI_OFF);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        xEventGroupClearBits(g_event_group, G_EVENT_WIFI_CONNECTED);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        uint8_t data = Q_EVENT_WIFI_SCAN_DONE;
        xQueueSend(g_event_queue_handle, &data, portMAX_DELAY);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_SCAN_DONE);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        xEventGroupSetBits(g_event_group, G_EVENT_WIFI_CONNECTED);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        wifi_connect_status(true);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
}



void low_energy()
{
    if (ttgo->bl->isOn()) {
        ttgo->closeBL();
        ttgo->stopLvglTick();
        ttgo->bma->enableStepCountInterrupt(false);
        ttgo->bma->enableWakeupInterrupt(true);
        ttgo->bma->enableAccel();
        ttgo->displaySleep();
        if (!WiFi.isConnected()) {
            lenergy = true;
            backlight_dimmed = false;
            WiFi.mode(WIFI_OFF);
            Serial.println("ENTER IN LIGHT SLEEEP MODE");
            gpio_wakeup_enable ((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable ((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
            esp_sleep_enable_gpio_wakeup ();
            esp_light_sleep_start();
        }
    } else {
        ttgo->startLvglTick();
        ttgo->displayWakeup();
        ttgo->rtc->syncToSystem();
        last_rtc_sync_ms = millis();
        updateStepCounter(ttgo->bma->getCounter());
        updateBatteryLevel();
        updateBatteryIcon(LV_ICON_CALCULATION);
        lv_disp_trig_activity(NULL);
        ttgo->openBL();
        ttgo->setBrightness(ACTIVE_BRIGHTNESS);
        backlight_dimmed = false;
        lenergy = false;
        ttgo->bma->enableWakeupInterrupt(false);
        ttgo->bma->enableAccel();
        ttgo->bma->enableStepCountInterrupt();
    }
}

void setup()
{
    Serial.begin(115200);

    //Create a program that allows the required message objects and group flags
    g_event_queue_handle = xQueueCreate(20, sizeof(uint8_t));
    g_event_group = xEventGroupCreate();

    ttgo = TTGOClass::getWatch();

    //Initialize TWatch
    ttgo->begin();

    // Tier 1: Reduce CPU frequency for better efficiency
    setCpuFrequencyMhz(80);

    // Turn on the IRQ used
    ttgo->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
    ttgo->power->enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);
    // Tier 1: Turn off audio power rail when unused
    ttgo->power->setPowerOutPut(AXP202_LDO3, AXP202_OFF);
    ttgo->power->clearIRQ();

    //Initialize lvgl
    ttgo->lvgl_begin();

    // Enable BMA423 interrupt ，
    // The default interrupt configuration,
    // you need to set the acceleration parameters, please refer to the BMA423_Accel example
    ttgo->bma->attachInterrupt();

    //Connection interrupted to the specified pin
    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, [] {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t data = Q_EVENT_BMA_INT;
        xQueueSendFromISR(g_event_queue_handle, &data, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR ();
        }
    }, RISING);

    // Connection interrupted to the specified pin
    pinMode(AXP202_INT, INPUT);
    attachInterrupt(AXP202_INT, [] {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t data = Q_EVENT_AXP_INT;
        xQueueSendFromISR(g_event_queue_handle, &data, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR ();
        }
    }, FALLING);

    //Check if the RTC clock matches, if not, use compile time
    ttgo->rtc->check();

    //Synchronize time to system time
    ttgo->rtc->syncToSystem();
    last_rtc_sync_ms = millis();

#ifdef LILYGO_WATCH_HAS_BUTTON

    /*
        ttgo->button->setClickHandler([]() {
            Serial.println("Button2 Pressed");
        });
    */

    //Set the user button long press to restart
    ttgo->button->setLongClickHandler([]() {
        Serial.println("Pressed Restart Button,Restart now ...");
        delay(1000);
        esp_restart();
    });
#endif

    //Setting up the network
    setupNetwork();

    //Execute your own GUI interface
    setupGui();

    //Clear lvgl counter
    lv_disp_trig_activity(NULL);

#ifdef LILYGO_WATCH_HAS_BUTTON
    //In lvgl we call the button processing regularly
    lv_task_create([](lv_task_t *args) {
        ttgo->button->loop();
    }, 30, 1, nullptr);
#endif

    //When the initialization is complete, turn on the backlight
    ttgo->openBL();
    // Tier 1: Lower default brightness
    ttgo->setBrightness(ACTIVE_BRIGHTNESS);
}

void loop()
{
    bool  rlst;
    uint8_t data;
    if (xQueueReceive(g_event_queue_handle, &data, 5 / portTICK_RATE_MS) == pdPASS) {
        switch (data) {
        case Q_EVENT_BMA_INT:
            do {
                rlst =  ttgo->bma->readInterrupt();
            } while (!rlst);
            //! setp counter
            if (ttgo->bma->isStepCounter()) {
                updateStepCounter(ttgo->bma->getCounter());
            }
            break;
        case Q_EVENT_AXP_INT:
            ttgo->power->readIRQ();
            if (ttgo->power->isVbusPlugInIRQ()) {
                updateBatteryIcon(LV_ICON_CHARGE);
            }
            if (ttgo->power->isVbusRemoveIRQ()) {
                updateBatteryIcon(LV_ICON_CALCULATION);
            }
            if (ttgo->power->isChargingDoneIRQ()) {
                updateBatteryIcon(LV_ICON_CALCULATION);
            }
            if (ttgo->power->isPEKShortPressIRQ()) {
                ttgo->power->clearIRQ();
                low_energy();
                return;
            }
            ttgo->power->clearIRQ();
            break;
        case Q_EVENT_WIFI_SCAN_DONE: {
            int16_t len =  WiFi.scanComplete();
            for (int i = 0; i < len; ++i) {
                wifi_list_add(WiFi.SSID(i).c_str());
            }
            break;
        }
        default:
            break;
        }
    }
    if (lenergy) {
        return;
    }

    if (lv_disp_get_inactive_time(NULL) < DEFAULT_SCREEN_TIMEOUT) {
        lv_task_handler();
    }

    // Tier 2: Multi-stage dimming + sleep policy
    updatePowerPolicies();

    // Tier 2: Periodic RTC synchronization
    periodicRTCSync();

    // Tier 2: Smart WiFi sync only when charging
    smartWiFiSyncChargingOnly();
}

