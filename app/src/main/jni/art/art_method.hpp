#pragma once

#include "../utils/sym_resolver.hpp"
#include "code_item.hpp"

namespace ArtMethod {
    auto JniShortName = "_ZN3art9ArtMethod12JniShortNameEv"_sym.as<std::string(void *method)>;
    auto PrettyMethod = "_ZN3art9ArtMethod12PrettyMethodEb"_sym.as<std::string(void *method, bool with_signature)>;
    auto GetCodeItem = "_ZN3art9ArtMethod11GetCodeItemEv"_sym.as<CodeItem *(void *method)>;
} // namespace ArtMethod