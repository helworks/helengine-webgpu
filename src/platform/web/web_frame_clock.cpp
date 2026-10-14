#include "web_frame_clock.hpp"
#include <cmath>
#include <stdexcept>
namespace helengine::webgpu {
double WebFrameClock::Advance(double timestamp) {
    if (!std::isfinite(timestamp)) throw std::invalid_argument("Frame timestamp must be finite");
    double delta = HasPrevious ? (timestamp - Previous) / 1000.0 : 0;
    if (delta < 0) throw std::invalid_argument("Frame timestamps must be monotonic");
    Previous = timestamp; HasPrevious = true; return delta;
}
}
