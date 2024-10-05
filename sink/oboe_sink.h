// SPDX-FileCopyrightText: Copyright 2024 Oboe Integration Project
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <string>
#include <vector>

#include <audio_core/sink/sink.h>
#include <oboe/Oboe.h>

namespace Core {
class System;
}

namespace AudioCore::Sink {
class SinkStream;

/**
 * Oboe backend sink, holds multiple output streams and is responsible for sinking samples to
 * hardware. Used by Audio Render, Audio In, and Audio Out.
 */
class OboeSink final : public Sink {
public:
    explicit OboeSink(std::string_view device_id);
    ~OboeSink() override;

    SinkStream* AcquireSinkStream(Core::System& system, u32 system_channels,
                                  const std::string& name, StreamType type) override;

    void CloseStream(SinkStream* stream) override;
    void CloseStreams() override;

    f32 GetDeviceVolume() const override;
    void SetDeviceVolume(f32 volume) override;
    void SetSystemVolume(f32 volume) override;

private:
    /// Vector of streams managed by this sink
    std::vector<SinkStreamPtr> sink_streams{};
    /// Number of channels supported by the device
    u32 device_channels{2};  // Default to stereo
};

} // namespace AudioCore::Sink
