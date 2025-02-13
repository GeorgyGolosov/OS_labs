#include <iostream>

extern "C" float Derivative(float A, float deltaX);
extern "C" char* translation(long x);

int main() {
    int prog;
    while (true) {
        std::cout << "Input program code:\n 1 -> Calculate derivative\n 2 -> Translation\n-1 -> Exit\n";
        std::cin >> prog;
        switch (prog) {
            case 1: {
                std::cout << "Enter A and deltaX: ";
                float A, deltaX;
                std::cin >> A >> deltaX;
                std::cout << "Calculated derivative: " << Derivative(A, deltaX) << "\n\n";
                break;
            }
            case 2: {
                long x;
                std::cout << "Enter x: ";
                std::cin >> x;
                std::cout << "Translated number: " << translation(x) << "\n\n";
                break;
            }
            case -1:
                std::cout << "Exit\n";
                return 0;
            default:
                std::cout << "Invalid input. Try again.\n";
        }
    }
}
