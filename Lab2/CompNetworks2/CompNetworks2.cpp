#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

struct ThreadData {
    int mode;
    bool isPositive;
    int* sharedIndex;
    int* sharedArray;
    HANDLE hSemaphore;
    HANDLE hHeap;
};

CRITICAL_SECTION g_cs;
HANDLE g_eventPos;
HANDLE g_eventNeg;

void CheckError(HANDLE handle, const char* errorMessage) {
    if (handle == NULL) {
        cerr << "\n[Error] " << errorMessage << ". Code: " << GetLastError() << endl;
        exit(1);
    }
}

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;

    WaitForSingleObject(data->hSemaphore, INFINITE);

    int* localValue = (int*)HeapAlloc(data->hHeap, HEAP_ZERO_MEMORY, sizeof(int));
    if (localValue == NULL) {
        ReleaseSemaphore(data->hSemaphore, 1, NULL);
        return 1;
    }

    int lineBreakCounter = 0;

    if (data->mode == 0) {
        // БЛОК 1: БЕЗ СИНХРОНІЗАЦІЇ
        for (int i = 1; i <= 500; ++i) {
            *localValue = data->isPositive ? i : -i;
            data->sharedArray[data->sharedIndex[0]++] = *localValue;

            printf("%5d", *localValue);
            if (++lineBreakCounter % 15 == 0) printf("\n");

            Sleep(1);
        }
    }
    else if (data->mode == 1) {
        //  БЛОК 2: ПОДІЯ
        if (data->isPositive) {
            WaitForSingleObject(g_eventPos, INFINITE);
            for (int i = 1; i <= 500; ++i) {
                *localValue = i;
                data->sharedArray[data->sharedIndex[0]++] = *localValue;
                printf("%5d", *localValue);
                if (++lineBreakCounter % 15 == 0) printf("\n");
            }
            SetEvent(g_eventNeg);
        }
        else {
            WaitForSingleObject(g_eventNeg, INFINITE);
            for (int i = 1; i <= 500; ++i) {
                *localValue = -i;
                data->sharedArray[data->sharedIndex[0]++] = *localValue;
                printf("%5d", *localValue);
                if (++lineBreakCounter % 15 == 0) printf("\n");
            }
        }
    }
    else if (data->mode == 2) {
        // БЛОК 3: КРИТИЧНА СЕКЦІЯ
        EnterCriticalSection(&g_cs);
        for (int i = 1; i <= 500; ++i) {
            *localValue = data->isPositive ? i : -i;
            data->sharedArray[data->sharedIndex[0]++] = *localValue;
            printf("%5d", *localValue);
            if (++lineBreakCounter % 15 == 0) printf("\n");
        }
        LeaveCriticalSection(&g_cs);
    }

    HeapFree(data->hHeap, 0, localValue);
    ReleaseSemaphore(data->hSemaphore, 1, NULL);
    return 0;
}

int main() {

    int totalSize = sizeof(int) * 3005;
    HANDLE hFile = CreateFile(L"shared_data.bin", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CheckError(hFile, "CreateFile");

    HANDLE hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, totalSize, L"Local\\MySharedMem");
    CheckError(hMapFile, "CreateFileMapping");

    int* pSharedMem = (int*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if (pSharedMem == NULL) { cout << "Помилка MapViewOfFile: " << GetLastError() << endl; return 1; }
    pSharedMem[0] = 0;

    
    if (!InitializeCriticalSectionAndSpinCount(&g_cs, 4000)) {
        cout << "Помилка CriticalSection: " << GetLastError() << endl; return 1;
    }
    g_eventPos = CreateEvent(NULL, FALSE, FALSE, NULL);
    CheckError(g_eventPos, "CreateEvent Pos");
    g_eventNeg = CreateEvent(NULL, FALSE, FALSE, NULL);
    CheckError(g_eventNeg, "CreateEvent Neg");
    HANDLE hSemaphore = CreateSemaphore(NULL, 2, 2, NULL);
    CheckError(hSemaphore, "CreateSemaphore");

    string headers[3] = {
        "\n\nWithout synch\n",
        "\n\nWith synch (event)\n",
        "\n\nWith synch (critial section)\n"
    };

    for (int mode = 0; mode < 3; ++mode) {
        cout << headers[mode];

        HANDLE hHeapPos = HeapCreate(0, 4096, 0);
        CheckError(hHeapPos, "HeapCreate Pos");
        HANDLE hHeapNeg = HeapCreate(0, 4096, 0);
        CheckError(hHeapNeg, "HeapCreate Neg");

        ThreadData dataPos = { mode, true, pSharedMem, pSharedMem + 1, hSemaphore, hHeapPos };
        ThreadData dataNeg = { mode, false, pSharedMem, pSharedMem + 1, hSemaphore, hHeapNeg };

        HANDLE hThreadPos = CreateThread(NULL, 0, ThreadProc, &dataPos, CREATE_SUSPENDED, NULL);
        CheckError(hThreadPos, "CreateThread Pos");
        HANDLE hThreadNeg = CreateThread(NULL, 0, ThreadProc, &dataNeg, CREATE_SUSPENDED, NULL);
        CheckError(hThreadNeg, "CreateThread Neg");

        SetThreadPriority(hThreadPos, THREAD_PRIORITY_ABOVE_NORMAL);
        SetThreadPriority(hThreadNeg, THREAD_PRIORITY_BELOW_NORMAL);

        if (mode == 1) SetEvent(g_eventPos);

        ResumeThread(hThreadPos);

        if (mode == 0 || mode == 2) Sleep(10);

        ResumeThread(hThreadNeg);

        HANDLE currentPair[2] = { hThreadPos, hThreadNeg };
        WaitForMultipleObjects(2, currentPair, TRUE, INFINITE);

        CloseHandle(hThreadPos);
        CloseHandle(hThreadNeg);
        HeapDestroy(hHeapPos);
        HeapDestroy(hHeapNeg);
    }

    cout << "All threads completed successfully!" << endl;
    cout << "Total numbers written: " << pSharedMem[0] << endl;

    UnmapViewOfFile(pSharedMem);
    CloseHandle(hMapFile);
    CloseHandle(hFile);
    CloseHandle(hSemaphore);
    CloseHandle(g_eventPos);
    CloseHandle(g_eventNeg);
    DeleteCriticalSection(&g_cs);

    return 0;
}