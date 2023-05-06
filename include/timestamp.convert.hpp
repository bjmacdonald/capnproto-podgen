#pragma once

#include <sys/stat.h>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <optional>


/**
 * pod field converter for storing a chrono timestamp in a capnp uint64 as nanoseconds.
 */

namespace podgen {

using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;

inline Timestamp timestampFromNanos(uint64_t value) {
    return Timestamp(std::chrono::nanoseconds(value));
}

inline uint64_t timestampToNanos(const Timestamp& timestamp) {
    return timestamp.time_since_epoch().count();
}

inline void outTimestamp(std::ostream& os, const Timestamp& o) {
    static const auto tz = std::chrono::current_zone();
    const std::chrono::zoned_time zt{tz, o};
    os << '[' << zt << ']';;
}

/// Include an std::optional output function in case the field is declared as an optional.
/// (ADL cannot find an _out function for the Timestamp type since it is in the std namespace.)
inline void outTimestamp(std::ostream& os, const std::optional<Timestamp>& o) {
    if (o) {
        outTimestamp(os, *o);
    } else {
        os << "{}";
    }
}

} // namespace podgen


namespace std {

template <> struct hash<podgen::Timestamp> {
    std::size_t operator()(const podgen::Timestamp& o) const {
        return podgen::timestampToNanos(o);
    }
};

} // namespace std
