
#include <spud/arch.h>
#include <spud/signature.h>

#include <bit>
#include <span>
#include <string_view>
#include <vector>

#include <immintrin.h>
#if SPUD_OS_WIN
#include <intrin.h>
#endif

namespace spud {
namespace detail {

void find_matches_avx2(std::string_view mask, std::string_view data,
                       size_t buffer_end, std::span<uint8_t> search_buffer,
                       std::vector<signature_result> &results) {
  __m256i first = _mm256_set1_epi8(0);
  __m256i second = _mm256_set1_epi8(0);

  const auto signature_size = std::min(mask.size(), size_t(32));

  size_t spread = 0;
  size_t first_i = 0;

  for (size_t i = 0; i < signature_size; ++i) {
    if (mask[i] != '?') {
      first = _mm256_set1_epi8(data[i]);
      first_i = i;
      break;
    }
  }

  for (size_t i = signature_size - 1; i >= 0; --i) {
    if (mask[i] != '?') {
      second = _mm256_set1_epi8(data[i]);
      spread = i - first_i;
      break;
    }
  }

  const auto end = buffer_end - spread - ((buffer_end - spread) % 32);

  for (size_t offset = 0; offset < end; offset += 32) {
    const auto first_block = _mm256_loadu_si256(
        reinterpret_cast<const __m256i *>(search_buffer.data() + offset));
    const auto second_block =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(
            search_buffer.data() + offset + spread));

    const __m256i eq_first = _mm256_cmpeq_epi8(first, first_block);
    const __m256i eq_second = _mm256_cmpeq_epi8(second, second_block);

    uint32_t mm_mask =
        _mm256_movemask_epi8(_mm256_and_si256(eq_first, eq_second));

    while (mm_mask != 0) {
      const auto bit_pos = std::countr_zero(mm_mask);
      if (signature_does_match(offset + bit_pos, mask, data, search_buffer)) {
        results.emplace_back(signature_result{search_buffer, offset + bit_pos});
      }
      mm_mask = mm_mask & (mm_mask - 1);
    }
  }

  // Scan the remainder
  for (size_t offset = end; offset < buffer_end; ++offset) {
    if (signature_does_match(offset, mask, data, search_buffer)) {
      results.emplace_back(signature_result{search_buffer, offset});
    }
  }
}

} // namespace detail
} // namespace spud
