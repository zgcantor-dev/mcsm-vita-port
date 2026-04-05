#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <so_util/so_util.h>
#include <string.h>

#define TELLTALE_PKG_NAME "com.telltalegames.minecraft100"
#define TELLTALE_OBB_VERSION "40137"
#define TELLTALE_EXTERNAL_STORAGE_ROOT "/storage/emulated/0"

#define TELLTALE_MAIN_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/main." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"
#define TELLTALE_PATCH_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/patch." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"

extern so_module so_mod;

typedef void (*native_on_permission_complete_fn)(JNIEnv *env, jobject thiz, jint request_code, jboolean granted);

static native_on_permission_complete_fn resolve_native_permission_callback(void) {
	static native_on_permission_complete_fn cached;
	static int resolved;
	if (!resolved) {
		resolved = 1;
		cached = (native_on_permission_complete_fn)so_symbol(
				&so_mod, "Java_com_telltalegames_telltale_TelltaleActivity_nativeOnPermissionComplete");
	}
	return cached;
}

static jobject jni_getObbFileName(jmethodID id, va_list args) {
	(void)id;

	// Signature in libGameEngine.so: (Z)Ljava/lang/String;
	// false -> main OBB, true -> patch OBB.
	int want_patch = va_arg(args, int);

	const char *path = want_patch ? TELLTALE_PATCH_OBB_ANDROID_PATH : TELLTALE_MAIN_OBB_ANDROID_PATH;
	return jni->NewStringUTF(&jni, path);
}

static jobject jni_getExternalStorageDirectory(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, TELLTALE_EXTERNAL_STORAGE_ROOT);
}

static jobject jni_getExternalStorageDirs(jmethodID id, va_list args) {
	(void)id;
	(void)args;

	jclass string_class = jni->FindClass(&jni, "java/lang/String");
	if (!string_class)
		return NULL;

	jobjectArray dirs = jni->NewObjectArray(&jni, 1, string_class, NULL);
	if (!dirs)
		return NULL;

	jstring root = jni->NewStringUTF(&jni, TELLTALE_EXTERNAL_STORAGE_ROOT);
	if (!root)
		return dirs;

	jni->SetObjectArrayElement(&jni, dirs, 0, root);
	return dirs;
}

static jobject jni_getHardwareModel(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "PlayStation Vita");
}

static jboolean jni_hasFeature(jmethodID id, va_list args) {
	(void)id;
	jstring feature_jstr = va_arg(args, jstring);

	if (!feature_jstr)
		return JNI_FALSE;

	const char *feature = jni->GetStringUTFChars(&jni, feature_jstr, NULL);
	if (!feature)
		return JNI_FALSE;

	// Conservative defaults so the Android game path doesn't assume unsupported
	// platform capabilities on Vita.
	jboolean supported = JNI_FALSE;
	if (strcmp(feature, "android.hardware.touchscreen") == 0)
		supported = JNI_TRUE;

	jni->ReleaseStringUTFChars(&jni, feature_jstr, (char *)feature);
	return supported;
}

static void jni_setFramebufferSize(jmethodID id, va_list args) {
	(void)id;
	(void)va_arg(args, int);
	(void)va_arg(args, int);
}

static jint jni_getSampleRate(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 48000;
}

static jint jni_getOutputFramesPerBuffer(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 1024;
}

static jboolean jni_isUsingBluetooth(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return JNI_FALSE;
}

static jboolean jni_hasPermission(jmethodID id, va_list args) {
	(void)id;
	jstring permission_jstr = va_arg(args, jstring);
	if (!permission_jstr)
		return JNI_TRUE;

	const char *permission = jni->GetStringUTFChars(&jni, permission_jstr, NULL);
	if (!permission)
		return JNI_TRUE;

	jboolean granted = JNI_TRUE;
	if (strcmp(permission, "android.permission.READ_EXTERNAL_STORAGE") == 0 ||
			strcmp(permission, "android.permission.WRITE_EXTERNAL_STORAGE") == 0) {
		granted = JNI_TRUE;
	}

	jni->ReleaseStringUTFChars(&jni, permission_jstr, (char *)permission);
	return granted;
}

static void jni_requestPermission(jmethodID id, va_list args) {
	(void)id;
	int request_code = va_arg(args, int);

	native_on_permission_complete_fn cb = resolve_native_permission_callback();
	if (cb) {
		cb(&jni, NULL, request_code, JNI_TRUE);
	}
}

static jobject jni_getHardwareDisplay(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "qHD");
}

static jobject jni_getHardwareManufacturer(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "Sony");
}

static jobject jni_getLocale(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "en_US");
}

static jobject jni_getHardwareOS(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "PlayStation Vita");
}

static jobject jni_getHardwareBoard(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "PCH-1000");
}

static jfloat jni_getXDPI(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 220.0f;
}

static jfloat jni_getYDPI(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 220.0f;
}

/*
 * JNI Methods
*/

NameToMethodID nameToMethodId[] = {
		{ 100, "getObbFileName", METHOD_TYPE_OBJECT },
		{ 101, "getExternalStorageDirectory", METHOD_TYPE_OBJECT },
		{ 102, "getExternalStorageDirs", METHOD_TYPE_OBJECT },
		{ 103, "getHardwareModel", METHOD_TYPE_OBJECT },
		{ 104, "hasFeature", METHOD_TYPE_BOOLEAN },
		{ 105, "setFramebufferSize", METHOD_TYPE_VOID },
		{ 106, "getSampleRate", METHOD_TYPE_INT },
		{ 107, "getOutputFramesPerBuffer", METHOD_TYPE_INT },
		{ 108, "isUsingBluetooth", METHOD_TYPE_BOOLEAN },
		{ 109, "getHardwareDisplay", METHOD_TYPE_OBJECT },
		{ 110, "getHardwareManufacturer", METHOD_TYPE_OBJECT },
		{ 111, "getLocale", METHOD_TYPE_OBJECT },
		{ 112, "getHardwareOS", METHOD_TYPE_OBJECT },
		{ 113, "getHardwareBoard", METHOD_TYPE_OBJECT },
		{ 114, "getXDPI", METHOD_TYPE_FLOAT },
		{ 115, "getYDPI", METHOD_TYPE_FLOAT },
		{ 116, "hasPermission", METHOD_TYPE_BOOLEAN },
		{ 117, "requestPermission", METHOD_TYPE_VOID },
};

MethodsBoolean methodsBoolean[] = {
		{ 104, jni_hasFeature },
		{ 108, jni_isUsingBluetooth },
		{ 116, jni_hasPermission },
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {
		{ 114, jni_getXDPI },
		{ 115, jni_getYDPI },
};
MethodsInt methodsInt[] = {
		{ 106, jni_getSampleRate },
		{ 107, jni_getOutputFramesPerBuffer },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
		{ 100, jni_getObbFileName },
		{ 101, jni_getExternalStorageDirectory },
		{ 102, jni_getExternalStorageDirs },
		{ 103, jni_getHardwareModel },
		{ 109, jni_getHardwareDisplay },
		{ 110, jni_getHardwareManufacturer },
		{ 111, jni_getLocale },
		{ 112, jni_getHardwareOS },
		{ 113, jni_getHardwareBoard },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
		{ 105, jni_setFramebufferSize },
		{ 117, jni_requestPermission },
};

/*
 * JNI Fields
*/

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
// Possible values: https://developer.android.com/reference/android/os/Build.VERSION_CODES
const int SDK_INT = 19; // Android 4.4 / KitKat

NameToFieldID nameToFieldId[] = {
		{ 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT }, 
		{ 1, "SDK_INT", FIELD_TYPE_INT },
		// SDL on Android asks org/libsdl/app/SDLActivity.mAssetMgr and passes
		// it to AAssetManager_fromJava(). On Vita we do not use real Java state,
		// but exposing this field keeps the JNI probe path alive and lets the
		// native filesystem-backed AAssetManager shim take over.
		{ 2, "mAssetMgr", FIELD_TYPE_OBJECT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
		{ 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ 0, WINDOW_SERVICE },
		{ 2, (jobject)0x42424242 },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
