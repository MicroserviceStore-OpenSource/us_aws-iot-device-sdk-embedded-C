
/* Enable all the log levels */
#define LOG_INFO_ENABLED        1
#define LOG_WARNING_ENABLED     1
#define LOG_ERROR_ENABLED       1
#include "SysCall.h"

#include "us-AWSIoT.h"
#include "VendorRPCustomTags.h"

#ifndef RP_CUSTOM_ITEM_AWSIOT_DEVCERT
#error "RP_CUSTOM_ITEM_AWSIOT_DEVCERT is not defined."
#endif

#ifndef RP_CUSTOM_ITEM_AWSIOT_PKEY
#error "RP_CUSTOM_ITEM_AWSIOT_PKEY is not defined."
#endif

#ifndef AWS_CONNECT_TIMEOUT
#define AWS_CONNECT_TIMEOUT         50000
#endif

#ifndef AWS_PUBLISH_TIMEOUT
#define AWS_PUBLISH_TIMEOUT         10000
#endif

#define CHECK_US_ERR(_sysStatus, _usStatus) \
                if (_sysStatus != SysStatus_Success || _usStatus != usStatus_Success) \
                { LOG_PRINTF(" > us Test Failed. Line %d. Sys Status %d | us Status %d. Exiting the User Application...", __LINE__, _sysStatus, _usStatus); Sys_Exit(); }

#error "Please define the following macros. Embed values in the app or ideally get from the Root Params"
#define AWS_IOT_URL                 ""
#define AWS_IOT_THINGNAME           ""
#define AWS_IOT_TOPIC_TEMP          ""

int main(void)
{
    SysStatus retVal;

    AWSIoTContext awsIoTCtx;
    usStatus status;

    LOG_PRINTF(" > Container : Microservice Test User App");

    SYS_INITIALISE_IPC_MESSAGEBOX(retVal, 4);

    us_AWSIoT_Initialise();

    retVal = us_AWSIoT_Connect(AWS_IOT_URL, AWS_IOT_THINGNAME, RP_CUSTOM_ITEM_AWSIOT_DEVCERT, RP_CUSTOM_ITEM_AWSIOT_PKEY, AWS_CONNECT_TIMEOUT, &awsIoTCtx, &status);
    CHECK_US_ERR(retVal, status);
    
    LOG_PRINTF(" > Connected to AWS IoT Core. Starting to publish temparature periodically...");

    for (int i = 0; ; i++)
    {
        char* payload;

        if (i % 2)
        {
            payload = "{\"Temp\":27}";
        }
        else
        {
            payload = "{\"Temp\":28}";
        }

        retVal = us_AWSIoT_PublishToTopic(&awsIoTCtx, AWS_IOT_TOPIC_TEMP, payload, AWS_PUBLISH_TIMEOUT, &status);
        CHECK_US_ERR(retVal, status);
        LOG_PRINTF(" > Temparature Published");

        Sys_Sleep(10000);
    }

    LOG_PRINTF(" > Exiting the User Application");
    /* Exit the Container */
    Sys_Exit();

    return 0;
}
