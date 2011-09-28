/******************************************************************************
 *       Filename:  74LS165_button.c
 *      
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2008年06月24日  09时24分57秒  CST
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Curtus
 *        Company:  Grandchips  Microelectronics  co.,Ltd 
 *
 ******************************************************************************/

#include<linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/timer.h>
#include<linux/ioctl.h>
#include<linux/io.h>
#include<linux/types.h>
#include<linux/kernel.h>
#include<linux/ioport.h>
#include<linux/errno.h>
#include<asm/uaccess.h>
#include<linux/miscdevice.h>
#include<linux/wait.h>
#include<linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include "74LV165_button.h"

#define BUF_MAXSIZE 16
#define TIMER_DELAY 5

static  unsigned int  BReadBuf = 0;

 
static  int delay(int time){
    while(--time);  
    return 0;
}

static void prepare_for_read(void){

	int reg = 0;
	
	/* CP =0 */
	LS1B_74LV165_READ(reg, LS1B_74LV165_CP);
	LS1B_74LV165_WRITE(LS1B_74LV165_CP, reg & (~(1 << 2)));
	
	/* PL = 0 */
	LS1B_74LV165_READ(reg, LS1B_74LV165_PL);
	LS1B_74LV165_WRITE(LS1B_74LV165_PL, reg & ~(1 << 1));
	
	/* delay 100 */
	delay(10000);
	
	/* PL = 1 */
	LS1B_74LV165_READ(reg, LS1B_74LV165_PL);
	LS1B_74LV165_WRITE(LS1B_74LV165_PL, reg | (1 << 1));


}

static void scanf_keyboard(void){
		
	int reg = 0, val = 0;
	int time;

	delay(10);
	LS1B_74LV165_READ(reg, LS1B_74LV165_DATA);
	val = ((~reg) & (1 << 0));
	BReadBuf = val >> 0;
	
	for(time = 1; time < 16; time ++){
		/* delay */
		  delay(100);

	      /* CP = 1 */
	      LS1B_74LV165_READ(reg, LS1B_74LV165_CP);
	      LS1B_74LV165_WRITE(LS1B_74LV165_CP, reg | (1 << 2));

	      delay(100);
	      LS1B_74LV165_READ(reg, LS1B_74LV165_DATA);
	      val = ((~reg) & (1 << 0));
	      BReadBuf |= val << time;

	      /* CP = 0 */
	      LS1B_74LV165_READ(reg, LS1B_74LV165_CP);
	      LS1B_74LV165_WRITE(LS1B_74LV165_CP, reg & (~(1 << 2)));
      }
	
}

static  void button_read(void){

      prepare_for_read();

      /* scanf keyboard */
	  scanf_keyboard();

      if(BReadBuf){
      	      printk("\nBReadBuf value is 0x%08x\n",BReadBuf);
      }
 
}

static  int LS1B_74LV165_button_open(struct inode *inode, struct file *filp){
    int reg;
    printk("Welcome to use 74LV165 driver\n");
    //enalbe pin
	LS1B_74LV165_EN_GPIO(GPIO_KEY_DATA);
	LS1B_74LV165_EN_GPIO(GPIO_KEY_EN);
	LS1B_74LV165_EN_GPIO(GPIO_KEY_SCL);
	//enable input/output
	LS1B_74LV165_OEN_GPIO(GPIO_KEY_SCL);
	LS1B_74LV165_OEN_GPIO(GPIO_KEY_EN);
	LS1B_74LV165_IEN_GPIO(GPIO_KEY_DATA);
     printk("init 74LV165 is done\n");
#if 0
	LS1B_74LV165_READ(reg, LS1B_GPIO_REG_BASE+REG_LOW_GPIO_CFG);
	printk("74LV165_CFG value is 0x%04X\n",reg);
	LS1B_74LV165_READ(reg, LS1B_GPIO_REG_BASE+REG_LOW_GPIO_OE);
	printk("74LV165_OE value is 0x%04X\n",reg);

    LS1B_74LV165_READ(reg, LS1B_74LV165_PL);
    printk("74LV165_PL value is 0x%04X\n",reg);

    LS1B_74LV165_READ(reg, LS1B_74LV165_CP);
    printk("74LV165_CLK value is 0x%04X\n",reg);
#endif
    return 0;
}

static  ssize_t LS1B_74LV165_button_read(struct file  *filp, char __user *buf, size_t count, loff_t *oppos){

	button_read();

    if(BReadBuf){
   	 	copy_to_user(buf,&BReadBuf,count);
    	BReadBuf= 0;
		delay(10000000);
    	return count;
    }
    return -EFAULT;
}

static int LS1B_74LV165_button_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    return 0;
}

static  struct file_operations ls1b_74Llv165_button_fops = {
    .open = LS1B_74LV165_button_open,
    .read = LS1B_74LV165_button_read,
    .unlocked_ioctl = LS1B_74LV165_button_ioctl,
};

static struct miscdevice ls1b_74lv165_button = {
	.minor = LS1B_BUTTON_MINOR,
	.name  = "ls1b_buttons",
	.fops  = &ls1b_74Llv165_button_fops,
};

static  int __init  LS1B_74LV165_button_init(void){
    int ret;
    printk("======================button init=========================\n");
//    ret = register_chrdev(0,"LS1B_74LV165_button",&LS1B_74LV165_button_fops);
	ret = misc_register(&ls1b_74lv165_button);
    if(ret < 0){
      printk("74LV165_button can't get major number !\n");
      return ret;
    }

#ifdef CONFIG_DEVFS_FS
    devfs_button_dir = devfs_mk_dir(NULL,"LS1B_74LV165_button",NULL);
    devfs_buttonraw = devfs_register(devfs_kbd_dir,"0raw",DEVFS_FL_DEFAULT,kbdMajor,KBDRAW_MINOR,S_IFCHR|S_IRUSR|S_IWUSR,&LS1B_74LV165_button_fops,NULL);
#endif
    return 0;
}

static  void  __exit  LS1B_74LV165_button_exit(void){
	misc_deregister(&ls1b_74lv165_button);
}

module_init(LS1B_74LV165_button_init);
module_exit(LS1B_74LV165_button_exit);