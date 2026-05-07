#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    int count = 1;

    while (1) {
        printf("hello %d\n", count);

        count++;
        if (count > 9) {
            count = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
