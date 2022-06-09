#ifndef _CANIOT_DEVICE_H
#define _CANIOT_DEVICE_H

#include <caniot/caniot.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct caniot_identification
{
	caniot_did_t did;
	uint16_t version;
	char name[32];
	uint32_t magic_number;
};

struct caniot_system
{
	uint32_t uptime_synced;
	uint32_t time;
	uint32_t uptime;
	uint32_t start_time;
	uint32_t last_telemetry;
	struct {
		uint32_t total;
		uint32_t read_attribute;
		uint32_t write_attribute;
		uint32_t command;
		uint32_t request_telemetry;
		uint32_t _unused2;
		uint32_t _unused3;
	} received;
	struct {
		uint32_t total;
		uint32_t telemetry;
	} sent;
	uint32_t _unused4;
	int16_t last_command_error;
	int16_t last_telemetry_error;
	int16_t _unused5;
	uint8_t battery;
};

struct caniot_config
{
	struct {
		uint32_t period; /* period in seconds */
		union {
			uint16_t delay_min; /* minimum in milliseconds */
			uint16_t delay;     /* delay in milliseconds */
		};
		uint16_t delay_max; /* maximum in milliseconds */
	} telemetry;

	struct {
		uint8_t error_response : 1;
		uint8_t telemetry_delay_rdm : 1;
		uint8_t telemetry_endpoint : 2;
	} flags;

	int32_t timezone;

	struct {
		char region[2];
		char country[2];
	} location;

	struct {
		struct {
			union {
				uint32_t array[4];
				struct {
					uint32_t oc1;
					uint32_t oc2;
					uint32_t rl1;
					uint32_t rl2;
				};
			} pulse_duration;
			struct {
				union {
					uint32_t mask;
					struct {
						uint8_t open_collectors : 2;
						uint8_t relays : 2;
					};
					struct {
						uint8_t oc1 : 1;
						uint8_t oc2 : 1;
						uint8_t rl1 : 1;
						uint8_t rl2 : 1;
					};
				} outputs_default;

				union {
					uint32_t mask;
					struct {
						uint8_t relays : 2;
						uint8_t open_collectors : 2;
						uint8_t inputs : 4;
					};
					struct {
						uint8_t rl1 : 1;
						uint8_t rl2 : 1;
						uint8_t oc1 : 1;
						uint8_t oc2 : 1;
						uint8_t in1 : 1;
						uint8_t in2 : 1;
						uint8_t in3 : 1;
						uint8_t in4 : 1;
					};
				} telemetry_on_change;
			} mask;
		} gpio;
	} custompcb;
};

// struct caniot_scheduled
// {
//         uint8_t days;
//         uint8_t time;
//         uint32_t command;
// };

struct caniot_device
{
	const struct caniot_identification *identification;
	struct caniot_system system;
	struct caniot_config *config;
	// struct caniot_scheduled scheduled[32];

	const struct caniot_api *api;

#if CANIOT_DRIVERS_API
	const struct caniot_drivers_api *driv;
#endif

	struct {
		uint8_t request_telemetry : 1;
	} flags;
};

typedef int (caniot_telemetry_handler_t)(struct caniot_device *dev,
					 caniot_endpoint_t ep, 
					 char *buf,
					 uint8_t *len);

typedef int (caniot_command_handler_t)(struct caniot_device *dev,
				       caniot_endpoint_t ep, 
				       const char *buf,
				       uint8_t len);

struct caniot_api
{
	struct {
		/* called before configuration will be read */
		int (*on_read)(struct caniot_device *dev,
			       struct caniot_config *config);

		/* called after configuration is updated */
		int (*on_write)(struct caniot_device *dev,
				struct caniot_config *config);
	} config;

	// int (*scheduled_handler)(struct caniot_device *dev, struct caniot_scheduled *sch);

	struct {
		int (*read)(struct caniot_device *dev,
			    uint16_t key,
			    uint32_t *val);
		int (*write)(struct caniot_device *dev,
			     uint16_t key,
			     uint32_t val);
	} custom_attr;

	/* Handle command */
	caniot_command_handler_t *command_handler;

	/* Build telemetry */
	caniot_telemetry_handler_t *telemetry_handler;

};

void caniot_print_device_identification(const struct caniot_device *dev);

int caniot_device_handle_rx_frame(struct caniot_device *dev,
				  const struct caniot_frame *req,
				  struct caniot_frame *resp);

caniot_did_t caniot_device_get_id(struct caniot_device *dev);

uint32_t caniot_device_telemetry_remaining(struct caniot_device *dev);

static inline uint16_t caniot_device_get_mask(void)
{
	return 0x1fc; // 0b00111111100U;
}

uint16_t caniot_device_get_filter(caniot_did_t did);

uint16_t caniot_device_get_filter_broadcast(caniot_did_t did);

/*___________________________________________________________________________*/

void caniot_app_init(struct caniot_device *dev);

/**
 * @brief Receive incoming CANIOT message if any and handle it
 *
 * @param dev
 * @return int
 */
int caniot_device_process(struct caniot_device *dev);

int caniot_device_scales_rdmdelay(struct caniot_device *dev,
				  uint32_t *rdmdelay);

bool caniot_device_time_synced(struct caniot_device *dev);

/*___________________________________________________________________________*/

/**
 * @brief Verify if device is properly defined
 *
 * @param dev
 * @return int
 */
int caniot_device_verify(struct caniot_device *dev);

/*___________________________________________________________________________*/

#define CANIOT_CONFIG_DEFAULT_INIT() \
{ \
	.telemetry = { \
		.period = CANIOT_TELEMETRY_PERIOD_DEFAULT,  \
		.delay_min = CANIOT_TELEMETRY_DELAY_MIN_DEFAULT,  \
		.delay_max = CANIOT_TELEMETRY_DELAY_MAX_DEFAULT,  \
	},  \
	.flags = { \
		.error_response = 1u,  \
		.telemetry_delay_rdm = 1u,  \
		.telemetry_endpoint = CANIOT_TELEMETRY_ENDPOINT_DEFAULT  \
	},  \
	.timezone = CANIOT_TIMEZONE_DEFAULT,  \
	.location = { \
		.region = CANIOT_LOCATION_REGION_DEFAULT,  \
		.country = CANIOT_LOCATION_COUNTRY_DEFAULT,  \
	},  \
	.custompcb = { \
		.gpio = { \
			.pulse_duration = { \
				.rl1 = 0U, \
				.rl2 = 0U, \
				.oc1 = 0U, \
				.oc2 = 0U, \
			}, \
			.mask = { \
				.outputs_default.mask = 0U, \
				.telemetry_on_change.mask = 0xFFFFFFFFLU, \
			} \
		} \
	}\
}

#define CANIOT_API_FULL_INIT(cmd, tlm, cfgr, cfgw, attr, attw) \
{ \
	.config = { \
		.on_read = cfgr,  \
		.on_write = cfgw  \
	},  \
	.custom_attr = { \
		.read = attr,  \
		.write = attw  \
	},  \
	.command_handler = cmd,  \
	.telemetry_handler = tlm  \
}

#define CANIOT_API_STD_INIT(cmd, tlm, cfgr, cfgw) \
	CANIOT_API_FULL_INIT(cmd, tlm, cfgr, cfgw, NULL, NULL)

#define CANIOT_API_CFG_INIT(cmd, tlm, cfgr, cfgw) \
	CANIOT_API_STD_INIT(cmd, tlm, cfgr, cfgw)

#define CANIOT_API_MIN_INIT(cmd, tlm) \
	CANIOT_API_CFG_INIT(cmd, tlm, NULL, NULL)

#define TELEMTRY_MASK

#ifdef __cplusplus
}
#endif

#endif /* _CANIOT_DEVICE_H */