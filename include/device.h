#ifndef _CANIOT_DEVICE_H
#define _CANIOT_DEVICE_H

#include "caniot.h"

#include <stdbool.h>

#define CANIOT_MASK_SELF(cls, dev) CANIOT_ID(0b00, 0b1, 0b111, 0b111, 0b00)
#define CANIOT_FILTER_SELF(cls, dev) CANIOT_ID(0b00, 0b1, cls, dev, 0b00)

#define CANIOT_MASK_BROADCAST(cls, dev) CANIOT_ID(0b00, 0b1, 0b111, 0b111, 0b00)
#define CANIOT_FILTER_BROADCAST(cls, dev) CANIOT_ID(0b00, 0b1, cls, 0b111, 0b00)

struct caniot_identification
{
	union deviceid did;
        uint16_t version;
        char name[32];
	uint32_t magic_number;
};

struct caniot_system
{
        uint32_t uptime;
        uint32_t abstime;
        uint32_t calculated_abstime;
        uint32_t uptime_shift;
        uint32_t last_telemetry;
        struct {
                uint32_t total;
                uint32_t read_attribute;
                uint32_t write_attribute;
                uint32_t command;
                uint32_t request_telemetry;
                uint32_t processed;
                uint32_t query_failed;
        } received;
        struct {
                uint32_t total;
                uint32_t telemetry;
        } sent;
        struct {
                uint32_t total;

        } events;
        int32_t last_query_error;
        int32_t last_telemetry_error;
        int32_t last_event_error;
        int32_t battery;
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
		uint8_t error_response: 1;
		uint8_t telemetry_delay_rdm: 1;
		uint8_t telemetry_endpoint: 2;
	} flags;
};

struct caniot_scheduled
{
        uint8_t days;
        uint8_t time;
        uint32_t command;
};

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
		uint8_t request_telemetry: 1;
	} flags;
};

struct caniot_api
{
	/* called when time is updated with new time */
	int (*update_time)(struct caniot_device *dev,
			   uint32_t ts);

	struct {
		/* called before configuration will be read */
		int (*on_read)(struct caniot_device *dev,
			       struct caniot_config *config);

		/* called after configuration is updated */
		int (*written)(struct caniot_device *dev,
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
	int (*command_handler)(struct caniot_device *dev,
			       uint8_t ep, char *buf,
			       uint8_t len);

			       /* Build telemetry */
	int (*telemetry)(struct caniot_device *dev,
			 uint8_t ep, char *buf,
			 uint8_t *len);
};

void caniot_print_device_identification(const struct caniot_device *dev);

int caniot_device_handle_rx_frame(struct caniot_device *dev,
				  struct caniot_frame *req,
				  struct caniot_frame *resp);

union deviceid caniot_device_get_id(struct caniot_device *dev);

uint32_t caniot_device_telemetry_remaining(struct caniot_device *dev);

/*___________________________________________________________________________*/

/**
 * @brief Receive incoming CANIOT message if any and handle it
 * 
 * @param dev 
 * @return int 
 */
int caniot_device_process(struct caniot_device *dev);

bool caniot_device_is_target(union deviceid did,
			     struct caniot_frame *frame);

int caniot_device_scales_rdmdelay(struct caniot_device *dev,
				  uint32_t *rdmdelay);

/*___________________________________________________________________________*/

/**
 * @brief Verify if device is properly defined
 * 
 * @param dev 
 * @return int 
 */
int caniot_device_verify(struct caniot_device *dev);

#endif /* _CANIOT_DEVICE_H */