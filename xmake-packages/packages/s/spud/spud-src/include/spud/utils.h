#pragma once

#include <cstddef>

// From:
// http://stackoverflow.com/questions/1082192/how-to-generate-random-variable-names-in-c-using-macros/17624752#17624752
//
#define SPUD_PP_CAT(a, b) SPUD_PP_CAT_I(a, b)
#define SPUD_PP_CAT_I(a, b) SPUD_PP_CAT_II(~, a##b)
#define SPUD_PP_CAT_II(p, res) res

#define SPUD_UNUSED(x) (void)x

namespace spud {
void *alloc_executable_memory(size_t size);
void free_executable_memory(void *ptr, size_t size);

// TODO(tashcan): Ideally this should be a scoped thing
void disable_jit_write_protection();
void enable_jit_write_protection();
} // namespace spud
