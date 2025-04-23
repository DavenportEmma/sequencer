#include "stm32f722xx.h"
#include "rotary_encoder.h"
#include "FreeRTOS.h"
#include "semphr.h"

static volatile int8_t direction = 0;

int8_t get_encoder_direction() {
    int8_t ret = direction;

    direction = 0;

    return ret;
}

void EXTI15_10_IRQHandler(void) {
    if((EXTI->PR & (1 << 11)) || (EXTI->PR & (1 << 12))) {
        uint16_t pin11 = (GPIOA->IDR & (1 << 11)) ? 1 : 0;
        uint16_t pin12 = (GPIOA->IDR & (1 << 12)) ? 1 : 0;

        if(EXTI->PR & (1 << 11)) {
            if(pin12) {
                direction = 1;
            }
            EXTI->PR |= (1 << 11);      // clear interrupt flag
        }

        if(EXTI->PR & (1 << 12)) {
            if(pin11) {
                direction = -1;
            }
            EXTI->PR |= (1 << 12);      // clear interrupt flag
        }
        

    }
}
