#pragma once

#include <stdint.h>
#include <stdlib.h>

int GetProcessId();

typedef struct {
    int sock;
    int isOpen;
} BaseConnection;

BaseConnection* BaseConnection_Create();
void BaseConnection_Destroy(BaseConnection** c);
int BaseConnection_Open(BaseConnection* self);
int BaseConnection_Close(BaseConnection* self);
int BaseConnection_Write(BaseConnection* self, const void* data, size_t length);
int BaseConnection_Read(BaseConnection* self, void* data, size_t length);