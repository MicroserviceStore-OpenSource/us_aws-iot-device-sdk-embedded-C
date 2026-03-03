/*
 * @file us.h
 *
 * @brief Microservice Public API
 *
 ******************************************************************************/

#ifndef __US_AWSIOT_H
#define __US_AWSIOT_H

/********************************* INCLUDES ***********************************/

#include "uService.h"

/***************************** MACRO DEFINITIONS ******************************/

#define AWS_HOST_NAME_MAX_LEN           128
#define AWS_THING_NAME_MAX_LEN          32
#define AWS_TOPIC_MAX_LEN               32
#define AWS_PUBLISH_DATA_MAX_LEN        128

/***************************** TYPE DEFINITIONS *******************************/

/*
 * Default Status
 */
typedef enum
{
    usStatus_Success = 0,
    /* Operation not defined or the access not granted */
    usStatus_InvalidOperation,
    /* Timeout occurred during the opereration */
    usStatus_Timeout,
    /* Microservice does not have any available session */
    usStatus_NoSessionSlotAvailable,
    /* Request to an invalid session */
    usStatus_InvalidSession,
    /* Invalid Parameter */
    usStatus_InvalidParam,
    /* Invalid Parameter - Insufficient Input or Output Size  */
    usStatus_InvalidParam_UnsufficientSize,
    /* Invalid Parameter - Input or Output exceeds the allowed capacity  */
    usStatus_InvalidParam_SizeExceedAllowed,

    /* The developer can defines custom statuses */
    usStatus_CustomStart = 32,

    usStatus_AWSTLSConnectError,
    usStatus_AWSMQTTInitError,
    usStatus_AWSMQTTConnectError,
    usStatus_AWSMQTTSubscribeError,
    usStatus_AWSMQTTPublishError,

    usStatus_AWSAlreadyConnected,
} usStatus;

/*
 * AWS IoT Context
 */
typedef struct
{
    void* ctx;
} AWSIoTContext;

/**************************** FUNCTION PROTOTYPES *****************************/

/******************************** VARIABLES ***********************************/

/*
 * Initialise the Microservice
 *
 * @param none
 *
 * @retval SysStatus_Success Success
 * @retval SysStatus_NotFound The Microservice not found on the device.
 */
SysStatus us_AWSIoT_Initialise(void);

/*
 * Connect to AWS IoT Thing
 *
 * @param       host                AWS IoT Thing URL
 * @param       thingName           AWS IoT Thing Name
 * @param       deviceCertHandle    AWS IoT Thing Device Certificate Handle
 * @param       privateKeyHandle    AWS IoT Thing Device Private Key
 * @param       timeoutInMs         Timeout for the blocking operation in milliseconds
 * @param[out]  ctx                 AWS IoT Session Context/Handle
 * @param[out]  usStatus    Microservice Specific Error
 *
 * @retval SysStatus_Success Success
 * @retval SysStatus_NotFound The Microservice not found on the device.
 */
SysStatus us_AWSIoT_Connect(char host[AWS_HOST_NAME_MAX_LEN], char thingName[AWS_THING_NAME_MAX_LEN], uint32_t deviceCertHandle, uint32_t privateKeyHandle, uint32_t timeoutInMs, AWSIoTContext* ctx, usStatus* usStatus);

/*
 * Subscribe to a topic
 *
 * @param       ctx         AWS IoT Session Context/Handle
 * @param       topicName   Topic to subscribe
 * @param       timeoutInMs Timeout for the blocking operation in milliseconds
 * @param[out]  usStatus    Microservice Specific Error
 *
 * @retval SysStatus_Success Success
 * @retval SysStatus_NotFound The Microservice not found on the device.
 */
SysStatus us_AWSIoT_SubscribeToTopic(AWSIoTContext* ctx, char topicName[AWS_TOPIC_MAX_LEN], uint32_t timeoutInMs, usStatus* usStatus);

/*
 * Puslish to a topic
 *
 * @param       ctx         AWS IoT Session Context/Handle
 * @param       topicName   Topic to publish
 * @param       payload     Payload to publish
 * @param       timeoutInMs Timeout for the blocking operation in milliseconds
 * @param[out]  usStatus    Microservice Specific Error
 *
 * @retval SysStatus_Success Success
 * @retval SysStatus_NotFound The Microservice not found on the device.
 */
SysStatus us_AWSIoT_PublishToTopic(AWSIoTContext* ctx, char topicName[AWS_TOPIC_MAX_LEN], char payload[AWS_PUBLISH_DATA_MAX_LEN], uint32_t timeoutInMs, usStatus* usStatus);

#endif /* __US_AWSIOT_H */
