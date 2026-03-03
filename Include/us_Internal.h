/*
 * @file us_Internal.h
 *
 * @brief Microservice Internal Definitions
 *
 ******************************************************************************/

#ifndef __US_INTERNAL_H
#define __US_INTERNAL_H

/********************************* INCLUDES ***********************************/

#include "uService.h"

/***************************** MACRO DEFINITIONS ******************************/

#define AWS_HOST_NAME_MAX_LEN           128
#define AWS_THING_NAME_MAX_LEN          32
#define AWS_TOPIC_MAX_LEN               32
#define AWS_PUBLISH_DATA_MAX_LEN        128

/***************************** TYPE DEFINITIONS *******************************/

typedef enum
{
    usOp_Connect,
    usOp_SubscribeToTopic,
    usOp_PublishToTopic,
} usOperations;

typedef struct
{
    uServicePackageHeader header;

    union
    {
        struct
        {
            char hostName[AWS_HOST_NAME_MAX_LEN];
            char thingName[AWS_THING_NAME_MAX_LEN];
            uint32_t deviceCertHandle;
            uint32_t privateKeyHandle;
        } connect;
        struct
        {
            void* ctx;
            char topicName[AWS_TOPIC_MAX_LEN];
        } subscribeToTopic;
        struct
        {
            void* ctx;
            char topicName[AWS_TOPIC_MAX_LEN];
            char publishData[AWS_PUBLISH_DATA_MAX_LEN];
        } publishToTopic;
    } payload;
} usRequestPackage;

typedef struct
{
    uServicePackageHeader header;

    union
    {
        struct
        {
            void* ctx;
        } connect;
        struct
        {
            int32_t status;
        } registerActuator, sendDeltaInteger, sendDeltaBoolean;
    } payload;
} usResponsePackage;

/**************************** FUNCTION PROTOTYPES *****************************/

/******************************** VARIABLES ***********************************/

#endif /* __US_INTERNAL_H */
