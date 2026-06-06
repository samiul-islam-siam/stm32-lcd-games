# Pacman Game 

A lightweight **Pacman game** built for the **STM32** using the STM32 HAL library and a 16×2 LCD in 4-bit mode. 
Control Pacman, collect all dots, avoid the ghost, and progress through increasingly difficult levels.

## Features

- 16×2 LCD graphics using custom characters
- UART keyboard controls (WASD and arrow keys)
- Physical button support
- Multiple levels with increasing ghost speed
- Score tracking
- Game Over and level progression screens


## Hardware Requirements

- STM32 Nucleo board
- 16×2 HD44780-compatible LCD
- Push button
- Serial terminal via ST-Link Virtual COM Port
- Jumper wires

## Connections & Controls
### LCD 1602

| LCD Pin | LCD Signal | STM32 Pin                  |
|---------|------------|----------------------------|
| 1       | VSS        | GND                        |
| 2       | VDD        | 5V                         |
| 3       | VO         | Potentiometer middle pin   |
| 4       | RS         | PB5 (D4)                   |
| 5       | RW         | GND                        |
| 6       | EN         | PB4 (D5)                   |
| 7–10    | D0–D3      | Not connected (4-bit mode) |
| 11      | D4         | PC7 (D9)                   |
| 12      | D5         | PB6 (D10)                  |
| 13      | D6         | PA7 (D11)                  |
| 14      | D7         | PA6 (D12)                  |
| 15      | A (BL+)    | 3.3V                       |
| 16      | K (BL−)    | GND                        |

> Connect VO to GND if potentiometer is not available. You can also add resistors instead of potentiometer.

### Push Button

| Button Pin | Connect To |
|------------|------------|
| One side   | PA0 (A0)   |
| Other side | 3.3V       |

> PA0 is configured as input with internal pull-down. Button reads HIGH when pressed.

### Potentiometer (Contrast)

| Pot Pin | Connect To     |
|---------|----------------|
| Left    | GND            |
| Middle  | LCD pin 3 (VO) |
| Right   | 3.3V           |

## STM32CubeMX Setup

### GPIO

```text
PA0  -> GPIO_Input (Pull-Down)
PA6  -> GPIO_Output
PA7  -> GPIO_Output

PB4  -> GPIO_Output
PB5  -> GPIO_Output
PB6  -> GPIO_Output

PC7  -> GPIO_Output
```

### USART2

```text
Baud Rate      : 9600
Word Length    : 8 Bits
Parity         : None
Stop Bits      : 1
Hardware Flow  : None
```

## Controls

| Key        | Action           |
|------------|------------------|
| W / ↑      | Move Up          |
| S / ↓      | Move Down        |
| A / ←      | Move Left        |
| D / →      | Move Right       |
| PA0 Button | Move Down/Select |


## How to Play

1. Move Pacman around the LCD using the keyboard controls.
2. Collect all dots on the screen to clear the level.
3. Avoid the ghost that continuously chases Pacman.
4. Each completed level increases the ghost's speed.
5. If the ghost catches Pacman, the game ends and restarts.
6. Try to achieve the highest score by clearing as many levels as possible.

### Scoring

```text
+1 point for every dot collected
```