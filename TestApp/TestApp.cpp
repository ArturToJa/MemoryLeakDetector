#include <iostream>
#include <new>

struct alignas(64) AlignedData
{
    int value;
};

int main()
{
    std::cout << "========== TEST START ==========\n";

    // 1. zwykły new / delete
    {
        int* p = new int;
        *p = 42;
        delete p;
    }

    // 2. zwykły new[] / delete[]
    {
        int* p = new int[100];
        p[0] = 42;
        delete[] p;
    }

    // 3. new / brak delete -> LEAK
    {
        int* p = new int;
        *p = 123;
    }

    // 4. new[] / brak delete[] -> LEAK
    {
        int* p = new int[50];
        p[0] = 123;
    }

    // 5. delete nullptr
    {
        int* p = nullptr;
        delete p;
    }

    // 6. delete[] nullptr
    {
        int* p = nullptr;
        delete[] p;
    }

    // 7. nothrow new / delete
    {
        int* p = new (std::nothrow) int;

        if (p)
        {
            *p = 42;
            delete p;
        }
    }

    // 8. aligned new / delete
    {
        AlignedData* p = new AlignedData;
        p->value = 42;
        delete p;
    }

    // 9. aligned new -> LEAK
    {
        AlignedData* p = new AlignedData;
        p->value = 123;
    }

    std::cout << "=========== TEST END ===========\n";

    return 0;
}