#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/sysfs.h>

/* left led group */
#define GPIO_OUTPUT_LEFT_1 17
#define GPIO_OUTPUT_LEFT_2 27
#define GPIO_OUTPUT_LEFT_3 22
#define GPIO_OUTPUT_LEFT_4 5

/* right led group */
#define GPIO_OUTPUT_RIGHT_1 6
#define GPIO_OUTPUT_RIGHT_2 26
#define GPIO_OUTPUT_RIGHT_3 23
#define GPIO_OUTPUT_RIGHT_4 24

#define GPIO_INPUT_1 16

#define ON  1
#define OFF 0
#define DEVICE_NAME "toy_engine"
#define DEVICE_CLASS DEVICE_NAME
#define DEVICE_DRIVER DEVICE_NAME "_driver"

#define BUF_SIZE 1024
#define MOTOR_SPEED_BASE 500
#define MOTOR_SPPED_MAX 10000

#define MOTOR_1_LED_LEFT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_1, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_2, 1);	\
} while (0)

#define MOTOR_1_LED_LEFT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_1, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_2, 0);	\
} while (0)

#define MOTOR_1_LED_RIGHT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_3, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_4, 1);	\
} while (0)

#define MOTOR_1_LED_RIGHT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_3, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_LEFT_4, 0);	\
} while (0)

#define MOTOR_2_LED_LEFT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_1, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_2, 1);	\
} while (0)

#define MOTOR_2_LED_LEFT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_1, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_2, 0);	\
} while (0)

#define MOTOR_2_LED_RIGHT_ON() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_3, 1);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_4, 1);	\
} while (0)

#define MOTOR_2_LED_RIGHT_OFF() 	\
do {	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_3, 0);	\
	gpio_set_value(TOY_GPIO_OUTPUT_RIGHT_4, 0);	\
} while (0)


#define MOTOR_1_SET_SPEED _IOW('w', '1', int32_t *)
#define MOTOR_2_SET_SPEED _IOW('w', '2', int32_t *)
#define MOTOR_1_HALT _IOW('w', '3', int32_t *)
#define MOTOR_2_HALT _IOW('w', '3', int32_t *)

static unsigned int input_irq;

dev_t dev;
struct class *cls;
struct cdev cdev;
struct kobject *kobj;

static struct motor_info motor_1 = {
	.attr = {
		.name = "motor_1",
		.mode = 0644,
	},
	.speed = MOTOR_SPEED_BASE,
	.toggle = 0,
};
static struct motor_info motor_2 = {
	.attr = {
		.name = "motor_2",
		.mode = 0644,
	},
	.speed = MOTOR_SPEED_BASE,
	.toggle = 0,
};

static unsigned int gpio_outputs[] = {
	GPIO_OUTPUT_LEFT_1,
	GPIO_OUTPUT_LEFT_2,
	GPIO_OUTPUT_LEFT_3,
	GPIO_OUTPUT_LEFT_4,
	GPIO_OUTPUT_RIGHT_1,
	GPIO_OUTPUT_RIGHT_2,
	GPIO_OUTPUT_RIGHT_3,
	GPIO_OUTPUT_RIGHT_4,
};
static unsigned int gpio_inputs[] = {
	GPIO_INPUT_1,
};

static unsigned gpio_outputs_len = sizeof(gpio_outputs) / sizeof(int);
static unsigned gpio_inputs_len = sizeof(gpio_inputs) / sizeof(int);

int calibrate_ms(long ms)
{
	if (ms < 1 || ms > MOTOR_SPEED_MAX) return MOTOR_SPEED_BASE;
	return (int)ms;
}

int open_engine_driver(struct inode *inode, struct file *file)
{
	pr_info("[%s] open engine driver", DEVICE_NAME);
	return 0;
}

int close_engine_driver(struct inode *inode, struct file *file)
{
	pr_info("[%s] close engine driver", DEVICE_NAME);
	return 0;
}

ssize_t read_engine_driver(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	pr_info("[%s] read engine driver", DEVICE_NAME);
	return count;
}

ssize_t write_engine_driver(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	pr_info("[%s] write engine driver", DEVICE_NAME);
	return count;
}

long ioctl_engine_driver(struct file *file, unsigned int cmd, unsigned long args)
{
	switch (cmd) {
		case MOTOR_1_SET_SPEED:
			pr_info("[%s] motor 1 set speed: %d\n", DEVICE_NAME, args);
			motor_1.speed = calibrate_ms(args);
			break;
		case MOTOR_2_SET_SPEED:
			pr_info("[%s] motor 2 set speed: %d\n", DEVICE_NAME, args);
			motor_2.speed = calibrate_ms(args);
			break;
		default:
			pr_info("[%s] unknown ioctl command", DEVICE_NAME);
	}
	return 0;
}

ssize_t show_engine_class(struct kobject *ko, struct attribute *attr, char *buf)
{
	struct motor_info *info = container_of(attr, struct motor_info, attr);
	pr_info("[%s] %s show %d\n", DEVICE_NAME, info->attr.name, info->speed);
	return scnprintf(buf, sizeof(int) + 1, "%d", info->speed);
}

ssize_t store_engine_class(struct kobject *ko, struct attribute *attr, const char *buf, size_t count)
{
	int speed;
	struct motor_info *info = container_of(attr, struct motor_info, attr);
	if (kstrtoint(buf, 10, &speed) < 0) {
		pr_err("[%s] fail to convert store string to int");
		return -1;
	}
	info->speed = speed;
	pr_info("[%s] %s store %d", DEVICE_NAME, info->attr.name, speed);
	return count;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = open_engine_driver,
	.read = read_engine_driver,
	.write = write_engine_driver,
	.release = close_engine_driver,
	.unlocked_ioctl = ioctl_engine_driver,
};

struct sysfs_ops sfops = {
	.show = show_engine_class,
	.store = store_engine_class,
};

struct attribute *engine_attrs[] = {
	&motor_1.attr,
	&motor_2.attr,
	NULL,
};

ATTRIBUTE_GROUPS(engine);

struct kobj_type ktype = {
	.default_groups = engine_groups,
	.sysfs_ops = &sfops,
};

enum hrtimer_restart motor_1_restart_callback(struct hrtimer *timer)
{
	motor_1.toggle = !motor_1.toggle;
	if (motor_1.toggle) {
		MOTOR_1_LED_LEFT_ON();
		MOTOR_1_LED_RIGHT_OFF();
	} else {
		MOTOR_1_LED_LEFT_OFF();
		MOTOR_1_LED_RIGHT_ON();
	}

	hrtimer_forward_now(timer, ms_to_ktime(motor_1.speed));
	return HRTIMER_RESTART;
}

enum hrtimer_restart motor_2_restart_callback(struct hrtimer *timer)
{
	motor_2.toggle = !motor_2.toggle;
	if (motor_2.toggle) {
		MOTOR_2_LED_LEFT_ON();
		MOTOR_2_LED_RIGHT_OFF();
	} else {
		MOTOR_2_LED_LEFT_OFF();
		MOTOR_2_LED_RIGHT_ON();
	}

	hrtimer_forward_now(timer, ms_to_ktime(motor_2.speed));
	return HRTIMER_RESTART;
}

irqreturn_t gpio_irq_handler(int irq, void *argv)
{
	int input_value;

	pr_info("[%s] gpio irq handler - stopped motor!", DEVICE_NAME);

	input_value = gpio_get_value(GPIO_INPUT_1);

	if (input_value) {
		hrtimer_cancel(&motor_1.timer);
		hrtimer_cancel(&motor_2.timer);

		MOTOR_1_LED_LEFT_OFF();
		MOTOR_1_LED_RIGHT_OFF();
		MOTOR_2_LED_LEFT_OFF();
		MOTOR_2_LED_RIGHT_OFF();
	}

	return IRQ_HANDLED;
}

int register_gpio(int gpio, int in)
{
	char name[40];

	snprintf(name, sizeof(name), "gpio-%d", gpio);

	if (gpio_request(gpio, name) < 0) {
		pr_err("[%s] fail to request gpio", DEVICE_NAME);
		return -1;
	}

	if (in == 1) {
		if (gpio_direction_input(gpio) < 0) {
			pr_err("[%s] fail to change direction to input", DEVICE_NAME);
			return -1;
		}
	} else {
		if (gpio_direction_output(gpio, 0) < 0) {
			pr_err("[%s] fail to change direction to output", DEVICE_NAME);
			return -1;
		}
	}

	gpio_set_debounce(gpio, 300);

	return 0;
}

int unregister_gpio(int gpio)
{
	gpio_set_value(gpio, 0);
	gpio_free(gpio);

	return 0;
}

static int __init engine_module_init(void)
{
	int i;

	pr_info("[%s] engine module init", DEVICE_NAME);

	if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0) {
		pr_err("[%s] fail to allocate character device region", DEVICE_NAME);
		goto region_err;
	}

	if (!(cls = class_create(THIS_MODULE, DEVICE_CLASS))) {
		pr_err("[%s] fail to create device class", DEVICE_NAME);
		goto class_err;
	}

	if (!device_create(cls, NULL, dev, NULL, DEVICE_DRIVER)) {
		pr_err("[%s] fail to create device driver", DEVICE_NAME);
		goto device_err;
	}

	cdev_init(&cdev, &fops);
	if (cdev_add(&cdev, dev, 1) < 0) {
		pr_err("[%s] fail to add character drvice", DEVICE_NAME);
		goto cdev_err;
	}

	kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!kobj) {
		pr_err("[%s] fail to allocate kobject", DEVICE_NAME);
		goto cdev_err;
	}

	kobject_init(kobj, &ktype);
	if (kobject_add(kobj, kernel_kobj, DEVICE_NAME) < 0) {
		pr_err("[%s] fail to add kobject to /sys/kernel", DEVICE_NAME);
		goto kobj_err;
	}

	for (i = 0; i < gpio_outputs_len; i++) {
		register_gpio(gpio_outputs[i], 0);
	}

	for (i = 0; i < gpio_inputs_len; i++) {
		register_gpio(gpio_inputs[i], 1);
	}

	input_irq = gpio_to_irq(GPIO_INPUT_1);

	if (request_irq(input_irq, gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_irq_handler", NULL) < 0) {
		pr_err("[%s] fail to request irq handler", DEVICE_NAME);
		goto gpio_err;
	}

	hrtimer_init(&motor_1.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	motor_1.timer.function = &motor_1_restart_callback;
	hrtimer_start(&motor_1.timer, 0, HRTIMER_MODE_REL);

	hrtimer_init(&motor_2.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	motor_2.timer.function = &motor_2_restart_callback;
	hrtimer_start(&motor_2.timer, 0, HRTIMER_MODE_REL);

	return 0;

gpio_err:
	for (i = 0; i < gpio_outputs_len; i++)
		unregister_gpio(gpio_outputs[i]);

	for (i = 0; i < gpio_inputs_len; i++)
		unregister_gpio(gpio_inputs[i]);

	free_irq(input_irq, NULL);

kobj_err:
	kobject_put(kobj);
	kfree(kobj);

cdev_err:
	device_destroy(cls, dev);

device_err:
	class_destroy(cls);

class_err:
	unregister_chrdev_region(dev, 1);

region_err:
	return -1;
}

static void __exit engine_module_exit(void)
{
	int i;

	hrtimer_cancel(&motor_1.timer);
	hrtimer_cancel(&motor_2.timer);

	free_irq(input_irq, NULL);

	for (i = 0; i < gpio_outputs_len; i++)
		unregister_gpio(gpio_outputs[i]);	

	for (i = 0; i < gpio_inputs_len; i++)
		unregister_gpio(gpio_inputs[i]);	

	kobject_put(kobj);
	kfree(kobj);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);

	pr_info("[%s] exit module\n", DEVICE_NAME);
}

module_init(engine_module_init);
module_exit(engine_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hj <hyejeong12311@gmail.com>");
MODULE_DESCRIPTION("motor engine driver");
MODULE_VERSION("1.0.0");