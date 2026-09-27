#pragma once
// core/Handle.h — handle generacional {index, generation}.
// index: slot no storage (free-list do Scene); generation: versão do slot.
// destroy() avança a generation do slot — handles antigos ficam inválidos
// e nunca são reutilizados silenciosamente.
#include "Types.h"

namespace vv {

struct Handle {
    static constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

    u32 index      = kInvalidIndex;
    u32 generation = 0;   // 0 = handle nulo/inválido por definição

    static constexpr Handle invalid() { return Handle{kInvalidIndex, 0u}; }

    constexpr bool valid() const { return index != kInvalidIndex && generation != 0u; }

    constexpr bool operator==(const Handle& o) const {
        return index == o.index && generation == o.generation;
    }
    constexpr bool operator!=(const Handle& o) const { return !(*this == o); }
};

} // namespace vv
