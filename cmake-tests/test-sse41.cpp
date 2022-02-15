#include <immintrin.h>
#include <stdint.h>
int main() {
    __m128i l = _mm_set1_epi32(0);
    return _mm_extract_epi32(l, 3);
}
