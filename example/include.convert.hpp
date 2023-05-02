#pragma once

#include <capnp/schema.h>

#include "include.pod.hpp"
#include "include.capnp.h"

::inc::Point podFromCapnp(::inc::capnp::Point::Reader r);
void podToCapnp(::inc::capnp::Point::Builder b, const ::inc::Point& obj);
::inc::capnp::Point capnpTypeOf(::inc::Point);
::capnp::List<::inc::capnp::Point, ::capnp::Kind::STRUCT> capnpTypeOf(std::vector<::inc::Point>);

::inc::Info podFromCapnp(::inc::capnp::Info::Reader r);
void podToCapnp(::inc::capnp::Info::Builder b, const ::inc::Info& obj);
::inc::capnp::Info capnpTypeOf(::inc::Info);
::capnp::List<::inc::capnp::Info, ::capnp::Kind::STRUCT> capnpTypeOf(std::vector<::inc::Info>);

namespace inc {

// from

template <typename CT>
auto _fromCapnp(CT v) {
    if constexpr (std::is_same_v<CT, ::capnp::Void>) {
        return std::monostate();
    } else if constexpr (std::is_arithmetic_v<CT>) {
        return v;
    } else if constexpr (std::is_same_v<CT, ::capnp::Text::Reader>) {
        return static_cast<std::string>(v);
    } else if constexpr (std::is_same_v<CT, ::capnp::Data::Reader>) {
        auto bytes = v.asBytes();
        auto begin = reinterpret_cast<const podgen::Data::value_type*>(bytes.begin());
        return podgen::Data(begin, begin + bytes.size());
    } else if constexpr (podgen::is_iterable<CT>) {
        using CE = decltype(std::declval<CT>().begin().operator*());
        using E = decltype(_fromCapnp(std::declval<CE>()));
        std::vector<E> out;
        out.reserve(v.size());
        for (auto&& ce : v) {
            out.push_back(_fromCapnp(ce));
        }
        return out;
    } else {
        return podFromCapnp(v);
    }
}

template <typename T, typename CL>
auto _fromCapnpList(CL list) {
    using CE = decltype(std::declval<CL>().begin().operator*());
    using E = typename std::decay<decltype(std::declval<T>().begin().operator*())>::type;

    T out;

    for (auto&& ce : list) {
        if constexpr (std::is_same_v<CE, ::capnp::Text::Reader>) {
            out.insert(out.end(), _fromCapnp(ce));
        } else if constexpr (std::is_same_v<CE, ::capnp::Data::Reader>) {
            out.insert(out.end(), _fromCapnp(ce));
        } else if constexpr (podgen::is_iterable<CE>) {
            out.insert(out.end(), _fromCapnpList<E>(ce));
        } else if constexpr (podgen::is_pair<E>) {
            out.insert(out.end(), std::make_pair(_fromCapnp(ce.getKey()), _fromCapnp(ce.getValue())));
        } else {
            out.insert(out.end(), _fromCapnp(ce));
        }
    }

    return out;
}


// to

template <typename T>
auto _toCapnp(T v) {
    if constexpr (std::is_arithmetic_v<T>) {
        return v;
    } else if constexpr (std::is_enum_v<T>) {
        return podToCapnp(v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return ::capnp::Text::Reader(v);
    } else if constexpr (std::is_same_v<T, podgen::Data>) {
        return ::capnp::Data::Reader(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    } else {
        return;
    }
}

template <typename B, typename T>
void _toCapnpList(B builder, const T& container) {
    using E = typename std::decay<decltype(std::declval<T>().begin().operator*())>::type;

    if (builder.size() != container.size()) {
        throw std::invalid_argument("capnp list builder must be initialized to source vector size");
    }

    uint i = 0;
    for (const auto& e : container) {
        if constexpr (!std::is_same_v<void, decltype(_toCapnp(std::declval<E>()))>) {
            builder.set(i, _toCapnp(e));
        } else if constexpr (podgen::is_pair<E>) {
            builder[i].setKey(_toCapnp(e.first));
            builder[i].setValue(_toCapnp(e.second));
        } else if constexpr (podgen::is_iterable<E>) {
            auto b = builder.init(i, e.size());
            _toCapnpList(b, e);
        } else {
            podToCapnp(builder[i], e);
        }

        i++;
    }
}

}
