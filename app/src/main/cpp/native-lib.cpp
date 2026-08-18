// GuitarAmpApp - motore audio a bassa latenza basato su Oboe
//
// Architettura: due stream Oboe (input e output) in modalità
// esclusiva/low-latency. Lo stream di OUTPUT ha una callback che, ad ogni
// buffer richiesto dal sistema, legge in modo non bloccante quanto
// disponibile dallo stream di INPUT, applica la simulazione dell'ampli
// (waveshaping + filtro tone) e scrive il risultato nel buffer di uscita.

#include <jni.h>
#include <atomic>
#include <cmath>
#include <memory>
#include <android/log.h>
#include <oboe/Oboe.h>

#define LOG_TAG "GuitarAmpEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::atomic<float> gGain{5.0f};
std::atomic<float> gTone{0.5f};
std::atomic<float> gVolume{0.8f};

class OnePoleFilter {
public:
    float process(float input, float cutoffCoeff) {
        state_ += cutoffCoeff * (input - state_);
        return state_;
    }
    void reset() { state_ = 0.0f; }
private:
    float state_ = 0.0f;
};

class AmpEngine : public oboe::AudioStreamDataCallback,
                   public oboe::AudioStreamErrorCallback {
public:
    bool start() {
        if (started_) return true;

        oboe::AudioStreamBuilder inBuilder;
        inBuilder.setDirection(oboe::Direction::Input)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                ->setFormat(oboe::AudioFormat::Float)
                ->setChannelCount(oboe::ChannelCount::Mono)
                ->setSampleRate(48000)
                ->setInputPreset(oboe::InputPreset::VoicePerformance);

        oboe::Result result = inBuilder.openStream(inputStream_);
        if (result != oboe::Result::OK) {
            LOGE("Impossibile aprire lo stream di input: %s", oboe::convertToText(result));
            return false;
        }

        oboe::AudioStreamBuilder outBuilder;
        outBuilder.setDirection(oboe::Direction::Output)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                ->setFormat(oboe::AudioFormat::Float)
                ->setChannelCount(oboe::ChannelCount::Stereo)
                ->setSampleRate(inputStream_->getSampleRate())
                ->setDataCallback(this)
                ->setErrorCallback(this);

        result = outBuilder.openStream(outputStream_);
        if (result != oboe::Result::OK) {
            LOGE("Impossibile aprire lo stream di output: %s", oboe::convertToText(result));
            inputStream_->close();
            return false;
        }

        conversionBuffer_.resize(outputStream_->getFramesPerBurst() * 4);

        inputStream_->requestStart();
        outputStream_->requestStart();

        toneFilter_.reset();
        started_ = true;
        LOGI("Motore avviato. SampleRate=%d, FramesPerBurst(out)=%d",
             outputStream_->getSampleRate(), outputStream_->getFramesPerBurst());
        return true;
    }

    void stop() {
        if (!started_) return;
        if (outputStream_) {
            outputStream_->requestStop();
            outputStream_->close();
            outputStream_.reset();
        }
        if (inputStream_) {
            inputStream_->requestStop();
            inputStream_->close();
            inputStream_.reset();
        }
        started_ = false;
        LOGI("Motore fermato.");
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *stream,
                                           void *audioData,
                                           int32_t numFrames) override {
        auto *output = static_cast<float *>(audioData);

        if (static_cast<size_t>(numFrames) > conversionBuffer_.size()) {
            conversionBuffer_.resize(numFrames);
        }

        auto result = inputStream_->read(conversionBuffer_.data(), numFrames, 0);

        int32_t framesRead = 0;
        if (result) {
            framesRead = result.value();
        }

        const float gain = gGain.load(std::memory_order_relaxed);
        const float tone = gTone.load(std::memory_order_relaxed);
        const float volume = gVolume.load(std::memory_order_relaxed);
        const float cutoffCoeff = 0.02f + tone * 0.5f;

        for (int32_t i = 0; i < numFrames; ++i) {
            float sample = (i < framesRead) ? conversionBuffer_[i] : 0.0f;

            sample *= gain;
            sample = std::tanh(sample);
            sample = toneFilter_.process(sample, cutoffCoeff);
            sample *= volume;

            output[i * 2] = sample;
            output[i * 2 + 1] = sample;
        }

        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override {
        LOGE("Stream chiuso per errore: %s. Provo a riavviare il motore.",
             oboe::convertToText(error));
        started_ = false;
        start();
    }

private:
    std::shared_ptr<oboe::AudioStream> inputStream_;
    std::shared_ptr<oboe::AudioStream> outputStream_;
    std::vector<float> conversionBuffer_;
    OnePoleFilter toneFilter_;
    bool started_ = false;
};

std::unique_ptr<AmpEngine> gEngine;

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_guitarampengine_NativeLib_startEngine(JNIEnv *, jobject) {
    if (!gEngine) {
        gEngine = std::make_unique<AmpEngine>();
    }
    return gEngine->start();
}

JNIEXPORT void JNICALL
Java_com_example_guitarampengine_NativeLib_stopEngine(JNIEnv *, jobject) {
    if (gEngine) {
        gEngine->stop();
    }
}

JNIEXPORT void JNICALL
Java_com_example_guitarampengine_NativeLib_setGain(JNIEnv *, jobject, jfloat value) {
    gGain.store(1.0f + value * 19.0f, std::memory_order_relaxed);
}

JNIEXPORT void JNICALL
Java_com_example_guitarampengine_NativeLib_setTone(JNIEnv *, jobject, jfloat value) {
    gTone.store(value, std::memory_order_relaxed);
}

JNIEXPORT void JNICALL
Java_com_example_guitarampengine_NativeLib_setVolume(JNIEnv *, jobject, jfloat value) {
    gVolume.store(value, std::memory_order_relaxed);
}

}  // extern "C"
