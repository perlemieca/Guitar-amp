package com.example.guitarampengine

/**
 * Ponte verso il motore audio nativo (C++ / Oboe) definito in native-lib.cpp.
 */
object NativeLib {
    init {
        System.loadLibrary("guitarampengine")
    }

    external fun startEngine(): Boolean
    external fun stopEngine()
    external fun setGain(value: Float)
    external fun setTone(value: Float)
    external fun setVolume(value: Float)
}
