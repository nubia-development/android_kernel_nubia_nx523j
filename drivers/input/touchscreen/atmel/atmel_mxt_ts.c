/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011-2012 Atmel Corporation
 * Copyright (C) 2012 Google, Inc.
 *
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/****************************************************************  
	Pitter Liao add for macro for the global platform
		email:  pitter.liao@atmel.com 
		mobile: 13244776877
-----------------------------------------------------------------*/
#define DRIVER_VERSION 0xB485
#define ZTEMT_C_AREA_WIDTH	33
#define ZTEMT_B_AREA_WIDTH	33


/*----------------------------------------------------------------
0.42
1 add self tune support in T102
2 change MXT_MAX_BLOCK_WRITE to 128
3 change T24/T61/T92/T93 wakeup support algorithm
4 change bootloader DMA address
5 delete pm supend if earlysuspend or fb call back support

0.41
1 fixed bugs at alternative config
0.40
1 add fb callback
2 and alternative chip support
0.39
1 t65 support
0.386:
1 fixed bug don't report message when enable_report is disable, which cause crash in update cfg
2 delete call back in pluging force stop
0.385
0.384
1 compatible bootloader mode
0.383
1 t80 support
0.382
1 fixed bugs for deinit plugin at update cfg
0.381
1 add mxt_plugin_hook_reg_init support(support t38 protect)
0.38 
1 fixed bug at deinit
0.37
1 modify plug resume order, independ the check and calibration function
1 support no key project
0.343
0.342:
1 fixed t15 report virtual key

0.34
1 add t15 individual key to virtual key
2 config failed contiue to work

0.33
1 add t55 address
2 do calibration when crc match, this will useful for stable POR calibration 
3 add update fw name check
4 add suspend/resume input_dev check, this is for bootup bootloader mode

0.32
1 add t6 address
2 modify kthread exit contidion
0.31
1 version for simple workaround without debug message
0.3
1 Add plugin support
0.2
1 Add early suspend/resume
2 Add MTK supprot without test
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/kthread.h>

#include "atmel_mxt_ts.h"
#include "atmel_mxt_ts_platform.h"
#if defined(CONFIG_FB_PM)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
//#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
#include "plug.h"
//#endif
#include <linux/wakelock.h>//add by ztemt 20150403

/*** ZTEMT start ,add for fource touch ***/
#if defined(CONFIG_ZTEMT_PRESS_TOUCHSCREEN_CYPRESS)
#include <linux/cyttsp3_ff_api.h>
#include <linux/cyttsp2_ff_api.h>
#endif
/*** ZTEMT end ***/


/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"

/* Registers */
#define MXT_OBJECT_START	0x07
#define MXT_OBJECT_SIZE		6
#define MXT_INFO_CHECKSUM_SIZE	3
#define MXT_MAX_BLOCK_READ	250
#define MXT_MAX_BLOCK_WRITE	128

/* Object types */
//#if !defined(CONFIG_MXT_PLUGIN_SUPPORT)
#if 0
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_SPT_ADAPTIVE_THRESHOLD_T55	55
#define MXT_SPT_TIMER_T61	61
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_PROCI_LENSBENDING_T65	65
#define MXT_PROCG_NOISESUPPRESSION_T72 72
#define MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80 80
#define MXT_TOUCH_MULTITOUCHSCREEN_T100 100
#endif

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* Define for T6 status byte */
#define MXT_T6_STATUS_RESET	(1 << 7)
#define MXT_T6_STATUS_OFL	(1 << 6)
#define MXT_T6_STATUS_SIGERR	(1 << 5)
//#define MXT_T6_STATUS_CAL	(1 << 4)
#define MXT_T6_STATUS_CFGERR	(1 << 3)
#define MXT_T6_STATUS_COMSERR	(1 << 2)

#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_T9_ORIENT		9
#define MXT_T9_RANGE		18

/* MXT_TOUCH_MULTI_T9 status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)

struct t9_range {
	u16 x;
	u16 y;
} __packed;

/* MXT_TOUCH_MULTI_T9 orient */
#define MXT_T9_ORIENT_SWITCH	(1 << 0)

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1
#define MXT_COMMS_RETRIGEN	  (1 << 6)

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55

/* Define for MXT_PROCI_TOUCHSUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* T47 Stylus */
#define MXT_TOUCH_MAJOR_T47_STYLUS	1
#define MXT_TOUCH_MAJOR_T15_VIRTUAL_KEY 2

/* T63 Stylus */
#define MXT_T63_STYLUS_PRESS	(1 << 0)
#define MXT_T63_STYLUS_RELEASE	(1 << 1)
#define MXT_T63_STYLUS_MOVE		(1 << 2)
#define MXT_T63_STYLUS_SUPPRESS	(1 << 3)

#define MXT_T63_STYLUS_DETECT	(1 << 4)
#define MXT_T63_STYLUS_TIP		(1 << 5)
#define MXT_T63_STYLUS_ERASER	(1 << 6)
#define MXT_T63_STYLUS_BARREL	(1 << 7)

#define MXT_T63_STYLUS_PRESSURE_MASK	0x3F

/* Define for NOISE SUPPRESSION T72 */
#define MXT_T72_NOISE_SUPPRESSION_NOISELVCHG	(1 << 4)

/* T100 Multiple Touch Touchscreen */
//#define MXT_T100_CTRL		0
#define MXT_T100_CFG1		1
#define MXT_T100_SCRAUX		2
#define MXT_T100_TCHAUX		3
#define MXT_T100_XRANGE		13
#define MXT_T100_YRANGE		24

#define MXT_T100_CFG_SWITCHXY	(1 << 5)

#define MXT_T100_SCRAUX_NUMTCH	(1 << 0)
#define MXT_T100_SCRAUX_TCHAREA	(1 << 1)
#define MXT_T100_SCRAUX_ATCHAREA	(1 << 2)
#define MXT_T100_SCRAUX_INTTCHAREA		(1 << 3)

#define MXT_T100_TCHAUX_VECT	(1 << 0)
#define MXT_T100_TCHAUX_AMPL	(1 << 1)
#define MXT_T100_TCHAUX_AREA	(1 << 2)
#define MXT_T100_TCHAUX_HW		(1 << 3)
#define MXT_T100_TCHAUX_PEAK	(1 << 4)
#define MXT_T100_TCHAUX_AREAHW	(1 << 5)

#define MXT_T100_DETECT		(1 << 7)
#define MXT_T100_TYPE_MASK	0x70
#define MXT_T100_TYPE_STYLUS	0x20

/* MXT_SPT_SELFCAPHOVERCTECONFIG_T102 */
#define MXT_SELF_CHGTIME	13

/* cmd and message */
#define MXT_SELFCAP_CMD	0x1
#define MXT_SELFCMD_TUNE	0x1
#define MXT_SELFCMD_NVM_TUNE	0x2
#define MXT_SELFCMD_RAM_TUNE	0x3
#define MXT_SELFCMD_RAM_FINE	0x4
#define MXT_SELFCMD_STORE	0x5
#define MXT_SELFCMD_BG_TUNE	0x6

struct mxt_selfcap_status {
	u8 cause;
	u8 error_code;
};

/* Delay times */
#define MXT_BACKUP_TIME		50	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_TIMEOUT	3000	/* msec */
#define MXT_CRC_TIMEOUT		1000	/* msec */
#define MXT_FW_RESET_TIME	3000	/* msec */
#define MXT_FW_CHG_TIMEOUT	30/*300*/	/* msec */
#define MXT_WAKEUP_TIME		25	/* msec */
#define MXT_REGULATOR_DELAY	150	/* msec */
#define MXT_POWERON_DELAY	150	/* msec */

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

#define MXT_PIXELS_PER_MM	20

#define DEBUG_MSG_MAX		200

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size_minus_one;
	u8 instances_minus_one;
	u8 num_report_ids;
} __packed;

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    struct input_dev *input_rim_dev;//ZTEMT add for panel without rim
#endif
	char phys[64];		/* device physical location */
	struct mxt_platform_data *pdata;
	struct mxt_object *object_table;
	struct mxt_info *info;
	void *raw_info_block;
	unsigned int irq;
	atomic_t depth;
	unsigned int max_x;
	unsigned int max_y;
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	unsigned int max_y_t;  //touch max y
#endif
	bool in_bootloader;
	u16 mem_size;

	u8 tchcfg[4];

	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool debug_v2_enabled;
	u8 *debug_msg_data;
	u16 debug_msg_count;
	struct bin_attribute debug_msg_attr;
	struct mutex debug_msg_lock;
	u8 max_reportid;
	u32 config_crc;
	u32 info_crc;
#if defined(CONFIG_MXT_I2C_DMA)
	unsigned short bootloader_addr;
	void *i2c_dma_va;
	dma_addr_t i2c_dma_pa;
#else		
	u8 bootloader_addr;
#endif
	struct mutex bus_access_mutex;

	struct t7_config t7_cfg;
	u8 *msg_buf;
	u8 t6_status;
	bool update_input;
	u8 last_message_count;
	u8 num_touchids;
	u8 num_stylusids;
	unsigned long t15_keystatus;
	bool use_retrigen_workaround;
	bool use_regulator;
	struct regulator *reg_vdd;
	struct regulator *reg_avdd;
	char *fw_name;
	char *cfg_name;
#define ALT_CHIP_BIT_FW  (1<<8)
	unsigned long alt_chip;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u8 T6_reportid;
	u16 T6_address;
	u16 T7_address;
	u16 T8_address;
	u16 T9_address;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u16 T15_address;
	u16 T18_address;
	u16 T19_address;
	u8 T19_reportid;
	u8  t19_msg[1];
	u16 T24_address;
	u8 T24_reportid;
	u16 T25_address;
	u8  T25_reportid;
	u8  t25_msg[6];
	u16 T37_address;
	u16 T38_address;
	u16 T40_address;
	u16 T42_address;
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u16 T46_address;
	u16 T47_address;
	u8 T48_reportid;
	u16 T55_address;
	u16 T56_address;
	u8 T61_reportid_min;
	u8 T61_reportid_max;
	u16 T61_address;
	u16 T61_instances;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u16 T65_address;
	u16 T68_address;
	u8 T68_reportid_min;
	u8 T68_reportid_max;	
	u16 T71_address;
	u16 T72_address;
	u8 T72_reportid_min;
	u8 T72_reportid_max;
	u16 T78_address;
	u16 T80_address;
	u16 T81_address;
	u8 T81_reportid_min;
	u8 T81_reportid_max;
	u16 T92_address;
	u8 T92_reportid;
	u16 T93_address;
	u8 T93_reportid;
	u8 T97_reportid_min;
	u8 T97_reportid_max;
	u16 T96_address;
	u16 T97_address;
	u16 T99_address;
	u8 T99_reportid;
	u16 T100_address;
	u8 T100_reportid_min;
	u8 T100_reportid_max;
	u16 T102_address;
	u8  T102_reportid;
	struct mxt_selfcap_status selfcap_status;
	u16 T104_address;
	u16 T113_address;
	u16 T115_address;
	u8 T115_reportid;

	/* for fw update in bootloader */
	struct completion bl_completion;

	/* for reset handling */
	struct completion reset_completion;

	/* for reset handling */
	struct completion crc_completion;

	/* Enable reporting of input events */
	bool enable_reporting;

#define MXT_WK_ENABLE 1
#define MXT_WK_DETECTED 2
	unsigned long enable_wakeup;

	/* Indicates whether device is in suspend */
	bool suspended;
	
	struct mutex access_mutex;
	
#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
	struct task_struct *irq_tsk;
	wait_queue_head_t wait;
#endif
#define MXT_EVENT_IRQ 1
#define MXT_EVENT_EXTERN 2
#define MXT_EVENT_IRQ_FLAG 5
	unsigned long busy;

#if defined(CONFIG_FB_PM)
	struct notifier_block fb_notif;
#if defined(NUBIA_DEFERRED_RESUME_TP)
	struct work_struct nubia_tp_resume_work;
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	struct plug_interface plug;
#endif

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	struct kobject *properties_kobj;
#endif

    struct wake_lock wake_lock;//add by ztemt 20150403
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    char c_zone_flag;
#endif
};

/*** ZTEMT Add, 2015/01/09 ***/
enum {
	ZONE_DEFAULT = 0,
    ZONE_A,
    ZONE_B,
    ZONE_C,
};

enum {
	IGNORE_TURN_A = 0,
    IGNORE_TURN_C,
    IGNORE_TURN_A_TO_C,
    IGNORE_TURN_C_TO_A,
};


#if 0//def CONFIG_ZTEMT_HW_VERSION
extern int ztemt_get_hw_id(void);
#endif
/*** ZTEMT end ***/

#if defined(CONFIG_FB_PM)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data);

#if defined(NUBIA_DEFERRED_RESUME_TP)
static void nubia_tp_resume_work_func(struct work_struct *work);
#endif

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void mxt_early_suspend(struct early_suspend *es);
static void mxt_late_resume(struct early_suspend *es);
#endif

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static int mxt_set_reset(void *pl_dev, int por);
static void mxt_plugin_pre_process(struct plug_interface *pl, bool in_boot);
static long mxt_plugin_post_process(struct plug_interface *pl, bool in_boot);
static void mxt_plugin_hook_t6(struct plug_interface *pl, u8 status);
static int mxt_plugin_hook_t9(struct plug_interface *pl, int id, int x, int y, u8 *info);
//static int mxt_plugin_hook_t24(struct plug_interface *pl, u8* msg);
static int mxt_plugin_hook_t42(struct plug_interface *pl, u8 status);
static void mxt_plugin_hook_t61(struct plug_interface *pl, int id, u8 status);
static void mxt_plugin_hook_t68(struct plug_interface *pl, u8* msg);
static void mxt_plugin_hook_t72(struct plug_interface *pl, u8* msg);
static int mxt_plugin_get_pid_name(struct plug_interface *pl, char *name, int len);

static int mxt_plugin_hook_gesture_msg(struct plug_interface *pl, u8 type, u8 *msg);

static int mxt_plugin_hook_reg_init(struct plug_interface *pl, u8 *config_mem, size_t config_mem_size, int cfg_start_ofs);
static int mxt_plugin_hook_reg_access(struct plug_interface *pl, u16 addr, u16 reg, u16 len, const void *val, unsigned long flag, int result, bool is_w);
static void mxt_plugin_hook_reset_slots(struct plug_interface *pl);
static int mxt_plugin_hook_t100(struct plug_interface *pl, int id, int x, int y, struct  ext_info *in);
static int mxt_plugin_hook_t100_scraux(struct plug_interface *pl, struct scr_info *in);
static int mxt_plugin_hook_set_t7(struct plug_interface *pl, bool sleep);
static int mxt_plugin_cal_t37_check_and_calibrate(struct plug_interface *pl, bool check_sf, bool resume);
static void mxt_plugin_thread_stopped(struct plug_interface *pl);
static int mxt_plugin_force_stop(struct plug_interface *pl);
static int mxt_plugin_wakeup_enable(struct plug_interface *pl);
//static int mxt_plugin_wakeup_disable(struct plug_interface *pl);
static int mxt_plugin_start(struct plug_interface *pl,bool resume);
static int mxt_plugin_stop(struct plug_interface *pl,bool suspend);
static int mxt_plugin_pre_init(struct mxt_data *data);
static int mxt_plugin_init(struct mxt_data *data);
static void mxt_plugin_deinit(struct plug_interface *pl);
static ssize_t mxt_plugin_show(struct device *dev,	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_store(struct device *dev,	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_tag_show(struct device *dev, struct device_attribute *attr, char *buf);
/*** ZTEMT start,cut down UID detect ***/
//static ssize_t mxt_plugin_tag_uid_show(struct device *dev, struct device_attribute *attr, char *buf);//add by ZTE ,20150414
/*ZTEMT end*/
#	if defined(CONFIG_MXT_CAL_WORKAROUND)
static ssize_t mxt_plugin_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
#	endif

#	if defined(CONFIG_MXT_PI_WORKAROUND)
static ssize_t mxt_plugin_glove_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_glove_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_stylus_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_stylus_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_wakeup_gesture_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_wakeup_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_touch_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_touch_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
static ssize_t mxt_c_zone_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_c_zone_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
#endif
static ssize_t mxt_plugin_gesture_list_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_gesture_list_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_gesture_trace_show(struct device *dev,
	struct device_attribute *attr, char *buf);
#	endif
#	if defined(CONFIG_MXT_MISC_WORKAROUND)
static ssize_t mxt_plugin_misc_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_misc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
#	endif
#	if defined(CONFIG_MXT_CLIP_WORKAROUND)
static ssize_t mxt_plugin_clip_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mxt_plugin_clip_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mxt_plugin_clip_tag_show(struct device *dev,
	struct device_attribute *attr, char *buf);
#	endif
#endif
/*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
static int zte_ignore_zone(int point_id, int x, int y);

struct slot{
    char slot_zoneid;//C 1,A 0,AtoC 2,CtoA 3
    //int slot_X0;
    //int slot_Y0;
};
struct slot slot_ignore[10];
#endif
/*zte end*/

/*** ZTEMT start ***/
int mxt_rst_number;
int mxt_int_number;
struct atmel_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};
static struct atmel_pinctrl_info atmel_pctrl;

static int atmel_pinctrl_init(struct device *dev)
{
	atmel_pctrl.pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR_OR_NULL(atmel_pctrl.pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	atmel_pctrl.gpio_state_active = pinctrl_lookup_state(
					       atmel_pctrl.pinctrl,
					       ATMEL_PINCTRL_STATE_DEFAULT);

	if (IS_ERR_OR_NULL(atmel_pctrl.gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	atmel_pctrl.gpio_state_suspend = pinctrl_lookup_state(
						atmel_pctrl.pinctrl,
						ATMEL_PINCTRL_STATE_SLEEP);

	if (IS_ERR_OR_NULL(atmel_pctrl.gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}


#if defined(CONFIG_FB_PM)
#if defined(NUBIA_DEFERRED_RESUME_TP)
static int mxt_resume(struct device *dev);
static void nubia_tp_resume_work_func(struct work_struct *work)
{
      struct mxt_data *mxt = container_of(work, struct mxt_data, nubia_tp_resume_work);
#if defined(CONFIG_ZTEMT_PRESS_TOUCHSCREEN_CYPRESS)
	id_number = 0;
#endif
       dev_info(&mxt->client->dev, "%s: ztemt atmel resume work func\n", __func__);
	if (mxt_resume(&mxt->client->dev) != 0)
		dev_err(&mxt->client->dev, "%s: failed\n", __func__);
}
#endif
#endif
/*ZTEMT end*/


static inline int mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size_minus_one + 1;
}

static inline int mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances_minus_one + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	/*
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_GEN_DATASOURCE_T53:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GESTURE_T92:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
	*/
		return true;
	default:
		return false;
	}
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
			   message, data->T5_msg_size, false);
}

static void mxt_debug_msg_enable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (data->debug_v2_enabled)
		return;

	mutex_lock(&data->debug_msg_lock);

	data->debug_msg_data = kcalloc(DEBUG_MSG_MAX,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->debug_msg_data) {
		dev_err(&data->client->dev, "Failed to allocate buffer\n");
		return;
	}

	data->debug_v2_enabled = true;
	mutex_unlock(&data->debug_msg_lock);

	dev_info(dev, "Enabled message output\n");
}

static void mxt_debug_msg_disable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_v2_enabled)
		return;

	dev_info(dev, "disabling message output\n");
	data->debug_v2_enabled = false;

	mutex_lock(&data->debug_msg_lock);
	kfree(data->debug_msg_data);
	data->debug_msg_data = NULL;
	data->debug_msg_count = 0;
	mutex_unlock(&data->debug_msg_lock);
	dev_info(dev, "Disabled message output\n");
}

static void mxt_debug_msg_add(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return;
	}
	mutex_lock(&data->debug_msg_lock);

	if (data->debug_msg_count < DEBUG_MSG_MAX) {
		memcpy(data->debug_msg_data + data->debug_msg_count * data->T5_msg_size,
			   msg,
			   data->T5_msg_size);
		data->debug_msg_count++;
	} else {
		dev_dbg(dev, "Discarding %u messages\n", data->debug_msg_count);
		data->debug_msg_count = 0;
	}

	mutex_unlock(&data->debug_msg_lock);

	sysfs_notify(&data->client->dev.kobj, NULL, "debug_notify");
}

static ssize_t mxt_debug_msg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	return -EIO;
}

static ssize_t mxt_debug_msg_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t bytes)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	size_t bytes_read;

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return 0;
	}

	mutex_lock(&data->debug_msg_lock);

	count = bytes / data->T5_msg_size;

	if (count > DEBUG_MSG_MAX)
		count = DEBUG_MSG_MAX;

	if (count > data->debug_msg_count)
		count = data->debug_msg_count;

	bytes_read = count * data->T5_msg_size;

	memcpy(buf, data->debug_msg_data, bytes_read);
	data->debug_msg_count = 0;

	mutex_unlock(&data->debug_msg_lock);

	return bytes_read;
}

static int mxt_debug_msg_init(struct mxt_data *data)
{
	sysfs_bin_attr_init(&data->debug_msg_attr);
	data->debug_msg_attr.attr.name = "debug_msg";
	data->debug_msg_attr.attr.mode = 0666;
	data->debug_msg_attr.read = mxt_debug_msg_read;
	data->debug_msg_attr.write = mxt_debug_msg_write;
	data->debug_msg_attr.size = data->T5_msg_size * DEBUG_MSG_MAX;

	if (sysfs_create_bin_file(&data->client->dev.kobj,
				  &data->debug_msg_attr) < 0) {
		dev_err(&data->client->dev, "Failed to create %s\n",
			data->debug_msg_attr.attr.name);
		return -EINVAL;
	}

	return 0;
}

static void mxt_debug_msg_remove(struct mxt_data *data)
{
	if (data->debug_msg_attr.attr.name)
		sysfs_remove_bin_file(&data->client->dev.kobj,
					  &data->debug_msg_attr);
}

static int mxt_wait_for_completion(struct mxt_data *data,
			struct completion *comp, unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		dev_err(dev, "Wait for completion interrupted.\n");
		return -EINTR;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion timed out.\n");
		return -ETIMEDOUT;
	}
	return 0;
}

#define I2C_ACCESS_R_REG_FIXED   (1 << 0)

#define I2C_ACCESS_NO_REG   (1 << 4)
#define I2C_ACCESS_NO_CACHE   (1 << 5)

static int __mxt_cache_read(struct i2c_client *client,u16 addr, 
				u16 reg, u16 len, void *val, u8 *r_cache, u8 *r_cache_pa, u8 *w_cache, u8 *w_cache_pa, unsigned long flag)
{
	struct i2c_msg *msgs;
	int num;

	struct i2c_msg xfer[2];
	char buf[2];
	u16 transferred;
	int retry = 3;
	int ret;

	if (test_flag(I2C_ACCESS_NO_CACHE,&flag)) {
		w_cache = w_cache_pa = buf;
		r_cache = r_cache_pa = val;
	}

	if (test_flag(I2C_ACCESS_NO_REG,&flag)) {
		msgs = &xfer[1];
		num = 1;
	}else{
		w_cache[0] = reg & 0xff;
		w_cache[1] = (reg >> 8) & 0xff;

		msgs = &xfer[0];
		num = ARRAY_SIZE(xfer);

		/* Write register */
		xfer[0].addr = addr;
		xfer[0].flags = 0;
		xfer[0].len = 2;
		xfer[0].buf = w_cache_pa;
	}

	/* Read data */
	xfer[1].addr = addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].buf = r_cache_pa;

#if defined(CONFIG_MXT_I2C_EXTFLAG)
	xfer[1].ext_flag = xfer[0].ext_flag = client->addr & 0xff00;
	xfer[1].timing = xfer[0].timing = 100;
	dev_dbg(&client->dev, "%s: i2c transfer(r)  (addr %x extflag %x) reg %d len %d\n",
		__func__, client->addr, xfer[0].ext_flag, reg, len);
#endif

	transferred = 0;
	while(transferred < len) {
		if (!test_flag(I2C_ACCESS_NO_REG | I2C_ACCESS_R_REG_FIXED,&flag)) {
			w_cache[0] = (reg +  transferred) & 0xff;
			w_cache[1] = ((reg + transferred) >> 8) & 0xff;
		}

		if (test_flag(I2C_ACCESS_NO_CACHE,&flag))
			xfer[1].buf = r_cache_pa + transferred;
		xfer[1].len = len - transferred;
		if (xfer[1].len > MXT_MAX_BLOCK_READ)
			xfer[1].len = MXT_MAX_BLOCK_READ;
	retry_read:
		ret = i2c_transfer(client->adapter, msgs, num);
		if (ret != num) {
			if (retry) {
				dev_dbg(&client->dev, "%s: i2c transfer(r) retry, reg %d\n", __func__, reg);
				msleep(MXT_WAKEUP_TIME);
				retry--;
				goto retry_read;
			} else {
				dev_err(&client->dev, "%s: i2c transfer(r) failed (%d) reg %d len %d transferred %d\n",
					__func__, ret, reg, len, transferred);
				return -EIO;
			}
		}
		if (!test_flag(I2C_ACCESS_NO_CACHE,&flag))
			memcpy(val + transferred, r_cache, xfer[1].len);
		transferred += xfer[1].len;

#if (DBG_LEVEL > 1)
		dev_dbg(&client->dev, "[mxt] i2c transfer(r) reg %d len %d current %d transferred %d\n",
			reg, len, xfer[1].len, transferred);
		print_hex_dump(KERN_DEBUG, "[mxt] r:", DUMP_PREFIX_NONE, 16, 1,
					test_flag(I2C_ACCESS_NO_CACHE,&flag) ? xfer[1].buf : r_cache, xfer[1].len, false);
#endif
	}

	return 0;
}

static int __mxt_cache_write(struct i2c_client *client,u16 addr, 
				u16 reg, u16 len, const void *val, u8 *w_cache, u8 *w_cache_pa, unsigned long flag)
{
	struct i2c_msg xfer;
	void *buf = NULL;
	u16 transferred,extend;
	int retry = 3;
	int ret=0;

	if (test_flag(I2C_ACCESS_NO_REG,&flag)) {
		extend = 0;
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag))
			w_cache = w_cache_pa = (u8 *)val;
	}else {
		extend = 2;
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag)) {
			buf = kmalloc( len + extend, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;
			w_cache = w_cache_pa = buf;
		}

		w_cache[0] = reg & 0xff;
		w_cache[1] = (reg >> 8) & 0xff;
	}

	/* Write register */
	xfer.addr = addr;
	xfer.flags = 0;
	xfer.buf = w_cache_pa;

#if defined(CONFIG_MXT_I2C_EXTFLAG)
	xfer.ext_flag = client->addr & 0xff00;
	xfer.timing = 100;
	dev_dbg(&client->dev, "%s: i2c transfer(w) (addr %x extflag %x) reg %d len %d\n",
		__func__, client->addr , xfer.ext_flag, reg, len);
#endif

	transferred = 0;
	while(transferred < len) {
		xfer.len = len - transferred+ extend;
		if (xfer.len> MXT_MAX_BLOCK_WRITE)
			xfer.len = MXT_MAX_BLOCK_WRITE;
	
		if (test_flag(I2C_ACCESS_NO_CACHE,&flag) &&
			test_flag(I2C_ACCESS_NO_REG,&flag))
			xfer.buf = w_cache_pa + transferred;
		else
			memcpy(w_cache + extend, val + transferred, xfer.len - extend);

		if (extend) {
			w_cache[0] = (reg +  transferred) & 0xff;
			w_cache[1] = ((reg + transferred) >> 8) & 0xff;
		}

	retry_write:
		ret = i2c_transfer(client->adapter, &xfer, 1);
		if (ret != 1) {
			if (retry) {
				dev_dbg(&client->dev, "%s: i2c transfer(w) retry, reg %d\n", __func__, reg);
				msleep(MXT_WAKEUP_TIME);
				retry--;
				goto retry_write;
			} else {
				dev_err(&client->dev, "%s: i2c transfer(w) failed (%d) reg %d len %d transferred %d\n",
					__func__, ret, reg, len, transferred);
				if (buf)
					kfree(buf);
				return -EIO;
			}
		}

		transferred += xfer.len -extend;
#if (DBG_LEVEL > 1)
		dev_dbg(&client->dev, "[mxt] i2c transfer(w) reg %d len %d current %d transferred %d\n",
			reg, len, xfer.len -extend, transferred);
		print_hex_dump(KERN_DEBUG, "[mxt] w:", DUMP_PREFIX_NONE, 16, 1,
					test_flag(I2C_ACCESS_NO_CACHE,&flag) ? xfer.buf : w_cache, xfer.len, false);
#endif
	}

	if (buf)
		kfree(buf);
	return 0;
}

static int __mxt_read_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len,
				void *val, unsigned long flag)
{
	struct mxt_data *data = i2c_get_clientdata(client);
	u8 *r_cache,*r_cache_pa,*w_cache,*w_cache_pa;
	int ret;

#if defined(CONFIG_MXT_I2C_DMA)	
	r_cache = data->i2c_dma_va;
	r_cache_pa = (void *)data->i2c_dma_pa;

	w_cache = data->i2c_dma_va + PAGE_SIZE;
	w_cache_pa = (void *)data->i2c_dma_pa + PAGE_SIZE;
#else
	r_cache_pa = r_cache = NULL;
	w_cache_pa = w_cache = NULL;

	flag |= I2C_ACCESS_NO_CACHE;
#endif

	mutex_lock(&data->bus_access_mutex);
	ret = __mxt_cache_read(client, addr, reg, len, val, r_cache, r_cache_pa, w_cache, w_cache_pa, flag);
	mutex_unlock(&data->bus_access_mutex);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reg_access(&data->plug, addr, reg, len, val, flag, ret, false);
#endif
	return ret;
}


static int __mxt_write_reg_ext(struct i2c_client *client, u16 addr, u16 reg, u16 len,
				const void *val, unsigned long flag)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	u8 *w_cache,*w_cache_pa;
	int ret;

#if defined(CONFIG_MXT_I2C_DMA)	
	w_cache = data->i2c_dma_va + PAGE_SIZE;
	w_cache_pa = (void *)data->i2c_dma_pa + PAGE_SIZE;
#else
	w_cache_pa = w_cache = NULL;

	flag |= I2C_ACCESS_NO_CACHE;
#endif
	mutex_lock(&data->bus_access_mutex);
	ret = __mxt_cache_write(client, addr, reg, len, val, w_cache, w_cache_pa, flag);
	mutex_unlock(&data->bus_access_mutex);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reg_access(&data->plug, addr, reg, len, val, flag, ret, true);
#endif
	return ret;
}

static int __mxt_read_reg(struct i2c_client *client, u16 reg, u16 len,
				void *val)
{
	return __mxt_read_reg_ext(client, client->addr, reg, len, val, 0);
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
				const void *val)
{
	return __mxt_write_reg_ext(client, client->addr, reg, len, val, 0);
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static int mxt_bootloader_read(struct mxt_data *data, u8 *val, unsigned int count)
{
	struct i2c_client *client = data->client;

	return __mxt_read_reg_ext(client, data->bootloader_addr, 0, count, val, I2C_ACCESS_NO_REG);
}

static int mxt_bootloader_write(struct mxt_data *data, const u8 * const val,
	unsigned int count)
{
	struct i2c_client *client = data->client;

	return __mxt_write_reg_ext(client, data->bootloader_addr, 0, count, val, I2C_ACCESS_NO_REG);
}


static int mxt_lookup_bootloader_address(struct mxt_data *data, bool retry)
{
	u8 appmode = data->client->addr & 0x7F;
	u8 bootloader;
	u8 family_id = 0;

	if (data->info)
		family_id = data->info->family_id;

	switch (appmode) {
	case 0x4a:
	case 0x4b:
		/* Chips after 1664S use different scheme */
		if (retry || family_id >= 0xa2) {
			bootloader = appmode - 0x24;
			break;
		}
		/* Fall through for normal case */
	case 0x4c:
	case 0x4d:
	case 0x5a:
	case 0x5b:
		bootloader = appmode - 0x26;
		break;
	default:
		dev_err(&data->client->dev,
			"Appmode i2c address 0x%02x not found\n",
			appmode);
		return -EINVAL;
	}

	data->bootloader_addr = bootloader;
#if defined(CONFIG_MXT_I2C_DMA)	
	data->bootloader_addr |= I2C_RS_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG;
#endif
	dev_dbg(&data->client->dev,
			"Appmode i2c address 0x%02x, bootloader 0x%02x\n",appmode,bootloader);
	return 0;
}

static int mxt_probe_bootloader(struct mxt_data *data, bool retry)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;

	ret = mxt_lookup_bootloader_address(data, retry);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
			val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state,
				bool wait)
{
	struct device *dev = &data->client->dev;
	u8 val;
	int ret;

recheck:
	if (wait) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		ret = mxt_wait_for_completion(data, &data->bl_completion,
						  MXT_FW_CHG_TIMEOUT);
		if (ret) {
			/*
			 * TODO: handle -EINTR better by terminating fw update
			 * process before returning to userspace by writing
			 * length 0x000 to device (iff we are in
			 * WAITING_FRAME_DATA state).
			 */
			dev_err(dev, "Update wait error %d\n", ret);
			// don't return false if there is interrupt issue
			//return ret;
		}
	}

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	if (state == MXT_WAITING_BOOTLOAD_CMD)
		val = mxt_get_bootloader_version(data, val);

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK) {
			goto recheck;
		} else if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader state %02X != %02X\n",
			val, state);
		return -EINVAL;
	}

	return 0;
}

static int mxt_send_bootloader_cmd(struct mxt_data *data, bool unlock)
{
	int ret;
	u8 buf[2];

	if (unlock) {
		buf[0] = MXT_UNLOCK_CMD_LSB;
		buf[1] = MXT_UNLOCK_CMD_MSB;
	} else {
		buf[0] = 0x01;
		buf[1] = 0x01;
	}

	ret = mxt_bootloader_write(data, buf, 2);
	if (ret)
		return ret;

	return 0;
}

#if defined(CONFIG_MXT_PROBE_ALTERNATIVE_CHIP)
static unsigned short mxt_lookup_chip_address(struct mxt_data *data, int retry)
{
	unsigned short address = data->client->addr & 0x7F;  //7 bit address

	if (retry && !(retry & 0x1)) {
		address++;
	}

	dev_err(&data->client->dev, "[mxt] try %d chip address 0x%x\n",retry,address);

	return address;
}
#endif

static struct mxt_object *
mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	if (!data->object_table)
		return NULL;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_warn(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
}

static void mxt_reset_slots(struct mxt_data *data);

static int mxt_proc_gesture_messages(struct mxt_data *data, u8 type, u8 key, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	
	u8 num_keys;
	const unsigned int *keymap;
	int idx = -EINVAL;
	
	/* do not report events if input device not yet registered */
	if (!input_dev)
		return -ENODEV;

	if (!data->pdata->keymap || !data->pdata->num_keys)
		return -ENODEV;

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	idx = mxt_plugin_hook_gesture_msg(&data->plug, type, msg);
#endif
	num_keys =data->pdata->num_keys[key];
	keymap = data->pdata->keymap[key];
	if (idx >= 0 && num_keys) {
		if(idx >= num_keys)
			idx = num_keys - 1;
		if (idx >= 0 && idx < num_keys) {
			input_event(input_dev, EV_KEY,keymap[idx], 1);
			input_sync(input_dev);
			input_event(input_dev, EV_KEY,keymap[idx], 0);
			input_sync(input_dev);

			return 0;
		}else{
			dev_err(dev, "T%d discard unused key %d\n",
				type,
				idx);
		}
	}

	return -EINVAL;
}
static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_dbg(dev, "T6 Config Checksum: 0x%06X\n", crc);
	}

	complete(&data->crc_completion);

	/* Detect transition out of reset */
	if (data->t6_status & MXT_T6_STATUS_RESET) {
		if (!(status & MXT_T6_STATUS_RESET)) {
		complete(&data->reset_completion);
		}
		data->enable_wakeup = 0;
	}

	/* Output debug if status has changed */
	if (status != data->t6_status) {
		dev_info(dev, "T6 Status 0x%02X%s%s%s%s%s%s%s\n",
			status,
			(status == 0) ? " OK" : "",
			(status & MXT_T6_STATUS_RESET) ? " RESET" : "",
			(status & MXT_T6_STATUS_OFL) ? " OFL" : "",
			(status & MXT_T6_STATUS_SIGERR) ? " SIGERR" : "",
			(status & MXT_T6_STATUS_CAL) ? " CAL" : "",
			(status & MXT_T6_STATUS_CFGERR) ? " CFGERR" : "",
			(status & MXT_T6_STATUS_COMSERR) ? " COMSERR" : "");

		if (status & MXT_T6_STATUS_CAL)
			mxt_reset_slots(data); //release all points in calibration for safe

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t6(&data->plug, status);
#endif
	}
	/* Save current status */
	data->t6_status = status;
}

#if defined(CONFIG_MXT_VENDOR_ID_BY_T19)	
static void mxt_proc_t19_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	/* Output debug if status has changed */
	dev_info(dev, "T19 Status 0x%x\n",
		status);

	/* Save current status */
	memcpy(&data->t19_msg[0], &msg[1], sizeof(data->t19_msg));
}
#endif

static void mxt_proc_t24_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	int ret;

	dev_info(dev, "T24 Status 0x%x Info: %x %x %x %x %x %x\n",
		status,
		msg[2],
		msg[3],
		msg[4],
		msg[5],
		msg[6],
		msg[7]);
	
	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		ret = mxt_proc_gesture_messages(data, MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24,
			T24_KEY, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key message, ret %d\n", ret);
	}
}

static void mxt_proc_t25_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	/* Output debug if status has changed */
	dev_info(dev, "T25 Status 0x%x Info: %x %x %x %x %x\n",
		status,
		msg[2],
		msg[3],
		msg[4],
		msg[5],
		msg[6]);

	/* Save current status */
	memcpy(&data->t25_msg[0], &msg[1], sizeof(data->t25_msg));
}

static int mxt_t19_command(struct mxt_data *data, bool enable, bool wait)
{
	u16 reg;
	int timeout_counter = 0;
	int ret;
	u8  val[1];

	reg = data->T19_address;
	val[0] = 60;

	ret = __mxt_write_reg(data->client, reg + 3, sizeof(val), val);
	if (ret)
		return ret;

	val[0] = 7;
	ret = __mxt_write_reg(data->client, reg, sizeof(val), val);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(MXT_WAKEUP_TIME);
		ret = __mxt_read_reg(data->client, reg, 1, &val[0]);
		if (ret)
			return ret;
	} while ((val[0] & 0x4) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static ssize_t mxt_t19_gpio_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%02x (GPIO 0x%02x)\n",
			 data->t19_msg[0],
			 (data->t19_msg[0]>>2) & 0xf);
}

static void mxt_reinit_t15_key_order(struct mxt_data *data);
static ssize_t mxt_t19_gpio_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 cmd;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	char suffix[16];
	int ret;
#endif

	if (sscanf(buf, "%hhd", &cmd) == 1) {
		if (cmd == 0) {
			data->alt_chip = 0;
			return count;
		}else if (cmd == 1) {
			if (mxt_t19_command(data,!!cmd,1) == 0) {
				data->alt_chip = cmd;
				return count;
			}
			dev_dbg(dev, "mxt_t19_cmd_store write cmd %x error\n",cmd);
		}else if (cmd == 2){
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			suffix[0] = '\0';
			ret = mxt_plugin_get_pid_name(&data->plug, suffix, sizeof(suffix));
			if (ret == 0) {
				if (suffix[0] != '\0') {
					data->alt_chip = cmd;
					scnprintf(data->plug.suffix_pid_name, sizeof(data->plug.suffix_pid_name), "%s", suffix);
				}
			}
			mxt_reinit_t15_key_order(data);
#endif
		}
		return -EINVAL;
	} else {
		dev_dbg(dev, "mxt_t19_cmd_store write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_irq_depth_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "depth %d\n",
				 atomic_read(&data->depth));
}

static ssize_t mxt_irq_depth_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int cmd;

	if (sscanf(buf, "%d", &cmd) == 1) {
		atomic_set(&data->depth,cmd);
	}

	return count;
}

static int mxt_t25_command(struct mxt_data *data, u8 cmd, bool wait)
{
	u16 reg;
	int timeout_counter = 0;
	int ret;
	u8  val[2];

	reg = data->T25_address;
	val[0] = 0x3;
	val[1] = cmd;

	ret = __mxt_write_reg(data->client, reg, sizeof(val), val);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(MXT_WAKEUP_TIME);
		ret = __mxt_read_reg(data->client, reg + 1, 1, &val[1]);
		if (ret)
			return ret;
	} while ((val[1] != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_t25_selftest_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t offset = 0;

	if (data->t25_msg[0] == 0xFE)
		offset += scnprintf(buf, PAGE_SIZE, "0\n");//PASS
	else
		offset += scnprintf(buf, PAGE_SIZE, "1\n");//FAILED

	offset += scnprintf(buf + offset, PAGE_SIZE, "%x %x %x %x %x %x\n",
		 data->t25_msg[0],
		 data->t25_msg[1],
		 data->t25_msg[2],
		 data->t25_msg[3],
		 data->t25_msg[4],
		 data->t25_msg[5]);
	return offset;
}

static ssize_t mxt_t25_selftest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u32 cmd;

	if (sscanf(buf, "%x", &cmd) == 1) {
		if (mxt_t25_command(data,(u8)cmd,1) == 0)
			return count;
			
		dev_dbg(dev, "mxt_t25_cmd_store write cmd %x error\n",cmd);
		return -EINVAL;
	} else {
		dev_dbg(dev, "mxt_t25_cmd_store write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;
#endif
	u32 cmd;
	const char *command_list[] = {
		"# por",
		"# reset",
		"msc ptc tune 3",
	};

	dev_info(dev, "[mxt]%s\n",buf);

	if (sscanf(buf, "%x", &cmd) >= 1) {
		//dev_info(dev, "[mxt] cmd %d (%d): %s\n",cmd,ARRAY_SIZE(command_list),command_list[cmd]);
		if (cmd >=0 && cmd < ARRAY_SIZE(command_list)) {
			if (cmd == 0) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
				mxt_set_reset(&data->plug, 1);
#endif
				return count;
			}if (cmd == 1) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
				mxt_set_reset(&data->plug, 0);
#endif
			}else {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
				ret = mxt_plugin_store(dev, attr, command_list[cmd], strlen(command_list[cmd]));
				if (ret > 0)
					return count;
				return ret;
#endif
			}
		}
		return -EINVAL;
	} else {
		dev_dbg(dev, "mxt_t19_cmd_store write error\n");
		return -EINVAL;
	}
}

#if !defined(CONFIG_MXT_VENDOR_ID_BY_T19)	
static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct input_dev *input_dev = data->input_dev;
	const struct mxt_platform_data *pdata = data->pdata;
	bool button;
	int i;

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	/* Active-low switch */
	for (i = 0; i < pdata->t19_num_keys; i++) {
		if (pdata->t19_keymap[i] == KEY_RESERVED)
			continue;
		button = !(message[1] & (1 << i));
		input_report_key(input_dev, pdata->t19_keymap[i], button);
	}
}
#endif

static void mxt_input_sync(struct input_dev *input_dev)
{
	if (!input_dev) {
		printk(KERN_ERR "mxt_input_sync input dev is NULL!\n");
		return;
	}
	
	input_mt_report_pointer_emulation(input_dev, false);
	input_sync(input_dev);
}

static void mxt_proc_t9_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int tool;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	u8 info[4];
	int ret;
#endif

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T9_reportid_min;
	status = message[1];
	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	/* Handle 10/12 bit switching */
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;

	area = message[5];

	amplitude = message[6];
	vector = message[7];

	dev_dbg(dev,
		"[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n",
		id,
		(status & MXT_T9_DETECT) ? 'D' : '.',
		(status & MXT_T9_PRESS) ? 'P' : '.',
		(status & MXT_T9_RELEASE) ? 'R' : '.',
		(status & MXT_T9_MOVE) ? 'M' : '.',
		(status & MXT_T9_VECTOR) ? 'V' : '.',
		(status & MXT_T9_AMP) ? 'A' : '.',
		(status & MXT_T9_SUPPRESS) ? 'S' : '.',
		(status & MXT_T9_UNGRIP) ? 'U' : '.',
		x, y, area, amplitude, vector);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		info[MSG_T9_T100_STATUS] = status;
		info[MSG_T9_T100_AREA] = (u8)area;
		info[MSG_T9_T100_AMP] = (u8)amplitude;
		info[MSG_T9_T100_VEC] = (u8)vector;
		ret = mxt_plugin_hook_t9(&data->plug, id, x, y, info);
		if (ret) {
			dev_info(dev,
				"[%u] status:%02X x:%u y:%u area:%02X amp:%02X vec:%02X skip report\n",
				id,
				status,
				x, y,
				info[1] ,
				info[2],
				info[3]);
			status = info[MSG_T9_T100_STATUS];
		}
#endif
	input_mt_slot(input_dev, id);

	if (status & MXT_T9_DETECT) {
		/* Multiple bits may be set if the host is slow to read the
		 * status messages, indicating all the events that have
		 * happened */
		if (status & MXT_T9_RELEASE) {
			input_mt_report_slot_state(input_dev,
							MT_TOOL_FINGER, 0);
			mxt_input_sync(input_dev);
            //mxt_input_sync(data->input_rim_dev);//ZTEMT add for panel without rim
		}

		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if (area == 0) {
			area = MXT_TOUCH_MAJOR_T47_STYLUS;
			tool = MT_TOOL_PEN;
		} else {
			tool = MT_TOOL_FINGER;
		}

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	data->update_input = true;
}

void parse_t100_ext_message(const u8 *message, const u8 *tchcfg, struct ext_info *in)
{
	u8 aux = 6;
	u8 exp;

	memset(in, 0, sizeof(struct ext_info));
	in->status = message[1];

	if (!(in->status & MXT_T100_DETECT))
		return;

	if (test_flag_8bit(MXT_T100_TCHAUX_VECT, &tchcfg[MXT_T100_TCHAUX]))
		in->vec = message[aux++];

	if (aux < 10) {
		if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &tchcfg[MXT_T100_TCHAUX])) {
			in->amp = message[aux++];			
			if (in->amp < 0xff)
				in->amp++;
		}
	}

	if (aux < 10) {
		if (test_flag_8bit(MXT_T100_TCHAUX_AREA, &tchcfg[MXT_T100_TCHAUX]))
			in->area = message[aux++];
	}

	if (aux < 9) {
		if (test_flag_8bit(MXT_T100_TCHAUX_HW, &tchcfg[MXT_T100_TCHAUX])) {
				in->height = message[aux++];
				in->width = message[aux++];
			if (test_flag_8bit(MXT_T100_CFG_SWITCHXY, &tchcfg[MXT_T100_CFG1])) 
				swap(in->height,in->width);
		}
	}

	if (aux < 10) {
		if (test_flag_8bit(MXT_T100_TCHAUX_PEAK, &tchcfg[MXT_T100_TCHAUX]))
			in->peak = message[aux++];
	}

	if (aux < 9) {
		if (test_flag_8bit(MXT_T100_TCHAUX_AREAHW, &tchcfg[MXT_T100_TCHAUX])) {
			exp = (message[aux] >> 5) & 0x3;
			in->area = (message[aux] & 0x1f) << exp;
			in->height = (message[aux + 1] & 0xf)  << exp;
			in->width = (message[aux + 1] >> 4)  << exp;
			if (test_flag_8bit(MXT_T100_CFG_SWITCHXY, &tchcfg[MXT_T100_CFG1])) 
				swap(in->height,in->width);			
		}
	}
}

/*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
static int zte_ignore_zone(int point_id, int x, int y) //point_id
{
    int i = point_id;

    if((slot_ignore[i].slot_zoneid) == ZONE_C) {//front point in C zone
        if(x < ZTEMT_C_AREA_WIDTH || x > (1080 - ZTEMT_C_AREA_WIDTH)) {//in C zone
            return IGNORE_TURN_C;
        } else if(x < ZTEMT_B_AREA_WIDTH || x > (1080 - ZTEMT_B_AREA_WIDTH)) {
            slot_ignore[i].slot_zoneid = ZONE_B;//CtoB
            printk(KERN_ERR"\nC to B\n");
            return IGNORE_TURN_C_TO_A;
        } else {//CtoA
            slot_ignore[i].slot_zoneid = ZONE_A;
            printk(KERN_ERR"\nC to A\n");
            return IGNORE_TURN_C_TO_A;
        }
    } else if((slot_ignore[i].slot_zoneid) == ZONE_A) {//front point in A zone

        return IGNORE_TURN_A;

    } else if ((slot_ignore[i].slot_zoneid) == ZONE_B) {
        if(x < ZTEMT_C_AREA_WIDTH || x > (1080 - ZTEMT_C_AREA_WIDTH)) {//in C zone
            slot_ignore[i].slot_zoneid = ZONE_C;
            printk(KERN_ERR"\nB to C\n");
            return IGNORE_TURN_A_TO_C;
        } else if(x < ZTEMT_B_AREA_WIDTH || x > (1080 - ZTEMT_B_AREA_WIDTH)) {
            return IGNORE_TURN_A;
        } else {
            slot_ignore[i].slot_zoneid = ZONE_A;
            printk(KERN_ERR"\nB to A\n");
            return IGNORE_TURN_A;
        }
    }
    else {//new point (slot_ignore[i].slot_zoneid) == ZONE_DEFAULT
        if(x < ZTEMT_C_AREA_WIDTH || x > (1080 - ZTEMT_C_AREA_WIDTH)){//in C zone
            slot_ignore[i].slot_zoneid = ZONE_C;
            printk(KERN_ERR"start from zone C\n");
            return IGNORE_TURN_C;
        } else if (x < ZTEMT_B_AREA_WIDTH || x > (1080 - ZTEMT_B_AREA_WIDTH)) {
            slot_ignore[i].slot_zoneid = ZONE_B;//in B zone
            printk(KERN_ERR"start from zone B\n");
            return IGNORE_TURN_A;
        } else {//in A zone
            slot_ignore[i].slot_zoneid = ZONE_A;
            printk(KERN_ERR"start from zone A\n");
            return IGNORE_TURN_A;
        }
    }
    return -1;
}
#endif
/*zte end*/

static void mxt_proc_t100_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
    struct input_dev *input_dev;
	//struct input_dev *input_dev = data->input_dev;
    //struct input_dev *input_rim_dev = data->input_rim_dev;

	int id;
	u8 status;
	int x;
	int y;
	int tool;
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    int ret;
#endif
	struct ext_info info;

	/* do not report events if input device not yet registered */
	//if (!input_dev)
	//	return;
	
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T100_reportid_min - 2;
	/* ignore SCRSTATUS events */
	if (id < 0) {
		dev_dbg(dev,
			"T100 [%d] SCRSTATUS : 0x%x\n",
			id,
			message[1]);
		return;
	}

	status = message[1];
	x = (message[3] << 8) | message[2];
	y = (message[5] << 8) | message[4];
	parse_t100_ext_message(message, data->tchcfg, &info);
	dev_dbg(dev,
		"[%u] status:%02X x:%u y:%u [amp]:%02X [vec]:%02X [area]:%02X [peak]:%02X [width]:%02X [height]:%02X\n",
		id,
		status,
		x, y,
		info.amp,
		info.vec,
		info.area,
		info.peak,
		info.width,
		info.height);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	if (mxt_plugin_hook_t100(&data->plug, id, x, y, &info) != 0) {
		status = info.status;
		dev_dbg(dev,
			"[%u] status:%02X x:%u y:%u [amp]:%02X [vec]:%02X [area]:%02X [peak]:%02X [width]:%02X [height]:%02X *\n",
			id,
			status,
			x, y,
			info.amp,
			info.vec,
			info.area,
			info.peak,
			info.width,
			info.height);
	}
#endif
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
if (data->c_zone_flag == 1)
{
/*ZTEMT start add for panel without rim*/
    ret = zte_ignore_zone(id,x,y);

    if(ret == IGNORE_TURN_C){//C zone
        input_dev = data->input_rim_dev;

    }else if(ret == IGNORE_TURN_A){//A zone
        input_dev = data->input_dev;
        
    }else if(ret == IGNORE_TURN_C_TO_A){//C-A zone
        dev_err(dev, "ztemt lee C-A!  id=%d\n",id);
        input_dev = data->input_rim_dev;
        input_mt_slot(input_dev, id);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
        slot_ignore[id].slot_zoneid = ZONE_DEFAULT;
       // mxt_input_sync(data->input_rim_dev);
        input_dev = data->input_dev;
        
    }else if(ret == IGNORE_TURN_A_TO_C){//A-C zone
        dev_err(dev, "ztemt lee A-C!  id=%d\n",id);
        input_dev = data->input_dev;
        input_mt_slot(input_dev, id);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
        slot_ignore[id].slot_zoneid = ZONE_DEFAULT;
       // mxt_input_sync(data->input_dev);
        input_dev = data->input_rim_dev;

    }else{
        dev_err(dev, "ztemt lee error!\n");
    }

}else{
    input_dev = data->input_dev;
}
#else
    input_dev = data->input_dev;
#endif
    if (!input_dev)
		return;

    input_mt_slot(input_dev, id);
    
	if (status & MXT_T100_DETECT) {
		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if ((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_STYLUS)
			tool = MT_TOOL_PEN;
		else
			tool = MT_TOOL_FINGER;

   /*     if(event_du){
            dev_err(dev, "ztemt lee event_du=1!  id=%d\n",id);
            input_mt_report_slot_state(input_dev, tool, 1);
            event_du = 0;
        }
        */
		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

		if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX])){
/*** ZTEMT start ,add for fource touch ***/
#if defined(CONFIG_ZTEMT_PRESS_TOUCHSCREEN_CYPRESS)
        if (_cyttsp3_force_function.FT_ready_flag == true) {
             info.amp = (u8) cyttsp2_get_force(dev, id, (u16)x, (u16)y, (u16)info.amp);
            // dev_err(dev,"ztemt pressure =%d\n",info.amp);
        }
#endif
/*** ZTEMT end ***/

        input_report_abs(input_dev, ABS_MT_PRESSURE, info.amp);
        }

		if (test_flag_8bit(MXT_T100_TCHAUX_AREA | MXT_T100_TCHAUX_AREAHW, &data->tchcfg[MXT_T100_TCHAUX])) {
			if (tool == MT_TOOL_PEN)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, MXT_TOUCH_MAJOR_T47_STYLUS);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, info.area);
		}

		if (test_flag_8bit(MXT_T100_TCHAUX_VECT, &data->tchcfg[MXT_T100_TCHAUX]))
			input_report_abs(input_dev, ABS_MT_ORIENTATION,
					 info.vec);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
        slot_ignore[id].slot_zoneid = ZONE_DEFAULT;
#endif
	}

	data->update_input = true;
}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
void parse_t100_scr_message(const u8 *message, unsigned long scraux, struct scr_info *in)
{
	u8 aux = 1;

	memset(in, 0, sizeof(struct scr_info));
	in->status = message[aux++];

	if (test_flag(MXT_T100_SCRAUX_NUMTCH, &scraux))
		in->num_tch = message[aux++];

	if (test_flag(MXT_T100_SCRAUX_TCHAREA, &scraux)) {
		in->area_tch = MAKEWORD(message[aux], message[aux + 1]);		
		aux += 2;
	}

	if (test_flag(MXT_T100_SCRAUX_ATCHAREA, &scraux)) {
		in->area_atch = MAKEWORD(message[aux], message[aux + 1]);		
		aux += 2;
	}

	if (test_flag(MXT_T100_SCRAUX_INTTCHAREA, &scraux)) {
		in->area_inttch = MAKEWORD(message[aux], message[aux + 1]);		
		aux += 2;
	}
}

static void mxt_proc_t100_scr_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	struct scr_info info;

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T100_reportid_min - 2;

	if (id != -2) {
		dev_dbg(dev,
			"T100 [%d] msg : 0x%x\n",
			id,
			message[1]);
		return;
	}

	status = message[1];
	parse_t100_scr_message(message, data->tchcfg[MXT_T100_SCRAUX], &info);
	dev_dbg(dev,
		"[scr] status:%02X  [num]:%d [area]:%d %d %d\n",
		status,
		info.num_tch,
		info.area_tch,
		info.area_atch,
		info.area_inttch);

	mxt_plugin_hook_t42(&data->plug, status);

	if (mxt_plugin_hook_t100_scraux(&data->plug, &info) == 0)
		return;

	dev_dbg(dev,
		"[scr] status:%02X  [num]:%d [area]:%d %d %d\n",
		info.status,
		info.num_tch,
		info.area_tch,
		info.area_atch,
		info.area_inttch);

	status = info.status;
	if (status & MXT_SCRAUX_STS_SUP) {
		data->update_input = false;
		//data->enable_reporting = false;

#if defined(ZTEMT_ATMEL_PALM_KEY_SUPPORT)
		mxt_reset_slots(data);
		input_report_key(input_dev, KEY_PALM_LOCK, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_PALM_LOCK, 0);
		input_sync(input_dev);
		mxt_reset_slots(data);
		dev_info(dev, "lee atmel have a pamel with key\n");
#else
#define ABS_TOOL_SURRESS_PRESSURE 1000
		mxt_reset_slots(data);

		input_mt_slot(input_dev, 0);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, 100);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, 0);
		input_report_abs(input_dev, ABS_MT_PRESSURE,
					 ABS_TOOL_SURRESS_PRESSURE);
		mxt_input_sync(data->input_dev);
        //mxt_input_sync(data->input_rim_dev);//ZTEMT start add for panel without rim
        dev_err(dev, "lee atmel have a pamel\n");

		mxt_reset_slots(data);
#endif
	}//else 
		//data->enable_reporting = true;
}
#endif

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct mxt_virtual_key_space *v_ratio = &data->pdata->vkey_space_ratio;
	struct device *dev = &data->client->dev;
	int key, x_rage,x_edge,x_space;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);
	u8 num_keys;

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	if (!data->pdata->keymap || !data->pdata->num_keys)
		return;

	num_keys =data->pdata->num_keys[T15_T97_KEY];
	if (!num_keys)
		return;

	x_edge = (data->max_x + 1) * v_ratio->x_edge / 100;
	x_space = (data->max_x + 1) * v_ratio->x_space / 100;
	x_rage = (data->max_x + 1) * (100 - v_ratio->x_edge * 2 + v_ratio->x_space)
		/ num_keys / 100;

	for (key = 0; key < num_keys; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->t15_keystatus);
			input_mt_slot(input_dev,CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM);
			
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,1);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x_edge + (x_rage + x_space) * key + (x_rage >> 1));
			input_report_abs(input_dev, ABS_MT_POSITION_Y, data->max_y + 1);
			input_report_abs(input_dev, ABS_MT_PRESSURE, 1);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, MXT_TOUCH_MAJOR_T15_VIRTUAL_KEY);

			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->t15_keystatus);
			input_mt_slot(input_dev,CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM);
			
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,0);
			sync = true;
		}
	}

	if (!data->update_input)
		data->update_input = sync;
}
#else

static void mxt_reinit_t15_key_order(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	u8 num_keys;
	char suffix[16];
	int key;
	bool sync = false;
	int ver  = -100;
	unsigned int *keymap;
	int ret;

	/* do not report events if input device not yet registered */	
	if (!data->pdata->keymap || !data->pdata->num_keys)
		return;

	num_keys =data->pdata->num_keys[T15_T97_KEY];
	keymap = (unsigned int *)data->pdata->keymap[T15_T97_KEY];

	suffix[0] = '\0';
	ret = mxt_plugin_get_pid_name(&data->plug, suffix, sizeof(suffix));
	if (ret == 0) {
		if (suffix[0] >= '0' && suffix[0] <= '9') {
			if (strncmp("03", suffix, 2) < 0) {
				ver = 4;
			}else if (strncmp("03", suffix, 2) == 0) {
				ver = 3;
			}else
				ver = 1;
		}else{
		    dev_err(dev, "the type of pid is error\n");
			ver = 4;
        }

	}else if(ret == -EINVAL) {
        dev_err(dev, "read pid failed EINVAL\n");
		ver = 1;
    }else {
        dev_err(dev, "read pid failed ENODEV \n");
		ver = 4;
    }


	dev_err(dev, "mxt_reinit_t15_order: %s %d ret %d\n", suffix,ver,ret);

	for (key = 0; key < num_keys; key++) {
		if (test_and_clear_bit(key, &data->t15_keystatus)) {
			dev_err(dev, "T15 key release: %u\n", key);
			input_event(input_dev, EV_KEY,
					keymap[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);		

	if (num_keys == 3) {
		dev_err(dev, "switch key to version %d \n", ver);
		if (ver == 1) {
			keymap[0] = KEY_BACK;
			keymap[1] = KEY_MENU;
			keymap[2] = KEY_HOMEPAGE;
		}else if (ver == 3){
			keymap[0] = KEY_HOMEPAGE;
			keymap[1] = KEY_BACK;
			keymap[2] = KEY_MENU;
		}else if (ver == 4) {
			keymap[0] = KEY_MENU;
			keymap[1] = KEY_HOMEPAGE;
			keymap[2] = KEY_BACK;
		}
	}
}

static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);
	u8 num_keys;
	const unsigned int *keymap;

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	if (!data->pdata->keymap || !data->pdata->num_keys)
		return;

	num_keys =data->pdata->num_keys[T15_T97_KEY];
	keymap = data->pdata->keymap[T15_T97_KEY];
	for (key = 0; key < num_keys; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
					keymap[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
					keymap[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}
#endif

static void mxt_proc_t97_messages(struct mxt_data *data, u8 *msg)
{
	mxt_proc_t15_messages(data,msg);
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status & MXT_T42_MSG_TCHSUP)
		dev_info(dev, "T42 suppress\n");
	else
		dev_info(dev, "T42 normal\n");

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t42(&data->plug, status);		
#endif
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	dev_dbg(dev, "T48 state %d status %02X %s%s%s%s%s\n",
			state,
			status,
			(status & 0x01) ? "FREQCHG " : "",
			(status & 0x02) ? "APXCHG " : "",
			(status & 0x04) ? "ALGOERR " : "",
			(status & 0x10) ? "STATCHG " : "",
			(status & 0x20) ? "NLVLCHG " : "");

	return 0;
}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static void mxt_proc_t61_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 reportid = msg[0];
	int ret;
	
	dev_dbg(dev, "T61 status 0x%x 0x%x\n",
			msg[0],
			msg[1]);

	msg[0] -= data->T61_reportid_min;
	mxt_plugin_hook_t61(&data->plug, msg[0], msg[1]);
	
	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		ret = mxt_proc_gesture_messages(data, MXT_SPT_TIMER_T61,
			T61_KEY, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key message, ret %d\n", ret);
	}
	msg[0] = reportid;
}
#endif

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	/* do not report events if input device not yet registered */
	if (!input_dev)
		return;
	
	if (!data->enable_reporting)
		return;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_T63_STYLUS_PRESSURE_MASK;

	dev_dbg(dev,
		"[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		(msg[1] & MXT_T63_STYLUS_SUPPRESS) ? 'S' : '.',
		(msg[1] & MXT_T63_STYLUS_MOVE)	 ? 'M' : '.',
		(msg[1] & MXT_T63_STYLUS_RELEASE)  ? 'R' : '.',
		(msg[1] & MXT_T63_STYLUS_PRESS)	? 'P' : '.',
		x, y, pressure,
		(msg[2] & MXT_T63_STYLUS_BARREL) ? 'B' : '.',
		(msg[2] & MXT_T63_STYLUS_ERASER) ? 'E' : '.',
		(msg[2] & MXT_T63_STYLUS_TIP)	? 'T' : '.',
		(msg[2] & MXT_T63_STYLUS_DETECT) ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_T63_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS,
			 (msg[2] & MXT_T63_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2,
			 (msg[2] & MXT_T63_STYLUS_BARREL));

	mxt_input_sync(input_dev);
    //mxt_input_sync(data->input_rim_dev);//ZTEMT start add for panel without rim
}

static void mxt_proc_t68_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "T68 state = 0x%x\n" , 
		msg[1]);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t68(&data->plug, msg);
#endif
}
static void mxt_proc_t72_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "T72 noise state1 = 0x%x state2 = 0x%x\n" , 
		msg[1],
		msg[2]);
	
	if (msg[1] & MXT_T72_NOISE_SUPPRESSION_NOISELVCHG) {
		dev_info(dev, "T72 noise change, state = %d, peak = %d, level = %d\n" , 
			msg[2] & 0x7,
			msg[4],
			msg[5]);
	}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_t72(&data->plug, msg);
#endif
}

static void mxt_proc_T81_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 reportid = msg[0];
 	u8 status = msg[1];
	int ret;
	
	dev_info(dev, "T81 Status 0x%x Info: %x %x %x %x\n",
		status,
		msg[2],
		msg[3],
		msg[4],
		msg[5]);

	msg[0] -= data->T81_reportid_min;
	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
       // wake_lock_timeout(&data->wake_lock, 3*HZ);//add by ztemt 20150403
        ret = mxt_proc_gesture_messages(data, MXT_PROCI_UNLOCKGESTURE_T81,
			T81_KEY, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key message, ret %d\n", ret);
	}
	msg[0] = reportid;
}

static void mxt_proc_T92_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	int ret;

	dev_info(dev, "T92 %s 0x%x Info: %x %x %x %x %x %x\n",
		(status & 0x80) ? "stroke" : "symbol",
		status,
		msg[2],
		msg[3],
		msg[4],
		msg[5],
		msg[6],
		msg[7]);

	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		ret = mxt_proc_gesture_messages(data, MXT_PROCI_GESTURE_T92,
			T92_KEY, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key message, ret %d\n", ret);
	}
}

static void mxt_proc_T93_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	int ret;

	dev_info(dev, "T93 Status 0x%x Info: %x\n",
		status,
		msg[1]);

	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		ret = mxt_proc_gesture_messages(data, MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
			T93_KEY, msg);

        if(&data->wake_lock){
            wake_lock_timeout(&data->wake_lock, 3*HZ);//add by ztemt 20150403
        }
        
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key 93 message, ret %d\n", ret);
	}
}

static void mxt_proc_T99_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	int ret;

	dev_info(dev, "T99 Status 0x%x Event: %d Index %d\n",
		status,
		msg[1] & 0xF,
		msg[2]);

	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		ret = mxt_proc_gesture_messages(data, MXT_PROCI_KEYGESTUREPROCESSOR_T99,
			T99_KEY, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key 99 message, ret %d\n", ret);
	}
}

static void mxt_proc_t102_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	dev_info(dev, "msg for t102 = 0x%x 0x%x 0x%x 0x%x\n",
		msg[2], msg[3], msg[4], msg[5]);

	data->selfcap_status.cause = msg[2];
	data->selfcap_status.error_code = msg[3];
}

static void mxt_proc_T115_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 type,key;
	u8 status = msg[1];
	int ret;

	dev_info(dev, "T115 Status 0x%x Info: %x\n",
		status,
		msg[1]);

	/* do not report events if input device not yet registered */
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		if (status & 0x80) {
			type = MXT_PROCI_SYMBOLGESTURE_T115;
			key = T115_KEY;
		}else {
			type = MXT_SPT_SYMBOLGESTURECONFIG_T116;
			key = T116_KEY;
		}
		ret = mxt_proc_gesture_messages(data, type,
			key, msg);
		if (!ret)
			set_bit(MXT_WK_DETECTED,&data->enable_wakeup);
		else
			dev_info(dev, "Unhandled key 115 message, ret %d\n", ret);
	}
}

static int mxt_proc_message(struct mxt_data *data, u8 *message)
{
	u8 report_id = message[0];
	bool dump = data->debug_enabled;

	if (report_id == MXT_RPTID_NOMSG)
		return 0;

	if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, message);
	} else if (report_id >= data->T9_reportid_min
		&& report_id <= data->T9_reportid_max) {
		mxt_proc_t9_message(data, message);
	} else if (report_id >= data->T100_reportid_min
		&& report_id <= data->T100_reportid_max) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		if (report_id < data->T100_reportid_min + 2)
			mxt_proc_t100_scr_message(data, message);
		else
#endif
			mxt_proc_t100_message(data, message);
	} else if (report_id == data->T19_reportid) {
#if defined(CONFIG_MXT_VENDOR_ID_BY_T19)
		mxt_proc_t19_messages(data, message);
#else
		mxt_input_button(data, message);
		data->update_input = true;
#endif
	} else if (report_id == data->T25_reportid) {
		mxt_proc_t25_messages(data, message);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	} else if (report_id >= data->T61_reportid_min
			&& report_id <= data->T61_reportid_max) {
		mxt_proc_t61_messages(data, message);
#endif
	} else if (report_id >= data->T63_reportid_min
			&& report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T42_reportid_min
			&& report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, message);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, message);
	} else if (report_id >= data->T15_reportid_min
			&& report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, message);
	} else if ( report_id == data->T24_reportid) {
		mxt_proc_t24_messages(data, message);
	} else if (report_id >= data->T68_reportid_min
			&& report_id <= data->T68_reportid_max) {
		mxt_proc_t68_messages(data, message);
	} else if (report_id >= data->T72_reportid_min
			&& report_id <= data->T72_reportid_max) {
		mxt_proc_t72_messages(data, message);
	} else if (report_id >= data->T81_reportid_min
			&& report_id <= data->T81_reportid_max) {
		mxt_proc_T81_messages(data, message);
	} else if (report_id == data->T92_reportid) {
		mxt_proc_T92_messages(data, message);
	} else if (report_id == data->T93_reportid) {
		mxt_proc_T93_messages(data, message);
	} else if (report_id >= data->T97_reportid_min
			&& report_id <= data->T97_reportid_max) {
		mxt_proc_t97_messages(data, message);
	} else if (report_id == data->T99_reportid) {
		mxt_proc_T99_messages(data, message);
	} else if (report_id == data->T102_reportid) {
		mxt_proc_t102_messages(data, message);
	} else if (report_id == data->T115_reportid) {
		mxt_proc_T115_messages(data, message);
	} else {
		dump = true;
	}

	if (dump || report_id > data->max_reportid)
		mxt_dump_message(data, message);

	if (data->debug_v2_enabled && report_id <= data->max_reportid)
		mxt_debug_msg_add(data, message);

	return 1;
}

static int mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = __mxt_read_reg_ext(data->client, data->client->addr, data->T5_address,
				data->T5_msg_size * count, data->msg_buf, I2C_ACCESS_R_REG_FIXED);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 1)
			num_valid++;
	}

	/* return number of messages read */
	return num_valid;
}

static irqreturn_t mxt_process_messages_t44(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;

	/* Read T44 and T5 together */
	ret = __mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read T44 and T5 (%d)\n", ret);
		return IRQ_NONE;
	}

	count = data->msg_buf[0];

	if (count == 0) {
		/*dev_warn*/dev_dbg(dev, "Interrupt triggered but zero messages\n");
		return IRQ_NONE;
	} else if (count > data->max_reportid) {
		dev_err(dev, "T44 count %d exceeded max report id\n", count);
		count = data->max_reportid;
	}

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		dev_warn(dev, "Unexpected invalid message\n");
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0)
			goto end;
		else if (ret != num_left)
			dev_warn(dev, "Unexpected invalid message\n");
	}

end:
	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
        mxt_input_sync(data->input_rim_dev);//ZTEMT start add for panel without rim
#endif
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static int mxt_process_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int count, read;

	count = data->max_reportid;

	/* Read messages until we force an invalid */
	do {
		read = mxt_read_and_process_messages(data, 1);
		if (read < 1)
			return 0;
	} while (--count);

	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
        mxt_input_sync(data->input_rim_dev);//ZTEMT start add for panel without rim
#endif
		data->update_input = false;
	}

	dev_err(dev, "CHG pin isn't cleared\n");
	return -EBUSY;
}

static irqreturn_t mxt_process_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;

	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_and_process_messages(data, count + 1);
	if (total_handled < 0)
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	else if (total_handled <= count)
		goto update_count;

	/* read two at a time until an invalid message or else we reach
	 * reportid limit */
	do {
		num_handled = mxt_read_and_process_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;

	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
        mxt_input_sync(data->input_rim_dev);//ZTEMT start add for panel without rim
#endif
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;
	irqreturn_t ret;

	if (data->in_bootloader) {
		/* bootloader state transition completion */
		complete(&data->bl_completion);
		return IRQ_HANDLED;
	}

	if (!data->object_table)
		return IRQ_NONE;

	if (!data->msg_buf)
		return IRQ_NONE;

	if (data->T44_address) {
		ret = mxt_process_messages_t44(data);
	} else {
		ret = mxt_process_messages(data);
	}

	return ret;
}

#if defined(CONFIG_MXT_IRQ_WORKQUEUE)

static void mxt_active_proc_thread(void *dev_id, unsigned int event)
{
	struct mxt_data *data;

	data = (struct mxt_data *)dev_id;
	if (!data)
		return;
	
	set_bit(event,&data->busy);
	dev_dbg(&data->client->dev, "mxt_active_proc_thread event %d busy %lx depth %d\n", 
		event, data->busy,atomic_read(&data->depth));
	if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
		if (event == MXT_EVENT_IRQ)
			set_bit(MXT_EVENT_IRQ_FLAG,&data->busy);
	}
	wake_up_interruptible(&data->wait);
}

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)

static void board_pulse_irq_thread(void)
{
	struct mxt_data *data = mxt_g_data;

	if (!data) {
		printk(KERN_ERR "MXT_EXTERNAL_TRIGGER_IRQ: mxt_g_data is not prepared \n");
		return;
	}

	board_disable_irq(data->pdata,data->irq);
	if(atomic_read(&data->depth) > 0)
		atomic_set(&data->depth, 0);
	mxt_active_proc_thread(data,MXT_EVENT_IRQ);
}

#else

static irqreturn_t mxt_interrupt_pulse_workqueue(int irq, void *dev_id)
{
	struct mxt_data *data = (struct mxt_data *)dev_id;

#if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	disable_irq_nosync(irq);
	if(atomic_read(&data->depth) > 0)
		atomic_set(&data->depth, 0);
#endif
	if (data) {
		//cancel_delayed_work_sync(&data->irq_work);
		mxt_active_proc_thread(data,MXT_EVENT_IRQ);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
#endif

//warning: don't put any write/read code in this function
static int mxt_process_message_thread(void *dev_id)
{
	struct mxt_data *data = dev_id;
	struct mxt_platform_data *pdata = data->pdata;
	long interval = MAX_SCHEDULE_TIMEOUT;
	int post = false;
	irqreturn_t iret;
	
	while (!kthread_should_stop()) {

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		mxt_plugin_pre_process(&data->plug,data->in_bootloader);
#endif
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible_timeout(
			data->wait, 
			test_bit(MXT_EVENT_IRQ,&data->busy)||test_bit(MXT_EVENT_EXTERN,&data->busy)||
				kthread_should_stop() , 
			interval);

		dev_dbg(&data->client->dev, "mxt_process_message_thread busy %lx suspend %d  boot %d interval %ld(0x%lx)\n",
			data->busy,
			data->suspended,
			data->in_bootloader,
			interval,interval);
		
		if (kthread_should_stop()) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)		
			mxt_plugin_thread_stopped(&data->plug);
#endif
			break;
		}

		if (test_and_clear_bit(MXT_EVENT_IRQ,&data->busy)) {
			iret = mxt_interrupt(data->irq, (void *)data);
			if(iret == IRQ_NONE) {
				if (data->pdata->irqflags & IRQF_TRIGGER_LOW) {
					dev_dbg(&data->client->dev, "Invalid irq: busy 0x%lx depth %d, sleep\n",
						data->busy,atomic_read(&data->depth));
						msleep(MXT_WAKEUP_TIME);
				}
			}

			if (atomic_read(&data->depth) <= 0) 
				atomic_inc(&data->depth);
			board_enable_irq(pdata,data->irq);
		}

		if (data->suspended) {
			interval = MAX_SCHEDULE_TIMEOUT;
			if (test_bit(MXT_EVENT_EXTERN,&data->busy))
				post = true;
		}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		else{
			post = true;
		}
		
		if (post) {
			clear_bit(MXT_EVENT_EXTERN,&data->busy);
			interval = (long)mxt_plugin_post_process(&data->plug,data->in_bootloader);
		}
#endif
	}

	set_current_state(TASK_RUNNING);

	return 0;
}

#endif

static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset,
			  u8 value, bool wait)
{
	u16 reg;
	u8 command_register;
	int timeout_counter = 0;
	int ret;

	reg = data->T6_address + cmd_offset;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(MXT_WAKEUP_TIME);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while ((command_register != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static int mxt_soft_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;

	dev_info(dev, "Resetting chip\n");

	INIT_COMPLETION(data->reset_completion);

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret)
		return ret;

	ret = mxt_wait_for_completion(data, &data->reset_completion,
					  MXT_RESET_TIMEOUT);
	if (ret)
		return ret;

	msleep(MXT_WAKEUP_TIME);

	return 0;
}

#if defined(CONFIG_MXT_SELFCAP_TUNE)

static int mxt_update_self_chgtime(struct mxt_data *data, bool inc)
{
	int error = -EINVAL;
	u8 val;
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	int index = /*data->current_index*/0;

	WARN_ON(!pdata->config_array);

	if (pdata->config_array) {
		error = __mxt_read_reg(data->client, data->T102_address + MXT_SELF_CHGTIME,
					1, &val);
		if (error) {
			dev_err(dev, "Failed to get self charge time!\n");
			return error;
		}

		if (inc) {
			val += 5;
			if (val > pdata->config_array[index].self_chgtime_max)
				return -ERANGE;
		}
		else {
			val -= 5;
			if (val < pdata->config_array[index].self_chgtime_min)
				return -ERANGE;
		}

		error = __mxt_write_reg(data->client, data->T102_address + MXT_SELF_CHGTIME,
					1, &val);
	}
	return error;
}

static void mxt_wait_for_self_tune_msg(struct mxt_data *data)
{
	int time_out = 1000;
	int i = 0;

	while(i < time_out) {
		if (data->selfcap_status.cause == 0x3)
			return;
		i++;
		msleep(10);
	}

	return;
}

static int mxt_do_self_tune(struct mxt_data *data, u8 cmd, bool nv_backup)
{
	int error;
	struct device *dev = &data->client->dev;

	memset(&data->selfcap_status, 0x0, sizeof(data->selfcap_status));

	if (!data->T102_address) {
		dev_err(dev, "Not T102 exist!\n");
		return 0;
	}

	error = __mxt_write_reg(data->client, data->T102_address + MXT_SELFCAP_CMD,
					1, &cmd);
	if (error) {
		dev_err(dev, "Error when execute cmd 0x%x!\n", cmd);
		return error;
	}

	mxt_wait_for_self_tune_msg(data);
	if (data->selfcap_status.cause == 0x3) {
		if (data->selfcap_status.error_code & 0x02) {
			error = mxt_update_self_chgtime(data, true);
			if (error)
				return error;
			return -EINVAL;
		} else if (data->selfcap_status.error_code & 0x01) {
			error = mxt_update_self_chgtime(data, false);
			if (error)
				return error;
			return -EINVAL;
		}
	}

	if (nv_backup) {
		cmd = MXT_SELFCMD_STORE;
		error = __mxt_write_reg(data->client, data->T102_address + MXT_SELFCAP_CMD,
					1, &cmd);
		if (error) {
			dev_err(dev, "Error when execute cmd store!\n");
			return error;
		}
	}

	return 0;
}

static void mxt_self_tune(struct mxt_data *data, u8 nv_backup)
{
	struct device *dev = &data->client->dev;
	int retry_times = 10;
	int i = 0;
	int error;

	while(i < retry_times) {
		error = mxt_do_self_tune(data, MXT_SELFCMD_TUNE, (bool)nv_backup);
		if (!error)
			return;
		else if (error == -ERANGE) {
			dev_err(dev, "self out of range!\n");
			return;
		}
		i++;
	}

	dev_err(dev, "Even retry self tuning for 10 times, still can't pass.!\n");
}

static ssize_t mxt_self_tune_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u8 nv_backup;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%hhu", &nv_backup) == 1)
		mxt_self_tune(data, nv_backup);
	else
		return -EINVAL;

	return count;
}

#endif

static void mxt_update_crc(struct mxt_data *data, u8 cmd, u8 value)
{
	/* on failure, CRC is set to 0 and config will always be downloaded */
	data->config_crc = 0;
	INIT_COMPLETION(data->crc_completion);

	mxt_t6_command(data, cmd, value, true);

	/* Wait for crc message. On failure, CRC is set to 0 and config will
	 * always be downloaded */
	mxt_wait_for_completion(data, &data->crc_completion, MXT_CRC_TIMEOUT);
}

static void mxt_calc_crc24(u32 *crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u32 data_word;

	data_word = (secondbyte << 8) | firstbyte;
	result = ((*crc << 1) ^ data_word);

	if (result & 0x1000000)
		result ^= crcpoly;

	*crc = result;
}

static u32 mxt_calculate_crc(u8 *base, off_t start_off, off_t end_off)
{
	u32 crc = 0;
	u8 *ptr = base + start_off;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	while (ptr < last_val) {
		mxt_calc_crc24(&crc, *ptr, *(ptr + 1));
		ptr += 2;
	}

	/* if len is odd, fill the last byte with 0 */
	if (ptr == last_val)
		mxt_calc_crc24(&crc, *ptr, 0);

	/* Mask to 24-bit */
	crc &= 0x00FFFFFF;

	return crc;
}

static int mxt_set_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	int val;

	error = __mxt_read_reg(client,
					data->T18_address + MXT_COMMS_CTRL,
					1, &val);
	if (error)
		return error;

	if (data->pdata->irqflags & IRQF_TRIGGER_LOW) {
		val &= ~0x44;  //normal mode 0
	}else{
		val |= 0x44;	//retriggle
	}

	error = __mxt_write_reg(client,
					data->T18_address + MXT_COMMS_CTRL,
					1, &val);

	return error;
}

static int mxt_check_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	int val;

	if (data->pdata->irqflags & IRQF_TRIGGER_LOW)
		return 0;

	if (data->T18_address) {
		error = __mxt_read_reg(client,
						data->T18_address + MXT_COMMS_CTRL,
						1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data);

/*
 * mxt_check_reg_init - download configuration to chip
 *
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *	<TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *	<TYPE> - 2-byte object type as hex
 *	<INSTANCE> - 2-byte object instance number as hex
 *	<SIZE> - 2-byte object size as hex
 *	<CONTENTS> - array of <SIZE> 1-byte hex values
 */
static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info={0};
	struct mxt_object *object;
	const struct firmware *cfg = NULL;
	int ret;
	int offset;
	int data_pos;
	int byte_offset;
	int i;
	int cfg_start_ofs;
	u32 info_crc, config_crc, calculated_crc;
	u8 *config_mem;
	size_t config_mem_size;
	unsigned int type, instance, size;
	u8 val;
	u16 reg;
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
	u8 *object_mem,*object_offset;
#endif

	if (!data->cfg_name) {
		dev_dbg(dev, "Skipping cfg download\n");
		return 0;
	}

	ret = request_firmware(&cfg, data->cfg_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			data->cfg_name);
			
		return 0;
	}

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}

	data_pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + data_pos, "%hhx%n",
				 (unsigned char *)&cfg_info + i,
				 &offset);
	#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		print_trunk(cfg->data, data_pos, offset);
	#endif
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		data_pos += offset;
	}

	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Info CRC\n");
		ret = -EINVAL;
		goto release;
	}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	print_trunk(cfg->data, data_pos, offset);
#endif
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Config CRC\n");
		ret = -EINVAL;
		goto release;
	}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	print_trunk(cfg->data, data_pos, offset);
#endif
	data_pos += offset;

	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "Config CRC 0x%06X: OK\n",
				 data->config_crc);
#if defined(CONFIG_MXT_CAL_TRIGGER_CAL_WHEN_CFG_MATCH)
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		/* Recalibrate since chip has been in deep sleep */
		if (mxt_plugin_cal_t37_check_and_calibrate(&data->plug, false,false) != 0)
#endif
		mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
#endif
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			data->info_crc, info_crc);
	}

	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START
		+ data->info->object_num * sizeof(struct mxt_object)
		+ MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
	config_mem_size <<= 1;
#endif
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto release;
	}
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
	object_mem = config_mem + (config_mem_size>>1);
#endif

	while (data_pos < cfg->size - 16) {
		/* Read type, instance, length */
		ret = sscanf(cfg->data + data_pos, "%x %x %x%n",
				 &type, &instance, &size, &offset);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		print_trunk(cfg->data, data_pos, offset);
#endif
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
		object_offset = object_mem;
#endif
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format: failed to parse object\n");
			/*ret = -EINVAL;
			goto release_mem;*/
			break;
		}
		data_pos += offset;

		object = mxt_get_object(data, type);
		if (!object) {
			/* Skip object */
			for (i = 0; i < size; i++) {
				ret = sscanf(cfg->data + data_pos, "%hhx%n",
						 &val,
						 &offset);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
				print_trunk(cfg->data, data_pos, offset);
#endif

				data_pos += offset;
			}
			continue;
		}

		if (size > mxt_obj_size(object)) {
			/* Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited */
			dev_warn(dev, "Discarding %d byte(s) in T%d\n",
				 size - mxt_obj_size(object), type);
		} else if (mxt_obj_size(object) > size) {
			/* If firmware is upgraded, new bytes may be added to
			 * end of objects. It is generally forward compatible
			 * to zero these bytes - previous behaviour will be
			 * retained. However this does invalidate the CRC and
			 * will force fallback mode until the configuration is
			 * updated. We warn here but do nothing else - the
			 * malloc has zeroed the entire configuration. */
			dev_warn(dev, "Zeroing %d byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}

		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + mxt_obj_size(object) * instance;

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + data_pos, "%hhx%n",
					 &val,
					 &offset);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
			print_trunk(cfg->data, data_pos, offset);
#endif
			if (ret != 1) {
				dev_err(dev, "Bad format in T%d\n", type);
				ret = -EINVAL;
				goto release_mem;
			}
			data_pos += offset;

			if (i > mxt_obj_size(object))
				continue;

			byte_offset = reg + i - cfg_start_ofs;

			if ((byte_offset >= 0)
				&& (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
		#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
				*(object_offset++)=val ;
		#endif
			} else {
				dev_err(dev, "Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}
		}
		
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
		ret = __mxt_write_reg(data->client,reg,size,object_mem);
		if(ret!=0){
			dev_err(dev,"write object[%d] error\n",object->type);
			goto release_mem ;
		}
#endif
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_warn(dev, "Config CRC error, calculated=%06X, file=%06X\n",
			 calculated_crc, config_crc);

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reg_init(&data->plug, config_mem, config_mem_size, cfg_start_ofs);
#endif
	/* Write configuration as blocks */
	byte_offset = 0;
#if defined(CONFIG_MXT_UPDATE_BY_OBJECT)
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;

		ret = __mxt_write_reg(data->client,
					  cfg_start_ofs + byte_offset,
					  size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}

		byte_offset += size;
	}
#endif

	mxt_update_crc(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);

	ret = mxt_check_retrigen(data);
	if (ret)
		goto release_mem;

	/*
	ret = mxt_soft_reset(data);
	if (ret)
		goto release_mem;
	*/
	mxt_soft_reset(data);

	dev_info(dev, "Config written\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);

release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	return ret;
}

static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 0, .idle = 0 };

	if (sleep == MXT_POWER_CFG_DEEPSLEEP)
		new_config = &deepsleep;
	else
		new_config = &data->t7_cfg;

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_set_t7(&data->plug, sleep == MXT_POWER_CFG_DEEPSLEEP);
#endif
	error = __mxt_write_reg(data->client, data->T7_address,
			sizeof(data->t7_cfg),
			new_config);
	if (error)
		return error;

	dev_dbg(dev, "Set T7 ACTV:%d IDLE:%d\n",
		new_config->active, new_config->idle);

	msleep(10);

	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;
	bool retry = false;

recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;

	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {
			dev_info(dev, "T7 cfg zero, resetting\n");
			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		} else {
			dev_dbg(dev, "T7 cfg zero after reset, overriding\n");
			data->t7_cfg.active = 20;
			data->t7_cfg.idle = 100;
			return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
	} else {
		dev_info(dev, "Initialised power cfg: ACTV %d, IDLE %d\n",
				data->t7_cfg.active, data->t7_cfg.idle);
		return 0;
	}
}

static int mxt_acquire_irq(struct mxt_data *data)
{
	struct mxt_platform_data *pdata = data->pdata;
	int error;

	if(atomic_read(&data->depth) <= 0) {
		atomic_inc(&data->depth);
		board_enable_irq(pdata,data->irq);
	}
	
	if (data->use_retrigen_workaround) {
		error = mxt_process_messages_until_invalid(data);
		if (error)
			return error;
	}

	return 0;
}

static void mxt_free_input_device(struct mxt_data *data)
{
	mxt_debug_msg_remove(data);

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	if (data->properties_kobj) {
		kobject_put(data->properties_kobj);
		data->properties_kobj = NULL;
	}
#endif
	
	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}

#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
	if (data->input_rim_dev) {
		input_unregister_device(data->input_rim_dev);
		data->input_rim_dev = NULL;
	}	
#endif
}

static void mxt_free_object_table(struct mxt_data *data)
{
	//mxt_debug_msg_remove(data);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_force_stop(&data->plug);
	mxt_plugin_deinit(&data->plug);
#endif
	kfree(data->raw_info_block);
	data->object_table = NULL;
	data->info = NULL;
	data->raw_info_block = NULL;
	kfree(data->msg_buf);
	data->msg_buf = NULL;

	mxt_free_input_device(data);

	data->enable_reporting = false;
	data->T5_address = 0;
	data->T5_msg_size = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T8_address = 0;
	data->T9_address = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_address = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_address = 0;
	data->T19_reportid = 0;
	data->T24_address = 0;
	data->T24_reportid = 0;
	data->T25_address = 0;
	data->T25_reportid = 0;
	data->T37_address = 0;
	data->T38_address = 0;
	data->T40_address = 0;
	data->T42_address = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T46_address = 0;
	data->T47_address = 0;
	data->T48_reportid = 0;
	data->T55_address = 0;
	data->T56_address = 0;
	data->T61_address = 0;
	data->T61_reportid_min = 0;
	data->T61_reportid_max = 0;
	data->T61_instances = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T65_address = 0;
	data->T68_address = 0;
	data->T71_address = 0;
	data->T72_address = 0;
	data->T72_reportid_min = 0;
	data->T72_reportid_max = 0;
	data->T78_address = 0;
	data->T80_address = 0;
	data->T81_address = 0;
	data->T81_reportid_min= 0;
	data->T81_reportid_max= 0;
	data->T92_address = 0;
	data->T92_reportid = 0;
	data->T93_address = 0;
	data->T93_reportid = 0;
	data->T96_address = 0;
	data->T97_address = 0;
	data->T97_reportid_min = 0;
	data->T97_reportid_max = 0;
	data->T99_address = 0;
	data->T99_reportid = 0;	
	data->T100_address = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->T102_address = 0;
	data->T102_reportid = 0;
	data->T104_address = 0;
	data->T113_address = 0;
	data->T115_address = 0;
	data->T115_reportid = 0;	
	data->max_reportid = 0;
}

static int mxt_parse_object_table(struct mxt_data *data,
				  struct mxt_object *object_table)
{
	struct i2c_client *client = data->client;
	int i;
	u8 reportid;
	u16 end_address;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	data->mem_size = 0;
	for (i = 0; i < data->info->object_num; i++) {
		struct mxt_object *object = object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		dev_dbg(&data->client->dev,
			"T%u Start:%u Size:%u Instances:%u Report IDs:%u-%u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

		switch (object->type) {
		case MXT_GEN_MESSAGEPROCESSOR_T5:
			if (data->info->family_id == 0x80) {
				/* On mXT224 read and discard unused CRC byte
				 * otherwise DMA reads are misaligned */
				data->T5_msg_size = mxt_obj_size(object);
			} else {
				/* CRC not enabled, so skip last byte */
				data->T5_msg_size = mxt_obj_size(object) - 1;
			}
			data->T5_address = object->start_address;
		case MXT_GEN_COMMANDPROCESSOR_T6:
			data->T6_reportid = min_id;
			data->T6_address = object->start_address;
			break;
		case MXT_GEN_POWERCONFIG_T7:
			data->T7_address = object->start_address;
			break;
		case MXT_GEN_ACQUISITIONCONFIG_T8:
			data->T8_address = object->start_address;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T9:
			/* Only handle messages from first T9 instance */
			data->T9_reportid_min = min_id;
			data->T9_reportid_max = min_id +
						object->num_report_ids - 1;
			data->T9_address = object->start_address;
			data->num_touchids = object->num_report_ids;
			break;
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_min = min_id;
			data->T15_reportid_max = max_id;
			data->T15_address = object->start_address;
			break;
		case MXT_SPT_COMCONFIG_T18:
			data->T18_address = object->start_address;
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_address = object->start_address;
			data->T19_reportid = min_id;
			break;			
		case MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24:
			data->T24_address = object->start_address;
			data->T24_reportid = min_id;
			break;			
		case MXT_SPT_SELFTEST_T25:
			data->T25_address = object->start_address;
			data->T25_reportid = min_id;
			break;
		case MXT_DEBUG_DIAGNOSTIC_T37:
			data->T37_address = object->start_address;
			break;	
		case MXT_SPT_USERDATA_T38:
			data->T38_address = object->start_address;
			break;			
		case MXT_PROCI_GRIPSUPPRESSION_T40:
			data->T40_address = object->start_address;
			break;
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_address = object->start_address;
			data->T42_reportid_min = min_id;
			data->T42_reportid_max = max_id;
			break;
		case MXT_SPARE_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_SPT_CTECONFIG_T46:
			data->T46_address = object->start_address;
			break;
		case MXT_PROCI_STYLUS_T47:
			data->T47_address = object->start_address;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T48:
			data->T48_reportid = min_id;
			break;
		case MXT_ADAPTIVE_T55:
			data->T55_address = object->start_address;
			break;			
		case MXT_PROCI_SHIELDLESS_T56:
			data->T56_address = object->start_address;
			break;
		case MXT_SPT_TIMER_T61:
			/* Only handle messages from first T63 instance */
			data->T61_address = object->start_address;
			data->T61_reportid_min = min_id;
			data->T61_reportid_max = max_id;
			data->T61_instances = mxt_obj_instances(object);
			break;
		case MXT_PROCI_ACTIVESTYLUS_T63:
			/* Only handle messages from first T63 instance */
			data->T63_reportid_min = min_id;
			data->T63_reportid_max = min_id;
			data->num_stylusids = 1;
			break;
		case MXT_PROCI_LENSBENDING_T65:
			data->T65_address = object->start_address;
			break;
		case MXT_SPARE_T68:
			data->T68_address = object->start_address;
			data->T68_reportid_min = min_id;
			data->T68_reportid_max = max_id;
			break;
		case MXT_SPT_DYNAMICCONFIGURATIONCONTAINER_T71:
			data->T71_address = object->start_address;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T72:
			data->T72_address = object->start_address;
			data->T72_reportid_min = min_id;
			data->T72_reportid_max = max_id;
			break;
		case MXT_PROCI_GLOVEDETECTION_T78:
			data->T78_address = object->start_address;
			break;
		case MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80:
			data->T80_address = object->start_address;
			break;
		case MXT_PROCI_UNLOCKGESTURE_T81:
			data->T81_address = object->start_address;
			data->T81_reportid_min = min_id;
			data->T81_reportid_max = max_id;
			break;
		case MXT_PROCI_GESTURE_T92:
			data->T92_address = object->start_address;
			data->T92_reportid = min_id;
			break;
		case MXT_PROCI_TOUCHSEQUENCELOGGER_T93:
			data->T93_address = object->start_address;
			data->T93_reportid = min_id;
			break;
		case MXT_TOUCH_SPT_PTC_TUNINGPARAMS_T96:
			data->T96_address = object->start_address;
			break;
		case MXT_TOUCH_PTC_KEYS_T97:
			data->T97_reportid_min = min_id;
			data->T97_reportid_max = max_id;
			data->T97_address = object->start_address;
			break;
		case MXT_PROCI_KEYGESTUREPROCESSOR_T99:
			data->T99_address = object->start_address;
			data->T99_reportid = min_id;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T100:
			data->T100_reportid_min = min_id;
			data->T100_reportid_max = max_id;
			data->T100_address = object->start_address;
			/* first two report IDs reserved */
			data->num_touchids = object->num_report_ids - 2;
			break;
		case MXT_SPT_SELFCAPHOVERCTECONFIG_T102:
			data->T102_address = object->start_address;
			data->T102_reportid = min_id;
			break;			
		case MXT_PROCI_AUXTOUCHCONFIG_T104:
			data->T104_address = object->start_address;
			break;
		case MXT_SPT_SELFCAPMEASURECONFIG_T113:
			data->T113_address = object->start_address;
			break;			
		case MXT_PROCI_SYMBOLGESTURE_T115:
			data->T115_address = object->start_address;
			data->T115_reportid = min_id;
			break;
		}

		end_address = object->start_address
			+ mxt_obj_size(object) * mxt_obj_instances(object) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T5 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(&client->dev, "Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "Failed to allocate message buffer\n");
		return -ENOMEM;
	}

	return 0;
}

static int mxt_read_info_block(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	size_t size;
	void *id_buf, *buf;
	uint8_t num_objects;
	u32 calculated_crc;
	u8 *crc_ptr;

	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	id_buf = kzalloc(size, GFP_KERNEL);
	if (!id_buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, 0, size, id_buf);
	if (error) {
		kfree(id_buf);
		return error;
	}

	/* Resize buffer to give space for rest of info block */
	num_objects = ((struct mxt_info *)id_buf)->object_num;
	size += (num_objects * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = krealloc(id_buf, size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Read rest of info block */
	error = __mxt_read_reg(client, MXT_OBJECT_START,
					size - MXT_OBJECT_START,
					buf + MXT_OBJECT_START);
	if (error)
		goto err_free_mem;

	/* Extract & calculate checksum */
	crc_ptr = buf + size - MXT_INFO_CHECKSUM_SIZE;
	data->info_crc = crc_ptr[0] | (crc_ptr[1] << 8) | (crc_ptr[2] << 16);

	calculated_crc = mxt_calculate_crc(buf, 0,
						size - MXT_INFO_CHECKSUM_SIZE);

	/* CRC mismatch can be caused by data corruption due to I2C comms
	 * issue or else device is not using Object Based Protocol */
	if ((data->info_crc == 0) || (data->info_crc != calculated_crc)) {
		dev_err(&client->dev,
			"Info Block CRC error calculated=0x%06X read=0x%06X\n",
			calculated_crc, data->info_crc);
		//dev_err(&client->dev, "info block size %d\n",size);
		print_hex_dump(KERN_ERR, "[mxt] INFO:", DUMP_PREFIX_NONE, 16, 1,
			buf, size, false);
		error = -EIO;
		goto err_free_mem;
	}

	/* Save pointers in device data structure */
	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;

	dev_info(&client->dev,
		 "Family: %u Variant: %u Firmware V%u.%u.%02X Objects: %u\n",
		 data->info->family_id, data->info->variant_id,
		 data->info->version >> 4, data->info->version & 0xf,
		 data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data, buf + MXT_OBJECT_START);
	if (error) {
		dev_err(&client->dev, "Error %d parsing object table\n", error);
		mxt_free_object_table(data);
		return error;
	}

	dev_info(&client->dev,
		 "T5 message size %d\n", data->T5_msg_size);

	data->object_table = (struct mxt_object *)(buf + MXT_OBJECT_START);
	return 0;

err_free_mem:
	kfree(buf);
	return error;
}

static int mxt_read_t9_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct t9_range range;
	unsigned char orient;
	struct mxt_object *object;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T9);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
				   object->start_address + MXT_T9_RANGE,
				   sizeof(range), &range);
	if (error)
		return error;

	le16_to_cpus(&range.x);
	le16_to_cpus(&range.y);

	error = __mxt_read_reg(client,
				object->start_address + MXT_T9_ORIENT,
				1, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (range.x == 0)
		range.x = 1023;

	if (range.y == 0)
		range.y = 1023;

	if (orient & MXT_T9_ORIENT_SWITCH) {
		data->max_x = range.y;
		data->max_y = range.x;
	} else {
		data->max_x = range.x;
		data->max_y = range.y;
	}

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	data->max_y_t = data->pdata->max_y_t;
	if (data->max_y_t > data->max_y)
		data->max_y_t = data->max_y;
#endif

	dev_info(&client->dev,
		 "Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static void mxt_regulator_enable(void * device)
{
    int rc;
	struct mxt_data *data = device;

    struct device *dev = &data->client->dev;
	if (!device)
		return;
    
	gpio_set_value(mxt_rst_number, 0);//gpio_set_value(data->pdata->gpio_reset, 0);

    
        if (regulator_count_voltages(data->reg_vdd) > 0)
        {
            rc = regulator_set_voltage(data->reg_vdd, 1800000, 1800000);
            if (rc)
            {
                dev_err(dev, "Regulator set i2c vtg failed rc=%d\n", rc);
             }
        }
        
        rc = reg_set_optimum_mode_check(data->reg_vdd, 10000);
        if (rc < 0)
        {
            dev_err(dev, "Regulator vcc_i2c set_opt failed rc=%d\n", rc);
         }
 
	rc = regulator_enable(data->reg_vdd);
    if (rc < 0)
	{
		dev_err(dev, "Regulator vdd(vcc_i2c) set_opt failed rc=%d\n", rc);
	}

	if (regulator_count_voltages(data->reg_avdd) > 0)
    {
		rc = regulator_set_voltage(data->reg_avdd, 3300000, 3300000);
		if (rc)
        {
			dev_err(dev, "Regulator set ana vtg failed rc=%d\n", rc);
		}
	}
    
    rc = reg_set_optimum_mode_check(data->reg_avdd, 15000);
    if (rc < 0)
    {
        dev_err(dev, "Regulator vcc_ana set_opt failed rc=%d\n", rc);
    }

	rc = regulator_enable(data->reg_avdd);
    if (rc < 0)
	{
		dev_err(dev, "Regulator avdd(vdd_ana) set_opt failed rc=%d\n", rc);
	}
	msleep(MXT_REGULATOR_DELAY);

	INIT_COMPLETION(data->bl_completion);
	gpio_set_value(mxt_rst_number, 1);//gpio_set_value(data->pdata->gpio_reset, 1);
	mxt_wait_for_completion(data, &data->bl_completion, MXT_POWERON_DELAY);
}

static void mxt_regulator_disable(void * device)
{
	struct mxt_data *data = device;

	if (!device)
		return;

	//regulator_disable(data->reg_vdd);
	regulator_disable(data->reg_avdd);
}

static void mxt_probe_regulators(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;
/*** ZTEMT Add, 2015/01/09 ***/
#if 0//def CONFIG_ZTEMT_HW_VERSION
        int ztemt_hw_bl_id = 0;
#endif
/*** ZTEMT end ***/

	/* According to maXTouch power sequencing specification, RESET line
	 * must be kept low until some time after regulators come up to
	 * voltage */

	board_gpio_init(data->pdata);
	if(atomic_read(&data->depth) > 0) {
		board_disable_irq(data->pdata,data->irq);
		atomic_dec(&data->depth);
	}

	if (!mxt_rst_number) {//if (!data->pdata->gpio_reset) {
		dev_warn(dev, "Must have reset GPIO to use regulator support\n");
		goto fail;
	}

/*** ZTEMT Add, 2015/01/09 ***/
#if 0 //def CONFIG_ZTEMT_HW_VERSION
        ztemt_hw_bl_id = ztemt_get_hw_id();
                if(ztemt_hw_bl_id){ 
                    data->reg_vdd = regulator_get(dev, "vdd2");
                    dev_err(dev, "ZTEMT  ztemt_get_hw_id = B\n");
                }else{
                    data->reg_vdd = regulator_get(dev, "vdd");
                    dev_err(dev, "ZTEMT  ztemt_get_hw_id = A\n");
                }
#else 
	data->reg_vdd = regulator_get(dev, "vdd");
#endif
/*** ZTEMT end ***/
	if (IS_ERR(data->reg_vdd)) {
		error = PTR_ERR(data->reg_vdd);
		dev_err(dev, "Error %d getting vdd regulator\n", error);
		goto fail;
	}

	data->reg_avdd = regulator_get(dev, "avdd");
	if (IS_ERR(data->reg_vdd)) {
		error = PTR_ERR(data->reg_vdd);
		dev_err(dev, "Error %d getting avdd regulator\n", error);
		goto fail_release;
	}

	//no need use regulator for suspend and resume, set false here
	data->use_regulator = false;
	mxt_regulator_enable(data);

	dev_dbg(dev, "Initialised regulators\n");
	return;

fail_release:
		regulator_put(data->reg_vdd);
fail:
	data->reg_vdd = NULL;
	data->reg_avdd = NULL;
	data->use_regulator = false;
}

static int mxt_read_t100_config(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct mxt_object *object;
	u16 range_x, range_y;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
				   object->start_address + MXT_T100_XRANGE,
				   sizeof(range_x), &range_x);
	if (error)
		return error;

	le16_to_cpus(&range_x);

	error = __mxt_read_reg(client,
					object->start_address + MXT_T100_YRANGE,
					sizeof(range_y), &range_y);
	if (error)
		return error;

	le16_to_cpus(&range_y);

	error = __mxt_read_reg(client,
				object->start_address,
				sizeof(data->tchcfg), &data->tchcfg);
	if (error)
		return error;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	if (range_y == 0)
		range_y = 1023;

	if (test_flag_8bit(MXT_T100_CFG_SWITCHXY, &data->tchcfg[MXT_T100_CFG1])) {
		data->max_x = range_y;
		data->max_y = range_x;
	} else {
		data->max_x = range_x;
		data->max_y = range_y;
	}

	/* allocate aux bytes */

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	data->max_y_t = data->pdata->max_y_t;
	if (data->max_y_t > data->max_y)
		data->max_y_t = data->max_y;
#endif
	dev_info(&client->dev,
		 "T100 Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static int mxt_input_open(struct input_dev *dev);
static void mxt_input_close(struct input_dev *dev);

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;
#if !defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	int i,j;
#endif
/*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
//	struct device *dev = &data->client->dev;
	struct input_dev *input_rim_dev;
//	int error;
#endif
/*ZTEMT end 20140210*/

	error = mxt_read_t100_config(data);
	if (error)
		dev_warn(dev, "Failed to initialize T100 resolution\n");

	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = "atmel_mxt_ts_T100_touchscreen";

	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	set_bit(EV_ABS, input_dev->evbit);
//	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
#if defined(INPUT_PROP_DIRECT)
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	/* For single touch */
/*
	input_set_abs_params(input_dev, ABS_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_dev, ABS_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_Y,
				 0, data->max_y, 0, 0);
#endif

	if ((test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX])))
		input_set_abs_params(input_dev, ABS_PRESSURE,
					 0, 255, 0, 0);
*/
	/* For multi touch */
	error = input_mt_init_slots(input_dev, data->num_touchids,0/*, INPUT_MT_DIRECT*/);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	//input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				 0, data->max_y, 0, 0);
#endif

	if (test_flag_8bit(MXT_T100_TCHAUX_AREA | MXT_T100_TCHAUX_AREAHW, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
					 0, MXT_MAX_AREA, 0, 0);

	if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_dev, ABS_MT_PRESSURE,
					 0, 255, 0, 0);

	if (test_flag_8bit(MXT_T100_TCHAUX_VECT, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
					 0, 255, 0, 0);

	/* For T15 key array */
	data->t15_keystatus = 0;
	/* For Key register */
#if !defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	i = j;	
	if (data->pdata->keymap && data->pdata->num_keys) {
		for (i = 0; i < NUM_KEY_TYPE; i++) {
			for (j = 0; j < data->pdata->num_keys[i]; j++) {
				set_bit(data->pdata->keymap[i][j], input_dev->keybit);
				input_set_capability(input_dev, EV_KEY,
							 data->pdata->keymap[i][j]);
			}
		}
	}
#endif

#if defined(ZTEMT_ATMEL_PALM_KEY_SUPPORT)
    set_bit(KEY_PALM_LOCK, input_dev->keybit);
    input_set_capability(input_dev, EV_KEY, KEY_PALM_LOCK);
#endif

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

/*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
	input_rim_dev = input_allocate_device();
	if (!data || !input_rim_dev) {
		dev_err(dev, "Failed to allocate memory input_rim_dev\n");
		return -ENOMEM;
	}

	input_rim_dev->name = "atmel_mxt_ts_T100_touchscreen_rim";

	input_rim_dev->phys = data->phys;
	input_rim_dev->id.bustype = BUS_I2C;
	input_rim_dev->dev.parent = &data->client->dev;
	input_rim_dev->open = mxt_input_open;
	input_rim_dev->close = mxt_input_close;

	set_bit(EV_ABS, input_rim_dev->evbit);
//	input_set_capability(input_rim_dev, EV_KEY, BTN_TOUCH);
#if defined(INPUT_PROP_DIRECT)
	set_bit(INPUT_PROP_DIRECT, input_rim_dev->propbit);
#endif

	/* For single touch */
/*
	input_set_abs_params(input_rim_dev, ABS_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_rim_dev, ABS_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_rim_dev, ABS_Y,
				 0, data->max_y, 0, 0);
#endif

	if ((test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX])))
		input_set_abs_params(input_rim_dev, ABS_PRESSURE,
					 0, 255, 0, 0);
*/
	/* For multi touch */
	error = input_mt_init_slots(input_rim_dev, data->num_touchids,0/*, INPUT_MT_DIRECT*/);
	if (error) {
		dev_err(dev, "Error %d initialising slots input_rim_dev\n", error);
		goto err_free_mem;
	}

	//input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(input_rim_dev, ABS_MT_POSITION_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_rim_dev, ABS_MT_POSITION_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_rim_dev, ABS_MT_POSITION_Y,
				 0, data->max_y, 0, 0);
#endif

	if (test_flag_8bit(MXT_T100_TCHAUX_AREA | MXT_T100_TCHAUX_AREAHW, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_rim_dev, ABS_MT_TOUCH_MAJOR,
					 0, MXT_MAX_AREA, 0, 0);

	if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_rim_dev, ABS_MT_PRESSURE,
					 0, 255, 0, 0);

	if (test_flag_8bit(MXT_T100_TCHAUX_VECT, &data->tchcfg[MXT_T100_TCHAUX]))
		input_set_abs_params(input_rim_dev, ABS_MT_ORIENTATION,
					 0, 255, 0, 0);

	/* For T15 key array */
	data->t15_keystatus = 0;
	/* For Key register */
#if !defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	i = j;	
	if (data->pdata->keymap && data->pdata->num_keys) {
		for (i = 0; i < NUM_KEY_TYPE; i++) {
			for (j = 0; j < data->pdata->num_keys[i]; j++) {
				set_bit(data->pdata->keymap[i][j], input_rim_dev->keybit);
				input_set_capability(input_rim_dev, EV_KEY,
							 data->pdata->keymap[i][j]);
			}
		}
	}
#endif

#if defined(ZTEMT_ATMEL_PALM_KEY_SUPPORT)
    set_bit(KEY_PALM_LOCK, input_rim_dev->keybit);
    input_set_capability(input_rim_dev, EV_KEY, KEY_PALM_LOCK);
#endif

	input_set_drvdata(input_rim_dev, data);

	error = input_register_device(input_rim_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device input_rim_dev\n", error);
		goto err_free_mem;
	}

	data->input_rim_dev = input_rim_dev;
#endif
/*ZTEMT end 20140210*/

	return 0;

err_free_mem:
	input_free_device(input_dev);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    input_free_device(input_rim_dev);
#endif
	return error;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data);
static int mxt_configure_objects(struct mxt_data *data);

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	bool alt_bootloader_addr = false;
	bool retry = false;
#if defined(CONFIG_MXT_PROBE_ALTERNATIVE_CHIP)	
	unsigned short addr_bak = 0;
	int probe_retry = 0;
#endif
//dev_info(&client->dev, "lee atmel driver  mxt_initialize\n");

retry_info:
	error = mxt_read_info_block(data);
#if defined(CONFIG_MXT_PROBE_ALTERNATIVE_CHIP)
	while(error) {
#   if defined(CONFIG_MXT_POWER_CONTROL_SUPPORT_AT_PROBE)
		board_gpio_init(data->pdata);
#   endif
		client->addr = mxt_lookup_chip_address(data, probe_retry);
#if defined(CONFIG_MXT_I2C_DMA)
		client->addr |= I2C_RS_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG;
#endif
		if (!addr_bak)
			addr_bak = client->addr;
		error = mxt_read_info_block(data);

		if (++probe_retry > CONFIG_MXT_PROBE_ALTERNATIVE_CHIP)
			break;
	}
	if (error)
		client->addr = addr_bak;
#endif
	if (error) {
retry_bootloader:
		error = mxt_probe_bootloader(data, alt_bootloader_addr);
		if (error) {
			if (alt_bootloader_addr) {
				/* Chip is not in appmode or bootloader mode */
				return error;
			}

			dev_info(&client->dev, "Trying alternate bootloader address\n");
			alt_bootloader_addr = true;
			goto retry_bootloader;
		} else {
			if (retry) {
				dev_err(&client->dev,
						"Could not recover device from "
						"bootloader mode\n");
				/* this is not an error state, we can reflash
				 * from here */
				data->in_bootloader = true;
				return 0;
			}

			/* Attempt to exit bootloader into app mode */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FW_RESET_TIME);
			retry = true;
			goto retry_info;
		}
	}

#if defined(CONFIG_MXT_FORCE_RESET_AT_POWERUP)
	dev_warn(&client->dev, "board force a reset after gpio init\n");
	mxt_soft_reset(data);
#endif

	error = mxt_check_retrigen(data);
	if (error)
		return error;

	//  enable irq at last step, which make config crc at zero
	board_init_irq(data->pdata);

	error = mxt_acquire_irq(data);
	if (error)
		return error;

	error = mxt_configure_objects(data);
	if (error)
		return error;

	return 0;
}

static int mxt_configure_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
//    dev_info(&client->dev, "lee atmel driver :mxt_configure_objects\n");

	error = mxt_debug_msg_init(data);
	if (error)
		return error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	error = mxt_plugin_pre_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d plugin init\n",
			error);
		return error;
	}
#endif
	/* Check register init values */
	error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d initialising configuration, do a soft reset\n",
			error);
		//don't return error whether config failed, just send a reset command
		mxt_soft_reset(data);
		//return error;
	}

	if (data->T9_reportid_min) {
		error = mxt_initialize_t9_input_device(data);
		if (error)
			return error;
	} else if (data->T100_reportid_min) {
		error = mxt_initialize_t100_input_device(data);
		if (error)
			return error;
	} else {
		dev_warn(&client->dev, "No touch object detected\n");
	}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	error = mxt_plugin_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d plugin init\n",
			error);
		return error;
	}
#endif
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	data->properties_kobj = create_virtual_key_object(&client->dev);
#endif
	data->enable_reporting = true;
	return 0;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	if (!data->info)
		return 0;
		
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 data->info->version >> 4, data->info->version & 0xf,
			 data->info->build);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);

	if (!data->info)
		return 0;
	
	return scnprintf(buf, PAGE_SIZE, "%02x.%02x\n",
			data->info->family_id, data->info->variant_id);
}

static ssize_t mxt_show_instance(char *buf, int count,
				 struct mxt_object *object, int instance,
				 const u8 *val)
{
	int i;

	if (mxt_obj_instances(object) > 1)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "Instance %u\n", instance);

	for (i = 0; i < mxt_obj_size(object); i++)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\t[%2u]: %02x (%d)\n", i, val[i], val[i]);
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t mxt_object_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 *obuf;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	error = 0;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"T%u:\n", object->type);

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				goto done;

			count = mxt_show_instance(buf, count, object, j, obuf);
		}
	}

done:
	kfree(obuf);
	return error ?: count;
}

static int mxt_check_firmware_format(struct device *dev,
					 const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/* To convert file try
	 * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw */
	dev_err(dev, "Aborting: firmware file must be in binary format\n");

	return -1;
}

static int mxt_load_fw(struct device *dev)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_platform_data *pdata = data->pdata;
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;

	if (!data->fw_name)
		return -EEXIST;

	dev_info(dev, "mxt_load_fw %s\n",data->fw_name);
	
	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", data->fw_name);
		return ret;
	}

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret)
		goto release_firmware;

	if (data->suspended) {
		if (data->use_regulator)
			mxt_regulator_enable(data);

		if(atomic_read(&data->depth) <= 0) {
			atomic_inc(&data->depth);
			board_enable_irq(pdata,data->irq);
		}
		
		data->suspended = false;
	}
	if (data->in_bootloader) {
		ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
		if(ret) {
			dev_err(dev, "false bootloader check %d\n", ret);
			data->in_bootloader = false;
		}
	}

	if (!data->in_bootloader) {
		/* Change to the bootloader mode */		
		mxt_set_retrigen(data);
		
		data->in_bootloader = true;

		ret = mxt_t6_command(data, MXT_COMMAND_RESET,
					 MXT_BOOT_VALUE, false);
		if (ret) {
			dev_err(dev, "reset to boot loader mode return %d\n", ret);
			//don't return failed, maybe it's in bootloader mode
			//goto release_firmware;
		}
		msleep(MXT_RESET_TIME);

		mxt_lookup_bootloader_address(data, 0);

		/* At this stage, do not need to scan since we know
		 * family ID */
		do {
			ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
			if(ret == 0)
				break;
			dev_err(dev, "mxt_bootloader_read failed %d retry %d\n", ret,retry);
			mxt_lookup_bootloader_address(data, retry);
		}while(++retry <= 3);
		
		if (ret) {
			data->in_bootloader = false;
			goto release_firmware;
		}
	}

	if(atomic_read(&data->depth) <= 0) {
		atomic_inc(&data->depth);
		board_enable_irq(pdata,data->irq);
	}

	mxt_free_object_table(data);
	INIT_COMPLETION(data->bl_completion);
	
	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		if (ret) {
			data->in_bootloader = false;
			goto disable_irq;
		}
		complete(&data->bl_completion);
	} else {
		dev_info(dev, "Unlocking bootloader\n");

		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		if (ret) {
			data->in_bootloader = false;
			goto disable_irq;
		}
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret)
			goto disable_irq;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, fw->data + pos, frame_size);
		if (ret)
			goto disable_irq;

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto disable_irq;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}

		if (frame % 50 == 0)
			dev_info(dev, "Sent %d frames, %d/%zd bytes\n",
				 frame, pos, fw->size);

	}

	/* Wait for flash. */
	INIT_COMPLETION(data->bl_completion);
	ret = mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);
	/*
	if (ret)
		goto disable_irq;
	*/
	
	dev_info(dev, "Sent %ud frames, %ud bytes\n", frame, pos);

	/* Wait for device to reset. Some bootloader versions do not assert
	 * the CHG line after bootloading has finished, so ignore error */
	INIT_COMPLETION(data->bl_completion);
	ret = mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);

	data->in_bootloader = false;

disable_irq:
	if(atomic_read(&data->depth) > 0) {
		board_disable_irq(data->pdata,data->irq);
		atomic_dec(&data->depth);
	}
#if defined(CONFIG_MXT_IRQ_NESTED)
	if(test_and_clear_bit(MXT_EVENT_IRQ,&data->busy)) {
		mxt_acquire_irq(data);
	}
#endif
	data->busy = 0;
release_firmware:
	release_firmware(fw);
	return ret;
}

static int mxt_update_file_name(struct device *dev, char **file_name,
				const char *buf, size_t count, unsigned long alternative)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char *file_name_tmp;
	char suffix[16];
	int size;
	u8 cmd = alternative & 0xF;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	char *sc;
 	int ret; 
#endif
	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	size = count + 1 + sizeof(suffix);
	file_name_tmp = krealloc(*file_name,size , GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	if (cmd == 1) {
		snprintf(suffix, sizeof(suffix), ".%02X.%02X.cfg",
			(u8)(data->client->addr & 0x7f),
			(data->t19_msg[0]>>2) & 0xf);
		strncat((*file_name),suffix,size);
	}else if (cmd == 2){
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		suffix[0] = '\0';
		if (data->plug.suffix_pid_name[0] == '\0') {
			ret = mxt_plugin_get_pid_name(&data->plug, suffix, sizeof(suffix));
			if (ret == 0) {
				if (suffix[0] != '\0') {
					scnprintf(data->plug.suffix_pid_name, sizeof(data->plug.suffix_pid_name), "%s", suffix);
				}
			}
		}else {
			strncpy(suffix, data->plug.suffix_pid_name, sizeof(suffix));
		}

		if (suffix[0] != '\0') {
			sc = strpbrk(suffix, ".");
			if (test_flag(ALT_CHIP_BIT_FW, &alternative)) {
				if (sc) {
					if (*sc != '\0') {
						strncat((*file_name), sc, size);
					}
				}
			}else {
				if(sc) {
					if (sc != suffix) {  //may no cfg name
						*sc = '\0';
						strncat((*file_name), ".", size);
						strncat((*file_name), suffix, size);
					}
				}else {
					strncat((*file_name), ".", size);
					strncat((*file_name), suffix, size);
				}
			}
		}
#endif
	}

	return 0;
}

static int mxt_update_cfg_name_by_fw_name(struct device *dev, char **file_name,
				const char *buf, size_t count)
{
	char *file_name_tmp;

	/*
	if (*file_name) {
		dev_warn(dev, "File name exist %s\n", *file_name);
		return 0;
	}
	*/

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	if (count > 3) {
		if (!strcmp(&buf[count - 3], ".fw"))
			count -= 3;
	}
	
	file_name_tmp = krealloc(*file_name, count + 1 + 4, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);
	memcpy(*file_name + count, ".cfg", 4);
	count += 4;

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	return 0;
}

int mxt_check_firmware_version(struct mxt_data *data, const char *version_str)
{
	struct device *dev = &data->client->dev;
	char firmware_version[64];
	u8 family_id,variant_id,version,version2,build;

	if (data->info == NULL)
		return 0;

	snprintf(firmware_version, 64, "%02X_%02X_%u.%u_%02X.fw",
		   data->info->family_id,
		   data->info->variant_id,
		   (data->info->version & 0xF0) >> 4,
		   (data->info->version & 0x0F),
		   data->info->build);

	dev_info(dev, "[mxt] firmware version %s - %s\n",version_str, firmware_version);

	if (!strncmp(firmware_version, version_str, strlen(firmware_version)))
		return -EEXIST;

	family_id = data->info->family_id;
	variant_id = data->info->variant_id;
	if(sscanf(version_str,"%02hhX_%02hhX_%hhu.%hhu_%02hhX.fw",
		&family_id,
		&variant_id,
		&version,
		&version2,
		&build) >= 2) {
		if(data->info->family_id != family_id ||
			data->info->variant_id != variant_id) {
			dev_info(dev, "Check chip version mismatch: %02X %02X %u.%u.%02X\n",
				family_id, variant_id,version,version2,build);
			return -ENXIO;
		}
	}

	return 0;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;

	dev_info(dev, "mxt_update_fw_store\n");

	error = mxt_update_file_name(dev, &data->fw_name, buf, count, /*false*/data->alt_chip | ALT_CHIP_BIT_FW);
	if (error)
		return error;

	error = mxt_check_firmware_version(data,data->fw_name);
	if (error)
		return error;

	error = mxt_update_cfg_name_by_fw_name(dev, &data->cfg_name, data->fw_name, strlen(data->fw_name));
	if (error)
		return error;

	//lock it for disable outside access
	mutex_lock(&data->access_mutex);
	error = mxt_load_fw(dev);
	mutex_unlock(&data->access_mutex);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		data->suspended = false;

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	return count;
}

static ssize_t mxt_update_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);	
	int wait = 10;
	int ret;

	dev_info(dev, "mxt_update_cfg_store\n");

	if (data->in_bootloader) {
		dev_err(dev, "Not in appmode\n");
		return -EINVAL;
	}

	if (!data->object_table) {
		dev_err(dev, "Not initialized\n");
		return -EINVAL;
	}

	ret = mxt_update_file_name(dev, &data->cfg_name, buf, count, data->alt_chip);
	if (ret)
		return ret;

	data->enable_reporting = false;
	//there wait for a while after report disable
	do{
		if (!(test_bit(MXT_EVENT_IRQ,&data->busy)||test_bit(MXT_EVENT_EXTERN,&data->busy)))
			break;

		msleep(100);
		dev_err(dev, "wait interrupt release %d\n",wait);
	}while(--wait >= 0);
#if defined(CONFIG_MXT_IRQ_NESTED)
	if(test_and_clear_bit(MXT_EVENT_IRQ,&data->busy)) {
		mxt_acquire_irq(data);
	}
#endif
	data->busy = 0;

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_force_stop(&data->plug);
	mxt_plugin_deinit(&data->plug);
#endif
	mxt_free_input_device(data);

	if (data->suspended) {
		if (data->use_regulator)
			mxt_regulator_enable(data);
		/*else
			mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		*/
		mxt_acquire_irq(data);

		data->suspended = false;
	}

	ret = mxt_configure_objects(data);
	if (ret)
		goto out;

	ret = count;

	//mxt_acquire_irq(data);

#if defined(CONFIG_MXT_SELFCAP_TUNE)
	//mxt_self_tune(data, false);
#endif
out:
	return ret;
}

static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char c;

	c = data->debug_enabled ? '1' : '0';
	return scnprintf(buf, PAGE_SIZE, "%c\n", c);
}

static ssize_t mxt_debug_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t mxt_debug_v2_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1)
			mxt_debug_msg_enable(data);
		else
			mxt_debug_msg_disable(data);

		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_bootloader_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char c;

	c = data->in_bootloader ? '1' : '0';
	return scnprintf(buf, PAGE_SIZE, "%c\n", c);
}

static ssize_t mxt_bootloader_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->in_bootloader = (i == 1);

		dev_dbg(dev, "%s\n", i ? "in bootloader" : "app mode");
		return count;
	} else {
		dev_dbg(dev, "in_bootloader write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
						size_t *count, int max_size)
{
	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > max_size)
		*count = max_size;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count, MXT_MAX_BLOCK_READ);
	if (ret < 0)
		return ret;

	mutex_lock(&data->access_mutex);
	
	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	mutex_unlock(&data->access_mutex);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count, MXT_MAX_BLOCK_WRITE);
	if (ret < 0)
		return ret;

	mutex_lock(&data->access_mutex);
	
	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);
	
	mutex_unlock(&data->access_mutex);

	return ret == 0 ? count : 0;
}

/*** ZTEMT start ,20150123***/

//add by ZTE ,20150205
static ssize_t mxt_manual_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int rc = 0;
    dev_err(dev, "start mxt_manual_cali_show \n");

    rc =mxt_cmd_store(dev, attr, "2", 1);   //msc ptc tune 3
    if(rc < 0){
        dev_dbg(dev, "mxt_manual_cali_show write error\n");
        return -EINVAL;
    }
    msleep(300);
    return snprintf(buf, PAGE_SIZE, "0\n");//PASS
}
//end 20150205
static ssize_t mxt_ic_detect_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t offset = 0;
    int rc = 0;

    rc = mxt_t25_selftest_store(dev, attr, "0xFE", 4);
    if(rc < 0){
		dev_dbg(dev, "mxt_t25_selftest_store write error\n");
		return -EINVAL;
	}
    msleep(300);
    
	if (data->t25_msg[0] == 0xFE)
		offset += scnprintf(buf, PAGE_SIZE, "0\n");//PASS
	else
		offset += scnprintf(buf, PAGE_SIZE, "1\n");//FAILED
/*
	offset += scnprintf(buf + offset, PAGE_SIZE, "%x %x %x %x %x %x\n",
		 data->t25_msg[0],
		 data->t25_msg[1],
		 data->t25_msg[2],
		 data->t25_msg[3],
		 data->t25_msg[4],
		 data->t25_msg[5]);
*/
	return offset;

}

static ssize_t mxt_easy_wakeup_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    //struct mxt_data *data = dev_get_drvdata(dev);
   // struct plug_interface *pl = &data->plug;
    int rc = 0;
	u32 cmd;
   // char *buf1;
    dev_err(dev, "set mxt_easy_wakeup_gesture \n");

	if (sscanf(buf, "%x", &cmd) == 1) {
        dev_err(dev, "mxt_easy_wakeup_gesture_store write cmd %x \n",cmd);
		if(cmd == 1){
            //echo "TAP 1" > gesture_list
                rc = mxt_plugin_gesture_list_store(dev, attr, "TAP 1", 5);
                if(rc < 0){
                    msleep(50);
                    rc = mxt_plugin_gesture_list_store(dev, attr, "TAP 1", 5);
                    if(rc < 0){
                        dev_err(dev, "mxt_plugin_gesture_list_store write 1 error\n");
                        //return -EINVAL;
                    }
                }
                msleep(500);

                //echo "1" > en_gesture
                rc = mxt_plugin_wakeup_gesture_store(dev, attr, "1", 1);
                if(rc < 0){
                    dev_err(dev, "mxt_plugin_wakeup_gesture_store write 1 error\n");
                    msleep(3000);
                    rc = mxt_plugin_wakeup_gesture_store(dev, attr, "1", 1);
                    if(rc < 0){
                        dev_err(dev, "mxt_plugin_wakeup_gesture_store write error 1\n");
                        return -EINVAL;
                    }
                }

        }else{
            //echo "TAP 0" > gesture_list
                rc = mxt_plugin_gesture_list_store(dev, attr, "TAP 2", 5);
                if(rc < 0){
                    msleep(50);
                    rc = mxt_plugin_gesture_list_store(dev, attr, "TAP 2", 5);
                    if(rc < 0){
                        dev_err(dev, "mxt_plugin_gesture_list_store write 2 error\n");
                        //return -EINVAL;
                    }
                }
                msleep(500);
#if 1
                rc = mxt_plugin_wakeup_gesture_store(dev, attr, "0", 1);
                if(rc < 0){
                    msleep(3000);
                    rc = mxt_plugin_wakeup_gesture_store(dev, attr, "0", 1);
                    if(rc < 0){
                        dev_err(dev, "mxt_plugin_wakeup_gesture_store write 0 error\n");
                        return -EINVAL;
                    }
                }
#else
/*** ZTEMT add , judge all of gesture ***/

                if (!pl->inited)
                    return -ENODEV;
                dev_err(dev, "zte_plugin_proci_pi_gesture_show \n");
                rc = zte_plugin_proci_pi_gesture_show(pl->pi, buf1, PAGE_SIZE);
                if(rc != 1){
                      //echo "0" > en_gesture
                      dev_err(dev, "mxt_plugin_wakeup_gesture_store echo 0 > en_gesture\n");
                      rc = mxt_plugin_wakeup_gesture_store(dev, attr, "0", 1);
                      if(rc < 0){
                          dev_err(dev, "mxt_plugin_wakeup_gesture_store write 0 error\n");
                          return -EINVAL;
                      }
                }
#endif
/*ZTEMT end*/
        }
        return count;

	} else {
		dev_dbg(dev, "mxt_easy_wakeup_gesture_store cmd error\n");
		return -EINVAL;
	}
}

//add by ZTE ,20150206
/*
static ssize_t mxt_slide_switch_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct mxt_data *data = dev_get_drvdata(dev);
    struct plug_interface *pl = &data->plug;
    int rc = 0;
	u32 cmd;
    char *buf1;

	if (sscanf(buf, "%x", &cmd) == 1) {
        dev_err(dev, "mxt_slide_switch_gesture_store write cmd %x \n",cmd);
		if(cmd == 1){
            //echo "TAP 1" > gesture_list
                rc = mxt_plugin_gesture_list_store(dev, attr, "UNLOCK0 1;UNLOCK1 1;", 20);
                if(rc < 0){
                    dev_err(dev, "mxt_slide_switch_gesture_store write error\n");
                    return -EINVAL;
                }
                msleep(500);

                //echo "1" > en_gesture
                rc = mxt_plugin_wakeup_gesture_store(dev, attr, "1", 1);
                if(rc < 0){
                    dev_err(dev, "mxt_slide_switch_gesture_store write error\n");
                    return -EINVAL;
                }

        }else{
            //echo "TAP 0" > gesture_list
                rc = mxt_plugin_gesture_list_store(dev, attr, "UNLOCK0 2;UNLOCK1 2;", 20);
                if(rc < 0){
                    dev_dbg(dev, "mxt_slide_switch_gesture_store write error\n");
                    return -EINVAL;
                }
                
                msleep(500);
                
//judge all of gesture
                if (!pl->inited)
                    return -ENODEV;
                dev_err(dev, "zte_plugin_proci_pi_gesture_show \n");
                rc = zte_plugin_proci_pi_gesture_show(pl->pi, buf1, PAGE_SIZE);
                if(rc != 1){
                      //echo "0" > en_gesture
                      dev_err(dev, "mxt_slide_switch_gesture_store echo 0 > en_gesture\n");
                      rc = mxt_plugin_wakeup_gesture_store(dev, attr, "0", 1);
                      if(rc < 0){
                          dev_err(dev, "mxt_slide_switch_gesture_store write 0 error\n");
                          return -EINVAL;
                      }
                }
         }
        return count;

	} else {
		dev_dbg(dev, "mxt_easy_wakeup_gesture_store cmd error\n");
		return -EINVAL;
	}
}
*/
//ZTE end ,20150206

static int tm_value = -1;
static ssize_t mxt_touch_mode_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n",tm_value);
}

static ssize_t mxt_touch_mode_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    //struct mxt_data *data = dev_get_drvdata(dev);

    int inv = -1,ret = -1;
    ret = sscanf(buf,"%d",&inv);
    if( ret == 1 )
    {
       switch(inv)
       {
           case 0:
           case 1:
               ret = mxt_plugin_glove_store(dev,attr,"0",1);
               if( ret == 1)
                   tm_value = inv ;
               break;
           case 2:
               ret = mxt_plugin_glove_store(dev,attr,"1",1);
               if ( ret == 1 )
                   tm_value = inv ;
                break;
           default:
                break;
       }
    }
    dev_info(dev, "set touch_mode in: %d, ret: %d, tm: %d \n", inv, ret, tm_value);

    return count;

}

/*ZTEMT end*/

//ZTE end ,20150529

#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
static ssize_t mxt_c_zone_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    struct mxt_data *data = dev_get_drvdata(dev);
	int value = 0;
    dev_info(dev, "mxt_c_zone_show \n");

    value = data->c_zone_flag;
    if (value < 0) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return snprintf(buf, PAGE_SIZE, "error\n");
    }
    return snprintf(buf, PAGE_SIZE, "0x%02X\n",value);
}

static ssize_t mxt_c_zone_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct mxt_data *data = dev_get_drvdata(dev);
	int rc = 0;
	int value = 1;
    dev_info(dev, "mxt_c_zone_store \n");

	rc = sscanf(buf, "%x", &value);
	if (rc != 1) {
		dev_err(dev, "%s: Invalid value\n", __func__);
		return count;
	}
   
    data->c_zone_flag = value;
    
    return count;
}
#endif
/*ZTEMT end*/

static DEVICE_ATTR(fw_version, S_IRUSR|S_IRGRP|S_IROTH, mxt_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUSR|S_IRGRP|S_IROTH, mxt_hw_version_show, NULL);
static DEVICE_ATTR(object, S_IRUSR|S_IRGRP|S_IROTH, mxt_object_show, NULL);
static DEVICE_ATTR(update_fw0, S_IWUSR|S_IXUSR|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH , NULL, mxt_update_fw_store);
static DEVICE_ATTR(update_cfg0, S_IWUSR|S_IXUSR|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH , NULL, mxt_update_cfg_store);
static DEVICE_ATTR(debug_v2_enable, S_IWUSR|S_IXUSR|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH , NULL, mxt_debug_v2_enable_store);
static DEVICE_ATTR(debug_notify, S_IRUSR|S_IRGRP|S_IROTH, mxt_debug_notify_show, NULL);
static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR | S_IWGRP | S_IWOTH, mxt_debug_enable_show, mxt_debug_enable_store);
static DEVICE_ATTR(bootloader, S_IWUSR | S_IRUSR | S_IWGRP | S_IWOTH, mxt_bootloader_show, mxt_bootloader_store);
static DEVICE_ATTR(t19, S_IWUSR | S_IRUSR | S_IWGRP | S_IWOTH , mxt_t19_gpio_show, mxt_t19_gpio_store);
static DEVICE_ATTR(t25, S_IWUSR | S_IRUSR | S_IWGRP | S_IWOTH, mxt_t25_selftest_show, mxt_t25_selftest_store);
static DEVICE_ATTR(cmd, S_IWUSR, NULL,mxt_cmd_store);
/*** ZTEMT start ,20150123***/
static DEVICE_ATTR(ic_detect, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, mxt_ic_detect_show,NULL);
static DEVICE_ATTR(manual_cali, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, mxt_manual_cali_show,NULL);
static DEVICE_ATTR(easy_wakeup_gesture, S_IWUSR|S_IXUSR|S_IWGRP|S_IXGRP|S_IWOTH|S_IXOTH, NULL, mxt_easy_wakeup_gesture_store);//mxt_plugin_wakeup_gesture_show
//static DEVICE_ATTR(slide_switch_gesture, S_IWUSR|S_IWGRP|S_IWOTH, NULL, mxt_slide_switch_gesture_store);//add by ZTE ,20150206
static DEVICE_ATTR(touch_mode, S_IRWXU | S_IRWXG | S_IRWXO, mxt_touch_mode_show, mxt_touch_mode_store);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
static DEVICE_ATTR(c_zone, S_IRWXU | S_IRWXG | S_IRWXO, mxt_c_zone_show, mxt_c_zone_store);
#endif
/*ZTEMT end*/
static DEVICE_ATTR(depth, S_IRWXU | S_IRWXG | S_IRWXO, mxt_irq_depth_show, mxt_irq_depth_store);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
static DEVICE_ATTR(plugin, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_show, mxt_plugin_store);
static DEVICE_ATTR(plugin_tag, S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, mxt_plugin_tag_show, NULL);
/*** ZTEMT start,cut down UID detect ***/
//static DEVICE_ATTR(plugin_tag_uid, S_IRUSR|S_IRGRP|S_IROTH, mxt_plugin_tag_uid_show, NULL);//add by ZTE ,20150414
/*ZTEMT end*/
#	if defined(CONFIG_MXT_CAL_WORKAROUND)
	static DEVICE_ATTR(cal, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_cal_show, mxt_plugin_cal_store);
#     endif

#	if defined(CONFIG_MXT_PI_WORKAROUND)
static DEVICE_ATTR(en_glove, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_glove_show, mxt_plugin_glove_store);
static DEVICE_ATTR(en_stylus, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_stylus_show, mxt_plugin_stylus_store);
static DEVICE_ATTR(en_gesture, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_wakeup_gesture_show, mxt_plugin_wakeup_gesture_store);
static DEVICE_ATTR(gesture_list, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_gesture_list_show, mxt_plugin_gesture_list_store);
static DEVICE_ATTR(gesture_trace, S_IRUSR|S_IRGRP|S_IROTH, mxt_plugin_gesture_trace_show, NULL);
#	endif
#	if defined(CONFIG_MXT_MISC_WORKAROUND)
	static DEVICE_ATTR(misc, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_misc_show, mxt_plugin_misc_store);
#     endif
#	if defined(CONFIG_MXT_CLIP_WORKAROUND)
	static DEVICE_ATTR(clip, S_IRWXU | S_IRWXG | S_IRWXO, mxt_plugin_clip_show, mxt_plugin_clip_store);
	static DEVICE_ATTR(clip_tag, S_IRUSR|S_IRGRP|S_IROTH, mxt_plugin_clip_tag_show, NULL);
#     endif
#endif

#if defined(CONFIG_MXT_SELFCAP_TUNE)
static DEVICE_ATTR(self_tune, S_IWUSR|S_IWGRP|S_IWOTH, NULL, mxt_self_tune_store);
#endif

static struct attribute *mxt_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_object.attr,
	&dev_attr_update_fw0.attr,
	&dev_attr_update_cfg0.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_debug_v2_enable.attr,
	&dev_attr_debug_notify.attr,
	&dev_attr_bootloader.attr,
	&dev_attr_t19.attr,
	&dev_attr_t25.attr,
	&dev_attr_cmd.attr,
	&dev_attr_ic_detect.attr,//add by ZTE ,20150123
	&dev_attr_easy_wakeup_gesture.attr,//add by ZTE ,20150123
//	&dev_attr_slide_switch_gesture.attr,//add by ZTE ,20150206
	&dev_attr_manual_cali.attr,//add by ZTE ,20150205
	&dev_attr_touch_mode.attr,
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
	&dev_attr_c_zone.attr,
#endif
	&dev_attr_depth.attr,
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	&dev_attr_plugin.attr,
	&dev_attr_plugin_tag.attr,
/*** ZTEMT start,cut down UID detect ***/
//	&dev_attr_plugin_tag_uid.attr,//add by ZTE ,20150414
/*ZTEMT end*/
#	if defined(CONFIG_MXT_CAL_WORKAROUND)
	&dev_attr_cal.attr,
#     endif
#	if defined(CONFIG_MXT_PI_WORKAROUND)
	&dev_attr_en_glove.attr,
	&dev_attr_en_stylus.attr,
	&dev_attr_en_gesture.attr,
	&dev_attr_gesture_list.attr,
	&dev_attr_gesture_trace.attr,
#	endif
#	if defined(CONFIG_MXT_MISC_WORKAROUND)
	&dev_attr_misc.attr,
#     endif
#	if defined(CONFIG_MXT_CLIP_WORKAROUND)
	&dev_attr_clip.attr,
	&dev_attr_clip_tag.attr,
#	endif
#endif
#if defined(CONFIG_MXT_SELFCAP_TUNE)
	&dev_attr_self_tune.attr,
#endif
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};




static void mxt_reset_slots(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    struct input_dev *input_rim_dev = data->input_rim_dev;
#endif
	unsigned int num_mt_slots;
	int id;

#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
	if (!input_dev || !input_rim_dev)
		return;
#else
    if (!input_dev)
		return;
#endif
	num_mt_slots = data->num_touchids + data->num_stylusids;
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
	mxt_plugin_hook_reset_slots(&data->plug);
#endif
	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_dev, id);
		if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX]))
			input_report_abs(input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}
/*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_rim_dev, id);
		if (test_flag_8bit(MXT_T100_TCHAUX_AMPL, &data->tchcfg[MXT_T100_TCHAUX]))
			input_report_abs(input_rim_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(input_rim_dev, MT_TOOL_FINGER, 0);
	}
#endif
/*ZTEMT end 20140210*/

	mxt_input_sync(input_dev);
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    mxt_input_sync(input_rim_dev);//ZTEMT start add for panel without rim
#endif

#if defined(CONFIG_ZTEMT_PRESS_TOUCHSCREEN_CYPRESS)
	id_number = 0;
	if(id_number == 0 && _cyttsp3_force_function.FT_ready_flag == true) {
		if (cyttsp3_set_touch_status(false) < 0) {
			printk(KERN_ERR "%s ztemt cyttsp3 set release failed!!\n", __func__);
	}
}

#endif
}

static void mxt_start(struct mxt_data *data, bool resume)
{
	struct device *dev = &data->client->dev;
	int ret = 0;
	
	if (!data->suspended || data->in_bootloader)
		return;
	
	dev_info(dev, "mxt_start\n");

	if (data->use_regulator) {
		mxt_regulator_enable(data);
	} else {
		/* Discard any messages still in message buffer from before
		 * chip went to sleep */
		if (test_bit(MXT_WK_ENABLE,&data->enable_wakeup)) {
			if (test_and_clear_bit(MXT_EVENT_IRQ_FLAG,&data->busy)) {
				mxt_process_messages_until_invalid(data);
				if (!test_bit(MXT_WK_DETECTED,&data->enable_wakeup)) {
					dev_info(dev, "detect a invalid wakeup signal\n");
					//mxt_acquire_irq(data);
#if !(defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_FB_PM))
					return;
#endif
				}
			}
		}
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		//ret = mxt_plugin_wakeup_disable(&data->plug);
#endif
		//mxt_process_messages_until_invalid(data);

		//mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		/* Recalibrate since chip has been in deep sleep */
		//if (mxt_plugin_cal_t37_check_and_calibrate(&data->plug, false,resume) != 0)
#endif
			//mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
		mxt_set_reset(&data->plug, 0);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		mxt_plugin_start(&data->plug, resume);
#endif
	}

	mxt_reset_slots(data);  //it's strange some platform will resverse some points in last touch
	data->enable_wakeup = 0;
	data->enable_reporting = true;
	data->suspended = false;

	if (ret != -EBUSY) {
		mxt_acquire_irq(data);
	}else{		
		board_disable_irq_wake(data->pdata,data->irq);
	}
}

static void mxt_stop(struct mxt_data *data,bool suspend)
{
	struct device *dev = &data->client->dev;
	struct mxt_platform_data *pdata = data->pdata;
	int ret = 0;
	
	if (data->suspended || data->in_bootloader)
		return;

	dev_info(dev, "mxt_stop\n");

	if(atomic_read(&data->depth) > 0) {
		board_disable_irq(data->pdata,data->irq);
		atomic_dec(&data->depth);
	}
	data->enable_reporting = false;

	if (data->use_regulator)
		mxt_regulator_disable(data);
	else{
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		mxt_plugin_stop(&data->plug,suspend);
#endif
	}

	if (suspend) {
#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
		ret = mxt_plugin_wakeup_enable(&data->plug);
#endif
		if (ret == -EBUSY) {
			dev_info(dev, "mxt_stop: set wakeup enable\n");
			//msleep(200);
			mxt_process_messages_until_invalid(data);
			if(atomic_read(&data->depth) <= 0) {
				atomic_inc(&data->depth);
				board_enable_irq(pdata,data->irq);
			}
			board_enable_irq_wake(data->pdata,data->irq);
			set_bit(MXT_WK_ENABLE,&data->enable_wakeup);
			clear_bit(MXT_EVENT_IRQ_FLAG,&data->busy);
		}
	}

	mxt_reset_slots(data);

	data->suspended = true;
}

static int mxt_input_open(struct input_dev *input_dev)
{
	struct mxt_data *data = input_get_drvdata(input_dev);
	struct device *dev = &data->client->dev;

	dev_info(dev, "mxt_input_open\n");

	mxt_start(data,false);

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	//board_pulse_irq_thread();
	//mxt_active_proc_thread(data, MXT_EVENT_EXTERN);
#endif
	return 0;
}

static void mxt_input_close(struct input_dev *input_dev)
{
	struct mxt_data *data = input_get_drvdata(input_dev);
	struct device *dev = &data->client->dev;

	dev_info(dev, "mxt_input_close\n");

	mxt_stop(data,false);
}

#ifdef CONFIG_OF
#if 0
static struct mxt_platform_data *mxt_parse_dt(struct i2c_client *client)
{
	struct mxt_platform_data *pdata;
	struct device *dev = &client->dev;
	struct property *prop;
	unsigned int *keymap;
	int proplen, ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;
//    dev_info(dev, "%s: lee atmel driver: mxt_parse_dt \n",__func__);


/*** ZTEMT start ***/
#if 0
  	/* reset gpio */
	pdata->gpio_reset = of_get_named_gpio_flags(dev->of_node,
		"atmel,reset-gpio", 0, NULL);  
#else
	mxt_rst_number = of_get_named_gpio_flags(dev->of_node,"atmel,reset-gpio", 0, NULL);
    pdata->gpio_reset = mxt_rst_number;
	printk("--------pdata->reset_gpio------------: %ud\n",mxt_rst_number);
	if (mxt_rst_number < 0)
		printk("Fail to find  mxt_rst_number\n");//return mxt_rst_number;
    
    mxt_int_number = of_get_named_gpio_flags(dev->of_node,"atmel,irq-gpio", 0, NULL);
    pdata->gpio_irq = mxt_int_number;
    printk("--------pdata->irq_gpio------------: %ud\n",mxt_int_number);
	if (mxt_int_number < 0)
		printk("Fail to find  mxt_int_number\n");//return mxt_int_number;
#endif
/*ZTEMT end*/

	of_property_read_string(dev->of_node, "atmel,cfg_name",
				&pdata->cfg_name);
/*  
//4958:11: error: 'struct mxt_platform_data' has no member named 'input_name'
	of_property_read_string(dev->of_node, "atmel,input_name",
				&pdata->input_name);
*/

	prop = of_find_property(dev->of_node, "linux,gpio-keymap", &proplen);
	if (prop) {
//        dev_info(dev, "%s: lee atmel driver: get 'linux,gpio-keymap' \n",__func__);
		pdata->t19_num_keys = proplen / sizeof(u32);

		keymap = devm_kzalloc(dev,
			pdata->t19_num_keys * sizeof(u32), GFP_KERNEL);
		if (!keymap)
			return NULL;

		pdata->t19_keymap = keymap;

		ret = of_property_read_u32_array(client->dev.of_node,
			"linux,gpio-keymap", keymap, pdata->t19_num_keys);
		if (ret) {
			dev_err(dev,
				"Unable to read device tree key codes: %d\n",
				 ret);
			return NULL;
			}
	}

	return pdata;
}
#endif

static struct mxt_platform_data *mxt_parse_dt(struct i2c_client *client)
{
	struct mxt_platform_data *pdata;
	struct device *dev = &client->dev;
	//struct device_node *node = dev->of_node;
	struct property *prop;	
	u8 *num_keys;
	unsigned int (*keymap)[MAX_KEYS_SUPPORTED_IN_DRIVER];
	//int proplen, ret;
	//char temp[255];
	int i,j;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

#if 0
	/* reset gpio */
	gpio = of_get_named_gpio(node, "reset-gpio", 0);
	if (gpio_is_valid(gpio)) {
		if (!gpio_request(gpio, client->name)) {
			gpio_direction_output(gpio, 1);
			pdata->gpio_reset = gpio;
		}
	}
   #else
	mxt_rst_number = of_get_named_gpio_flags(dev->of_node,"atmel,reset-gpio", 0, NULL);
    pdata->gpio_reset = mxt_rst_number;
	printk("--------pdata->reset_gpio------------: %ud\n",mxt_rst_number);
	if (mxt_rst_number < 0)
		printk("Fail to find  mxt_rst_number\n");//return mxt_rst_number;
    
    mxt_int_number = of_get_named_gpio_flags(dev->of_node,"atmel,irq-gpio", 0, NULL);
    pdata->gpio_irq = mxt_int_number;
    printk("--------pdata->irq_gpio------------: %ud\n",mxt_int_number);
	if (mxt_int_number < 0)
		printk("Fail to find  mxt_int_number\n");//return mxt_int_number;

   #endif
	/*
	of_property_read_string(dev->of_node, "fw_name",
				&pdata->fw_name);
	*/
#if defined(CONFIG_DUMMY_PARSE_DTS)
	prop = (struct property *)1;
#else
	prop = of_find_property(dev->of_node, "gpio-keymap-num", &proplen);
#endif
	if (prop) {
		num_keys = devm_kzalloc(dev,
				sizeof(*num_keys) * NUM_KEY_TYPE, GFP_KERNEL);
			if (!num_keys)
				return NULL;
		keymap = devm_kzalloc(dev,
			sizeof(*keymap) * NUM_KEY_TYPE, GFP_KERNEL);
		if (!keymap) {
			devm_kfree(dev,num_keys);
			return NULL;
		}
#if defined(CONFIG_DUMMY_PARSE_DTS)
		num_keys[T15_T97_KEY] = 3;
                num_keys[T81_KEY] = 1;
                num_keys[T93_KEY] = 1;

		keymap[T15_T97_KEY][0] = KEY_MENU;//KEY_HOMEPAGE;//KEY_MENU;
		keymap[T15_T97_KEY][1] = KEY_HOMEPAGE;//KEY_BACK/*KEY_MENU*/;//KEY_BACK;
		keymap[T15_T97_KEY][2] = KEY_BACK;//KEY_MENU/*KEY_HOMEPAGE*/;

		keymap[T81_KEY][0] = KEY_WAKEUP;
		keymap[T93_KEY][0] = KEY_WAKEUP;
#else
		ret = of_property_read_u8_array(client->dev.of_node,
			"gpio-keymap-num", num_keys, NUM_KEY_TYPE);
		if (ret) {
			dev_err(dev,
				"Unable to read device tree key codes num keys: %d\n",
				 ret);
			devm_kfree(dev,num_keys);
			devm_kfree(dev,keymap);
			return NULL;
		}

		for (i = 0; i < NUM_KEY_TYPE; i++) {
			sprintf(temp,"gpio-keymap_%d",i);
			ret = of_property_read_u32_array(client->dev.of_node,
				temp, keymap[i], num_keys[i]);
			if (ret) {
				dev_err(dev,
					"Unable to read device tree key codes: %d\n",
					 ret);
				//return NULL;
				num_keys[i] = 0;
			}else {
				if (num_keys[i] > MAX_KEYS_SUPPORTED_IN_DRIVER)
					num_keys[i] = MAX_KEYS_SUPPORTED_IN_DRIVER;
			}
		}
#endif
		pdata->num_keys = num_keys;
		pdata->keymap = (void *)keymap;
	}else {
		dev_info(&client->dev, "couldn't find gpio-keymap-num\n");
	}

	
	dev_info(&client->dev, "mxt reset %ld, irq flags 0x%lx\n",
		pdata->gpio_reset,pdata->irqflags);
	if (pdata->num_keys) {
		for (i = 0; i < NUM_KEY_TYPE; i++) {
			dev_dbg(&client->dev, "mxt type %d (%d): ",
				i, pdata->num_keys[i]);
			for (j = 0; j < pdata->num_keys[i]; j++) 
				dev_dbg(&client->dev, "%d ",
					pdata->keymap[i][j]);
			dev_dbg(&client->dev, "\n");
		}
	}

	pdata->irqflags = IRQF_TRIGGER_LOW;
	return pdata;
}


static void mxt_free_dt(struct mxt_data *data)
{
	struct mxt_platform_data *pdata = data->pdata;
	struct device *dev = &data->client->dev;

	if (!pdata)
		return;

	if (pdata->gpio_reset) {
		gpio_free(pdata->gpio_reset);
		pdata->gpio_reset = 0;
	}
	if (pdata->num_keys) {
		devm_kfree(dev, (void *)pdata->num_keys);
		pdata->num_keys = NULL;
	}

	if (pdata->keymap) {
		devm_kfree(dev, (void *)pdata->keymap);
		pdata->keymap = NULL;
	}
}
#endif

static int mxt_handle_pdata(struct mxt_data *data)
{
	data->pdata = dev_get_platdata(&data->client->dev);
//    dev_info(&data->client->dev, "%s: lee atmel driver: mxt_handle_pdata \n",__func__);

	/* Use provided platform data if present */
	if (data->pdata) {
		if (data->pdata->cfg_name)
			mxt_update_file_name(&data->client->dev,
						 &data->cfg_name,
						 data->pdata->cfg_name,
						 strlen(data->pdata->cfg_name),
						 0);

		return 0;
	}

	data->pdata = devm_kzalloc(&data->client->dev,
		sizeof(*data->pdata), GFP_KERNEL);
	if (!data->pdata) {
		dev_err(&data->client->dev, "Failed to allocate pdata\n");
		return -ENOMEM;
	}

	/* Set default parameters */
	data->pdata->irqflags = IRQF_TRIGGER_LOW;

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	int i,j;

	error = mxt_read_t9_resolution(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = "Atmel_maXTouch_Touchscreen";
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;  //vid: 03eb pid: 8c25
	input_dev->dev.parent = dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
//	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
#if defined(INPUT_PROP_DIRECT)
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif
	if (pdata->keymap && pdata->num_keys) {
		if (pdata->num_keys[T19_KEY]) {
			__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);
			i = T19_KEY;
			for (j = 0; j < pdata->num_keys[i]; j++)
				if (pdata->keymap[i][j] != KEY_RESERVED)
					input_set_capability(input_dev, EV_KEY,
								 pdata->keymap[i][j]);

		__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
		__set_bit(BTN_TOOL_DOUBLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_QUADTAP, input_dev->keybit);

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);

			input_dev->name = "Atmel maXTouch Touchpad";
		}
	}

	/* For single touch */
    /*
	input_set_abs_params(input_dev, ABS_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_dev, ABS_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_Y,
				 0, data->max_y, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_PRESSURE,
				 0, 255, 0, 0);
*/
	/* For multi touch */
	num_mt_slots = data->num_touchids + data->num_stylusids;
	error = input_mt_init_slots(input_dev, num_mt_slots, 0/*, INPUT_MT_DIRECT*/);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				 0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				 0, data->max_x, 0, 0);
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				 0, data->max_y_t, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				 0, data->max_y, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
				 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
				 0, 255, 0, 0);

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS);
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS2);
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	data->t15_keystatus = 0;
	/* For Key register */
#if !defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	if (data->pdata->keymap && pdata->num_keys) {
		for (i = 0; i < NUM_KEY_TYPE; i++) {
			for (j = 0; j < data->pdata->num_keys[i]; j++) {
				set_bit(data->pdata->keymap[i][j], input_dev->keybit);
				input_set_capability(input_dev, EV_KEY,
							 data->pdata->keymap[i][j]);
			}
		}
	}
#endif
	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int mxt_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
    int i;
#endif
	dev_info(&client->dev, "%s: driver version 0x%x\n", 
			__func__,DRIVER_VERSION);

/*** ZTEMT start ***/
    atmel_pinctrl_init(&client->dev);
    error = pinctrl_select_state(atmel_pctrl.pinctrl,
        atmel_pctrl.gpio_state_active);
    if (error)
        dev_err(&client->dev,"%s:%d cannot set pin to gpio_state_active state",
            __func__, __LINE__);
/*ZTEMT end*/

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	mutex_init(&data->bus_access_mutex);
#if defined(CONFIG_MXT_I2C_DMA)
	client->addr |= I2C_RS_FLAG | I2C_ENEXT_FLAG | I2C_DMA_FLAG;
	data->i2c_dma_va = (u8 *)dma_alloc_coherent(NULL, PAGE_SIZE * 2, &data->i2c_dma_pa, GFP_KERNEL);
	if (!data->i2c_dma_va)	{
		error = -ENOMEM;
		dev_err(&client->dev, "Allocate DMA I2C Buffer failed!\n");
		goto err_free_mem;
	}
#endif

#ifdef CONFIG_OF
	if (!data->pdata && client->dev.of_node)
		data->pdata = mxt_parse_dt(client);
#endif
	if (!data->pdata) {
		error = mxt_handle_pdata(data);
		if (error)
			goto err_free_mem;
	}
	
	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);
	mutex_init(&data->debug_msg_lock);
	mutex_init(&data->access_mutex);
	atomic_set(&data->depth,1);

#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
 //   dev_err(&client->dev, "zte lee atmel MXT_IRQ_WORKQUEUE)1 \n");

    init_waitqueue_head(&data->wait);
	data->irq_tsk = kthread_run(mxt_process_message_thread, data,
						"Atmel_mxt_ts");
	if (!data->irq_tsk) {
		dev_err(&client->dev, "Error %d Can't create irq thread\n",
			error);
		error = -ESRCH;
		goto err_free_pdata;
	}


#if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
        dev_dbg(&client->dev, "atmel data->pdata->irqflags= %ld \n",(data->pdata->irqflags));
        data->pdata->irqflags = IRQF_TRIGGER_LOW;
    
        error = request_irq(client->irq, mxt_interrupt_pulse_workqueue,
                data->pdata->irqflags /*IRQF_TRIGGER_LOW*/,
                client->name, data);
        if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
            goto err_free_irq_workqueue;
        }
        
#endif

#else
    dev_err(&client->dev, "2 zte lee atmel data->pdata->irqflags= %ld \n",(data->pdata->irqflags));
	data->pdata->irqflags = IRQF_TRIGGER_LOW;

	error = request_threaded_irq(data->irq, NULL, mxt_interrupt,
					 data->pdata->irqflags | IRQF_ONESHOT,
					 client->name, data);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_pdata;
	}
#endif

	mxt_probe_regulators(data);
#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE) || defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	mxt_g_data = data;
#endif
	error = mxt_initialize(data);
	if (error)
		goto err_free_irq;

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		goto err_free_object;
	}

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRWXUGO /*S_IRUGO | S_IWUSR*/;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

#if defined(CONFIG_FB_PM)
//    dev_info(&client->dev, "%s: lee atmel driver: defined(CONFIG_FB_PM) fb_notifier_callback \n",__func__);

	data->fb_notif.notifier_call = fb_notifier_callback;
	error = fb_register_client(&data->fb_notif);
	if (error) {
		dev_err(&client->dev,
			"Unable to register fb_notifier: %d\n",
			error);
		goto err_remove_mem_access_attr;
	}
#if defined(NUBIA_DEFERRED_RESUME_TP)
	INIT_WORK(&data->nubia_tp_resume_work, nubia_tp_resume_work_func);
#endif


#elif defined(CONFIG_HAS_EARLYSUSPEND)
//    dev_info(&client->dev, "%s: lee atmel driver: defined(CONFIG_HAS_EARLYSUSPEND) mxt_early_suspend \n",__func__);

	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = mxt_early_suspend;
	data->early_suspend.resume = mxt_late_resume;
	register_early_suspend(&data->early_suspend);

#endif
    /*ZTEMT start add for panel without rim*/
#if defined(CONFIG_ZTEMT_TOUCHSCREEN_ATMEL_MXTS_C_ZONE)
        for(i=0;i<10;i++){
            slot_ignore[i].slot_zoneid = ZONE_DEFAULT; 
            //slot_ignore[i].slot_X0 = 0;
            //slot_ignore[i].slot_Y0 = 0;
        }
        data->c_zone_flag = 1;
#endif
    /*ZTEMT end 20140210*/
        wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "atmel_mxt_wakelock");//add by ztemt 20150403

	dev_info(&client->dev, "Mxt probe finished\n");
	return 0;

#if defined(CONFIG_FB_PM)
err_remove_mem_access_attr:
	sysfs_remove_bin_file(&client->dev.kobj,
				  &data->mem_access_attr);
#endif
err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_object:
	mxt_free_object_table(data);
err_free_irq:
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	if (data->properties_kobj) {
		kobject_put(data->properties_kobj);
		data->properties_kobj = NULL;
	}
#endif
	board_free_irq(data->pdata,data->irq, data);
#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
#	if !defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
err_free_irq_workqueue:	
#   endif
	kthread_stop(data->irq_tsk);
#endif
err_free_pdata:
	mutex_destroy(&data->access_mutex);
	mutex_destroy(&data->debug_msg_lock);
#if defined(CONFIG_OF)
	mxt_free_dt(data);
#endif
	if (!dev_get_platdata(&data->client->dev))
		devm_kfree(&data->client->dev,data->pdata);
err_free_mem:
#if defined(CONFIG_MXT_I2C_DMA)	
	dma_free_coherent(NULL, PAGE_SIZE * 2, data->i2c_dma_va, data->i2c_dma_pa);
#endif
	kfree(data);
	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

#if defined(CONFIG_FB_PM)
	fb_unregister_client(&data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
#if defined(CONFIG_MXT_IRQ_WORKQUEUE)
	kthread_stop(data->irq_tsk);
#endif

	if (data->mem_access_attr.attr.name)
		sysfs_remove_bin_file(&client->dev.kobj,
					  &data->mem_access_attr);

	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
#if defined(CONFIG_MXT_I2C_DMA)
	dma_free_coherent(NULL, PAGE_SIZE * 2, data->i2c_dma_va, data->i2c_dma_pa);
#endif	
#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
	if (data->properties_kobj) {
		kobject_del(data->properties_kobj);
		data->properties_kobj = NULL;
	}
#endif
	board_free_irq(data->pdata,data->irq, data);
#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
	mxt_g_data = NULL;
#endif
	board_gpio_deinit(data->pdata);
	regulator_put(data->reg_avdd);
	regulator_put(data->reg_vdd);
	mxt_free_object_table(data);
	mutex_destroy(&data->access_mutex);
	mutex_destroy(&data->debug_msg_lock);
#if defined(CONFIG_OF)
	mxt_free_dt(data);
#endif
	if (!dev_get_platdata(&data->client->dev))
		devm_kfree(&data->client->dev,data->pdata);
	kfree(data);

	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	dev_info(dev, "mxt_suspend\n");

	if (!input_dev)  //maybe bootup in bootloader mode
		return 0;
	
	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_stop(data,true);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	dev_info(dev, "mxt_resume\n");

	if (!input_dev)  //maybe bootup in bootloader mode
		return 0;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_start(data,true);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

#if defined(CONFIG_FB_PM)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mxt_data *mxt =
		container_of(self, struct mxt_data, fb_notif);
//    dev_info(&mxt->client->dev, "%s: lee atmel driver: fb_notifier_callback \n",__func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && mxt && mxt->client) {
			blank = evdata->data;
			if (*blank == FB_BLANK_UNBLANK) {
#if defined (NUBIA_DEFERRED_RESUME_TP)
				dev_info(&mxt->client->dev, "%s: ztemt atmel FB_BLANK_UNBLANK, deferred\n", __func__);
				if (!work_pending(&mxt->nubia_tp_resume_work))
					schedule_work(&mxt->nubia_tp_resume_work);
#else
				dev_err(&mxt->client->dev, "%s: ztemt atmel FB_BLANK_UNBLANK\n", __func__);
#if defined(CONFIG_ZTEMT_PRESS_TOUCHSCREEN_CYPRESS)
				id_number = 0;
#endif
				if (mxt_resume(&mxt->client->dev) != 0)
					dev_err(&mxt->client->dev, "%s: failed\n", __func__);
#endif
			}else if (*blank == FB_BLANK_POWERDOWN) {
				dev_err(&mxt->client->dev, "%s: ztemt atmel FB_BLANK_POWERDOWN\n", __func__);
#if defined (NUBIA_DEFERRED_RESUME_TP)
				cancel_work_sync(&mxt->nubia_tp_resume_work);
#endif
				if (mxt_suspend(&mxt->client->dev) != 0)
					dev_err(&mxt->client->dev, "%s: failed\n", __func__);
			}
    }

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void mxt_early_suspend(struct early_suspend *es)
{
	struct mxt_data *mxt;
	mxt = container_of(es, struct mxt_data, early_suspend);

	if (mxt_suspend(&mxt->client->dev) != 0)
		dev_err(&mxt->client->dev, "%s: failed\n", __func__);
}

static void mxt_late_resume(struct early_suspend *es)
{
	struct mxt_data *mxt;
	mxt = container_of(es, struct mxt_data, early_suspend);

	if (mxt_resume(&mxt->client->dev) != 0)
		dev_err(&mxt->client->dev, "%s: failed\n", __func__);
}
#else
static SIMPLE_DEV_PM_OPS(mxt_pm_ops, mxt_suspend, mxt_resume);
#endif
#endif

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	if(atomic_read(&data->depth) >= 0) {
		board_disable_irq(data->pdata,data->irq);
		atomic_dec(&data->depth);
	}
}

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "atmel_mxt_tp", 0 },
	{ "mXT224", 0 },
	{ "Atmel MXT336T", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

#ifdef CONFIG_USE_OF
static struct of_device_id mxt_match_table[] = {
	{ .compatible = "atmel,mxt-ts",},
	{ },
};
#else
#define mxt_match_table NULL
#endif

#if defined(CONFIG_MXT_PLUGIN_SUPPORT)
#include "plug.if"
#endif

#if defined(CONFIG_MXT_EXTERNAL_MODULE)
#include "atmel_mxt_ts_mtk_interface.if"
#else
static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= "atmel_mxt_ts",
		.owner	= THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = mxt_match_table,
#endif
#if !(defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_FB_PM))
		.pm	= &mxt_pm_ops,
#endif
	},
	.probe		= mxt_probe,
	.remove		= mxt_remove,
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

static int __init mxt_init(void)
{
	return i2c_add_driver(&mxt_driver);
}

static void __exit mxt_exit(void)
{
	i2c_del_driver(&mxt_driver);
}

module_init(mxt_init);
module_exit(mxt_exit);
#endif

#if defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
#include "virtualkey.if"
#endif

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");

