/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Pacman for STM32
  *
  * LCD 16x2, 4-bit parallel:
  *   RS → PB5,  EN → PB4
  *   D4 → PC7,  D5 → PB6,  D6 → PA7,  D7 → PA6
  *
  * Button → PA0 (PULLDOWN, active-HIGH) = any action / select
  *
  * UART2 keyboard (PA2=TX, PA3=RX, 9600 baud):
  *   'w' / Up arrow    = move up
  *   's' / Down arrow  = move down
  *   'a' / Left arrow  = move left
  *   'd' / Right arrow = move right
  *   Physical button   = move down (toggle row, like original select)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SPEED_PAC   150    /* ms between pacman moves   */
#define SPEED_GHOST 2000    /* base ms between ghost moves */
#define MAXX          15
#define MAXY           1
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;
UART_HandleTypeDef huart2;

/* ── Custom character bitmaps ───────────────────────────────── */
static uint8_t bmp_pacman[8] = {
    0x00,0x00,0x0E,0x1B,0x1C,0x0E,0x00,0x00
};
static uint8_t bmp_ghost[8] = {
    0x00,0x00,0x0E,0x15,0x1F,0x1F,0x15,0x00
};
static uint8_t bmp_point[8] = {
    0x00,0x00,0x00,0x0E,0x0E,0x00,0x00,0x00
};

/* ── Game state ──────────────────────────────────────────────── */
static uint8_t  points[MAXX+1][MAXY+1];  /* 1=dot present */

static int      xpac = 2,  ypac = 1;     /* pacman position  */
static int      xghost = 15, yghost = 0;   /* ghost position   */
static uint8_t  gameInProgress = 1;
static uint8_t  empty          = 0;
static uint8_t  level         = 0;
static int      score         = 0;

static uint32_t lastPacMoveTime = 0;   /* last pacman move time */
static uint32_t lastGhostMoveTime = 0;   /* last ghost move time  */
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */

/* ════════════════════════════════════════════════════════════════
 * UART / input helpers
 * ════════════════════════════════════════════════════════════════ */
static uint8_t UART_ReadChar(char *c)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        *c = (char)(huart2.Instance->DR & 0xFF);
        return 1;
    }
    return 0;
}

/* Returns: 'L' 'R' 'U' 'D' or 0 */
static char readInput(void)
{
    /* Physical button → move down (mimics original btnSelect toggling row) */
    static uint8_t btn_prev = 0;
    uint8_t btn_curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t edge = (btn_curr && !btn_prev);
    btn_prev = btn_curr;
    if (edge) return 'D';

    char c = 0;
    if (!UART_ReadChar(&c)) return 0;

    /* Arrow key ESC sequence */
    if (c == 0x1B) {
        char c2 = 0, c3 = 0;
        uint32_t t = HAL_GetTick();
        while (!UART_ReadChar(&c2) && (HAL_GetTick()-t) < 5)  {}
        while (!UART_ReadChar(&c3) && (HAL_GetTick()-t) < 10) {}
        if (c2 == '[') {
            if (c3 == 'A') return 'U';
            if (c3 == 'B') return 'D';
            if (c3 == 'C') return 'R';
            if (c3 == 'D') return 'L';
        }
        return 0;
    }

    switch (c) {
        case 'w': case 'W': return 'U';
        case 's': case 'S': return 'D';
        case 'a': case 'A': return 'L';
        case 'd': case 'D': return 'R';
        default:            return 0;
    }
}

/* ════════════════════════════════════════════════════════════════
 * LCD build
 * ════════════════════════════════════════════════════════════════ */
static Lcd_HandleTypeDef lcd_build(void)
{
    static Lcd_PortType dp[4]   = { GPIOC, GPIOB, GPIOA, GPIOA };
    static Lcd_PinType  dpin[4] = { GPIO_PIN_7, GPIO_PIN_6,
                                    GPIO_PIN_7, GPIO_PIN_6 };
    return Lcd_create(dp, dpin,
                      GPIOB, GPIO_PIN_5,
                      GPIOB, GPIO_PIN_4,
                      LCD_4_BIT_MODE);
}

static void loadSprites(void)
{
    Lcd_define_char(&lcd, 0, bmp_pacman);
    Lcd_define_char(&lcd, 1, bmp_ghost);
    Lcd_define_char(&lcd, 2, bmp_point);
}

/* ════════════════════════════════════════════════════════════════
 * Game functions
 * ════════════════════════════════════════════════════════════════ */

/* Forward declarations */
static void won(void);
static void game_over(void);
static void initLevel(void);

/* ── move_pac: move pacman by (dx,dy) ──────────────────────────── */
static void move_pac(int dx, int dy)
{
    int oldx = xpac;
    int oldy = ypac;

    if ((xpac+dx) >= 0 && (xpac+dx) <= MAXX) xpac += dx;
    if ((ypac+dy) >= 0 && (ypac+dy) <= MAXY) ypac += dy;

    /* Draw pacman at new position */
    Lcd_cursor(&lcd, ypac, xpac);
    Lcd_write_char(&lcd, 0);   /* char 0 = pacman */

    /* Erase old position if moved */
    if (xpac != oldx || ypac != oldy) {
        Lcd_cursor(&lcd, oldy, oldx);
        Lcd_write_char(&lcd, 0x20);   /* space */
    }

    /* Eat dot */
    if (points[xpac][ypac]) {
        points[xpac][ypac] = 0;
        score++;
    }

    /* Check if all dots eaten */
    empty = 1;
    for (int i = 0; i <= MAXX; i++)
        for (int j = 0; j <= MAXY; j++)
            if (points[i][j]) { empty = 0; break; }

    if (empty && gameInProgress) won();
}

/* ── move_ghost: move ghost one step toward pacman ────────────── */
static void move_ghost(void)
{
    int oldx = xghost;
    int oldy = yghost;

    /* Simple AI: close Y gap first, then X */
    if      (yghost < ypac) yghost++;
    else if (yghost > ypac) yghost--;
    else if (xghost < xpac) xghost++;
    else if (xghost > xpac) xghost--;

    /* Draw ghost at new position */
    Lcd_cursor(&lcd, yghost, xghost);
    Lcd_write_char(&lcd, 1);   /* char 1 = ghost */

    /* Erase / restore old position */
    if (oldx != xghost || oldy != yghost) {
        Lcd_cursor(&lcd, oldy, oldx);
        if (points[oldx][oldy])
            Lcd_write_char(&lcd, 2);   /* restore dot  */
        else
            Lcd_write_char(&lcd, 0x20); /* empty space */
    }
}

/* ── game_over: game over screen then restart ───────────────────── */
static void game_over(void)
{
    gameInProgress = 0;
    Lcd_clear(&lcd);
    loadSprites();
    Lcd_cursor(&lcd, 0, 0); Lcd_string(&lcd, "***Game Over****");
    Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, "***Score: ");
    Lcd_int(&lcd, score);
    Lcd_string(&lcd, "***");
    HAL_Delay(2000);

    /* Software reset: reinitialise everything */
    xpac = 2;  ypac = 1;
    xghost = 15; yghost = 0;
    level = 0;
    score = 0;
    gameInProgress = 1;
    Lcd_clear(&lcd);
    loadSprites();
    initLevel();
}

/* ── won: level clear screen then next level ──────────────── */
static void won(void)
{
    level++;
    Lcd_clear(&lcd);
    loadSprites();
    Lcd_cursor(&lcd, 0, 0); Lcd_string(&lcd, "*** Next level **");
    Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, "***  Lvl ");
    Lcd_int(&lcd, level);
    Lcd_string(&lcd, "  ***");
    HAL_Delay(2000);
    Lcd_clear(&lcd);
    loadSprites();
    initLevel();
}

/* ── initLevel: fill dots, draw initial screen ──────────────── */
static void initLevel(void)
{
    for (int i = 0; i <= MAXX; i++) {
        for (int j = 0; j <= MAXY; j++) {
            points[i][j] = 1;
            Lcd_cursor(&lcd, j, i);
            Lcd_write_char(&lcd, 2);   /* dot */
        }
    }

    /* Draw pacman and ghost starting positions */
    Lcd_cursor(&lcd, ypac,  xpac);  Lcd_write_char(&lcd, 0);
    Lcd_cursor(&lcd, yghost, xghost); Lcd_write_char(&lcd, 1);

    lastGhostMoveTime = HAL_GetTick();
    lastPacMoveTime = HAL_GetTick();
    empty = 0;
}

/* USER CODE END 0 */

/* ════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    lcd = lcd_build();
    loadSprites();

    /* Splash screen */
    Lcd_cursor(&lcd, 0, 0);
    Lcd_string(&lcd, "   Pacman!  ");
    Lcd_cursor(&lcd, 1, 0);
    Lcd_string(&lcd, " WASD to move ");
    HAL_Delay(3000);
    Lcd_clear(&lcd);
    loadSprites();

    initLevel();

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* ── Pacman input (rate-limited) ───────────────────── */
        if (now - lastPacMoveTime > SPEED_PAC) {
            char inp = readInput();
            if (inp) {
                switch (inp) {
                    case 'L': move_pac(-1,  0); break;
                    case 'R': move_pac( 1,  0); break;
                    case 'U': move_pac( 0, -1); break;
                    case 'D': move_pac( 0,  1); break;
                }
                lastPacMoveTime = HAL_GetTick();
            }
        }

        /* ── Ghost movement (speed increases with level) ───── */
        uint32_t ghostInterval = (uint32_t)(SPEED_GHOST / (level + 1)) + 10;
        if (now - lastGhostMoveTime > ghostInterval) {
            move_ghost();
            lastGhostMoveTime = HAL_GetTick();
        }

        /* ── Collision detection ────────────────────────────── */
        if (xpac == xghost && ypac == yghost && gameInProgress)
            game_over();
    }
}

/* ════════════════════════════════════════════════════════════════
 * Peripheral init
 * ════════════════════════════════════════════════════════════════ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button, PULLDOWN */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6=D7, PA7=D6 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB4=EN, PB5=RS, PB6=D5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC7=D4 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 9600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
