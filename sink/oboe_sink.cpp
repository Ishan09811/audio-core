// SPDX-FileCopyrightText: Copyright 2024 Oboe Integration Project
// SPDX-License-Identifier: MPL-2.0

#include <span>
#include <vector>

#include <audio_core/common/common.h>
#include <audio_core/sink/oboe_sink.h>
#include <audio_core/sink/sink_stream.h>
#include <audio_core/common/logging/log.h>
#include <core/core.h>

#include <oboe/Oboe.h>

namespace AudioCore::Sink {

/**
 * Oboe sink stream, responsible for sinking samples to hardware.
 */
class OboeSinkStream final : public SinkStream, public oboe::AudioStreamCallback {
public:
    /**
     * Create a new sink stream.
     *
     * @param device_channels_ - Number of channels supported by the hardware.
     * @param system_channels_ - Number of channels the audio systems expect.
     * @param name_            - Name of this stream.
     * @param type_            - Type of this stream.
     * @param system_          - Core system.
     */
    OboeSinkStream(u32 device_channels_, u32 system_channels_, const std::string& name_,
                   StreamType type_, Core::System& system_)
        : SinkStream(system_, type_), stream(nullptr) {
        name = name_;
        device_channels = device_channels_;
        system_channels = system_channels_;

        oboe::AudioStreamBuilder builder;
        builder.setDirection(type_ == StreamType::In ? oboe::Direction::Input : oboe::Direction::Output)
               .setChannelCount(device_channels_)
               .setSampleRate(TargetSampleRate)
               .setFormat(oboe::AudioFormat::I16)
               .setPerformanceMode(oboe::PerformanceMode::LowLatency)
               .setCallback(this);

        oboe::Result result = builder.openStream(&stream);
        if (result != oboe::Result::OK) {
            LOG_CRITICAL(Audio_Sink, "Error initializing Oboe stream: {}", oboe::convertToText(result));
            return;
        }

        LOG_INFO(Service_Audio, "Opened Oboe stream {} with: rate {} channels {}",
                 name, TargetSampleRate, device_channels_);
    }

    /**
     * Destroy the sink stream.
     */
    ~OboeSinkStream() override {
        if (stream) {
            stream->close();
        }
    }

    /**
     * Start the sink stream.
     */
    void Start(bool resume = false) override {
        if (!stream || !paused) return;
        paused = false;
        oboe::Result result = stream->requestStart();
        if (result != oboe::Result::OK) {
            LOG_CRITICAL(Audio_Sink, "Error starting Oboe stream: {}", oboe::convertToText(result));
        }
    }

    /**
     * Stop the sink stream.
     */
    void Stop() override {
        if (!stream || paused) return;
        paused = true;
        oboe::Result result = stream->requestStop();
        if (result != oboe::Result::OK) {
            LOG_CRITICAL(Audio_Sink, "Error stopping Oboe stream: {}", oboe::convertToText(result));
        }
    }

    /**
     * Oboe callback when audio data is needed or received.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override {
        const std::size_t frame_size = device_channels;
        std::span<s16> buffer{reinterpret_cast<s16*>(audioData), numFrames * frame_size};

        if (type == StreamType::In) {
            ProcessAudioIn(buffer, numFrames);
        } else {
            ProcessAudioOutAndRender(buffer, numFrames);
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    /// Oboe stream backend
    oboe::AudioStream* stream;
};

OboeSink::OboeSink(std::string_view target_device_name) {
    LOG_INFO(Audio_Sink, "Initializing Oboe sink with device: {}", target_device_name);

    // Here, we could set up specific device ID if supported
    // Oboe manages output device automatically by default
}

OboeSink::~OboeSink() {
    for (auto& sink_stream : sink_streams) {
        sink_stream.reset();
    }
}

SinkStream* OboeSink::AcquireSinkStream(Core::System& system, u32 system_channels,
                                        const std::string& name, StreamType type) {
    SinkStreamPtr& stream = sink_streams.emplace_back(std::make_unique<OboeSinkStream>(
        device_channels, system_channels, name, type, system));

    return stream.get();
}

void OboeSink::CloseStream(SinkStream* stream) {
    sink_streams.erase(std::remove_if(sink_streams.begin(), sink_streams.end(),
        [stream](const auto& s) { return s.get() == stream; }), sink_streams.end());
}

void OboeSink::CloseStreams() {
    sink_streams.clear();
}

f32 OboeSink::GetDeviceVolume() const {
    if (sink_streams.empty()) {
        return 1.0f;
    }

    return sink_streams.front()->GetDeviceVolume();
}

void OboeSink::SetDeviceVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetDeviceVolume(volume);
    }
}

void OboeSink::SetSystemVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetSystemVolume(volume);
    }
}

u32 GetOboeLatency() {
    for (auto& stream : sink_streams) {
        int32_t buffer_size = stream->getBufferSizeInFrames();
        int32_t sample_rate = stream->getSampleRate();

        if (sample_rate > 0) {
            return static_cast<u32>((buffer_size * 1000) / sample_rate);  // Latency in ms
        }
        return 0;
    }
}

} // namespace AudioCore::Sink
