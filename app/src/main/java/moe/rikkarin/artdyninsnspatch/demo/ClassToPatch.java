package moe.rikkarin.artdyninsnspatch.demo;

import androidx.annotation.Keep;

@Keep
class ClassToPatch {
    public static Boolean methodToPatchBoolean() {
        return false;
    }

    public static Integer methodToPatchInteger() {
        return 0xdeadbeef;
    }

    public static Long methodToPatchLong() {
        return 0xdeadbeefdeadbeefL;
    }

    public static Float methodToPatchFloat() {
        return 13.37f;
    }

    public static Double methodToPatchDouble() {
        return 13.37;
    }

    public static String methodToPatchString() {
        return "Not modified.";
    }
}