/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Snake Game for STM32
  *
  * LCD 16x2, 4-bit parallel:
  *   RS → PB5,  EN → PB4
  *   D4 → PC7,  D5 → PB6,  D6 → PA7,  D7 → PA6
  *
  * Button → PA0 (PULLDOWN, active-HIGH) – used as SELECT/confirm
  *
  * UART2 keyboard control (PA2=TX, PA3=RX, 9600 baud):
  *   'd' / Right arrow = move right
  *   'a' / Left arrow  = move left
  *   'w' / Up arrow    = move up
  *   's' / Down arrow  = move down
  *   Enter / 'e'       = select / confirm
  *   '+' / '>'         = level up   (on start screen)
  *   '-' / '<'         = level down (on start screen)
  *   Physical button   = SELECT
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define GRID_ROWS    16
#define GRID_COLS    80
#define LCD_ROWS      2
#define LCD_COLS     16

#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_RIGHT 2
#define DIR_LEFT  3

#define STATE_START    0
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;
UART_HandleTypeDef huart2;

/* ── Snake custom character animations (title screen) ────────── */
static uint8_t mySnake[8][8] = {
    {0x00,0x00,0x03,0x06,0x0C,0x18,0x00,0x00},
    {0x00,0x18,0x1E,0x03,0x01,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x1F,0x0E,0x00},
    {0x00,0x00,0x03,0x0F,0x18,0x00,0x00,0x00},
    {0x00,0x1C,0x1F,0x01,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x18,0x0D,0x07,0x00,0x00},
    {0x00,0x00,0x0E,0x1B,0x1F,0x0E,0x00,0x00},
    {0x00,0x00,0x00,0x08,0x10,0x08,0x00,0x00},
};

/* ── Level wall definitions [level][row][col] ─────────────────── */
/* level 0 = no walls, 1..3 = progressively harder               */
static const uint8_t levelz[4][2][16] = {
    /* level 0 – open field */
    {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
    /* level 1 – corner pillars */
    {{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
     {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
    /* level 2 – more pillars */
    {{1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
     {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1}},
    /* level 3 – scattered */
    {{1,0,1,0,0,0,0,0,0,1,0,0,0,1,0,0},
     {0,0,0,0,1,0,0,1,0,0,0,1,0,0,0,1}},
};

/* ── Game grid: 16 pixel-rows × 80 pixel-cols ────────────────── */
static uint8_t grid[GRID_ROWS][GRID_COLS];   /* 0=empty 1=snake 2=food */

/* ── Snake linked list ───────────────────────────────────────── */
typedef struct Part {
    int16_t row, col;
    uint8_t dir;
    struct Part *next;
} Part;

static Part *head = NULL;
static Part *tail = NULL;

/* ── Game state variables ────────────────────────────────────── */
static uint8_t  gameState    = STATE_START;
static uint8_t  selectedLevel = 1;
static uint8_t  levels        = 4;         /* 1..3, index 0 unused */
static int      collected     = 0;
static int      gameSpeed     = 8;         /* moves per second */
static int16_t  foodRow, foodCol;          /* current food position */
static uint32_t lastMoveTime  = 0;

/* ── Null char for clearing CGRAM slots ──────────────────────── */
static uint8_t nullChar[8] = {0,0,0,0,0,0,0,0};
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */

/* ════════════════════════════════════════════════════════════════
 * UART helpers
 * ════════════════════════════════════════════════════════════════ */
static uint8_t UART_ReadChar(char *c)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        *c = (char)(huart2.Instance->DR & 0xFF);
        return 1;
    }
    return 0;
}

/* ── Button: rising-edge detect (PULLDOWN, active-HIGH) ──────── */
static uint8_t buttonJustPressed(void)
{
    static uint8_t prev = 0;
    uint8_t curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t edge = (curr && !prev);
    prev = curr;
    return edge;
}

/* ── Read keyboard input ─────────────────────────────────────── */
/* Returns: 'L'=left 'R'=right 'U'=up 'D'=down 'S'=select '+'=lvlup '-'=lvldown 0=none */
static char readInput(void)
{
    /* Hardware button = select */
    if (buttonJustPressed()) return 'S';

    char c = 0;
    if (!UART_ReadChar(&c)) return 0;

    /* Arrow key ESC sequence */
    if (c == 0x1B) {
        char c2 = 0, c3 = 0;
        uint32_t t = HAL_GetTick();
        while (!UART_ReadChar(&c2) && (HAL_GetTick() - t) < 5)  {}
        while (!UART_ReadChar(&c3) && (HAL_GetTick() - t) < 10) {}
        if (c2 == '[') {
            if (c3 == 'A') return 'U';   /* up    */
            if (c3 == 'B') return 'D';   /* down  */
            if (c3 == 'C') return 'R';   /* right */
            if (c3 == 'D') return 'L';   /* left  */
        }
        return 0;
    }

    switch (c) {
        case 'w': case 'W': return 'U';
        case 's': case 'S': return 'D';
        case 'a': case 'A': return 'L';
        case 'd': case 'D': return 'R';
        case 'e': case 'E':
        case '\r': case '\n': return 'S';   /* Enter = select */
        case '+': case '>':   return '+';
        case '-': case '<':   return '-';
        default: return 0;
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

/* ════════════════════════════════════════════════════════════════
 * drawMatrix
 *
 * The 16×80 boolean grid maps to the 16×2 LCD like this:
 *   LCD row 0 covers pixel rows  0..7  (grid rows 0-7)
 *   LCD row 1 covers pixel rows  8..15 (grid rows 8-15)
 *   LCD col c covers pixel cols c*5 .. c*5+4
 *
 * Each LCD cell = one 5×8 custom char built from the grid bits.
 * Only 8 CGRAM slots available, so we render at most 8 unique
 * non-empty cells per frame and write them into slots 0-7.
 * Empty cells get the built-in space (0x20) or wall (0xFF).
 * ════════════════════════════════════════════════════════════════ */
static void drawMatrix(void)
{
    uint8_t cc = 0;   /* CGRAM slot counter, wraps at 8 */

    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        for (uint8_t c = 0; c < LCD_COLS; c++) {

            /* Build 5-wide × 8-tall bitmap for this LCD cell */
            uint8_t myChar[8];
            uint8_t special = 0;

            for (uint8_t row = 0; row < 8; row++) {
                uint8_t b = 0;
                uint8_t gr = r * 8 + row;          /* grid pixel row  */
                uint8_t gc0 = c * 5;               /* grid pixel col start */

                if (grid[gr][gc0+0]) { b |= 0x10; special = 1; }
                if (grid[gr][gc0+1]) { b |= 0x08; special = 1; }
                if (grid[gr][gc0+2]) { b |= 0x04; special = 1; }
                if (grid[gr][gc0+3]) { b |= 0x02; special = 1; }
                if (grid[gr][gc0+4]) { b |= 0x01; special = 1; }
                myChar[row] = b;
            }

            if (special) {
                /* Write to next CGRAM slot (cycle 0-7) */
                Lcd_define_char(&lcd, cc & 0x07, myChar);
                Lcd_cursor(&lcd, r, c);
                Lcd_write_char(&lcd, cc & 0x07);
                cc++;
                /* If we've used all 8 slots, reset counter –
                   earlier cells may flicker but game remains playable */
                if (cc >= 8) cc = 0;
            } else {
                Lcd_cursor(&lcd, r, c);
                if (levelz[selectedLevel][r][c])
                    Lcd_write_char(&lcd, 0xFF);   /* filled block = wall */
                else
                    Lcd_write_char(&lcd, 0x20);   /* space = empty */
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * Snake list management
 * ════════════════════════════════════════════════════════════════ */
static void freeList(void)
{
    Part *p = tail;
    while (p != NULL) {
        Part *q = p;
        p = p->next;
        free(q);
    }
    head = tail = NULL;
}

static void growSnake(void)
{
    Part *p = (Part*)malloc(sizeof(Part));
    if (!p) return;
    p->row  = tail->row;
    p->col  = tail->col;
    p->dir  = tail->dir;
    p->next = tail;
    tail    = p;
}

static void createSnake(int n)
{
    /* Clear grid */
    memset(grid, 0, sizeof(grid));

    freeList();

    tail       = (Part*)malloc(sizeof(Part));
    tail->row  = 7;
    tail->col  = 39 + n / 2;
    tail->dir  = DIR_LEFT;
    tail->next = NULL;
    grid[tail->row][tail->col] = 1;

    Part *q = tail;
    for (int i = 0; i < n - 1; i++) {
        Part *p   = (Part*)malloc(sizeof(Part));
        p->row    = q->row;
        p->col    = q->col - 1;
        p->dir    = q->dir;
        p->next   = NULL;
        grid[p->row][p->col] = 1;
        q->next   = p;
        q         = p;
    }
    head = q;
}

/* ════════════════════════════════════════════════════════════════
 * Food placement
 * ════════════════════════════════════════════════════════════════ */
static void newPoint(void)
{
    uint8_t ok = 0;
    while (!ok) {
        foodRow = (int16_t)(HAL_GetTick() * 1664525u + 1013904223u) % GRID_ROWS;
        if (foodRow < 0) foodRow += GRID_ROWS;
        foodCol = (int16_t)((HAL_GetTick() * 22695477u + 1u) >> 4) % GRID_COLS;
        if (foodCol < 0) foodCol += GRID_COLS;

        /* reject if on wall lcd-cell */
        if (levelz[selectedLevel][foodRow / 8][foodCol / 5]) continue;
        /* reject if on snake */
        if (grid[foodRow][foodCol] == 1) continue;
        ok = 1;
    }
    grid[foodRow][foodCol] = 2;   /* mark food */

    if (collected < 13) growSnake();
}

/* ════════════════════════════════════════════════════════════════
 * Movement
 * ════════════════════════════════════════════════════════════════ */
static uint8_t moveAll(void)
{
    /* Erase tail tip from grid */
    Part *p = tail;
    grid[p->row][p->col] = 0;

    /* Slide all segments forward */
    while (p->next != NULL) {
        p->row = p->next->row;
        p->col = p->next->col;
        p->dir = p->next->dir;
        p = p->next;
    }
    /* p == head now, advance it */
    switch (head->dir) {
        case DIR_UP:    head->row--; break;
        case DIR_DOWN:  head->row++; break;
        case DIR_RIGHT: head->col++; break;
        case DIR_LEFT:  head->col--; break;
    }
    /* Wrap around */
    if (head->col >= GRID_COLS) head->col = 0;
    if (head->col < 0)          head->col = GRID_COLS - 1;
    if (head->row >= GRID_ROWS) head->row = 0;
    if (head->row < 0)          head->row = GRID_ROWS - 1;

    /* Wall collision */
    if (levelz[selectedLevel][head->row / 8][head->col / 5]) return 1;

    /* Self collision */
    if (grid[head->row][head->col] == 1) return 1;

    /* Food pickup */
    if (grid[head->row][head->col] == 2) {
        collected++;
        if (gameSpeed < 25) gameSpeed += 3;
        grid[head->row][head->col] = 0;   /* clear food marker */
        newPoint();                       /* place new food, grow snake */
    }

    grid[head->row][head->col] = 1;   /* mark head */
    return 0;   /* no collision */
}

/* ════════════════════════════════════════════════════════════════
 * Screen helpers
 * ════════════════════════════════════════════════════════════════ */
static void showStartScreen(void)
{
    /* Clear CGRAM of any game chars, load snake animation */
    for (uint8_t i = 0; i < 8; i++)
        Lcd_define_char(&lcd, i, mySnake[i]);

    Lcd_clear(&lcd);

    /* Reload after clear (some controllers wipe CGRAM) */
    for (uint8_t i = 0; i < 8; i++)
        Lcd_define_char(&lcd, i, mySnake[i]);

    Lcd_cursor(&lcd, 0, 0);
    Lcd_string(&lcd, "Select level: ");
    Lcd_int(&lcd, selectedLevel);

    /* Draw snake sprite across bottom row */
    for (uint8_t i = 0; i < 8; i++) {
        Lcd_cursor(&lcd, 1, i + 4);
        Lcd_write_char(&lcd, i);
    }
}

static void showLevelSplash(void)
{
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 4);
    Lcd_string(&lcd, "LEVEL ");
    Lcd_int(&lcd, selectedLevel);
    Lcd_cursor(&lcd, 1, 3);
    Lcd_string(&lcd, "Use WASD keys");
    HAL_Delay(1500);
    Lcd_clear(&lcd);
}

static void showGameOver(void)
{
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 3);
    Lcd_string(&lcd, "Game Over!");
    Lcd_cursor(&lcd, 1, 2);
    Lcd_string(&lcd, "Score: ");
    Lcd_int(&lcd, collected);
    Lcd_cursor(&lcd, 1, 12);
    Lcd_string(&lcd, "BTN>");
}

/* ════════════════════════════════════════════════════════════════
 * Game initialisation
 * ════════════════════════════════════════════════════════════════ */
static void startGame(void)
{
    collected  = 0;
    gameSpeed  = 8;
    createSnake(3);
    newPoint();
    showLevelSplash();
    lastMoveTime = HAL_GetTick();
    gameState    = STATE_PLAYING;
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

    /* Seed LCG with SysTick */
    srand(HAL_GetTick());

    /* Show start screen */
    selectedLevel = 1;
    showStartScreen();
    buttonJustPressed();   /* flush startup glitch */

    while (1)
    {
        char inp = readInput();

        /* ══════════════════════════════════════════════════════
         * STATE: START – level select screen
         * ══════════════════════════════════════════════════════ */
        if (gameState == STATE_START)
        {
            if (inp == '+' && selectedLevel < (levels - 1)) {
                selectedLevel++;
                Lcd_cursor(&lcd, 0, 14);
                Lcd_int(&lcd, selectedLevel);
            }
            if (inp == '-' && selectedLevel > 1) {
                selectedLevel--;
                Lcd_cursor(&lcd, 0, 14);
                /* Erase old digit cleanly */
                Lcd_string(&lcd, "  ");
                Lcd_cursor(&lcd, 0, 14);
                Lcd_int(&lcd, selectedLevel);
            }
            if (inp == 'S') {
                /* Flush edge-detect so start press doesn't count as move */
                buttonJustPressed();
                startGame();
            }
            HAL_Delay(20);
            continue;
        }

        /* ══════════════════════════════════════════════════════
         * STATE: GAME OVER – wait for button to restart
         * ══════════════════════════════════════════════════════ */
        if (gameState == STATE_GAMEOVER)
        {
            if (inp == 'S') {
                freeList();
                selectedLevel = 1;
                showStartScreen();
                buttonJustPressed();
                gameState = STATE_START;
            }
            HAL_Delay(20);
            continue;
        }

        /* ══════════════════════════════════════════════════════
         * STATE: PLAYING
         * ══════════════════════════════════════════════════════ */

        /* Direction input – prevent 180° reversal */
        uint8_t moved = 0;
        if (inp == 'R' && head->dir != DIR_LEFT)  { head->dir = DIR_RIGHT; moved = 1; }
        if (inp == 'L' && head->dir != DIR_RIGHT) { head->dir = DIR_LEFT;  moved = 1; }
        if (inp == 'U' && head->dir != DIR_DOWN)  { head->dir = DIR_UP;    moved = 1; }
        if (inp == 'D' && head->dir != DIR_UP)    { head->dir = DIR_DOWN;  moved = 1; }

        /* Immediate step on direction change (responsive feel) */
        if (moved) {
            if (moveAll()) {
                HAL_Delay(200);
                showGameOver();
                buttonJustPressed();
                gameState = STATE_GAMEOVER;
                continue;
            }
            drawMatrix();
            lastMoveTime = HAL_GetTick();
        }

        /* Timed automatic step */
        uint32_t interval = 1000u / (uint32_t)gameSpeed;
        if (!moved && (HAL_GetTick() - lastMoveTime) >= interval) {
            lastMoveTime = HAL_GetTick();
            if (moveAll()) {
                HAL_Delay(200);
                showGameOver();
                buttonJustPressed();
                gameState = STATE_GAMEOVER;
                continue;
            }
            drawMatrix();
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * SystemClock_Config – HSI 16 MHz, no PLL
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

/* ════════════════════════════════════════════════════════════════
 * MX_GPIO_Init
 * ════════════════════════════════════════════════════════════════ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button, PULLDOWN, polled */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6=D7, PA7=D6 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB4=EN, PB5=RS, PB6=D5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC7 – D4 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ════════════════════════════════════════════════════════════════
 * MX_USART2_UART_Init – PA2=TX PA3=RX 9600 baud
 * ════════════════════════════════════════════════════════════════ */
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
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
