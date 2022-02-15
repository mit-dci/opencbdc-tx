#include <immintrin.h>
#include <stdint.h>

int main() {
    __m256i l = _mm256_set1_epi32(0);
    return _mm256_extract_epi32(l, 7);
}
