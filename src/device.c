/*
 * Copyright (c) 2023 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <caniot/caniot.h>
#include <caniot/caniot_private.h>
#include <caniot/datatype.h>
#include <caniot/device.h>

typedef uint16_t attr_key_t;

enum section_option {
	READONLY   = 1 << 0,
	VOLATILE   = 1 << 1,
	PERSISTENT = 1 << 2
};

#define ATTR_OPTION_READABLE_POS  0u
#define ATTR_OPTION_WRITABLE_POS  1u
#define ATTR_OPTION_CLASS_POS	  2u
#define ATTR_OPTION_CLASS_MSK	  0x7u
#define ATTR_OPTION_CLASS_ALL_POS 5u

enum attr_option {
	HIDDEN		   = 0u,
	READABLE	   = 1 << ATTR_OPTION_READABLE_POS,
	WRITABLE	   = 1 << ATTR_OPTION_WRITABLE_POS,
	ATTR_CLASS0	   = 0u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS1	   = 1u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS2	   = 2u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS3	   = 3u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS4	   = 4u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS5	   = 5u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS6	   = 6u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS7	   = 7u << ATTR_OPTION_CLASS_POS,
	ATTR_CLASS_ALL = 1 << ATTR_OPTION_CLASS_ALL_POS,
	// OPT_POS6 = 1 << 6u,
	// OPT_POS7 = 1 << 7u,
};

struct attr_ref {
	enum attr_option option;
	enum section_option section_option;
	uint8_t section;
	uint8_t offset;
	uint8_t size;
};

struct attribute {
	uint8_t offset;
	uint8_t size; /* TODO merge fields size and option together */
	uint8_t option;
#if CONFIG_CANIOT_ATTRIBUTE_NAME
	char name[CANIOT_ATTR_NAME_MAX_LEN];
#endif
};

struct attr_section {
	enum section_option option;
	const struct attribute *array;
	uint8_t array_size;
#if CONFIG_CANIOT_ATTRIBUTE_NAME
	char name[16u];
#endif
};

#define ATTR_IDENTIFICATION 0
#define ATTR_SYSTEM			1
#define ATTR_CONFIG			2

#define ATTR_KEY_SECTION_OFFSET 12u
#define ATTR_KEY_SECTION_SIZE	4u
#define ATTR_KEY_SECTION_MASK	((1u << ATTR_KEY_SECTION_SIZE) - 1u)

#define ATTR_KEY_ATTR_OFFSET 4u
#define ATTR_KEY_ATTR_SIZE	 8u
#define ATTR_KEY_ATTR_MASK	 ((1u << ATTR_KEY_ATTR_SIZE) - 1u)

#define ATTR_KEY_PART_OFFSET 0u
#define ATTR_KEY_PART_SIZE	 4u
#define ATTR_KEY_PART_MASK	 ((1u << ATTR_KEY_PART_SIZE) - 1u)

#define ATTR_KEY_SECTION_GET(key)                                                        \
	(((key) >> ATTR_KEY_SECTION_OFFSET) & ATTR_KEY_SECTION_MASK)
#define ATTR_KEY_ATTR_GET(key) (((key) >> ATTR_KEY_ATTR_OFFSET) & ATTR_KEY_ATTR_MASK)
#define ATTR_KEY_PART_GET(key) (((key) >> ATTR_KEY_PART_OFFSET) & ATTR_KEY_PART_MASK)

#define ATTR_KEY_SECTION_SET(key, section)                                               \
	((key) = ((key) & ~(ATTR_KEY_SECTION_MASK << ATTR_KEY_SECTION_OFFSET)) |             \
			 ((section & ATTR_KEY_SECTION_MASK) << ATTR_KEY_SECTION_OFFSET))
#define ATTR_KEY_ATTR_SET(key, attr)                                                     \
	((key) = ((key) & ~(ATTR_KEY_ATTR_MASK << ATTR_KEY_ATTR_OFFSET)) |                   \
			 ((attr & ATTR_KEY_ATTR_MASK) << ATTR_KEY_ATTR_OFFSET))
#define ATTR_KEY_PART_SET(key, part)                                                     \
	((key) = ((key) & ~(ATTR_KEY_PART_MASK << ATTR_KEY_PART_OFFSET)) |                   \
			 ((part & ATTR_KEY_PART_MASK) << ATTR_KEY_PART_OFFSET))

/* A part is is 4B */
#define ATTR_KEY_DATA_BYTE_OFFSET(key) (ATTR_KEY_PART_GET(key) << 2)

#define ATTR_KEY(section, attr, part) CANIOT_ATTR_KEY(section, attr, part)

static void attr_option_adjust(enum attr_option *attr_opt, enum section_option sec_opt)
{
	if (sec_opt & READONLY) {
		*attr_opt &= ~WRITABLE;
	}
}

#define MEMBER_SIZEOF(s, member) (sizeof(((s *)0)->member))

#if CONFIG_CANIOT_ATTRIBUTE_NAME

#define ATTRIBUTE_HELPER(s, opt, _name, param)                                           \
	{                                                                                    \
		.offset = (uint8_t)offsetof(s, param), .size = (uint8_t)MEMBER_SIZEOF(s, param), \
		.option = (uint8_t)(opt), .name = _name,                                         \
	}

#define SECTION(options, _name, array)                                                   \
	{                                                                                    \
		options, array, ARRAY_SIZE(array), _name                                         \
	}

#else

#define ATTRIBUTE_HELPER(s, opt, _name, param)                                           \
	{                                                                                    \
		.offset = (uint8_t)offsetof(s, param), .size = (uint8_t)MEMBER_SIZEOF(s, param), \
		.option = (uint8_t)(opt),                                                        \
	}

#define SECTION(options, _name, array)                                                   \
	{                                                                                    \
		options, array, ARRAY_SIZE(array)                                                \
	}

#endif

/* Global attribute for all classes */
#define ATTRIBUTE(s, rw, _name, param)                                                   \
	ATTRIBUTE_HELPER(s, (rw) | ATTR_CLASS_ALL, _name, param)

/* Attribute can only be accessed for the specific class (e.g. config)*/
#define CLASS_ATTR(s, rw, cls, _name, param)                                             \
	ATTRIBUTE_HELPER(s, (rw) | (cls), _name, param)

static const struct attribute identification_attr[] ROM = {
	[0x0] = ATTRIBUTE(struct caniot_device_id, READABLE, "nodeid", did),
	[0x1] = ATTRIBUTE(struct caniot_device_id, READABLE, "version", version),
	[0x2] = ATTRIBUTE(struct caniot_device_id, READABLE, "name", name),
	[0x3] = ATTRIBUTE(struct caniot_device_id, READABLE, "magic_number", magic_number),
#if CONFIG_CANIOT_BUILD_INFOS
	[0x4] = ATTRIBUTE(struct caniot_device_id, READABLE, "build_date", build_date),
	[0x5] = ATTRIBUTE(struct caniot_device_id, READABLE, "build_commit", build_commit),
#endif
	[0x6] = ATTRIBUTE(struct caniot_device_id, READABLE, "features", features),
};

static const struct attribute system_attr[] ROM = {
	[0x0] =
		ATTRIBUTE(struct caniot_device_system, READABLE, "uptime_synced", uptime_synced),
	[0x1] = ATTRIBUTE(struct caniot_device_system, READABLE | WRITABLE, "time", time),
	[0x2] = ATTRIBUTE(struct caniot_device_system, READABLE, "uptime", uptime),
	[0x3] = ATTRIBUTE(struct caniot_device_system, READABLE, "start_time", start_time),
	[0x4] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "last_telemetry", last_telemetry),
	[0xB] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "_last_telemetry_ms", _last_telemetry_ms),
	[0x5] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "received.total", received.total),
	[0x6] = ATTRIBUTE(struct caniot_device_system,
					  READABLE,
					  "received.read_attribute",
					  received.read_attribute),
	[0x7] = ATTRIBUTE(struct caniot_device_system,
					  READABLE,
					  "received.write_attribute",
					  received.write_attribute),
	[0x8] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "received.command", received.command),
	[0x9] = ATTRIBUTE(struct caniot_device_system,
					  READABLE,
					  "received.request_telemetry",
					  received.request_telemetry),
	[0xA] = ATTRIBUTE(
		struct caniot_device_system, HIDDEN, "received.ignored", received.ignored),
	[0xC] = ATTRIBUTE(struct caniot_device_system, READABLE, "sent.total", sent.total),
	[0xD] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "sent.telemetry", sent.telemetry),
	[0xE] = ATTRIBUTE(struct caniot_device_system, HIDDEN, "", _unused4),
	[0xF] = ATTRIBUTE(
		struct caniot_device_system, READABLE, "last_command_error", last_command_error),
	[0x10] = ATTRIBUTE(struct caniot_device_system,
					   READABLE,
					   "last_telemetry_error",
					   last_telemetry_error),
	[0x11] = ATTRIBUTE(struct caniot_device_system, HIDDEN, "", _unused5),
	[0x12] = ATTRIBUTE(struct caniot_device_system, READABLE, "battery", battery),
};

static const struct attribute config_attr[] ROM = {
	[0x0] = ATTRIBUTE(struct caniot_device_config,
					  READABLE | WRITABLE,
					  "telemetry.period",
					  telemetry.period), /* ms */
	[0x1] = ATTRIBUTE(struct caniot_device_config,
					  READABLE | WRITABLE,
					  "telemetry.delay",
					  telemetry.delay), /* ms */
	[0x2] = ATTRIBUTE(struct caniot_device_config,
					  READABLE | WRITABLE,
					  "telemetry.delay_min",
					  telemetry.delay_min), /* ms */
	[0x3] = ATTRIBUTE(struct caniot_device_config,
					  READABLE | WRITABLE,
					  "telemetry.delay_max",
					  telemetry.delay_max), /* ms */
	[0x4] = ATTRIBUTE(struct caniot_device_config, READABLE | WRITABLE, "flags", flags),
	[0x5] =
		ATTRIBUTE(struct caniot_device_config, READABLE | WRITABLE, "timezone", timezone),
	[0x6] =
		ATTRIBUTE(struct caniot_device_config, READABLE | WRITABLE, "location", location),

	/* Class 0 */
	[0x7] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.pulse_duration.oc1",
					   cls0_gpio.pulse_durations[0u]),
	[0x8] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.pulse_duration.oc2",
					   cls0_gpio.pulse_durations[1u]),
	[0x9] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.pulse_duration.rl1",
					   cls0_gpio.pulse_durations[2u]),
	[0xA] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.pulse_duration.rl2",
					   cls0_gpio.pulse_durations[3u]),
	[0xB] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.outputs_default",
					   cls0_gpio.outputs_default),
	[0xC] = CLASS_ATTR(struct caniot_device_config,
					   READABLE | WRITABLE,
					   ATTR_CLASS0,
					   "cls0_gpio.mask.telemetry_on_change",
					   cls0_gpio.telemetry_on_change),

	/* Class 1 */
	[0xD]  = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pc0",
						cls1_gpio.pulse_durations[0u]),
	[0xE]  = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pc1",
						cls1_gpio.pulse_durations[1u]),
	[0xF]  = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pc2",
						cls1_gpio.pulse_durations[2u]),
	[0x10] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pc3",
						cls1_gpio.pulse_durations[3u]),
	[0x11] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pd0",
						cls1_gpio.pulse_durations[4u]),
	[0x12] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pd1",
						cls1_gpio.pulse_durations[5u]),
	[0x13] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pd2",
						cls1_gpio.pulse_durations[6u]),
	[0x14] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pd3",
						cls1_gpio.pulse_durations[7u]),
	[0x15] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei0",
						cls1_gpio.pulse_durations[8u]),
	[0x16] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei1",
						cls1_gpio.pulse_durations[9u]),
	[0x17] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei2",
						cls1_gpio.pulse_durations[10u]),
	[0x18] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei3",
						cls1_gpio.pulse_durations[11u]),
	[0x19] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei4",
						cls1_gpio.pulse_durations[12u]),
	[0x1A] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei5",
						cls1_gpio.pulse_durations[13u]),
	[0x1B] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei6",
						cls1_gpio.pulse_durations[14u]),
	[0x1C] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pei7",
						cls1_gpio.pulse_durations[15u]),
	[0x1D] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pb0",
						cls1_gpio.pulse_durations[16u]),
	[0x1E] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pe0",
						cls1_gpio.pulse_durations[17u]),
	[0x1F] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration.pe1",
						cls1_gpio.pulse_durations[18u]),
	[0x20] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.pulse_duration._reserved",
						cls1_gpio.pulse_durations[19u]),
	[0x21] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.directions",
						cls1_gpio.directions),
	[0x22] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.outputs_default",
						cls1_gpio.outputs_default),
	[0x23] = CLASS_ATTR(struct caniot_device_config,
						READABLE | WRITABLE,
						ATTR_CLASS1,
						"cls1_gpio.mask.telemetry_on_change",
						cls1_gpio.telemetry_on_change),
};

static const struct attr_section attr_sections[] ROM = {
	[0] = SECTION(READONLY, "identification", identification_attr),
	[1] = SECTION(VOLATILE, "system", system_attr),
	[2] = SECTION(PERSISTENT, "configuration", config_attr),
};

static inline void arch_rom_cpy_byte(uint8_t *d, const uint8_t *p)
{
#ifdef __AVR__
	*d = pgm_read_byte(p);
#else
	*d = *p;
#endif
}

static inline void arch_rom_cpy_word(uint16_t *d, const uint16_t *p)
{
#ifdef __AVR__
	*d = pgm_read_word(p);
#else
	*d = *p;
#endif
}

static inline void arch_rom_cpy_dword(uint32_t *d, const uint32_t *p)
{
#ifdef __AVR__
	*d = pgm_read_dword(p);
#else
	*d = *p;
#endif
}

static inline void arch_rom_cpy_ptr(void **d, const void **p)
{
#ifdef __AVR__
	*d = pgm_read_ptr(p);
#else
	*d = (void *)*p;
#endif
}

static inline void arch_rom_cpy_mem(void *d, const void *p, uint16_t size)
{
#ifdef __AVR__
	memcpy_P(d, p, size);
#else
	memcpy(d, p, size);
#endif
}

static inline void arch_rom_cpy(void *d, const void *p, uint16_t size)
{
	switch (size) {
	case 1u:
		arch_rom_cpy_byte(d, p);
		break;
	case 2u:
		arch_rom_cpy_word(d, p);
		break;
	case 4u:
		arch_rom_cpy_dword(d, p);
		break;
	default:
		arch_rom_cpy_mem(d, p, size);
		break;
	}
}

static inline uint8_t attr_get_section_size(const struct attr_section *section)
{
	uint8_t size;
	arch_rom_cpy_byte(&size, &section->array_size);
	return size;
}

static inline const struct attribute *
attr_get_section_array(const struct attr_section *section)
{
	const struct attribute *array;
	arch_rom_cpy_ptr((void **)&array, (const void **)&section->array);
	return array;
}

static inline enum section_option
attr_get_section_option(const struct attr_section *section)
{
	uint8_t option;
	arch_rom_cpy_byte(&option, (const uint8_t *)&section->option);
	return option;
}

static inline uint8_t attr_get_size(const struct attribute *attr)
{
	uint8_t size;
	arch_rom_cpy_byte(&size, &attr->size);
	return size;
}

static inline uint8_t attr_get_offset(const struct attribute *attr)
{
	uint8_t offset;
	arch_rom_cpy_byte(&offset, &attr->offset);
	return offset;
}

static inline enum attr_option attr_get_option(const struct attribute *attr)
{
	uint8_t option;
	arch_rom_cpy_byte(&option, &attr->option);
	return option;
}

static const struct attr_section *attr_get_section(attr_key_t key)
{
	uint8_t index = ATTR_KEY_SECTION_GET(key);
	if (index < ARRAY_SIZE(attr_sections)) {
		return &attr_sections[index];
	}
	return NULL;
}

static const struct attribute *attr_get(attr_key_t key,
										const struct attr_section *section)
{
	uint8_t index = ATTR_KEY_ATTR_GET(key);
	if (index < attr_get_section_size(section)) {
		return &attr_get_section_array(section)[index];
	}
	return NULL;
}

static int attr_resolve(attr_key_t key, struct attr_ref *ref)
{
	const struct attr_section *section;
	const struct attribute *attr;
	uint8_t attr_size;

	section = attr_get_section(key);
	if (!section) {
		return -CANIOT_EKEYSECTION;
	}

	attr = attr_get(key, section);
	if (!attr) {
		return -CANIOT_EKEYATTR;
	}

	attr_size = attr_get_size(attr);
	if (ATTR_KEY_DATA_BYTE_OFFSET(key) >= attr_size) {
		return -CANIOT_EKEYPART;
	}

	ref->section		= ATTR_KEY_SECTION_GET(key);
	ref->size			= MIN(attr_size, 4u);
	ref->offset			= ATTR_KEY_DATA_BYTE_OFFSET(key) + attr_get_offset(attr);
	ref->option			= attr_get_option(attr);
	ref->section_option = attr_get_section_option(section);

	/* adjust attribute options in function of uppermost section options */
	attr_option_adjust(&ref->option, ref->section_option);

	return 0;
}

static void read_identificate_attr(struct caniot_device *dev,
								   const struct attr_ref *ref,
								   struct caniot_attribute *attr)
{
	arch_rom_cpy(&attr->val, (uint8_t *)dev->identification + ref->offset, ref->size);
}

static void read_rom_identification(struct caniot_device_id *d,
									const struct caniot_device_id *p)
{
	arch_rom_cpy_mem(d, p, sizeof(struct caniot_device_id));
}

void caniot_print_device_identification(const struct caniot_device *dev)
{
	struct caniot_device_id id;

	read_rom_identification(&id, dev->identification);

	CANIOT_INF(F("name    = %s\ncls/dev = %d/%d\nversion = %hhx\n"),
			   id.name,
			   CANIOT_DID_CLS(id.did),
			   CANIOT_DID_SID(id.did),
			   id.version);

#if CONFIG_CANIOT_BUILD_INFOS
	CANIOT_INF(F("commit  = "));
	for (uint8_t i = 0u; i < 20u; i++) {
		CANIOT_INF(F("%02hx"), id.build_commit[i]);
	}
	CANIOT_INF(F("\n"));
#endif
}

int caniot_device_system_reset(struct caniot_device *dev)
{
	if (!dev) return -CANIOT_EINVAL;

	memset(&dev->system, 0, sizeof(struct caniot_device_system));

	return 0;
}

void caniot_device_config_mark_dirty(struct caniot_device *dev)
{
	dev->flags.config_dirty = true;
}

static inline void read_identification_nodeid(struct caniot_device *dev,
											  caniot_did_t *did)
{
	arch_rom_cpy_byte(did, (const uint8_t *)&dev->identification->did);
}

caniot_did_t caniot_device_get_id(struct caniot_device *dev)
{
	caniot_did_t did;
	read_identification_nodeid(dev, &did);
	return did;
}

uint16_t caniot_device_get_filter(caniot_did_t did)
{
	const caniot_id_t filter = {
		.query = CANIOT_QUERY,
		.sid   = CANIOT_DID_SID(did),
		.cls   = CANIOT_DID_CLS(did),
	};

	return caniot_id_to_canid(filter);
}

uint16_t caniot_device_get_filter_broadcast(void)
{
	const caniot_id_t filter = {
		.query = CANIOT_QUERY,
		.sid   = CANIOT_SUBID_BROADCAST,
		.cls   = CANIOT_CLASS_BROADCAST,
	};

	return caniot_id_to_canid(filter);
}

uint16_t caniot_device_get_filter_by_cls(uint8_t cls)
{
	const caniot_id_t filter = {
		.query = CANIOT_QUERY,
		.sid   = 0u,
		.cls   = CANIOT_DID_CLS(cls),
	};

	return caniot_id_to_canid(filter);
}

static int prepare_config_read(struct caniot_device *dev)
{
	int ret = 0;

	ASSERT(dev != NULL);

	/* local configuration in RAM should be updated */
	if (dev->flags.config_dirty && dev->api->config.on_read != NULL) {
		CANIOT_DBG(F("config read\n"));
		ret = dev->api->config.on_read(dev);
		if (ret == 0) {
			dev->flags.config_dirty = false;
		}
	}

	return ret;
}

static int config_written(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	int ret = 0;

	/* local configuration in RAM should be updated */
	if (dev->api->config.on_write != NULL) {
#if CONFIG_CANIOT_DEVICE_DRIVERS_API
		/* we update the last telemetry time which could
		 * have changed if the timezone changed
		 */
		uint32_t prev_sec, new_sec;
		uint16_t prev_msec, new_msec;
		dev->driv->get_time(&prev_sec, &prev_msec);
#endif /* CONFIG_CANIOT_DEVICE_DRIVERS_API */

		CANIOT_DBG(F("config write\n"));

		/* call application callback to apply the new configuration */
		ret = dev->api->config.on_write(dev);

#if CONFIG_CANIOT_DEVICE_DRIVERS_API
		dev->driv->get_time(&new_sec, &new_msec);

		const int32_t diff_sec	= new_sec - prev_sec;
		const int32_t diff_msec = diff_sec * 1000u + new_msec - prev_msec;

		/* adjust start time */
		dev->system.start_time += diff_sec;

		/* adjust last_telemetry time,
		 * in order to not trigger it on time update
		 */
		dev->system.last_telemetry += diff_msec;
#endif /* CONFIG_CANIOT_DEVICE_DRIVERS_API */
	}

	return ret;
}

static int read_config_attr(struct caniot_device *dev,
							const struct attr_ref *ref,
							struct caniot_attribute *attr)
{
	ASSERT(dev != NULL);
	ASSERT(ref != NULL);
	ASSERT(attr != NULL);

	/* local configuration in RAM should be updated */
	int ret = prepare_config_read(dev);

	if (ret == 0) {
		memcpy(&attr->val, (uint8_t *)dev->config + ref->offset, ref->size);
	}

	return ret;
}

static int write_config_attr(struct caniot_device *dev,
							 const struct attr_ref *ref,
							 const struct caniot_attribute *attr)
{
	memcpy((uint8_t *)dev->config + ref->offset, &attr->val, ref->size);

	return config_written(dev);
}

static bool device_class_attr_exists(struct caniot_device *dev,
									 const struct attr_ref *ref)
{
	ASSERT(dev != NULL);
	ASSERT(ref != NULL);

	/* Check whether the attribute can be read within the current device
	 * (e.g. if the attribute exists for the current device class)
	 */
	if (ref->option & ATTR_CLASS_ALL) {
		return true;
	} else {
		caniot_did_t did;
		read_identification_nodeid(dev, &did);
		return ((ref->option >> ATTR_OPTION_CLASS_POS) & ATTR_OPTION_CLASS_MSK) ==
			   CANIOT_DID_CLS(did);
	}
}

static int attribute_read(struct caniot_device *dev,
						  const struct attr_ref *ref,
						  struct caniot_attribute *attr)
{
	ASSERT(dev != NULL);
	ASSERT(ref != NULL);
	ASSERT(attr != NULL);

	int ret = 0;

	/* print debug attr_ref */
	CANIOT_DBG(F("attr_ref: section = %hhu, offset = %hhu, option = %hhu\n"),
			   ref->section,
			   ref->offset,
			   ref->option);

	if (!device_class_attr_exists(dev, ref)) {
		return -CANIOT_ECLSATTR;
	}

	switch (ref->section) {
	case CANIOT_SECTION_DEVICE_IDENTIFICATION: {
		read_identificate_attr(dev, ref, attr);
		break;
	}

	case CANIOT_SECTION_DEVICE_SYSTEM: {
		memcpy(&attr->val, (uint8_t *)&dev->system + ref->offset, ref->size);
		break;
	}

	case CANIOT_SECTION_DEVICE_CONFIG: {
		ret = read_config_attr(dev, ref, attr);
		break;
	}

	default:
		ret = -CANIOT_EREADATTR;
	}

	return ret;
}

static void prepare_response(struct caniot_device *dev,
							 struct caniot_frame *resp,
							 caniot_frame_type_t resp_type,
							 caniot_endpoint_t endpoint)
{
	ASSERT(dev != NULL);
	ASSERT(resp != NULL);

	caniot_did_t did;

	caniot_clear_frame(resp);

	/* read device class and id from ROM */
	read_identification_nodeid(dev, &did);

	/* id */
	resp->id.query	  = CANIOT_RESPONSE;
	resp->id.endpoint = endpoint;
	resp->id.cls	  = CANIOT_DID_CLS(did);
	resp->id.sid	  = CANIOT_DID_SID(did);
	resp->id.type	  = resp_type;
}

static void resp_wrap_error(struct caniot_device *dev,
							struct caniot_frame *resp,
							const struct caniot_frame *req,
							int32_t error_code,
							uint32_t *p_error_arg)
{
	ASSERT(resp != NULL);

	/* if the error occured during a command or query telemetry,
	 * then the error frame is a RESPONSE/COMMAND
	 *
	 * otherwise (if it's an attribute error), error frame is RESPONSE/WRITE_ATTR
	 *
	 * Use same endpoint as request, even if it may not be relevant
	 * (e.g. if the error occured during a attribute request)
	 */
	prepare_response(dev, resp, caniot_resp_error_for(req->id.type), req->id.endpoint);

	write_le32(resp->buf, *(uint32_t *)&error_code);

	/* Encode the error argument if provided */
	if (p_error_arg != NULL) {
		write_le32(resp->buf + 4u, *p_error_arg);
		resp->len = 8u;
	} else {
		resp->len = 4u;
	}
}

static int write_system_attr(struct caniot_device *dev,
							 const struct attr_ref *ref,
							 const struct caniot_attribute *attr)
{
	ASSERT(dev != NULL);
	ASSERT(ref != NULL);
	ASSERT(attr != NULL);

#if CONFIG_CANIOT_DEVICE_DRIVERS_API
	if (attr->key == 0x1010U) { /* time */
		uint32_t prev_sec;
		uint16_t prev_msec;
		dev->driv->get_time(&prev_sec, &prev_msec);
		const uint32_t epoch_s = attr->val;
		dev->driv->set_time(epoch_s);

		const uint32_t diff_s = epoch_s - prev_sec;

		/* adjust last_telemetry time,
		 * in order to not trigger it on time update
		 */
		dev->system._last_telemetry_ms += diff_s * 1000u - prev_msec;
		dev->system.last_telemetry += diff_s;
		dev->system.start_time += diff_s;

		/* sets the system time for the current loop,
		 * the response to reading an attribute will
		 * send the value acknowledgement.
		 */
		dev->system.time = epoch_s;

		/* last uptime when the UNIX time was set */
		dev->system.uptime_synced = epoch_s - dev->system.start_time;

		return 0;
	}
#endif

	memcpy((uint8_t *)&dev->system + ref->offset, &attr->val, ref->size);

	return 0;
}

static int attribute_write(struct caniot_device *dev,
						   const struct attr_ref *ref,
						   const struct caniot_attribute *attr)
{
	ASSERT(dev != NULL);
	ASSERT(ref != NULL);
	ASSERT(attr != NULL);

	if ((ref->option & WRITABLE) == 0U) {
		return -CANIOT_EROATTR;
	}

	int ret;

	/* print debug attr_ref */
	CANIOT_DBG(F("attr_ref: section = %hhu, offset = %hhu, option = %hhu\n"),
			   ref->section,
			   ref->offset,
			   ref->option);

	switch (ref->section) {
	case CANIOT_SECTION_DEVICE_SYSTEM: {
		ret = write_system_attr(dev, ref, attr);
		break;
	}
	case CANIOT_SECTION_DEVICE_CONFIG: {
		ret = write_config_attr(dev, ref, attr);
		break;
	}

	default:
		ret = -CANIOT_EWRITEATTR;
	}

	return ret;
}

#if CONFIG_CANIOT_DEVICE_HANDLE_BLC_SYS_CMD
static int call_blc_sys_cmd_cb(struct caniot_device *dev, caniot_blc_sys_cmd_t cmd)
{
	return call_blc_sys_cmd_cb(dev, cmd);
}

static int handle_blc_sys_cmd(struct caniot_device *dev,
							  struct caniot_blc_sys_command *sys_cmd)
{
	int ret = 0;

	/* Inhibit has the highest priority */
	switch (sys_cmd->inhibit) {
	case CANIOT_TSP_CMD_ON:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_INHIBIT_ON);
		break;
	case CANIOT_TSP_CMD_OFF:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_INHIBIT_OFF);
		break;
	case CANIOT_TSP_CMD_PULSE:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_INHIBIT_PULSE);
		break;
	default:
		break;
	}
	if (ret) goto exit;

	if (sys_cmd->config_reset) {
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_CONFIG_RESET);
		if (ret) goto exit;
	}

	switch (sys_cmd->watchdog) {
	case CANIOT_TS_CMD_ON:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_WATCHDOG_ENABLE);
		break;
	case CANIOT_TS_CMD_OFF:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_WATCHDOG_DISABLE);
		break;
	case CANIOT_TS_CMD_TOGGLE:
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_WATCHDOG_TOGGLE);
		break;
	default:
		break;
	}
	if (ret) goto exit;

	if (sys_cmd->reset) {
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_RESET);
		if (ret) goto exit;
	}

	if (sys_cmd->_watchdog_reset) {
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_WATCHDOG_RESET);
		if (ret) goto exit;
	}

	if (sys_cmd->_software_reset) {
		ret = call_blc_sys_cmd_cb(dev, CANIOT_BLC_SYS_CMD_SOFT_RESET);
		if (ret) goto exit;
	}

exit:
	return ret;
}
#endif

static int handle_command_req(struct caniot_device *dev, const struct caniot_frame *req)
{
	ASSERT(dev != NULL);
	ASSERT(req != NULL);

	int ret;
	const caniot_endpoint_t ep = req->id.endpoint;

	CANIOT_DBG(F("Executing command handler (0x%p) for endpoint %d\n"),
			   (void *)&dev->api->command_handler,
			   ep);

#if CONFIG_CANIOT_DEVICE_HANDLE_BLC_SYS_CMD
	/* Last byte of the BLC command is the system command */
	if ((ep == CANIOT_ENDPOINT_BOARD_CONTROL) && (req->len > 8u) &&
		(dev->api->blc_sys_cmd_handler != NULL)) {
		struct caniot_blc_sys_command sys_cmd;
		caniot_blc_sys_command_from_byte(&sys_cmd, req->buf[8u]);
		ret = handle_blc_sys_cmd(dev, &sys_cmd);
		if (ret != 0) {
			return ret;
		}
	}
#endif

	if (dev->api->command_handler != NULL) {
		ret = dev->api->command_handler(dev, ep, req->buf, req->len);
		dev->system.last_command_error = ret;
	} else {
		ret = -CANIOT_EHANDLERC;
	}

	return ret;
}

static int build_telemetry_resp(struct caniot_device *dev,
								struct caniot_frame *resp,
								caniot_endpoint_t ep)
{
	ASSERT(dev != NULL);
	ASSERT(resp != NULL);

	int ret;

	/* TODO check endpoint relative to class*/

	prepare_response(dev, resp, CANIOT_FRAME_TYPE_TELEMETRY, ep);

	if (dev->api->telemetry_handler == NULL) {
		return -CANIOT_EHANDLERT;
	}

	CANIOT_DBG(F("Executing telemetry handler (0x%p) for endpoint %d\n"),
			   (void *)&dev->api->telemetry_handler,
			   ep);

	/* buffer */
	ret = dev->api->telemetry_handler(dev, ep, resp->buf, &resp->len);
	if (ret == 0) {
		dev->system.sent.telemetry++;
	}

	dev->system.last_telemetry_error = ret;

	/* TODO check and force response length */

	return ret;
}

static int handle_req_attribute(struct caniot_device *dev,
								const struct caniot_frame *req,
								struct caniot_frame *resp,
								bool do_write,
								uint16_t *key)
{
	ASSERT(dev != NULL);
	ASSERT(req != NULL);
	ASSERT(resp != NULL);

	int ret;
	struct attr_ref ref;
	struct caniot_attribute attr;
	bool custom_attr;

	// get attribute key from buffer
	if (req->len >= 2u) {
		attr.key = read_le16(req->buf);
	} else {
		ret = -CANIOT_EFRAME; // invalid frame (key missing for attribute);
		goto exit;
	}

	// copy key to output (so that it can be used in error response)
	if (key) *key = attr.key;

	// Resolve attribute location in the system
	ret = attr_resolve(attr.key, &ref);
	if (ret == 0) {
		custom_attr = false;
	} else if ((dev->api->custom_attr.read != NULL) &&
			   (dev->api->custom_attr.write != NULL)) {
		custom_attr = true;
		ret			= 0;
	} else {
		goto exit;
	}

	// Write attribute request
	if (do_write) {
		if (req->len >= 6u) {
			attr.val = read_le32(req->buf + 2u);
		} else {
			ret = -CANIOT_EFRAME; // invalid frame (value missing for attribute write)
			goto exit;
		}

		if (!custom_attr) {
			// standard attribute
			ret = attribute_write(dev, &ref, &attr);
		} else {
			// custom attribute
			ret = dev->api->custom_attr.write(dev, attr.key, attr.val);
		}
	}

	// Read back attribute value
	if (ret == 0) {
		// clear value
		attr.val = 0u;

		if (!custom_attr) {
			// standard attribute
			ret = attribute_read(dev, &ref, &attr);
		} else {
			// custom attribute
			ret = dev->api->custom_attr.read(dev, attr.key, &attr.val);
		}
	}

	// Prepare response
	if (ret == 0) {
		/* Use same endpoint as request, even if this is meaningless for
		 * attribute request with current specification of the protocol
		 */
		prepare_response(dev, resp, CANIOT_FRAME_TYPE_READ_ATTRIBUTE, req->id.endpoint);

		resp->len = 6u;
		write_le16(resp->buf, attr.key);
		write_le32(resp->buf + 2u, attr.val);
	}

exit:
	return ret;
}

int caniot_device_handle_rx_frame(struct caniot_device *dev,
								  const struct caniot_frame *req,
								  struct caniot_frame *resp)
{
	ASSERT(dev != NULL);
	ASSERT(req != NULL);
	ASSERT(resp != NULL);

	int ret;
	uint32_t error_arg = 0u;
	uint32_t *p_arg	   = NULL;

	/* no response in this case */
	if (req->id.query != CANIOT_QUERY) {
		ret = -CANIOT_EINVAL;
		goto exit;
	}

	dev->system.received.total++;

	switch (req->id.type) {
	case CANIOT_FRAME_TYPE_COMMAND: {
		dev->system.received.command++;
		ret = handle_command_req(dev, req);
		if (ret == 0) {
			ret = build_telemetry_resp(dev, resp, req->id.endpoint);
		}
		break;
	}
	case CANIOT_FRAME_TYPE_TELEMETRY: {
		dev->system.received.request_telemetry++;
		ret = build_telemetry_resp(dev, resp, req->id.endpoint);
		break;
	}

	case CANIOT_FRAME_TYPE_WRITE_ATTRIBUTE: {
		uint16_t key;
		dev->system.received.write_attribute++;
		ret = handle_req_attribute(dev, req, resp, true, &key);
		if ((ret != 0) && (ret != -CANIOT_EFRAME)) {
			error_arg = (uint32_t)key;
			p_arg	  = &error_arg;
		}
		break;
	}
	case CANIOT_FRAME_TYPE_READ_ATTRIBUTE: {
		uint16_t key;
		dev->system.received.read_attribute++;
		ret = handle_req_attribute(dev, req, resp, false, &key);
		if ((ret != 0) && (ret != -CANIOT_EFRAME)) {
			error_arg = (uint32_t)key;
			p_arg	  = &error_arg;
		}
		break;
	}
	}

exit:
	if (ret != 0) {
		/* prepare error response */
		resp_wrap_error(dev, resp, req, (int32_t)ret, p_arg);
	}

	return ret;
}

int caniot_device_verify(struct caniot_device *dev)
{
	(void)dev;

	return -CANIOT_ENIMPL;
}

bool caniot_device_time_synced(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	return dev->system.uptime_synced != 0;
}

/*____________________________________________________________________________*/

#if CONFIG_CANIOT_DEVICE_DRIVERS_API
uint32_t caniot_device_time_until_process(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	uint32_t remaining;

	if (CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES && !dev->flags.startup_attrs_sent) {
		remaining = 0; /* Startup attributes not sent yet, process immediately */
	} else if (prepare_config_read(dev) != 0) {
		remaining = 1000u; /* default 1 second in case of config read error */
	} else if (!dev->config->flags.telemetry_periodic_enabled) {
		remaining = (uint32_t)-1; /* Periodic telemetry disabled */
	} else {
		uint32_t sec;
		uint16_t msec;
		dev->driv->get_time(&sec, &msec);
		const uint32_t now_ms	   = sec * 1000 + msec;
		const uint32_t ellapsed_ms = now_ms - dev->system._last_telemetry_ms;
		CANIOT_DBG(F("now: %u _last_telemetry_ms: %u since last: %u < period: %u "
					 "? (* ms)\n"),
				   (FMT_UINT_CAST)now_ms,
				   (FMT_UINT_CAST)dev->system._last_telemetry_ms,
				   (FMT_UINT_CAST)ellapsed_ms,
				   (FMT_UINT_CAST)dev->config->telemetry.period);

		if (dev->config->telemetry.period <= ellapsed_ms) {
			remaining = 0u;
		} else {
			remaining = dev->config->telemetry.period - ellapsed_ms;
		}
	}

	return remaining;
}

static uint32_t get_response_delay(struct caniot_device *dev, bool random)
{
	ASSERT(dev != NULL);

	uint32_t delay_ms = 0U;

	/* delay only on broadcast command */
	if (random == true) {
		ASSERT(dev->driv->entropy != NULL);

		/* define default parameters */
		uint16_t delay_min = CANIOT_TELEMETRY_DELAY_MIN_DEFAULT_MS;
		uint16_t delay_max = CANIOT_TELEMETRY_DELAY_MAX_DEFAULT_MS;

		uint16_t rdm;
		dev->driv->entropy((uint8_t *)&rdm, sizeof(rdm));

		/* get parameters from local configuration if possible */
		int ret = prepare_config_read(dev);
		if (ret == 0) {
			delay_min = dev->config->telemetry.delay_min;
			delay_max = dev->config->telemetry.delay_max;
		}

		/* evaluate amplitude */
		uint32_t amplitude = CANIOT_TELEMETRY_DELAY_MAX_DEFAULT_MS;
		if (delay_max > delay_min) {
			amplitude = delay_max - delay_min;
		}

		delay_ms = delay_min + (rdm % amplitude);
	}

	return delay_ms;
}

void caniot_device_trigger_telemetry_ep(struct caniot_device *dev, caniot_endpoint_t ep)
{
	ASSERT(dev != NULL);
	ASSERT(ep <= CANIOT_ENDPOINT_BOARD_CONTROL);

	dev->flags.request_telemetry_ep |= (1u << ep);
}

void caniot_device_trigger_periodic_telemetry(struct caniot_device *dev)
{
	caniot_device_trigger_telemetry_ep(dev, dev->config->flags.telemetry_endpoint);
}

bool caniot_device_triggered_telemetry_ep(struct caniot_device *dev, caniot_endpoint_t ep)
{
	ASSERT(dev != NULL);
	ASSERT(ep <= CANIOT_ENDPOINT_BOARD_CONTROL);

	return (dev->flags.request_telemetry_ep & (1u << ep)) != 0u;
}

bool caniot_device_triggered_telemetry_any(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	return dev->flags.request_telemetry_ep != 0u;
}

static inline void telemetry_trig_clear_ep(struct caniot_device *dev,
										   caniot_endpoint_t ep)
{
	ASSERT(dev != NULL);
	ASSERT(ep <= CANIOT_ENDPOINT_BOARD_CONTROL);

	dev->flags.request_telemetry_ep &= ~(1u << ep);
}

int caniot_device_process(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	int ret;
	struct caniot_frame req, resp;
	uint32_t now_ms;

	/* Refresh configuration */
	prepare_config_read(dev);

	/* get current time (ms precision) */
	uint16_t msec;
	dev->driv->get_time(&dev->system.time, &msec);
	dev->system.uptime = dev->system.time - dev->system.start_time;

	/* Periodic telemetry enabled */
	if (dev->config->flags.telemetry_periodic_enabled) {
		/* check if we need to send telemetry (calculated in seconds) */
		now_ms					   = dev->system.time * 1000 + msec;
		const uint32_t ellapsed_ms = now_ms - dev->system._last_telemetry_ms;

		CANIOT_DBG(F("now: %u _last_telemetry_ms: %u ellapsed_ms: %u >= period: "
					 "%u ? (* "
					 "ms)\n"),
				   (FMT_UINT_CAST)now_ms,
				   (FMT_UINT_CAST)dev->system._last_telemetry_ms,
				   (FMT_UINT_CAST)ellapsed_ms,
				   (FMT_UINT_CAST)dev->config->telemetry.period);

		if (ellapsed_ms >= dev->config->telemetry.period) {
			caniot_device_trigger_telemetry_ep(dev,
											   dev->config->flags.telemetry_endpoint);

			CANIOT_DBG(F("Requesting telemetry\n"));
		}
	}

	/* received any incoming frame */
	caniot_clear_frame(&req);
	ret = dev->driv->recv(&req);

	/* response delay is not random by default */
	bool random_delay = false;

#if CONFIG_CANIOT_DEBUG || CONFIG_CANIOT_DEVICE_FILTER_FRAME
	if (ret == 0) {
		if (!caniot_device_is_target(caniot_device_get_id(dev), &req)) {
			dev->system.received.ignored++;
			CANIOT_ERR(F("Unexpected frame id received: %u\n"));
			ret = -CANIOT_EUNEXPECTED;
		}
	}
#endif

	/* if we received a frame */
	if (ret == 0) {
		/* handle received frame */
		ret = caniot_device_handle_rx_frame(dev, &req, &resp);

		/* broadcast request requires a randomly delayed response */
		if (caniot_is_broadcast(caniot_frame_get_did(&req)) == true) {
			random_delay = true;
		}

#if CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES
	} else if ((ret == -CANIOT_EAGAIN) && !dev->flags.startup_attrs_sent &&
			   dev->_startup_attrs_cursor) {
		/* if we didn't received a frame but startup attributes are pending */

		uint16_t attribute = *dev->_startup_attrs_cursor;

		/* Create a fake frame to read the attribute */
		caniot_clear_frame(&req);
		caniot_frame_set_did(&req, caniot_device_get_id(dev));
		caniot_build_query_read_attribute(&req, attribute);

		/* Handle the request.
		 * Use deep function handle_req_attribute() instead
		 * of caniot_device_handle_rx_frame() to avoid impacting the stats
		 * due to the fake frame.
		 */
		ret = handle_req_attribute(dev, &req, &resp, false, NULL);
		switch (ret) {
		case -CANIOT_ECLSATTR:
		case -CANIOT_EKEYSECTION:
		case -CANIOT_EKEYATTR:
		case -CANIOT_EKEYPART:
		case -CANIOT_ENOATTR:
		case -CANIOT_EREADATTR:
		case -CANIOT_EROATTR:
		case -CANIOT_EWRITEATTR:
			/* Ignore errors related to attributes */
			ret = 0;
			break;
		}

		if (ret == 0) {
			dev->_startup_attrs_cursor++;

			/* If all startup attributes have been sent */
			if (!*dev->_startup_attrs_cursor) {
				dev->flags.startup_attrs_sent = 1u;
			}
		}

		/* As the req frame is sent by the current device, it doesn't need to be
		 * randomly delayed.
		 */
#endif /* CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES */
	} else if ((ret == -CANIOT_EAGAIN) && caniot_device_triggered_telemetry_any(dev)) {
		/* if we didn't received a frame but telemetry is requested */

		/* Iterate over all endpoints then prepare the telemetry frame for the
		 * first found. In the case of multiple endpoints requesting telemetry,
		 * "board control" has the highest priority.
		 */
		for (int8_t ep = CANIOT_ENDPOINT_BOARD_CONTROL; ep >= CANIOT_ENDPOINT_APP; ep--) {
			if (caniot_device_triggered_telemetry_ep(dev, ep) == true) {
				ret = build_telemetry_resp(dev, &resp, ep);
				break;
			}
		}
	} else {
		/* Error */
		goto exit;
	}

	if (ret != 0) {
		prepare_config_read(dev);

		/* if "error frame" are not enabled */
		if (dev->config->flags.error_response == 0u) {
			goto exit;
		}
	}

	/* send response or error frame if configured */
	ret = dev->driv->send(&resp, get_response_delay(dev, random_delay));
	if (ret == 0) {
		dev->system.sent.total++;

		/* if we sent a telemetry frame */
		if (is_telemetry_response(&resp) == true) {
			telemetry_trig_clear_ep(dev, resp.id.endpoint);

			/* If period telemetry is enabled and
			 * the endpoint is the one configured for periodic telemetry,
			 * update the last telemetry timestamp.
			 */
			if (dev->config->flags.telemetry_periodic_enabled &&
				resp.id.endpoint == dev->config->flags.telemetry_endpoint) {
				dev->system._last_telemetry_ms = now_ms;
				dev->system.last_telemetry	   = dev->system.time;
			}
		}
	}

exit:
	return ret;
}

void caniot_app_init(struct caniot_device *dev)
{
	ASSERT(dev != NULL);
	ASSERT(dev->driv != NULL);
	ASSERT(dev->driv->get_time != NULL);

	memset(&dev->system, 0x00U, sizeof(dev->system));

	dev->driv->get_time(&dev->system.start_time, NULL);

	dev->flags.request_telemetry_ep = 0u;
	dev->flags.config_dirty			= 1u;
	dev->flags.initialized			= 1u;

#if CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES
	if (dev->startup_attrs && *dev->startup_attrs) {
		dev->flags.startup_attrs_sent = 0u;
		dev->_startup_attrs_cursor	  = dev->startup_attrs;
	} else {
		dev->flags.startup_attrs_sent = 1u;
		dev->_startup_attrs_cursor	  = NULL;
	}
#endif /* CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES */
}

void caniot_app_deinit(struct caniot_device *dev)
{
	ASSERT(dev != NULL);

	dev->flags.request_telemetry_ep = 0u;
	dev->flags.initialized			= 0u;
	dev->flags.config_dirty			= 1u;
}

#endif /* CONFIG_CANIOT_DEVICE_DRIVERS_API */

static void attribute_copy_from_ref(struct caniot_device_attribute *attr,
									struct attr_ref *ref)
{
	attr->read		 = ref->option & READABLE ? 1u : 0u;
	attr->write		 = ref->option & WRITABLE ? 1u : 0u;
	attr->persistent = ref->section_option & PERSISTENT ? 1u : 0u;
	attr->section	 = ref->section;
}

static void attribute_copy_name_from_key(struct caniot_device_attribute *attr,
										 uint16_t key)
{
#if CONFIG_CANIOT_ATTRIBUTE_NAME
	const struct attr_section *section = attr_get_section(key);
	const struct attribute *attribute  = section ? attr_get(key, section) : NULL;
	if (attribute) {
		strncpy(attr->name, attribute->name, CANIOT_ATTR_NAME_MAX_LEN);
		return;
	}
#endif
	memset(attr->name, 0x00u, CANIOT_ATTR_NAME_MAX_LEN);
}

int caniot_attr_get_by_key(struct caniot_device_attribute *attr, uint16_t key)
{
	int ret;
	struct attr_ref ref;

	if (!attr) {
		return -CANIOT_EINVAL;
	}

	ret = attr_resolve(key, &ref);
	if (ret != 0) {
		return ret;
	}

	attribute_copy_from_ref(attr, &ref);
	attribute_copy_name_from_key(attr, key);
	attr->key = key;

	return 0;
}

int caniot_attr_get_by_name(struct caniot_device_attribute *attr, const char *name)
{
	(void)attr;
	(void)name;

	return -CANIOT_ENOTSUP;
}

int caniot_attr_iterate(caniot_device_attribute_handler_t *handler, void *user_data)
{
	if (!handler) {
		return -CANIOT_EINVAL;
	}

	int count = 0;
	const struct attr_section *section;
	struct caniot_device_attribute attr;
	uint16_t key;

	for (uint8_t si = 0u; si < ARRAY_SIZE(attr_sections); si++) {
		section = &attr_sections[si];

		for (uint8_t ai = 0u; ai < attr_get_section_size(section); ai++) {
			key = 0u;
			ATTR_KEY_SECTION_SET(key, si);
			ATTR_KEY_ATTR_SET(key, ai);

			if (caniot_attr_get_by_key(&attr, key) == 0) {
				count++;
				bool zcontinue = handler(&attr, user_data);
				if (!zcontinue) goto exit;
			}
		}
	}

exit:
	return count;
}

static bool verify_filter_or_broadcast(uint32_t id, uint16_t filter)
{
	bool targeted;

	const uint16_t std_id = id & 0x7FFu; /* CAN standard ID mask (11 bits) */

	const uint16_t mask		  = caniot_device_get_mask();
	const uint16_t broad_filt = caniot_device_get_filter_broadcast();

	CANIOT_DBG(F("mask: 0x%04X, filt: 0x%04X, broad_filt: 0x%04X, "
				 "std_id: 0x%04X\n"),
			   mask,
			   filter,
			   broad_filt,
			   std_id);

	if ((std_id & mask) == filter) {
		targeted = true;
	} else if ((std_id & mask) == broad_filt) {
		targeted = true;
	} else {
		targeted = false;
	}

	return targeted;
}

bool caniot_device_targeted(caniot_did_t did, bool ext, bool rtr, uint32_t id)
{
	(void)rtr;

	if (ext) return false;

	const uint16_t dev_filt = caniot_device_get_filter(did);
	return verify_filter_or_broadcast(id, dev_filt);
}

bool caniot_device_targeted_class(uint8_t cls, bool ext, bool rtr, uint32_t id)
{
	(void)rtr;

	if (ext) return false;

	const uint16_t cls_filt = caniot_device_get_filter_by_cls(cls);
	return verify_filter_or_broadcast(id, cls_filt);
}

uint16_t caniot_attr_key_get_root(uint16_t key)
{
	return key & ((ATTR_KEY_SECTION_MASK << ATTR_KEY_SECTION_OFFSET) |
				  (ATTR_KEY_ATTR_MASK << ATTR_KEY_ATTR_OFFSET));
}

uint8_t caniot_attr_key_get_part(uint16_t key)
{
	return (uint8_t)key & ATTR_KEY_PART_MASK;
}

enum caniot_device_section caniot_attr_key_get_section(uint16_t key)
{
	return (enum caniot_device_section)ATTR_KEY_SECTION_GET(key);
}