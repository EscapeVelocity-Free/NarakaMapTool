#ifndef MAP_TRANSFORM_H
#define MAP_TRANSFORM_H

#include <array>

struct MapScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

class MapTransform {
public:
    static constexpr int kMaxZoomSteps = 32;
    static constexpr double kMapCoordinateSize = 2048.0;

    MapTransform();

    void reset(double originX, double originY, double viewportSize);
    bool setZoomScales(const std::array<double, kMaxZoomSteps + 1>& scales);

    bool applyWheelDelta(int wheelDelta, double anchorX, double anchorY);
    bool applyWheelSteps(int steps, double anchorX, double anchorY);

    MapScreenPoint mapToScreen(double mapX, double mapY) const;
    double mapLengthToScreen(double mapLength) const;
    bool isScreenPointInsideViewport(double screenX, double screenY) const;

    int zoomStep() const;
    int stepsToMaxZoom() const;
    double zoomScale() const;
    double viewportSize() const;

private:
    static std::array<double, kMaxZoomSteps + 1> makeDefaultZoomScales();
    void clampOriginToViewport();

    std::array<double, kMaxZoomSteps + 1> m_zoomScales;
    double m_viewportX = 0.0;
    double m_viewportY = 0.0;
    double m_originX = 0.0;
    double m_originY = 0.0;
    double m_viewportSize = 0.0;
    double m_pixelsPerMapUnit = 0.0;
    int m_zoomStep = 0;
};

#endif
