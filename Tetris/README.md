# Tetris 🧩

A miniature implementation of the classic **Tetris** game running on an **STM32 Nucleo** board using a **16×2 LCD (LCD1602A)**. 



## Hardware Requirements

- STM32 Nucleo board (STM32F4 series)
- LCD1602A (HD44780 compatible)
- USB-UART terminal (PuTTY, Tera Term, etc.)



## LCD Connections
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

## UART Connections

USART2 is used for game controls.

| Function | STM32 Pin |
|-----------|------------|
| TX | PA2 |
| RX | PA3 |

UART Configuration:

- Baud Rate: 9600
- Data Bits: 8
- Parity: None
- Stop Bits: 1



## Controls

| Key | Action |
|-------|----------|
| A / ← | Move Left |
| D / → | Move Right |
| W / ↑ | Rotate Piece |
| S / ↓ | Soft Drop |
| Space | Hard Drop |

Arrow keys from VT100-compatible terminals are also supported.



## Display Implementation

The LCD1602A only supports **8 custom characters**, so the game creates a virtual pixel display:

- Virtual Resolution: **10 × 16 pixels**
- Playable Area: **8 × 14 pixels**
- Border around the playfield
- Four CGRAM slots used to render the screen

This technique allows smooth Tetris gameplay on a standard character LCD.



## Scoring

- Points are awarded when lines are cleared.
- Clearing multiple lines grants bonus points.
- The current score is displayed on the LCD.

