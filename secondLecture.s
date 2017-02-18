ret = __builtin_ia32_crc32di(ret, buf[i]);

__asm__("crc32q\t" "(%1), %0"
: "=r" (ret)
: "r"(buf+i), "0"(ret));

ret = _mm_crc32_u64(ret, buf[i]);
