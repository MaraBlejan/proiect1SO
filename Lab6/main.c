#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SHM_NAME L"Global\\CountSharedMemoryExample"
#define SEM_NAME L"Global\\CountSemaphoreExample"

typedef struct {
    volatile LONG current;
    LONG reserved;        
} SharedData;

void print_last_error(const char* msg) {
    DWORD e = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf, 0, NULL);
    fprintf(stderr, "%s: %s (code %lu)\n", msg, (char*)lpMsgBuf, e);
    LocalFree(lpMsgBuf);
}

int main(int argc, char* argv[]) {
    const char* role = "proc";
    if (argc >= 2) role = argv[1];

    srand((unsigned int)(GetTickCount() ^ GetCurrentProcessId() ^ (uintptr_t)argv));

    HANDLE hMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        SHM_NAME);

    if (hMap == NULL) {
        print_last_error("CreateFileMappingW failed");
        return 1;
    }

    SharedData* shared = (SharedData*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (shared == NULL) {
        print_last_error("MapViewOfFile failed");
        CloseHandle(hMap);
        return 1;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        shared->current = 0;
    }

    HANDLE hSem = CreateSemaphoreW(NULL, 1, 1, SEM_NAME);
    if (hSem == NULL) {
        print_last_error("CreateSemaphoreW failed");
        UnmapViewOfFile(shared);
        CloseHandle(hMap);
        return 1;
    }

    printf("[%s] Started. Shared current = %ld\n", role, (long)shared->current);

    while (1) {
        DWORD wait = WaitForSingleObject(hSem, INFINITE);
        if (wait != WAIT_OBJECT_0) {
            print_last_error("WaitForSingleObject failed");
            break;
        }

        long cur = (long)shared->current;
        if (cur >= 1000) {
            ReleaseSemaphore(hSem, 1, NULL);
            break;
        }

        int coin = (rand() % 2) + 1; 
        printf("[%s] read=%ld coin=%d", role, cur, coin);

        if (coin == 2) {
            long next = cur + 1;
            shared->current = (LONG)next;
            printf(" -> wrote=%ld\n", next);
        } else {
            printf(" -> did not write\n");
        }

        if (!ReleaseSemaphore(hSem, 1, NULL)) {
            print_last_error("ReleaseSemaphore failed");
            break;
        }

        Sleep(10);
    }

    printf("[%s] Detected completion (>=1000). Final value = %ld\n", role, (long)shared->current);

    CloseHandle(hSem);
    UnmapViewOfFile(shared);
    CloseHandle(hMap);

    return 0;
}
