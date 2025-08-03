# Runtime Patching of ART Method Code: Implementation Strategy

## Class Verification Process Overview

### Step 1: ClassLinker::VerifyClass

Symbol: `_ZN3art11ClassLinker11VerifyClassEPNS_6ThreadEPNS_8verifier12VerifierDepsENS_6HandleINS_6mirror5ClassEEENS3_15HardFailLogModeE`

```cpp
verifier::FailureKind ClassLinker::VerifyClass(Thread* self,
                                               verifier::VerifierDeps* verifier_deps,
                                               Handle<mirror::Class> klass,
                                               verifier::HardFailLogMode log_level) {
    // ...

    // Skip verification if disabled.
    if (!Runtime::Current()->IsVerificationEnabled()) {
      mirror::Class::SetStatus(klass, ClassStatus::kVerified, self);
      UpdateClassAfterVerification(klass, image_pointer_size_, verifier::FailureKind::kNoFailure);
      return verifier::FailureKind::kNoFailure;
    }

    // ... (verification logic) ...

    UpdateClassAfterVerification(klass, image_pointer_size_, verifier_failure);
    return verifier_failure;
}
```

### Step 2: ClassLinker::UpdateClassAfterVerification

* Inlined

```cpp
static void UpdateClassAfterVerification(Handle<mirror::Class> klass,
                                         PointerSize pointer_size,
                                         verifier::FailureKind failure_kind)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // ...

  // Now that the class has passed verification, try to set nterp entrypoints
  // to methods that currently use the switch interpreter.
  if (interpreter::CanRuntimeUseNterp()) {
    for (ArtMethod& m : klass->GetMethods(pointer_size)) {
      if (class_linker->IsQuickToInterpreterBridge(m.GetEntryPointFromQuickCompiledCode())) {
        runtime->GetInstrumentation()->InitializeMethodsCode(&m); // Or ReinitializeMethodsCode in newer versions
      }
    }
  }
}
```

### Step 3: Instrumentation::InitializeMethodsCode

* Symbol: `_ZN3art15instrumentation15Instrumentation21InitializeMethodsCodeEPNS_9ArtMethodEPKv` or newer `_ZN3art15instrumentation15Instrumentation23ReinitializeMethodsCodeEPNS_9ArtMethodE`

* Note:
Android now uses `ReinitializeMethodsCode` in newer versions (probably A16+).
([View the change](https://android-review.googlesource.com/c/platform/art/+/3489795))

```cpp
void Instrumentation::InitializeMethodsCode(ArtMethod* method) {
    // ...
}

// Or newer
void Instrumentation::ReinitializeMethodsCode(ArtMethod* method) {
    // ...
}
```

**Why this is ideal**:  
Hooking this function provides direct access to `ArtMethod*` instances during class loading, enabling safe runtime code modification before method execution begins.

---

## Method Identification Techniques

### Key ArtMethod Accessors

```cpp
class EXPORT ArtMethod final {
    public:
        // ...
        const DexFile* GetDexFile() REQUIRES_SHARED(Locks::mutator_lock_); // * Inlined

        const char* GetDeclaringClassDescriptor() REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod27GetDeclaringClassDescriptorEv
        std::string_view GetDeclaringClassDescriptorView() REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod31GetDeclaringClassDescriptorViewEv

        ALWAYS_INLINE const char* GetShorty() REQUIRES_SHARED(Locks::mutator_lock_); // * Inlined

        const char* GetShorty(uint32_t* out_length) REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod9GetShortyEPj

        std::string_view GetShortyView() REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod13GetShortyViewEv

        const Signature GetSignature() REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod12GetSignatureEv

        ALWAYS_INLINE const char* GetName() REQUIRES_SHARED(Locks::mutator_lock_); // * Inlined

        ALWAYS_INLINE std::string_view GetNameView() REQUIRES_SHARED(Locks::mutator_lock_); // * Inlined

        ObjPtr<mirror::String> ResolveNameString() REQUIRES_SHARED(Locks::mutator_lock_); // * Inlined

        bool NameEquals(ObjPtr<mirror::String> name) REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod10NameEqualsENS_6ObjPtrINS_6mirror6StringEEE

        const dex::CodeItem* GetCodeItem() REQUIRES_SHARED(Locks::mutator_lock_); // _ZN3art9ArtMethod11GetCodeItemEv
        // ...

        // Returns a human-readable signature for 'm'. Something like "a.b.C.m" or
        // "a.b.C.m(II)V" (depending on the value of 'with_signature').
        static std::string PrettyMethod(ArtMethod* m, bool with_signature = true)        // _ZN3art9ArtMethod12PrettyMethodEPS0_b
            REQUIRES_SHARED(Locks::mutator_lock_);
        std::string PrettyMethod(bool with_signature = true)                             // _ZN3art9ArtMethod12PrettyMethodEb
            REQUIRES_SHARED(Locks::mutator_lock_);
        // Returns the JNI native function name for the non-overloaded method 'm'.
        std::string JniShortName()                                                      // _ZN3art9ArtMethod12JniShortNameEv
            REQUIRES_SHARED(Locks::mutator_lock_);
        // Returns the JNI native function name for the overloaded method 'm'.
        std::string JniLongName()                                                      // _ZN3art9ArtMethod11JniLongNameEv
            REQUIRES_SHARED(Locks::mutator_lock_);
```

### CodeItem Structure Essentials

Base:

```cpp
// Base code_item, compact dex and standard dex have different code item layouts.
struct CodeItem {
    protected:
    CodeItem() = default;

    private:
    DISALLOW_COPY_AND_ASSIGN(CodeItem);
};
```

Extended:

* Compat DEX file (at `art/libdexfile/dex/compact_dex_file.h`)

```cpp
// CompactDex is a currently ART internal dex file format that aims to reduce storage/RAM usage.
class CompactDexFile : public DexFile {
public:
    static constexpr uint8_t kDexMagic[kDexMagicSize] = { 'c', 'd', 'e', 'x' };
    // Last change: remove code item deduping.
    static constexpr uint8_t kDexMagicVersion[] = {'0', '0', '1', '\0'};

    // ...

    // Like the standard code item except without a debug info offset. Each code item may have a
    // preheader to encode large methods. In 99% of cases, the preheader is not used. This enables
    // smaller size with a good fast path case in the accessors.
    struct CodeItem : public dex::CodeItem {
        // ...

      private:
        // Packed code item data, 4 bits each: [registers_size, ins_size, outs_size, tries_size]
        uint16_t fields_;

        // 5 bits for if either of the fields required preheader extension, 11 bits for the number of
        // instruction code units.
        uint16_t insns_count_and_flags_;

        uint16_t insns_[1];                  // actual array of bytecode.

        // ...
    }

    // ...
}
```

* **Standard DEX file** (at `art/libdexfile/dex/standard_dex_file.h`)

```cpp
// Standard dex file. This is the format that is packaged in APKs and produced by tools.
class StandardDexFile : public DexFile {
public:
    class Header : public DexFile::Header {
        // Same for now.
    };

    struct CodeItem : public dex::CodeItem {
        // ...

    private:
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

        // ...
    };

    // ...

    uint32_t GetCodeItemSize(const dex::CodeItem& item) const override; // _ZNK3art15StandardDexFile15GetCodeItemSizeERKNS_3dex8CodeItemE in libdexfile.so

    // ...
};
```

---

## Implementation Considerations

**Mostly recommended**: By identifying `CodeItem::debug_info_off_`.
> Generally speaking, we don't need to consider `CompactDexFile`,
> which will not appear in common apps.

Or get the name of a method by `ArtMethod::PrettyMethod()`,
but I don't think it's a good choice.  
It's slow, and probably not so unique.

---

## Further Reading

1. [ART Dex Instruction List](https://cs.android.com/android/platform/superproject/main/+/main:art/libdexfile/dex/dex_instruction_list.h)  
2. [Dex Instruction Reference](https://cs.android.com/android/platform/superproject/main/+/main:art/libdexfile/dex/dex_instruction.h)  
3. [Real-world Obfuscation Analysis](https://blog.quarkslab.com/dji-the-art-of-obfuscation.html)