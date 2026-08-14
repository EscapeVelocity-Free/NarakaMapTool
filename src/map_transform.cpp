#include "map_transform.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kWheelDelta = 120;
// Video calibration shows the game reaches approximately 12x at 32 wheel steps.
constexpr double kGameMaxZoomScale = 12.0;
constexpr double kScaleEpsilon = 0.000001;
}

MapTransform::MapTransform() : m_zoomScales(makeDefaultZoomScales()) {}

std::array<double, MapTransform::kMaxZoomSteps + 1> MapTransform::makeDefaultZoomScales() {
    std::array<double, kMaxZoomSteps + 1> scales{};
    scales[0] = 1.0;
    for (int step = 1; step <= kMaxZoomSteps; ++step) {
        scales[step] = std::pow(kGameMaxZoomScale,
            static_cast<double>(step) / static_cast<double>(kMaxZoomSteps));
    }
    return scales;
}

void MapTransform::reset(double originX, double originY, double viewportSize) {
    m_viewportX = originX;
    m_viewportY = originY;
    m_originX = originX;
    m_originY = originY;
    m_viewportSize = std::max(0.0, viewportSize);
    m_pixelsPerMapUnit = m_viewportSize / kMapCoordinateSize;
    m_zoomStep = 0;
}

bool MapTransform::setZoomScales(const std::array<double, kMaxZoomSteps + 1>& scales) {
    if (scales[0] <= 0.0 || std::abs(scales[0] - 1.0) > kScaleEpsilon) {
        return false;
    }

    for (int step = 1; step <= kMaxZoomSteps; ++step) {
        if (scales[step] <= scales[step - 1]) {
            return false;
        }
    }

    m_zoomScales = scales;
    return true;
}

bool MapTransform::applyWheelDelta(int wheelDelta, double anchorX, double anchorY) {
    if (wheelDelta == 0) {
        return false;
    }

    const int steps = wheelDelta / kWheelDelta;
    if (steps == 0) {
        return false;
    }
    return applyWheelSteps(steps, anchorX, anchorY);
}

bool MapTransform::applyWheelSteps(int steps, double anchorX, double anchorY) {
    if (steps == 0 || m_viewportSize <= 0.0) {
        return false;
    }

    const int direction = steps > 0 ? 1 : -1;
    const int count = std::abs(steps);
    bool changed = false;

    for (int index = 0; index < count; ++index) {
        const int nextStep = std::clamp(m_zoomStep + direction, 0, kMaxZoomSteps);
        if (nextStep == m_zoomStep) {
            break;
        }

        if (nextStep == 0) {
            // The game's base view is a fixed full-map viewport. Do not retain
            // a pan offset accumulated while zooming around different anchors.
            m_originX = m_viewportX;
            m_originY = m_viewportY;
        }
        else {
            const double scaleRatio = m_zoomScales[nextStep] / m_zoomScales[m_zoomStep];
            m_originX = anchorX - (anchorX - m_originX) * scaleRatio;
            m_originY = anchorY - (anchorY - m_originY) * scaleRatio;
        }
        m_zoomStep = nextStep;
        clampOriginToViewport();
        changed = true;
    }

    return changed;
}

MapScreenPoint MapTransform::mapToScreen(double mapX, double mapY) const {
    const double scale = m_pixelsPerMapUnit * m_zoomScales[m_zoomStep];
    return {m_originX + mapX * scale, m_originY + mapY * scale};
}

double MapTransform::mapLengthToScreen(double mapLength) const {
    return mapLength * m_pixelsPerMapUnit * m_zoomScales[m_zoomStep];
}

bool MapTransform::isScreenPointInsideViewport(double screenX, double screenY) const {
    return screenX >= m_viewportX && screenX <= m_viewportX + m_viewportSize &&
        screenY >= m_viewportY && screenY <= m_viewportY + m_viewportSize;
}

int MapTransform::zoomStep() const {
    return m_zoomStep;
}

int MapTransform::stepsToMaxZoom() const {
    return kMaxZoomSteps - m_zoomStep;
}

double MapTransform::zoomScale() const {
    return m_zoomScales[m_zoomStep];
}

double MapTransform::viewportSize() const {
    return m_viewportSize;
}

void MapTransform::clampOriginToViewport() {
    const double totalMapSize = mapLengthToScreen(kMapCoordinateSize);
    if (totalMapSize <= m_viewportSize) {
        m_originX = m_viewportX;
        m_originY = m_viewportY;
        return;
    }

    m_originX = std::clamp(
        m_originX, m_viewportX + m_viewportSize - totalMapSize, m_viewportX);
    m_originY = std::clamp(
        m_originY, m_viewportY + m_viewportSize - totalMapSize, m_viewportY);
}
