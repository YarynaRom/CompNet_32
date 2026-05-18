#include <windows.h>
#include <iostream>
#include <vector>

using namespace std;

vector<double> coeffs = { 1.0, 2.0, 3.0, 4.0, 5.0 };
const int num_values = 5;

HANDLE hPipe1Read, hPipe1Write;
HANDLE hPipe2Read, hPipe2Write;

HANDLE hPrintMutex;

HANDLE hEventT1Started;
HANDLE hEventT2Started;

// Функція обчислення за схемою Горнера
double horner(const vector<double>& a, double x) {
    double result = a[0];
    for (size_t i = 1; i < a.size(); i++) {
        result = result * x + a[i];
    }
    return result;
}

// Генератор
DWORD WINAPI GeneratorThread(LPVOID lpParam) {
    SetEvent(hEventT1Started);

    double x_values[] = { 1.0, 2.0, -1.0, 0.5, 0.0 };

    for (int i = 0; i < num_values; ++i) {
        DWORD bytesWritten;
        WriteFile(hPipe1Write, &x_values[i], sizeof(double), &bytesWritten, NULL);
        Sleep(100);
    }
    return 0;
}

// Обчислювач
DWORD WINAPI CalculatorThread(LPVOID lpParam) {
    SetEvent(hEventT2Started);

    for (int i = 0; i < num_values; ++i) {
        double x;
        DWORD bytesRead, bytesWritten;

        ReadFile(hPipe1Read, &x, sizeof(double), &bytesRead, NULL);

        double y = horner(coeffs, x);

        WriteFile(hPipe2Write, &y, sizeof(double), &bytesWritten, NULL);
    }

    WaitForSingleObject(hEventT1Started, INFINITE);

    return 0;
}

// Споживач
DWORD WINAPI ConsumerThread(LPVOID lpParam) {
    for (int i = 0; i < num_values; ++i) {
        double y;
        DWORD bytesRead;

        ReadFile(hPipe2Read, &y, sizeof(double), &bytesRead, NULL);

        WaitForSingleObject(hPrintMutex, INFINITE);
        cout << "Pipeline result " << i + 1 << ": P(x) = " << y << endl;
        ReleaseMutex(hPrintMutex);
    }

    WaitForSingleObject(hEventT2Started, INFINITE);

    return 0;
}

int main() {
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hPipe1Read, &hPipe1Write, &sa, 0) ||
        !CreatePipe(&hPipe2Read, &hPipe2Write, &sa, 0)) {
        cerr << "Error creating pipes!" << endl;
        return 1;
    }

    hPrintMutex = CreateMutex(NULL, FALSE, NULL);
    hEventT1Started = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventT2Started = CreateEvent(NULL, TRUE, FALSE, NULL);

    cout << "Starting multithreaded pipeline (WinAPI, Variant 11)..." << endl;

    HANDLE hThreads[3];
    hThreads[0] = CreateThread(NULL, 0, GeneratorThread, NULL, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, CalculatorThread, NULL, 0, NULL);
    hThreads[2] = CreateThread(NULL, 0, ConsumerThread, NULL, 0, NULL);

    WaitForMultipleObjects(3, hThreads, TRUE, INFINITE);

    CloseHandle(hPipe1Read); CloseHandle(hPipe1Write);
    CloseHandle(hPipe2Read); CloseHandle(hPipe2Write);
    CloseHandle(hPrintMutex);
    CloseHandle(hEventT1Started); CloseHandle(hEventT2Started);
    for (int i = 0; i < 3; ++i) CloseHandle(hThreads[i]);

    cout << "Pipeline finished successfully!" << endl;

    return 0;
}