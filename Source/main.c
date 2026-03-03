/*
 * @file main.c
 *
 * @brief The Microservice Body that receives, parses and processes requests, and response.
 *
 ******************************************************************************/

/********************************* INCLUDES ***********************************/

#define LOG_ERROR_ENABLED   1
#define LOG_WARNING_ENABLED 1
#define LOG_INFO_ENABLED    1
#include "uService.h"
#include "SysCall_UniversalResources.h"

#include "us-AWSIoT.h"

#include "core_mqtt.h"

#include "us_Internal.h"

#if CFG_ENABLE_AWS_LOGS
#define LOG_AWS         LOG_INFO
#else
#define LOG_AWS(...)
#endif

/***************************** MACRO DEFINITIONS ******************************/

#ifndef CFG_US_MAX_NUM_OF_SESSION
#define CFG_US_MAX_NUM_OF_SESSION   1       /* Let us allow one session at a time */
#endif

/*
 * Make them configurable
 */
#define MQTT_KEEPALIVE_SEC                  60
#define MQTT_CONNACK_TIMEOUT_MS             5000
#define MQTT_PROCESS_LOOP_MS                10000

/***************************** TYPE DEFINITIONS *******************************/

typedef union
{
    char host[AWS_HOST_NAME_MAX_LEN + 1];
    char thingName[AWS_THING_NAME_MAX_LEN + 1];
    char actuatorpublishDataType[AWS_PUBLISH_DATA_MAX_LEN + 1];
    char input[1];
} InputStringCheckContainer;

typedef struct {
    int  fd;        /* TCP socket */
    void* ctx; //SSL_CTX* ctx;
    void* ssl; //SSL* ssl;
} TLSSession_t;

typedef struct NetworkContext {
    TLSSession_t tls;
} NetworkContext_t;

typedef struct
{
    struct
    {
        uint32_t inUse              : 1;
        uint32_t connected          : 1;
        uint32_t sessionID          : 16;
        uint8_t subscriberExecID    : 8;
    } flags;

    NetworkContext_t netCtx;
    TransportInterface_t transport;
    MQTTContext_t mqttCtx;

    char thingName[AWS_THING_NAME_MAX_LEN];
    char subscribeTopic[AWS_TOPIC_MAX_LEN];
} AWSThingSession;

typedef struct
{
    uint32_t numOfSessions;
    #define MQTT_NET_BUF_SIZE    1024        /* coreMQTT internal buffer */
    uint8_t netBuf[MQTT_NET_BUF_SIZE];
} AWSSettings;


/**************************** FUNCTION PROTOTYPES *****************************/

PRIVATE void startService(void);
PRIVATE void processRequest(uint8_t senderID, usRequestPackage* request);
PRIVATE void sendError(uint8_t receiverID, uint16_t operation, uint16_t status);

/******************************** VARIABLES ***********************************/

#ifndef NEW_LINE
#define NEW_LINE        "\n"
#endif

/*
 * AWS ROOT CA
 * 
 * Let us embed into the code for now.
 */
PRIVATE const char AWS_IOT_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----" NEW_LINE
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF" NEW_LINE
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6" NEW_LINE
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL" NEW_LINE
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv" NEW_LINE
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj" NEW_LINE
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM" NEW_LINE
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw" NEW_LINE
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6" NEW_LINE
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L" NEW_LINE
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm" NEW_LINE
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC" NEW_LINE
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA" NEW_LINE
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI" NEW_LINE
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs" NEW_LINE
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv" NEW_LINE
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU" NEW_LINE
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy" NEW_LINE
    "rqXRfboQnoZsG4q5WTP468SQvvG5" NEW_LINE
    "-----END CERTIFICATE-----";

#define AWS_IOT_PORT            8883

PRIVATE AWSThingSession awsThingSessions[CFG_US_MAX_NUM_OF_SESSION];
PRIVATE AWSSettings awsSettings;

int main()
{
    SysStatus retVal;

    uService_PrintIntro();

    SYS_INITIALISE_IPC_MESSAGEBOX(retVal, CFG_US_MAX_NUM_OF_SESSION);
    if (retVal != SysStatus_Success)
    {
        LOG_ERROR("Failed to initialise MessageBox. Error : %d", retVal);
    }
    else
    {
        startService();
    }

    LOG_ERROR("Exiting the Microservice...");
    Sys_Exit();
}

PRIVATE void startService(void)
{
    usRequestPackage request;

    uint32_t sequenceNo;
    (void)sequenceNo;
    usStatus responseStatus;
    uint8_t senderID = 0xFF;

    while (true)
    {
        bool dataReceived = false;
        uint32_t receivedLen = 0;
        responseStatus = usStatus_Success;

        (void)Sys_IsMessageReceived(&dataReceived, &receivedLen, &sequenceNo);
        if (!dataReceived || receivedLen == 0)
        {
            /* Sleep until receive an IPC message */
            Sys_WaitForEvent(SysEvent_IPCMessage);

            continue;
        }

        if (receivedLen <= USERVICE_PACKAGE_HEADER_SIZE)
        {
            responseStatus = usStatus_InvalidParam_UnsufficientSize;
            LOG_PRINTF(" > Unsufficint Mandatory Received Length (%d)/(%d)",
                receivedLen, USERVICE_PACKAGE_HEADER_SIZE);
        }

#if 0
        if (responseStatus == usStatus_Success && 
            receivedLen > <PACKAGE_MAX_SIZE>)
        {
            responseStatus = usStatus_InvalidParam_SizeExceedAllowed;

            LOG_PRINTF(" > Received Length (%d) exceed than allowed length(%d)",
                receivedLen, <PACKAGE_MAX_SIZE>);

            /* Let us just get the header, as not need for the payload */
            receivedLen = USERVICE_PACKAGE_HEADER_SIZE;
        }
#endif

        /* Get the message */
        (void)Sys_ReceiveMessage(&senderID, (uint8_t*)&request, receivedLen, &sequenceNo);

        /* Do not process the message if there was an error */
        if (responseStatus != usStatus_Success)
        {
            sendError(senderID, request.header.operation, responseStatus);
            continue;
        }

        /* Process the request */
        processRequest(senderID, &request);
    }
}


/**************************** PRIVATE FUNCTIONS ******************************/

PRIVATE ALWAYS_INLINE uint32_t get_time_ms(void)
{
    return (uint32_t)Sys_GetTimeInMs();
}

/*
 * A helper function to check the string content securely
 */
PRIVATE bool isAlphaNumString(char* str, int maxLen, bool allowZeroLen, bool allowJson)
{
    int len;
    InputStringCheckContainer inputContainer;
    bool isValid;

    memcpy(inputContainer.input, str, maxLen);
    inputContainer.input[maxLen] = '\0';

    len = strlen(inputContainer.input);

    if ((!allowZeroLen && len == 0) ||
        ((int)strlen(inputContainer.input) >= maxLen))
    {
        return false;
    }

    /* Only "A-Za-z0-9_-" */
    {
        int i;
        for (i = 0; i < len; i++)
        {
            isValid = false;
            if ((str[i] >= 'A' && str[i] <= 'Z') ||
                (str[i] >= 'a' && str[i] <= 'z') ||
                (str[i] >= '0' && str[i] <= '9') ||
                 str[i] == '-' ||
                 str[i] == '_' ||
                 str[i] == '.')
            {
                isValid = true;
            }

            if (!isValid)
            {
                if (allowJson)
                {
                    if (str[i] == '{' || str[i] == '}' ||
                        str[i] == '\r' || str[i] == '\n' ||
                        str[i] == '"' || str[i] == ':' || str[i] == '.' || str[i] == ',' || str[i] == ' ')
                    {
                        isValid = true;
                    }
                }
            }

            if (!isValid) return false;
        }
    }

    return true;
}

/*
 * Returns the session ID, returns -1 if no slot available
 */
#define SESSION_ID(_execId, _index)       ((_execId)<<8 | (_index))
PRIVATE ALWAYS_INLINE uint32_t getSessionID(uint8_t senderID)
{
    for (int i = 0; i < CFG_US_MAX_NUM_OF_SESSION; i++)
    {
        AWSThingSession* session = &awsThingSessions[i];
        if (!session->flags.inUse)
        {
            session->flags.sessionID = SESSION_ID(senderID, i);
            return i;
        }
    }

    return -1;
}

/*
 * Checks whether the session ID is valid
 */
PRIVATE usStatus checkSessionID(uint8_t senderID, uint32_t sessionIndex)
{
    uint16_t expectedSessionID = SESSION_ID(senderID, sessionIndex);
    if (sessionIndex >= CFG_US_MAX_NUM_OF_SESSION)
    {
        return usStatus_InvalidSession;
    }

    AWSThingSession* session = &awsThingSessions[sessionIndex];
    if (!session->flags.inUse || session->flags.sessionID != expectedSessionID)
    {
        return usStatus_InvalidSession;
    }

    return usStatus_Success;
}

/*
 * TLS Transport Layer for MQTT
 */

PRIVATE int tls_connect(TLSSession_t* s, const char* host, uint16_t port,
                        const char* ca, uint32_t deviceCertHandle, uint32_t devicePKeyHandle)
{
    int32_t rc; (void)rc;

    SysUniversalResourceCredentials rootCert =   { .flags = { .type = SYS_UNIVERSAL_RES_CREDENTIAL_TYPE_RAW},       .context = {.raw =        {.key = ca, .keyLen = strlen(ca) + 1 } } };
    SysUniversalResourceCredentials deviceCert = { .flags = { .type = SYS_UNIVERSAL_RES_CREDENTIAL_TYPE_ROOTPARAM}, .context = {.rootParam = { .tag = deviceCertHandle } } };
    SysUniversalResourceCredentials privateKey = { .flags = { .type = SYS_UNIVERSAL_RES_CREDENTIAL_TYPE_ROOTPARAM}, .context = {.rootParam = { .tag = devicePKeyHandle } } };

    SysStatus retVal = Sys_TLSGetSocket((int32_t*)&s->ssl, &rc);
    if (retVal != SysStatus_Success) return -1;
    
    retVal = Sys_TLSConnect((int32_t)s->ssl, &rootCert, &deviceCert, &privateKey, host, (uint16_t)strlen(host), port, 2000, &rc);
    if (retVal != SysStatus_Success) return rc;

    return 0;
}

PRIVATE void tls_disconnect(TLSSession_t* s)
{
    int32_t rc; (void)rc;
    Sys_TLSClose(*(int32_t*)s->ssl, &rc);
}

PRIVATE int32_t transport_send(NetworkContext_t* ctx, const void* buf, size_t len)
{
    int32_t rc; (void)rc;
    int32_t writtenLen = 0;
    SysStatus retVal = Sys_TLSWrite((int32_t)ctx->tls.ssl, buf, (int)len, &writtenLen, &rc);
    return (retVal != SysStatus_Success || writtenLen <= 0) ? -1 : (int32_t)writtenLen;
}

PRIVATE int32_t transport_recv(NetworkContext_t* ctx, void* buf, size_t len)
{
    int32_t rc; (void)rc;
    int32_t readLen = 0;
    SysStatus retVal = Sys_TLSRead((int32_t)ctx->tls.ssl, buf, (int)len, &readLen, &rc);
    if (retVal == SysStatus_Success && readLen > 0) return readLen;

    return 0;
}

/*
 * MWTT Event Callback
 */
PRIVATE void mqtt_event_cb(MQTTContext_t* ctx, MQTTPacketInfo_t* pkt, MQTTDeserializedInfo_t* info)
{
    (void)ctx;

    LOG_AWS("New AWS Event : %d", pkt->type & 0xF0U);

    /* Inbound PUBLISH from a subscribed topic */
    if ((pkt->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
    {
        int i;
        MQTTPublishInfo_t* pub = info->pPublishInfo;

        for (i = 0; i < CFG_US_MAX_NUM_OF_SESSION; i++)
        {
            AWSThingSession* session = &awsThingSessions[i];
            if (session->flags.inUse)
            {
                // TODO Parse the thing name and topic, and if any waiting Exec, send message
            }
        }
    }
}

/*
 * AWS IoT Tick to receive/process events
 */
PRIVATE ALWAYS_INLINE void awsIoTTick()
{
    int i;

    for (i = 0; i < CFG_US_MAX_NUM_OF_SESSION; i++)
    {
        AWSThingSession* session = &awsThingSessions[i];
        if (session->flags.inUse)
        {
            MQTT_ProcessLoop(&session->mqttCtx);
        }
    }
}

/*
 * AWS IoT Connect Handler
 */
PRIVATE ALWAYS_INLINE SysStatus awsConnect(uint8_t senderID, usRequestPackage* request, usResponsePackage* response)
{
    MQTTStatus_t mqttStatus;
    int tlsStatus;
    MQTTFixedBuffer_t fixedBuf;
    AWSThingSession* thingSession = NULL;
    bool sessionPresent;
    int sessionID;

    fixedBuf.pBuffer = awsSettings.netBuf;
    fixedBuf.size = MQTT_NET_BUF_SIZE;

    LOG_AWS("AWS Connect");

    if (awsSettings.numOfSessions >= CFG_US_MAX_NUM_OF_SESSION || 
        ((sessionID = getSessionID(senderID)) < 0))
    {
        LOG_ERROR("No Slot Available");
        response->header.status = usStatus_NoSessionSlotAvailable;
        return SysStatus_Success;
    }

    if (!isAlphaNumString(request->payload.connect.hostName, AWS_HOST_NAME_MAX_LEN, false, false) ||
        !isAlphaNumString(request->payload.connect.thingName, AWS_THING_NAME_MAX_LEN, false, false))
    {
        LOG_ERROR("Invalid Input");
        response->header.status = usStatus_InvalidParam;
        return SysStatus_Success;
    }

    thingSession = &awsThingSessions[sessionID];

    strcpy(thingSession->thingName, request->payload.connect.thingName);

    /* Initialise/Connect TLS */
    {
        LOG_AWS("Connecting TLS to %s:%d ...\n", request->payload.connect.hostName, AWS_IOT_PORT);
        tlsStatus = tls_connect(&thingSession->netCtx.tls,
                                request->payload.connect.hostName, AWS_IOT_PORT,
                                AWS_IOT_ROOT_CA, request->payload.connect.deviceCertHandle, request->payload.connect.privateKeyHandle);
        if (tlsStatus != 0)
        {
            response->header.status = usStatus_AWSTLSConnectError;
            LOG_ERROR("TLS Connect Failed : %d", tlsStatus);
            return 1;
        }
    }

    /* Initialise MQTT */
    {
        TransportInterface_t transport;
        transport.pNetworkContext = &thingSession->netCtx;
        transport.send = transport_send;
        transport.recv = transport_recv;
        transport.writev = NULL;

        mqttStatus = MQTT_Init(&thingSession->mqttCtx, &transport, get_time_ms, mqtt_event_cb, &fixedBuf);
        if (mqttStatus != MQTTSuccess)
        {
            LOG_ERROR("AWS MQTT Init Error : %d", mqttStatus);
            response->header.status = usStatus_AWSMQTTInitError;
            tls_disconnect(&thingSession->netCtx.tls);

            return SysStatus_Success;
        }

        LOG_AWS("AWS MQTT Init Success");
    }

    /* MQTT Connect */
    {
        MQTTConnectInfo_t connectInfo;
        connectInfo.cleanSession = true;
        connectInfo.keepAliveSeconds = MQTT_KEEPALIVE_SEC;
        connectInfo.pClientIdentifier = thingSession->thingName;
        connectInfo.clientIdentifierLength = (uint16_t)strlen(thingSession->thingName);
        connectInfo.pUserName = NULL;  /* cert-based auth � no user/pass */
        connectInfo.userNameLength = 0;
        connectInfo.pPassword = NULL;
        connectInfo.passwordLength = 0;

        mqttStatus = MQTT_Connect(&thingSession->mqttCtx, &connectInfo, NULL, MQTT_CONNACK_TIMEOUT_MS, &sessionPresent);
        if (mqttStatus != MQTTSuccess)
        {
            LOG_ERROR("AWS MQTT Connect Error : %d", mqttStatus);
            response->header.status = usStatus_AWSMQTTConnectError;
            tls_disconnect(&thingSession->netCtx.tls);
            return SysStatus_Success;
        }

        LOG_AWS("AWS MQTT Connect Success");
    }

    /* Approve the Session slot */
    thingSession->flags.inUse = true;
    awsSettings.numOfSessions++;

    /* Response back */
    response->header.status = usStatus_Success;
    response->payload.connect.ctx = (void*)sessionID;

    return SysStatus_Success;
}

/*
 * AWS IoT Subscribe to Topic Handler
 */
PRIVATE ALWAYS_INLINE SysStatus subscribeToTopic(uint8_t senderID, usRequestPackage* request, usResponsePackage* response)
{
    AWSThingSession* thingSession = NULL;
    uint32_t sessionIndex;
    usStatus usstatus;
    MQTTStatus_t mqttStatus;

    LOG_AWS("AWS Subscribe Topic");

    sessionIndex = (uint32_t)request->payload.subscribeToTopic.ctx;

    usstatus = checkSessionID(senderID, sessionIndex);
    if (usstatus != usStatus_Success)
    {
        LOG_ERROR("Invalid Session");
        request->header.status = usstatus;
        return SysStatus_Success;
    }
    LOG_AWS("< - ");
    if (!isAlphaNumString(request->payload.subscribeToTopic.topicName, AWS_TOPIC_MAX_LEN, false, false))
    {
        LOG_ERROR("Invalid Input");
        response->header.status = usStatus_InvalidParam;
        return SysStatus_Success;
    }

    thingSession = &awsThingSessions[sessionIndex];

    strcpy(thingSession->subscribeTopic, request->payload.subscribeToTopic.topicName);

    /* MQTT Subscribe */
    {
        MQTTSubscribeInfo_t subscriptionInfo;
        subscriptionInfo.qos = MQTTQoS0;
        subscriptionInfo.pTopicFilter = thingSession->subscribeTopic;
        subscriptionInfo.topicFilterLength = (uint16_t)strlen(thingSession->subscribeTopic);

        // Obtain a new packet id for the subscription.
        //packetId = MQTT_GetPacketId(&thingSession->mqttCtx);

        LOG_AWS("MQTT SUBSCRIBE");
        mqttStatus = MQTT_Subscribe(&thingSession->mqttCtx, &subscriptionInfo, 1, 1);
        if (mqttStatus != MQTTSuccess)
        {
            LOG_ERROR("AWS MQTT Subscribe Error : %d", mqttStatus);
            response->header.status = usStatus_AWSMQTTSubscribeError;
            return SysStatus_Success;
        }

        LOG_AWS("AWS MQTT Subscribe Success");
    }

    /* Pump once to receive SUBACK */
    MQTT_ProcessLoop(&thingSession->mqttCtx);

    response->header.status = usStatus_Success;

    return SysStatus_Success;
}

/*
 * AWS IoT Publish to Topic Handler
 */
PRIVATE ALWAYS_INLINE SysStatus publishToTopic(uint8_t senderID, usRequestPackage* request, usResponsePackage* response)
{
    AWSThingSession* thingSession = NULL;
    uint32_t sessionIndex;
    usStatus usstatus;
    MQTTStatus_t mqttStatus;
    //char fullTopic[256];
    // uint16_t packetId;

    LOG_AWS("AWS Publish to Topic");

    sessionIndex = (uint32_t)request->payload.publishToTopic.ctx;

    usstatus = checkSessionID(senderID, sessionIndex);
    if (usstatus != usStatus_Success)
    {
        LOG_ERROR("Invalid Session");
        request->header.status = usstatus;
        return SysStatus_Success;
    }

    if (!isAlphaNumString(request->payload.publishToTopic.topicName, AWS_TOPIC_MAX_LEN, false, false) ||
        !isAlphaNumString(request->payload.publishToTopic.publishData, AWS_PUBLISH_DATA_MAX_LEN, false, true))
    {
        LOG_ERROR("Invalid Input");
        response->header.status = usStatus_InvalidParam;
        return SysStatus_Success;
    }

    thingSession = &awsThingSessions[sessionIndex];

    //snprintf(fullTopic, sizeof(fullTopic), "things/%s/%s", thingSession->thingName, request->payload.publishToTopic.topicName);

    /* MQTT Publish */
    {
        MQTTPublishInfo_t pubInfo;
        pubInfo.qos = MQTTQoS0;
        pubInfo.retain = false;
        pubInfo.pTopicName = request->payload.publishToTopic.topicName;
        pubInfo.topicNameLength = (uint16_t)strlen(request->payload.publishToTopic.topicName);
        pubInfo.pPayload = request->payload.publishToTopic.publishData;
        pubInfo.payloadLength = strlen(request->payload.publishToTopic.publishData);

        LOG_AWS("MQTT PUBLISH ");
        mqttStatus = MQTT_Publish(&thingSession->mqttCtx, &pubInfo, 0U);
        if (mqttStatus != MQTTSuccess)
        {
            LOG_ERROR("MQTT_Publish error: %d", mqttStatus);
            response->header.status = usStatus_AWSMQTTPublishError;
            return SysStatus_Success;
        }

        LOG_AWS("AWS MQTT Publish Done");
    }

    /* Pump once to receive ACK */
    MQTT_ProcessLoop(&thingSession->mqttCtx);

    response->header.status = usStatus_Success;

    return SysStatus_Success;
}

PRIVATE void processRequest(uint8_t senderID, usRequestPackage* request)
{
    SysStatus retVal = SysStatus_Success;
    usResponsePackage response;
    uint32_t sequenceNo;

    awsIoTTick();

    response.header = request->header;

    switch (request->header.operation)
    {
        case usOp_Connect:
            retVal = awsConnect(senderID, request, &response);
            break;
        case usOp_SubscribeToTopic:
            retVal = subscribeToTopic(senderID, request, &response);
            break;
        case usOp_PublishToTopic:
            retVal = publishToTopic(senderID, request, &response);
            break;

        /* Unrecognised operation */
        default:
            sendError(senderID, response.header.operation, usStatus_InvalidOperation);
            break;
    }

    if (response.header.status != usStatus_Success)
    {
        sendError(senderID, response.header.operation, response.header.status);
    }
    else
    {
        (void)Sys_SendMessage(senderID, (uint8_t*)&response, sizeof(usResponsePackage), &sequenceNo);
    }
}

PRIVATE void sendError(uint8_t receiverID, uint16_t operation, uint16_t status)
{
    uint32_t sequenceNo;
    (void)sequenceNo;
    usResponsePackage response =
    {
        .header.operation = operation,
        .header.status = status,
        .header.length = 0
    };

    (void)Sys_SendMessage(receiverID, (uint8_t*)&response, sizeof(response), &sequenceNo);
}
