#define _GNU_SOURCE
#include "../sophia_desktop_connection.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(void)
{
    struct sophia_desktop_endpoint endpoint = { SOPHIA_DESKTOP_IPC, "unchanged" };
    struct sophia_desktop_connection connection = { .fd = -1 };
    char directory[] = "/tmp/sophia-sdk-connect-XXXXXX";
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    int listener, fd, peer, result;
    assert(sophia_desktop_select_shell(NULL, NULL, &endpoint) < 0);
    assert(sophia_desktop_select_shell("/files", "/ipc", &endpoint) < 0);
    assert(sophia_desktop_select_shell("", NULL, &endpoint) < 0);
    assert(sophia_desktop_select_shell("relative", NULL, &endpoint) < 0);
    assert(!strcmp(endpoint.path, "unchanged"));
    assert(!sophia_desktop_select_shell("/files", NULL, &endpoint));
    assert(endpoint.wire == SOPHIA_DESKTOP_FILES);
    assert(!sophia_desktop_select_shell(NULL, "/ipc", &endpoint));
    assert(endpoint.wire == SOPHIA_DESKTOP_IPC);
    assert(mkdtemp(directory));
    assert(snprintf(address.sun_path, sizeof(address.sun_path), "%s/socket", directory) > 0);
    listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert(listener >= 0);
    assert(!bind(listener, (struct sockaddr *)&address, sizeof(address)));
    assert(!listen(listener, 4));
    result = sophia_desktop_connection_begin(&connection, address.sun_path);
    if (result == SOPHIA_DESKTOP_CONNECTING) {
        struct pollfd wait = { connection.fd, sophia_desktop_connection_events(&connection), 0 };
        assert(wait.events == POLLOUT);
        assert(poll(&wait, 1, 1000) == 1);
        result = sophia_desktop_connection_finish(&connection, wait.revents);
    }
    assert(result == SOPHIA_DESKTOP_CONNECTED);
    assert(!sophia_desktop_connection_events(&connection));
    fd = sophia_desktop_connection_take(&connection);
    assert(fd >= 0 && connection.fd == -1);
    assert(sophia_desktop_connection_take(&connection) == -1);
    assert(fcntl(fd, F_GETFL) & O_NONBLOCK);
    assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
    peer = accept(listener, NULL, NULL);
    assert(peer >= 0);
    close(peer);
    close(fd);
    close(listener);
    assert(!unlink(address.sun_path));
    assert(sophia_desktop_connection_begin(&connection, directory) == SOPHIA_DESKTOP_CONNECT_PEER);
    assert(connection.fd == -1);
    assert(!symlink(directory, address.sun_path));
    assert(sophia_desktop_connection_begin(&connection, address.sun_path) == SOPHIA_DESKTOP_CONNECT_PEER);
    assert(!unlink(address.sun_path));
    assert(!rmdir(directory));
    assert(sophia_desktop_connection_begin(&connection, address.sun_path) == SOPHIA_DESKTOP_CONNECT_IO);
    assert(connection.fd == -1 && connection.system_error);
    sophia_desktop_connection_close(&connection);
    puts("desktop_connection selection=explicit peer=same_uid fd=nonblocking status=pass");
    return 0;
}
