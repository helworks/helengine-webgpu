#pragma once
namespace helengine::webgpu {
/// Converts animation timestamps to engine deltas without accumulating hidden-tab time.
class WebFrameClock {
public:
    /// Returns zero on the first frame and elapsed seconds thereafter.
    double Advance(double timestampMilliseconds);
    /// Discards the previous visible frame timestamp.
    void Reset() { HasPrevious = false; }
private:
    /// Stores the preceding visible animation timestamp.
    double Previous = 0;
    /// Distinguishes an actual timestamp from an uninitialized clock.
    bool HasPrevious = false;
};
}
