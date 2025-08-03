#pragma once

#include <cstdint>

struct CodeItem {
    CodeItem() = default;

    uint16_t registers_size_;            // the number of registers used by this code
    //   (locals + parameters)
    uint16_t ins_size_;                  // the number of words of incoming arguments to the method
    //   that this code is for
    uint16_t outs_size_;                 // the number of words of outgoing argument space required
    //   by this code for method invocation
    uint16_t tries_size_;                // the number of try_items for this instance. If non-zero,
    //   then these appear as the tries array just after the
    //   insns in this instance.
    uint32_t debug_info_off_;            // Holds file offset to debug info stream.

    uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
    uint16_t insns_[1];                  // actual array of bytecode.
};