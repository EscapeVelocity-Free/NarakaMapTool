#pragma once

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QPushButton>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

class QLabel;

class ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit ControlPanel(QWidget* parent = nullptr);

signals:
    // 切换地图信号，参数为 (mapId, mapName)
    void mapChanged(const std::string& mapId, const std::string& mapName);
    // 切换大风歌楼层信号，0 为地表层，1 为地下层
    void layerChanged(int layer);
    // 资源勾选信号
    void selectionChanged(const std::vector<std::string>& selectedKeys);
    void toggleBackground(bool show);

private:
    struct ResourceItem;
    struct ResourceGroup;
    struct ResourceBinding {
        std::string key;
        bool riftOnly;
    };

    static std::vector<ResourceGroup> buildResourceGroups();
    void setupWindow();
    void setupStyle();
    QFrame* createHeaderCard();
    QFrame* createResourceSection(const ResourceGroup& group);
    QPushButton* createResourceChip(const ResourceItem& item, bool riftOnly);
    QPushButton* createCommandButton(const QString& text, const QString& objectName);
    void applyCardShadow(QFrame* card);
    void connectActions(QPushButton* selectAllButton, QPushButton* clearAllButton, QCheckBox* showBackgroundBox);
    bool isRiftMapSelected() const;
    bool isStormchantMapSelected() const;
    bool isResourceAvailable(const ResourceBinding& binding) const;
    void updateRiftSectionVisibility();
    void updateLayerSelectorVisibility();
    void setAllResourcesChecked(bool checked);
    void notifySelectionChanged();

    std::map<QAbstractButton*, ResourceBinding> m_resourceButtons;
    std::vector<QFrame*> m_riftOnlySections;
    QComboBox* m_mapCombo = nullptr;
    QLabel* m_layerLabel = nullptr;
    QComboBox* m_layerCombo = nullptr;
};
