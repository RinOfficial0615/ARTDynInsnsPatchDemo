package moe.rikkarin.artdyninsnspatch.demo

import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.Composable
import androidx.compose.ui.tooling.preview.Preview
import moe.rikkarin.artdyninsnspatch.demo.jni.JNICore
import moe.rikkarin.artdyninsnspatch.demo.ui.theme.ARTDynamicInstructionsPatchPoCTheme
import moe.rikkarin.artdyninsnspatch.demo.utils.AppContext

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()
        setContent {
            ARTDynamicInstructionsPatchPoCTheme {
                App()
            }
        }
    }

    override fun attachBaseContext(newBase: Context?) {
        super.attachBaseContext(newBase)
        AppContext.init(this)
        JNICore.init()
    }
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    ARTDynamicInstructionsPatchPoCTheme {
        App()
    }
}