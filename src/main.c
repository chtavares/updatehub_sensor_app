/*
 * Copyright (c) 2019 O.S.Systems
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <updatehub.h>
#include <dfu/mcuboot.h>
#include <logging/log.h>
#include <sensor.h>

LOG_MODULE_REGISTER(main);

K_SEM_DEFINE(sem, 0, 1);

static void trigger_handler(struct device *dev, struct sensor_trigger *trigger)
{
	if (sensor_sample_fetch(dev)) {
		LOG_ERR("sensor_sample_fetch failed");
		return;
	}

	k_sem_give(&sem);
}

static void sensor_inf(void)
{
	struct device *dev;
	struct sensor_value accel[3];
	struct sensor_value attr = { .val1 = 6,
				     .val2 = 250000, };
	struct sensor_trigger trig = { .type = SENSOR_TRIG_DELTA,
				       .chan = SENSOR_CHAN_ACCEL_XYZ, };
	int last_value = -1;

	dev = device_get_binding(DT_NXP_FXOS8700_0_LABEL);
	if (dev == NULL) {
		LOG_ERR("Could not get fxos8700 device");
		return;
	}

	if (sensor_attr_set(dev, SENSOR_CHAN_ALL,
			    SENSOR_ATTR_SAMPLING_FREQUENCY, &attr)) {
		LOG_ERR("Could not set sampling frequency");
		return;
	}

	attr.val1 = 10;
	attr.val2 = 00000;

	if (sensor_attr_set(dev, SENSOR_CHAN_ALL,
			    SENSOR_ATTR_SLOPE_TH, &attr)) {
		LOG_ERR("Could not set slope threshold");
		return;
	}

	if (sensor_trigger_set(dev, &trig, trigger_handler)) {
		LOG_ERR("Could not set trigger");
		return;
	}

	LOG_INF("Rotate the device to ask updates from the server");

	while (1) {
		k_sem_take(&sem, K_FOREVER);

		if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
			LOG_ERR("Could not read the sensor value");
			return;
		}

		if (last_value > 0 && accel[2].val1 < 0 ||
		    last_value < 0 && accel[2].val1 > 0) {

			LOG_INF("Starting the UpdateHub");

			switch (updatehub_probe()) {
			case UPDATEHUB_HAS_UPDATE:
				switch (updatehub_update()) {
				case UPDATEHUB_OK:
					sys_reboot(0);
					break;

				default:
					LOG_ERR("Error installing update.");
					break;
				}

			case UPDATEHUB_NO_UPDATE:
				LOG_INF("No update found");

				break;

			default:
				LOG_ERR("Invalid response");
				break;
			}

			last_value = accel[2].val1;
		}
	}
}


int main(void)
{
	int ret = -1;

	LOG_INF("Running the app");

	/* The image of application needed be confirmed */
	LOG_INF("Confirming the boot image");
	ret = boot_write_img_confirmed();
	if (ret < 0) {
		LOG_ERR("Error to confirm the image");
	}

	sensor_inf();

	return ret;
}
