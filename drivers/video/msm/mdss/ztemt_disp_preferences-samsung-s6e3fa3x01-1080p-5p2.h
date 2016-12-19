#ifndef __ZTE_DISP_PREFERENCES_SAMSUNG_S6E3FA3X01_1080P_5P2_H__
#define __ZTE_DISP_PREFERENCES_SAMSUNG_S6E3FA3X01_1080P_5P2_H__

struct mdp_pcc_cfg_data samsung_s6e3fa3x01_1080p_5p2_pcc_cfg_warm = {
	.block = 0x10,
	.ops = 0x5,
	{
		.c = 0,
		.r = 0x8000,
		.g = 0,
		.b = 0,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
	{
		.c = 0,
		.r = 0,
		.g = 0x8000,
		.b = 0,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
	{
		.c = 0,
		.r = 0,
		.g = 0,
		.b = 0x7c7c,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
};

struct mdp_pcc_cfg_data samsung_s6e3fa3x01_1080p_5p2_pcc_cfg_natural = {
	.block = 0x10,
	.ops = 0x5,
	{
		.c = 0,
		.r = 0x8000,
		.g = 0,
		.b = 0,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
	{
		.c = 0,
		.r = 0,
		.g = 0x8000,
		.b = 0,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
	{
		.c = 0,
		.r = 0,
		.g = 0,
		.b = 0x8000,
		.rr = 0,
		.gg = 0,
		.bb = 0,
		.rg = 0,
		.gb = 0,
		.rb = 0,
		.rgb_0 = 0,
		.rgb_1 = 0
	},
};

struct mdp_pcc_cfg_data samsung_s6e3fa3x01_1080p_5p2_pcc_cfg_cool = {
	.block = 0x10,
	.ops = 0x5,
	{
		.c = 0,
		.r = 0x7d80,
		.g = 0,
		.b = 0,
	},
	{
		.c = 0,
		.r = 0,
		.g = 0x7d76,
		.b = 0,
	},
	{
		.c = 0,
		.r = 0,
		.g = 0,
		.b = 0x8000,
	},
};

/*
static char samsungca_basic_9b[] = {
	0xca, 0x01, 0x80, 0x98, 0x98, 0x9b, 0x40, 0xbe, 0xbe, 0x20, 0x20,
	0x80, 0xfe, 0x0a, 0x4a, 0x37, 0xa0, 0x55, 0xf8, 0x0c, 0x0c, 0x20, 0x10, 0x3f, 0x3f, 0x00,
	0x00, 0x10, 0x10, 0x3f, 0x3f, 0x3f, 0x3f,
};
static struct dsi_cmd_desc samsung_s6e3fa3x01_1080p_5p2_glow[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(samsungca_basic_9b)}, samsungca_basic_9b},
};

static char samsungca_basic_92[] = {
	0xca, 0x01, 0x80, 0x92, 0x92, 0x9b, 0x75, 0x9b, 0x9b, 0x20, 0x20,
	0x80, 0xfe, 0x0a, 0x4a, 0x37, 0xa0, 0x55, 0xf8, 0x0c, 0x0c, 0x20, 0x10, 0x3f, 0x3f, 0x00,
	0x00, 0x10, 0x10, 0x3f, 0x3f, 0x3f, 0x3f,
};
static struct dsi_cmd_desc samsung_s6e3fa3x01_1080p_5p2_standard[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(samsungca_basic_92)}, samsungca_basic_92},
};

static char samsungca_basic_norm[] = {
	0xca, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x08, 0x20,
	0x80, 0x80, 0x0a, 0x4a, 0x37, 0xa0, 0x55, 0xf8, 0x0c, 0x0c, 0x20, 0x10, 0x3f, 0x3f, 0x00,
	0x00, 0x10, 0x10, 0x3f, 0x3f, 0x3f, 0x3f,
};

static struct dsi_cmd_desc samsung_s6e3fa3x01_1080p_5p2_soft[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(samsungca_basic_norm)}, samsungca_basic_norm},
};
*/

#endif
