#include <stdio.h>
#include <pthread.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h" 
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Local includes. */
#include "console.h"

#include <termios.h>

static void prvTask_getChar(void *pvParameters);
static void prvTask_led(void *pvParameters);
static void prvTask_decodificador(void *pvParameters);
static void prvTask_processText(void *pvParameters);

#define TASK1_PRIORITY 1
#define TASK2_PRIORITY 1
#define TASK3_PRIORITY 1
#define TASK4_PRIORITY 1

#define DELAY_POINT   500
#define DELAY_TRACE   1500
#define DELAY_ESPACE  500
#define DELAY_DEFAULT 500
#define DELAY_CHAR    1500

#define BLACK "\033[30m" /* Black */
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */
#define DISABLE_CURSOR() printf("\e[?25l")
#define ENABLE_CURSOR() printf("\e[?25h")

#define clear() printf("\033[H\033[J")
#define gotoxy(x, y) printf("\033[%d;%dH", (y), (x))

#define ESC   0x1B
#define ENTER 0x0A
#define SPACE 0x20

static uint32_t pos_x_init = 2;
static uint32_t pos_y_init = 4;

const char* caracter_morse[37] = {
    "-----", //0 
    ".----", //1
    "..---", //2
    "...--", //3
    "....-", //4
    ".....", //5
    "-....", //6
    "--...", //7
    "---..", //8
    "----.",  //9
    ".-",   //A
    "-...", //B
    "-.-.", //C
    "-..",  //D
    ".",    //E
    "..-.", //F
    "--.",  //G
    "....", //H
    "..",   //I
    ".---", //J
    "-.-",  //K
    ".-..", //L
    "--",   //M
    "-.",   //N
    "---",  //O
    ".--.", //P
    "--.-", //Q
    ".-.",  //R
    "...",  //S
    "-",    //T
    "..-",  //U
    "...-", //V
    ".--",  //W
    "-..-", //X
    "-.--", //Y
    "--..", //Z
    "    " //space
};


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
TaskHandle_t keyTask_hdlr, msgTask_hdlr, decodeTask_hdlr;

/*funcao responsavel por validar os caracteres(somente letras e numeros e espaço)*/
static int data_Validator(uint8_t data)
{
    if((data >= '0' &&  data <= '9') || (data >= 'A' && data <= 'Z') || (data >= 'a' && data <= 'z') || data == SPACE)
    {
        return 1;
    }

    return 0;
}

/*funcao responsavel por decodificar os caracteres em morse*/
static char* decodificador_Morse(char data)
{
    char index = 0;

    if((data >= '0' &&  data <= '9'))
    {
        index = data - '0';
        return (uint8_t*)caracter_morse[index];
    }
    else if((data >= 'A' && data <= 'Z') )
    {
        index = (data - 'A') + 10;
        return (uint8_t*)caracter_morse[index];
    }
    else if((data >= 'a' && data <= 'z'))
    {
        index = (data - 'a') + 10;
        return (uint8_t*)caracter_morse[index];
    }
    else
    {
        index = 36;
        return (uint8_t*)caracter_morse[index];
    }

    return '0';
}
/*task responsavel por pegar o caracter do teclado*/
static void prvTask_getChar(void *pvParameters)
{
    uint32_t notificationValue;      /*variavel para receber a notificacao*/
    unsigned long timer_notify = 0;  /*variavel para determinar o wait da notificacao*/
    char key;                        /*variavel para receber a tecla pressionada*/


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

    for (;;)
    {
        if (xTaskNotifyWait(
        ULONG_MAX,
        ULONG_MAX,
        &notificationValue,
        timer_notify))
        {
            /*caso receba uma notificacao com o valor da tecla ESC encerra o programa*/
            if(notificationValue == ESC)
            {
                break;
            }
            /*notificacao com o valor ENTER, coloca o timer da notificaco no maximo*/
            else if(notificationValue == ENTER)
            {
                timer_notify = portMAX_DELAY;
            }
            /*notificacao com outro valor, coloca o timer da notificaco zerada para receber os dados do teclado*/
            else{
                timer_notify = 0;
            }
        }

        /*pega a tecla difitada*/
        key = getchar();
        
        /*verifica se e maior que 1*/
        if(key > 1 )

        {
            /*insere a tecla digitada na queue*/
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

/*task responsavel por processar as teclas passadas via queue */
static void prvTask_processText(void *pvParameters)
{
    char text;
    uint32_t text_lenght = 0;

    for(;;)
    {
        /*se receber algo pela queue entra no if*/
        if(xQueueReceive(structQueue_key,&text,portMAX_DELAY) == pdPASS)
        {
            /*se a tecla for ENTER e ja tiver recebido algo entra no if*/
            if(text == ENTER && text_lenght > 0)
            {   
                DISABLE_CURSOR();

                /*cria a task led para mostrar o resultado*/
                xTaskCreate(prvTask_led, "LED_Output", configMINIMAL_STACK_SIZE, &green, TASK4_PRIORITY, NULL);

                /*manda uma notificacao para a task prvTask_getChar para pausar a leitura do teclado*/
                xTaskNotify(keyTask_hdlr, ENTER, eSetValueWithOverwrite);

                 /*coloca a task de decoder e do led em funcionamento*/
                 vTaskResume(decodeTask_hdlr);

                /*manda uma notificacao apara a a task de decoder passando a quantidade de caracteres*/
                xTaskNotify(decodeTask_hdlr, text_lenght, eSetValueWithOverwrite);

                vTaskSuspend(msgTask_hdlr);

                text_lenght = 0;
                text = 0;
            }
            else if (text == ESC)
            {
                /*manda uma notificacao para a task prvTask_getChar para encerrar o programa*/
                 xTaskNotify(keyTask_hdlr, ESC, eSetValueWithOverwrite);
            }
            else
            {
                /*Verifica se é um dado valido para repassar ao decodificador, 
                dado valido seria numeros e letras e o espaço*/
                if(1 == data_Validator(text))
                {
                    /*se o dado for valido insere na queue e incrementa a variavel text_lenght*/
                    if(xQueueSend(structQueue_text, &text, 0) == pdTRUE)
                    {
                        text_lenght++;
                    }
                }
            }
        }

        /*senao recebeu nada da um delay de 50ms */
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/*task responsavel por decodificar os caracteres em codigo morse (. -)*/
static void prvTask_decodificador(void *pvParameters)
{
    uint32_t notificationValue;  /*variavel onde recebe os dados da notificacao*/
    uint32_t decode_lenght = 0;  /*variavel que recebe o tamanho dos dados a serem recebidos*/
    unsigned long timer_decode = portMAX_DELAY;   /*variavel de timer para a notificacao*/
    char caracter;   /*variavel para receber o caracter pela queue*/

    for(;;)
    {
        if (xTaskNotifyWait(
                ULONG_MAX,
                ULONG_MAX,
                &notificationValue,
                timer_decode))
        {
            /*recebe o tamanho dos dados a serem processados pela notificacao*/
            decode_lenght = notificationValue; 
            
            /*zera o timer da notificacao para entrar a todo momento*/
            timer_decode = 0;
        }

        if(decode_lenght > 0)
        {
            if(xQueueReceive(structQueue_text, &caracter, 0) == pdPASS)
            {
                /*passa o caracter para a funao e recebe um ponteiro com o codigo morse*/
                char* data_morse = decodificador_Morse(caracter);
                
                /*insere na queue o codigo morse recebido*/
                xQueueSend(structQueue_morse, &data_morse, 0);
                
                /*declementa a quantidade de dados a serem decodificado*/
                decode_lenght--;
            }
        }
        else
        {
            /*retorna a task de processamento*/
            vTaskResume(msgTask_hdlr);

            /*coloca o timer da notificacao para o maximo e suspende a task*/
            timer_decode = portMAX_DELAY;
            vTaskSuspend(decodeTask_hdlr);
        }
    }
    vTaskDelete(NULL);
}

static void prvTask_led(void *pvParameters)
{
    char x;
    char* morse_code;
    uint8_t lenght_code = 0;

    // pvParameters contains LED params
    st_led_param_t *led = (st_led_param_t *)pvParameters;
    portTickType xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        /*se tiver alguma coisa na queue entra, caso nao tem esoera por 150 */ 
        if(xQueueReceive(structQueue_morse, &morse_code, 150) == pdPASS)
        {
            /*recebe o tamanho do codigo morse referente a um caracter 
            e faz um for para mostrar o codigo ao usuario*/
            lenght_code = strlen(morse_code);
            for(int i = 0; i < lenght_code; i++)
            {
                x = morse_code[i];

                if(x == '.')
                {
                    gotoxy(pos_x_init++, pos_y_init);
                    printf("%c", x);

                    gotoxy(led->pos, 2);
                    printf("%s⬤", led->color);
                    fflush(stdout);
                    vTaskDelay(DELAY_POINT / portTICK_PERIOD_MS);

                    gotoxy(led->pos, 2);
                    printf("%s ", BLACK);
                    fflush(stdout);
                    vTaskDelay(DELAY_DEFAULT / portTICK_PERIOD_MS);
                }
                else if(x == '-'){

                    gotoxy(pos_x_init++, pos_y_init);
                    printf("%c", x);

                    gotoxy(led->pos, 2);
                    printf("%s⬤", led->color);
                    fflush(stdout);
                    vTaskDelay(DELAY_TRACE / portTICK_PERIOD_MS);

                    gotoxy(led->pos, 2);
                    printf("%s ", BLACK);
                    fflush(stdout);
                    vTaskDelay(DELAY_DEFAULT / portTICK_PERIOD_MS);
                }
                else{
                    gotoxy(led->pos, 2);
                    printf("%s ", BLACK);
                    fflush(stdout);
                    vTaskDelay(DELAY_ESPACE / portTICK_PERIOD_MS);
                }

            }

            /*da um delay de 1,5s a cada codigo morse*/
            vTaskDelay(DELAY_CHAR / portTICK_PERIOD_MS);
            gotoxy(pos_x_init++, pos_y_init);
            printf("  ");
        }
        /*caso nao tenha nada na queue reseta as variaveis glabais e deleta a task*/
        else
        {
            /*envia uma notificacao para a task key para que volte a ler o teclado*/
            xTaskNotify(keyTask_hdlr, 0UL, eSetValueWithOverwrite);
            
            pos_x_init = 2;
            pos_y_init++;

            vTaskDelete(NULL);
        }
    }

    vTaskDelete(NULL);
}

void app_run(void)
{
    structQueue_key = xQueueCreate(100,sizeof(uint8_t));
    structQueue_text = xQueueCreate(100,sizeof(uint8_t));
    structQueue_morse = xQueueCreate(100,8);

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

    /*Inicialmente suspende as task de decoder e led*/
    vTaskSuspend(decodeTask_hdlr);
    //vTaskSuspend(ledTask_hdlr);

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