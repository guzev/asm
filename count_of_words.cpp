//
// Created by Ivan Guzev on 29.09.17.
//

#include <iostream>
#include <cstddef>
#include <cstdint>
#include <xmmintrin.h>
#include <tmmintrin.h>

using namespace std;

int word_count_short(const char* str, size_t size) {
    int result = 0;
    bool is_space = true;

    for (size_t i = 0; i < size; i++) {
        if (is_space && str[i] != ' ')
            result++;
        is_space = (str[i] == ' ');
    }

    return result;
}

int word_count(const char* str, size_t size) {

    if (size <= sizeof(__m128i) * 2)
        return word_count_short(str, size);

    size_t current_position = 0;
    size_t result = 0;
    bool is_space = false;

    while ((size_t)(str + current_position) % 16 != 0) {

        char c = *(str + current_position);
        if (is_space && c != ' ')
            result++;

        is_space = (c == ' ');
        current_position++;
    }

    if (*str != ' ')
        result++;
    if (is_space && *(str + current_position) != ' ' && current_position != 0)
        result++;

    size_t aligned_end = size - (size - current_position) % 16 - 16;
    const __m128i SPACE_BYTES_16 = _mm_set1_epi8(' ');
    __m128i byte_storage = _mm_set1_epi8(0);
    __m128i next_spaces_mask = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + current_position)), SPACE_BYTES_16); // PCMPEQB

    for (size_t i = current_position; i < aligned_end; i += sizeof(__m128i)) {
        __m128i curr_spaces_mask = next_spaces_mask;
        next_spaces_mask = _mm_cmpeq_epi8(_mm_load_si128((__m128i *) (str + i + 16)), SPACE_BYTES_16);
        __m128i shifted_spaces_mask = _mm_alignr_epi8(next_spaces_mask, curr_spaces_mask, 1); // PALIGNR
        __m128i word_beginnings_mask = _mm_andnot_si128(shifted_spaces_mask, curr_spaces_mask); // PANDN
        byte_storage = _mm_adds_epu8(_mm_and_si128(_mm_set1_epi8(1), word_beginnings_mask), byte_storage); // PADDUSB

        if (_mm_movemask_epi8(byte_storage) != 0 || i + 16 >= aligned_end) { // PMOVMSKB
            byte_storage = _mm_sad_epu8(_mm_set1_epi8(0), byte_storage); // PSADBW
            result += _mm_cvtsi128_si32(byte_storage); // MOVD
            byte_storage = _mm_srli_si128(byte_storage, 8); // PSRLDQ
            result += _mm_cvtsi128_si32(byte_storage);
            byte_storage = _mm_set1_epi8(0);
        }
    }

    current_position = aligned_end;

    if (*(str + current_position - 1) == ' ' && *(str + current_position) != ' ')
        result--;

    is_space = *(str + current_position - 1) == ' ';
    for (size_t i = current_position; i < size; i++){
        char cur = *(str + i);
        if (is_space && cur != ' '){
            result++;
        }
        is_space = (cur == ' ');
    }

    return result;

}

int main() {

    const char* test_string = "Java    and   CPP     more    powerful   ";
    int length = 40;
    cout << word_count(test_string, length);
    return 0;
}
