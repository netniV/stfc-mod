#include <spud/arch.h>
#include <spud/signature.h>

#include <bit>
#include <span>
#include <string_view>
#include <vector>

#include <arm_neon.h>

namespace spud {
namespace detail {

void find_matches_neon(std::string_view mask, std::string_view data,
                       size_t buffer_end, std::span<uint8_t> search_buffer,
                       std::vector<signature_result> &results) {
  uint8x16_t first = vdupq_n_u8(0);
  uint8x16_t second = vdupq_n_u8(0);

  const auto signature_size = std::min(mask.size(), size_t(16));

  size_t spread = 0;
  size_t first_i = 0;

  for (size_t i = 0; i < signature_size; ++i) {
    if (mask[i] != '?') {
      first = vdupq_n_u8(static_cast<uint8_t>(data[i]));
      first_i = i;
      break;
    }
  }

  for (size_t i = signature_size - 1; i >= 0; --i) {
    if (mask[i] != '?') {
      second = vdupq_n_u8(static_cast<uint8_t>(data[i]));
      spread = i - first_i;
      break;
    }
  }

  const auto end = buffer_end - spread - ((buffer_end - spread) % 16);

  for (size_t offset = 0; offset < end; offset += 16) {
    const uint8x16_t first_block = vld1q_u8(search_buffer.data() + offset);
    const uint8x16_t second_block = vld1q_u8(search_buffer.data() + offset + spread);

    const uint8x16_t eq_first = vceqq_u8(first, first_block);
    const uint8x16_t eq_second = vceqq_u8(second, second_block);

    const uint8x16_t input = vandq_u8(eq_first, eq_second);
    const uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(input, 7));
    const uint32x4_t paired16 =
        vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));
    const uint64x2_t paired32 =
        vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));
    const uint8x16_t paired64 =
        vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));
    uint32_t mm_mask =
        vgetq_lane_u8(paired64, 0) | ((int)vgetq_lane_u8(paired64, 8) << 8);

    while (mm_mask != 0) {
      const auto bit_pos = std::countr_zero(mm_mask);
      if (signature_does_match(offset + bit_pos, mask, data, search_buffer)) {
        results.emplace_back(
            signature_result{search_buffer, uintptr_t(offset + bit_pos)});
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
