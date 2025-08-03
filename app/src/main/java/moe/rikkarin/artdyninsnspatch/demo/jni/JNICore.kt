package moe.rikkarin.artdyninsnspatch.demo.jni

import android.content.res.AssetManager
import moe.rikkarin.artdyninsnspatch.demo.utils.AppContext.applicationContext

object JNICore {
    fun init() {
        System.loadLibrary("dyn_patcher")
        init0(applicationContext!!.assets)
    }

    fun iterateClass(cls: String) {
        Class.forName(cls)
    }

    external fun init0(assetManager: AssetManager)
    external fun getInitializedMethods0(): Array<String>?
}
