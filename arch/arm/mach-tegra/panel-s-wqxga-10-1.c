/*
 * arch/arm/mach-tegra/panel-s-wqxga-10-1.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <mach/iomap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/max8831_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra11_host1x_devices.h"

#define TEGRA_DSI_GANGED_MODE	1

#define DSI_PANEL_RESET		1
#define DSI_PANEL_RST_GPIO	TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM	TEGRA_GPIO_PH1

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

#define en_vdd_bl	TEGRA_GPIO_PG0
#define lvds_en		TEGRA_GPIO_PG3


static bool reg_requested;
static bool gpio_requested;
static struct platform_device *disp_device;
static struct regulator *avdd_lcd_3v3;
static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *dvdd_lcd_1v8;
static struct regulator *vdd_ds_1v8;

#define en_vdd_bl	TEGRA_GPIO_PG0
#define lvds_en		TEGRA_GPIO_PG3

static tegra_dc_bl_output dsi_s_wqxga_10_1_bl_output_measured = {
	0, 0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 9, 10, 11, 12, 13,
	13, 14, 15, 16, 17, 17, 18, 19,
	20, 21, 22, 22, 23, 24, 25, 26,
	27, 27, 28, 29, 30, 31, 32, 32,
	33, 34, 35, 36, 37, 37, 38, 39,
	40, 41, 42, 42, 43, 44, 45, 46,
	47, 48, 48, 49, 50, 51, 52, 53,
	54, 55, 56, 57, 57, 58, 59, 60,
	61, 62, 63, 64, 65, 66, 67, 68,
	69, 70, 71, 71, 72, 73, 74, 75,
	76, 77, 77, 78, 79, 80, 81, 82,
	83, 84, 85, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 102, 103, 104, 105, 106, 107,
	108, 109, 110, 111, 112, 113, 115, 116,
	117, 118, 119, 120, 121, 122, 123, 124,
	125, 126, 127, 128, 129, 130, 131, 132,
	133, 134, 135, 136, 137, 138, 139, 141,
	142, 143, 144, 146, 147, 148, 149, 151,
	152, 153, 154, 155, 156, 157, 158, 158,
	159, 160, 161, 162, 163, 165, 166, 167,
	168, 169, 170, 171, 172, 173, 174, 176,
	177, 178, 179, 180, 182, 183, 184, 185,
	186, 187, 188, 189, 190, 191, 192, 194,
	195, 196, 197, 198, 199, 200, 201, 202,
	203, 204, 205, 206, 207, 208, 209, 210,
	211, 212, 213, 214, 215, 216, 217, 219,
	220, 221, 222, 224, 225, 226, 227, 229,
	230, 231, 232, 233, 234, 235, 236, 238,
	239, 240, 241, 242, 243, 244, 245, 246,
	247, 248, 249, 250, 251, 252, 253, 255
};

static struct tegra_dsi_cmd dsi_s_wqxga_10_1_init_cmd[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(2000),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_DLY_MS(100),
};

static struct tegra_dsi_out dsi_s_wqxga_10_1_pdata = {
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	.n_data_lanes = 2,
	.controller_vs = DSI_VS_0,
#else
	.controller_vs = DSI_VS_1,
#endif

	.n_data_lanes = 8,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,
	.ganged_type = TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD,

	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.dsi_instance = DSI_INSTANCE_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.dsi_init_cmd = dsi_s_wqxga_10_1_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_s_wqxga_10_1_init_cmd),
};

static int dalmore_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;
	dvdd_lcd_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR_OR_NULL(dvdd_lcd_1v8)) {
		pr_err("dvdd_lcd regulator get failed\n");
		err = PTR_ERR(dvdd_lcd_1v8);
		dvdd_lcd_1v8 = NULL;
		goto fail;
	}
	avdd_lcd_3v3 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR_OR_NULL(avdd_lcd_3v3)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v3);
		avdd_lcd_3v3 = NULL;
		goto fail;
	}

	vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
	if (IS_ERR_OR_NULL(vdd_lcd_bl)) {
		pr_err("vdd_lcd_bl regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl);
		vdd_lcd_bl = NULL;
		goto fail;
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR_OR_NULL(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int dalmore_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	/* free pwm GPIO */
	err = gpio_request(DSI_PANEL_BL_PWM, "panel pwm");
	if (err < 0) {
		pr_err("panel pwm gpio request failed\n");
		goto fail;
	}
	gpio_free(DSI_PANEL_BL_PWM);
	gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_wqxga_10_1_enable(struct device *dev)
{
	int err = 0;

	err = dalmore_dsi_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}
	err = dalmore_dsi_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	if (vdd_ds_1v8) {
		err = regulator_enable(vdd_ds_1v8);
		if (err < 0) {
			pr_err("vdd_ds_1v8 regulator enable failed\n");
			goto fail;
		}
	}

	if (dvdd_lcd_1v8) {
		err = regulator_enable(dvdd_lcd_1v8);
		if (err < 0) {
			pr_err("dvdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	if (avdd_lcd_3v3) {
		err = regulator_enable(avdd_lcd_3v3);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	msleep(100);
#if DSI_PANEL_RESET
	gpio_direction_output(DSI_PANEL_RST_GPIO, 1);
	usleep_range(1000, 5000);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	msleep(150);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	msleep(1500);
#endif

	return 0;
fail:
	return err;
}

static int dsi_s_wqxga_10_1_disable(void)
{
	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	if (avdd_lcd_3v3)
		regulator_disable(avdd_lcd_3v3);

	if (dvdd_lcd_1v8)
		regulator_disable(dvdd_lcd_1v8);

	if (vdd_ds_1v8)
		regulator_disable(vdd_ds_1v8);

	return 0;
}

static int dsi_s_wqxga_10_1_postsuspend(void)
{
	return 0;
}

static struct tegra_dc_mode dsi_s_wqxga_10_1_modes[] = {
	{
		.pclk = 268460000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 2,
		.h_back_porch = 16,
		.v_back_porch = 33,
		.h_active = 2560,
		.v_active = 1600,
		.h_front_porch = 128,
		.v_front_porch = 10,
	},
};

static int dsi_s_wqxga_10_1_bl_notify(struct device *unused, int brightness)
{
	atomic_t __maybe_unused sd_brightness = ATOMIC_INIT(255);
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = dsi_s_wqxga_10_1_bl_output_measured[brightness];

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	return brightness;
}

static int dsi_s_wqxga_10_1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &disp_device->dev;
}

static struct platform_pwm_backlight_data dsi_s_wqxga_10_1_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 1000000,
	.notify		= dsi_s_wqxga_10_1_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= dsi_s_wqxga_10_1_check_fb,
};

static struct platform_device __maybe_unused
		dsi_s_wqxga_10_1_bl_device __initdata = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &dsi_s_wqxga_10_1_bl_data,
	},
};

static struct platform_device __maybe_unused
			*dsi_s_wqxga_10_1_bl_devices[] __initdata = {
	&tegra_pwfm1_device,
	&dsi_s_wqxga_10_1_bl_device,
};

static int __init dsi_s_wqxga_10_1_register_bl_dev(void)
{
	int err = 0;
	err = platform_add_devices(dsi_s_wqxga_10_1_bl_devices,
				ARRAY_SIZE(dsi_s_wqxga_10_1_bl_devices));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
	return err;
}

static void dsi_s_wqxga_10_1_set_disp_device(
	struct platform_device *dalmore_display_device)
{
	disp_device = dalmore_display_device;
}
/* Sharp is configured in Ganged mode */
static void dsi_s_wqxga_10_1_resources_init(struct resource *
resources, int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsia_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
		if (resource_type(r) == IORESOURCE_MEM &&
			!strcmp(r->name, "ganged_dsib_regs")) {
			r->start = TEGRA_DSIB_BASE;
			r->end = TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1;
		}
	}
}

static void dsi_s_wqxga_10_1_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_s_wqxga_10_1_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_s_wqxga_10_1_modes;
	dc->n_modes = ARRAY_SIZE(dsi_s_wqxga_10_1_modes);
	dc->enable = dsi_s_wqxga_10_1_enable;
	dc->disable = dsi_s_wqxga_10_1_disable;
	dc->postsuspend	= dsi_s_wqxga_10_1_postsuspend,
	dc->width = 216;
	dc->height = 135;
	dc->flags = DC_CTRL_MODE;
}

static void dsi_s_wqxga_10_1_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_s_wqxga_10_1_modes[0].h_active;
	fb->yres = dsi_s_wqxga_10_1_modes[0].v_active;
}

static void
dsi_s_wqxga_10_1_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	settings->bl_device_name = "pwm-backlight";
}

struct tegra_panel __initdata dsi_s_wqxga_10_1 = {
	.init_sd_settings = dsi_s_wqxga_10_1_sd_settings_init,
	.init_dc_out = dsi_s_wqxga_10_1_dc_out_init,
	.init_fb_data = dsi_s_wqxga_10_1_fb_data_init,
	.init_resources = dsi_s_wqxga_10_1_resources_init,
	.register_bl_dev = dsi_s_wqxga_10_1_register_bl_dev,
	.set_disp_device = dsi_s_wqxga_10_1_set_disp_device,
};
EXPORT_SYMBOL(dsi_s_wqxga_10_1);
