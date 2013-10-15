#ifndef __ASM_ARCH_LS1X_PWM_H
#define __ASM_ARCH_LS1X_PWM_H

struct pwm_device {
	unsigned int id;
	unsigned int gpio;
	bool used;
};

#endif /* __ASM_ARCH_LS1X_PWM_H */
