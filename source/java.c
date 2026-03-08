#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include <string.h>

#define TELLTALE_PKG_NAME "com.telltalegames.minecraft100"
#define TELLTALE_OBB_VERSION "40137"
#define TELLTALE_EXTERNAL_STORAGE_ROOT "/storage/emulated/0"

#define TELLTALE_MAIN_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/main." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"
#define TELLTALE_PATCH_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/patch." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"

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

	jni->ReleaseStringUTFChars(&jni, feature_jstr, feature);
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
};

MethodsBoolean methodsBoolean[] = {
		{ 104, jni_hasFeature },
		{ 108, jni_isUsingBluetooth },
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
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
