import java.util.Properties

plugins {
    id("com.android.application")
}

val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties().apply {
    if (keystorePropertiesFile.exists()) {
        keystorePropertiesFile.inputStream().use { load(it) }
    }
}

android {
    namespace = "com.imrelighter.android"
    compileSdk = 36
    ndkVersion = "26.3.11579264"

    defaultConfig {
        applicationId = "com.imrelighter.android"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"

        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_static"
                abiFilters += listOf("arm64-v8a", "x86_64")
            }
        }
    }

    signingConfigs {
        create("release") {
            if (keystorePropertiesFile.exists()) {
                storeFile = file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (keystorePropertiesFile.exists()) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        aidl = false
        buildConfig = false
        compose = false
        shaders = false
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }

    sourceSets["main"].assets.srcDir("build/generated/assets")
    sourceSets["main"].jniLibs.srcDir(rootProject.projectDir.resolve("third_party/onnxruntime-android-1.20.0/jni"))
}

val generatedAssets = layout.buildDirectory.dir("generated/assets")
val repoRoot = rootProject.projectDir.parentFile

val syncModels by tasks.registering(Copy::class) {
    doFirst {
        // Remove legacy assets left by earlier builds. Q4 is the only bundled
        // Android model now.
        delete(layout.buildDirectory.dir(
            "generated/assets/models/depth-anything-v2-small/q4f16"))
    }
    from(repoRoot.resolve("models/depth-anything-v2-small/q4/model_q4.onnx")) {
        into("models/depth-anything-v2-small/q4")
    }
    from(repoRoot.resolve("models/depth-anything-v2-small/preprocessor_config.json")) {
        into("models/depth-anything-v2-small")
    }

    into(generatedAssets)
}

tasks.named("preBuild").configure {
    dependsOn(syncModels)
}
