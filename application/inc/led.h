
#ifndef LED_H
#define LED_H

#include "stdint.h"
#include "stdbool.h"
#include "sdk_errors.h"

void led_init(void);
void lfclk_request(void);

#endif // LED_H
