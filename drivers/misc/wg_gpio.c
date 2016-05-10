/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 * Copyright 2010, 2011 David Jander <david@protonic.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>

static int wg_major = 252;
static int wg_minor = 0;

static int num_of_devices = 1;

struct class *wg_class= NULL;

char data[50]="foobar not equal to barfoo";

struct cdev cdev;


char wg_value_read[2] = {0x55,0xaa};
volatile unsigned int wg_value_ptr = 0;
static unsigned count = 0;
static unsigned start = 0;
static unsigned char data_receive[5] ={0x00,0x00,0x00,0x00,0x00};

static unsigned int *data_receive_data = data_receive + 1;
struct timer_list timer_list;

static int wg_open(struct inode *inode, struct file *filp)
{
	printk("open wg_device!\r\n");
	return 0;
}
static int wg_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static int wg_read(struct file *filp,char *buff,size_t count,loff_t *offp)
{	
	if(copy_to_user(buff,&wg_value_ptr,count)){
		printk("read error\r\n");
		return 0;
	}
	return count;

}

static int wg_write(struct file *filp,const char *buff,size_t count,loff_t *offp)
{

	mod_timer(&timer_list,jiffies + HZ/8);    //125ms 一周期
	if(copy_from_user(data_receive,buff,5))
	{
		printk("write error\r\n");
		return 0;
	}
	return count;
}

struct file_operations wg_fops = {
	.owner = THIS_MODULE,
	.open = wg_open,
	.release = wg_release,
	.read = wg_read,
	.write = wg_write,
};


struct wg_platform_data {
	unsigned int wg0_data0;
	unsigned int wg0_data1;
	unsigned int wg1_data0;
	unsigned int wg1_data1;
};
static irqreturn_t gpio_wg_data0(int irq, void *dev_id)
{

	//printk("0%d\r\n",count);
	wg_value_ptr &= ~(1 << count);
	
	count ++;
	if(count == 26)
	{
		printk("id is 0x%x\r\n",wg_value_ptr);
		count = 0;
	}	
	return IRQ_HANDLED;

}


static irqreturn_t gpio_wg_data1(int irq, void *dev_id)
{
	//printk("1%d\r\n",count);
	wg_value_ptr |= (1 << count);
	count ++;
	if(count == 26)
	{
		printk("id is 0x%x\r\n",wg_value_ptr);
		count =0;
	}
	return IRQ_HANDLED;
}
static int __devinit gpio_wg_probe(struct platform_device *pdev)
{

	//printk("++++++++++this is in gpio_wg_probe\r\n");

	struct wg_platform_data *wg_data = (struct wg_platform_data *)(pdev->dev.platform_data);
	int i,error;
	struct device *dev = &pdev->dev;
	const char *desc_wg0_data0 = "gpio_wg0_data0";
	const char *desc_wg0_data1 = "gpio_wg0_data1";
	unsigned int wg0_data0,wg0_data1;
	unsigned int irq0,irq1;
	unsigned long irq0_flags,irq1_flags;
	wg0_data0 = wg_data->wg0_data0;
	wg0_data1 = wg_data->wg0_data1;

	printk("the wg is %d , %d \r\n",wg0_data0,wg0_data1);
	error = gpio_request(wg0_data0,desc_wg0_data0);
	if(error < 0 ){
		printk("failed to request GPIO %d\r\n",wg0_data0);
	}

	error = gpio_request(wg0_data1,desc_wg0_data1);
	if(error < 0){
		printk("failed top request GPIO %d\r\n",wg0_data1);
	}
	gpio_request(55,"out_55");
	gpio_request(56,"out_56");

	error = gpio_direction_input(wg0_data0);
        if(error < 0 ){
                  printk("failed to set wg0_data0 is input\r\n");
        }
	error = gpio_direction_input(wg0_data1);
	if(error < 0){
		printk("failed to set wg0_data1 is input\r\n");
	}

	gpio_direction_output(56,1);
	gpio_direction_output(55,1);
	irq0 = gpio_to_irq(wg0_data0);
	if(irq0 < 0){
		printk("failed to get irq0\r\n");
	}
	irq1 = gpio_to_irq(wg0_data1);
	if(irq1 < 0){
		printk("failed to get irq1\r\n");
	}
	printk("the irq is %d ,%d\r\n",irq0,irq1);

	irq0_flags = IRQF_TRIGGER_FALLING;
	irq1_flags = IRQF_TRIGGER_FALLING;

	error = request_irq(irq0,  gpio_wg_data0, irq0_flags, desc_wg0_data0, wg_data);
	if(error < 0)
	{
		printk("failed to request irq0\r\n");
	}	
	error = request_irq(irq1, gpio_wg_data1, irq1_flags, desc_wg0_data1, wg_data);
	if(error < 0)
	{
		printk("failed top request irq1\r\n");
	}


	printk("----------this is in gpio_wg_probe\r\n");
	return 0;
}

static int __devexit gpio_wg_remove(struct platform_device *pdev)
{
	return 0;
}

static void timer_function(unsigned long data)
{
	//mod_timer(&timer_list,jiffies + HZ/6);

//	printk("timer\r\n");
	static unsigned int count_out=0;
	unsigned int wg_send_value = *data_receive_data;
	
	//if(start){
	if(data_receive[0]){
	mod_timer(&timer_list,jiffies + HZ/8);    //125ms 一周期
	if(wg_send_value & (1 << count_out))
	{
		gpio_set_value(55,0);
		udelay(50);              //低电平 保持在50us
		gpio_set_value(55,1);
	}else
	{
		gpio_set_value(56,0);
		udelay(50);             //低电平 保持在50us
		gpio_set_value(56,1);
	}

	count_out++;
	if(count_out == 26)
	{
		data_receive[0] = 0x00;
		count_out =0;
		printk("out\r\n");
	}
	}
	//gpio_set_value(56,1);
}


static struct platform_driver gpio_wg_device_driver = {
	.probe		= gpio_wg_probe,
	.remove		= __devexit_p(gpio_wg_remove),
	.driver		= {
		.name	= "gpio_wg",
		.owner	= THIS_MODULE,
	}
};

static int __init gpio_wg_init(void)
{
	int error,devno=MKDEV(wg_major,wg_minor);
	cdev_init(&cdev,&wg_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &wg_fops;

	error = cdev_add(&cdev,devno,1);
	if(error)
	{
		printk("this is a error for cdev\r\n");
	}

	wg_class = class_create(THIS_MODULE,"wg_gpio_class");

	if(IS_ERR(wg_class))
	{
		printk("failed to create class\r\n");
	}

	device_create(wg_class,NULL,devno,NULL,"wg_gpio_class");

	/*init timer*/
	init_timer(&timer_list);
	timer_list.function = timer_function;
	timer_list.expires = jiffies + HZ/8;
	add_timer(&timer_list);
	printk("++++++++++gpio_wg_init\r\n");
	return platform_driver_register(&gpio_wg_device_driver);
}

static void __exit gpio_wg_exit(void)
{
	platform_driver_unregister(&gpio_wg_device_driver);
}

module_init(gpio_wg_init);
module_exit(gpio_wg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhengsj <zhengsj@163.com>");
MODULE_DESCRIPTION("wg driver for GPIOs");
MODULE_ALIAS("platform:gpio-wg");
