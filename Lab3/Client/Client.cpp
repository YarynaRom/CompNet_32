#include <windows.h>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

#define SERVER_ANNOUNCE_SLOT "\\\\.\\mailslot\\MainServerSlot"

string targetPipe;

void backgroundReader(HANDLE pipeHandle) {
    char recvBuf[1024];
    DWORD readBytes;
    DWORD dataAvailable;

    HANDLE consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);

    for (;;) {
        if (PeekNamedPipe(pipeHandle, NULL, 0, NULL, &dataAvailable, NULL)) {
            if (dataAvailable > 0) {
                if (ReadFile(pipeHandle, recvBuf, sizeof(recvBuf) - 1, &readBytes, NULL) && readBytes > 0) {
                    recvBuf[readBytes] = '\0';

                    CONSOLE_SCREEN_BUFFER_INFO info;
                    GetConsoleScreenBufferInfo(consoleOut, &info);

                    COORD startPos = info.dwCursorPosition;
                    startPos.X = 0;

                    DWORD charsWritten;
                    FillConsoleOutputCharacterA(consoleOut, ' ', info.dwSize.X, startPos, &charsWritten);

                    SetConsoleCursorPosition(consoleOut, startPos);
                    cout << recvBuf << "\n>>> " << flush;
                }
            }
            else {
                Sleep(40);
            }
        }
        else {
            cout << "\n[CONNECTION LOST]\n";
            exit(1);
        }
    }
}

int main() {
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);

    string myName;
    cout << "Enter your nickname: ";
    getline(cin, myName);

    string mySlotPath = "\\\\.\\mailslot\\UserSlot_" + myName;
    HANDLE localSlot = CreateMailslotA(mySlotPath.c_str(), 0, MAILSLOT_WAIT_FOREVER, NULL);
    if (localSlot == INVALID_HANDLE_VALUE) {
        cout << "Error creating mailbox!\n";
        return 1;
    }

    HANDLE servSlot = CreateFileA(SERVER_ANNOUNCE_SLOT, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (servSlot == INVALID_HANDLE_VALUE) {
        cout << "Server not found. Check if it is up and running..\n";
        return 1;
    }

    DWORD wroteBytes;
    WriteFile(servSlot, mySlotPath.c_str(), mySlotPath.size() + 1, &wroteBytes, NULL);
    CloseHandle(servSlot);

    char addrBuf[512];
    DWORD readCnt, sizeNext, msgAmount;

    for (;;) {
        BOOL status = GetMailslotInfo(localSlot, NULL, &sizeNext, &msgAmount, NULL);
        if (!status) return 1;

        if (msgAmount > 0) break;

        cout << "Waiting for a response from the server...\n";
        Sleep(600);
    }

    ReadFile(localSlot, addrBuf, sizeof(addrBuf) - 1, &readCnt, NULL);
    addrBuf[readCnt] = '\0';
    targetPipe = addrBuf;

    cout << "Network found: " << targetPipe << endl;
    CloseHandle(localSlot);

    HANDLE commPipe;
    for (;;) {
        commPipe = CreateFileA(targetPipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (commPipe != INVALID_HANDLE_VALUE) break;

        if (GetLastError() != ERROR_PIPE_BUSY) return 1;
        if (!WaitNamedPipeA(targetPipe.c_str(), 5000)) return 1;
    }

    DWORD pipeMode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(commPipe, &pipeMode, NULL, NULL);

    cout << "Successfully connected\n>>> ";

    thread reader(backgroundReader, commPipe);
    reader.detach();

    string outText;

    for (;;) {
        getline(cin, outText);

        if (outText == "exit" || outText.empty()) break;

        string finalMsg = "[" + myName + "]: " + outText;

        WriteFile(commPipe, finalMsg.c_str(), finalMsg.size() + 1, &wroteBytes, NULL);
        cout << ">>> " << flush;
    }

    CloseHandle(commPipe);
    return 0;
}