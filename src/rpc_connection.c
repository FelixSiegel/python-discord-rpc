#include "rpc_connection.h"
#include "serialization.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const int RpcVersion = 1;
static RpcConnection Instance;

RpcConnection* RpcConnection_Create(const char* applicationId) {
    Instance.connection = BaseConnection_Create();
    StringCopy(Instance.appId, applicationId);
    return &Instance;
}

void RpcConnection_Destroy(RpcConnection** c) {
    RpcConnection_Close(*c);
    BaseConnection_Destroy(&(*c)->connection);
    *c = NULL;
}

void RpcConnection_Open(RpcConnection* self) {
    if (self->state == State_Connected) {
        return;
    }

    if (self->state == State_Disconnected && !BaseConnection_Open(self->connection)) {
        return;
    }

    if (self->state == State_SentHandshake) {
        JsonDocument message;
        if (RpcConnection_Read(self, &message)) {
            const char* cmd = GetStrMember(&message, "cmd");
            const char* evt = GetStrMember(&message, "evt");
            if (cmd && evt && strcmp(cmd, "DISPATCH") == 0 && strcmp(evt, "READY") == 0) {
                self->state = State_Connected;
                if (self->onConnect) {
                    self->onConnect(&message);
                }
            }
        }
    } else {
        self->sendFrame.opcode = Opcode_Handshake;
        self->sendFrame.length = (uint32_t)JsonWriteHandshakeObj(
            self->sendFrame.message, sizeof(self->sendFrame.message), RpcVersion, self->appId);

        if (BaseConnection_Write(self->connection, &self->sendFrame, sizeof(MessageFrameHeader) + self->sendFrame.length)) {
            self->state = State_SentHandshake;
        } else {
            RpcConnection_Close(self);
        }
    }
}

void RpcConnection_Close(RpcConnection* self) {
    if (self->onDisconnect && (self->state == State_Connected || self->state == State_SentHandshake)) {
        self->onDisconnect(self->lastErrorCode, self->lastErrorMessage);
    }
    BaseConnection_Close(self->connection);
    self->state = State_Disconnected;
}

bool RpcConnection_Write(RpcConnection* self, const void* data, size_t length) {
    self->sendFrame.opcode = Opcode_Frame;
    memcpy(self->sendFrame.message, data, length);
    self->sendFrame.length = (uint32_t)length;
    if (!BaseConnection_Write(self->connection, &self->sendFrame, sizeof(MessageFrameHeader) + length)) {
        RpcConnection_Close(self);
        return false;
    }
    return true;
}

bool RpcConnection_Read(RpcConnection* self, JsonDocument* message) {
    if (self->state != State_Connected && self->state != State_SentHandshake) {
        return false;
    }
    MessageFrame readFrame;
    for (;;) {
        bool didRead = BaseConnection_Read(self->connection, &readFrame, sizeof(MessageFrameHeader));
        if (!didRead) {
            if (!self->connection->isOpen) {
                self->lastErrorCode = (int)ErrorCode_PipeClosed;
                StringCopy(self->lastErrorMessage, "Pipe closed");
                RpcConnection_Close(self);
            }
            return false;
        }

        if (readFrame.length > 0) {
            didRead = BaseConnection_Read(self->connection, readFrame.message, readFrame.length);
            if (!didRead) {
                self->lastErrorCode = (int)ErrorCode_ReadCorrupt;
                StringCopy(self->lastErrorMessage, "Partial data in frame");
                RpcConnection_Close(self);
                return false;
            }
            readFrame.message[readFrame.length] = 0;
        }

        switch (readFrame.opcode) {
        case Opcode_Close:
            JsonDocument_ParseInsitu(message, readFrame.message);
            self->lastErrorCode = GetIntMember(message, "code");
            StringCopy(self->lastErrorMessage, GetStrMember(message, "message", ""));
            RpcConnection_Close(self);
            return false;
        case Opcode_Frame:
            JsonDocument_ParseInsitu(message, readFrame.message);
            return true;
        case Opcode_Ping:
            readFrame.opcode = Opcode_Pong;
            if (!BaseConnection_Write(self->connection, &readFrame, sizeof(MessageFrameHeader) + readFrame.length)) {
                RpcConnection_Close(self);
            }
            break;
        case Opcode_Pong:
            break;
        case Opcode_Handshake:
        default:
            self->lastErrorCode = (int)ErrorCode_ReadCorrupt;
            StringCopy(self->lastErrorMessage, "Bad ipc frame");
            RpcConnection_Close(self);
            return false;
        }
    }
}