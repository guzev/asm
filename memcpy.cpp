#include <iostream>
#include <cstdint>
#include <emmintrin.h>

void memcpy_asm(char* dst, const char* src, size_t N) {

    size_t current_position = 0;

    char current_symbol;

    while ((size_t) (current_position + dst) % 16 != 0 && current_position < N) {
        current_symbol = *(src + current_position);
        *(dst + current_position) = current_symbol;
        current_position++;
    }

    size_t remainder = (N - current_position) % 16;

    for (size_t i = current_position; i < N - remainder; i += 16) {
        __m128i registr;

        __asm__ volatile(
        "movdqu\t (%1), %0\n"
        "movntdq\t %0, (%2)\n"
        :"=x"(registr)
        :"r"(src + i), "r"(dst + i)
        :"memory"
        );
    }

    for (size_t i = N - remainder; i < N; i++) {
        *(dst + i) = *(src + i);
    }
}

int main() {

    char* a = new char[36];

    for (int i = 0; i < 6; i++) {
        a[i] = 'a';
    }

    for (int i = 6; i < 36; i++) {
        a[i] = 'b';
    }

    char* b = new char[1024];

    memcpy_asm(b, a, 36);

    std::cout << b;
    return 0;
}
