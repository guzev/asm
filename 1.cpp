#include <cstdint>

// first task

// как подсчитать кол-во битов 
// how to count one in binary number

uint32_t pop_count(uint32_t a) {
    a = (a & 0x55555555) + ((a & aaaaaaaa) >> 1);
    a = (a & 0x33333333) + ((a & cccccccc) >> 2);
    a = (a & 0x0f0f0f0f) + ((a & f0f0f0f0) >> 4);
    a = (a & 0x00ff00ff) + ((a & ff00ff00) >> 8);
    a = (a & 0x0000ffff) + ((a & ffff0000) >> 16);
}

//  a                 abcdefgh
//  a & 0x55          0b0d0f0h
//  a & 0xaa          a0c0e0g0
//  (a & 0x55) >> 1   0a0c0e0g

// end first task

//second task 
//
