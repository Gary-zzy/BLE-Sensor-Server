/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/sensor.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/sensor_types.h>
#include <dk_buttons_and_leds.h>
#include <float.h>
#include "model_handler.h"
#include <zephyr/random/random.h>



#if DT_NODE_HAS_STATUS(DT_NODELABEL(bme680), okay)
/** Thingy53 */
#define SENSOR_NODE DT_NODELABEL(bme680)
#define SENSOR_DATA_TYPE SENSOR_CHAN_AMBIENT_TEMP
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(temp), okay)
/** nRF52 DK */
#define SENSOR_NODE DT_NODELABEL(temp)
#define SENSOR_DATA_TYPE SENSOR_CHAN_DIE_TEMP
#else
#error "Unsupported board!"
#endif

static const struct device *icm20948_dev = NULL; // 没有实际设备时直接置为 NULL

static const uint8_t cmp2_elem_offset_people_count[1] = { 3 };

static const struct bt_mesh_comp2_record comp_rec[4] = {
	{
	.id = BT_MESH_NLC_PROFILE_ID_OCCUPANCY_SENSOR,
	.version.x = 1,
	.version.y = 0,
	.version.z = 0,
	.elem_offset_cnt = 1,
	.elem_offset = cmp2_elem_offset_people_count,
	.data_len = 0
	}
};

static const struct bt_mesh_comp2 comp_p2 = {
	.record_cnt = 4,
	.record = comp_rec
};

/* Pick a custom Property ID in the vendor range. 
 * For demo here, we just use 0x4A01 (0x4A00–0x4AFF is a typical vendor range example).
 */
/* Pick a custom Property ID in the vendor range, for example: */
#define BT_MESH_PROP_ID_CUSTOM_ICM20948 0x4A01 // ???
#define BT_MESH_PROP_ID_MAGNETIC_FIELD_X  0x4A01 // ???
#define BT_MESH_PROP_ID_MAGNETIC_FIELD_Y  0x4A02 // ???
#define BT_MESH_PROP_ID_MAGNETIC_FIELD_Z  0x4A03 // ???

struct sensor_value_range {
	struct bt_mesh_sensor_value start;
	struct bt_mesh_sensor_value end;
};

static uint16_t dummy_people_count_value;

// 定义将单位转换为微单位，即将数据乘以1000000。
#define BASE_UNITS_TO_MICRO(value) ((value) * 1000000LL)

// 获取一个虚拟的人数识别传感器的数据，即随机生成一个32位数据
static int people_count_get(struct bt_mesh_sensor_srv *srv,
                            struct bt_mesh_sensor *sensor,
                            struct bt_mesh_msg_ctx *ctx,
                            struct bt_mesh_sensor_value *rsp)
{
    // 使用随机数模拟人数
    dummy_people_count_value = sys_rand32_get() % 100; // 模拟0到99之间的随机人数

    printk("Simulated People Count Value: %u\n", dummy_people_count_value);

    int err = bt_mesh_sensor_value_from_micro(sensor->type->channels[0].format,
                                              BASE_UNITS_TO_MICRO(dummy_people_count_value), rsp);

    if (err && err != -ERANGE) {
        printk("Error encoding people count (%d)\n", err);
        return err;
    }
    return 0;
}


// 人数检测传感器实例定义
static struct bt_mesh_sensor people_count_sensor = {
	.type = &bt_mesh_sensor_people_count,
	.get = people_count_get,
};


// 定义多个传感器数组，分别对应不同的功能。
static struct bt_mesh_sensor *const people_count_sensor_data[] = {
	&people_count_sensor,
};

// 传感器服务初始化
static struct bt_mesh_sensor_srv people_count_sensor_srv =
	BT_MESH_SENSOR_SRV_INIT(people_count_sensor_data, ARRAY_SIZE(people_count_sensor_data));

// 初始化一个bool类型的传感器值
#define BOOLEAN_INIT(_bool) { .format = &bt_mesh_sensor_format_boolean, .raw = { (_bool) } }


// 初始化按钮功能
#define BUTTON_PRESSED(p, c, b) ((p & b) && (c & b))
#define BUTTON_RELEASED(p, c, b) (!(p & b) && (c & b))

// 定义输出元素
static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_SENSOR_SRV(&people_count_sensor_srv)),
		     BT_MESH_MODEL_NONE),
};

// 模型初始化函数
static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};
// 初始化和配置设备模型及其他硬件资源
const struct bt_mesh_comp *model_handler_init(void)
{
	if (bt_mesh_comp2_register(&comp_p2)) {
		printf("Failed to register comp2\n");
	}
	
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		settings_subsys_init();
	}
	return &comp;
}
