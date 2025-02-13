#include <cmath>

extern "C" float Derivative(float A, float deltaX) {
    return (cos(A + deltaX) - cos(A)) / deltaX;
}
