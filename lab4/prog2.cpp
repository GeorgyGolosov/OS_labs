#include <iostream>
#include <dlfcn.h>

int main() {
    int prog = 1;
    int real = 1;
    void *lib = nullptr;

    typedef float (*DerivativeFunc)(float, float);
    typedef char* (*TranslationFunc)(long);

    DerivativeFunc Derivative;
    TranslationFunc translation;

    // Начальная загрузка объединённой библиотеки для реализации 1
    lib = dlopen("./libimpl1.so", RTLD_LAZY);
    if (!lib) {
        std::cerr << "Error loading initial library: " << dlerror() << std::endl;
        return 1;
    }
    std::cout << "Library is loaded\n";

    Derivative = (DerivativeFunc) dlsym(lib, "Derivative");
    translation = (TranslationFunc) dlsym(lib, "translation");
    if (!Derivative || !translation) {
        std::cerr << "Failed to load symbols: " << dlerror() << std::endl;
        dlclose(lib);
        return 1;
    }

    while (true) {
        std::cout << "Input program code:\n 0 -> Library switch\n 1 -> Calculate derivative\n 2 -> Translation\n-1 -> Exit\n";
        std::cin >> prog;
        switch (prog) {
            case 0:
                dlclose(lib); // Закрываем текущую библиотеку
                if (real == 1) {
                    lib = dlopen("./libimpl2.so", RTLD_LAZY);
                    real = 2;
                } else {
                    lib = dlopen("./libimpl1.so", RTLD_LAZY);
                    real = 1;
                }
                if (!lib) {
                    std::cerr << "Error loading library: " << dlerror() << std::endl;
                    return 1;
                }
                std::cout << "Library switched successfully!\n";
                // Перезагружаем символы
                Derivative = (DerivativeFunc) dlsym(lib, "Derivative");
                translation = (TranslationFunc) dlsym(lib, "translation");
                if (!Derivative || !translation) {
                    std::cerr << "Failed to load symbols: " << dlerror() << std::endl;
                    dlclose(lib);
                    return 1;
                }
                break;
            case 1: {
                float A, deltaX;
                std::cout << "Enter A and deltaX: ";
                std::cin >> A >> deltaX;
                if (real == 1)
                    std::cout << "Calculating derivative using first method\n";
                else
                    std::cout << "Calculating derivative using second method\n";
                std::cout << "Derivative: " << Derivative(A, deltaX) << "\n\n";
                break;
            }
            case 2: {
                long x;
                std::cout << "Enter x: ";
                std::cin >> x;
                if (real == 1)
                    std::cout << "Translating to binary\n";
                else
                    std::cout << "Translating to ternary\n";
                std::cout << "Result is: " << translation(x) << "\n\n";
                break;
            }
            case -1:
                std::cout << "Exit\n";
                dlclose(lib);
                return 0;
            default:
                std::cout << "Invalid input. Try again.\n";
        }
    }
}
