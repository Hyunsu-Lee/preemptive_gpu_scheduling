/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "../fimc-is-resourcemgr.h"
#include "fimc-is-device-8b1.h"

#define SENSOR_NAME "S5K8B1"
#define DEFAULT_SENSOR_WIDTH	184
#define DEFAULT_SENSOR_HEIGHT	104
#define SENSOR_MEMSIZE DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT

#define SENSOR_REG_VIS_DURATION_MSB			(0x6026)
#define SENSOR_REG_VIS_DURATION_LSB			(0x6027)
#define SENSOR_REG_VIS_FRAME_LENGTH_LINE_ALV_MSB	(0x4340)
#define SENSOR_REG_VIS_FRAME_LENGTH_LINE_ALV_LSB	(0x4341)
#define SENSOR_REG_VIS_LINE_LENGTH_PCLK_ALV_MSB		(0x4342)
#define SENSOR_REG_VIS_LINE_LENGTH_PCLK_ALV_LSB		(0x4343)
#define SENSOR_REG_VIS_GAIN_RED				(0x6029)
#define SENSOR_REG_VIS_GAIN_GREEN			(0x602A)
#define SENSOR_REG_VIS_AE_TARGET			(0x600A)
#define SENSOR_REG_VIS_AE_SPEED				(0x5034)
#define SENSOR_REG_VIS_AE_NUMBER_OF_PIXEL_MSB		(0x5030)
#define SENSOR_REG_VIS_AE_NUMBER_OF_PIXEL_LSB		(0x5031)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_1x1_2		(0x6000)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_1x3_4		(0x6001)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_2x1_2		(0x6002)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_2x3_4		(0x6003)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_3x1_2		(0x6004)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_3x3_4		(0x6005)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_4x1_2		(0x6006)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_4x3_4		(0x6007)
#define SENSOR_REG_VIS_AE_MANUAL_EXP_MSB		(0x5039)
#define SENSOR_REG_VIS_AE_MANUAL_EXP_LSB		(0x503A)
#define SENSOR_REG_VIS_AE_MANUAL_ANG_MSB		(0x503B)
#define SENSOR_REG_VIS_AE_MANUAL_ANG_LSB		(0x503C)
#define SENSOR_REG_VIS_BIT_CONVERTING_MSB		(0x602B)
#define SENSOR_REG_VIS_BIT_CONVERTING_LSB		(0x7203)
#define SENSOR_REG_VIS_AE_OFF				(0x5000)

static struct fimc_is_sensor_cfg config_8b1[] = {
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 30, 17, 0),
	/* 1936x1090@24fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 24, 13, 1),
};

static int sensor_8b1_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);
	return 0;
}
static int sensor_8b1_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);
	return 0;
}
static int sensor_8b1_registered(struct v4l2_subdev *sd)
{
	pr_info("%s\n", __func__);
	return 0;
}

static void sensor_8b1_unregistered(struct v4l2_subdev *sd)
{
	pr_info("%s\n", __func__);
}

static const struct v4l2_subdev_internal_ops internal_ops = {
	.open = sensor_8b1_open,
	.close = sensor_8b1_close,
	.registered = sensor_8b1_registered,
	.unregistered = sensor_8b1_unregistered,
};

static int sensor_8b1_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_8b1 *module_8b1;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_8b1 = module->private_data;
	client = module->client;

	module_8b1->system_clock = 146 * 1000 * 1000;
	module_8b1->line_length_pck = 146 * 1000 * 1000;

	pr_info("%s\n", __func__);
	/* sensor init */
	fimc_is_sensor_write(client, 0x705A, 0x01);
	fimc_is_sensor_write(client, 0x7090, 0x01);
	fimc_is_sensor_write(client, 0x7029, 0x10);
	fimc_is_sensor_write(client, 0x7306, 0x04);
	fimc_is_sensor_write(client, 0x702A, 0x04);

	fimc_is_sensor_write(client, 0x7209, 0xF5);
	fimc_is_sensor_write(client, 0x720B, 0xF5);
	fimc_is_sensor_write(client, 0x7245, 0xC4);
	fimc_is_sensor_write(client, 0x703B, 0x1A);
	fimc_is_sensor_write(client, 0x7064, 0x43); /* 10 */

	fimc_is_sensor_write(client, 0x7058, 0x6F);
	fimc_is_sensor_write(client, 0x7073, 0x04);
	fimc_is_sensor_write(client, 0x705C, 0x40);
	fimc_is_sensor_write(client, 0x706E, 0xFA);
	fimc_is_sensor_write(client, 0x706D, 0x77);

	fimc_is_sensor_write(client, 0x7070, 0x0F);
	fimc_is_sensor_write(client, 0x7060, 0x07);
	fimc_is_sensor_write(client, 0x7087, 0x00);
	fimc_is_sensor_write(client, 0x7115, 0x01);
	fimc_is_sensor_write(client, 0x7061, 0x40); /* 20 */


	fimc_is_sensor_write(client, 0x4345, 0x08);
	fimc_is_sensor_write(client, 0x4348, 0x07);
	fimc_is_sensor_write(client, 0x4349, 0x83);
	fimc_is_sensor_write(client, 0x434C, 0x01);
	fimc_is_sensor_write(client, 0x434D, 0x40);

	fimc_is_sensor_write(client, 0x4381, 0x01);
	fimc_is_sensor_write(client, 0x4383, 0x05);
	fimc_is_sensor_write(client, 0x4347, 0x08);
	fimc_is_sensor_write(client, 0x434A, 0x04);
	fimc_is_sensor_write(client, 0x434B, 0x3B); /* 30 */

	fimc_is_sensor_write(client, 0x434E, 0x00);
	fimc_is_sensor_write(client, 0x434F, 0xB4);
	fimc_is_sensor_write(client, 0x4385, 0x01);
	fimc_is_sensor_write(client, 0x4387, 0x05);
	fimc_is_sensor_write(client, 0x6029, 0x05);

	fimc_is_sensor_write(client, 0x602A, 0x05);
	fimc_is_sensor_write(client, 0x602B, 0x02);
	fimc_is_sensor_write(client, 0x602C, 0x04);
	fimc_is_sensor_write(client, 0x7454, 0x02);
	fimc_is_sensor_write(client, 0x6014, 0x27); /* 40 */

	fimc_is_sensor_write(client, 0x6015, 0x1D);
	fimc_is_sensor_write(client, 0x6018, 0x01);
#ifdef VISION_30FPS
	fimc_is_sensor_write(client, 0x7460, 0x01);
#else
	fimc_is_sensor_write(client, 0x7460, 0x00);
#endif
	fimc_is_sensor_write(client, 0x5030, 0x06);
	fimc_is_sensor_write(client, 0x5031, 0x01);

	fimc_is_sensor_write(client, 0x600E, 0x05);
	fimc_is_sensor_write(client, 0x503F, 0x02);
	fimc_is_sensor_write(client, 0x503D, 0x20);
	fimc_is_sensor_write(client, 0x503E, 0x70);
	fimc_is_sensor_write(client, 0x7467, 0x20); /* 50 */

	fimc_is_sensor_write(client, 0x7468, 0x20);
	fimc_is_sensor_write(client, 0x7469, 0x20);
	fimc_is_sensor_write(client, 0x746A, 0x20);
	fimc_is_sensor_write(client, 0x746B, 0x20);
	fimc_is_sensor_write(client, 0x746C, 0x20);

	fimc_is_sensor_write(client, 0x7405, 0x28);
	fimc_is_sensor_write(client, 0x7407, 0xC0);
	fimc_is_sensor_write(client, 0x7465, 0x4B);
	fimc_is_sensor_write(client, 0x7461, 0x01);
	fimc_is_sensor_write(client, 0x7462, 0xF8); /* 60 */

	fimc_is_sensor_write(client, 0x7463, 0x1E);
	fimc_is_sensor_write(client, 0x7464, 0x02);
#ifdef VISION_30FPS
	fimc_is_sensor_write(client, 0x5014, 0x08);
	fimc_is_sensor_write(client, 0x5015, 0x33);
#else
	fimc_is_sensor_write(client, 0x5014, 0x1A);
	fimc_is_sensor_write(client, 0x5015, 0x00);
#endif

	fimc_is_sensor_write(client, 0x5016, 0x00);

	fimc_is_sensor_write(client, 0x5017, 0x02);
	fimc_is_sensor_write(client, 0x5035, 0x02);
	fimc_is_sensor_write(client, 0x5036, 0x00);
	fimc_is_sensor_write(client, 0x5037, 0x06);
	fimc_is_sensor_write(client, 0x5038, 0xC0); /* 70 */

	fimc_is_sensor_write(client, 0x5034, 0x00);
	fimc_is_sensor_write(client, 0x746E, 0xFF);
	fimc_is_sensor_write(client, 0x746F, 0x01);
	fimc_is_sensor_write(client, 0x5004, 0x01);
	fimc_is_sensor_write(client, 0x5005, 0x1E);

	fimc_is_sensor_write(client, 0x6040, 0x11);
	fimc_is_sensor_write(client, 0x6041, 0x11);
	fimc_is_sensor_write(client, 0x6042, 0x11);
	fimc_is_sensor_write(client, 0x6043, 0x11);
	fimc_is_sensor_write(client, 0x6044, 0x11); /* 80 */

	fimc_is_sensor_write(client, 0x6045, 0x13);
	fimc_is_sensor_write(client, 0x6046, 0x31);
	fimc_is_sensor_write(client, 0x6047, 0x11);
	fimc_is_sensor_write(client, 0x6048, 0x11);
	fimc_is_sensor_write(client, 0x6049, 0x24);

	fimc_is_sensor_write(client, 0x604A, 0x42);
	fimc_is_sensor_write(client, 0x604B, 0x11);
	fimc_is_sensor_write(client, 0x604C, 0x11);
	fimc_is_sensor_write(client, 0x604D, 0x24);
	fimc_is_sensor_write(client, 0x604E, 0x42); /* 90 */

	fimc_is_sensor_write(client, 0x604F, 0x11);
	fimc_is_sensor_write(client, 0x6050, 0x11);
	fimc_is_sensor_write(client, 0x6051, 0x24);
	fimc_is_sensor_write(client, 0x6052, 0x42);
	fimc_is_sensor_write(client, 0x6053, 0x11);

	fimc_is_sensor_write(client, 0x6054, 0x11);
	fimc_is_sensor_write(client, 0x6055, 0x23);
	fimc_is_sensor_write(client, 0x6056, 0x32);
	fimc_is_sensor_write(client, 0x6057, 0x11);
	fimc_is_sensor_write(client, 0x600A, 0x3A); /* 100 */

	fimc_is_sensor_write(client, 0x7406, 0x28);
	fimc_is_sensor_write(client, 0x746D, 0x04);
	fimc_is_sensor_write(client, 0x7339, 0x03);
#ifdef VISION_30FPS
#else
	fimc_is_sensor_write(client, 0x6027, 0x69);
#endif
	fimc_is_sensor_write(client, 0x4100, 0x01);

	pr_info("[MOD:D:%d] %s(%d)\n", module->id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_8b1_init
};

static int sensor_8b1_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;

	pr_info("%s\n", __func__);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!sensor) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (enable) {
		ret = CALL_MOPS(sensor, stream_on, subdev);
		if (ret) {
			err("s_duration is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = CALL_MOPS(sensor, stream_off, subdev);
		if (ret) {
			err("s_duration is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int sensor_8b1_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u64 duration;

	BUG_ON(!subdev);
	BUG_ON(!param);

	pr_info("%s\n", __func__);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->denominator) {
		err("denominator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	if (!tpf->numerator) {
		err("numerator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	duration = (u64)(tpf->numerator * 1000 * 1000 * 1000) /
					(u64)(tpf->denominator);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!sensor) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_MOPS(sensor, s_duration, subdev, duration);
	if (ret) {
		err("s_duration is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = sensor_8b1_s_stream,
	.s_parm = sensor_8b1_s_param
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int sensor_8b1_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_write(client, 0x4100, 1);
	if (ret) {
		err("fimc_is_sensor_write is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_8b1_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_write(client, 0x4100, 0);
	if (ret) {
		err("fimc_is_sensor_write is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_8b1_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	int ret = 0;
	u8 value[2];
	u32 framerate, result;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	pr_info("%s\n", __func__);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = 1000 * 1000 * 1000 / (u32)duration;
	result = 1060 / framerate;
	value[0] = result & 0xFF;
	value[1] = (result >> 8) & 0xFF;

	fimc_is_sensor_write(client, SENSOR_REG_VIS_DURATION_MSB, value[1]);
	fimc_is_sensor_write(client, SENSOR_REG_VIS_DURATION_LSB, value[0]);

p_err:
	return ret;
}

int sensor_8b1_g_min_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_g_max_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	int ret = 0;
	u8 value;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	pr_info("%s(%d)\n", __func__, (u32)exposure);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	value = exposure & 0xFF;

	fimc_is_sensor_write(client, SENSOR_REG_VIS_AE_TARGET, value);

p_err:
	return ret;
}

int sensor_8b1_g_min_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_g_max_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	return ret;
}

int sensor_8b1_g_min_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_g_max_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_s_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_g_min_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_8b1_g_max_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

struct fimc_is_sensor_ops module_8b1_ops = {
	.stream_on	= sensor_8b1_stream_on,
	.stream_off	= sensor_8b1_stream_off,
	.s_duration	= sensor_8b1_s_duration,
	.g_min_duration	= sensor_8b1_g_min_duration,
	.g_max_duration	= sensor_8b1_g_max_duration,
	.s_exposure	= sensor_8b1_s_exposure,
	.g_min_exposure	= sensor_8b1_g_min_exposure,
	.g_max_exposure	= sensor_8b1_g_max_exposure,
	.s_again	= sensor_8b1_s_again,
	.g_min_again	= sensor_8b1_g_min_again,
	.g_max_again	= sensor_8b1_g_max_again,
	.s_dgain	= sensor_8b1_s_dgain,
	.g_min_dgain	= sensor_8b1_g_min_dgain,
	.g_max_dgain	= sensor_8b1_g_max_dgain
};

int sensor_8b1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_S5K8B1_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* S5K8B1 */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_S5K8B1_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_S5K8B1_INSTANCE;
	module->ops = &module_8b1_ops;
	module->client = client;
	module->active_width = 1920;
	module->active_height = 1080;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 30;
	module->position = SENSOR_POSITION_FRONT;
	module->setfile_name = "setfile_8b1.bin";
	module->cfgs = ARRAY_SIZE(config_8b1);
	module->cfg = config_8b1;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_8b1), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ext = &module->ext;
	ext->mipi_lane_num = 1;
	ext->I2CSclk = I2C_L0;
	ext->sensor_con.product_name = SENSOR_NAME_S5K8B1;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->sensor_con.peri_setting.i2c.slave_address = 0;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

#ifdef SENSOR_S5K8B1_DRIVING
	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
	subdev_module->internal_ops = &internal_ops;
#else
	v4l2_subdev_init(subdev_module, &subdev_ops);
#endif
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

p_err:
	info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_8b1_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_8b1_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-8b1",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_8b1_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_8b1_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_8b1_match
#endif
	},
	.probe	= sensor_8b1_probe,
	.remove	= sensor_8b1_remove,
	.id_table = sensor_8b1_idt
};

static int __init sensor_8b1_load(void)
{
        return i2c_add_driver(&sensor_8b1_driver);
}

static void __exit sensor_8b1_unload(void)
{
        i2c_del_driver(&sensor_8b1_driver);
}

module_init(sensor_8b1_load);
module_exit(sensor_8b1_unload);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor 8B1 driver");
MODULE_LICENSE("GPL v2");
