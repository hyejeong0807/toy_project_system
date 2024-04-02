#include "bmp280.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spi/spi.h>

#define TOY_BUS_NUM 0 
static struct spi_device *bmp280_dev;

struct spi_board_info spi_device_info = {
    .modalias = "bmp280",
    .max_speed_hz = 1000000,
    .bus_num = TOY_BUS_NUM,
    .chip_select = 0,
    .mode = 3,
};

s32 read_temperature(void)
{
	int var1, var2;
	s32 raw_temp;
	s32 d1, d2, d3;

	d1 = spi_w8r8(bmp280_dev, 0xFA);
	d2 = spi_w8r8(bmp280_dev, 0xFB);
	d3 = spi_w8r8(bmp280_dev, 0xFC);
	raw_temp = ((d1 << 16) | (d2 << 8) | d3) >> 4;
	pr_info("raw_temp: %d\n", raw_temp);

	var1 = ((((raw_temp >> 3) - (dig_T1 << 1))) * (dig_T2)) >> 11;

	var2 = (((((raw_temp >> 4) - (dig_T1)) * ((raw_temp >> 4) - (dig_T1))) >> 12) * (dig_T3)) >> 14;
	t_fine = var1 + var2;

	return ((var1 + var2) * 5 + 128) >> 8;
}


s32 read_pressure(void)
{
	int var1, var2, press;
	s32 d1, d2, d3, raw_press;

	d1 = spi_w8r8(bmp280_dev, 0xF7);
	d2 = spi_w8r8(bmp280_dev, 0xF8);
	d3 = spi_w8r8(bmp280_dev, 0xF9);

	raw_press = ((d1 << 16) | (d2 << 8) | d3) >> 4;

	var1 = t_fine - 128000;
	var2 = var1 * var1 * dig_P6;
	var2 = var2 + (var1 * ((s64)dig_P5 << 17));
	var2 = var2 + ((s64)dig_P4 << 35);
	var1 = ((var1 * var1 * (s64)dig_P3) >> 8) + ((var1 * (s64)dig_P2) << 12);
	var1 = (((((s64)1) << 47) + var1)) * ((s64)(u16)dig_P1) >> 33;
	if (var1 == 0) {
		// avoid exception caused by division by zero
		return 0;
	}

	press = 1048576 - raw_press;
	press = ((press << 31) - var2) * 3125 / var1;
	var1 = (dig_P9 * (press >> 13) * (press >> 13)) >> 25;
	var2 = (dig_P8 * press) >> 19;
	press = ((press + var1 + var2) >> 8) + (dig_P7 << 4);

	return (u32)press / 256; // hPa
}


static ssize_t k_driver_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	char out_string[20];
	int temperature, pressure;

	temperature = read_temperature();
	pressure = read_pressure();

	snprintf(out_string, sizeof(out_string), "%d.%d %d.%d\n", temperature / 100, temperature % 100, pressure / 100, pressure % 100);
	pr_info("Temperature: %d.%d°C, Pressure: %d.%dhPa\n", temperature / 100, temperature % 100, pressure / 100, pressure % 100);

	if (copy_to_user(buf, out_string, count)) {
		pr_err("read: error!\n");
	}

	return count;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = k_driver_open,
	.release = k_driver_close,
	.read = k_driver_read,
	.write = k_driver_write
};

static int __init k_module_init(void)
{
	u8 id;
	struct spi_master *master;

	if( alloc_chrdev_region(&k_dev, 0, 1, DRIVER_NAME) < 0) {
		pr_info("Device Nr. could not be allocated!\n");
		return -1;
	}

	if((k_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		pr_info("Device class can not be created!\n");
		goto cerror;
	}

	if(device_create(k_class, NULL, k_dev, NULL, DRIVER_NAME) == NULL) {
		pr_info("Can not create device file!\n");
		goto device_error;
	}

	cdev_init(&k_device, &fops);

	if(cdev_add(&k_device, k_dev, 1) == -1) {
		pr_err("Registering of device to kernel failed!\n");
		goto reg_error;
	}

	master = spi_busnum_to_master(TOY_BUS_NUM);
	if (!master) {
		pr_err("Error! spi bus with Nr. %d\n", TOY_SPI_BUS_NUM);
		goto reg_error;
	}

	bmp280_dev = spi_new_device(master, &spi_device_info);
	if(!bmp280_dev) {
		pr_info("Could not spi create device!\n");
		goto reg_error;
	}

	bmp280_dev->bits_per_word = 8;

	if(spi_setup(bmp280_dev) != 0){
		pr_info("Could not change bus setup!\n");
		spi_unregister_device(bmp280_dev);
		goto spi_error;
	}

	id = spi_w8r8(bmp280_dev, 0xD0);
	pr_info("ID: 0x%x\n", id);

	return 0;

spi_error:
	spi_unregister_device(bmp280_dev);
reg_error:
	device_destroy(k_class, k_dev);
device_error:
	class_destroy(k_class);
cerror:
	unregister_chrdev_region(k_dev, 1);
	return -1;
}

static void __exit k_module_exit(void)
{
	if(bmp280_dev)
		spi_unregister_device(bmp280_dev);
	cdev_del(&k_device);
	device_destroy(k_class, k_dev);
	class_destroy(k_class);
	unregister_chrdev_region(k_dev, 1);
	pr_info("exit\n");
}

module_init(k_module_init);
module_exit(k_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("K <k@k.com>");
MODULE_DESCRIPTION("k spi");
MODULE_VERSION("1.0.0");