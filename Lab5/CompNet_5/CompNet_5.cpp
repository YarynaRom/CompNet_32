#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Launch error. Please specify the process number (1 or 2).\n";
        cout << "Usage example: program.exe 1\n";
        return 1;
    }

    int processId = stoi(argv[1]);

    HANDLE mutex1 = CreateMutexA(NULL, FALSE, "Global\\MyDeadlockMutex1");
    HANDLE mutex2 = CreateMutexA(NULL, FALSE, "Global\\MyDeadlockMutex2");

    if (mutex1 == NULL || mutex2 == NULL) {
        cerr << "Error creating mutexes.\n";
        return 1;
    }

    cout << "Process " << processId << " started\n\n";

    if (processId == 1) {
        cout << "[Process 1]: Trying to acquire Resource 1...\n";
        WaitForSingleObject(mutex1, INFINITE);
        cout << "[Process 1]: Resource 1 acquired!\n";

        cout << "[Process 1]: Working... (sleeping for 5 seconds)\n";
        Sleep(5000);

        cout << "[Process 1]: Trying to acquire Resource 2...\n";
        WaitForSingleObject(mutex2, INFINITE); 
        cout << "[Process 1]: Resource 2 acquired!\n";

        ReleaseMutex(mutex2);
        ReleaseMutex(mutex1);
    }
    else if (processId == 2) {
        cout << "[Process 2]: Trying to acquire Resource 2...\n";
        WaitForSingleObject(mutex2, INFINITE);
        cout << "[Process 2]: Resource 2 acquired!\n";

        cout << "[Process 2]: Working... (sleeping for 5 seconds)\n";
        Sleep(5000);

        cout << "[Process 2]: Trying to acquire Resource 1...\n";
        WaitForSingleObject(mutex1, INFINITE);
        cout << "[Process 2]: Resource 1 acquired!\n";

        ReleaseMutex(mutex1);
        ReleaseMutex(mutex2);
    }
    else {
        cout << "Unknown process number. Please enter 1 or 2.\n";
    }

    cout << "\nProcess " << processId << " successfully finished\n";

    CloseHandle(mutex1);
    CloseHandle(mutex2);

    return 0;
}