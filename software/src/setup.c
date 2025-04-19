#include "stm32f722xx.h"
#include "uart.h"
#include "spi.h"
#include "midi.h"
#include "w25q128jv.h"
#include "keyboard.h"
#include "menu.h"

void setup() {
    /*
    setup uart for st link
    setup uart for midi
    setup spi flash chip
    setup gpio for keyboard
    */
   int err;

    USART_Handler u;
    u.uart = USART3;
    u.baud = 9600;
    u.gpio = GPIOD;
    u.tx_pin = 8;
    u.rx_pin = 9;
    u.afr_reg = 1;
    u.rx_interrupts = 0;

    err = init_uart(&u);
    if(err == 0) {
        send_uart(USART3, "ST-Link UART initialised\n\r", 26);
    }

    u.uart = USART1;
    u.baud = 31250;
    u.gpio = GPIOB;
    u.tx_pin = 6;
    u.rx_pin = 7;
    u.afr_reg = 0;
    u.rx_interrupts = 0;

    err = init_uart(&u);
    if(err) {
        send_uart(USART3, "Error initialising MIDI UART\n\r", 30);
    }
    
    NVIC_EnableIRQ(USART1_IRQn);

    SPI_Handler s;
    s.spi = SPI1;
    s.gpio = GPIOA;
    s.miso = 6;
    s.mosi = 7;
    s.clk = 5;
    s.cs = 4;

    err = init_spi(&s);
    if(err) {
        send_uart(USART3, "Error initialising SPI\n\r", 24);
    }

    uint8_t tx[] = {0x9F, 0x00, 0x00, 0x00};
    uint8_t rx[4];
    CS_low(GPIOA, 4);
    SPI_tx_rx(SPI1, tx, rx, 4);
    CS_high(GPIOA, 4);
    if(rx[1] != 0xEF || rx[2] != 0x40 || rx[3] != 0x18) {
        send_uart(USART3, "Error initialising SPI flash chip\n\r", 34);
    }

    // shift register
    // initialise gpio
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    GPIOD->MODER |= 0x5400;
    GPIOD->ODR |= 0x0000;
    // pull CLR high
    GPIOD->ODR |= CLR;

    // column input pins
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    // port C defaults to input on all pins
    // enable pull down resistors
    GPIOC->PUPDR |= (2 << (ROW_INPUT_0_PIN * 2)) | (2 << (ROW_INPUT_1_PIN * 2)) |
                    (2 << (ROW_INPUT_2_PIN * 2)) | (2 << (ROW_INPUT_3_PIN * 2)) |
                    (2 << (ROW_INPUT_4_PIN * 2)) | (2 << (ROW_INPUT_5_PIN * 2)) |
                    (2 << (ROW_INPUT_6_PIN * 2)) | (2 << (ROW_INPUT_7_PIN * 2));

    uint8_t buffer[CONFIG_ROLLOVER];
    kbuf_handle_t kbuf = kbuf_init(buffer, CONFIG_ROLLOVER);
    
    kbuf_reset(kbuf);
    
    scan(kbuf);

    #ifdef CONFIG_ENABLE_STARTUP_MEMORY_CLEAR
    if(!kbuf_empty(kbuf)) {
        send_uart(USART3, "Start up key press\n\r", 20);
        send_uart(USART3, "Erasing flash\n\r", 15);
        eraseChip();
    }
    #endif

    kbuf_free(kbuf);

    // initialise menu state machine
    menu(E_MAIN_MENU);

    send_uart(USART3, "Finished initialisation\n\r", 25);
}
