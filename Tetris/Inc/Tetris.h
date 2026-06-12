#ifndef TETRIS_H_
#define TETRIS_H_

#include "Stone.h"
#include "lcd.h"
#include <stdint.h>

/*
 * UART Controls (9600 baud, 8N1)
 *   a / A  or  left-arrow   →  Move Left
 *   d / D  or  right-arrow  →  Move Right
 *   w / W  or  up-arrow     →  Rotate
 *   s / S  or  down-arrow   →  Soft drop
 *   SPACE                   →  Hard drop
 */

extern UART_HandleTypeDef huart2;
extern Lcd_HandleTypeDef  hlcd;

void Te_Init(void);
void Te_Update(uint32_t frameCount);
void Te_Draw(void);

#endif /* TETRIS_H_ */
