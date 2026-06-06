# Space Invaders Game 👾

A simple **Space Invaders game** developed for the **STM32** microcontroller using the **16×2 LCD (4-bit mode)**.

The player controls a spaceship, destroys incoming aliens, advances through levels, and avoids enemy bullets. 
The game supports both a physical push button and UART keyboard controls.

## Features

- 16×2 LCD graphics using custom CGRAM characters
- Multiple game levels with increasing difficulty
- Alien movement and random enemy firing
- Score tracking
- Pause/Resume functionality
- UART keyboard controls
- Physical button support
- Game Over and restart system

## How to Play

1. Press the **hardware button (PA0)** to start the game.
2. Move the spaceship using:
    - **A / ←** : Move Left
    - **D / →** : Move Right
3. Shoot aliens using:
    - **W / Space / ↑**
    - **Push Button**
4. Destroy all aliens to advance to the next level.
5. Avoid enemy bullets—getting hit results in **Game Over**.
6. Press **P** to pause/resume the game.
7. After Game Over, press the **Push button** to restart.

### Scoring

- Each alien destroyed awards **10 × Current Level** points.
- Higher levels increase the game's difficulty and score rewards.


## Hardware Requirements

- STM32 Nucleo board
- 16×2 LCD (HD44780 compatible)
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

> All GPIO pins are configured as output mode.

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

### UART Connections

USART2 is used for keyboard control (9600 8N1).

| USART2 Signal | STM32 Pin |
|---------------|-----------|
| TX            | PA2       |
| RX            | PA3       |

Parameters:

```text
Baud Rate      : 9600
Word Length    : 8 Bits
Parity         : None
Stop Bits      : 1
Hardware Flow  : None
```

### Keyboard Controls

| Key       | Action         |
|-----------|----------------|
| A/←       | Move Left      |
| D/→       | Move Right     |
| W/Space/↑ | Shoot          |
| P         | Pause / Resume |

### Physical Button

| Button            | Action                   |
|-------------------|--------------------------|
| Push Button (PA0) | Start / Shoot / Restart  |
