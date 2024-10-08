#pragma once

#include "cJSON.h"
#include "discord_rpc.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    cJSON *root;
    char parseBuffer[32 * 1024];
} JsonDocument;

// Simple function to copy a string
size_t StringCopy(char *dest, size_t len, const char *src);

// Function to convert a number to a string
void NumberToString(char *dest, int64_t number);

// Function to add an optional string to a cJSON object
void cJSON_AddOptionalStringToObject(cJSON *object, const char *name, const char *value);

// Function to write a rich presence object to JSON
size_t JsonWriteRichPresenceObj(char *dest, size_t maxLen, int nonce, int pid, const DiscordRichPresence *presence);

// Function to write a handshake JSON object
size_t JsonWriteHandshakeObj(char *dest, size_t maxLen, int version, const char *applicationId);

void JsonDocument_Init(JsonDocument *doc);
void JsonDocument_Parse(JsonDocument *doc, const char *json);
void JsonDocument_Delete(JsonDocument *doc);
const char *JsonDocument_Serialize(const JsonDocument *doc);

const char *GetStrMember(const JsonDocument *doc, const char *name);
int GetIntMember(const JsonDocument *doc, const char *name);
