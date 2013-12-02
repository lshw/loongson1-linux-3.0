/*
 * Generic PWM rfid driver data - see drivers/misc/pwm_rfid.c
 */
#ifndef __LINUX_PWM_RFID_H
#define __LINUX_PWM_RFID_H

//#include <linux/backlight.h>

struct platform_pwm_rfid_data {
	int pwm_id;
//	unsigned int max_brightness;
//	unsigned int dft_brightness;
//	unsigned int lth_brightness;
	unsigned int pwm_period_ns;
	unsigned 	gpio;
	int (*init)(struct device *dev);
	int (*notify)(struct device *dev, int brightness);
	void (*exit)(struct device *dev);
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

#endif
