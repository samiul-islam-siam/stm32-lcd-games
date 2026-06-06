/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Space Invaders for STM32
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
#define WIDTH        16
#define HEIGHT        4
#define ALIENS_NUM    8
#define GAME_STEP   100

#define SHIP          0
#define BULLET_UP     1
#define BULLET_DOWN   2
#define SHIP_BULLET   3
#define ALIEN1        4
#define ALIEN2        5
#define ALIEN1BULLET  6
#define ALIEN2BULLET  7

#define btnRIGHT  0
#define btnLEFT   1
#define btnSHOOT  2
#define btnPAUSE  3
#define btnNONE   4

#define STATE_START    0
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;

/* Sprites */
static uint8_t ship_sprite[]            = {0x00,0x00,0x00,0x00,0x00,0x04,0x0E,0x1B};
static uint8_t ship_bullet_sprite[]     = {0x00,0x04,0x04,0x00,0x00,0x04,0x0E,0x1B};
static uint8_t bullet_down_sprite[]     = {0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x00};
static uint8_t bullet_up_sprite[]       = {0x00,0x04,0x04,0x00,0x00,0x00,0x00,0x00};
static uint8_t alien1_1_sprite[]        = {0x0A,0x15,0x0E,0x11,0x00,0x00,0x00,0x00};
static uint8_t alien1_2_sprite[]        = {0x0A,0x15,0x0E,0x0A,0x00,0x00,0x00,0x00};
static uint8_t alien1_1_bullet_sprite[] = {0x0A,0x15,0x0E,0x11,0x00,0x04,0x04,0x00};
static uint8_t alien1_2_bullet_sprite[] = {0x0A,0x15,0x0E,0x0A,0x00,0x04,0x04,0x00};

/* Game object types */
typedef struct { int8_t x, y, speed; uint8_t active; } Bullet;
typedef struct { int8_t x, y; } Ship_t;
typedef struct { int8_t x, y, speed; uint8_t alive, state; } Alien;

/* Game state */
static Ship_t ship;
static Bullet shipBullet;
static Alien  aliens[ALIENS_NUM];
static Bullet alienBullets[ALIENS_NUM];

static uint8_t  animationStep   = 0;
static uint8_t  alienStep       = 5;
static uint8_t  fireProbability = 20;
static int      score           = 0;
static uint8_t  level           = 1;
static uint8_t  aliensLeft      = 0;
static uint8_t  gameState       = STATE_START;

static char screenBuffer[HEIGHT / 2][WIDTH + 1];
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ── UART handle ─────────────────────────────────────────────── */
UART_HandleTypeDef huart2;

/* ── LCG random ──────────────────────────────────────────────── */
static uint32_t rng_state = 73529;
static uint32_t rng(uint32_t max)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (rng_state >> 16) % max;
}

/* ── Non-blocking UART read ──────────────────────────────────── */
static uint8_t UART_ReadChar(char *c)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        *c = (char)(huart2.Instance->DR & 0xFF);
        return 1;
    }
    return 0;
}

/* ── Button: debounced rising-edge detect (PULLDOWN, active-HIGH) */
static uint8_t buttonJustPressed(void)
{
    static uint8_t prev = 0;
    uint8_t curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t edge = (curr && !prev);
    prev = curr;
    return edge;
}

/* ── Read all inputs: UART keyboard + hardware button ────────── */
static uint8_t readInput(void)
{
    /* Hardware button → shoot */
    if (buttonJustPressed())
        return btnSHOOT;

    /* UART keyboard */
    char c = 0;
    if (!UART_ReadChar(&c)) return btnNONE;

    /* Echo received character */
    HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, 100);

    /* Arrow keys: ESC [ C/D */
    if (c == 0x1B) {
        char c2 = 0, c3 = 0;
        uint32_t t = HAL_GetTick();
        while (!UART_ReadChar(&c2) && (HAL_GetTick() - t) < 5)  {}
        while (!UART_ReadChar(&c3) && (HAL_GetTick() - t) < 10) {}
        if (c2 == '[') {
            if (c3 == 'C') return btnRIGHT;
            if (c3 == 'D') return btnLEFT;
            if (c3 == 'A') return btnSHOOT;
        }
        return btnNONE;
    }

    switch (c) {
        case 'a': case ',': case 'A': return btnLEFT;
        case 'd': case '.': case 'D': return btnRIGHT;
        case ' ': case 'w': case 'W': return btnSHOOT;
        case 'p': case 'P':           return btnPAUSE;
        default:                      return btnNONE;
    }
}

/* ──────── LCD build ──────── */
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

/* ── Load all 8 custom characters into LCD CGRAM ────────────── */
static void loadSprites(void)
{
    Lcd_define_char(&lcd, SHIP,         ship_sprite);
    Lcd_define_char(&lcd, BULLET_UP,    bullet_up_sprite);
    Lcd_define_char(&lcd, BULLET_DOWN,  bullet_down_sprite);
    Lcd_define_char(&lcd, SHIP_BULLET,  ship_bullet_sprite);
    Lcd_define_char(&lcd, ALIEN1,       alien1_1_sprite);
    Lcd_define_char(&lcd, ALIEN2,       alien1_2_sprite);
    Lcd_define_char(&lcd, ALIEN1BULLET, alien1_1_bullet_sprite);
    Lcd_define_char(&lcd, ALIEN2BULLET, alien1_2_bullet_sprite);
}

/* ── updateScreen ────────────────────────────────────────────── */
static void updateScreen(void)
{
    uint8_t shipDisplayed = 0;

    /* Clear buffer */
    for (uint8_t i = 0; i < HEIGHT / 2; i++) {
        for (uint8_t j = 0; j < WIDTH; j++)
            screenBuffer[i][j] = ' ';
        screenBuffer[i][WIDTH] = '\0';
    }

    /* Ship bullet */
    if (shipBullet.active) {
        if ((ship.x == shipBullet.x) && (shipBullet.y == 2)) {
            screenBuffer[shipBullet.y / 2][shipBullet.x] = SHIP_BULLET;
            shipDisplayed = 1;
        } else {
            screenBuffer[shipBullet.y / 2][shipBullet.x] =
                (shipBullet.y % 2) ? BULLET_DOWN : BULLET_UP;
        }
    }

    /* Aliens */
    for (uint8_t i = 0; i < ALIENS_NUM; i++) {
        if (aliens[i].alive)
            screenBuffer[aliens[i].y / 2][aliens[i].x] =
                aliens[i].state ? ALIEN1 : ALIEN2;
    }

    /* Alien bullets */
    for (uint8_t i = 0; i < ALIENS_NUM; i++) {
        if (!alienBullets[i].active) continue;
        uint8_t bulletDisplayed = 0;
        for (uint8_t j = 0; j < ALIENS_NUM; j++) {
            if ((aliens[j].x == alienBullets[i].x) &&
                (alienBullets[i].y == 1) &&
                (aliens[i].alive)) {
                screenBuffer[alienBullets[i].y / 2][alienBullets[i].x] =
                    aliens[i].state ? ALIEN1BULLET : ALIEN2BULLET;
                bulletDisplayed = 1;
            }
        }
        if (!bulletDisplayed) {
            if ((ship.x == alienBullets[i].x) && (alienBullets[i].y == 2)) {
                screenBuffer[alienBullets[i].y / 2][alienBullets[i].x] = SHIP_BULLET;
                shipDisplayed = 1;
            } else {
                screenBuffer[alienBullets[i].y / 2][alienBullets[i].x] =
                    (alienBullets[i].y % 2) ? BULLET_DOWN : BULLET_UP;
            }
        }
    }

    /* Flush buffer to LCD */
    for (uint8_t i = 0; i < HEIGHT / 2; i++) {
        Lcd_cursor(&lcd, i, 0);
        Lcd_string(&lcd, screenBuffer[i]);
    }

    /* Ship char is code 0 – must write separately */
    if (!shipDisplayed) {
        Lcd_cursor(&lcd, ship.y / 2, ship.x);
        Lcd_write_char(&lcd, SHIP);
    }
}

/* ── initLevel ───────────────────────────────────────────────── */
static void initLevel(uint8_t l)
{
    level = l;
    if (level > 42) level = 42;

    ship.x = WIDTH / 2;
    ship.y = 3;

    shipBullet.x      = WIDTH / 2;
    shipBullet.y      = 3;
    shipBullet.active = 0;

    for (uint8_t i = 0; i < ALIENS_NUM; i++) {
        aliens[i].x     = i;
        aliens[i].y     = 0;
        aliens[i].speed = 1;
        aliens[i].alive = 1;
        aliens[i].state = 0;
        alienBullets[i].active = 0;
    }

    animationStep   = 0;
    alienStep       = 6 - level / 2;
    if (alienStep < 1) alienStep = 1;
    fireProbability = 110 - level * 10;
    if (fireProbability < 10) fireProbability = 10;
    aliensLeft = ALIENS_NUM;

    /* "Level N" splash for 1 second */
    loadSprites();          /* reload CGRAM after Lcd_clear below */
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 4);
    Lcd_string(&lcd, "LEVEL ");
    Lcd_int(&lcd, level);
    Lcd_cursor(&lcd, 1, 3);
    Lcd_string(&lcd, "Score: ");
    Lcd_int(&lcd, score);
    HAL_Delay(1500);
    Lcd_clear(&lcd);
    loadSprites();          /* CGRAM wiped by CLEAR_DISPLAY, reload */
}

/* ── showGameOver ────────────────────────────────────────────── */
static void showGameOver(void)
{
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 3);
    Lcd_string(&lcd, "GAME  OVER");
    Lcd_cursor(&lcd, 1, 1);
    Lcd_string(&lcd, "Score:");
    Lcd_cursor(&lcd, 1, 8);
    Lcd_int(&lcd, score);
    Lcd_cursor(&lcd, 1, 12);
    Lcd_string(&lcd, "BTN>");
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    lcd = lcd_build();
    loadSprites();

    /* ── Splash screen ── */
    Lcd_cursor(&lcd, 0, 1);
    Lcd_string(&lcd, "SPACE INVADERS");
    Lcd_cursor(&lcd, 1, 3);
    Lcd_string(&lcd, "Press Button");
    HAL_Delay(100);             /* let LCD settle before button poll */
    buttonJustPressed();        /* flush any startup glitch */

    gameState = STATE_START;
    score     = 0;

    while (1)
    {
        uint8_t input = readInput();

        /* ══════════════════════════════════════════════════════
         * STATE: START – static splash, wait for button
         * ══════════════════════════════════════════════════════ */
        if (gameState == STATE_START)
        {
            if (input == btnSHOOT)
            {
                score = 0;
                level = 1;
                initLevel(1);
                gameState = STATE_PLAYING;
            }
            HAL_Delay(20);
            continue;
        }

        /* ══════════════════════════════════════════════════════
         * STATE: GAME OVER – static screen, wait for button
         * ══════════════════════════════════════════════════════ */
        if (gameState == STATE_GAMEOVER)
        {
            if (input == btnSHOOT)
            {
                score = 0;
                level = 1;
                initLevel(1);
                loadSprites();
                gameState = STATE_PLAYING;
            }
            HAL_Delay(20);
            continue;
        }

        /* ══════════════════════════════════════════════════════
         * STATE: PLAYING
         * ══════════════════════════════════════════════════════ */
        switch (input)
        {
            case btnRIGHT:
                if (ship.x < WIDTH - 1) ship.x++;
                break;
            case btnLEFT:
                if (ship.x > 0) ship.x--;
                break;
            case btnPAUSE:
                /* Wait for 'p' or button press to resume */
                Lcd_cursor(&lcd, 0, 0);
                Lcd_string(&lcd, "   -- PAUSE --  ");
                while (1) {
                    char c = 0;
                    if (UART_ReadChar(&c) && (c == 'p' || c == 'P')) break;
                    /* button also resumes */
                    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
                        HAL_Delay(200);   /* debounce */
                        break;
                    }
                    HAL_Delay(50);
                }
                Lcd_clear(&lcd);
                loadSprites();  /* CGRAM lost after clear */
                break;
            case btnSHOOT:
                if (!shipBullet.active) {
                    shipBullet.x      = ship.x;
                    shipBullet.y      = ship.y;
                    shipBullet.speed  = -1;
                    shipBullet.active = 1;
                }
                break;
            default:
                break;
        }

        /* Move ship bullet */
        if (shipBullet.active) {
            shipBullet.y += shipBullet.speed;
            if (shipBullet.y < 0 || shipBullet.y >= HEIGHT)
                shipBullet.active = 0;
        }

        /* Aliens + alien bullets */
        for (uint8_t i = 0; i < ALIENS_NUM; i++) {

            /* Alien bullet movement */
            if (alienBullets[i].active) {
                alienBullets[i].y += alienBullets[i].speed;
                if (alienBullets[i].y < 0 || alienBullets[i].y >= HEIGHT)
                    alienBullets[i].active = 0;

                /* Bullet hits ship → game over */
                if (alienBullets[i].active &&
                    alienBullets[i].x == ship.x &&
                    alienBullets[i].y == ship.y)
                {
                    HAL_Delay(200);
                    buttonJustPressed();    /* flush so death press doesn't restart */
                    showGameOver();
                    gameState = STATE_GAMEOVER;
                    goto next_frame;        /* skip rest of game logic this tick */
                }
            }

            /* Alien movement every alienStep frames */
            if (!(animationStep % alienStep)) {
                aliens[i].x    += aliens[i].speed;
                aliens[i].state = !aliens[i].state;
                if (aliens[i].x < 0 || aliens[i].x >= WIDTH)
                    aliens[i].x -= aliens[i].speed;
            }

            /* Alien hit by ship bullet */
            if (aliens[i].alive &&
                shipBullet.active &&
                aliens[i].x == shipBullet.x &&
                aliens[i].y == shipBullet.y)
            {
                aliens[i].alive   = 0;
                shipBullet.active = 0;
                score += 10 * level;
                aliensLeft--;
            }

            /* Alien fires randomly */
            if (aliens[i].alive &&
                !alienBullets[i].active &&
                rng(fireProbability) == 0)
            {
                alienBullets[i].x      = aliens[i].x;
                alienBullets[i].y      = aliens[i].y + 1;
                alienBullets[i].speed  = 1;
                alienBullets[i].active = 1;
            }
        }

        /* Reverse alien direction at edges */
        if (!(animationStep % alienStep)) {
            if (aliens[0].x == 0 || aliens[ALIENS_NUM-1].x == WIDTH - 1) {
                for (uint8_t i = 0; i < ALIENS_NUM; i++)
                    aliens[i].speed = -aliens[i].speed;
            }
        }

        updateScreen();
        animationStep++;
        HAL_Delay(GAME_STEP);

        /* All aliens cleared → next level */
        if (!aliensLeft) {
            initLevel(level + 1);
            loadSprites();
        }

        next_frame:;
    }
}

/* ── SystemClock_Config ──────────────────────────────────────── */
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

/* ── MX_GPIO_Init – identical to working dino project ───────── */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Output reset levels */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button, PULLDOWN, polled (active-HIGH) */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6=D7, PA7=D6 – LCD data outputs */
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

    /* PC7 – LCD D4 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ── MX_USART2_UART_Init – PA2=TX PA3=RX 9600 baud ──────────── */
static void MX_USART2_UART_Init(void)
{
    /* Enable clocks */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2=TX, PA3=RX as AF7 */
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
