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
import android.provider.DocumentsContract
import android.provider.MediaStore
import android.provider.OpenableColumns
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
import java.security.DigestOutputStream
import java.security.MessageDigest

class MainActivity : Activity(), SurfaceHolder.Callback {
    companion object {
        private const val TAG = "ImRelighterAndroid"
        private const val MAX_IMAGE_BYTES = 64 * 1024 * 1024
        private const val REQUEST_OPEN_IMAGE = 1001
        private const val REQUEST_PICK_MODEL = 1002
        private const val REQUEST_PICK_MODEL_FOLDER = 1003
    }

    private lateinit var surfaceView: SurfaceView
    private var pickerInFlight = false
    private var modelPickerInFlight = false
    private var modelFolderPickerInFlight = false
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
            val savedStorage = getSharedPreferences("imrelighter", MODE_PRIVATE)
                .getString("model_storage_label", null)
                ?: tr("应用内部存储", "App storage")
            nativeSetModelStorage(savedStorage)
            window.decorView.setOnApplyWindowInsetsListener { _, insets ->
                val top = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    insets.getInsets(WindowInsets.Type.statusBars()).top
                } else {
                    @Suppress("DEPRECATION") insets.systemWindowInsetTop
                }
                val bottom = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    insets.getInsets(WindowInsets.Type.navigationBars()).bottom
                } else {
                    @Suppress("DEPRECATION") insets.systemWindowInsetBottom
                }
                nativeSetSystemInsets(top, bottom)
                insets
            }
            window.decorView.requestApplyInsets()
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
            append(tr("\n\n请通过 adb logcat 查看 ImRelighterAndroid 日志。", "\n\nCheck adb logcat tag ImRelighterAndroid for details."))
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
            // Draw edge-to-edge and apply the status-bar inset inside the
            // native UI. This avoids OEMs that ignore decor fits and let the
            // status bar overlap the app's top bar.
            window.setDecorFitsSystemWindows(false)
            val controller = window.insetsController ?: return
            controller.show(WindowInsets.Type.statusBars())
            controller.hide(WindowInsets.Type.navigationBars())
            controller.systemBarsBehavior =
                WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            controller.setSystemBarsAppearance(
                0, WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
            )
        } else {
            @Suppress("DEPRECATION")
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility =
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
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
        if (requestCode == REQUEST_PICK_MODEL_FOLDER) {
            modelFolderPickerInFlight = false
            if (resultCode != RESULT_OK) {
                return
            }
            val treeUri = data?.data ?: return
            val preferences = getSharedPreferences("imrelighter", MODE_PRIVATE)
            val savedTree = preferences.getString("model_storage_tree_uri", null)
            if (savedTree != null && sameTreeFolder(Uri.parse(savedTree), treeUri)) {
                showTransientMessage(
                    tr("该模型文件夹已导入，无需重复导入",
                       "This model folder is already imported")
                )
                return
            }
            try {
                contentResolver.takePersistableUriPermission(
                    treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
            } catch (error: Throwable) {
                Log.w(TAG, "could not persist tree permission", error)
            }
            Thread {
                try {
                    val imported = importModelFolder(treeUri)
                    if (imported.second.isEmpty()) {
                        showTransientMessage(tr("文件夹中没有 .onnx 模型",
                                                "No .onnx model found in the folder"))
                        return@Thread
                    }
                    runOnUiThread {
                        getSharedPreferences("imrelighter", MODE_PRIVATE).edit()
                            .putString("model_storage_label", imported.first)
                            .putString("model_storage_tree_uri", treeUri.toString())
                            .remove("model_last_document_uri")
                            .apply()
                        nativeSetModelStorage(imported.first)
                        nativeRegisterModelFiles(imported.second.toTypedArray())
                        showTransientMessage(
                            tr("已导入 ${imported.second.size} 个模型",
                               "Imported ${imported.second.size} model(s)")
                        )
                    }
                } catch (error: Throwable) {
                    Log.e(TAG, "model folder import failed", error)
                    showTransientMessage(
                        tr("模型文件夹导入失败: ${error.message ?: error.javaClass.simpleName}",
                           "Model folder import failed: ${error.message ?: error.javaClass.simpleName}")
                    )
                }
            }.start()
            return
        }
        if (requestCode == REQUEST_PICK_MODEL) {
            modelPickerInFlight = false
            if (resultCode != RESULT_OK) {
                return
            }
            val uri = data?.data ?: return
            getSharedPreferences("imrelighter", MODE_PRIVATE).edit()
                .putString("model_last_document_uri", uri.toString())
                .apply()
            Thread {
                try {
                    val path = copyModelFile(uri)
                    if (path == null) {
                        showTransientMessage(tr("模型文件复制失败", "Failed to copy model file"))
                        return@Thread
                    }
                    runOnUiThread {
                        nativeSelectModelFile(path)
                        showTransientMessage(tr("已选择模型文件", "Model file selected"))
                    }
                } catch (error: Throwable) {
                    Log.e(TAG, "model file selection failed", error)
                    showTransientMessage(
                        tr("模型文件选择失败: ", "Model file selection failed: ") + (error.message ?: error.javaClass.simpleName)
                    )
                }
            }.start()
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
                    showTransientMessage(tr("图片解码失败，请换一张图片", "Image decode failed; choose another image"))
                    return@Thread
                }
                val width = bitmap.width
                val height = bitmap.height
                nativeOpenBitmap(bitmap)
                Log.i(TAG, "image selected: ${width}x${height} RGBA bitmap")
                bitmap.recycle()
                showTransientMessage(tr("已导入 ${width} × ${height} 图片", "Imported ${width} × ${height} image"))
            } catch (error: Throwable) {
                Log.e(TAG, "failed to read selected image", error)
                showTransientMessage(tr("图片导入失败: ", "Image import failed: ") + (error.message ?: error.javaClass.simpleName))
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
        val displayName = queryDisplayName(uri)
            ?: uri.lastPathSegment?.substringAfterLast('/')
            ?: "model.onnx"
        if (!displayName.lowercase().endsWith(".onnx")) {
            throw IllegalArgumentException(tr("请选择 .onnx 模型文件", "Choose an .onnx model file"))
        }
        val safeName = sanitizeFilePart(displayName).ifBlank { "model.onnx" }
        val stem = safeName.substringBeforeLast('.').ifBlank { "model" }
        val folderName = modelFolderName(uri, safeName)
        val modelsDir = File(filesDir, "models")
        val targetDir = File(modelsDir, folderName)
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            return null
        }

        val temp = File(targetDir, ".tmp-${System.nanoTime()}.onnx")
        val digest = MessageDigest.getInstance("SHA-256")
        val input = contentResolver.openInputStream(uri) ?: return null
        input.use { source ->
            temp.outputStream().use { output ->
                DigestOutputStream(output, digest).use { digestOutput ->
                    source.copyTo(digestOutput)
                }
            }
        }
        if (temp.length() == 0L) {
            temp.delete()
            throw IllegalArgumentException(tr("模型文件为空", "The model file is empty"))
        }
        val suffix = digest.digest().joinToString("") { "%02x".format(it) }.take(8)
        val target = File(targetDir, "${stem}_${suffix}.onnx")
        if (!temp.renameTo(target)) {
            temp.copyTo(target, overwrite = true)
            temp.delete()
        }

        // Mirror the desktop rule and keep the effective config in the model's
        // own folder so it cannot leak to a different custom model.
        copyPreprocessorConfig(uri, targetDir)
        Log.i(TAG, "model file selected: ${target.absolutePath} (${target.length()} bytes)")
        return target.absolutePath
    }

    private fun queryDisplayName(uri: Uri): String? {
        return try {
            contentResolver.query(
                uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null
            )?.use { cursor ->
                if (cursor.moveToFirst()) cursor.getString(0) else null
            }
        } catch (error: Throwable) {
            Log.w(TAG, "could not query model display name", error)
            null
        }
    }

    private fun sanitizeFilePart(value: String): String {
        // Preserve Unicode folder names such as "手机模型"; only remove path
        // separators, reserved characters and control bytes.
        return value.replace(Regex("[\\\\/:*?\"<>|\\u0000-\\u001F]"), "_")
            .trim()
            .trimEnd('.', ' ')
            .take(120)
    }

    private fun modelFolderName(uri: Uri, fileName: String): String {
        return try {
            val documentId = DocumentsContract.getDocumentId(uri)
            val parent = documentId.substringBeforeLast('/', "")
            val folder = parent.substringAfterLast('/')
            sanitizeFilePart(folder).ifBlank { sanitizeFilePart(fileName.substringBeforeLast('.')) }
                .ifBlank { "custom" }
        } catch (error: Throwable) {
            sanitizeFilePart(fileName.substringBeforeLast('.')).ifBlank { "custom" }
        }
    }

    private fun copyPreprocessorConfig(uri: Uri, modelDir: File) {
        val stale = File(modelDir, "preprocessor_config.json")
        val authority = uri.authority
        val documentId = try {
            DocumentsContract.getDocumentId(uri)
        } catch (error: Throwable) {
            null
        }
        if (authority == null || documentId == null) {
            stale.delete()
            return
        }
        val parent = documentId.substringBeforeLast('/', "")
        var copied = false
        if (parent.isNotEmpty()) {
            copied = copyConfigFromDocumentChildren(authority, parent, modelDir)
            if (!copied) {
                val grandparent = parent.substringBeforeLast('/', "")
                if (grandparent.isNotEmpty() && grandparent != parent) {
                    copied = copyConfigFromDocumentChildren(authority, grandparent, modelDir)
                }
            }
        }
        if (!copied) {
            stale.delete()
        }
    }

    private fun copyConfigFromDocumentChildren(
        authority: String, parentDocumentId: String, destinationDir: File
    ): Boolean {
        return try {
            val childrenUri =
                DocumentsContract.buildChildDocumentsUri(authority, parentDocumentId)
            val projection = arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
            )
            contentResolver.query(childrenUri, projection, null, null, null)?.use { cursor ->
                while (cursor.moveToNext()) {
                    if (cursor.getString(1) != "preprocessor_config.json") continue
                    val childUri = DocumentsContract.buildDocumentUri(
                        authority, cursor.getString(0)
                    )
                    val destination = File(destinationDir, "preprocessor_config.json")
                    val temp = File(destinationDir, ".preprocessor_config.json.tmp")
                    contentResolver.openInputStream(childUri)?.use { input ->
                        temp.outputStream().use { output -> input.copyTo(output) }
                    }
                    if (!temp.renameTo(destination)) {
                        temp.copyTo(destination, overwrite = true)
                        temp.delete()
                    }
                    Log.i(TAG, "preprocessor config copied: ${destination.absolutePath}")
                    return true
                }
            }
            false
        } catch (error: Throwable) {
            Log.w(TAG, "could not copy preprocessor config", error)
            false
        }
    }

    private fun tr(chinese: String, english: String): String {
        return if (isEnglish()) english else chinese
    }

    private fun isEnglish(): Boolean {
        return try {
            File(filesDir, "language.txt").readText().trim() == "1"
        } catch (error: Throwable) {
            false
        }
    }

    private fun importModelFolder(treeUri: Uri): Pair<String, List<String>> {
        val rootDocumentId = DocumentsContract.getTreeDocumentId(treeUri)
        val rootUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, rootDocumentId)
        val rootName = queryDisplayName(rootUri) ?: "models"
        val safeRoot = sanitizeFilePart(rootName).ifBlank { "models" }
        val targetRoot = File(filesDir, "models/$safeRoot")
        if (targetRoot.exists()) {
            targetRoot.deleteRecursively()
        }
        if (!targetRoot.exists() && !targetRoot.mkdirs()) {
            throw IllegalStateException("cannot create model storage directory")
        }
        val models = mutableListOf<String>()
        copyDocumentTree(treeUri, rootDocumentId, targetRoot, models, 0)
        Log.i(TAG, "model folder imported: $safeRoot (${models.size} models)")
        return safeRoot to models
    }

    private fun copyDocumentTree(
        treeUri: Uri,
        documentId: String,
        targetDir: File,
        models: MutableList<String>,
        depth: Int
    ) {
        if (depth > 5) return
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
            treeUri, documentId
        )
        val projection = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        )
        contentResolver.query(childrenUri, projection, null, null, null)?.use { cursor ->
            while (cursor.moveToNext()) {
                val childId = cursor.getString(0) ?: continue
                val name = cursor.getString(1) ?: continue
                val mime = cursor.getString(2) ?: ""
                val safeName = sanitizeFilePart(name).ifBlank { "model.onnx" }
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                    val childDir = File(targetDir, safeName)
                    if (childDir.mkdirs() || childDir.exists()) {
                        copyDocumentTree(treeUri, childId, childDir, models, depth + 1)
                    }
                    continue
                }
                if (!name.lowercase().endsWith(".onnx") && name != "preprocessor_config.json") {
                    continue
                }
                val childUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId)
                if (name == "preprocessor_config.json") {
                    copyDocumentToFile(childUri, targetDir, name, false)
                } else {
                    val copied = copyDocumentToFile(childUri, targetDir, name, true)
                    if (copied != null) {
                        models.add(copied.absolutePath)
                    }
                }
            }
        }
    }

    private fun copyDocumentToFile(
        uri: Uri, targetDir: File, name: String, addHash: Boolean
    ): File? {
        val temp = File(targetDir, ".tmp-${System.nanoTime()}")
        val digest = MessageDigest.getInstance("SHA-256")
        val input = contentResolver.openInputStream(uri) ?: return null
        input.use { source ->
            temp.outputStream().use { output ->
                DigestOutputStream(output, digest).use { digestOutput ->
                    source.copyTo(digestOutput)
                }
            }
        }
        if (temp.length() == 0L) {
            temp.delete()
            return null
        }
        val target = if (addHash) {
            val suffix = digest.digest().joinToString("") { "%02x".format(it) }.take(8)
            File(targetDir, "${name.substringBeforeLast('.')}_${suffix}.onnx")
        } else {
            File(targetDir, name)
        }
        if (!temp.renameTo(target)) {
            temp.copyTo(target, overwrite = true)
            temp.delete()
        }
        return target
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
                        showTransientMessage(tr("已导出到相册", "Exported to gallery"))
                    }
                } else {
                    MediaStore.Images.Media.insertImage(
                        contentResolver, bitmap, name, "ImRelighter export")
                    Log.i(TAG, "export saved: $name")
                    showTransientMessage(tr("已导出到相册", "Exported to gallery"))
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

    fun requestModelFolderFromNative() {
        runOnUiThread {
            launchModelFolderPicker()
        }
    }

    private fun launchModelFolderPicker() {
        if (modelFolderPickerInFlight) {
            return
        }
        modelFolderPickerInFlight = true
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_PICK_MODEL_FOLDER)
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
            initialModelPickerUri()?.let { initialUri ->
                putExtra(DocumentsContract.EXTRA_INITIAL_URI, initialUri)
            }
        }
        startActivityForResult(intent, REQUEST_PICK_MODEL)
    }

    private fun sameTreeFolder(first: Uri, second: Uri): Boolean {
        return try {
            first.authority == second.authority &&
                DocumentsContract.getTreeDocumentId(first) ==
                DocumentsContract.getTreeDocumentId(second)
        } catch (error: Throwable) {
            first == second
        }
    }

    private fun initialModelPickerUri(): Uri? {
        val preferences = getSharedPreferences("imrelighter", MODE_PRIVATE)
        val savedTree = preferences.getString("model_storage_tree_uri", null)
        if (savedTree != null) {
            try {
                val treeUri = Uri.parse(savedTree)
                val treeDocumentId = DocumentsContract.getTreeDocumentId(treeUri)
                findFirstOnnx(treeUri, treeDocumentId, 0)?.let { return it }
                return DocumentsContract.buildDocumentUriUsingTree(
                    treeUri, treeDocumentId
                )
            } catch (error: Throwable) {
                Log.w(TAG, "could not resolve model storage folder", error)
            }
        }
        val lastModel = preferences.getString("model_last_document_uri", null)
        if (lastModel != null) {
            try {
                val uri = Uri.parse(lastModel)
                if (DocumentsContract.isDocumentUri(this, uri)) {
                    return uri
                }
            } catch (error: Throwable) {
                Log.w(TAG, "could not use last model document", error)
            }
        }
        return null
    }

    private fun findFirstOnnx(treeUri: Uri, documentId: String, depth: Int): Uri? {
        if (depth > 3) return null
        return try {
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                treeUri, documentId
            )
            val projection = arrayOf(
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE
            )
            val directories = mutableListOf<String>()
            var found: Uri? = null
            contentResolver.query(childrenUri, projection, null, null, null)?.use { cursor ->
                while (cursor.moveToNext()) {
                    val id = cursor.getString(0) ?: continue
                    val name = cursor.getString(1) ?: continue
                    val mime = cursor.getString(2) ?: ""
                    if (mime == DocumentsContract.Document.MIME_TYPE_DIR) {
                        directories.add(id)
                    } else if (name.lowercase().endsWith(".onnx")) {
                        found = DocumentsContract.buildDocumentUriUsingTree(treeUri, id)
                        break
                    }
                }
            }
            if (found != null) return found
            for (directoryId in directories) {
                findFirstOnnx(treeUri, directoryId, depth + 1)?.let { return it }
            }
            null
        } catch (error: Throwable) {
            Log.w(TAG, "could not scan model storage folder", error)
            null
        }
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
    private external fun nativeRegisterModelFiles(paths: Array<String>)
    private external fun nativeSetModelStorage(label: String)
    private external fun nativeSetSystemInsets(top: Int, bottom: Int)
    private external fun nativeResetModel()
    private external fun nativeClearSession()
    private external fun nativeShutdown()
}
