#!/bin/sh

####################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
####################################################################################

#masking dmcli usage when rbus service is not running
T2_MSG_CLIENT=/usr/bin/telemetry2_0_client
if [ -e /nvram/rbus_support ]; then
    rbus_alive=`ps | grep /usr/bin/rtrouted | grep -vc grep`
    if [ "$rbus_alive" -eq "1" ];then
        IS_T2_ENABLED=`dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Enable | grep value | awk '{print $5}'`
    else
        IS_T2_ENABLED=false
    fi
else
    IS_T2_ENABLED=`dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Enable | grep value | awk '{print $5}'`
fi

t2CountNotify() {
    if [ "$IS_T2_ENABLED" == "true" ]; then
        marker=$1
        $T2_MSG_CLIENT  "$marker" "1"
    fi
}

t2ValNotify() {
    if [ "$IS_T2_ENABLED" == "true" ]; then
        marker=$1
        shift
        $T2_MSG_CLIENT "$marker" "$*"
    fi    
}
