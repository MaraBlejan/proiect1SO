#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0) return false;
    int r = (int)floor(sqrt((double)n));
    for (int i = 3; i <= r; i += 2) if (n % i == 0) return false;
    return true;
}

int child_main() {
    int start, end;
    if (fread(&start, sizeof(start), 1, stdin) != 1) return 0;
    if (fread(&end, sizeof(end), 1, stdin) != 1) return 0;
    for (int x = start; x <= end; ++x) {
        if (isPrime(x)) {
            fwrite(&x, sizeof(x), 1, stdout);
            fflush(stdout);
        }
    }
    return 0;
}

int parent_main() {
    const int TOTAL = 10000;
    const int NPROC = 10;
    const int CHUNK = TOTAL / NPROC;

    vector<HANDLE> childStdInWrite(NPROC, NULL); 
    vector<HANDLE> childStdOutRead(NPROC, NULL); 
    vector<PROCESS_INFORMATION> procs(NPROC);

    for (int i = 0; i < NPROC; ++i) {
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        HANDLE hChildStd_IN_Rd = NULL;
        HANDLE hChildStd_IN_Wr = NULL;
        HANDLE hChildStd_OUT_Rd = NULL;
        HANDLE hChildStd_OUT_Wr = NULL;

        if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) {
            cerr << "StdIn CreatePipe failed\n";
            return 1;
        }
		
        if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
            cerr << "StdIn SetHandleInformation failed\n";
            return 1;
        }

        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
            cerr << "StdOut CreatePipe failed\n";
            return 1;
        }

        if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            cerr << "StdOut SetHandleInformation failed\n";
            return 1;
        }

        STARTUPINFOA siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        siStartInfo.hStdOutput = hChildStd_OUT_Wr; 
        siStartInfo.hStdInput = hChildStd_IN_Rd;   
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        char cmdline[256];
        GetModuleFileNameA(NULL, cmdline, sizeof(cmdline));
        string cmd = string(cmdline) + " --child";

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        BOOL success = CreateProcessA(
            NULL,
            cmd.data(),     
            NULL,          
            NULL,           
            TRUE,           
            0,              
            NULL,           
            NULL,           
            &siStartInfo,
            &piProcInfo);

        CloseHandle(hChildStd_IN_Rd);
        CloseHandle(hChildStd_OUT_Wr);

        if (!success) {
            cerr << "CreateProcess failed: " << GetLastError() << "\n";
            return 1;
        }

        childStdInWrite[i] = hChildStd_IN_Wr;   
        childStdOutRead[i] = hChildStd_OUT_Rd;
        procs[i] = piProcInfo;
    }

    for (int i = 0; i < NPROC; ++i) {
        int start = i * CHUNK + 1;
        int end = (i + 1) * CHUNK;
        DWORD written;
        if (!WriteFile(childStdInWrite[i], &start, sizeof(start), &written, NULL)) {
            cerr << "WriteFile failed to child stdin\n";
        }
        if (!WriteFile(childStdInWrite[i], &end, sizeof(end), &written, NULL)) {
            cerr << "WriteFile failed to child stdin\n";
        }
        CloseHandle(childStdInWrite[i]);
    }

    vector<bool> finished(NPROC, false);
    int remaining = NPROC;

    while (remaining > 0) {
        for (int i = 0; i < NPROC; ++i) {
            if (finished[i]) continue;
            int val;
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(childStdOutRead[i], &val, sizeof(val), &bytesRead, NULL);
            if (!ok || bytesRead == 0) {
                // EOF or error
                CloseHandle(childStdOutRead[i]);
                finished[i] = true;
                --remaining;
            } else if (bytesRead == sizeof(val)) {
                cout << val << endl;
            }
        }
    }

    for (int i = 0; i < NPROC; ++i) {
        WaitForSingleObject(procs[i].hProcess, INFINITE);
        CloseHandle(procs[i].hProcess);
        CloseHandle(procs[i].hThread);
    }

    return 0;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--child") == 0) return child_main();
    }
    return parent_main();
}

#else
int main() {
    puts("This is the Windows implementation file. Compile it on Windows.\n");
    return 0;
}
#endif
