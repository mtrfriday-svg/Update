#include "led_bar.h"
#include "config.h"

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const gpio_num_t kLedPins[8] = {LED_BAR_PIN0, LED_BAR_PIN1, LED_BAR_PIN2, LED_BAR_PIN3,
                                       LED_BAR_PIN4, LED_BAR_PIN5, LED_BAR_PIN6, LED_BAR_PIN7};

LedBar& LedBar::GetInstance() {
    static LedBar inst;
    return inst;
}

void LedBar::Init() {
    if (inited_) return;
    inited_ = true;
    for (int i = 0; i < 8; i++) {
        gpio_config_t io = {};
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = 1ULL << kLedPins[i];
        gpio_config(&io);
    }
    Write(0);
    xTaskCreate(&LedBar::Task, "led_bar", 2048, this, 2, nullptr);
}

void LedBar::Write(unsigned mask) {
    for (int i = 0; i < 8; i++) {
        bool on = (mask >> i) & 1;
        gpio_set_level(kLedPins[i], LED_BAR_ACTIVE_HIGH ? on : !on);
    }
}

void LedBar::Task(void* arg) {
    auto* self = static_cast<LedBar*>(arg);
    while (true) {
        int64_t now = esp_timer_get_time() / 1000;
        unsigned mask = 0;
        switch (self->pattern_) {
            case MdeeLights::kOff: mask = 0; break;
            case MdeeLights::kOn: mask = 0xFF; break;
            case MdeeLights::kChase: {
                int p = (now / 90) % 14;
                int i = p < 8 ? p : 14 - p;
                mask = 1u << i;
                break;
            }
            case MdeeLights::kBlink: mask = ((now / 400) & 1) ? 0xFF : 0; break;
            case MdeeLights::kSignalLeft: mask = ((now / 350) & 1) ? 0x0F : 0; break;
            case MdeeLights::kSignalRight: mask = ((now / 350) & 1) ? 0xF0 : 0; break;
            case MdeeLights::kBatteryBar: {
                auto& robot = MdeeRobot::GetInstance();
                if (!robot.LinkUp() || robot.BatteryVolts() < 4.0f) {
                    mask = ((now / 500) & 1) ? 0x01 : 0;       // no motor-battery reading: blink one LED
                } else {
                    int n = (robot.BatteryPercent() * 8 + 99) / 100;
                    mask = n >= 8 ? 0xFFu : ((1u << n) - 1u);
                }
                break;
            }
        }
        self->Write(mask);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
