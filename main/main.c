/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

const int ECHO_PIN = 14;
const int TRIGGER_PIN = 15;

volatile int time_init = 0;
volatile uint64_t time_end = 0;
volatile uint64_t timer_fired = 0;

void gpio_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {
        time_end = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8) {
        time_init = to_us_since_boot(get_absolute_time());
    }
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    timer_fired = 1;

    return 0;
}

void trigger_sensor() {
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);
}

float get_distance() {
    int duration = time_end - time_init;
    return (duration * 0.034) / 2.0;
}


int main() {
    stdio_init_all();

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);


    datetime_t t = {
        .year = 2025,
        .month = 03,
        .day = 15,
        .dotw = 6, // 0 is Sunday, so 3 is Wednesday
        .hour = 19,
        .min = 05,
        .sec = 00};
    rtc_init();
    rtc_set_datetime(&t);

    alarm_id_t alarm;
    char start;
    float distance = 0;
    int estado = 0;

    printf("Pressione 'A' para iniciar o monitoramento\n");
    printf("Pressione 'Z' para pausar o monitoramento\n\n");

    while (true) {
        if (!estado) {
            start = getchar_timeout_us(1000);
            if (start == 'A') {
                printf("---------------------\n");
                printf("INICIOU MONITORAMENTO\n");
                printf("---------------------\n\n");
                estado = 1;
            }
        }

        if (estado) {
            time_init = 0;
            time_end = 0;

            trigger_sensor();

            alarm = add_alarm_in_ms(1000, alarm_callback, NULL, false);

            if (!(alarm)) {
                printf("Failed to add timer\n");
            }

            // Aguarda o sensor ser acionado e falha após 1 segundo (caso não haja resposta)
            while (time_end == 0) {
                if (timer_fired) {
                    timer_fired = 0;
                    rtc_get_datetime(&t);
                    char datetime_buf[256];
                    char *datetime_str = &datetime_buf[0];
                    datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
                    printf("%s - Falhou!\n", datetime_str);
                    break;
                } 
            }

            if (time_end != 0) {
                cancel_alarm(alarm);

                distance = get_distance();

                rtc_get_datetime(&t);
                char datetime_buf[256];
                char *datetime_str = &datetime_buf[0];
                datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
                printf("%s - %f cm\n", datetime_str, distance);
            }

            start = getchar_timeout_us(1000);
            if (start == 'Z') {
                printf("---------------------\n");
                printf("PAROU O MONITORAMENTO\n");
                printf("---------------------\n\n");
                estado = 0;
            }

            sleep_ms(1000);
        }
    }
}
