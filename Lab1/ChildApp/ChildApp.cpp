#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    // Перевірка, чи передано достатньо аргументів
    if (argc < 4) {
        cerr << "Error: Not enough arguments for the child process!" << endl;
        return 1;
    }

    int id = stoi(argv[1]);
    HANDLE hMutex = (HANDLE)stoull(argv[2]);
    HANDLE hSemaphore = (HANDLE)stoull(argv[3]);

    // Очікування дозволу від семафора (ліміт не більше 3 одночасних процесів)
    WaitForSingleObject(hSemaphore, INFINITE);

    // Використовує м'ютекс, щоб запобігти перекриванню виводу консолі
    WaitForSingleObject(hMutex, INFINITE);
    cout << " >>> [Child " << id << "] started working..." << endl;
    ReleaseMutex(hMutex);

    Sleep(2000);

    WaitForSingleObject(hMutex, INFINITE);
    cout << " <<< [Child " << id << "] finished working and is releasing the slot." << endl;
    ReleaseMutex(hMutex);

    // Звільняє слот семафора для наступного процесу
    ReleaseSemaphore(hSemaphore, 1, NULL);

    return 0;
}