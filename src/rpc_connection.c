#include "rpc_connection.h"
#include "discord_rpc.h"
#include "serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const int RpcVersion = 1;
static RpcConnection Instance;

RpcConnection *RpcConnection_Create(const char *applicationId) {
    Instance.connection = BaseConnection_Create();
    StringCopy(Instance.appId, sizeof(Instance.appId), applicationId);
    return &Instance;
}

void RpcConnection_Open(RpcConnection *self) {
    if (self->state == State_Connected) {
        return;
    }

    if (self->state == State_Disconnected && !BaseConnection_Open(self->connection)) {
        return;
    }

    if (self->state == State_SentHandshake) {
        JsonDocument message;
        JsonDocument_Init(&message);
        if (RpcConnection_Read(self, &message)) {
            const char *cmd = GetStrMember(&message, "cmd");
            const char *evt = GetStrMember(&message, "evt");
            if (cmd && evt && strcmp(cmd, "DISPATCH") == 0 && strcmp(evt, "READY") == 0) {
                self->state = State_Connected;
                if (self->onConnect) {
                    self->onConnect(&message);
                }
            }
        }
        JsonDocument_Delete(&message);
    } else {
        self->sendFrame.header.opcode = Opcode_Handshake;
        self->sendFrame.header.length = (uint32_t)JsonWriteHandshakeObj(
            self->sendFrame.message, sizeof(self->sendFrame.message), RpcVersion, self->appId);

        printf("Sending handshake\n");
        if (BaseConnection_Write(self->connection, &self->sendFrame,
                                 sizeof(MessageFrameHeader) + self->sendFrame.header.length)) {
            self->state = State_SentHandshake;
        } else {
            RpcConnection_Close(self);
        }
    }
}

void RpcConnection_Close(RpcConnection *self) {
    if (self->onDisconnect && (self->state == State_Connected || self->state == State_SentHandshake)) {
        self->onDisconnect(self->lastErrorCode, self->lastErrorMessage);
    }
    BaseConnection_Close(self->connection);
    self->state = State_Disconnected;
}

void RpcConnection_Destroy(RpcConnection **c) {
    RpcConnection_Close(*c);
    BaseConnection_Destroy(&(*c)->connection);
    *c = NULL;
}

int RpcConnection_IsOpen(const RpcConnection *self) {
    return self->state == State_Connected;
}

int RpcConnection_Write(RpcConnection *self, const void *data, size_t length) {
    self->sendFrame.header.opcode = Opcode_Frame;
    memcpy(self->sendFrame.message, data, length);
    self->sendFrame.header.length = (uint32_t)length;
    if (!BaseConnection_Write(self->connection, &self->sendFrame, sizeof(MessageFrameHeader) + length)) {
        RpcConnection_Close(self);
        return 0;
    }
    return 1;
}

int RpcConnection_Read(RpcConnection *self, JsonDocument *message) {
    if (self->state != State_Connected && self->state != State_SentHandshake) {
        return 0;
    }
    MessageFrame readFrame;
    for (;;) {
        int didRead = BaseConnection_Read(self->connection, &readFrame, sizeof(MessageFrameHeader));
        if (!didRead) {
            if (!self->connection->isOpen) {
                self->lastErrorCode = (int)ErrorCode_PipeClosed;
                StringCopy(self->lastErrorMessage, sizeof(self->lastErrorMessage), "Pipe closed");
                RpcConnection_Close(self);
            }
            return 0;
        }

        if (readFrame.header.length > 0) {
            didRead = BaseConnection_Read(self->connection, readFrame.message, readFrame.header.length);
            if (!didRead) {
                self->lastErrorCode = (int)ErrorCode_ReadCorrupt;
                StringCopy(self->lastErrorMessage, sizeof(self->lastErrorMessage), "Partial data in frame");
                RpcConnection_Close(self);
                return 0;
            }
            readFrame.message[readFrame.header.length] = 0;
        }

        switch (readFrame.header.opcode) {
        case Opcode_Close:
            JsonDocument_Parse(message, readFrame.message);
            self->lastErrorCode = GetIntMember(message, "code");
            StringCopy(self->lastErrorMessage, sizeof(self->lastErrorMessage), GetStrMember(message, "message"));
            RpcConnection_Close(self);
            return 0;
        case Opcode_Frame:
            JsonDocument_Parse(message, readFrame.message);
            return 1;
        case Opcode_Ping:
            readFrame.header.opcode = Opcode_Pong;
            if (!BaseConnection_Write(self->connection, &readFrame,
                                      sizeof(MessageFrameHeader) + readFrame.header.length)) {
                RpcConnection_Close(self);
            }
            break;
        case Opcode_Pong:
            break;
        case Opcode_Handshake:
        default:
            self->lastErrorCode = (int)ErrorCode_ReadCorrupt;
            StringCopy(self->lastErrorMessage, sizeof(self->lastErrorMessage), "Bad ipc frame");
            RpcConnection_Close(self);
            return 0;
        }
    }
}

int main() {
    // Create and open a connection
    RpcConnection *conn = RpcConnection_Create("1089967606334763068");
    RpcConnection_Open(conn);

    // Create a rich presence object
    DiscordRichPresence presence;
    memset(&presence, 0, sizeof(presence));
    presence.type = 0;
    presence.state = "Playing";
    presence.details = "In a game";
    presence.startTimestamp = 1507665886;

    // add button
    DiscordButton buttons[2];
    memset(buttons, 0, sizeof(buttons));
    buttons[0].label = "Button 1";
    buttons[0].url = "https://example.com";

    presence.buttons[0] = buttons[0];

    // Write the rich presence object to the connection
    char buffer[4096];
    size_t length = JsonWriteRichPresenceObj(buffer, sizeof(buffer), 0, GetProcessId(), &presence);
    RpcConnection_Write(conn, buffer, length);

    // Read the response from the connection
    printf("\nReading response...\n");
    JsonDocument message;
    JsonDocument_Init(&message);

    // TODO: use epoll instead -> https://www.man7.org/linux/man-pages/man7/epoll.7.html
    int read_success = 0;
    while (!read_success) {
        read_success = RpcConnection_Read(conn, &message);
        if (!read_success) {
            printf("No data available right now, trying again...\n");
            usleep(100000);
        }
    }

    printf("Response: %s\n\n", JsonDocument_Serialize(&message));

    // print connection attributes
    printf("Connection state: %d\n", conn->state);
    printf("Connection appId: %s\n", conn->appId);
    printf("Connection lastErrorCode: %d\n", conn->lastErrorCode);
    printf("Connection lastErrorMessage: %s\n", conn->lastErrorMessage);
    printf("Connection isOpen: %d\n", conn->connection->isOpen);
}