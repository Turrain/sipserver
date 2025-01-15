// webrtc/rtc_base/numerics/saturated_cast.h
#ifndef WEBRTC_RTC_BASE_NUMERICS_SATURATED_CAST_H_
#define WEBRTC_RTC_BASE_NUMERICS_SATURATED_CAST_H_

#include <limits>
#include <type_traits>

namespace rtc {

template<typename Dst, typename Src>
struct saturated_cast_impl {
    static Dst Do(Src value)
    {
        // For same types, just return
        if (std::is_same<Dst, Src>::value)
            return static_cast<Dst>(value);

        // Handle unsigned to signed
        if (!std::numeric_limits<Src>::is_signed && std::numeric_limits<Dst>::is_signed) {
            if (value > static_cast<Src>(std::numeric_limits<Dst>::max()))
                return std::numeric_limits<Dst>::max();
            return static_cast<Dst>(value);
        }

        // Handle signed to unsigned
        if (std::numeric_limits<Src>::is_signed && !std::numeric_limits<Dst>::is_signed) {
            if (value < 0)
                return 0;
            if (static_cast<typename std::make_unsigned<Src>::type>(value) > std::numeric_limits<Dst>::max())
                return std::numeric_limits<Dst>::max();
            return static_cast<Dst>(value);
        }

        // Handle signed to signed
        if (value > static_cast<Src>(std::numeric_limits<Dst>::max()))
            return std::numeric_limits<Dst>::max();
        if (value < static_cast<Src>(std::numeric_limits<Dst>::min()))
            return std::numeric_limits<Dst>::min();
        return static_cast<Dst>(value);
    }
};

template<typename Dst, typename Src>
inline Dst saturated_cast(Src value)
{
    return saturated_cast_impl<Dst, Src>::Do(value);
}

} // namespace rtc

#endif // WEBRTC_RTC_BASE_NUMERICS_SATURATED_CAST_H_