#include "serialization.h"
#include "cJSON.h"
#include "discord_rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t StringCopy(char *dest, size_t len, const char *src) {
    if (!src || !len) {
        return 0;
    }
    size_t copied;
    char *out = dest;
    for (copied = 1; *src && copied < len; ++copied) {
        *out++ = *src++;
    }
    *out = 0;
    return copied - 1;
}

void NumberToString(char *dest, int64_t number) {
    if (!number) {
        *dest++ = '0';
        *dest++ = 0;
        return;
    }
    if (number < 0) {
        *dest++ = '-';
        number = -number;
    }
    char temp[32];
    int place = 0;
    while (number) {
        int digit = number % 10;
        number = number / 10;
        temp[place++] = '0' + (char)digit;
    }
    for (--place; place >= 0; --place) {
        *dest++ = temp[place];
    }
    *dest = 0;
}

void cJSON_AddOptionalStringToObject(cJSON *object, const char *name, const char *value) {
    if (value && value[0]) {
        cJSON_AddStringToObject(object, name, value);
    }
}

size_t JsonWriteRichPresenceObj(char *dest, size_t maxLen, int nonce, int pid, const DiscordRichPresence *presence) {
    cJSON *root = cJSON_CreateObject();

    char nonceBuffer[32];
    NumberToString(nonceBuffer, nonce);
    cJSON_AddStringToObject(root, "nonce", nonceBuffer);

    cJSON_AddStringToObject(root, "cmd", "SET_ACTIVITY");

    cJSON *args = cJSON_CreateObject();
    cJSON_AddNumberToObject(args, "pid", pid);

    if (presence) {
        cJSON *activity = cJSON_CreateObject();

        cJSON_AddNumberToObject(activity, "type", presence->type);
        cJSON_AddOptionalStringToObject(activity, "state", presence->state);
        cJSON_AddOptionalStringToObject(activity, "details", presence->details);

        if (presence->startTimestamp || presence->endTimestamp) {
            cJSON *timestamps = cJSON_CreateObject();
            if (presence->startTimestamp) {
                cJSON_AddNumberToObject(timestamps, "start", presence->startTimestamp);
            }
            if (presence->endTimestamp) {
                cJSON_AddNumberToObject(timestamps, "end", presence->endTimestamp);
            }
            cJSON_AddItemToObject(activity, "timestamps", timestamps);
        }

        if ((presence->largeImageKey && presence->largeImageKey[0]) ||
            (presence->largeImageText && presence->largeImageText[0]) ||
            (presence->smallImageKey && presence->smallImageKey[0]) ||
            (presence->smallImageText && presence->smallImageText[0])) {
            cJSON *assets = cJSON_CreateObject();
            cJSON_AddOptionalStringToObject(assets, "large_image", presence->largeImageKey);
            cJSON_AddOptionalStringToObject(assets, "large_text", presence->largeImageText);
            cJSON_AddOptionalStringToObject(assets, "small_image", presence->smallImageKey);
            cJSON_AddOptionalStringToObject(assets, "small_text", presence->smallImageText);
            cJSON_AddItemToObject(activity, "assets", assets);
        }

        if ((presence->partyId && presence->partyId[0]) || presence->partySize || presence->partyMax) {
            cJSON *party = cJSON_CreateObject();
            cJSON_AddOptionalStringToObject(party, "id", presence->partyId);
            if (presence->partySize && presence->partyMax) {
                cJSON *size = cJSON_CreateArray();
                cJSON_AddItemToArray(size, cJSON_CreateNumber(presence->partySize));
                cJSON_AddItemToArray(size, cJSON_CreateNumber(presence->partyMax));
                cJSON_AddItemToObject(party, "size", size);
            }
            cJSON_AddItemToObject(activity, "party", party);
        }

        if ((presence->matchSecret && presence->matchSecret[0]) || (presence->joinSecret && presence->joinSecret[0]) ||
            (presence->spectateSecret && presence->spectateSecret[0])) {
            cJSON *secrets = cJSON_CreateObject();
            cJSON_AddOptionalStringToObject(secrets, "match", presence->matchSecret);
            cJSON_AddOptionalStringToObject(secrets, "join", presence->joinSecret);
            cJSON_AddOptionalStringToObject(secrets, "spectate", presence->spectateSecret);
            cJSON_AddItemToObject(activity, "secrets", secrets);
        }

        if ((presence->emojiName && presence->emojiName[0]) || (presence->emojiId && presence->emojiId[0])) {
            cJSON *emoji = cJSON_CreateObject();
            cJSON_AddOptionalStringToObject(emoji, "name", presence->emojiName);
            cJSON_AddOptionalStringToObject(emoji, "id", presence->emojiId);
            cJSON_AddBoolToObject(emoji, "animated", presence->emojiAnimated);
            cJSON_AddItemToObject(activity, "emoji", emoji);
        }

        if (presence->buttons[0].label && presence->buttons[0].label[0]) {
            cJSON *buttons = cJSON_CreateArray();
            for (int i = 0; i < 2; ++i) {
                if (presence->buttons[i].label && presence->buttons[i].label[0]) {
                    cJSON *button = cJSON_CreateObject();
                    cJSON_AddStringToObject(button, "label", presence->buttons[i].label);
                    cJSON_AddStringToObject(button, "url", presence->buttons[i].url);
                    cJSON_AddItemToArray(buttons, button);
                }
            }
            cJSON_AddItemToObject(activity, "buttons", buttons);
        }

        cJSON_AddBoolToObject(activity, "instance", presence->instance);
        cJSON_AddItemToObject(args, "activity", activity);
    }

    cJSON_AddItemToObject(root, "args", args);

    // Output to string
    char *out = cJSON_PrintUnformatted(root);
    strncpy(dest, out, maxLen);

    size_t size = strlen(out);

    free(out);
    cJSON_Delete(root);

    return size;
}

size_t JsonWriteHandshakeObj(char *dest, size_t maxLen, int version, const char *applicationId) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "v", version);
    cJSON_AddStringToObject(root, "client_id", applicationId);

    char *out = cJSON_PrintUnformatted(root);
    strncpy(dest, out, maxLen);

    size_t size = strlen(out);

    free(out);
    cJSON_Delete(root);

    return size;
}

void JsonDocument_Init(JsonDocument *doc) {
    doc->root = NULL;
    memset(doc->parseBuffer, 0, sizeof(doc->parseBuffer));
}

void JsonDocument_Parse(JsonDocument *doc, const char *json) {
    if (doc->root) {
        cJSON_Delete(doc->root);
    }
    doc->root = cJSON_Parse(json);
}

void JsonDocument_Delete(JsonDocument *doc) {
    if (doc->root) {
        cJSON_Delete(doc->root);
        doc->root = NULL;
    }
}

const char *JsonDocument_Serialize(const JsonDocument *doc) {
    if (doc->root) {
        char *out = cJSON_PrintUnformatted(doc->root);
        return out;
    }
    return NULL;
}

const char *GetStrMember(const JsonDocument *doc, const char *name) {
    if (doc->root) {
        cJSON *item = cJSON_GetObjectItem(doc->root, name);
        if (cJSON_IsString(item)) {
            return item->valuestring;
        }
    }
    return NULL;
}

int GetIntMember(const JsonDocument *doc, const char *name) {
    if (doc->root) {
        cJSON *item = cJSON_GetObjectItem(doc->root, name);
        if (cJSON_IsNumber(item)) {
            return item->valueint;
        }
    }
    return 0;
}
