#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <map>
#include <string>
#include <vector>

class ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit ControlPanel(QWidget* parent = nullptr);

signals:
    // 切换地图信号：发送 (mapId, mapName)
    void mapChanged(const std::string& mapId, const std::string& mapName);
    // 资源勾选信号
    void selectionChanged(const std::vector<std::string>& selectedKeys);

    void toggleBackground(bool show);
private:
    void notifySelectionChanged();
    std::map<QCheckBox*, std::string> m_boxMap;
    QComboBox* m_mapCombo;
};