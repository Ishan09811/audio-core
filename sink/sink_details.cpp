// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: MPL-2.0

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <audio_core/sink/sink_details.h>
#ifdef HAVE_CUBEB
#include <audio_core/sink/cubeb_sink.h>
#endif
#ifdef HAVE_SDL2
#include <audio_core/sink/sdl2_sink.h>
#endif
#include <audio_core/sink/null_sink.h>
#include <audio_core/common/logging/log.h>

namespace AudioCore::Sink {
namespace {
struct SinkDetails {
    using FactoryFn = std::unique_ptr<Sink> (*)(std::string_view);
    using ListDevicesFn = std::vector<std::string> (*)(bool);
    using SuitableFn = bool (*)();

    /// Name for this sink.
    std::string_view id;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn factory;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
    /// Check whether this backend is suitable to be used.
    SuitableFn is_suitable;
};

// sink_details is ordered in terms of desirability, with the best choice at the top.
constexpr SinkDetails sink_details[] = {
#ifdef HAVE_CUBEB
    SinkDetails{
        "cubeb",
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<CubebSink>(device_id);
        },
        &ListCubebSinkDevices,
        &IsCubebSuitable,
    },
#endif
#ifdef HAVE_SDL2
    SinkDetails{
        "sdl2",
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<SDLSink>(device_id);
        },
        &ListSDLSinkDevices,
        &IsSDLSuitable,
    },
#endif
    SinkDetails{"null",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<NullSink>(device_id);
                },
                [](bool capture) { return std::vector<std::string>{"null"}; }, []() { return true; },
},
};

const SinkDetails& GetOutputSinkDetails(std::string_view sink_id) {
    const auto find_backend{[](std::string_view id) {
        return std::find_if(std::begin(sink_details), std::end(sink_details),
                            [&id](const auto& sink_detail) { return sink_detail.id == id; });
    }};

    auto iter = find_backend(sink_id);

    if (sink_id == "auto") {
        // Auto-select a backend. Use the sink details ordering, preferring cubeb first, checking
        // that the backend is available and suitable to use.
    for (auto& details : sink_details) {
            if (details.is_suitable()) {
                iter = &details;
                break;
            }
        }

        LOG_INFO(Service_Audio, "Auto-selecting the {} backend", iter->id);
    } else {
        if (iter != std::end(sink_details) && !iter->is_suitable()) {
            LOG_ERROR(Service_Audio, "Selected backend {} is not suitable, falling back to null", iter->id);
                iter = find_backend("null");
     }

    if (iter == std::end(sink_details)) {
        LOG_ERROR(Audio, "Invalid sink_id {}", sink_id);
        iter = find_backend("null");
    }

    return *iter;
}
} // Anonymous namespace

std::vector<std::string_view> GetSinkIDs() {
    std::vector<std::string_view> sink_ids(std::size(sink_details));

    std::transform(std::begin(sink_details), std::end(sink_details), std::begin(sink_ids),
                   [](const auto& sink) { return sink.id; });

    return sink_ids;
}

std::vector<std::string> GetDeviceListForSink(std::string_view sink_id, bool capture) {
    return GetOutputSinkDetails(sink_id).list_devices(capture);
}

std::unique_ptr<Sink> CreateSinkFromID(std::string_view sink_id, std::string_view device_id) {
    return GetOutputSinkDetails(sink_id).factory(device_id);
}

} // namespace AudioCore::Sink
