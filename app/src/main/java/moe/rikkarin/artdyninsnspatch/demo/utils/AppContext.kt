package moe.rikkarin.artdyninsnspatch.demo.utils

import android.content.Context

object AppContext {
    var applicationContext: Context? = null
        private set

    fun init(ctx: Context) {
        applicationContext = ctx.applicationContext
    }
}