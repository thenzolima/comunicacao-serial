#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"
#include "algarismos.h"

#define WS2812_PIN 7
#define NUM_PIXELS 25
#define LED_RED_PIN 13
#define LED_BLUE_PIN 12
#define LED_GREEN_PIN 11
#define BTN_A_PIN 5
#define BTN_B_PIN 6

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define SSD1306_ADDR 0x3C

#define DEBOUNCE_DELAY_US 300000

volatile bool btn_a_flag = false;
volatile bool btn_b_flag = false;
volatile uint64_t last_interrupt_time_a = 0;
volatile uint64_t last_interrupt_time_b = 0;

volatile bool led_green_state = false;
volatile bool led_blue_state = false;

ssd1306_t ssd;

// Envia um pixel com cor definida para o WS2812
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Converte valores RGB para o formato GRB usado pelo WS2812
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Exibe um número em LEDs WS2812 usando um buffer de bits
void display_number(bool *buffer, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        put_pixel(buffer[i] ? color : 0);
    }
}

// Atualiza o texto exibido no display OLED
void update_display_message(const char *msg)
{
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, msg, 0, 0);
    ssd1306_send_data(&ssd);
}

// Trata interrupções dos botões para alternar LEDs e evitar bouncing
void gpio_callback(uint gpio, uint32_t events)
{
    uint64_t current_time = time_us_64();

    if (gpio == BTN_A_PIN)
    {
        if (current_time - last_interrupt_time_a > DEBOUNCE_DELAY_US)
        {
            led_green_state = !led_green_state;
            gpio_put(LED_GREEN_PIN, led_green_state);
            last_interrupt_time_a = current_time;
            btn_a_flag = true;
        }
    }
    else if (gpio == BTN_B_PIN)
    {
        if (current_time - last_interrupt_time_b > DEBOUNCE_DELAY_US)
        {
            led_blue_state = !led_blue_state;
            gpio_put(LED_BLUE_PIN, led_blue_state);
            last_interrupt_time_b = current_time;
            btn_b_flag = true;
        }
    }
}

// Configuração inicial de GPIOs, I2C, display OLED e interrupções
void setup()
{
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);

    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, 128, 64, false, SSD1306_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    update_display_message("Sistema Iniciado");
}

// Função principal: inicializa o sistema e gerencia o loop principal
int main()
{
    stdio_init_all();
    setup();

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    char serial_char = 0;
    char msg[50];

    while (true)
    {
        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT)
        {
            serial_char = (char)ch;
            sprintf(msg, "Recebido: %c", serial_char);
            update_display_message(msg);
            printf("Caractere recebido: %c\n", serial_char);

            if (serial_char >= '0' && serial_char <= '9')
            {
                int digit = serial_char - '0';
                display_number(numbers[digit], 0, 180, 200);
            }
            else
            {
                for (int i = 0; i < NUM_PIXELS; i++)
                {
                    put_pixel(0);
                }
            }
        }

        if (btn_a_flag)
        {
            btn_a_flag = false;
            sprintf(msg, "LED Verde %s", led_green_state ? "ligado" : "desligado");
            update_display_message(msg);
            printf("Botao A pressionado: LED Verde %s\n", led_green_state ? "ligado" : "desligado");
        }
        if (btn_b_flag)
        {
            btn_b_flag = false;
            sprintf(msg, "LED Azul %s", led_blue_state ? "ligado" : "desligado");
            update_display_message(msg);
            printf("Botao B pressionado: LED Azul %s\n", led_blue_state ? "ligado" : "desligado");
        }
        sleep_ms(50);
    }

    return 0;
}
