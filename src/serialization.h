#pragma once

#include "cJSON.h"
#include "discord_rpc.h"
#include <stdint.h>
#include <stddef.h>

// Function to convert a number to a string
void NumberToString(char *dest, int64_t number);

// Function to add an optional string to a cJSON object
void cJSON_AddOptionalStringToObject(cJSON *object, const char *name, const char *value);

// Function to write a rich presence object to JSON
size_t JsonWriteRichPresenceObj(char *dest, size_t maxLen, int nonce, int pid, const DiscordRichPresence *presence);
