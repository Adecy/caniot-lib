/*
 * Copyright (c) 2023 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CANIOT_DEVICE_H
#define _CANIOT_DEVICE_H

#include <stdbool.h>

#include <caniot/caniot.h>

#ifdef __cplusplus
extern "C" {
#endif

// Device identification
struct caniot_device_id {
	/* Device ID (6 bits) defined has (Class << 3) | SID */
	caniot_did_t did;

	/* Device version defined as:
	 * - CANIOT version (8 bits)
	 * - Device firmware version (8 bits)
	 */
	uint16_t version;

	/* Device name (32 bytes) */
	char name[32];

	/* Magic number (32 bits) */
	uint32_t magic_number;

#if CONFIG_CANIOT_BUILD_INFOS
	/* Build date (32 bits) */
	uint32_t build_date;

	/* Build commit (32 bits) */
	uint8_t build_commit[20u];
#endif

	/* Firmware features (128 bits) */
	uint32_t features[4u];
} __PACKED;

struct caniot_device_system {
	uint32_t uptime_synced;	 /* s - uptime when time was last synced */
	uint32_t time;			 /* s - current time in seconds since epoch */
	uint32_t uptime;		 /* s - uptime in seconds */
	uint32_t start_time;	 /* s - start time in seconds since epoch */
	uint32_t last_telemetry; /* s - last telemetry time in seconds since epoch */

	/* ms - time in milliseconds in system time
	 * (can be uptime or time since epoch), modulo 32 !!!
	 * this is used to precisely measure time between two telemetry events
	 */
	uint32_t _last_telemetry_ms;

	struct {
		uint32_t total;
		uint32_t read_attribute;
		uint32_t write_attribute;
		uint32_t command;
		uint32_t request_telemetry;
		uint32_t ignored; /* frame doesn't target current device */
	} received;
	uint32_t _unused3;
	struct {
		uint32_t total;
		uint32_t telemetry;
	} sent;
	uint32_t _unused4;
	int16_t last_command_error;
	int16_t last_telemetry_error;
	int16_t _unused5;
	uint8_t battery;
} __PACKED;

struct caniot_class0_config {
	/* Duration in seconds of the pulse for OC1, OC2, RL1, RL2
	 * respectively. */
	uint32_t pulse_durations[4u];

	/* Output default values for OC1, OC2, RL1, RL2 respectively. */
	uint32_t outputs_default;

	/* The mask gpio to be used for notifications. */
	uint32_t telemetry_on_change;
} __PACKED;

struct caniot_class1_config {
	/* Duration in seconds of the pulse for all outputs. */
	uint32_t pulse_durations[19u];

	/* Bitmap of self managed IOs. i.e. that cannot be controlled remotely. */
	uint32_t self_managed;

	/* Bitmap of IO directions */
	uint32_t directions; /* 0 = input, 1 = output */

	/* Bitmap of output default values for outputs */
	uint32_t outputs_default;

	/* The mask gpio to be used for notifications. */
	uint32_t telemetry_on_change;
} __PACKED;

struct caniot_device_config {
	struct {
		uint32_t period; /* period in milliseconds */
		union {
			uint16_t delay_min; /* minimum in milliseconds */
			uint16_t delay;		/* delay in milliseconds */
		};
		uint16_t delay_max; /* maximum in milliseconds */
	} telemetry;

	struct {
		/* Allow the application to send error frames in case of error */
		uint8_t error_response : 1;

		/* Allow the application to randomly delay telemetry */
		uint8_t telemetry_delay_rdm : 1;

		/* Endpoint to use to send periodic telemetry */
		caniot_endpoint_t telemetry_endpoint : 2;

		/* Periodic telemetry is enabled */
		uint8_t telemetry_periodic_enabled : 1;
	} flags;

	int32_t timezone;

	struct {
		char region[2];
		char country[2];
	} location;

	union {
		struct caniot_class0_config cls0_gpio;
		struct caniot_class1_config cls1_gpio;
	};
} __PACKED;

struct caniot_device {
	const struct caniot_device_id *identification;
	struct caniot_device_system system;

	/* buffer used to store the configuration */
	struct caniot_device_config *config;

	const struct caniot_device_api *api;

#if CONFIG_CANIOT_DEVICE_DRIVERS_API
	const struct caniot_drivers_api *driv;
#endif

#if CONFIG_CANIOT_DEVICE_STARTUP_ATTRIBUTES
	/* NULL terminated list of attributes to send on startup
	 * * Attribute 0000 (node ID) cannot be sent on startup
	 */
	const uint16_t *startup_attrs;

	/* Cursor to the next attribute to send */
	const uint16_t *_startup_attrs_cursor;
#endif

	struct {
		uint8_t request_telemetry_ep : 4u; /* Bitmask represent what endpoint(s)
							  to send telemetry for */
		uint8_t initialized : 1u;		   /* Device is initialized */
		uint8_t config_dirty : 1u;		   /* Settings have been modified */
		uint8_t startup_attrs_sent : 1u;   /* Startup attributes have been sent */
	} flags;
};

typedef int(caniot_telemetry_handler_t)(struct caniot_device *dev,
										caniot_endpoint_t ep,
										unsigned char *buf,
										uint8_t *len);

typedef int(caniot_command_handler_t)(struct caniot_device *dev,
									  caniot_endpoint_t ep,
									  const unsigned char *buf,
									  uint8_t len);

#if CONFIG_CANIOT_DEVICE_HANDLE_BLC_SYS_CMD
/**
 * @brief Individual commands for the BLC_SYS command
 */
typedef enum {
	CANIOT_BLC_SYS_CMD_NONE = 0,
	CANIOT_BLC_SYS_CMD_RESET,
	CANIOT_BLC_SYS_CMD_SOFT_RESET,
	CANIOT_BLC_SYS_CMD_WATCHDOG_RESET,
	CANIOT_BLC_SYS_CMD_WATCHDOG_ENABLE,
	CANIOT_BLC_SYS_CMD_WATCHDOG_DISABLE,
	CANIOT_BLC_SYS_CMD_WATCHDOG_TOGGLE,
	CANIOT_BLC_SYS_CMD_CONFIG_RESET,
	CANIOT_BLC_SYS_CMD_INHIBIT_ON,
	CANIOT_BLC_SYS_CMD_INHIBIT_OFF,
	CANIOT_BLC_SYS_CMD_INHIBIT_PULSE,
} caniot_blc_sys_cmd_t;

typedef int(caniot_command_blc_sys_handler_t)(struct caniot_device *dev,
											  caniot_blc_sys_cmd_t sys_cmd);
#endif

struct caniot_device_api {
	struct {
		/* called before configuration will be read */
		int (*on_read)(struct caniot_device *dev);

		/* called after configuration is updated */
		int (*on_write)(struct caniot_device *dev);
	} config;

	struct {
		int (*read)(struct caniot_device *dev, uint16_t key, uint32_t *val);
		int (*write)(struct caniot_device *dev, uint16_t key, uint32_t val);
	} custom_attr;

	/* Handle command */
	caniot_command_handler_t *command_handler;

	/* Build telemetry */
	caniot_telemetry_handler_t *telemetry_handler;

#if CONFIG_CANIOT_DEVICE_HANDLE_BLC_SYS_CMD
	/* Board level control system command handler */
	caniot_command_blc_sys_handler_t *blc_sys_cmd_handler;
#endif
};

/**
 * @brief Log the device identification
 * 
 * @param dev 
 */
void caniot_print_device_identification(const struct caniot_device *dev);

/**
 * @brief Mark the device configuration as dirty
 * 
 * @param dev 
 */
void caniot_device_config_mark_dirty(struct caniot_device *dev);

/**
 * @brief Clear device system statistics
 * 
 * @param dev 
 * @return int 
 */
int caniot_device_system_reset(struct caniot_device *dev);

/**
 * @brief Handle incoming CANIOT frame
 * 
 * @param dev device
 * @param req Incoming frame
 * @param resp Response frame
 * @return int 0 on success, negative error code otherwise
 */
int caniot_device_handle_rx_frame(struct caniot_device *dev,
								  const struct caniot_frame *req,
								  struct caniot_frame *resp);

/**
 * @brief Returns the device caniot ID 
 * @param dev 
 * @return caniot_did_t 
 */
caniot_did_t caniot_device_get_id(struct caniot_device *dev);

/**
 * @brief Returns the time in milliseconds until the next process function should 
 * be called.
 * The device can be put to sleep until this time.
 * 
 * @param dev 
 * @return uint32_t 
 */
uint32_t caniot_device_time_until_process(struct caniot_device *dev);

/**
 * @brief Get the mask to receive all frames targeted to devices.
 *
 * @return uint16_t
 */
static inline uint16_t caniot_device_get_mask(void)
{
	return 0x1fc; // 0b00111111100U;
}

/**
 * @brief Get the mask to receive all frames targeted to class.
 *
 * @return uint16_t
 */
static inline uint16_t caniot_device_get_mask_by_cls(void)
{
	return 0x3c; // 0b00000111100U;
}

/**
 * @brief Get the filter for the given device ID
 *
 * @param did
 * @return uint16_t
 */
uint16_t caniot_device_get_filter(caniot_did_t did);

/**
 * @brief Get the broadcast filter
 *
 * @return uint16_t
 */
uint16_t caniot_device_get_filter_broadcast(void);

/**
 * @brief Get the filter for the given device class
 *
 * @param cls
 * @return uint16_t
 */
uint16_t caniot_device_get_filter_by_cls(uint8_t cls);

/**
 * @brief static-inline version of caniot_device_get_filter
 *
 * @param did
 * @return uint16_t
 */
static inline uint16_t _si_caniot_device_get_filter(caniot_did_t did)
{
	return CANIOT_ID(0U, CANIOT_QUERY, CANIOT_DID_CLS(did), CANIOT_DID_SID(did), 0U);
}

/**
 * @brief static-inline version of caniot_device_get_filter_broadcast
 *
 * @param did
 * @return uint16_t
 */
static inline uint16_t _si_caniot_device_get_filter_broadcast(void)
{
	return CANIOT_ID(0U,
					 CANIOT_QUERY,
					 CANIOT_DID_CLS(CANIOT_DID_BROADCAST),
					 CANIOT_DID_SID(CANIOT_DID_BROADCAST),
					 0U);
}

/**
 * @brief Verify whether the device is targeted by the CAN frame (ext, rtr, id)
 *
 * This function programmatically verifies if the device is targeted by the frame.
 *
 * @param dev
 * @param ext
 * @param rtr
 * @param id
 * @return true
 * @return false
 */
bool caniot_device_targeted(caniot_did_t did, bool ext, bool rtr, uint32_t id);

/**
 * @brief Verify whether the device class is targeted by the CAN frame (ext, rtr, id)
 *
 * This function programmatically verifies if the device is targeted by the frame.
 *
 * @param cls
 * @param ext
 * @param rtr
 * @param id
 * @return true
 * @return false
 */
bool caniot_device_targeted_class(uint8_t cls, bool ext, bool rtr, uint32_t id);

/*____________________________________________________________________________*/

void caniot_app_init(struct caniot_device *dev);

void caniot_app_deinit(struct caniot_device *dev);

/**
 * @brief Receive incoming CANIOT message if any and handle it
 *
 * @param dev
 * @return int
 */
int caniot_device_process(struct caniot_device *dev);

/**
 * @brief Return whether the device time has been synced
 * 
 * @param dev 
 * @return true 
 * @return false 
 */
bool caniot_device_time_synced(struct caniot_device *dev);

/**
 * @brief Request the device to send telemetry for the given endpoint
 * 
 * @param dev 
 * @param ep 
 */
void caniot_device_trigger_telemetry_ep(struct caniot_device *dev, caniot_endpoint_t ep);

/**
 * @brief Request the device to immediately send configured periodic telemetry
 * 
 * @param dev 
 */
void caniot_device_trigger_periodic_telemetry(struct caniot_device *dev);

/**
 * @brief Returns whether the device has triggered telemetry for the given endpoint
 * 
 * @param dev 
 */
bool caniot_device_triggered_telemetry_ep(struct caniot_device *dev,
										  caniot_endpoint_t ep);

/**
 * @brief Returns whether the device has triggered telemetry for any endpoint
 * 
 * @param dev 
 */
bool caniot_device_triggered_telemetry_any(struct caniot_device *dev);

/*____________________________________________________________________________*/

/**
 * @brief Verify if device is properly defined
 *
 * @param dev
 * @return int
 */
int caniot_device_verify(struct caniot_device *dev);

/*____________________________________________________________________________*/

#define CANIOT_ATTR_KEY(section, attr, part)                                             \
	((section & 0xF) << 12 | (attr & 0xFF) << 4 | (part & 0xF))

#define CANIOT_ATTR_KEY_ID_NODEID		CANIOT_ATTR_KEY(0, 0x0, 0) // 0x0000
#define CANIOT_ATTR_KEY_ID_VERSION		CANIOT_ATTR_KEY(0, 0x1, 0) // 0x0010
#define CANIOT_ATTR_KEY_ID_NAME			CANIOT_ATTR_KEY(0, 0x2, 0) // 0x0020
#define CANIOT_ATTR_KEY_ID_MAGIC_NUMBER CANIOT_ATTR_KEY(0, 0x3, 0) // 0x0030
#define CANIOT_ATTR_KEY_ID_BUILD_DATE	CANIOT_ATTR_KEY(0, 0x4, 0) // 0x0040
#define CANIOT_ATTR_KEY_ID_BUILD_COMMIT CANIOT_ATTR_KEY(0, 0x5, 0) // 0x0050
#define CANIOT_ATTR_KEY_ID_FEATURES		CANIOT_ATTR_KEY(0, 0x6, 0) // 0x0060

#define CANIOT_ATTR_KEY_SYSTEM_UPTIME_SYNCED		  CANIOT_ATTR_KEY(1, 0x0, 0) // 0x1000
#define CANIOT_ATTR_KEY_SYSTEM_TIME					  CANIOT_ATTR_KEY(1, 0x1, 0) // 0x1010
#define CANIOT_ATTR_KEY_SYSTEM_UPTIME				  CANIOT_ATTR_KEY(1, 0x2, 0) // 0x1020
#define CANIOT_ATTR_KEY_SYSTEM_START_TIME			  CANIOT_ATTR_KEY(1, 0x3, 0) // 0x1030
#define CANIOT_ATTR_KEY_SYSTEM_LAST_TELEMETRY		  CANIOT_ATTR_KEY(1, 0x4, 0) // 0x1040
#define CANIOT_ATTR_KEY_SYSTEM_LAST_TELEMETRY_MS_MOD  CANIOT_ATTR_KEY(1, 0xB, 0) // 0x10B0
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_TOTAL		  CANIOT_ATTR_KEY(1, 0x5, 0) // 0x1050
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_READ_ATTR	  CANIOT_ATTR_KEY(1, 0x6, 0) // 0x1060
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_WRITE_ATTR	  CANIOT_ATTR_KEY(1, 0x7, 0) // 0x1070
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_COMMAND		  CANIOT_ATTR_KEY(1, 0x8, 0) // 0x1080
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_REQ_TELEMETRY CANIOT_ATTR_KEY(1, 0x9, 0) // 0x1090
#define CANIOT_ATTR_KEY_SYSTEM_RECEIVED_IGNORED		  CANIOT_ATTR_KEY(1, 0xA, 0) // 0x10A0
#define CANIOT_ATTR_KEY_SYSTEM_SENT_TOTAL			  CANIOT_ATTR_KEY(1, 0xC, 0) // 0x10C0
#define CANIOT_ATTR_KEY_SYSTEM_SENT_TELEMETRY		  CANIOT_ATTR_KEY(1, 0xD, 0) // 0x10D0
#define CANIOT_ATTR_KEY_SYSTEM_UNUSED4				  CANIOT_ATTR_KEY(1, 0xE, 0) // 0x10E0
#define CANIOT_ATTR_KEY_SYSTEM_LAST_COMMAND_ERROR	  CANIOT_ATTR_KEY(1, 0xF, 0) // 0x10F0
#define CANIOT_ATTR_KEY_SYSTEM_LAST_TELEMETRY_ERROR	  CANIOT_ATTR_KEY(1, 0x10, 0) // 0x1100
#define CANIOT_ATTR_KEY_SYSTEM_UNUSED5				  CANIOT_ATTR_KEY(1, 0x11, 0) // 0x1110
#define CANIOT_ATTR_KEY_SYSTEM_BATTERY				  CANIOT_ATTR_KEY(1, 0x12, 0) // 0x1120

#define CANIOT_ATTR_KEY_CONFIG_TELEMETRY_PERIOD	   CANIOT_ATTR_KEY(2, 0x0, 0) // 0x2000
#define CANIOT_ATTR_KEY_CONFIG_TELEMETRY_DELAY	   CANIOT_ATTR_KEY(2, 0x1, 0) // 0x2010
#define CANIOT_ATTR_KEY_CONFIG_TELEMETRY_DELAY_MIN CANIOT_ATTR_KEY(2, 0x2, 0) // 0x2020
#define CANIOT_ATTR_KEY_CONFIG_TELEMETRY_DELAY_MAX CANIOT_ATTR_KEY(2, 0x3, 0) // 0x2030
#define CANIOT_ATTR_KEY_CONFIG_FLAGS			   CANIOT_ATTR_KEY(2, 0x4, 0) // 0x2040
#define CANIOT_ATTR_KEY_CONFIG_TIMEZONE			   CANIOT_ATTR_KEY(2, 0x5, 0) // 0x2050
#define CANIOT_ATTR_KEY_CONFIG_LOCATION			   CANIOT_ATTR_KEY(2, 0x6, 0) // 0x2060
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_PULSE_DURATION_OC1                              \
	CANIOT_ATTR_KEY(2, 0x7, 0) // 0x2070
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_PULSE_DURATION_OC2                              \
	CANIOT_ATTR_KEY(2, 0x8, 0) // 0x2080
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_PULSE_DURATION_RL1                              \
	CANIOT_ATTR_KEY(2, 0x9, 0) // 0x2090
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_PULSE_DURATION_RL2                              \
	CANIOT_ATTR_KEY(2, 0xA, 0) // 0x20A0
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_OUTPUTS_DEFAULT                                 \
	CANIOT_ATTR_KEY(2, 0xB, 0) // 0x20B0
#define CANIOT_ATTR_KEY_CONFIG_CLS0_GPIO_MASK_TELEMETRY_ON_CHANGE                        \
	CANIOT_ATTR_KEY(2, 0xC, 0) // 0x20C0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PC0                              \
	CANIOT_ATTR_KEY(2, 0xD, 0) // 0x20D0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PC1                              \
	CANIOT_ATTR_KEY(2, 0xE, 0) // 0x20E0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PC2                              \
	CANIOT_ATTR_KEY(2, 0xF, 0) // 0x20F0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PC3                              \
	CANIOT_ATTR_KEY(2, 0x10, 0) // 0x2100
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PD0                              \
	CANIOT_ATTR_KEY(2, 0x11, 0) // 0x2110
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PD1                              \
	CANIOT_ATTR_KEY(2, 0x12, 0) // 0x2120
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PD2                              \
	CANIOT_ATTR_KEY(2, 0x13, 0) // 0x2130
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PD3                              \
	CANIOT_ATTR_KEY(2, 0x14, 0) // 0x2140
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI0                             \
	CANIOT_ATTR_KEY(2, 0x15, 0) // 0x2150
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI1                             \
	CANIOT_ATTR_KEY(2, 0x16, 0) // 0x2160
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI2                             \
	CANIOT_ATTR_KEY(2, 0x17, 0) // 0x2170
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI3                             \
	CANIOT_ATTR_KEY(2, 0x18, 0) // 0x2180
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI4                             \
	CANIOT_ATTR_KEY(2, 0x19, 0) // 0x2190
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI5                             \
	CANIOT_ATTR_KEY(2, 0x1A, 0) // 0x21A0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI6                             \
	CANIOT_ATTR_KEY(2, 0x1B, 0) // 0x21B0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PEI7                             \
	CANIOT_ATTR_KEY(2, 0x1C, 0) // 0x21C0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PB0                              \
	CANIOT_ATTR_KEY(2, 0x1D, 0) // 0x21D0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PE0                              \
	CANIOT_ATTR_KEY(2, 0x1E, 0) // 0x21E0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_PE1                              \
	CANIOT_ATTR_KEY(2, 0x1F, 0) // 0x21F0
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_PULSE_DURATION_RESERVED                         \
	CANIOT_ATTR_KEY(2, 0x20, 0)													// 0x2200
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_DIRECTIONS CANIOT_ATTR_KEY(2, 0x21, 0) // 0x2210
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_OUTPUTS_DEFAULT                                 \
	CANIOT_ATTR_KEY(2, 0x22, 0) // 0x2220
#define CANIOT_ATTR_KEY_CONFIG_CLS1_GPIO_MASK_TELEMETRY_ON_CHANGE                        \
	CANIOT_ATTR_KEY(2, 0x23, 0) // 0x2230

#define CANIOT_ATTR_KEY_DIAG_RESET_COUNT		  CANIOT_ATTR_KEY(3, 0x00, 0) // 0x3000
#define CANIOT_ATTR_KEY_DIAG_LAST_RESET_REASON	  CANIOT_ATTR_KEY(3, 0x01, 0) // 0x3010
#define CANIOT_ATTR_KEY_DIAG_RESET_COUNT_UNKNOWN  CANIOT_ATTR_KEY(3, 0x02, 0) // 0x3020
#define CANIOT_ATTR_KEY_DIAG_RESET_COUNT_POWER_ON CANIOT_ATTR_KEY(3, 0x03, 0) // 0x3030
#define CANIOT_ATTR_KEY_DIAG_RESET_COUNT_WATCHDOG CANIOT_ATTR_KEY(3, 0x04, 0) // 0x3040
#define CANIOT_ATTR_KEY_DIAG_RESET_COUNT_EXTERNAL CANIOT_ATTR_KEY(3, 0x05, 0) // 0x3050
#define CANIOT_ATTR_KEY_DIAG_LAST_RUNTIME_UPTIME  CANIOT_ATTR_KEY(3, 0x06, 0) // 0x3060
#define CANIOT_ATTR_KEY_DIAG_LAST_RUNTIME_UPTIME_TOTAL                                   \
	CANIOT_ATTR_KEY(3, 0x07, 0)													 // 0x3070
#define CANIOT_ATTR_KEY_DIAG_LAST_RESET_STREAK_COUNT CANIOT_ATTR_KEY(3, 0x08, 0) // 0x3080

#define CANIOT_ATTR_KEY_DIAG_BOOT_SIGNAL CANIOT_ATTR_KEY(3, 0x10, 0) // 0x3100

enum caniot_device_section {
	CANIOT_SECTION_DEVICE_IDENTIFICATION = 0,
	CANIOT_SECTION_DEVICE_SYSTEM		 = 1,
	CANIOT_SECTION_DEVICE_CONFIG		 = 2,
	CANIOT_SECTION_DEVICE_DIAG			 = 3,
};

struct caniot_device_attribute {
	char name[CANIOT_ATTR_NAME_MAX_LEN];
	uint16_t key;
	uint8_t read : 1u;
	uint8_t write : 1u;
	uint8_t persistent : 1u;
	enum caniot_device_section section : 2u;
};

/**
 * @brief Retrieve the section of the attribute
 *
 * Example:
 * - caniot_attr_key_get_section(0x3047) gives CANIOT_SECTION_DEVICE_DIAG
 *
 * @param key
 * @return uint16_t
 */
enum caniot_device_section caniot_attr_key_get_section(uint16_t key);

/**
 * @brief Retrieve the attribute root
 *
 * Example:
 * - caniot_attr_key_get_root(0x3047) gives 0x3040
 *
 * @param key
 * @return uint8_t
 */
uint16_t caniot_attr_key_get_root(uint16_t key);

/**
 * @brief Retrieve the attribute part
 *
 * Example:
 * - caniot_attr_key_get_part(0x3047) gives 7
 *
 * @param key
 * @return uint8_t
 */
uint8_t caniot_attr_key_get_part(uint16_t key);

/**
 * @brief Get attribute name by key
 *
 * @param key
 * @return const char*
 */
int caniot_attr_get_by_key(struct caniot_device_attribute *attr, uint16_t key);

/**
 * @brief Get attribute by name
 *
 * @param name
 * @return int32_t positive key or negative error code
 */
int caniot_attr_get_by_name(struct caniot_device_attribute *attr, const char *name);

/**
 * @brief Callback for iterating over attributes
 *
 * @note return false to stop iteration
 */
typedef bool(caniot_device_attribute_handler_t)(struct caniot_device_attribute *attr,
												void *user_data);

/**
 * @brief Iterate over all existing attributes, call handler for each
 *
 * @param handler
 * @param user_data
 * @return int
 */
int caniot_attr_iterate(caniot_device_attribute_handler_t *handler, void *user_data);

/*____________________________________________________________________________*/

#define CANIOT_CONFIG_DEFAULT_INIT()                                                     \
	{                                                                                    \
		.telemetry =                                                                     \
			{                                                                            \
				.period	   = CANIOT_TELEMETRY_PERIOD_DEFAULT_MS,                         \
				.delay_min = CANIOT_TELEMETRY_DELAY_MIN_DEFAULT_MS,                      \
				.delay_max = CANIOT_TELEMETRY_DELAY_MAX_DEFAULT_MS,                      \
			},                                                                           \
		.flags =                                                                         \
			{                                                                            \
				.error_response				= 1u,                                        \
				.telemetry_delay_rdm		= 1u,                                        \
				.telemetry_endpoint			= CANIOT_TELEMETRY_ENDPOINT_DEFAULT,         \
				.telemetry_periodic_enabled = 1u,                                        \
			},                                                                           \
		.timezone = CANIOT_TIMEZONE_DEFAULT,                                             \
		.location =                                                                      \
			{                                                                            \
				.region	 = CANIOT_LOCATION_REGION_DEFAULT,                               \
				.country = CANIOT_LOCATION_COUNTRY_DEFAULT,                              \
			},                                                                           \
		.cls0_gpio = {                                                                   \
			.pulse_durations =                                                           \
				{                                                                        \
					[0] = 0u,                                                            \
					[1] = 0u,                                                            \
					[2] = 0u,                                                            \
					[3] = 0u,                                                            \
				},                                                                       \
			.outputs_default	 = 0u,                                                   \
			.telemetry_on_change = 0xFFFFFFFFlu,                                         \
		},                                                                               \
	}

#define CANIOT_DEVICE_API_FULL_INIT(cmd, tlm, cfgr, cfgw, attr, attw)                    \
	{                                                                                    \
		.config =                                                                        \
			{                                                                            \
				.on_read  = cfgr,                                                        \
				.on_write = cfgw,                                                        \
			},                                                                           \
		.custom_attr =                                                                   \
			{                                                                            \
				.read  = attr,                                                           \
				.write = attw,                                                           \
			},                                                                           \
		.command_handler = cmd, .telemetry_handler = tlm,                                \
	}

#define CANIOT_DEVICE_API_STD_INIT(cmd, tlm, cfgr, cfgw)                                 \
	CANIOT_DEVICE_API_FULL_INIT(cmd, tlm, cfgr, cfgw, NULL, NULL)

#define CANIOT_DEVICE_API_CFG_INIT(cmd, tlm, cfgr, cfgw)                                 \
	CANIOT_DEVICE_API_STD_INIT(cmd, tlm, cfgr, cfgw)

#define CANIOT_DEVICE_API_MIN_INIT(cmd, tlm)                                             \
	CANIOT_DEVICE_API_CFG_INIT(cmd, tlm, NULL, NULL)

#ifdef __cplusplus
}
#endif

#endif /* _CANIOT_DEVICE_H */