package moe.rikkarin.artdyninsnspatch.demo

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import moe.rikkarin.artdyninsnspatch.demo.jni.JNICore
import moe.rikkarin.artdyninsnspatch.demo.utils.AppContext.applicationContext

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun App() {
    Scaffold(
        topBar = {
            TopAppBar(title = { Text("The PoC") })
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues),
            contentAlignment = Alignment.Center
        ) {
            val alertDialogText = remember { mutableStateOf("") }
            val showAlertDialog = remember { mutableStateOf(false) }
            val alertDialogTitle = remember { mutableStateOf("") }

            val clsToIter = remember { mutableStateOf("moe.rikkarin.artdyninsnspatch.demo.ClassToPatch") }

            when {
                showAlertDialog.value -> {
                    AlertDialog(
                        onDismissRequest = { showAlertDialog.value = false },
                        title = { Text(alertDialogTitle.value) },
                        text = {
                            LazyColumn(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .background(MaterialTheme.colorScheme.surfaceVariant)
                                    .padding(8.dp)
                                    .horizontalScroll(rememberScrollState())
                            ) {
                                item {
                                    Text(
                                        text = alertDialogText.value,
                                        modifier = Modifier.padding(4.dp)
                                    )
                                }
                            }
                        },
                        confirmButton = {
                            Button(
                                onClick = { showAlertDialog.value = false }
                            ) {
                                Text("OK")
                            }
                        }
                    )
                }
            }

            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(8.dp)
                    .horizontalScroll(rememberScrollState())
            ) {
                item {
                    OutlinedTextField(
                        value = clsToIter.value,
                        onValueChange = { clsToIter.value = it },
                        label = { Text("Class to iterate") }
                    )
                }

                item {
                    Button(
                        content = { Text("Click to start!") },
                        onClick = {
                            runCatching {
                                Toast.makeText(applicationContext, "Result: " + ClassToPatch.methodToPatchBoolean(), Toast.LENGTH_LONG).show()

                                // Make sure the class is loaded
                                JNICore.iterateClass(clsToIter.value)

                                val methods = JNICore.getInitializedMethods0()
                                val sb = StringBuilder()
                                if (methods != null) {
                                    for (method in methods) {
                                        sb.append(method)
                                        sb.append("\n")
                                    }
                                }

                                alertDialogTitle.value = "Success"
                                alertDialogText.value = sb.toString()
                                showAlertDialog.value = true
                            }.onFailure {
                                alertDialogTitle.value = "Error"
                                alertDialogText.value = it.stackTraceToString()
                                showAlertDialog.value = true
                                it.printStackTrace()
                            }
                        }
                    )
                }
            }

        }
    }
}

