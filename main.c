/*
 * Copyright (C) 2017 Ken Bannister <kb2ma@runbox.com>
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
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "saul_reg.h"
#include "xtimer.h"

int main(void)
{
    printf("Data collector initializing...\n");

    saul_reg_t* dev = saul_reg_find_name("jc42");
    if (dev == NULL) {
        printf("Can't find jc42 sensor\n");
        return 1;
    }

    while (1) {
        phydat_t phy;

        int res = saul_reg_read(dev, &phy);
        if (res) {
            printf("temperature: %d.%02d C\n", phy.val[0] / 100, phy.val[0] % 100);
        }
        else {
            printf("Sensor read failure: %d\n", res);
        }

        xtimer_usleep(1000 * US_PER_MS);
    }

    return 0;
}
