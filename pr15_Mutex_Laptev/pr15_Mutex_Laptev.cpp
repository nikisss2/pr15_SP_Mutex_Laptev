#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <conio.h>

using namespace std;

struct BankAccount
{
    int accountNumber;
    int balance;
};

bool readAccount(BankAccount& acc) //чтение
{
    ifstream file("account.bin", ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    file.read((char*)&acc, sizeof(BankAccount));
    file.close();
    return true;
}

bool writeAccount(const BankAccount& acc) //сохранение
{
    ofstream file("account.bin", ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    file.write((char*)&acc, sizeof(BankAccount));
    file.close();
    return true;
}

//запись
void logOperation(int initialBalance, string opType, int amount, int finalBalance, string status)
{
    ofstream logFile("operations.log", ios::app);
    if (logFile.is_open())
    {
        logFile << GetCurrentProcessId() << ", " << initialBalance << ", " << opType << ", " << amount << ", " << finalBalance << ", " << status << endl;
        logFile.close();
    }
}

bool lockAccount(HANDLE hMutex)
{
    DWORD waitResult = WaitForSingleObject(hMutex, 3000);

    if (waitResult == WAIT_OBJECT_0)
    {
        return true;
    }
    if (waitResult == WAIT_ABANDONED)
    {
        cout << "Предыдущий процесс завершился, проверяем данные" << endl;
        return true;
    }
    if (waitResult == WAIT_TIMEOUT)
    {
        cout << "Превышено время ожидания" << endl;
        return false;
    }

    cout << "Ошибка ожидания мьютекса" << endl;
    return false;
}

void unlockAccount(HANDLE hMutex)
{
    ReleaseMutex(hMutex);
}

//баланс
void showBalance(HANDLE hMutex)
{
    if (!lockAccount(hMutex)) return;

    BankAccount acc;
    if (readAccount(acc))
    {
        cout << "Номер счета: " << acc.accountNumber << ", Текущий баланс: " << acc.balance << " руб." << endl;
    }
    else
    {
        cout << "Ошибка чтения файла" << endl;
    }

    unlockAccount(hMutex);
}

//пополнение
void deposit(HANDLE hMutex, int amount)
{
    if (amount <= 0)
    {
        cout << "Сумма пополнения должна быть больше 0" << endl;
        return;
    }

    if (!lockAccount(hMutex)) return;

    BankAccount acc;
    if (readAccount(acc))
    {
        int initial = acc.balance;

        acc.balance += amount;
        writeAccount(acc);

        cout << "PID: " << GetCurrentProcessId() << ", Начальный: " << initial << ", пополнение: +" << amount << ", баланс: " << acc.balance << endl;
        logOperation(initial, "Пополнение", amount, acc.balance, "успешно");
    }

    unlockAccount(hMutex);
}

//снятие со счета
void withdraw(HANDLE hMutex, int amount)
{
    if (amount <= 0)
    {
        cout << "Сумма снятия должна быть больше 0" << endl;
        return;
    }

    if (!lockAccount(hMutex)) return;

    BankAccount acc;
    if (readAccount(acc))
    {
        int initial = acc.balance;

        if (acc.balance < amount)
        {
            cout << "PID: " << GetCurrentProcessId() << ", Начальный: " << initial << ", снятие: -" << amount << ", недостаточно средств" << endl;
            logOperation(initial, "Снятие", amount, initial, "мало средства");
        }
        else
        {
            acc.balance -= amount;
            writeAccount(acc);

            cout << "PID: " << GetCurrentProcessId() << ", Начальный: " << initial << ", снятие: -" << amount << ", баланс: " << acc.balance << endl;
            logOperation(initial, "Снятие", amount, acc.balance, "успешно");
        }
    }

    unlockAccount(hMutex);
}

//поток
DWORD WINAPI AutoOperationsThread(LPVOID param)
{
    HANDLE hMutex = (HANDLE)param;
    cout << "\nЗапуск 20 автоматических операций" << endl;

    for (int i = 0; i < 20; i++)
    {
        int amount = 100 + rand() % 901;
        int op = rand() % 2;

        if (op == 0)
        {
            deposit(hMutex, amount);
        }
        else
        {
            withdraw(hMutex, amount);
        }

        int pause = 100 + rand() % 401;
        Sleep(pause);
    }

    cout << "20 операций завершены\n" << endl;
    return 0;
}

int main()
{
    setlocale(LC_ALL, "Russian");

    HANDLE hMutex = CreateMutexA(NULL, FALSE, "Local\\LaptevMutex");
    if (hMutex == NULL)
    {
        cout << "Не удалось создать мьютекс" << endl;
        _getch();
        return 0;
    }

    if (lockAccount(hMutex))
    {
        ifstream testFile("account.bin", ios::binary);
        if (!testFile.is_open())
        {
            BankAccount defaultAcc;
            defaultAcc.accountNumber = 10001;
            defaultAcc.balance = 10000;
            writeAccount(defaultAcc);
            cout << "Создан новый файл счета 10001 с балансом 10000 руб." << endl;
        }
        else
        {
            testFile.close();
        }
        unlockAccount(hMutex);
    }

    int choice = -1;
    while (choice != 0)
    {
        cout << "\nБанковский терминал (PID: " << GetCurrentProcessId() << ")" << endl;
        cout << "1. Просмотр баланса" << endl;
        cout << "2. Пополнение счета" << endl;
        cout << "3. Снятие средств" << endl;
        cout << "4. Автоматические операции (20 операций)" << endl;
        cout << "0. Выход" << endl;
        cout << "Выбор: ";
        cin >> choice;

        if (choice == 1)
        {
            showBalance(hMutex);
        }
        else if (choice == 2)
        {
            int sum = 0;
            cout << "Введите сумму пополнения: ";
            cin >> sum;
            deposit(hMutex, sum);
        }
        else if (choice == 3)
        {
            int sum = 0;
            cout << "Введите сумму для снятия: ";
            cin >> sum;
            withdraw(hMutex, sum);
        }
        else if (choice == 4)
        {
            HANDLE hThread = CreateThread(NULL, 0, AutoOperationsThread, hMutex, 0, NULL);
            if (hThread == NULL)
            {
                cout << "Не удалось создать поток" << endl;
            }
            else
            {
                WaitForSingleObject(hThread, INFINITE);
                CloseHandle(hThread);
            }
        }
    }

    CloseHandle(hMutex);
    return 0;
}