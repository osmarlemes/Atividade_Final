#include <stdio.h>
#include <pthread.h>

/* Kernel includes. */
#include "FreeRTOS.h" 
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"

static void prvTask_getChar(void *pvParameters);
static void prvTask_led(void *pvParameters);
static void prvTask_decodificador(void *pvParameters);
static void prvTask_processText(void *pvParameters);

#define TASK1_PRIORITY 1
#define TASK2_PRIORITY 1
#define TASK3_PRIORITY 1
#define TASK4_PRIORITY 1

#define DELAY_POINT  100
#define DELAY_TRACE  200
#define DELAY_ESPACE 400

#define BLACK "\033[30m" /* Black */
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */
#define DISABLE_CURSOR() printf("\e[?25l")
#define ENABLE_CURSOR() printf("\e[?25h")

#define clear() printf("\033[H\033[J")
#define gotoxy(x, y) printf("\033[%d;%dH", (y), (x))

#define ESC   0x1B
#define ENTER 0x0A

typedef struct
{
    int pos;
    char *color;
    int period_ms;
} st_led_param_t;

st_led_param_t green = {
    6,
    GREEN,
    250};
st_led_param_t red = {
    13,
    RED,
    100};

QueueHandle_t structQueue_key = NULL;
QueueHandle_t structQueue_text = NULL;
QueueHandle_t structQueue_morse = NULL;
TaskHandle_t keyTask_hdlr, msgTask_hdlr, decodeTask_hdlr, ledTask_hdlr;

#include <termios.h>

/**
 * @brief 
 * 
 * @param pvParameters 
 */
static int data_Validator(uint8_t data)
{
    if((data >= '0' &&  data <= '9') || (data >= 'A' && data <= 'Z') || (data >= 'a' && data <= 'z'))
    {
        return 1;
    }

    return 0;
}

static void prvTask_getChar(void *pvParameters)
{
    uint32_t notificationValue;
    char key;
    int n;

    /* I need to change  the keyboard behavior to
    enable nonblock getchar */
    struct termios initial_settings,
        new_settings;

    tcgetattr(0, &initial_settings);

    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON;
    new_settings.c_lflag &= ~ECHO;
    new_settings.c_lflag &= ~ISIG;
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 1;
    tcsetattr(0, TCSANOW, &new_settings);
    /* End of keyboard configuration */

    printf("task 1");
    for (;;)
    {
        if (xTaskNotifyWait(
        ULONG_MAX,
        ULONG_MAX,
        &notificationValue,
        0x0))
        {
            if(notificationValue == ESC)
            {
                
            }
        }

        key = getchar();
        
        if(key > 1 )
        {
            printf("task 1");
            if (xQueueSend(structQueue_key, &key, 0) != pdTRUE)
            {
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    tcsetattr(0, TCSANOW, &initial_settings);
    ENABLE_CURSOR();
    exit(0);
    vTaskDelete(NULL);
}

static void prvTask_processText(void *pvParameters)
{
    char text;
    uint32_t text_lenght = 0;

    for(;;)
    {
        if(xQueueReceive(structQueue_key,&text,portMAX_DELAY) == pdPASS)
        {
            if(text == ENTER)
            {
                printf("ENTER");
               // vTaskSuspend(keyTask_hdlr);
                xTaskNotify(decodeTask_hdlr, text_lenght, eSetValueWithOverwrite);
                printf("ENTER 2");
            }
            else if (text == ESC)
            {
                 xTaskNotify(keyTask_hdlr, 01UL, eSetValueWithOverwrite);
            }
            else
            {
                /*Verifica se é um dado valido para repassar ao decodificador, 
                dado valido seria numeros e letras e o espaço*/
                printf("ta aqui");
                if(1 == data_Validator(text))
                {
                    if(xQueueSend(structQueue_text, &text, 0) == pdTRUE)
                    {
                        text_lenght++;
                        printf("%i/n",text_lenght);
                    }
                }
            }
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void prvTask_decodificador(void *pvParameters)
{
    
    uint32_t notificationValue2;
    uint32_t decode_lenght = 0;
    char caracter;

    for(;;)
    {
        printf("task31");
        if (xTaskNotifyWait(
                ULONG_MAX,
                ULONG_MAX,
                &notificationValue2,
                portMAX_DELAY))
        {
            printf("task3");
            decode_lenght = notificationValue2; 
        }
        if(decode_lenght > 0)
        {
            printf("task 3");
            if(xQueueReceive(structQueue_text, &caracter, 0) == pdPASS);
            //printf("%s",caracter);
        }
    }
    vTaskDelete(NULL);
}

static void prvTask_led(void *pvParameters)
{
    // pvParameters contains LED params
    st_led_param_t *led = (st_led_param_t *)pvParameters;
    portTickType xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        // console_print("@");
        gotoxy(led->pos, 2);
        printf("%s⬤", led->color);
        fflush(stdout);
        vTaskDelay(led->period_ms / portTICK_PERIOD_MS);

        gotoxy(led->pos, 2);
        printf("%s ", BLACK);
        fflush(stdout);
        vTaskDelay(led->period_ms / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

static void prvTask_ledAction(void *pvParameters)
{
    uint32_t notificationValue;
    uint32_t var_pause = 0;

    for (;;)
    {        
        if (xTaskNotifyWait(
                ULONG_MAX,
                ULONG_MAX,
                &notificationValue,
                portMAX_DELAY))
        {
            if(notificationValue == 1)
            {
                /*limpa variavel para desbloquear teclas e os leds*/
                var_pause = 0;
                
                /*Apagar o led vermelho*/
                gotoxy(red.pos, 2);
                printf("%s ", BLACK);
                fflush(stdout);
            }
            else if(notificationValue == 2 && var_pause == 0)
            {
                /*seta variavel para bloquear as acoes dos demais teclas*/
                var_pause = 1;
                gotoxy(green.pos, 2);
                printf("%s ", BLACK);
                fflush(stdout);

                // console_print("@");
                gotoxy(red.pos, 2);
                printf("%s⬤", red.color);
                fflush(stdout);

            }
            else if(notificationValue == 3 && var_pause == 0)
            {
                // console_print("@");
                gotoxy(red.pos, 2);
                printf("%s⬤", red.color);
                fflush(stdout);
                vTaskDelay(red.period_ms / portTICK_PERIOD_MS);

                gotoxy(red.pos, 2);
                printf("%s ", BLACK);
                fflush(stdout);
            }
        }
    }

    vTaskDelete(NULL);
}

void app_run(void)
{
    structQueue_key = xQueueCreate(100,sizeof(uint8_t));
    structQueue_text = xQueueCreate(100,sizeof(uint8_t));
    structQueue_morse = xQueueCreate(100,sizeof(uint8_t));

    if(structQueue_key == NULL || structQueue_text == NULL || structQueue_morse == NULL)
    {
        exit(1);
    }

    clear();
    DISABLE_CURSOR();
    printf(
        "╔═════════════════╗\n"
        "║                 ║\n"
        "╚═════════════════╝\n");

    xTaskCreate(prvTask_getChar, "Get_key", configMINIMAL_STACK_SIZE, NULL, TASK1_PRIORITY, &keyTask_hdlr);
    xTaskCreate(prvTask_processText, "Msg_Text", configMINIMAL_STACK_SIZE, NULL, TASK2_PRIORITY, &msgTask_hdlr);
    xTaskCreate(prvTask_decodificador, "Decode", configMINIMAL_STACK_SIZE, NULL, TASK3_PRIORITY, &decodeTask_hdlr);
  //  xTaskCreate(prvTask_led, "LED_Output", configMINIMAL_STACK_SIZE, &green, TASK4_PRIORITY, &ledTask_hdlr);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached.  If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks      to be created.  See the memory management section on the
     * FreeRTOS web site for more details. */
    for (;;)
    {
    }
}