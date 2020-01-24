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

#ifndef _PROFILE_H_
#define _PROFILE_H_

#include <stdbool.h>
#include <pthread.h>
#include "cJSON.h"
#include "telemetry2_0.h"
#include "reportprofiles.h"
#include "t2common.h"
#include "collection.h"
#include "vector.h"
#include "reportgen.h"

typedef struct _JSONEncoding
{
    JSONFormat reportFormat;
    TimeStampFormat tsFormat;
}JSONEncoding;

typedef struct _Profile
{
    bool enable;
    bool isUpdated;
    bool reportInProgress;
    bool generateNow;
    bool bClearSeekMap;
    char* hash;
    char* name;
    char* protocol;
    char* encodingType;
    char* Description;
    char* version;
    JSONEncoding *jsonEncoding;
    unsigned int reportingInterval;
    unsigned int activationTimeoutPeriod;
    unsigned int timeRef;
    unsigned int paramNumOfEntries;
    Vector *paramList;
    Vector *staticParamList;
    T2HTTP *t2HTTPDest;
    Vector *eMarkerList;
    Vector *gMarkerList;
    Vector *cachedReportList;
    cJSON *jsonReportObj;
    pthread_t reportThread;
}Profile;

T2ERROR initProfileList();

T2ERROR uninitProfileList();

T2ERROR addProfile(Profile *profile);

int getProfileCount();

T2ERROR profileWithNameExists(const char *profileName, bool *bProfileExists);

T2ERROR enableProfile(const char *profileName);

T2ERROR disableProfile(const char *profileName, bool *isDeleteRequired);

T2ERROR deleteProfile(const char *profileName);

T2ERROR deleteAllProfiles(void);

void updateMarkerComponentMap();

hash_map_t *getProfileHashMap();

#endif /* _PROFILE_H_ */