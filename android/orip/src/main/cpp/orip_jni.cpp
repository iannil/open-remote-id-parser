#include <jni.h>
#include <string>
#include <vector>
#include "orip/orip.h"

// Cache class and method IDs for performance
static struct {
    jclass locationDataClass;
    jmethodID locationDataFromNative;

    jclass systemInfoClass;
    jmethodID systemInfoFromNative;

    jclass uavObjectClass;
    jmethodID uavObjectFromNative;

    jclass parseResultClass;
    jmethodID parseResultConstructor;

    jclass arrayListClass;
    jmethodID arrayListConstructor;
    jmethodID arrayListAdd;
} g_cache;

// Parser wrapper with callbacks
struct ParserWrapper {
    orip::RemoteIDParser parser;
    JavaVM* jvm;
    jobject parserRef;  // Global reference to Kotlin parser object
    bool callbacksEnabled[3];  // new, update, timeout

    ParserWrapper(const orip::ParserConfig& config)
        : parser(config), jvm(nullptr), parserRef(nullptr) {
        callbacksEnabled[0] = false;
        callbacksEnabled[1] = false;
        callbacksEnabled[2] = false;
    }
};

// Get JNI environment for current thread
static JNIEnv* getEnv(JavaVM* jvm) {
    JNIEnv* env = nullptr;
    jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    return env;
}

// Convert C++ UAVObject to Kotlin UAVObject
static jobject convertUAVToKotlin(JNIEnv* env, const orip::UAVObject& uav) {
    // Create LocationData
    jobject location = env->CallStaticObjectMethod(
        g_cache.locationDataClass,
        g_cache.locationDataFromNative,
        uav.location.valid,
        uav.location.latitude,
        uav.location.longitude,
        uav.location.altitude_baro,
        uav.location.altitude_geo,
        uav.location.height,
        uav.location.speed_horizontal,
        uav.location.speed_vertical,
        uav.location.direction,
        static_cast<jint>(uav.location.status)
    );

    // Create SystemInfo
    jobject system = env->CallStaticObjectMethod(
        g_cache.systemInfoClass,
        g_cache.systemInfoFromNative,
        uav.system.valid,
        uav.system.operator_latitude,
        uav.system.operator_longitude,
        uav.system.area_ceiling,
        uav.system.area_floor,
        static_cast<jint>(uav.system.area_count),
        static_cast<jint>(uav.system.area_radius),
        static_cast<jlong>(uav.system.timestamp)
    );

    // Strings
    jstring id = env->NewStringUTF(uav.id.c_str());
    jstring selfId = uav.self_id.valid ?
        env->NewStringUTF(uav.self_id.description.c_str()) : nullptr;
    jstring operatorId = uav.operator_id.valid ?
        env->NewStringUTF(uav.operator_id.id.c_str()) : nullptr;

    // Calculate lastSeenMs
    auto duration = uav.last_seen.time_since_epoch();
    jlong lastSeenMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // Create UAVObject
    jobject result = env->CallStaticObjectMethod(
        g_cache.uavObjectClass,
        g_cache.uavObjectFromNative,
        id,
        static_cast<jint>(uav.id_type),
        static_cast<jint>(uav.uav_type),
        static_cast<jint>(uav.protocol),
        static_cast<jint>(uav.transport),
        static_cast<jint>(uav.rssi),
        lastSeenMs,
        location,
        system,
        selfId,
        operatorId,
        static_cast<jint>(uav.message_count)
    );

    // Clean up local refs
    env->DeleteLocalRef(id);
    env->DeleteLocalRef(location);
    env->DeleteLocalRef(system);
    if (selfId) env->DeleteLocalRef(selfId);
    if (operatorId) env->DeleteLocalRef(operatorId);

    return result;
}

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // Cache LocationData class and method
    jclass localClass = env->FindClass("com/orip/LocationData");
    g_cache.locationDataClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    g_cache.locationDataFromNative = env->GetStaticMethodID(
        g_cache.locationDataClass, "fromNative",
        "(ZDDFFFFFFFILcom/orip/LocationData;");
    env->DeleteLocalRef(localClass);

    // For Kotlin companion object, we need to use the Companion class
    localClass = env->FindClass("com/orip/LocationData$Companion");
    g_cache.locationDataFromNative = env->GetMethodID(
        localClass, "fromNative",
        "(ZDDFFFFFFFILcom/orip/LocationData;");
    env->DeleteLocalRef(localClass);

    // Cache SystemInfo
    localClass = env->FindClass("com/orip/SystemInfo");
    g_cache.systemInfoClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);

    localClass = env->FindClass("com/orip/SystemInfo$Companion");
    g_cache.systemInfoFromNative = env->GetMethodID(
        localClass, "fromNative",
        "(ZDDFFIIJLcom/orip/SystemInfo;");
    env->DeleteLocalRef(localClass);

    // Cache UAVObject
    localClass = env->FindClass("com/orip/UAVObject");
    g_cache.uavObjectClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);

    localClass = env->FindClass("com/orip/UAVObject$Companion");
    g_cache.uavObjectFromNative = env->GetMethodID(
        localClass, "fromNative",
        "(Ljava/lang/String;IIIIIJLcom/orip/LocationData;Lcom/orip/SystemInfo;Ljava/lang/String;Ljava/lang/String;I)Lcom/orip/UAVObject;");
    env->DeleteLocalRef(localClass);

    // Cache ParseResult
    localClass = env->FindClass("com/orip/ParseResult");
    g_cache.parseResultClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    g_cache.parseResultConstructor = env->GetMethodID(
        g_cache.parseResultClass, "<init>",
        "(ZZLcom/orip/ProtocolType;Ljava/lang/String;Lcom/orip/UAVObject;)V");
    env->DeleteLocalRef(localClass);

    // Cache ArrayList
    localClass = env->FindClass("java/util/ArrayList");
    g_cache.arrayListClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    g_cache.arrayListConstructor = env->GetMethodID(g_cache.arrayListClass, "<init>", "()V");
    g_cache.arrayListAdd = env->GetMethodID(g_cache.arrayListClass, "add", "(Ljava/lang/Object;)Z");
    env->DeleteLocalRef(localClass);

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env = getEnv(vm);
    if (env) {
        env->DeleteGlobalRef(g_cache.locationDataClass);
        env->DeleteGlobalRef(g_cache.systemInfoClass);
        env->DeleteGlobalRef(g_cache.uavObjectClass);
        env->DeleteGlobalRef(g_cache.parseResultClass);
        env->DeleteGlobalRef(g_cache.arrayListClass);
    }
}

JNIEXPORT jstring JNICALL
Java_com_orip_RemoteIDParser_00024Companion_getVersion(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF(orip::VERSION);
}

JNIEXPORT jlong JNICALL
Java_com_orip_RemoteIDParser_nativeCreate(
    JNIEnv* env, jobject thiz,
    jint timeoutMs, jboolean enableDedup,
    jboolean enableAstm, jboolean enableAsd, jboolean enableCn
) {
    orip::ParserConfig config;
    config.uav_timeout_ms = static_cast<uint32_t>(timeoutMs);
    config.enable_deduplication = enableDedup;
    config.enable_astm = enableAstm;
    config.enable_asd = enableAsd;
    config.enable_cn = enableCn;

    auto* wrapper = new ParserWrapper(config);
    env->GetJavaVM(&wrapper->jvm);
    wrapper->parserRef = env->NewGlobalRef(thiz);
    wrapper->parser.init();

    return reinterpret_cast<jlong>(wrapper);
}

JNIEXPORT void JNICALL
Java_com_orip_RemoteIDParser_nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (wrapper) {
        if (wrapper->parserRef) {
            env->DeleteGlobalRef(wrapper->parserRef);
        }
        delete wrapper;
    }
}

JNIEXPORT jobject JNICALL
Java_com_orip_RemoteIDParser_nativeParse(
    JNIEnv* env, jobject thiz,
    jlong handle, jbyteArray payload, jint rssi, jint transport
) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (!wrapper) return nullptr;

    // Convert payload
    jsize len = env->GetArrayLength(payload);
    jbyte* bytes = env->GetByteArrayElements(payload, nullptr);
    std::vector<uint8_t> data(reinterpret_cast<uint8_t*>(bytes),
                               reinterpret_cast<uint8_t*>(bytes) + len);
    env->ReleaseByteArrayElements(payload, bytes, JNI_ABORT);

    // Parse
    auto result = wrapper->parser.parse(
        data,
        static_cast<int8_t>(rssi),
        static_cast<orip::TransportType>(transport)
    );

    // Get ProtocolType enum value
    jclass protocolTypeClass = env->FindClass("com/orip/ProtocolType");
    jmethodID fromValue = env->GetStaticMethodID(
        protocolTypeClass, "fromValue", "(I)Lcom/orip/ProtocolType;");
    jobject protocolEnum = env->CallStaticObjectMethod(
        protocolTypeClass, fromValue, static_cast<jint>(result.protocol));

    // Error string
    jstring error = result.error.empty() ? nullptr :
        env->NewStringUTF(result.error.c_str());

    // UAV object (if successful)
    jobject uav = nullptr;
    if (result.success) {
        uav = convertUAVToKotlin(env, result.uav);
    }

    // Create ParseResult
    jobject parseResult = env->NewObject(
        g_cache.parseResultClass,
        g_cache.parseResultConstructor,
        result.success,
        result.is_remote_id,
        protocolEnum,
        error,
        uav
    );

    // Cleanup
    env->DeleteLocalRef(protocolTypeClass);
    if (error) env->DeleteLocalRef(error);
    if (uav) env->DeleteLocalRef(uav);

    return parseResult;
}

JNIEXPORT jint JNICALL
Java_com_orip_RemoteIDParser_nativeGetActiveCount(JNIEnv* env, jobject thiz, jlong handle) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    return wrapper ? static_cast<jint>(wrapper->parser.getActiveCount()) : 0;
}

JNIEXPORT jobject JNICALL
Java_com_orip_RemoteIDParser_nativeGetActiveUAVs(JNIEnv* env, jobject thiz, jlong handle) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);

    // Create ArrayList
    jobject list = env->NewObject(g_cache.arrayListClass, g_cache.arrayListConstructor);

    if (wrapper) {
        auto uavs = wrapper->parser.getActiveUAVs();
        for (const auto& uav : uavs) {
            jobject kotlinUav = convertUAVToKotlin(env, uav);
            env->CallBooleanMethod(list, g_cache.arrayListAdd, kotlinUav);
            env->DeleteLocalRef(kotlinUav);
        }
    }

    return list;
}

JNIEXPORT jobject JNICALL
Java_com_orip_RemoteIDParser_nativeGetUAV(
    JNIEnv* env, jobject thiz, jlong handle, jstring id
) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (!wrapper) return nullptr;

    const char* idStr = env->GetStringUTFChars(id, nullptr);
    const orip::UAVObject* uav = wrapper->parser.getUAV(idStr);
    env->ReleaseStringUTFChars(id, idStr);

    if (!uav) return nullptr;
    return convertUAVToKotlin(env, *uav);
}

JNIEXPORT void JNICALL
Java_com_orip_RemoteIDParser_nativeClear(JNIEnv* env, jobject thiz, jlong handle) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (wrapper) {
        wrapper->parser.clear();
    }
}

JNIEXPORT jint JNICALL
Java_com_orip_RemoteIDParser_nativeCleanup(JNIEnv* env, jobject thiz, jlong handle) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (!wrapper) return 0;

    size_t before = wrapper->parser.getActiveCount();
    wrapper->parser.cleanup();
    size_t after = wrapper->parser.getActiveCount();
    return static_cast<jint>(before - after);
}

JNIEXPORT void JNICALL
Java_com_orip_RemoteIDParser_nativeSetCallbacksEnabled(
    JNIEnv* env, jobject thiz, jlong handle,
    jboolean newUav, jboolean update, jboolean timeout
) {
    auto* wrapper = reinterpret_cast<ParserWrapper*>(handle);
    if (!wrapper) return;

    wrapper->callbacksEnabled[0] = newUav;
    wrapper->callbacksEnabled[1] = update;
    wrapper->callbacksEnabled[2] = timeout;

    // Set up callbacks if enabled
    if (newUav) {
        wrapper->parser.setOnNewUAV([wrapper](const orip::UAVObject& uav) {
            JNIEnv* env = getEnv(wrapper->jvm);
            if (!env) return;

            jobject kotlinUav = convertUAVToKotlin(env, uav);
            jclass cls = env->GetObjectClass(wrapper->parserRef);
            jmethodID method = env->GetMethodID(cls, "onNativeNewUAV", "(Lcom/orip/UAVObject;)V");
            env->CallVoidMethod(wrapper->parserRef, method, kotlinUav);
            env->DeleteLocalRef(kotlinUav);
            env->DeleteLocalRef(cls);
        });
    } else {
        wrapper->parser.setOnNewUAV(nullptr);
    }

    if (update) {
        wrapper->parser.setOnUAVUpdate([wrapper](const orip::UAVObject& uav) {
            JNIEnv* env = getEnv(wrapper->jvm);
            if (!env) return;

            jobject kotlinUav = convertUAVToKotlin(env, uav);
            jclass cls = env->GetObjectClass(wrapper->parserRef);
            jmethodID method = env->GetMethodID(cls, "onNativeUAVUpdate", "(Lcom/orip/UAVObject;)V");
            env->CallVoidMethod(wrapper->parserRef, method, kotlinUav);
            env->DeleteLocalRef(kotlinUav);
            env->DeleteLocalRef(cls);
        });
    } else {
        wrapper->parser.setOnUAVUpdate(nullptr);
    }

    if (timeout) {
        wrapper->parser.setOnUAVTimeout([wrapper](const orip::UAVObject& uav) {
            JNIEnv* env = getEnv(wrapper->jvm);
            if (!env) return;

            jobject kotlinUav = convertUAVToKotlin(env, uav);
            jclass cls = env->GetObjectClass(wrapper->parserRef);
            jmethodID method = env->GetMethodID(cls, "onNativeUAVTimeout", "(Lcom/orip/UAVObject;)V");
            env->CallVoidMethod(wrapper->parserRef, method, kotlinUav);
            env->DeleteLocalRef(kotlinUav);
            env->DeleteLocalRef(cls);
        });
    } else {
        wrapper->parser.setOnUAVTimeout(nullptr);
    }
}

} // extern "C"
