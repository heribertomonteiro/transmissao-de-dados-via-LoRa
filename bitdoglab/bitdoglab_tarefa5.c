#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdbool.h>
#include "inc/lora_RFM95.h"
#include "inc/ssd1306.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15

// LoRa pinout (use the same pins as your other project)
#define PIN_RST 20
#define PIN_DIO0 8
#define LORA_FREQUENCY 915E6

ssd1306_t disp;
#include "blink.pio.h"

#define SEND_INTERVAL_MS 10000 

typedef struct {
    int16_t temperatura;
	int16_t umidade;
} aht10_dados;

void print_texto(char *msg, uint pos_x, uint pos_y, uint scale){
    ssd1306_draw_string(&disp, pos_x, pos_y, scale, (uint8_t*)msg); // cast para uint8_t* se draw_string espera isso
}

void to_binary_string(uint8_t val, char *str) {
    for (int i = 7; i >= 0; i--) {
        str[7 - i] = (val & (1 << i)) ? '1' : '0';
    }
    str[8] = '\0'; // termina com null
}

static int text_width_px(const char* s, int scale) {
    return (int)strlen(s) * 6 * (scale > 0 ? scale : 1);
}

static int center_x_for(const char* s, int scale) {
    int w = text_width_px(s, scale);
    int x = (128 - w) / 2;
    return x < 0 ? 0 : x;
}

static void print_texto_centered(const char* msg, int y, int scale) {
    int x = center_x_for(msg, scale);
    ssd1306_draw_string(&disp, x, y, (uint)scale, (const uint8_t*)msg);
}

static void show_temp_umid(float temp_c, float umid_pct) {
    char line[32];
    ssd1306_clear(&disp);
    int y_top = (64 - (16 * 2)) / 2;
    snprintf(line, sizeof(line), "T %.2fC", temp_c);
    print_texto_centered(line, y_top, 2);
    snprintf(line, sizeof(line), "U %.2f%%", umid_pct);
    print_texto_centered(line, y_top + 20, 2); 
    ssd1306_show(&disp);
}


int main()
{
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);
    gpio_pull_up(I2C_SDA);
    disp.external_vcc = false;
    
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    ssd1306_clear(&disp);
    print_texto_centered("Esperando dados...", (64 - 8) / 2, 1);
    ssd1306_show(&disp);
    sleep_ms(1000);
    
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);


    // -------------------------
    // Inicializacao do LoRa
    // -------------------------
    lora_config_t lora_cfg = {
        .spi_instance = SPI_PORT,
        .pin_miso = PIN_MISO,
        .pin_cs = PIN_CS,
        .pin_sck = PIN_SCK,
        .pin_mosi = PIN_MOSI,
        .pin_rst = PIN_RST,
        .pin_dio0 = PIN_DIO0,
        .frequency = LORA_FREQUENCY
    };

    printf("Inicializando modulo LoRa (%.0f Hz)...\n", (float)LORA_FREQUENCY);
    if (!lora_init(lora_cfg)) {
        printf("[ERRO] Falha ao inicializar o modulo LoRa.\n");
    } else {
        printf("[SUCESSO] Modulo LoRa inicializado. Colocando em RX contínuo...\n");
        lora_start_rx_continuous();
    }

    uint8_t rxbuf[64];
    bool got_first_data = false;
    uint32_t anim_tick = 0;
    int dots = 1;
    while (true) {
        int len = lora_receive_bytes(rxbuf, sizeof(rxbuf));
        if (len == sizeof(aht10_dados)) {
            aht10_dados rec;
            memcpy(&rec, rxbuf, sizeof(rec));
            float temp = rec.temperatura / 100.0f;
            float umid = rec.umidade / 100.0f;
            show_temp_umid(temp, umid);
            got_first_data = true;
            int rssi = lora_get_rssi();
            printf("Recebido T=%.2fC U=%.2f%% RSSI=%d dBm\n", temp, umid, rssi);
        } else if (len > 0) {
            printf("LoRa recebeu %d bytes (raw): ", len);
            for (int i = 0; i < len; ++i) printf("%02X ", rxbuf[i]);
            printf("\n");
        } else {
            if (!got_first_data) {
                anim_tick++;
                if ((anim_tick % 3) == 0) {
                    char msg[32];
                    snprintf(msg, sizeof(msg), "Esperando dados%.*s", dots, "...");
                    ssd1306_clear(&disp);
                    print_texto_centered(msg, (64 - 8) / 2, 1);
                    ssd1306_show(&disp);
                    dots++;
                    if (dots > 3) dots = 1;
                }
            }
        }
        sleep_ms(100);
    }
}
