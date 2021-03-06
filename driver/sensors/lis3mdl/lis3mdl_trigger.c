/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2021 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_lis3mdl

#include <device.h>
#include <drivers/i2c.h>
#include <sys/__assert.h>
#include <sys/util.h>
#include <kernel.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include "lis3mdl.h"

LOG_MODULE_DECLARE(LIS3MDL, CONFIG_SENSOR_LOG_LEVEL);

K_MSGQ_DEFINE(int_events, sizeof(uint32_t), 32, 4);

int lis3mdl_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
						sensor_trigger_handler_t handler)
{
	struct lis3mdl_data *drv_data = dev->data;
	// int16_t buf[3];
	int ret;

	switch (trig->type)
	{
	case SENSOR_TRIG_DATA_READY:
		/* TODO: check for drdy */

		// /* Disable first */
		// gpio_pin_interrupt_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, drdy_gpios),
		// 							 GPIO_INT_DISABLE);

		// if (handler == NULL)
		// {
		// 	return -EINVAL;
		// }

		// drv_data->trigger_handler = handler;
		// drv_data->data_ready_trigger = *trig;

		// gpio_pin_interrupt_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, drdy_gpios),
		// 							 GPIO_INT_EDGE_TO_ACTIVE);

		// /* dummy read: re-trigger interrupt */
		// ret = i2c_burst_read(drv_data->i2c, DT_INST_REG_ADDR(0), LIS3MDL_REG_SAMPLE_START,
		// 					 (uint8_t *)buf, 6);
		// if (ret != 0)
		// {
		// 	return ret;
		// }

		break;
	case SENSOR_TRIG_THRESHOLD:
	{

		/* Disable trigger if NULL */
		if (handler == NULL)
		{

			uint8_t reg_int_cfg = LIS3MDL_INT_BIT3;

			gpio_pin_interrupt_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, irq_gpios),
										 GPIO_INT_DISABLE);

			/* Disable interrupt */
			ret = i2c_reg_write_byte(drv_data->i2c, DT_INST_REG_ADDR(0), LIS3MDL_REG_INT_CFG,
									 reg_int_cfg);
			if (ret != 0)
			{
				LOG_ERR("Unable to set interrupt cfg register. Err %i", ret);
				return ret;
			}

			return 0;
		}

		drv_data->trigger_handler = handler;
		drv_data->threshold_trigger = *trig;

		gpio_pin_interrupt_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, irq_gpios),
									 GPIO_INT_EDGE_TO_ACTIVE);

		/* Int register */
		uint8_t reg_int_cfg = LIS3MDL_INT_BIT3 | LIS3MDL_INT_IEN | LIS3MDL_INT_IEA;

		switch (trig->chan)
		{
		case SENSOR_CHAN_MAGN_X:
			reg_int_cfg |= LIS3MDL_INT_X_EN;
			break;
		case SENSOR_CHAN_MAGN_Y:
			reg_int_cfg |= LIS3MDL_INT_Y_EN;
			break;
		case SENSOR_CHAN_MAGN_Z:
			reg_int_cfg |= LIS3MDL_INT_Z_EN;
			break;
		case SENSOR_CHAN_MAGN_XYZ:
			reg_int_cfg |= LIS3MDL_INT_XYZ_EN;
			break;
		default:
			LOG_ERR("Invalid channel");
			return -EINVAL;
			break;
		}

		/* Enable interrupt */
		ret = i2c_reg_write_byte(drv_data->i2c, DT_INST_REG_ADDR(0), LIS3MDL_REG_INT_CFG,
								 reg_int_cfg);
		if (ret != 0)
		{
			LOG_ERR("Unable to set interrupt cfg register. Err %i", ret);
			return ret;
		}

		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

static void lis3mdl_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

#if defined(CONFIG_LIS3MDL_TRIGGER_OWN_THREAD)
	k_msgq_put(&int_events, &pins, K_NO_WAIT);
#elif defined(CONFIG_LIS3MDL_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&drv_data->work);
#endif
}

static void lis3mdl_thread_cb(const struct device *dev, uint32_t pins)
{
	struct lis3mdl_data *drv_data = dev->data;

	if (drv_data->trigger_handler != NULL)
	{
		// if ((pins & BIT(DT_INST_GPIO_PIN(0, drdy_gpios))) > 0)
		// {
		// 	drv_data->trigger_handler(dev, &drv_data->data_ready_trigger);
		// 	gpio_pin_interrupt_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, drdy_gpios), GPIO_INT_EDGE_TO_ACTIVE);
		// }

		if ((pins & BIT(DT_INST_GPIO_PIN(0, irq_gpios))) > 0)
		{
			drv_data->trigger_handler(dev, &drv_data->threshold_trigger);
		}
	}
	else
	{
		LOG_ERR("Callback is null");
	}
}

#ifdef CONFIG_LIS3MDL_TRIGGER_OWN_THREAD
static void lis3mdl_thread(struct lis3mdl_data *drv_data)
{
	while (1)
	{
		uint32_t pins = 0;
		k_msgq_get(&int_events, &pins, K_FOREVER);
		lis3mdl_thread_cb(drv_data->dev, pins);
	}
}
#endif

#ifdef CONFIG_LIS3MDL_TRIGGER_GLOBAL_THREAD
static void lis3mdl_work_cb(struct k_work *work)
{
	struct lis3mdl_data *drv_data = CONTAINER_OF(work, struct lis3mdl_data, work);

	lis3mdl_thread_cb(drv_data->dev);
}
#endif

int lis3mdl_init_interrupt(const struct device *dev)
{
	struct lis3mdl_data *drv_data = dev->data;

	/* setup data ready gpio interrupt */
	drv_data->gpio = device_get_binding(DT_INST_GPIO_LABEL(0, irq_gpios));
	if (drv_data->gpio == NULL)
	{
		LOG_ERR("Cannot get pointer to %s device.", DT_INST_GPIO_LABEL(0, irq_gpios));
		return -EINVAL;
	}

	gpio_pin_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, irq_gpios),
					   GPIO_INPUT | DT_INST_GPIO_FLAGS(0, irq_gpios));

	// gpio_pin_configure(drv_data->gpio, DT_INST_GPIO_PIN(0, drdy_gpios),
	// 				   GPIO_INPUT | DT_INST_GPIO_FLAGS(0, drdy_gpios));

	gpio_init_callback(&drv_data->gpio_cb, lis3mdl_gpio_callback,
					   BIT(DT_INST_GPIO_PIN(0, irq_gpios)));

	if (gpio_add_callback(drv_data->gpio, &drv_data->gpio_cb) < 0)
	{
		LOG_ERR("Could not set gpio callback.");
		return -EIO;
	}

	/* clear data ready interrupt line by reading sample data */
	if (lis3mdl_sample_fetch(dev, SENSOR_CHAN_ALL) < 0)
	{
		LOG_ERR("Could not clear data ready interrupt line.");
		return -EIO;
	}

	drv_data->dev = dev;

#if defined(CONFIG_LIS3MDL_TRIGGER_OWN_THREAD)
	k_sem_init(&drv_data->gpio_sem, 0, K_SEM_MAX_LIMIT);

	k_thread_create(&drv_data->thread, drv_data->thread_stack, CONFIG_LIS3MDL_THREAD_STACK_SIZE,
					(k_thread_entry_t)lis3mdl_thread, drv_data, NULL, NULL,
					K_PRIO_COOP(CONFIG_LIS3MDL_THREAD_PRIORITY), 0, K_NO_WAIT);
#elif defined(CONFIG_LIS3MDL_TRIGGER_GLOBAL_THREAD)
	drv_data->work.handler = lis3mdl_work_cb;
#endif

	return 0;
}
