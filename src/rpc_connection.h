#pragma once

#include "connection.h"
#include "serialization.h"

#include <stdbool.h>

// I took this from the buffer size libuv uses for named pipes; I suspect ours would usually be much
// smaller.
#define MaxRpcFrameSize (64 * 1024)

typedef void (*OnConnectCallback)(JsonDocument *message);
typedef void (*OnDisconnectCallback)(int errorCode, const char *message);

typedef enum {
    ErrorCode_Success = 0,
    ErrorCode_PipeClosed = 1,
    ErrorCode_ReadCorrupt = 2,
} ErrorCode;

typedef enum {
    Opcode_Handshake = 0,
    Opcode_Frame = 1,
    Opcode_Close = 2,
    Opcode_Ping = 3,
    Opcode_Pong = 4,
} Opcode;

typedef struct {
    Opcode opcode;
    uint32_t length;
} MessageFrameHeader;

typedef struct {
    MessageFrameHeader header;
    char message[MaxRpcFrameSize - sizeof(MessageFrameHeader)];
} MessageFrame;

typedef enum {
    State_Disconnected,
    State_SentHandshake,
    State_AwaitingResponse,
    State_Connected,
} State;

typedef struct {
    BaseConnection *connection;
    State state;
    OnConnectCallback onConnect;
    OnDisconnectCallback onDisconnect;
    char appId[64];
    int lastErrorCode;
    char lastErrorMessage[256];
    MessageFrame sendFrame;
} RpcConnection;

RpcConnection *RpcConnection_Create(const char *applicationId);
void RpcConnection_Destroy(RpcConnection **c);

bool RpcConnection_IsOpen(const RpcConnection *self);

void RpcConnection_Open(RpcConnection *self);
void RpcConnection_Close(RpcConnection *self);
bool RpcConnection_Write(RpcConnection *self, const void *data, size_t length);
bool RpcConnection_Read(RpcConnection *self, JsonDocument *message);
