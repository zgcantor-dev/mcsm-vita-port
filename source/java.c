#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#define TELLTALE_PKG_NAME "com.telltalegames.minecraft100"
#define TELLTALE_OBB_VERSION "40137"
#define TELLTALE_EXTERNAL_STORAGE_ROOT "/storage/emulated/0"

#define TELLTALE_MAIN_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/main." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"
#define TELLTALE_PATCH_OBB_ANDROID_PATH TELLTALE_EXTERNAL_STORAGE_ROOT "/Android/obb/" TELLTALE_PKG_NAME "/patch." TELLTALE_OBB_VERSION "." TELLTALE_PKG_NAME ".obb"

static jobject jni_getObbFileName(jmethodID id, va_list args) {
	(void)id;
	(void)args;

	// Some builds call getObbFileName() once for main and derive patch path,
	// while others request both main and patch every startup attempt.
	// Alternate main/patch deterministically so repeated initialization loops
	// always receive a valid pair in order.
	static int call_count = 0;
	const char *path = ((call_count++ & 1) == 0) ? TELLTALE_MAIN_OBB_ANDROID_PATH : TELLTALE_PATCH_OBB_ANDROID_PATH;
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

/*
 * JNI Methods
*/

NameToMethodID nameToMethodId[] = {
		{ 100, "getObbFileName", METHOD_TYPE_OBJECT },
		{ 101, "getExternalStorageDirectory", METHOD_TYPE_OBJECT },
		{ 102, "getExternalStorageDirs", METHOD_TYPE_OBJECT },
};

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
		{ 100, jni_getObbFileName },
		{ 101, jni_getExternalStorageDirectory },
		{ 102, jni_getExternalStorageDirs },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {};

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
