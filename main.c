/*
 * Copyright (C) 2017  Koen Zandberg <koen@bergzand.net>,
 *                     Ken Bannister <kb2ma@runbox.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       Collect jc42 sensor readings
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "jc42.h"
#include "periph/i2c.h"
#include "xtimer.h"

int main(void)
{
    jc42_t dev;
    jc42_params_t params = {
        .i2c   = I2C_0,
        .addr  = 0x18,
        .speed = I2C_SPEED_NORMAL
    };

    printf("Initializing sensor...\n");

    if (jc42_init(&dev, &params) != 0) {
        printf("Failed\n");
        return 1;
    }

    /* read temperature every second */
    int16_t temperature;
    while (1) {
        if (jc42_get_temperature(&dev, &temperature) != 0) {
            printf("Temperature reading ailed\n");
        }
        else {
            printf("temperature: %d.%02d C\n", temperature / 100, temperature % 100);
        }

        xtimer_usleep(1000 * US_PER_MS);
    }

    return 0;
}
