#pragma once

#include <stddef.h>
#include <algorithm>
#include <concepts>

#include "xdl.h"

namespace Sym {

void *Resolve(void *handle, const char *symbol) {
    return xdl_dsym(handle, symbol, nullptr) ?: xdl_sym(handle, symbol, nullptr);
}
void *ResolveArt(const char *symbol) {
    static const void *sLibArtHandle = xdl_open("libart.so", XDL_DEFAULT);
    return Resolve(const_cast<void *>(sLibArtHandle), symbol);
}
void *ResolveArt(std::string &symbol) {
    return ResolveArt(symbol.c_str());
}

template <size_t N>
struct FixedString {
    consteval FixedString(const char (&str)[N]) { std::copy_n(str, N, data); }
    char data[N] = {};
};

template<typename T>
concept FuncType = std::is_function_v<T> || std::is_member_function_pointer_v<T>;

template <FixedString, FuncType>
struct ArtFunction;

template <FixedString Symbol, typename Ret, typename... Args>
struct ArtFunction<Symbol, Ret(Args...)> {
    [[gnu::always_inline]] static Ret operator()(Args... args) {
        return inner_.function_(args...);
    }
    [[gnu::always_inline]] explicit operator bool() { return inner_.raw_function_; }
    [[gnu::always_inline]] auto operator&() const { return inner_.function_; }
    [[gnu::always_inline]] ArtFunction &operator=(void *function) {
        inner_.raw_function_ = function;
        return *this;
    }

    ArtFunction() {
        inner_.raw_function_ = ResolveArt(Symbol.data);
    }

private:
    inline static union {
        Ret (*function_)(Args...);
        void *raw_function_ = nullptr;
    } inner_;

    static_assert(sizeof(inner_.function_) == sizeof(inner_.raw_function_));
};

    template<FixedString S>
struct ArtSymbol {
    template<typename T>
    inline static decltype([]{
        return ArtFunction<S, T>{};
    }()) as{};
};

} // namespace Sym

template <Sym::FixedString S> constexpr Sym::ArtSymbol<S> operator ""_sym() {
    return {};
}