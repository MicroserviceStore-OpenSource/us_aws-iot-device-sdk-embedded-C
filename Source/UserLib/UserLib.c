/*
 * @file
 *
 * @brief Microservice API static library source file. This source file/library
 *        runs in the caller execution.
 *
 ******************************************************************************/

/********************************* INCLUDES ***********************************/

#include "us-AWSIoT.h"

#include "us_Internal.h"

#include "uService.h"

/***************************** MACRO DEFINITIONS ******************************/

/***************************** TYPE DEFINITIONS *******************************/
typedef struct
{
    struct
    {
        uint32_t initialised        : 1;
    } flags;

    /*
     * The "Execution Index" is a system wide enumaration by the Microservice Runtime
     * to interact with the Microservice.
     */
    uint32_t execIndex;
} uS_UserLibSettings;

/**************************** FUNCTION PROTOTYPES *****************************/

/******************************** VARIABLES ***********************************/
PRIVATE uS_UserLibSettings userLibSettings;

PRIVATE const char usUID[SYS_EXEC_NAME_MAX_LENGTH] = USERVICE_UID;

/***************************** PRIVATE FUNCTIONS *******************************/

/***************************** PUBLIC FUNCTIONS *******************************/
#define INITIALISE_FUNCTIONEXPAND(a, b, c) a##b##c
#define INITIALISE_FUNCTION(name) INITIALISE_FUNCTIONEXPAND(us_, name, _Initialise)
SysStatus INITIALISE_FUNCTION(USERVICE_NAME_NONSTR)(void)
{
    /* Get the Microservice Index to interact with the Microservice */
    return uService_Initialise(usUID, &userLibSettings.execIndex);
}

SysStatus us_AWSIoT_Connect(char host[AWS_HOST_NAME_MAX_LEN], char thingName[AWS_THING_NAME_MAX_LEN], uint32_t deviceCertHandle, uint32_t privateKeyHandle, uint32_t timeoutInMs, AWSIoTContext* ctx, usStatus* result)
{
    SysStatus retVal;

    usResponsePackage response;
    usRequestPackage request;

    // Prepare the package
    {
        request.header.operation = usOp_Connect;
        request.header.length = sizeof(request);
        memcpy(request.payload.connect.hostName, host, AWS_HOST_NAME_MAX_LEN);
        memcpy(request.payload.connect.thingName, thingName, AWS_THING_NAME_MAX_LEN);
        request.payload.connect.deviceCertHandle = deviceCertHandle;
        request.payload.connect.privateKeyHandle = privateKeyHandle;
    }

    retVal = uService_RequestBlocker(userLibSettings.execIndex, (uServicePackage*)&request, (uServicePackage*)&response, timeoutInMs);
    *result = response.header.status;

    if (retVal == SysStatus_Success && *result == usStatus_Success)
    {
        ctx->ctx = response.payload.connect.ctx;
    }

    return retVal;
}

SysStatus us_AWSIoT_SubscribeToTopic(AWSIoTContext* ctx, char topicName[AWS_TOPIC_MAX_LEN], uint32_t timeoutInMs, usStatus* result)
{
    SysStatus retVal;

    usResponsePackage response;
    usRequestPackage request;

    // Prepare the package
    {
        request.header.operation = usOp_SubscribeToTopic;
        request.header.length = sizeof(request);
        memcpy(request.payload.subscribeToTopic.topicName, topicName, AWS_TOPIC_MAX_LEN);
        request.payload.subscribeToTopic.ctx = ctx->ctx;
    }

    retVal = uService_RequestBlocker(userLibSettings.execIndex, (uServicePackage*)&request, (uServicePackage*)&response, timeoutInMs);
    *result = response.header.status;

    return retVal;
}

SysStatus us_AWSIoT_PublishToTopic(AWSIoTContext* ctx, char topicName[AWS_TOPIC_MAX_LEN], char payload[AWS_PUBLISH_DATA_MAX_LEN], uint32_t timeoutInMs, usStatus* result)
{
    SysStatus retVal;

    usResponsePackage response;
    usRequestPackage request;

    // Prepare the package
    {
        request.header.operation = usOp_PublishToTopic;
        request.header.length = sizeof(request);
        memcpy(request.payload.publishToTopic.topicName, topicName, AWS_TOPIC_MAX_LEN);
        memcpy(request.payload.publishToTopic.publishData, payload, AWS_PUBLISH_DATA_MAX_LEN);
        request.payload.publishToTopic.ctx = ctx->ctx;
    }

    retVal = uService_RequestBlocker(userLibSettings.execIndex, (uServicePackage*)&request, (uServicePackage*)&response, timeoutInMs);
    *result = response.header.status;

    return retVal;
}
