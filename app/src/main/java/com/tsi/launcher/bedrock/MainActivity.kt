package com.tsi.launcher.bedrock

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.OpenInNew
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject
import java.text.DateFormat
import java.util.Date

private const val BEDROCK_PACKAGE = "com.mojang.minecraftpe"
private val Page = Color(0xFF101820)
private val Panel = Color(0xFF1A2834)
private val Accent = Color(0xFF43C78B)
private val Muted = Color(0xFFA8B6C2)

data class InstalledBedrock(
    val versionName: String,
    val versionCode: Long,
    val lastUpdateTime: Long
)

data class VersionObservation(
    val versionName: String,
    val versionCode: Long,
    val observedAt: Long
)

class MainActivity : ComponentActivity() {
    var onActivityResumed: (() -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { BedrockLauncherScreen() }
    }

    override fun onResume() {
        super.onResume()
        onActivityResumed?.invoke()
    }
}

@Suppress("DEPRECATION")
private fun packageInfo(context: Context): PackageInfo {
    val manager = context.packageManager
    return if (Build.VERSION.SDK_INT >= 33) {
        manager.getPackageInfo(
            BEDROCK_PACKAGE,
            PackageManager.PackageInfoFlags.of(0L)
        )
    } else {
        manager.getPackageInfo(BEDROCK_PACKAGE, 0)
    }
}

private fun readInstalledBedrock(context: Context): InstalledBedrock? {
    return try {
        val info = packageInfo(context)
        val code = if (Build.VERSION.SDK_INT >= 28) {
            info.longVersionCode
        } else {
            @Suppress("DEPRECATION")
            info.versionCode.toLong()
        }
        InstalledBedrock(
            versionName = info.versionName ?: "Unknown",
            versionCode = code,
            lastUpdateTime = info.lastUpdateTime
        )
    } catch (_: PackageManager.NameNotFoundException) {
        null
    } catch (_: SecurityException) {
        null
    }
}

private object VersionHistoryStore {
    private const val PREFS = "tsi_bedrock_history"
    private const val KEY = "observations"
    private const val MAX_ENTRIES = 20

    fun record(context: Context, installed: InstalledBedrock?): List<VersionObservation> {
        val preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val old = runCatching { JSONArray(preferences.getString(KEY, "[]")) }
            .getOrDefault(JSONArray())
        val next = JSONArray()

        if (installed != null) {
            val first = old.optJSONObject(0)
            val sameAsLatest = first != null &&
                first.optLong("versionCode") == installed.versionCode &&
                first.optString("versionName") == installed.versionName

            if (!sameAsLatest) {
                next.put(
                    JSONObject()
                        .put("versionName", installed.versionName)
                        .put("versionCode", installed.versionCode)
                        .put("observedAt", System.currentTimeMillis())
                )
            }
        }

        for (index in 0 until minOf(old.length(), MAX_ENTRIES - next.length())) {
            val item = old.optJSONObject(index) ?: continue
            next.put(item)
        }
        preferences.edit().putString(KEY, next.toString()).apply()
        return read(next)
    }

    private fun read(array: JSONArray): List<VersionObservation> {
        return buildList {
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                add(
                    VersionObservation(
                        versionName = item.optString("versionName", "Unknown"),
                        versionCode = item.optLong("versionCode"),
                        observedAt = item.optLong("observedAt")
                    )
                )
            }
        }
    }
}

@Composable
private fun BedrockLauncherScreen() {
    val context = LocalContext.current
    var installed by remember { mutableStateOf(readInstalledBedrock(context)) }
    var history by remember { mutableStateOf(VersionHistoryStore.record(context, installed)) }
    var message by remember {
        mutableStateOf(
            if (installed == null) {
                "Minecraft Bedrock was not found on this device."
            } else {
                "Detected the official Minecraft Bedrock app."
            }
        )
    }
    val host = context as? MainActivity

    fun refresh() {
        installed = readInstalledBedrock(context)
        history = VersionHistoryStore.record(context, installed)
        message = if (installed == null) {
            "Minecraft Bedrock is not installed, or Android did not expose its package."
        } else {
            "Installed version " + installed?.versionName + " detected."
        }
    }

    DisposableEffect(host) {
        host?.onActivityResumed = { refresh() }
        onDispose { host?.onActivityResumed = null }
    }

    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Accent,
            secondary = Color(0xFF7CD8AF),
            background = Page,
            surface = Panel,
            onPrimary = Color(0xFF07130E),
            onBackground = Color.White,
            onSurface = Color.White
        )
    ) {
        Scaffold(containerColor = Page) { padding ->
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(padding)
                    .background(Page)
                    .padding(horizontal = 18.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp)
            ) {
                item {
                    Column(
                        modifier = Modifier.padding(top = 22.dp, bottom = 4.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Text(
                            "TSI BEDROCK",
                            color = Accent,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.ExtraBold,
                            letterSpacing = 2.sp
                        )
                        Text(
                            "Minecraft Bedrock Edition",
                            fontSize = 25.sp,
                            fontWeight = FontWeight.ExtraBold,
                            color = Color.White
                        )
                        Text(
                            "A separate launcher companion for the official Android Bedrock app.",
                            color = Muted,
                            fontSize = 13.sp
                        )
                    }
                }

                item {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(containerColor = Panel),
                        shape = RoundedCornerShape(20.dp)
                    ) {
                        Column(
                            modifier = Modifier.padding(17.dp),
                            verticalArrangement = Arrangement.spacedBy(10.dp)
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    if (installed != null) Icons.Filled.CheckCircle else Icons.Filled.SportsEsports,
                                    contentDescription = null,
                                    tint = Accent
                                )
                                Spacer(Modifier.padding(start = 4.dp))
                                Text(
                                    if (installed != null) "OFFICIAL APP DETECTED" else "APP NOT INSTALLED",
                                    color = Accent,
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 12.sp
                                )
                            }
                            Text(
                                if (installed != null) "Minecraft " + installed?.versionName else "Minecraft Bedrock",
                                color = Color.White,
                                fontWeight = FontWeight.Bold,
                                fontSize = 21.sp
                            )
                            Text(
                                installed?.let {
                                    "Package: " + BEDROCK_PACKAGE + "\nVersion code: " + it.versionCode
                                } ?: "Install the official Minecraft app from Google Play. TSI does not download or replace the game APK.",
                                color = Muted,
                                fontSize = 12.sp
                            )
                            Button(
                                onClick = {
                                    try {
                                        val launchIntent = context.packageManager
                                            .getLaunchIntentForPackage(BEDROCK_PACKAGE)
                                        if (launchIntent == null) {
                                            message = "Android did not provide a launch activity for Minecraft."
                                        } else {
                                            context.startActivity(launchIntent)
                                            message = "Opened the official Minecraft Bedrock app."
                                        }
                                    } catch (error: Exception) {
                                        message = "Could not open Minecraft: " + (error.message ?: "unknown error")
                                    }
                                },
                                enabled = installed != null,
                                modifier = Modifier.fillMaxWidth(),
                                colors = ButtonDefaults.buttonColors(containerColor = Accent)
                            ) {
                                Icon(Icons.Filled.OpenInNew, null)
                                Spacer(Modifier.padding(start = 5.dp))
                                Text("PLAY MINECRAFT")
                            }
                            OutlinedButton(
                                onClick = {
                                    val market = Intent(
                                        Intent.ACTION_VIEW,
                                        Uri.parse("market://details?id=" + BEDROCK_PACKAGE)
                                    )
                                    val web = Intent(
                                        Intent.ACTION_VIEW,
                                        Uri.parse("https://play.google.com/store/apps/details?id=" + BEDROCK_PACKAGE)
                                    )
                                    try {
                                        context.startActivity(market)
                                    } catch (_: ActivityNotFoundException) {
                                        context.startActivity(web)
                                    } catch (_: Exception) {
                                        context.startActivity(web)
                                    }
                                },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Icon(Icons.Filled.Download, null)
                                Spacer(Modifier.padding(start = 5.dp))
                                Text(if (installed == null) "GET OFFICIAL BEDROCK APP" else "MANAGE IN GOOGLE PLAY")
                            }
                            OutlinedButton(
                                onClick = { refresh() },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Icon(Icons.Filled.Refresh, null)
                                Spacer(Modifier.padding(start = 5.dp))
                                Text("REFRESH INSTALLED VERSION")
                            }
                            Text(message, color = Muted, fontSize = 11.sp)
                        }
                    }
                }

                item {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(containerColor = Panel),
                        shape = RoundedCornerShape(20.dp)
                    ) {
                        Column(
                            modifier = Modifier.padding(17.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Text(
                                "ANDROID RELEASE / BETA CHANNEL",
                                color = Accent,
                                fontWeight = FontWeight.ExtraBold,
                                fontSize = 12.sp
                            )
                            Text(
                                "Android Beta is managed by Google Play. Joining Beta updates that same Minecraft package; it is not a second app that can run alongside Stable.",
                                color = Color.White,
                                fontSize = 13.sp
                            )
                            Text(
                                "There is no separate Android Preview app channel. The exact Play track cannot be read reliably from the installed package, so this screen reports the installed version and sends channel changes to Google Play instead of guessing.",
                                color = Muted,
                                fontSize = 11.sp
                            )
                            OutlinedButton(
                                onClick = {
                                    val intent = Intent(
                                        Intent.ACTION_VIEW,
                                        Uri.parse("https://help.minecraft.net/")
                                    )
                                    runCatching { context.startActivity(intent) }
                                },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Text("MINECRAFT HELP")
                            }
                        }
                    }
                }

                item {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Filled.History, null, tint = Accent)
                        Spacer(Modifier.padding(start = 5.dp))
                        Text(
                            "VERSION HISTORY",
                            color = Color.White,
                            fontWeight = FontWeight.ExtraBold,
                            fontSize = 14.sp
                        )
                    }
                }

                if (history.isEmpty()) {
                    item {
                        Text(
                            "No version changes have been observed yet. TSI records a local entry when it detects a new installed version.",
                            color = Muted,
                            fontSize = 12.sp
                        )
                    }
                } else {
                    items(history, key = { it.versionCode.toString() + "-" + it.observedAt }) { entry ->
                        Card(
                            modifier = Modifier.fillMaxWidth(),
                            colors = CardDefaults.cardColors(containerColor = Panel),
                            shape = RoundedCornerShape(14.dp)
                        ) {
                            Column(
                                modifier = Modifier.padding(14.dp),
                                verticalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Text(
                                    "Minecraft " + entry.versionName,
                                    color = Color.White,
                                    fontWeight = FontWeight.Bold
                                )
                                Text("Version code: " + entry.versionCode, color = Muted, fontSize = 11.sp)
                                Text(
                                    "Detected: " + DateFormat.getDateTimeInstance().format(Date(entry.observedAt)),
                                    color = Muted,
                                    fontSize = 11.sp
                                )
                            }
                        }
                    }
                }

                item {
                    Divider(color = Color(0xFF334552))
                    Text(
                        "TSI Bedrock Edition does not patch, downgrade, host or redistribute Minecraft. The official app and its updates remain under Google Play's package signature and licensing rules.",
                        color = Muted,
                        fontSize = 10.sp,
                        modifier = Modifier.padding(vertical = 10.dp)
                    )
                }
            }
        }
    }
}
