#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <generated/soc.h>
#include <system.h>

#include "aht10.h"
#include "lora_RFM95.h"

// Protótipos locais
static char *readstr(void);
static char *get_token(char **str);
static void prompt(void);
static void help(void);
static void reboot(void);
static void toggle_led(void);
static void console_service(void);
static void lorainfo(void);

// Novos protótipos
static void send_sensor_data(void);
static void busy_wait_ms(unsigned int ms);


static void busy_wait_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for (volatile int j = 0; j < 2000; j++); // ajuste conforme necessário
#endif
    }
}


static char *readstr(void)
{
    char c[2];
    static char s[64];
    static int ptr = 0;

    if(readchar_nonblock()) {
        c[0] = readchar();
        c[1] = 0;
        switch(c[0]) {
            case 0x7f:
            case 0x08:
                if(ptr > 0) {
                    ptr--;
                    putsnonl("\x08 \x08");
                }
                break;
            case 0x07:
                break;
            case '\r':
            case '\n':
                s[ptr] = 0x00;
                putsnonl("\n");
                ptr = 0;
                return s;
            default:
                if(ptr >= (sizeof(s) - 1))
                    break;
                putsnonl(c);
                s[ptr] = c[0];
                ptr++;
                break;
        }
    }
    return NULL;
}

static char *get_token(char **str)
{
    char *c, *d;

    c = (char *)strchr(*str, ' ');
    if(c == NULL) {
        d = *str;
        *str = *str+strlen(*str);
        return d;
    }
    *c = 0;
    d = *str;
    *str = c+1;
    return d;
}

static void prompt(void)
{
    printf("RUNTIME>");
}

static void help(void)
{
    puts("Available commands:");
    puts("help                            - this command");
    puts("reboot                          - reboot CPU");
    puts("led                             - led test");
    puts("send                            - ler AHT10 e enviar via LoRa");
    puts("lorainfo                        - exibir informações do módulo LoRa");
    puts("i2cscan                         - varrer barramento I2C e listar dispositivos");
}

static void reboot(void)
{
    ctrl_reset_write(1);
}

static void toggle_led(void)
{
    int i;
    printf("invertendo led...\n");
    i = leds_out_read();
    leds_out_write(!i);
}

// ============================================
// === Nova função: ler AHT10 e enviar via LoRa ===
// ============================================
static void send_sensor_data(void) {
    dados my_data; // definido em aht10.h
    printf("Lendo dados do sensor AHT10...\n");

    if (aht10_get_data(&my_data)) {
        printf("  Temperatura: %d.%02d C\n", my_data.temperatura/100, abs(my_data.temperatura) % 100);
        printf("  Umidade: %d.%02d %%\n", my_data.umidade/100, abs(my_data.umidade) % 100);

        if (!lora_send_bytes((uint8_t*)&my_data, sizeof(dados))) {
            printf("Erro durante o envio LoRa (verificar log da biblioteca).\n");
        } else {
            printf("Dados enviados via LoRa.\n");
        }
    } else {
        printf("Erro ao ler dados do AHT10. Envio LoRa abortado.\n");
    }
}

void lorainfo(void) {
    uint8_t version = lora_read_reg(0x42);
    printf("LoRa Version: 0x%02X\n", version);
}
// ============================================
/* Console */
static void console_service(void) {
    char *str;
    char *token;

    str = readstr();
    if(str == NULL) return;
    token = get_token(&str);
    if(strcmp(token, "help") == 0)
        help();
    else if(strcmp(token, "reboot") == 0)
        reboot();
    else if(strcmp(token, "led") == 0)
        toggle_led();
    else if(strcmp(token, "send") == 0)
        send_sensor_data();
    else if(strcmp(token, "lorainfo") == 0)
        lorainfo();
    else if(strcmp(token, "i2cscan") == 0)
        i2c_scan();
    else
        puts("Comando desconhecido. Digite 'help'.");
    prompt();
}


// ============================================
// === main ===
// ============================================
int main(void) {
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

#ifdef CSR_TIMER0_BASE
    timer0_en_write(0);
    timer0_reload_write(0);
    timer0_load_write(CONFIG_CLOCK_FREQUENCY / 1000000); // microssegundos
    timer0_en_write(1);
    timer0_update_value_write(1);
#endif

    busy_wait_ms(500);

    printf("Hellorld!\n");
    printf("Tarefa 05 – Transmissão de dados via LoRa\n");

    i2c_init();
    aht10_init();
    if (!lora_init()) {
        printf("ATENÇÃO: falha na inicialização do LoRa. Verifique conexões/config.\n");
        // continua para permitir uso do console
    }

    help();
    prompt();

    while(1) {
        console_service();
    }

    return 0;
}