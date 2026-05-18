#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm> 

using namespace std;

#define PIPE_PATH "\\\\.\\pipe\\LabNetPipe"
#define MAIN_SLOT "\\\\.\\mailslot\\MainServerSlot"

vector<HANDLE> activeUsers;
mutex syncLock;

// Функція розсилки повідомлень
void sendToAll(const string& text, HANDLE sender) {
    lock_guard<mutex> guard(syncLock);

    for (HANDLE userPipe : activeUsers) {
        if (userPipe == sender) continue;

        DWORD bytesWritten;
        WriteFile(userPipe, text.c_str(), text.length() + 1, &bytesWritten, NULL);
    }
}

// Потік обробки підключеного користувача
void ProcessConnection(HANDLE clientPipe) {
    char msgBuf[1024];
    DWORD readCount;
    DWORD availableData;

    for (;;) {
        if (PeekNamedPipe(clientPipe, NULL, 0, NULL, &availableData, NULL)) {
            if (availableData > 0) {
                BOOL ok = ReadFile(clientPipe, msgBuf, sizeof(msgBuf) - 1, &readCount, NULL);
                if (!ok || readCount == 0) break;

                msgBuf[readCount] = '\0';
                sendToAll(msgBuf, clientPipe);
            }
            else {
                Sleep(45);
            }
        }
        else {
            break;
        }
    }

    {
        lock_guard<mutex> guard(syncLock);
        activeUsers.erase(remove(activeUsers.begin(), activeUsers.end(), clientPipe), activeUsers.end());
    }

    DisconnectNamedPipe(clientPipe);
    CloseHandle(clientPipe);
    cout << "The user has disconnected\n";
}

// Потік для відстеження нових підключень через поштову скриньку
void SlotObserver() {
    HANDLE sHandle = CreateMailslotA(MAIN_SLOT, 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (sHandle == INVALID_HANDLE_VALUE) {
        cout << "Initialization Mailslot error !\n";
        return;
    }

    char tempBuf[256];
    DWORD cbRead;

    for (;;) {
        if (ReadFile(sHandle, tempBuf, sizeof(tempBuf) - 1, &cbRead, NULL)) {
            tempBuf[cbRead] = '\0';
            cout << "Connection request from: " << tempBuf << endl;

            HANDLE outSlot = CreateFileA(
                tempBuf, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
            );

            if (outSlot != INVALID_HANDLE_VALUE) {
                DWORD cbWritten;
                WriteFile(outSlot, PIPE_PATH, strlen(PIPE_PATH) + 1, &cbWritten, NULL);
                CloseHandle(outSlot);
            }
            else {
                cout << "Unable to respond to customer.\n";
            }
        }
    }
}

int main() {
    SetConsoleOutputCP(1251);
    cout << "Server started\n";

    thread observer(SlotObserver);
    observer.detach();

    for (;;) {
        HANDLE pHandle = CreateNamedPipeA(
            PIPE_PATH,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            1024, 1024, 0, NULL
        );

        if (pHandle == INVALID_HANDLE_VALUE) {
            Sleep(150);
            continue;
        }

        BOOL isConnected = ConnectNamedPipe(pHandle, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (isConnected) {
            {
                lock_guard<mutex> guard(syncLock);
                activeUsers.push_back(pHandle);
            }
            thread worker(ProcessConnection, pHandle);
            worker.detach();
        }
        else {
            CloseHandle(pHandle);
        }
    }
    return 0;
}