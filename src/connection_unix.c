#include "connection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

int GetProcessId() { return getpid(); }

static BaseConnection Connection = {.sock = -1, .isOpen = 0};
static struct sockaddr_un PipeAddr;
#ifdef MSG_NOSIGNAL
static int MsgFlags = MSG_NOSIGNAL;
#else
static int MsgFlags = 0;
#endif

static const char *GetTempPath() {
    const char *temp = getenv("XDG_RUNTIME_DIR");
    temp = temp ? temp : getenv("TMPDIR");
    temp = temp ? temp : getenv("TMP");
    temp = temp ? temp : getenv("TEMP");
    temp = temp ? temp : "/tmp";
    return temp;
}

BaseConnection *BaseConnection_Create() {
    PipeAddr.sun_family = AF_UNIX;
    printf("Connection created\n");
    return &Connection;
}

void BaseConnection_Destroy(BaseConnection **c) {
    BaseConnection_Close(*c);
    *c = NULL;
    printf("Connection destroyed\n");
}

int BaseConnection_Open(BaseConnection *self) {
    const char *tempPath = GetTempPath();
    self->sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (self->sock == -1) {
        return 0;
    }
    fcntl(self->sock, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int optval = 1;
    setsockopt(self->sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif

    for (int pipeNum = 0; pipeNum < 10; ++pipeNum) {
        /* Set socket unix path (sun_path), which is of type <tmp_dir>/discord-ipc-<[0-9]> */
        snprintf(PipeAddr.sun_path, sizeof(PipeAddr.sun_path), "%s/discord-ipc-%d", tempPath, pipeNum);
        int err = connect(self->sock, (const struct sockaddr *)&PipeAddr, sizeof(PipeAddr));
        if (err == 0) {
            printf("Connected to %s\n", PipeAddr.sun_path);
            self->isOpen = 1;
            return 1;
        }
    }
    BaseConnection_Close(self);
    return 0;
}

int BaseConnection_Close(BaseConnection *self) {
    if (self->sock == -1) {
        return 0;
    }
    close(self->sock);
    self->sock = -1;
    self->isOpen = 0;
    printf("Connection closed\n");
    return 1;
}

int BaseConnection_Write(BaseConnection *self, const void *data, size_t length) {
    if (self->sock == -1) {
        return 0;
    }

    ssize_t sentBytes = send(self->sock, data, length, MsgFlags);
    if (sentBytes < 0) {
        BaseConnection_Close(self);
    }
    printf("Sent %ld bytes\n", sentBytes);
    return sentBytes == (ssize_t)length;
}

int BaseConnection_Read(BaseConnection *self, void *data, size_t length) {
    if (self->sock == -1) {
        return 0;
    }

    int res = (int)recv(self->sock, data, length, MsgFlags);
    if (res < 0) {
        // error occured
        if (errno == EAGAIN) {
            // there is no data available right now, try again later
            return 0;
        }
        BaseConnection_Close(self);
    } else if (res == 0) {
        BaseConnection_Close(self);
    }
    return res == (int)length;
}
