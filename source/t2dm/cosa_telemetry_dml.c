/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

/**********************************************************************
 Copyright [2014] [Cisco Systems, Inc.]
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 **********************************************************************/

/**************************************************************************

 module: cosa_telemetry_dml.h

 For COSA Data Model Library Development

 -------------------------------------------------------------------

 description:

 This file defines the apis for objects to support Data Model Library.

 -------------------------------------------------------------------


 author:

 -------------------------------------------------------------------

 revision:

 11/18/2019    initial revision.

 **************************************************************************/

#include "cosa_telemetry_dml.h"
#include "telemetry2_0.h"
#include "t2log_wrapper.h"
#include "datamodel.h"
#include "ansc_platform.h"

/***********************************************************************

 APIs for Object:

 Telemetry.AddReportProfile

 *  AddProfile_GetParamStringValue
 *  AddProfile_SetParamStringValue

 ***********************************************************************/

ULONG Telemetry_GetParamStringValue(ANSC_HANDLE hInsContext, char* ParamName, char* pValue, ULONG* pUlSize)
{
    PCOSA_DATAMODEL_TELEMETRY pMyObject = (PCOSA_DATAMODEL_TELEMETRY) g_pCosaBEManager->hTelemetry;

    if (strcmp(ParamName, "ReportProfiles") == 0)
    {
        if (pMyObject->JsonBlob == NULL)
            return 0;

        if(*pUlSize < strlen(pMyObject->JsonBlob))
        {
            *pUlSize = strlen(pMyObject->JsonBlob);
            return 1;
        }

        AnscCopyString(pValue, pMyObject->JsonBlob);
        *pUlSize = strlen(pMyObject->JsonBlob);
        return 0;
    }

    if (strcmp(ParamName, "ReportProfilesMsgPack") == 0)
    {
        if (pMyObject->MsgpackBlob == NULL)
            return 0;

        if(*pUlSize < strlen(pMyObject->MsgpackBlob))
        {
            *pUlSize = strlen(pMyObject->MsgpackBlob);
            return 1;
        }

        AnscCopyString(pValue, pMyObject->MsgpackBlob);
        *pUlSize = strlen(pMyObject->MsgpackBlob);
        return 0;
    }
   
    return -1;
}

BOOL Telemetry_SetParamStringValue(ANSC_HANDLE hInsContext, char* ParamName, char* pString)
{
    PCOSA_DATAMODEL_TELEMETRY pMyObject = (PCOSA_DATAMODEL_TELEMETRY) g_pCosaBEManager->hTelemetry;
    if (strcmp(ParamName, "ReportProfiles") == 0)
    {
        if(T2ERROR_SUCCESS != datamodel_processProfile(pString))
        {
            return FALSE;
        }

	if (pMyObject->JsonBlob != NULL)
        {
            AnscFreeMemory((ANSC_HANDLE) (pMyObject->JsonBlob));
            pMyObject->JsonBlob = NULL;
        }

        pMyObject->JsonBlob = (char *)AnscAllocateMemory( AnscSizeOfString(pString) + 1 );
        strncpy(pMyObject->JsonBlob, pString, strlen(pString));
        pMyObject->JsonBlob[strlen(pString)] = '\0';

        return TRUE;
    }

    if (strcmp(ParamName, "ReportProfilesMsgPack") == 0)
    {
        char *webConfigString  = NULL;
        ULONG stringSize = 0;
	if(pString != NULL)
	{
 		webConfigString = AnscBase64Decode(pString, &stringSize);
	}
        if(T2ERROR_SUCCESS != datamodel_MsgpackProcessProfile(webConfigString,stringSize))
        {
        	return FALSE;
        }

        if (pMyObject->MsgpackBlob != NULL)
        {
            AnscFreeMemory((ANSC_HANDLE) (pMyObject->MsgpackBlob));
            pMyObject->MsgpackBlob = NULL;
        }

        pMyObject->MsgpackBlob = (char *)AnscAllocateMemory( AnscSizeOfString(pString) + 1 );
        strncpy(pMyObject->MsgpackBlob, pString, strlen(pString));
        pMyObject->MsgpackBlob[strlen(pString)] = '\0';
	
        return TRUE;
    }

    return FALSE;
}
