#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char* argv[]) {
    // 1. Перевірка наявності унікального екземпляра
    HANDLE hSingleInstance = CreateMutexA(NULL, TRUE, "Global\\MyUniqueAppMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        cout << "Another instance of the program is already running. Exiting..." << endl;
        if (hSingleInstance) CloseHandle(hSingleInstance);
        return 0;
    }

    // Налаштовування спадщини
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // 2. Створення об’єктів синхронізації
    HANDLE hAnonMutex = CreateMutexA(&sa, FALSE, NULL);
    HANDLE hSemaphore = CreateSemaphoreA(&sa, 3, 3, NULL);

    const int numProcesses = 10;
    HANDLE processHandles[numProcesses];

    cout << "Parent: Starting 10 child processes..." << endl;

    // 3.Запуск 10 дочірніх процесів
    for (int i = 0; i < numProcesses; i++) {
        // WARNING: Make sure the child executable is named "ChildApp.exe"
        string cmdLine = "ChildApp.exe " + to_string(i) + " "
            + to_string((unsigned long long)hAnonMutex) + " "
            + to_string((unsigned long long)hSemaphore);

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        // Перетворити рядок на змінний буфер, як вимагає CreateProcessA
        vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back('\0');

        if (CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            processHandles[i] = pi.hProcess;
            CloseHandle(pi.hThread);
        }
        else {
            cerr << "Error creating process " << i << ". Code: " << GetLastError() << endl;
            processHandles[i] = NULL;
        }
    }

    // 4. Wait 5 seconds using a Waitable Timer
    HANDLE hTimer = CreateWaitableTimerA(NULL, TRUE, NULL);
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -50000000LL; // 5 seconds
    SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);

    cout << "\nParent: Waiting 5 seconds via Waitable Timer..." << endl;
    WaitForSingleObject(hTimer, INFINITE);

    // 5.Остаточна перевірка станів дочірніх процесів
    cout << "\nProcess status report (after 5 sec):" << endl;
    for (int i = 0; i < numProcesses; i++) {
        if (processHandles[i] == NULL) continue;

        DWORD res = WaitForSingleObject(processHandles[i], 0);
        if (res == WAIT_OBJECT_0) {
            cout << "Process [" << i << "]: FINISHED" << endl;
        }
        else {
            cout << "Process [" << i << "]: STILL RUNNING" << endl;
        }
        CloseHandle(processHandles[i]);
    }

    CloseHandle(hAnonMutex);
    CloseHandle(hSemaphore);
    CloseHandle(hTimer);
    CloseHandle(hSingleInstance);

    cout << "\nParent: Work completed." << endl;
    system("pause");
    return 0;
}