# Keep all public API
-keep class com.orip.** { *; }

# Keep native methods
-keepclasseswithmembers class * {
    native <methods>;
}
