package com.imrelighter.android

import android.app.Activity
import android.content.ContentValues
import android.content.Intent
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.MediaStore
import android.system.Os
import android.system.OsConstants
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.TextView
import android.widget.Toast
import java.io.ByteArrayOutputStream
import java.io.File

class MainActivity : Activity(), SurfaceHolder.Callback {
    companion object {
        private const val TAG = "ImRelighterAndroid"
        private const val MAX_IMAGE_BYTES = 64 * 1024 * 1024
        private const val REQUEST_OPEN_IMAGE = 1001
        private const val REQUEST_PICK_MODEL = 1002
    }

    private lateinit var surfaceView: SurfaceView
    private var pickerInFlight = false
    private var modelPickerInFlight = false
    private var nativeInitialized = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        @Suppress("DEPRECATION")
        window.statusBarColor = Color.rgb(17, 19, 24)
        @Suppress("DEPRECATION")
        window.navigationBarColor = Color.rgb(17, 19, 24)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }

        Log.i(TAG, "device: ${Build.MANUFACTURER} ${Build.MODEL}, SDK ${Build.VERSION.SDK_INT}, " +
                "ABIs ${Build.SUPPORTED_ABIS.joinToString()}")
        try {
            Log.i(TAG, "native page size: ${Os.sysconf(OsConstants._SC_PAGESIZE)}")
        } catch (error: Throwable) {
            Log.w(TAG, "unable to query page size", error)
        }

        try {
            System.loadLibrary("imrelighter")
            Log.i(TAG, "native library loaded")
        } catch (error: Throwable) {
            Log.e(TAG, "native library load failed", error)
            showStartupError("原生库加载失败 / Native library load failed", error)
            return
        }

        surfaceView = SurfaceView(this).apply {
            holder.addCallback(this@MainActivity)
            isFocusable = true
            isFocusableInTouchMode = true
            setOnTouchListener { _, event -> handleTouch(event) }
        }
        setContentView(surfaceView)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            surfaceView.addOnLayoutChangeListener { view, _, top, _, bottom, _, _, _, _ ->
                val railWidth = (44 * resources.displayMetrics.density).toInt()
                view.systemGestureExclusionRects =
                    listOf(Rect(0, top, railWidth, bottom))
            }
        }
        window.decorView.post { enableImmersiveMode() }

        try {
            nativeInit(this, assets, filesDir.absolutePath, resources.displayMetrics.density)
            nativeInitialized = true
            Log.i(TAG, "native initialized")
        } catch (error: Throwable) {
            Log.e(TAG, "native initialization failed", error)
            showStartupError("原生初始化失败 / Native initialization failed", error)
        }
    }

    private fun showStartupError(title: String, error: Throwable) {
        val message = buildString {
            append(title)
            append("\n\n")
            append(error.javaClass.simpleName)
            if (!error.message.isNullOrBlank()) {
                append(": ")
                append(error.message)
            }
            append("\n\n请通过 adb logcat 查看 ImRelighterAndroid 日志。")
        }
        val view = TextView(this).apply {
            setBackgroundColor(Color.rgb(17, 19, 24))
            setTextColor(Color.rgb(232, 179, 106))
            textSize = 16f
            gravity = Gravity.CENTER
            setPadding(64, 64, 64, 64)
            text = message
        }
        setContentView(view)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enableImmersiveMode()
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                val railWidth = (44 * resources.displayMetrics.density).toInt()
                val root = window.decorView
                root.systemGestureExclusionRects =
                    listOf(Rect(0, 0, railWidth, root.height.coerceAtLeast(1)))
                Log.i(TAG, "gesture exclusion rail width: $railWidth")
            }
        }
    }

    private fun enableImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            val controller = window.insetsController ?: return
            controller.hide(WindowInsets.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility =
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                    View.SYSTEM_UI_FLAG_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        }
    }

    private fun handleTouch(event: MotionEvent): Boolean {
        if (!nativeInitialized) {
            return true
        }
        val action = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> 0
            MotionEvent.ACTION_MOVE, MotionEvent.ACTION_POINTER_UP -> 1
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> 2
            else -> return true
        }
        val count = event.pointerCount
        val x0 = event.getX(0)
        val y0 = event.getY(0)
        val x1 = if (count > 1) event.getX(1) else x0
        val y1 = if (count > 1) event.getY(1) else y0
        nativeTouch(action, count, x0, y0, x1, y1)
        return true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surface created ${holder.surfaceFrame.width()}x${holder.surfaceFrame.height()}")
        if (nativeInitialized) {
            nativeSurfaceCreated(holder.surface, holder.surfaceFrame.width(), holder.surfaceFrame.height())
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surface changed ${width}x${height}")
        if (nativeInitialized) {
            nativeSurfaceChanged(width, height)
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surface destroyed")
        if (nativeInitialized) {
            nativeSurfaceDestroyed()
        }
    }

    override fun onBackPressed() {
        if (!nativeInitialized || !nativeBack()) {
            super.onBackPressed()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_PICK_MODEL) {
            modelPickerInFlight = false
            if (resultCode != RESULT_OK) {
                return
            }
            val uri = data?.data ?: return
            try {
                val path = copyModelFile(uri)
                if (path == null) {
                    showTransientMessage("模型文件复制失败")
                    return
                }
                nativeSelectModelFile(path)
                showTransientMessage("已选择模型文件")
            } catch (error: Throwable) {
                Log.e(TAG, "model file selection failed", error)
                showTransientMessage("模型文件选择失败: ${error.message ?: error.javaClass.simpleName}")
            }
            return
        }
        if (requestCode != REQUEST_OPEN_IMAGE) {
            return
        }
        pickerInFlight = false
        if (resultCode != RESULT_OK) {
            return
        }
        val uri = data?.data ?: return
        // Decode and convert off the UI thread. The native side reads the
        // Bitmap's RGBA buffer directly, avoiding a 12M-iteration Kotlin loop.
        Thread {
            try {
                val bitmap = decodeSelectedImage(uri)
                if (bitmap == null) {
                    Log.e(TAG, "selected image could not be decoded")
                    showTransientMessage("图片解码失败，请换一张图片")
                    return@Thread
                }
                val width = bitmap.width
                val height = bitmap.height
                nativeOpenBitmap(bitmap)
                Log.i(TAG, "image selected: ${width}x${height} RGBA bitmap")
                bitmap.recycle()
                showTransientMessage("已导入 ${width} × ${height} 图片")
            } catch (error: Throwable) {
                Log.e(TAG, "failed to read selected image", error)
                showTransientMessage("图片导入失败: ${error.message ?: error.javaClass.simpleName}")
            }
        }.start()
    }

    private fun decodeSelectedImage(uri: Uri): Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input, null, bounds)
        }
        val width = bounds.outWidth
        val height = bounds.outHeight
        if (width <= 0 || height <= 0) {
            return null
        }
        var sample = 1
        while (width.toLong() / sample * (height.toLong() / sample) > 12_000_000L) {
            sample *= 2
        }
        val options = BitmapFactory.Options().apply {
            inSampleSize = sample
            inPreferredConfig = Bitmap.Config.ARGB_8888
        }
        var bitmap = contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input, null, options)
        } ?: return null
        if (bitmap.width.toLong() * bitmap.height > 12_000_000L) {
            val scale = kotlin.math.sqrt(12_000_000.0 / (bitmap.width.toLong() * bitmap.height))
            val scaled = Bitmap.createScaledBitmap(
                bitmap,
                (bitmap.width * scale).toInt().coerceAtLeast(1),
                (bitmap.height * scale).toInt().coerceAtLeast(1),
                true
            )
            if (scaled !== bitmap) {
                bitmap.recycle()
            }
            bitmap = scaled
        }
        Log.i(TAG, "decoded image: ${bounds.outWidth}x${bounds.outHeight}, sample=$sample, " +
                "result=${bitmap.width}x${bitmap.height}")
        return bitmap
    }

    private fun copyModelFile(uri: Uri): String? {
        val rawName = uri.lastPathSegment?.substringAfterLast('/')?.ifBlank { "model.onnx" }
            ?: "model.onnx"
        val targetDir = File(filesDir, "models")
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            return null
        }
        val target = File(targetDir, rawName)
        contentResolver.openInputStream(uri)?.use { input ->
            target.outputStream().use { output -> input.copyTo(output) }
        } ?: return null
        Log.i(TAG, "model file selected: ${target.absolutePath} (${target.length()} bytes)")
        return target.absolutePath
    }

    private fun showTransientMessage(message: String) {
        runOnUiThread {
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
        }
    }

    fun onExportReady(data: ByteArray, width: Int, height: Int, kind: String) {
        if (width <= 0 || height <= 0 || data.size != width * height * 4) {
            Log.e(TAG, "invalid export payload: ${width}x${height}, ${data.size} bytes")
            return
        }
        runOnUiThread {
            try {
                val pixels = IntArray(width * height)
                var index = 0
                for (i in pixels.indices) {
                    val r = data[index++].toInt() and 0xFF
                    val g = data[index++].toInt() and 0xFF
                    val b = data[index++].toInt() and 0xFF
                    val a = data[index++].toInt() and 0xFF
                    pixels[i] = (a shl 24) or (r shl 16) or (g shl 8) or b
                }
                val bitmap = Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888)
                val name = "ImRelighter_${kind}_${System.currentTimeMillis()}.png"
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    val values = ContentValues().apply {
                        put(MediaStore.Images.Media.DISPLAY_NAME, name)
                        put(MediaStore.Images.Media.MIME_TYPE, "image/png")
                        put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/ImRelighter")
                        put(MediaStore.Images.Media.IS_PENDING, 1)
                    }
                    val uri = contentResolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
                    if (uri != null) {
                        contentResolver.openOutputStream(uri)?.use { output ->
                            bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)
                        }
                        values.clear()
                        values.put(MediaStore.Images.Media.IS_PENDING, 0)
                        contentResolver.update(uri, values, null, null)
                        Log.i(TAG, "export saved: $uri")
                        showTransientMessage("已导出到相册")
                    }
                } else {
                    MediaStore.Images.Media.insertImage(
                        contentResolver, bitmap, name, "ImRelighter export")
                    Log.i(TAG, "export saved: $name")
                    showTransientMessage("已导出到相册")
                }
                bitmap.recycle()
            } catch (error: Exception) {
                Log.e(TAG, "failed to save export", error)
            }
        }
    }

    override fun onDestroy() {
        pickerInFlight = false
        if (nativeInitialized) {
            nativeInitialized = false
            // Keep the process-scoped session/render thread alive when the
            // Activity is recreated while the process remains alive. Only an
            // actual user exit destroys the session.
            if (isFinishing) {
                nativeClearSession()
                nativeShutdown()
            }
        }
        super.onDestroy()
    }

    fun requestOpenImageFromNative() {
        runOnUiThread {
            launchImagePicker()
        }
    }

    private fun launchImagePicker() {
        if (pickerInFlight) {
            return
        }
        pickerInFlight = true
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "image/*"
        }
        startActivityForResult(intent, REQUEST_OPEN_IMAGE)
    }

    fun requestModelFileFromNative() {
        runOnUiThread {
            launchModelPicker()
        }
    }

    private fun launchModelPicker() {
        if (modelPickerInFlight) {
            return
        }
        modelPickerInFlight = true
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            putExtra(
                Intent.EXTRA_MIME_TYPES,
                arrayOf("application/octet-stream", "application/onnx", "*/*")
            )
        }
        startActivityForResult(intent, REQUEST_PICK_MODEL)
    }

    private fun readImageBytes(uri: Uri): ByteArray? {
        return contentResolver.openInputStream(uri)?.use { input ->
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(64 * 1024)
            var total = 0
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                total += read
                if (total > MAX_IMAGE_BYTES) {
                    throw IllegalStateException("image is too large")
                }
                output.write(buffer, 0, read)
            }
            output.toByteArray()
        }
    }

    private external fun nativeInit(
        activity: Activity,
        assets: AssetManager,
        filesDir: String,
        density: Float
    )

    private external fun nativeSurfaceCreated(surface: Surface, width: Int, height: Int)
    private external fun nativeSurfaceChanged(width: Int, height: Int)
    private external fun nativeSurfaceDestroyed()
    private external fun nativeTouch(
        action: Int,
        pointerCount: Int,
        x0: Float,
        y0: Float,
        x1: Float,
        y1: Float
    )
    private external fun nativeBack(): Boolean
    private external fun nativeOpenImage(data: ByteArray)
    private external fun nativeOpenImageRgba(data: ByteArray, width: Int, height: Int)
    private external fun nativeOpenBitmap(bitmap: Bitmap)
    private external fun nativeSelectModel(modelId: String)
    private external fun nativeSelectModelFile(path: String)
    private external fun nativeResetModel()
    private external fun nativeClearSession()
    private external fun nativeShutdown()
}
