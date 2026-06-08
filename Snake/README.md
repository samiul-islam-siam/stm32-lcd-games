# Snake Game 🐍
A classic Snake game running on a 16x2 LCD, built with an STM32 Nucleo-F446RE board using STM32CubeMX and STM32CubeIDE.

## How to Play

- Select a difficulty level before starting
- Control the snake using UART keyboard input
- Collect food to grow the snake and increase your score
- Avoid walls and your own body
- The snake wraps around the screen edges
- Game speed increases as the snake grows
- Press the button or Enter key to start and restart the game
<!--
## Features

- Classic Snake gameplay on a 16x2 LCD
- Three selectable difficulty levels
- Progressive speed increase
- Snake growth system
- Screen wrap-around mechanics
- Wall obstacles on higher levels
- UART keyboard controls
- Animated title screen
- Real-time LCD pixel rendering using custom characters
-->
## Hardware

- STM32 Nucleo-F446RE board
- LCD 1602 (HD44780 compatible, 16x2)
- Push button (4 pin)
- 10kΩ potentiometer (LCD contrast)
- 220Ω resistor (LCD backlight)
- Breadboard and jumper wires

## Circuit Connections

### LCD 1602

| LCD Pin | LCD Signal | STM32 Pin                  |
|----------|------------|----------------------------|
| 1         | VSS        | GND                        |
| 2         | VDD        | 5V                         |
| 3         | VO         | Potentiometer middle pin   |
| 4         | RS         | PB5                        |
| 5         | RW         | GND                        |
| 6         | EN         | PB4                        |
| 7–10      | D0–D3      | Not connected (4-bit mode) |
| 11        | D4         | PC7                        |
| 12        | D5         | PB6                        |
| 13        | D6         | PA7                        |
| 14        | D7         | PA6                        |
| 15        | A (BL+)    | 3.3V                       |
| 16        | K (BL−)    | GND                        |

> All LCD GPIO pins are configured as output mode.

> Connect VO directly to GND if a potentiometer is unavailable.

### Push Button

| Button Pin | Connect To |
|------------|------------|
| One side   | PA0        |
| Other side | 3.3V       |

> PA0 is configured as input with internal pull-down.

> Used as Select / Confirm.

### UART Control

| STM32 Pin | Function |
|------------|----------|
| PA2 | USART2 TX |
| PA3 | USART2 RX |

### UART Settings

- Baud Rate: 9600
- Data Bits: 8
- Stop Bits: 1
- Parity: None

## Controls

| Action | Key |
|---------|-----|
| Move Up | W / ↑ |
| Move Down | S / ↓ |
| Move Left | A / ← |
| Move Right | D / → |
| Select / Start | Enter / E |
| Select / Start | Push Button |
| Increase Level | + / > |
| Decrease Level | - / < |

## Game Screens
<!--
### Start Screen

Displays:

```text
Select level: X
    🐍🐍🐍🐍
```

- Choose a level using `+` and `-`.
- Press Enter, E, or the hardware button to start.
-->
### Level Splash Screen

Displays:

```text
    LEVEL X
 Use WASD keys
```

before gameplay begins.

### Gameplay

- Navigate the snake around the field.
- Collect food to increase score and length.
- Avoid walls and self-collision.
- Snake wraps around screen boundaries.
- Speed increases as food is collected.

### Game Over Screen

Displays:

```text
Game Over!
Score: XX BTN>
```

Press the button or Enter key to return to the level selection screen.

## Difficulty Levels

### Level 1

- Mostly open field
- Corner wall pillars
- Beginner friendly

### Level 2

- Additional obstacle pillars
- Moderate difficulty

### Level 3

- Scattered wall layout
- Requires careful navigation

## Game Mechanics

### Snake Growth

- Snake starts with 3 segments.
- Grows when food is collected.
- Maximum automatic growth occurs during early gameplay.

### Speed Progression

- Initial speed: 8 moves per second
- Speed increases after collecting food
- Maximum speed capped automatically

### Screen Wrap

When reaching an edge:

- Left → Right
- Right → Left
- Top → Bottom
- Bottom → Top

### Collision Detection

The game ends when:

- Snake hits a wall
- Snake collides with its own body

## Rendering System

Unlike traditional LCD games, this project creates a virtual:

```text
16 × 80 pixel grid
```

which is dynamically mapped onto the:

```text
16 × 2 character LCD
```

using custom HD44780 CGRAM characters, allowing smooth snake movement and pixel-level gameplay.

## Difficulty Tuning

Edit these values in `main.c`:

| Variable | Default | Effect |
|-----------|---------|---------|
| `gameSpeed` | 8 | Initial snake speed |
| `gameSpeed += 3` | Food collection | Speed increase rate |
| `selectedLevel` | 1 | Starting level |
| `levelz[][][]` | Built-in layouts | Wall configuration |
<!--
## Development Environment

- STM32CubeMX
- STM32CubeIDE
- STM32 HAL Drivers
- STM32 Nucleo-F446RE

## Project Structure

```text
Core/
├── Inc/
│   ├── main.h
│   └── lcd.h
│
├── Src/
│   ├── main.c
│   └── lcd.c
│
└── Drivers/
```

## Gameplay Preview

```text
████     ●

  ████
     █
```

Collect food, grow the snake, avoid obstacles, and achieve the highest score possible!
-->
