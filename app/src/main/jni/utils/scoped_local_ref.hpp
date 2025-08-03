// modified from lsplant jni_helper.hpp

#pragma once

#include <type_traits>
#include <jni.h>

template <typename T>
concept JObject = std::is_base_of_v<std::remove_pointer_t<_jobject>, std::remove_pointer_t<T>>;

template <JObject T>
class ScopedLocalRef {
public:
    using BaseType [[maybe_unused]] = T;

    inline ScopedLocalRef(JNIEnv *env, T local_ref) : env_(env), local_ref_(nullptr) { reset(local_ref); }

    inline ScopedLocalRef(ScopedLocalRef &&s) noexcept : ScopedLocalRef(s.env_, s.release()) {}

    template <JObject U>
    explicit inline ScopedLocalRef(ScopedLocalRef<U> &&s) noexcept : ScopedLocalRef(s.env_, (T)s.release()) {}

    explicit inline ScopedLocalRef(JNIEnv *env) noexcept : ScopedLocalRef(env, T{nullptr}) {}

    ~ScopedLocalRef() { reset(); }

    inline void reset(T ptr = nullptr) {
        if (ptr != local_ref_) {
            if (local_ref_ != nullptr) {
                env_->DeleteLocalRef(local_ref_);
            }
            local_ref_ = ptr;
        }
    }

    [[nodiscard]] inline T release() {
        T localRef = local_ref_;
        local_ref_ = nullptr;
        return localRef;
    }

    inline T get() const { return local_ref_; }

    ScopedLocalRef<T> clone() const {
        return ScopedLocalRef<T>(env_, (T)env_->NewLocalRef(local_ref_));
    }

    ScopedLocalRef &operator=(ScopedLocalRef &&s) noexcept {
        reset(s.release());
        env_ = s.env_;
        return *this;
    }

    explicit inline operator bool() const { return local_ref_; }

    template <JObject U>
    friend class ScopedLocalRef;

    friend class JUTFString;

private:
    JNIEnv *env_;
    T local_ref_;
    ScopedLocalRef(const ScopedLocalRef &) = delete;
    void operator=(const ScopedLocalRef &) = delete;
};
