#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0) return false;
    int r = sqrt(n);
    for (int i = 3; i <= r; i += 2) if (n % i == 0) return false;
    return true;
}

int main() {
    const int TOTAL = 10000;
    const int NPROC = 10;
    const int CHUNK = TOTAL / NPROC; 


    vector<int> p2c_read(NPROC, -1), p2c_write(NPROC, -1);
    vector<int> c2p_read(NPROC, -1), c2p_write(NPROC, -1);
    vector<pid_t> children(NPROC, -1);

    for (int i = 0; i < NPROC; ++i) {
        int p2c[2];
        int c2p[2];
        if (pipe(p2c) == -1) { perror("pipe p2c"); exit(1); }
        if (pipe(c2p) == -1) { perror("pipe c2p"); exit(1); }
        p2c_read[i] = p2c[0];
        p2c_write[i] = p2c[1];
        c2p_read[i] = c2p[0];
        c2p_write[i] = c2p[1];

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            for (int j = 0; j <= i; ++j) {
                if (j != i) {
                    if (p2c_write[j] != -1) close(p2c_write[j]);
                    if (p2c_read[j] != -1) close(p2c_read[j]);
                    if (c2p_write[j] != -1) close(c2p_write[j]);
                    if (c2p_read[j] != -1) close(c2p_read[j]);
                }
            }
            close(p2c_write[i]); 
            close(c2p_read[i]);  

            
            int start, end;
            ssize_t r = read(p2c_read[i], &start, sizeof(start));
            if (r <= 0) { close(p2c_read[i]); close(c2p_write[i]); _exit(0); }
            r = read(p2c_read[i], &end, sizeof(end));
            if (r <= 0) { close(p2c_read[i]); close(c2p_write[i]); _exit(0); }
            close(p2c_read[i]);

            for (int x = start; x <= end; ++x) {
                if (isPrime(x)) {
                    int v = x;
                    if (write(c2p_write[i], &v, sizeof(v)) == -1) {
                        break;
                    }
                }
            }

            close(c2p_write[i]);
            _exit(0);
        } else {
            children[i] = pid;
            close(p2c_read[i]); 
            close(c2p_write[i]); 
        }
    }

    for (int i = 0; i < NPROC; ++i) {
        int start = i * CHUNK + 1;
        int end = (i + 1) * CHUNK;
        if (write(p2c_write[i], &start, sizeof(start)) == -1) perror("write start");
        if (write(p2c_write[i], &end, sizeof(end)) == -1) perror("write end");
        close(p2c_write[i]); 
    }


    fd_set readfds;
    int maxfd = -1;
    vector<bool> finished(NPROC, false);
    int remaining = NPROC;

    while (remaining > 0) {
        FD_ZERO(&readfds);
        maxfd = -1;
        for (int i = 0; i < NPROC; ++i) {
            if (!finished[i]) {
                FD_SET(c2p_read[i], &readfds);
                if (c2p_read[i] > maxfd) maxfd = c2p_read[i];
            }
        }
        if (maxfd == -1) break;
        int ret = select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (ret == -1) { perror("select"); break; }
        for (int i = 0; i < NPROC; ++i) {
            if (finished[i]) continue;
            if (FD_ISSET(c2p_read[i], &readfds)) {
                int v;
                ssize_t r = read(c2p_read[i], &v, sizeof(v));
                if (r == 0) {
                    close(c2p_read[i]);
                    finished[i] = true;
                    --remaining;
                } else if (r == sizeof(v)) {
                    cout << v << '\n';
                } else if (r == -1) {
                    perror("read child pipe");
                    close(c2p_read[i]);
                    finished[i] = true;
                    --remaining;
                }
            }
        }
    }

    for (int i = 0; i < NPROC; ++i) waitpid(children[i], nullptr, 0);

    return 0;
}
