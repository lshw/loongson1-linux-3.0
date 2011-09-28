/******************************************************************************
 * Func : 
 * Desc : 
 * Args : 
 * Outs : 
 ******************************************************************************/

#ifndef _74LV165_BUTTON_H_
#define _74LV165_BUTTON_H_

#define LS1B_GPIO_REG_BASE  0xbfd01000

#define REG_LOW_GPIO_CFG	0xC0
#define REG_HIG_GPIO_CFG    0XC4

#define REG_LOW_GPIO_OE		0xD0
#define REG_HIG_GPIO_OE		0xD4

#define REG_LOW_GPIO_IN		0xE0
#define REG_HIG_GPIO_IN		0xE4

#define REG_LOW_GPIO_OUT	0xF0
#define REG_HIG_GPIO_OUT	0xF4

#define GPIO_KEY_DATA 0
#define GPIO_KEY_EN   1
#define GPIO_KEY_SCL  2


#define LS1B_74LV165_PL  (LS1B_GPIO_REG_BASE + REG_LOW_GPIO_OUT)
#define LS1B_74LV165_CP  (LS1B_GPIO_REG_BASE + REG_LOW_GPIO_OUT)
#define LS1B_74LV165_DATA  (LS1B_GPIO_REG_BASE + REG_LOW_GPIO_IN)

#define LS1B_74LV165_EN_GPIO(gpio)	(*(volatile unsigned char *)(LS1B_GPIO_REG_BASE+REG_LOW_GPIO_CFG) |= (1 << gpio))
#define LS1B_74LV165_OEN_GPIO(gpio)	(*(volatile unsigned char *)(LS1B_GPIO_REG_BASE+REG_LOW_GPIO_OE) &= ~(1 << gpio))
#define LS1B_74LV165_IEN_GPIO(gpio)	(*(volatile unsigned char *)(LS1B_GPIO_REG_BASE+REG_LOW_GPIO_OE) |= (1 << gpio))

#define LS1B_74LV165_WRITE(reg, val)  ((*(volatile unsigned char *)(reg)) = val)
#define LS1B_74LV165_READ(val, reg)   (val = *(volatile unsigned char *)(reg))

#define LS1B_BUTTON_MINOR 143

#define GPIO_MAGIC 'K'
#define GPIO_READ_OE_UP    _IO(GPIO_MAGIC,   0)
#define GPIO_WRITE_OE_UP   _IO(GPIO_MAGIC,   1)
#define GPIO_READ          _IO(GPIO_MAGIC,   2)
#define GPIO_WRITE         _IO(GPIO_MAGIC,   3)
#define GPIO_WRITE_LOW     _IO(GPIO_MAGIC,   4)
#define GPIO_WRITE_HIGH    _IO(GPIO_MAGIC,   5)
#define GPIO_TIMER_ON      _IO(GPIO_MAGIC,   6)
#define GPIO_TIMER_OFF     _IO(GPIO_MAGIC,   7)

#endif