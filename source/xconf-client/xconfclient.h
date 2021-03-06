/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef _XCONF_CLIENT_H_
#define _XCONF_CLIENT_H_

#include <curl/curl.h>

#include "telemetry2_0.h"

/**
 * May not be advisable with opensource practices  !!! Get it from dcm.properties
 */
#define DEFAULT_XCONF_URL "https://xconf.xcal.tv/loguploader/getSettings"

#define DEVICE_PROPERTIES                           "/etc/device.properties"
#define TR181_DEVICE_MODEL                          "Device.DeviceInfo.ModelName"
#define TR181_DEVICE_FW_VERSION                     "Device.DeviceInfo.SoftwareVersion"
#define TR181_DEVICE_UPTIME                         "Device.DeviceInfo.UpTime"
#define TR181_DEVICE_WAN_MAC                        "Device.DeviceInfo.X_COMCAST-COM_WAN_MAC"
#define TR181_DEVICE_WAN_IPv4                       "Device.DeviceInfo.X_COMCAST-COM_WAN_IP"
#define TR181_DEVICE_WAN_IPv6                       "Device.DeviceInfo.X_COMCAST-COM_WAN_IPv6"
#define TR181_DEVICE_CM_MAC                         "Device.DeviceInfo.X_COMCAST-COM_CM_MAC"
#define TR181_DEVICE_CM_IP                          "Device.DeviceInfo.X_COMCAST-COM_CM_IP"
#define TR181_DEVICE_PARTNER_ID                     "Device.DeviceInfo.X_RDKCENTRAL-COM_Syndication.PartnerId"
#define TR181_DEVICE_ACCOUNT_ID                     "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AccountInfo.AccountID"
#define TR181_CONFIG_URL                            "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL"

#define MODEL_MAX_LENGTH                            64
#define FW_VERSION_MAX_LENGTH                       128
#define UPTIME_MAX_LENGTH                           64
#define WAN_MAC_MAX_LENGTH                          64
#define CM_MAC_MAX_LENGTH                           64
#define PARTNER_ID_MAX_LENGTH                       64
#define ACCOUNT_ID_MAX_LENGTH                       64
#define BUILD_TYPE_MAX_LENGTH                       64
#define DEVICE_IP_MAX_LENGTH                        64

typedef struct _rdk_utils_params {
    char model[MODEL_MAX_LENGTH];
    char build_type[BUILD_TYPE_MAX_LENGTH];
    char firmware[FW_VERSION_MAX_LENGTH];
    char uptime_secs[UPTIME_MAX_LENGTH];
    char wan_mac[WAN_MAC_MAX_LENGTH];
    char cm_mac[CM_MAC_MAX_LENGTH];
    char cm_ip[DEVICE_IP_MAX_LENGTH];
    char partner_id[PARTNER_ID_MAX_LENGTH];
    char account_id[ACCOUNT_ID_MAX_LENGTH];
    char iface_v6_ip[DEVICE_IP_MAX_LENGTH];
    char iface_v4_ip[DEVICE_IP_MAX_LENGTH];
} rdkParams_struct;

/*
 * Structure to store the xconf response, this is to manage large responses
 * as curl parses large responses in chunks
 */
typedef struct _curlResponseData {
    char *data;
    size_t size;
} curlResponseData;

void uninitXConfClient();

T2ERROR initXConfClient();

T2ERROR startXConfClient();

#endif /* _XCONF_CLIENT_H_ */
