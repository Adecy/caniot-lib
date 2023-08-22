/*
 * Copyright (c) 2023 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CANIOT_CONFIG_H_
#define CANIOT_CONFIG_H_

#ifndef CONFIG_CANIOT_CHECKS
#define CONFIG_CANIOT_CHECKS 0
#endif

// support for legacy CANIOT_DEVICE_DRIVERS_API macro
#ifdef CONFIG_CANIOT_DRIVERS_API
#define CONFIG_CANIOT_DEVICE_DRIVERS_API CONFIG_CANIOT_DRIVERS_API
#endif

#ifndef DCONFIG_CANIOT_DEVICE_DRIVERS_API
#define DCONFIG_CANIOT_DEVICE_DRIVERS_API 0
#endif

#ifndef CONFIG_CANIOT_CTRL_DRIVERS_API
#define CONFIG_CANIOT_CTRL_DRIVERS_API 0
#endif

#ifndef CONFIG_CANIOT_DEBUG
#define CONFIG_CANIOT_DEBUG 0
#endif

#ifndef CONFIG_CANIOT_MAX_PENDING_QUERIES
#define CONFIG_CANIOT_MAX_PENDING_QUERIES 4U
#endif

#ifndef CONFIG_CANIOT_ATTRIBUTE_NAME
#define CONFIG_CANIOT_ATTRIBUTE_NAME 0u
#endif

#ifndef CONFIG_CANIOT_CONTROLLER_DISCOVERY
#define CONFIG_CANIOT_CONTROLLER_DISCOVERY 1u
#endif

#ifndef CONFIG_CANIOT_FRAME_TIMESTAMP
#define CONFIG_CANIOT_FRAME_TIMESTAMP 0u
#endif

#ifndef CONFIG_CANIOT_ASSERT
#define CONFIG_CANIOT_ASSERT 0
#endif

#ifndef CONFIG_CANIOT_QUERY_ID
#define CONFIG_CANIOT_QUERY_ID 0u
#endif

#define CANIOT_ATTR_NAME_MAX_LEN 48u

#endif /* CANIOT_CONFIG_H_ */