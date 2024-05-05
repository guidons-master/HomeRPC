#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "HomeRPC.h"

#define BLINK_GPIO 2

static uint8_t s_led_state = 0;

static void configure_led(void) {
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

// 触发LED
static rpc_any_t trigger_led(rpc_any_t state) {
    gpio_set_level(BLINK_GPIO, state.uc);
    rpc_any_t ret;
    ret.i = 0;
    return ret;
}

// 获取LED状态
static rpc_any_t led_status(void) {
    rpc_any_t ret;
    ret.uc = s_led_state;
    return ret;
}

void app_main(void) {
    configure_led();
    // 启动HomeRPC
    HomeRPC.start();
    // 服务列表
    Service_t services[] = {
        {
            .func = trigger_led,
            .input_type = "i",
            .output_type = 'i',
            .name = "trigger",
            .desc = "open the light",
        },
        {
            .func = led_status,
            .input_type = "",
            .output_type = 'i',
            .name = "status",
            .desc = "check the light status, return 1 if on, 0 if off",
        }
    };
    // 设备信息
    Device_t led = {
        .place = "room",
        .type = "light",
        .id = 1,
        .services = services,
        .services_num = sizeof(services) / sizeof(Service_t)
    };
    // 注册设备
    HomeRPC.addDevice(&led);
    // 调用服务
    Device_t led2 = {
        .place = "room",
        .type = "light",
        .id = 1
    };

    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        // 调用服务
        // rpc_any_t status = HomeRPC.callService(&led2, "status", NULL, 10);
        // printf("led status: %d\n", status.i);
    }
}