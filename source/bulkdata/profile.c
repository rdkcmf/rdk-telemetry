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

#include <stdbool.h>
#include <malloc.h>
#include <string.h>

#include "profile.h"
#include "reportprofiles.h"
#include "t2eventreceiver.h"
#include "t2markers.h"
#include "t2log_wrapper.h"
#include "ccspinterface.h"
#include "curlinterface.h"
#include "scheduler.h"
#include "persistence.h"
#include "vector.h"
#include "dcautil.h"

static bool initialized = false;
static Vector *profileList;
static pthread_mutex_t plMutex;
static pthread_mutex_t reportLock;

static void freeRequestURIparam(void *data)
{
    if(data != NULL)
    {
        HTTPReqParam *hparam = (HTTPReqParam *)data;
        if(hparam->HttpName)
            free(hparam->HttpName);
        if(hparam->HttpRef)
            free(hparam->HttpRef);
        if(hparam->HttpValue)
            free(hparam->HttpValue);
        free(hparam);
        hparam = NULL;
    }
}

static void freeReportProfileConfig(void *data)
{
    if(data != NULL)
    {
        Config *config = (Config *)data;

        if(config->name)
            free(config->name);
        if(config->configData)
            free(config->configData);

        free(config);
    }
}

static void freeProfile(void *data)
{
    T2Debug("%s ++in \n", __FUNCTION__);
    if(data != NULL)
    {
        Profile *profile = (Profile *)data;
        if(profile->name)
            free(profile->name);
        if(profile->hash)
            free(profile->hash);
        if(profile->protocol)
            free(profile->protocol);
        if(profile->encodingType)
            free(profile->encodingType);
        if(profile->Description)
             free(profile->Description);
        if(profile->version)
             free(profile->version);
        if(profile->jsonEncoding)
            free(profile->jsonEncoding);
        if(profile->t2HTTPDest)
        {
            free(profile->t2HTTPDest->URL);
            if(profile->t2HTTPDest->RequestURIparamList) {
                Vector_Destroy(profile->t2HTTPDest->RequestURIparamList, freeRequestURIparam);
            }
            free(profile->t2HTTPDest);
        }
        if(profile->eMarkerList)
        {
            Vector_Destroy(profile->eMarkerList, freeEMarker);
        }
        if(profile->gMarkerList)
        {
            Vector_Destroy(profile->gMarkerList, freeGMarker);
        }
        if(profile->paramList)
        {
            Vector_Destroy(profile->paramList, freeParam);
        }
        if (profile->staticParamList)
        {
            Vector_Destroy(profile->staticParamList, freeStaticParam);
        }
        free(profile);
        profile = NULL;
    }
    T2Debug("%s ++out \n", __FUNCTION__);
}

static T2ERROR getProfile(const char *profileName, Profile **profile)
{
    int profileIndex = 0;
    Profile *tempProfile = NULL;
    T2Debug("%s ++in\n", __FUNCTION__);
    if(profileName == NULL)
    {
        T2Error("profileName is null\n");
        return T2ERROR_FAILURE;
    }
    for(; profileIndex < Vector_Size(profileList); profileIndex++)
    {
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        if(strcmp(tempProfile->name, profileName) == 0)
        {
            *profile = tempProfile;
            T2Debug("%s --out\n", __FUNCTION__);
            return T2ERROR_SUCCESS;
        }
    }
    T2Error("Profile with Name : %s not found\n", profileName);
    return T2ERROR_PROFILE_NOT_FOUND;
}

static T2ERROR initJSONReportProfile(cJSON** jsonObj, cJSON **valArray)
{
    *jsonObj = cJSON_CreateObject();
    if(*jsonObj == NULL)
    {
        T2Error("Failed to create cJSON object\n");
        return T2ERROR_FAILURE;
    }

    cJSON_AddItemToObject(*jsonObj, "Report", *valArray = cJSON_CreateArray());

    cJSON *arrayItem = NULL;

    return T2ERROR_SUCCESS;
}

T2ERROR profileWithNameExists(const char *profileName, bool *bProfileExists)
{
    int profileIndex = 0;
    Profile *tempProfile = NULL;
    T2Debug("%s ++in\n", __FUNCTION__);
    if(!initialized)
    {
        T2Error("profile list is not initialized yet, ignoring\n");
        return T2ERROR_FAILURE;
    }
    if(profileName == NULL)
    {
        T2Error("profileName is null\n");
        *bProfileExists = false;
        return T2ERROR_FAILURE;
    }
    pthread_mutex_lock(&plMutex);
    for(; profileIndex < Vector_Size(profileList); profileIndex++)
    {
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        if(strcmp(tempProfile->name, profileName) == 0)
        {
            *bProfileExists = true;
            pthread_mutex_unlock(&plMutex);
            return T2ERROR_SUCCESS;
        }
    }
    *bProfileExists = false;
    pthread_mutex_unlock(&plMutex);
    T2Error("Profile with Name : %s not found\n", profileName);
    return T2ERROR_PROFILE_NOT_FOUND;
}

static void CollectAndReport(void* data)
{
    if(data == NULL)
    {
        T2Error("data passed is NULL can't identify the profile, existing from CollectAndReport\n");
        return;
    }
    Profile* profile = (Profile *)data;
    profile->reportInProgress = true;

    Vector *profileParamVals = NULL;
    Vector *grepResultList = NULL;
    cJSON *valArray = NULL;
    char* jsonReport = NULL;

    struct timespec startTime;
    struct timespec endTime;
    struct timespec elapsedTime;

    T2ERROR ret = T2ERROR_FAILURE;
    T2Info("%s ++in profileName : %s\n", __FUNCTION__, profile->name);


    clock_gettime(CLOCK_REALTIME, &startTime);
    if(!strcmp(profile->encodingType, "JSON") || !strcmp(profile->encodingType, "MessagePack"))
    {
        JSONEncoding *jsonEncoding = profile->jsonEncoding;
        if (jsonEncoding->reportFormat != JSONRF_KEYVALUEPAIR)
        {
            //TODO: Support 'ObjectHierarchy' format in RDKB-26154.
            T2Error("Only JSON name-value pair format is supported \n");
            profile->reportInProgress = false;
            return;
        }

        if(T2ERROR_SUCCESS != initJSONReportProfile(&profile->jsonReportObj, &valArray))
        {
            T2Error("Failed to initialize JSON Report\n");
            profile->reportInProgress = false;
            return;
        }
        else
        {
            if(Vector_Size(profile->staticParamList) > 0)
            {
                T2Debug(" Adding static Parameter Values to Json report\n");
                encodeStaticParamsInJSON(valArray, profile->staticParamList);
            }
            if(Vector_Size(profile->paramList) > 0)
            {
                T2Debug("Fetching TR-181 Object/Parameter Values\n");
                profileParamVals = getProfileParameterValues(profile->paramList);
                if(profileParamVals != NULL)
                {
                    encodeParamResultInJSON(valArray, profile->paramList, profileParamVals);
                }
                Vector_Destroy(profileParamVals, freeProfileValues);
            }
            if(Vector_Size(profile->gMarkerList) > 0)
            {
                getGrepResults(profile->name, profile->gMarkerList, &grepResultList, profile->bClearSeekMap);
                encodeGrepResultInJSON(valArray, grepResultList);
            }
            if(Vector_Size(profile->eMarkerList) > 0)
            {
                encodeEventMarkersInJSON(valArray, profile->eMarkerList);
            }
            ret = prepareJSONReport(profile->jsonReportObj, &jsonReport);
            destroyJSONReport(profile->jsonReportObj);
            profile->jsonReportObj = NULL;

            if(ret != T2ERROR_SUCCESS)
            {
                T2Error("Unable to generate report for : %s\n", profile->name);
                profile->reportInProgress = false;
                return;
            }
            long size = strlen(jsonReport);
            T2Info("cJSON Report = %s\n", jsonReport);
            T2Info("Report Size = %ld\n", size);
            if(size > DEFAULT_MAX_REPORT_SIZE) {
                T2Warning("Report size is exceeding the max limit : %ld\n", DEFAULT_MAX_REPORT_SIZE);
            }
            if(strcmp(profile->protocol, "HTTP") == 0) {
                char *httpUrl = prepareHttpUrl(profile->t2HTTPDest); /* Append URL with http properties */
                ret = sendReportOverHTTP(httpUrl, jsonReport);

                if(ret == T2ERROR_FAILURE) {
                    if(Vector_Size(profile->cachedReportList) == MAX_CACHED_REPORTS) {
                        T2Debug("Max Cached Reports Limit Reached, Overwriting third recent report\n");
                        char *thirdCachedReport = (char *) Vector_At(profile->cachedReportList, MAX_CACHED_REPORTS - 3);
                        Vector_RemoveItem(profile->cachedReportList, thirdCachedReport, NULL);
                        free(thirdCachedReport);
                    }
                    Vector_PushBack(profile->cachedReportList, jsonReport);

                    T2Info("Report Cached, No. of reportes cached = %d\n", Vector_Size(profile->cachedReportList));
                }else if(Vector_Size(profile->cachedReportList) > 0) {
                    T2Info("Trying to send  %d cached reports\n", Vector_Size(profile->cachedReportList));
                    ret = sendCachedReportsOverHTTP(httpUrl, profile->cachedReportList);
                }
                free(httpUrl);
                httpUrl = NULL;
            }
            else
            {
                T2Error("Unsupported report send protocol : %s\n", profile->protocol);
            }
        }
    }
    else
    {
        T2Error("Unsupported encoding format : %s\n", profile->encodingType);
    }
    clock_gettime(CLOCK_REALTIME, &endTime);
    getElapsedTime(&elapsedTime, &endTime, &startTime);
    T2Info("Elapsed Time for : %s = %lu.%lu (Sec.NanoSec)\n", profile->name, elapsedTime.tv_sec, elapsedTime.tv_nsec);
    if(ret == T2ERROR_SUCCESS && jsonReport)
    {
        free(jsonReport);
        jsonReport = NULL;
    }

    profile->reportInProgress = false;
    T2Info("%s --out\n", __FUNCTION__);
}

void NotifyTimeout(const char* profileName, bool isClearSeekMap)
{
    T2Debug("%s ++in\n", __FUNCTION__);
    pthread_mutex_lock(&plMutex);

    Profile *profile = NULL;
    if(T2ERROR_SUCCESS != getProfile(profileName, &profile))
    {
        T2Error("Profile : %s not found\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }

    pthread_mutex_unlock(&plMutex);

    T2Info("%s: profile %s is in %s state\n", __FUNCTION__, profileName, profile->enable ? "Enabled" : "Disabled");
    if(profile->enable && !profile->reportInProgress)
    {
        /* To avoid previous report thread to go into zombie state, mark it detached. */
        if (profile->reportThread)
            pthread_detach(profile->reportThread);

        profile->bClearSeekMap = isClearSeekMap;
        pthread_create(&profile->reportThread, NULL, CollectAndReport, (void*)profile);
    }
    else
    {
        T2Warning("Either profile is disabled or report generation still in progress - ignoring the request\n");
    }

    T2Debug("%s --out\n", __FUNCTION__);
}


T2ERROR Profile_storeMarkerEvent(const char *profileName, T2Event *eventInfo)
{
    T2Debug("%s ++in\n", __FUNCTION__);

    pthread_mutex_lock(&plMutex);
    Profile *profile = NULL;
    if(T2ERROR_SUCCESS != getProfile(profileName, &profile))
    {
        T2Error("Profile : %s not found\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }
    pthread_mutex_unlock(&plMutex);
    if(!profile->enable)
    {
        T2Warning("Profile : %s is disabled, ignoring the event\n", profileName);
        return T2ERROR_FAILURE;
    }
    int eventIndex = 0;
    EventMarker *lookupEvent = NULL;
    for(; eventIndex < Vector_Size(profile->eMarkerList); eventIndex++)
    {
        EventMarker *tempEventMarker = (EventMarker *)Vector_At(profile->eMarkerList, eventIndex);
        if(!strcmp(tempEventMarker->markerName, eventInfo->name))
        {
            lookupEvent = tempEventMarker;
            break;
        }
    }
    if(lookupEvent != NULL)
    {
        switch(lookupEvent->mType)
        {
            case MTYPE_COUNTER:
                lookupEvent->u.count++;
                T2Debug("Increment marker count to : %d\n", lookupEvent->u.count);
                break;

            case MTYPE_ABSOLUTE:
            default:
                if(lookupEvent->u.markerValue)
                    free(lookupEvent->u.markerValue);
                lookupEvent->u.markerValue = strdup(eventInfo->value);
                T2Debug("New marker value saved : %s\n", lookupEvent->u.markerValue);
                break;
        }
    }
    else
    {
        T2Error("Event name : %s value : %s\n", eventInfo->name, eventInfo->value);
        T2Error("Event doens't match any marker information, shouldn't come here\n");
        return T2ERROR_FAILURE;
    }

    T2Debug("%s --out\n", __FUNCTION__);
    return T2ERROR_SUCCESS;
}

T2ERROR addProfile(Profile *profile)
{
    T2Debug("%s ++in\n", __FUNCTION__);
    if(!initialized)
    {
        T2Error("profile list is not initialized yet, ignoring\n");
        return T2ERROR_FAILURE;
    }
    pthread_mutex_lock(&plMutex);
    if(Vector_Size(profileList) == MAX_BULKDATA_PROFILES)
    {
        T2Error("Max profile count reached, can't create/add another profile\n");
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }

    Vector_PushBack(profileList, profile);

#ifdef _COSA_INTEL_XB3_ARM_
    if (Vector_Size(profile->gMarkerList) > 0)
        saveGrepConfig(profile->name, profile->gMarkerList) ;
#endif
    pthread_mutex_unlock(&plMutex);

    T2Debug("%s --out\n", __FUNCTION__);
    return T2ERROR_SUCCESS;
}

T2ERROR enableProfile(const char *profileName)
{
    T2Debug("%s ++in \n", __FUNCTION__);
    if(!initialized)
    {
        T2Error("profile list is not initialized yet, ignoring\n");
        return T2ERROR_FAILURE;
    }
    pthread_mutex_lock(&plMutex);
    Profile *profile = NULL;
    if(T2ERROR_SUCCESS != getProfile(profileName, &profile))
    {
        T2Error("Profile : %s not found\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }
    if(profile->enable)
    {
        T2Info("Profile : %s is already enabled - ignoring duplicate request\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_SUCCESS;
    }
    else
    {
        profile->enable = true;

        int emIndex = 0;
        EventMarker *eMarker = NULL;
        for(;emIndex < Vector_Size(profile->eMarkerList); emIndex++)
        {
            eMarker = (EventMarker *)Vector_At(profile->eMarkerList, emIndex);
            addT2EventMarker(eMarker->markerName, eMarker->compName, profile->name, eMarker->skipFreq);
        }
        if(registerProfileWithScheduler(profile->name, profile->reportingInterval, profile->activationTimeoutPeriod, true) != T2ERROR_SUCCESS)
        {
            profile->enable = false;
            T2Error("Unable to register profile : %s with Scheduler\n", profileName);
            pthread_mutex_unlock(&plMutex);
            return T2ERROR_FAILURE;
        }

        T2ER_StartDispatchThread();

        T2Info("Successfully enabled profile : %s\n", profileName);
    }
    T2Debug("%s --out\n", __FUNCTION__);
    pthread_mutex_unlock(&plMutex);
    return T2ERROR_SUCCESS;
}


void updateMarkerComponentMap()
{
    T2Debug("%s ++in\n", __FUNCTION__);

    int profileIndex = 0;
    Profile *tempProfile = NULL;

    for(; profileIndex < Vector_Size(profileList); profileIndex++)
    {
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        if(tempProfile->enable)
        {
            int emIndex = 0;
            EventMarker *eMarker = NULL;
            for(;emIndex < Vector_Size(tempProfile->eMarkerList); emIndex++)
            {
                eMarker = (EventMarker *)Vector_At(tempProfile->eMarkerList, emIndex);
                addT2EventMarker(eMarker->markerName, eMarker->compName, tempProfile->name, eMarker->skipFreq);
            }
        }
    }
    T2Debug("%s --out\n", __FUNCTION__);
}

T2ERROR disableProfile(const char *profileName, bool *isDeleteRequired) {
    T2Debug("%s ++in \n", __FUNCTION__);

    if(!initialized)
    {
        T2Error("profile list is not initialized yet, ignoring\n");
        return T2ERROR_FAILURE;
    }

    pthread_mutex_lock(&plMutex);
    Profile *profile = NULL;
    if(T2ERROR_SUCCESS != getProfile(profileName, &profile))
    {
        T2Error("Profile : %s not found\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }

    if (profile->generateNow) {
        *isDeleteRequired = true;
    } else {
        profile->enable = false;
    }
    pthread_mutex_unlock(&plMutex);

    T2Debug("%s --out\n", __FUNCTION__);

    return T2ERROR_SUCCESS;
}

T2ERROR deleteAllProfiles(void) {
    T2Debug("%s ++in\n", __FUNCTION__);

    int count = 0;
    int profileIndex = 0;
    Profile *tempProfile = NULL;

    if(profileList == NULL)
    {
        T2Error("profile list is not initialized yet or profileList is empty, ignoring\n");
        return T2ERROR_FAILURE;
    }

    pthread_mutex_lock(&plMutex);
    count = Vector_Size(profileList);
    pthread_mutex_unlock(&plMutex);

    for(; profileIndex < count; profileIndex++)
    {
        pthread_mutex_lock(&plMutex);
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        tempProfile->enable = false;
        pthread_mutex_unlock(&plMutex);

	if(T2ERROR_SUCCESS != unregisterProfileFromScheduler(tempProfile->name))
        {
            T2Error("Profile : %s failed to  unregister from scheduler\n", tempProfile->name);
        }

        pthread_mutex_lock(&plMutex);
        if (tempProfile->reportThread)
            pthread_join(tempProfile->reportThread, NULL);

        if (Vector_Size(tempProfile->gMarkerList) > 0)
            removeGrepConfig(tempProfile->name);
        pthread_mutex_unlock(&plMutex);
    }

    pthread_mutex_lock(&plMutex);
    T2Debug("Deleting all profiles from the profileList\n");
    Vector_Destroy(profileList, freeProfile);
    profileList = NULL;
    Vector_Create(&profileList);
    pthread_mutex_unlock(&plMutex);

    clearPersistenceFolder(REPORTPROFILES_PERSISTENCE_PATH);

    T2Debug("%s --out\n", __FUNCTION__);

    return T2ERROR_SUCCESS;
}

T2ERROR deleteProfile(const char *profileName)
{
    T2Debug("%s ++in\n", __FUNCTION__);
    if(!initialized)
    {
        T2Error("profile list is not initialized yet, ignoring\n");
        return T2ERROR_FAILURE;
    }

    Profile *profile = NULL;
    pthread_mutex_lock(&plMutex);
    if(T2ERROR_SUCCESS != getProfile(profileName, &profile))
    {
        T2Error("Profile : %s not found\n", profileName);
        pthread_mutex_unlock(&plMutex);
        return T2ERROR_FAILURE;
    }
    pthread_mutex_unlock(&plMutex);

    if(profile->enable)
        profile->enable = false;
    else
    {
        T2Error("Profile is disabled, ignoring the delete profile request\n");
        return T2ERROR_SUCCESS;
    }

    if(T2ERROR_SUCCESS != unregisterProfileFromScheduler(profileName))
    {
        T2Info("Profile : %s already removed from scheduler\n", profileName);
    }

    T2Info("Waiting for CollectAndReport to be complete : %s\n", profileName);
    if (profile->reportThread)
        pthread_join(profile->reportThread, NULL);

    pthread_mutex_lock(&plMutex);

    if (Vector_Size(profile->gMarkerList) > 0)
        removeGrepConfig(profileName);

    T2Info("removing profile : %s from profile list\n", profile->name);
    Vector_RemoveItem(profileList, profile, freeProfile);

    pthread_mutex_unlock(&plMutex);
    T2Debug("%s --out\n", __FUNCTION__);
    return T2ERROR_SUCCESS;
}

void sendLogUploadInterruptToScheduler()
{
    int profileIndex = 0;
    Profile *tempProfile = NULL;
    T2Debug("%s ++in\n", __FUNCTION__);

    pthread_mutex_lock(&plMutex);
    for(; profileIndex < Vector_Size(profileList); profileIndex++)
    {
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        if (Vector_Size(tempProfile->gMarkerList) > 0)
        {
            SendInterruptToTimeoutThread(tempProfile->name);
        }
    }
    pthread_mutex_unlock(&plMutex);
    T2Debug("%s --out\n", __FUNCTION__);
}

static void loadReportProfilesFromDisk()
{
    int configIndex = 0;
    Vector *configList = NULL;
    Config *config = NULL;
    T2Debug("%s ++in\n", __FUNCTION__);

    Vector_Create(&configList);
    fetchLocalConfigs(REPORTPROFILES_PERSISTENCE_PATH, configList);

     for(; configIndex < Vector_Size(configList); configIndex++)
     {
         config = Vector_At(configList, configIndex);
         Profile *profile = 0;
         T2Debug("Processing config with name : %s\n", config->name);
         T2Debug("Config Size = %d\n", strlen(config->configData));
         if(T2ERROR_SUCCESS == processConfiguration(&config->configData, config->name, NULL, &profile))
         {
             if(T2ERROR_SUCCESS == addProfile(profile))
             {
                 T2Info("Successfully created/added new profile : %s\n", profile->name);
                 if(T2ERROR_SUCCESS != enableProfile(profile->name))
                 {
                     T2Error("Failed to enable profile name : %s\n", profile->name);
                 }
             }
             else
             {
                 T2Error("Unable to create and add new profile for name : %s\n", config->name);
             }
         }
     }
     Vector_Destroy(configList, freeReportProfileConfig);
     T2Info("Completed processing %d profiles on the disk,trying to fetch new/updated profiles\n", Vector_Size(configList));

    T2Debug("%s --out\n", __FUNCTION__);
}

T2ERROR initProfileList()
{
    T2Debug("%s ++in\n", __FUNCTION__);
    if(initialized)
    {
        T2Info("profile list is already initialized\n");
        return T2ERROR_SUCCESS;
    }
    initialized = true;
    pthread_mutex_init(&plMutex, NULL);
    pthread_mutex_init(&reportLock, NULL);

    Vector_Create(&profileList);

    loadReportProfilesFromDisk();

    T2Debug("%s --out\n", __FUNCTION__);
    return T2ERROR_SUCCESS;
}

int getProfileCount()
{
    int count = 0;
    T2Debug("%s ++in\n", __FUNCTION__);
    if(!initialized)
    {
        T2Info("profile list isn't initialized\n");
        return count;
    }
    pthread_mutex_lock(&plMutex);
    count = Vector_Size(profileList);
    pthread_mutex_unlock(&plMutex);

    T2Debug("%s --out\n", __FUNCTION__);
    return count;
}

hash_map_t *getProfileHashMap()
{
    int profileIndex = 0;
    hash_map_t *profileHashMap = NULL;
    Profile *tempProfile = NULL;
    T2Debug("%s ++in\n", __FUNCTION__);

    pthread_mutex_lock(&plMutex);
    profileHashMap = hash_map_create();
    for(; profileIndex < Vector_Size(profileList); profileIndex++)
    {
        tempProfile = (Profile *)Vector_At(profileList, profileIndex);
        char *profileName = strdup(tempProfile->name);
        char *profileHash = strdup(tempProfile->hash);
        hash_map_put(profileHashMap, profileName, profileHash);
    }
    pthread_mutex_unlock(&plMutex);

    T2Debug("%s --out\n", __FUNCTION__);
    return profileHashMap;
}

T2ERROR uninitProfileList()
{
    T2Debug("%s ++in\n", __FUNCTION__);

    if(!initialized)
    {
        T2Info("profile list is not initialized yet, ignoring\n");
        return T2ERROR_SUCCESS;
    }

    initialized = false;
    deleteAllProfiles();

    pthread_mutex_destroy(&reportLock);
    pthread_mutex_destroy(&plMutex);

    T2Debug("%s --out\n", __FUNCTION__);
    return T2ERROR_SUCCESS;
}