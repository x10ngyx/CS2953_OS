#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <stddef.h>

int main(int argc, char *argv[]) {
    int p2ch_fd[2], ch2p_fd[2];
    pipe(p2ch_fd);
    pipe(ch2p_fd);
    char buf[8];
    if (fork() == 0) {
        // child process
        close(p2ch_fd[1]);
        close(ch2p_fd[0]);
        read(p2ch_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        write(ch2p_fd[1], "pong", strlen("pong"));
        close(p2ch_fd[0]);
        close(ch2p_fd[1]);
    }
    else {
        // parent process
        close(p2ch_fd[0]);
        close(ch2p_fd[1]);
        write(p2ch_fd[1], "ping", strlen("ping"));
        read(ch2p_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        close(p2ch_fd[1]);
        close(ch2p_fd[0]);

        wait(NULL);
    }
    exit(0);
}
