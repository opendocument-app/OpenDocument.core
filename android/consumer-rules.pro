# The native library resolves the java side by name: `FindClass`,
# `GetMethodID`, `GetFieldID` and enum ordinals (see `jni/src/jni_convert.hpp`
# and `jni_style.cpp`). R8 has no way to see those references, so anything it
# renames or strips turns into a NoSuchMethodError from a native frame at
# runtime — keep the whole binding surface instead.
-keep class app.opendocument.core.** { *; }

# Native methods and the classes declaring them.
-keepclasseswithmembernames class * {
    native <methods>;
}
