#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <memory>
#include <optional>
#include <set>

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <sys/mman.h>
#include <bits/sysconf.h>

#include "dobby.h"

#include "art/art_method.hpp"
#include "art/code_item.hpp"

#include "utils/sym_resolver.hpp"
#include "utils/logging.hpp"
#include "utils/scoped_local_ref.hpp"

struct PatchData {
    uint32_t debug_info_off{};
    uint32_t insns_size{};
    std::vector<uint16_t> insns{};
};
static std::optional<PatchData> g_patch_data;

std::unordered_map<std::string, void *> initialized_methods;

void make_writable(void *addr, size_t size, const std::function<void(void)>& action) {
    if (addr == nullptr || size == 0 || !action) return;

#define ALIGN_FLOOR(address, range) ((uintptr_t)address & ~((uintptr_t)range - 1))
    int page_size = (int)sysconf(_SC_PAGESIZE);
    uintptr_t patch_page = ALIGN_FLOOR(addr, page_size);
    uintptr_t end_addr = (uintptr_t)addr + size - 1;
    uintptr_t patch_end_page = ALIGN_FLOOR(end_addr, page_size);
#undef ALIGN_FLOOR

    // make memory writable
    mprotect((void *)patch_page, patch_end_page - patch_page + page_size, PROT_READ | PROT_WRITE | PROT_EXEC);

    action();

    // restore perms
    mprotect((void *)patch_page, patch_end_page - patch_page + page_size, PROT_READ | PROT_EXEC);

    __builtin___clear_cache((char*)addr, (char*)((uintptr_t)addr + size));
}

void CommonHook(void *method) {
    auto jni_short_name = ArtMethod::JniShortName(method);
    auto method_name = ArtMethod::PrettyMethod(method, true);
    if (!method_name.empty() && !jni_short_name.empty()) {
        // We only care about our stuff
        if (jni_short_name.starts_with("Java_moe_rikkarin")) {
            LOGE("*** In InitializeMethodsCodeHook, pretty name: %s", method_name.c_str());
            initialized_methods[method_name] = method;
        }
    }

    if (!g_patch_data) {
        return;
    }

    auto code_item = ArtMethod::GetCodeItem(method);
    if (code_item && code_item->debug_info_off_ == g_patch_data->debug_info_off) {
        LOGI("Found target method to patch: %s", method_name.c_str());
        LOGD("Target debug_info_off: %u, insns_size: %u", code_item->debug_info_off_, code_item->insns_size_in_code_units_);

        if (code_item->insns_size_in_code_units_ != g_patch_data->insns_size) {
            LOGE("Instruction size mismatch! Expected %u, but found %u. Aborting patch.", g_patch_data->insns_size, code_item->insns_size_in_code_units_);
            return;
        }

        LOGD("Patching instructions...");
        make_writable(code_item->insns_, code_item->insns_size_in_code_units_ * sizeof(uint16_t), [&]() {
            memcpy(code_item->insns_, g_patch_data->insns.data(), g_patch_data->insns.size() * sizeof(uint16_t));
        });
        LOGI("Patch successful for method: %s", method_name.c_str());

        g_patch_data.reset();
    }
}

void (* InitializeMethodsCodeBackup)(void *thiz, void *method, const void *quick_code);
void InitializeMethodsCodeHook(void *thiz, void *method, const void *quick_code) {
    InitializeMethodsCodeBackup(thiz, method, quick_code);

    CommonHook(method);
}

void (* ReinitializeMethodsCodeBackup)(void *thiz, void *method);
void ReinitializeMethodsCodeHook(void *thiz, void *method) {
    ReinitializeMethodsCodeBackup(thiz, method);

    CommonHook(method);
}

std::unordered_map<std::string, std::pair<void*, void**>> InitializeMethodsCodeHooks = {
    {
        "_ZN3art15instrumentation15Instrumentation23ReinitializeMethodsCodeEPNS_9ArtMethodE",
        std::make_pair<>(reinterpret_cast<void *>(ReinitializeMethodsCodeHook), reinterpret_cast<void **>(&ReinitializeMethodsCodeBackup))
    },
    {
        "_ZN3art15instrumentation15Instrumentation21InitializeMethodsCodeEPNS_9ArtMethodEPKv",
        std::make_pair<>(reinterpret_cast<void *>(InitializeMethodsCodeHook), reinterpret_cast<void **>(&InitializeMethodsCodeBackup))
    },
};

extern "C"
JNIEXPORT jint
JNI_OnLoad(JavaVM *vm, void *) {
    for (auto& [symbol, handler] : InitializeMethodsCodeHooks) {
        auto ret = DobbyHook(Sym::ResolveArt(symbol.c_str()),
                  handler.first,
                  handler.second);

        if (ret == 0)
            break;
    }

    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
JNI_OnUnLoad(JavaVM *vm, void *) {
    for (const auto& [symbol, _] : InitializeMethodsCodeHooks) {
        auto ret = DobbyDestroy(Sym::ResolveArt(symbol.c_str()));

        if (ret == 0)
            break;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_moe_rikkarin_artdyninsnspatch_demo_jni_JNICore_init0(JNIEnv *env, jobject /* thiz */, jobject assetManager) {
    if (g_patch_data) {
        LOGI("Patch data already loaded.");
        return;
    }

    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    if (!mgr) {
        LOGE("Failed to get AAssetManager from Java");
        return;
    }

    const char* filename = "code_item.bin";
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", filename);
        return;
    }

    size_t asset_size = AAsset_getLength(asset);
    if (asset_size < 8) {
        LOGE("Asset file is too small.");
        AAsset_close(asset);
        return;
    }

    const void* asset_buffer = AAsset_getBuffer(asset);
    if (!asset_buffer) {
        LOGE("Failed to get asset buffer.");
        AAsset_close(asset);
        return;
    }

    PatchData temp_data{};
    auto* data_ptr = static_cast<const unsigned char*>(asset_buffer);

    memcpy(&temp_data.debug_info_off, data_ptr, sizeof(uint32_t));
    memcpy(&temp_data.insns_size, data_ptr + 4, sizeof(uint32_t));

    size_t insns_bytes_size = temp_data.insns_size * sizeof(uint16_t);
    if (asset_size != 8 + insns_bytes_size) {
        LOGE("Asset file size mismatch. Expected %zu, got %zu.", 8 + insns_bytes_size, asset_size);
        AAsset_close(asset);
        return;
    }

    temp_data.insns.resize(temp_data.insns_size);
    memcpy(temp_data.insns.data(), data_ptr + 8, insns_bytes_size);

    g_patch_data.emplace(std::move(temp_data));

    LOGI("Successfully loaded patch data from '%s'.", filename);
    LOGI("Target Method Signature -> debug_info_off: %u, insns_size: %u", g_patch_data->debug_info_off, g_patch_data->insns_size);

    AAsset_close(asset);
}


extern "C"
JNIEXPORT jobjectArray JNICALL
Java_moe_rikkarin_artdyninsnspatch_demo_jni_JNICore_getInitializedMethods0(JNIEnv *env, jobject /* thiz */) {
    jclass stringClass = env->FindClass("java/lang/String");
    if (!stringClass) {
        LOGE("Failed to find String class");
        return nullptr;
    }

    jobjectArray arr = env->NewObjectArray(initialized_methods.size(), stringClass, nullptr);
    if (!arr) {
        LOGE("Failed to create object array");
        return nullptr;
    }

    int i = 0;
    for (const auto &[name, _] : initialized_methods) {
        jstring jkey = env->NewStringUTF(name.c_str());

        env->SetObjectArrayElement(arr, i++, jkey);
        env->DeleteLocalRef(jkey);
    }

    return arr;
}