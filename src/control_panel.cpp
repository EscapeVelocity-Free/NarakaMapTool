#include "control_panel.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QColor>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QRect>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>
#include <algorithm>
#include <sstream>
#include <set>
#include <utility>

#include "config_manager.h"
#include "logger.h"

#pragma execution_character_set("utf-8")

struct ControlPanel::ResourceItem {
    const char* name;
    const char* key;
};

struct ControlPanel::ResourceGroup {
    const char* title;
    const char* subtitle;
    std::vector<ResourceItem> items;
};

namespace {
constexpr int kSingleCategoryMaxWidth = 820;
constexpr int kSingleCategoryMinWidth = 760;

class ChipFlowLayout final : public QLayout {
public:
    explicit ChipFlowLayout(QWidget* parent = nullptr) : QLayout(parent) {
        setSpacing(8);
    }

    ~ChipFlowLayout() override {
        while (auto* item = takeAt(0)) {
            delete item;
        }
    }

    void addItem(QLayoutItem* item) override {
        m_items.push_back(item);
    }

    int count() const override {
        return static_cast<int>(m_items.size());
    }

    QLayoutItem* itemAt(int index) const override {
        if (index < 0 || index >= count()) {
            return nullptr;
        }
        return m_items[static_cast<std::size_t>(index)];
    }

    QLayoutItem* takeAt(int index) override {
        if (index < 0 || index >= count()) {
            return nullptr;
        }
        auto item = m_items[static_cast<std::size_t>(index)];
        m_items.erase(m_items.begin() + index);
        return item;
    }

    Qt::Orientations expandingDirections() const override {
        return {};
    }

    bool hasHeightForWidth() const override {
        return true;
    }

    int heightForWidth(int width) const override {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override {
        QSize size;
        for (const auto* item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }

        const auto margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    QSize sizeHint() const override {
        return minimumSize();
    }

    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    int doLayout(const QRect& rect, bool testOnly) const {
        const auto margins = contentsMargins();
        const int left = rect.x() + margins.left();
        const int right = rect.right() - margins.right();
        int x = left;
        int y = rect.y() + margins.top();
        int lineHeight = 0;
        const int gap = spacing() >= 0 ? spacing() : 8;

        for (auto* item : m_items) {
            const QSize itemSize = item->sizeHint();
            if (x > left && x + itemSize.width() > right) {
                x = left;
                y += lineHeight + gap;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), itemSize));
            }
            x += itemSize.width() + gap;
            lineHeight = std::max(lineHeight, itemSize.height());
        }

        return y + lineHeight + margins.bottom() - rect.y();
    }

    std::vector<QLayoutItem*> m_items;
};

class ResourceSectionFrame final : public QFrame {
public:
    using QFrame::QFrame;

    void setChipLayout(ChipFlowLayout* chipLayout) {
        m_chipLayout = chipLayout;
        updateGeometry();
    }

    void updateMinimumHeightForWidth(int width) {
        const int requiredHeight = heightForWidth(width);
        if (requiredHeight > 0) {
            setMinimumHeight(requiredHeight);
        }
    }

    bool hasHeightForWidth() const override {
        return m_chipLayout != nullptr;
    }

    int heightForWidth(int width) const override {
        if (!m_chipLayout || !layout()) {
            return -1;
        }

        const auto margins = layout()->contentsMargins();
        const int contentWidth = std::max(0, width - margins.left() - margins.right());
        const int headerHeight = layout()->count() > 0 ? layout()->itemAt(0)->sizeHint().height() : 0;
        const int spacing = layout()->count() > 1 ? layout()->spacing() : 0;
        return margins.top() + headerHeight + spacing +
            m_chipLayout->heightForWidth(contentWidth) + margins.bottom();
    }

private:
    ChipFlowLayout* m_chipLayout = nullptr;
};

const std::set<std::string>& DefaultResourceKeys() {
    static const std::set<std::string> keys = {
        "firefly", "wishingWell", "miniShrine", "goldenToad", "flyingTarget", "fireflyCage"
    };
    return keys;
}

std::string JoinResourceKeys(const std::vector<std::string>& keys) {
    std::ostringstream stream;
    for (size_t index = 0; index < keys.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        stream << keys[index];
    }
    return stream.str();
}

}

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent),
      m_resourceCatalog((std::filesystem::path(ConfigManager::resourcePath) / "resources_naraka.json").string()) {
    setupWindow();
    setupStyle();

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 14);
    rootLayout->setSpacing(12);
    rootLayout->addWidget(createHeaderCard());

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);
    bodyLayout->addWidget(createControlSidebar(), 0, Qt::AlignTop);
    bodyLayout->addWidget(createResourcePanel(), 1);
    rootLayout->addLayout(bodyLayout, 1);

    connectActions(m_selectAllButton, m_clearAllButton, m_showBackgroundBox,
        m_backgroundOpacitySlider, m_opacityValueLabel, m_allowMapZoomBox, m_alwaysVisibleBox);
    updateLayerSelectorVisibility();
    refreshResourceVisibility();
    QTimer::singleShot(100, this, &ControlPanel::notifySelectionChanged);
    Logger::info("Control panel initialized: resource_controls={} sections={} catalog_loaded={} default_map_id={}",
        m_resourceButtons.size(), m_resourceSections.size(), m_resourceCatalog.isLoaded(),
        m_mapCombo->currentData().toString().toStdString());
}

std::vector<ControlPanel::ResourceGroup> ControlPanel::buildResourceGroups() {
    return {
        {
            "常用消耗 · 采集",
            "高频路线点，适合日常跑图",
            {
                {"萤火虫", "firefly"}, {"许愿井", "wishingWell"}, {"迷你土地", "miniShrine"},
                {"金蟾", "goldenToad"}, {"迎春靶", "flyingTarget"}, {"萤火笼", "fireflyCage"},
                {"沙梨圣果", "pear"}, {"刺梨", "pricklyPear"}, {"沙叻", "salak"},
                {"蒲公英", "dandelion"}, {"锦鲤", "koi"}
            }
        },
        {
            "任务 · 挑战",
            "任务卷轴、任务目标与地脉仪等关键交互",
            {
                {"连续寻宝任务", "questSerialCache"}, {"寻宝任务", "questCache"}, {"祈福任务", "questShrine"},
                {"礼敬任务", "questBell"}, {"追击任务", "bounty"}, {"叫阵任务", "reverseBounty"}, {"地脉仪", "strongPoint"}
            }
        },
        {
            "重要设施",
            "货郎、返魂台、宝窟与功能设施",
            {
                {"货郎", "riftDealer"}, {"返魂台", "soulAltar"}, {"武器架", "weaponRack"},
                {"宝窟", "treasureCave"}, {"封魔结界", "forbiddenSeal"}, {"回阳境", "gateOfYang"},
                {"食人芋", "carnivorousYam"}, {"雪莲", "snowLotus"}, {"汤泉眼", "springSource"},
                {"贺兰艺", "helanArt"}, {"高资源区", "highResourceZone"}
            }
        },
        {
            "其他",
            "地图上的补充资源点",
            {
                {"金堆", "gold"}, {"绿堆", "green"}, {"漂浮堆", "floatingPile"},
                {"侦察钟", "bell"}, {"攻城弩", "ballista"}, {"滴滴打车", "soaringArm"},
                {"捕兽夹", "bearTrap"}, {"野生动物", "wildlife"}, {"篝火", "bonfire"},
                {"愈柳", "healingTree"}, {"老鼠", "rat"}, {"土地庙", "earthShrine"},
                {"铁海胆", "ironUrchin"}, {"铁背龟", "ironbackTurtle"}, {"灵蚌", "spiritClam"},
                {"雷池铜钱", "treasureCoin"}, {"神仙鱼", "angelfish"}, {"青雀鱼", "azureFinchFish"},
                {"碧波珊瑚", "azureWaveCoralline"}, {"金焰珊瑚", "goldenFlameCoralline"},
                {"巨蜥", "grandLizard"}, {"刺尾鱼", "stingtailFish"}, {"紫霄珊瑚", "violetCoralline"}
            }
        },
        {
            "摸金",
            "裂隙宝箱、任务据点与收藏容器",
            {
                {"神火秘匣·史诗", "riftChestEpic"},
                {"神火秘匣·史诗（必中）", "riftChestEpicGuaranteed"},
                {"神火秘匣·传说", "riftChestLegendary"},
                {"神火秘匣·史诗（沙）", "riftChestEpicSand"},
                {"神火秘匣·史诗（雪）", "riftChestEpicSnow"},
                {"神火秘匣·史诗（雷）", "riftChestEpicThunder"},
                {"神火秘匣·传说（雷）", "riftChestLegendaryThunder"},
                {"暗门", "riftSecret"},
                {"矿车", "riftMinecart"},
                {"守卫", "riftQuestGuard"},
                {"水井", "riftWaterWell"},
                {"闸门", "riftGateOfYang"},
                {"土堆", "riftShovel"},
                {"藤鬼据点", "riftStronghold"},
                {"藤鬼据点（小首领）", "riftStrongholdMiniBoss"},
                {"藤鬼据点（首领）", "riftStrongholdBoss"},
                {"骰子", "riftDice"},
                {"轿子", "riftSedanChair"},
                {"金色遗珍", "riftGoldRelic"},
                {"武器匣", "riftWeaponBox"},
                {"气流带", "riftAirCurrent"},
                {"小杂货箱", "riftSmallHamper"},
                {"大杂货箱", "riftBigHamper"},
                {"存钱罐", "riftPiggyBank"},
                {"显宝碑", "riftSteleDecipher"},
                {"洞窟钥匙", "riftCaveKey"},
                {"黑焰兵", "riftBlackFlameSoldier"},
                {"大药柜", "riftLargeMedicineCabinet"},
                {"信件", "riftLetter"},
                {"小铭文匣", "riftSmallMakeupBox"},
                {"大藏品箱", "riftBigCollectionContainer"},
                {"大藏品柜", "riftLargeCollectionCabinet"},
                {"大杂物柜", "riftBigUtilityCabinet"},
                {"大杂货铺", "riftLargeGroceryStore"},
                {"大藏品铺", "riftLargeCollectionShop"},
                {"断章", "riftSeveredStatutes"},
                {"安魂钟", "riftSoulCalmingBell"},
                {"裂隙出口", "riftSecretExitPortal"}
            }
        }
    };
}

void ControlPanel::setupWindow() {
    setWindowTitle(QString::fromUtf8("永劫无间资源地图控制中心"));
    setMinimumSize(1000, 640);
    resize(1280, 820);
}

void ControlPanel::setupStyle() {
    setStyleSheet(R"(
        QWidget {
            background: #f5f7fb;
            color: #182230;
            font-family: "HarmonyOS Sans SC", "Microsoft YaHei UI", "Microsoft YaHei";
            font-size: 13px;
            font-weight: 500;
        }
        QFrame#headerCard {
            background: #ffffff;
            border: 1px solid #e1e7ef;
            border-radius: 12px;
        }
        QFrame#controlSidebar {
            background: #ffffff;
            border: 1px solid #e1e7ef;
            border-radius: 12px;
        }
        QFrame#sectionCard {
            background: #ffffff;
            border: 1px solid #e1e7ef;
            border-radius: 10px;
        }
        QWidget#headerContent, QWidget#resourcePanel, QWidget#resourceHeader,
        QWidget#resourceHeaderActions, QWidget#contentCanvas, QWidget#contentScroll,
        QWidget#sidebarContent, QWidget#settingGroup, QWidget#settingRow {
            background: transparent;
        }
        QLabel#heroKicker {
            background: #eff6ff;
            color: #2563eb;
            border: 1px solid #bfdbfe;
            border-radius: 6px;
            padding: 2px 7px;
            font-size: 9px;
            font-weight: 800;
        }
        QLabel#appTitle {
            background: transparent;
            color: #182230;
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#appSubtitle, QLabel#sectionSubtitle, QLabel#fieldLabel,
        QLabel#sectionDescription, QLabel#selectionSummary, QLabel#sidebarHint {
            background: transparent;
            color: #8491a3;
            font-size: 11px;
            font-weight: 500;
        }
        QLabel#sectionTitle, QLabel#resourcePanelTitle, QLabel#sidebarTitle {
            background: transparent;
            color: #182230;
            font-weight: 700;
        }
        QLabel#sectionTitle {
            font-size: 14px;
        }
        QLabel#resourcePanelTitle {
            font-size: 17px;
        }
        QLabel#sidebarTitle {
            font-size: 13px;
        }
        QLabel#sourceLink {
            background: transparent;
            color: #8491a3;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#sourceLink a {
            color: #2563eb;
            text-decoration: none;
        }
        QLabel#sourceLink a:hover {
            color: #2563eb;
            text-decoration: underline;
        }
        QFrame#sidebarDivider {
            background: transparent;
            border: 0;
            border-top: 1px solid #edf0f5;
            min-height: 1px;
            max-height: 1px;
        }
        QLabel#opacityLabel, QLabel#opacityValueLabel, QLabel#settingLabel {
            background: transparent;
            color: #526071;
            font-size: 10px;
            font-weight: 600;
        }
        QLabel#opacityValueLabel {
            min-width: 32px;
            color: #2563eb;
            font-weight: 700;
        }
        QSlider#backgroundOpacitySlider {
            min-width: 0;
            min-height: 16px;
        }
        QSlider#backgroundOpacitySlider::groove:horizontal {
            height: 4px;
            border-radius: 2px;
            background: #dfe5ee;
        }
        QSlider#backgroundOpacitySlider::sub-page:horizontal {
            border-radius: 2px;
            background: #2563eb;
        }
        QSlider#backgroundOpacitySlider::add-page:horizontal {
            border-radius: 2px;
            background: #dfe5ee;
        }
        QSlider#backgroundOpacitySlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -5px 0;
            border: 2px solid #ffffff;
            border-radius: 6px;
            background: #2563eb;
        }
        QComboBox {
            min-height: 36px;
            padding: 3px 28px 3px 10px;
            border: 1px solid #d8dee9;
            border-radius: 8px;
            background: #ffffff;
            color: #182230;
            font-weight: 600;
        }
        QComboBox:hover {
            border-color: #93b4e8;
        }
        QComboBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox#mapCombo {
            min-width: 0;
        }
        QComboBox#layerCombo {
            min-width: 0;
        }
        QComboBox::drop-down {
            border: 0;
            width: 26px;
        }
        QCheckBox {
            spacing: 0;
            color: #182230;
        }
        QCheckBox#switchCheck {
            min-width: 38px;
            max-width: 38px;
            min-height: 20px;
            max-height: 20px;
        }
        QPushButton#resourceChip {
            min-width: 76px;
            min-height: 34px;
            max-height: 34px;
            padding: 4px 12px;
            border: 1px solid #d8dee9;
            border-radius: 7px;
            background: #ffffff;
            color: #344054;
            font-size: 12px;
            font-weight: 600;
            text-align: center;
        }
        QPushButton#resourceChip:hover {
            border-color: #93b4e8;
            background: #f8fafc;
        }
        QPushButton#resourceChip:checked {
            border-color: #60a5fa;
            background: #eff6ff;
            color: #2563eb;
        }
        QPushButton#resourceChip:checked:hover {
            border-color: #3b82f6;
            background: #dbeafe;
        }
        QPushButton#resourceChip:pressed {
            background: #e0edff;
            border-color: #60a5fa;
        }
        QPushButton#resourceChip:focus {
            border: 1px solid #2563eb;
        }
        QCheckBox#switchCheck::indicator {
            width: 38px;
            height: 20px;
            border-radius: 10px;
            border: 1px solid #c7d0dd;
            background: #e4e9f0;
        }
        QCheckBox#switchCheck::indicator:checked {
            border-color: #2563eb;
            background: #2563eb;
        }
        QCheckBox#switchCheck::indicator:disabled {
            background: #eef1f5;
            border-color: #d9e0e8;
        }
        QPushButton {
            min-height: 34px;
            padding: 5px 12px;
            border-radius: 8px;
            font-weight: 700;
        }
        QPushButton#primaryButton {
            color: #ffffff;
            background: #2563eb;
            border: 1px solid #2563eb;
        }
        QPushButton#primaryButton:hover {
            background: #1d4ed8;
        }
        QPushButton#primaryButton:pressed {
            background: #1e40af;
        }
        QPushButton#primaryButton:focus {
            border: 1px solid #1d4ed8;
        }
        QPushButton#secondaryButton {
            color: #344054;
            background: #ffffff;
            border: 1px solid #d8dee9;
        }
        QPushButton#secondaryButton:hover {
            background: #f8fafc;
            border-color: #93b4e8;
        }
        QPushButton#secondaryButton:pressed {
            background: #eef2f7;
        }
        QPushButton#secondaryButton:focus {
            border: 1px solid #2563eb;
        }
        QSlider#backgroundOpacitySlider:disabled {
            background: transparent;
        }
        QSlider#backgroundOpacitySlider:disabled::groove:horizontal {
            background: #e7ebf1;
        }
        QSlider#backgroundOpacitySlider:disabled::sub-page:horizontal {
            background: #d6dde8;
        }
        QSlider#backgroundOpacitySlider:disabled::handle:horizontal {
            background: #b8c3d3;
        }
        QScrollArea#contentScroll, QWidget#contentCanvas, QWidget#resourcePanel {
            background: transparent;
            border: 0;
        }
        QScrollBar:vertical {
            width: 7px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            min-height: 42px;
            border-radius: 3px;
            background: #b8c3d3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
}

QFrame* ControlPanel::createHeaderCard() {
    auto* card = new QFrame(this);
    card->setObjectName("headerCard");
    applyCardShadow(card);
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(18, 10, 18, 10);
    layout->setSpacing(16);

    auto* titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(1);
    titleBlock->setContentsMargins(0, 0, 0, 0);

    auto* kicker = new QLabel(QString::fromUtf8("NARAKA MAP TOOL"), card);
    kicker->setObjectName("heroKicker");
    kicker->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* title = new QLabel(QString::fromUtf8("地图资源辅助"), card);
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel(QString::fromUtf8("选择地图与资源类型，覆盖层会随游戏地图自动显示。"), card);
    subtitle->setObjectName("appSubtitle");
    titleBlock->addWidget(kicker);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);

    layout->addLayout(titleBlock);
    layout->addStretch(1);

    auto* headerHint = new QLabel(QString::fromUtf8("选择地图后，在右侧筛选要显示的资源"), card);
    headerHint->setObjectName("sidebarHint");
    headerHint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(headerHint);
    return card;
}

QFrame* ControlPanel::createControlSidebar() {
    auto* sidebar = new QFrame(this);
    sidebar->setObjectName("controlSidebar");
    sidebar->setFixedWidth(252);
    sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    applyCardShadow(sidebar);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(12);

    auto* mapTitle = new QLabel(QString::fromUtf8("地图"), sidebar);
    mapTitle->setObjectName("sidebarTitle");
    layout->addWidget(mapTitle);

    auto* mapLabel = new QLabel(QString::fromUtf8("当前地图"), sidebar);
    mapLabel->setObjectName("fieldLabel");
    layout->addWidget(mapLabel);

    m_mapCombo = new QComboBox(sidebar);
    m_mapCombo->setObjectName("mapCombo");
    m_mapCombo->addItem(QString::fromUtf8("聚窟州"), "0");
    m_mapCombo->addItem(QString::fromUtf8("火罗国"), "1");
    m_mapCombo->addItem(QString::fromUtf8("龙隐洞天"), "2");
    m_mapCombo->addItem(QString::fromUtf8("风起火罗"), "3");
    m_mapCombo->addItem(QString::fromUtf8("满江红"), "4");
    m_mapCombo->addItem(QString::fromUtf8("宛渠"), "5");
    m_mapCombo->addItem(QString::fromUtf8("大风歌"), "6");
    m_mapCombo->setCurrentIndex(2);
    layout->addWidget(m_mapCombo);

    m_layerField = new QWidget(sidebar);
    m_layerField->setObjectName("settingGroup");
    auto* layerField = new QVBoxLayout(m_layerField);
    layerField->setSpacing(5);
    layerField->setContentsMargins(0, 0, 0, 0);
    m_layerLabel = new QLabel(QString::fromUtf8("地图楼层"), m_layerField);
    m_layerLabel->setObjectName("fieldLabel");
    layerField->addWidget(m_layerLabel);
    m_layerCombo = new QComboBox(m_layerField);
    m_layerCombo->setObjectName("layerCombo");
    m_layerCombo->addItem(QString::fromUtf8("地表层"), 0);
    m_layerCombo->addItem(QString::fromUtf8("地下层"), 1);
    m_layerCombo->setCurrentIndex(0);
    layerField->addWidget(m_layerCombo);
    layout->addWidget(m_layerField);
    auto* divider = new QFrame(sidebar);
    divider->setObjectName("sidebarDivider");
    layout->addWidget(divider, 0, Qt::AlignTop);

    auto* displayTitle = new QLabel(QString::fromUtf8("显示设置"), sidebar);
    displayTitle->setObjectName("sidebarTitle");
    layout->addWidget(displayTitle);

    auto addSwitchRow = [sidebar, layout](const QString& text) {
        auto* row = new QWidget(sidebar);
        row->setObjectName("settingRow");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto* label = new QLabel(text, row);
        label->setObjectName("settingLabel");
        auto* toggle = new QCheckBox(row);
        toggle->setObjectName("switchCheck");
        toggle->setCursor(Qt::PointingHandCursor);
        rowLayout->addWidget(label);
        rowLayout->addStretch();
        rowLayout->addWidget(toggle);
        layout->addWidget(row);
        return toggle;
    };

    m_showBackgroundBox = addSwitchRow(QString::fromUtf8("背景地图"));
    m_showBackgroundBox->setChecked(false);
    m_showBackgroundBox->setToolTip(QString::fromUtf8("显示或隐藏背景地图"));

    auto* opacityLabelRow = new QHBoxLayout();
    opacityLabelRow->setContentsMargins(0, 2, 0, 0);
    auto* opacityLabel = new QLabel(QString::fromUtf8("透明度"), sidebar);
    opacityLabel->setObjectName("opacityLabel");
    m_opacityValueLabel = new QLabel(QString::fromUtf8("100%"), sidebar);
    m_opacityValueLabel->setObjectName("opacityValueLabel");
    opacityLabelRow->addWidget(opacityLabel);
    opacityLabelRow->addStretch();
    opacityLabelRow->addWidget(m_opacityValueLabel);
    layout->addLayout(opacityLabelRow);

    m_backgroundOpacitySlider = new QSlider(Qt::Horizontal, sidebar);
    m_backgroundOpacitySlider->setObjectName("backgroundOpacitySlider");
    m_backgroundOpacitySlider->setRange(0, 100);
    m_backgroundOpacitySlider->setValue(100);
    m_backgroundOpacitySlider->setCursor(Qt::PointingHandCursor);
    m_backgroundOpacitySlider->setEnabled(false);
    layout->addWidget(m_backgroundOpacitySlider);
    layout->addSpacing(4);

    m_allowMapZoomBox = addSwitchRow(QString::fromUtf8("允许缩放"));
    m_allowMapZoomBox->setChecked(false);
    m_allowMapZoomBox->setToolTip(QString::fromUtf8("开启后可使用鼠标滚轮同步缩放地图"));

    m_alwaysVisibleBox = addSwitchRow(QString::fromUtf8("无条件显示"));
    m_alwaysVisibleBox->setChecked(false);
    m_alwaysVisibleBox->setToolTip(QString::fromUtf8("开启后不受游戏地图展开状态影响，始终显示透明图标地图"));

    layout->addSpacing(8);
    auto* footerDivider = new QFrame(sidebar);
    footerDivider->setObjectName("sidebarDivider");
    layout->addWidget(footerDivider, 0, Qt::AlignTop);
    auto* sourceLink = new QLabel(
        QString::fromUtf8("<a href=\"https://github.com/EscapeVelocity-Free/NarakaMapTool\">↗ GitHub 开源</a>"), sidebar);
    sourceLink->setObjectName("sourceLink");
    sourceLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    sourceLink->setOpenExternalLinks(true);
    sourceLink->setToolTip(QString::fromUtf8("在浏览器中打开 NarakaMapTool 开源仓库"));
    layout->addWidget(sourceLink);
    return sidebar;
}

QWidget* ControlPanel::createResourcePanel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("resourcePanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(10);

    auto* header = new QWidget(panel);
    header->setObjectName("resourceHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(2, 0, 2, 0);
    headerLayout->setSpacing(10);

    auto* heading = new QVBoxLayout();
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(1);
    auto* title = new QLabel(QString::fromUtf8("资源筛选"), header);
    title->setObjectName("resourcePanelTitle");
    auto* description = new QLabel(QString::fromUtf8("根据当前地图选择需要显示的资源"), header);
    description->setObjectName("sectionDescription");
    heading->addWidget(title);
    heading->addWidget(description);
    headerLayout->addLayout(heading);
    headerLayout->addStretch(1);

    m_selectionSummaryLabel = new QLabel(QString::fromUtf8("已显示 0 / 0"), header);
    m_selectionSummaryLabel->setObjectName("selectionSummary");
    headerLayout->addWidget(m_selectionSummaryLabel);
    m_selectAllButton = createCommandButton(QString::fromUtf8("全选"), "primaryButton");
    m_clearAllButton = createCommandButton(QString::fromUtf8("清空"), "secondaryButton");
    m_selectAllButton->setMinimumWidth(62);
    m_clearAllButton->setMinimumWidth(62);
    headerLayout->addWidget(m_selectAllButton);
    headerLayout->addWidget(m_clearAllButton);
    panelLayout->addWidget(header);

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setObjectName("contentScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scrollArea);
    content->setObjectName("contentCanvas");
    auto* contentLayout = new QGridLayout(content);
    contentLayout->setContentsMargins(2, 2, 8, 2);
    contentLayout->setHorizontalSpacing(10);
    contentLayout->setVerticalSpacing(10);
    contentLayout->setColumnStretch(0, 1);
    contentLayout->setColumnStretch(1, 1);
    m_resourceGrid = contentLayout;

    for (const auto& group : buildResourceGroups()) {
        createResourceSection(group);
    }

    scrollArea->setWidget(content);
    panelLayout->addWidget(scrollArea, 1);
    return panel;
}

QFrame* ControlPanel::createResourceSection(const ResourceGroup& group) {
    auto* card = new ResourceSectionFrame(this);
    card->setObjectName("sectionCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    applyCardShadow(card);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);

    auto* header = new QVBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(1);

    auto* title = new QLabel(QString::fromUtf8(group.title), card);
    title->setObjectName("sectionTitle");
    auto* subtitle = new QLabel(QString::fromUtf8(group.subtitle), card);
    subtitle->setObjectName("sectionDescription");
    header->addWidget(title);
    header->addWidget(subtitle);
    layout->addLayout(header);

    auto* chips = new ChipFlowLayout();
    chips->setContentsMargins(0, 0, 0, 0);
    card->setChipLayout(chips);

    ResourceSectionView section{card, chips, {}};
    section.buttons.reserve(group.items.size());
    for (const auto& item : group.items) {
        auto* button = createResourceChip(item);
        chips->addWidget(button);
        section.buttons.push_back(button);
    }

    layout->addLayout(chips);
    m_resourceSections.push_back(std::move(section));
    return card;
}

QPushButton* ControlPanel::createResourceChip(const ResourceItem& item) {
    auto* button = new QPushButton(QString::fromUtf8(item.name), this);
    button->setObjectName("resourceChip");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_resourceButtons[button] = ResourceBinding{item.key, item.name};

    const QString label = QString::fromUtf8(item.name);
    const bool defaultChecked = DefaultResourceKeys().count(item.key) != 0;
    button->setChecked(defaultChecked);
    button->setText(defaultChecked ? QString::fromUtf8("✓ %1").arg(label) : QString::fromUtf8("  %1").arg(label));

    connect(button, &QPushButton::toggled, this, [this, button, label](bool checked) {
        button->setText(checked ? QString::fromUtf8("✓ %1").arg(label) : QString::fromUtf8("  %1").arg(label));
        notifySelectionChanged();
    });
    return button;
}

QPushButton* ControlPanel::createCommandButton(const QString& text, const QString& objectName) {
    auto* button = new QPushButton(text, this);
    button->setObjectName(objectName);
    return button;
}

void ControlPanel::applyCardShadow(QFrame* card) {
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(15, 23, 42, 16));
    card->setGraphicsEffect(shadow);
}

void ControlPanel::connectActions(QPushButton* selectAllButton, QPushButton* clearAllButton,
    QCheckBox* showBackgroundBox, QSlider* backgroundOpacitySlider, QLabel* opacityValueLabel,
    QCheckBox* allowMapZoomBox, QCheckBox* alwaysVisibleBox) {
    connect(showBackgroundBox, &QCheckBox::stateChanged, [this](int state) {
        if (m_backgroundOpacitySlider) {
            m_backgroundOpacitySlider->setEnabled(state == Qt::Checked);
        }
        Logger::info("Control panel background switch changed: show={}", state == Qt::Checked);
        emit toggleBackground(state == Qt::Checked);
    });

    connect(backgroundOpacitySlider, &QSlider::valueChanged, [this, opacityValueLabel](int value) {
        opacityValueLabel->setText(QString::fromUtf8("%1%").arg(value));
        emit backgroundOpacityChanged(value);
    });

    connect(allowMapZoomBox, &QCheckBox::toggled, [this](bool enabled) {
        Logger::info("Control panel map zoom switch changed: enabled={}", enabled);
        emit toggleMapZoom(enabled);
    });

    connect(alwaysVisibleBox, &QCheckBox::toggled, [this](bool enabled) {
        Logger::info("Control panel always-visible switch changed: enabled={}", enabled);
        emit toggleAlwaysVisible(enabled);
    });

    connect(m_mapCombo, &QComboBox::currentIndexChanged, [this](int) {
        if (m_layerCombo) {
            m_layerCombo->blockSignals(true);
            m_layerCombo->setCurrentIndex(0);
            m_layerCombo->blockSignals(false);
        }
        Logger::info("Control panel map selection changed: id={} name={}",
            m_mapCombo->currentData().toString().toStdString(), m_mapCombo->currentText().toStdString());
        emit mapChanged(m_mapCombo->currentData().toString().toStdString(), m_mapCombo->currentText().toStdString());
        updateLayerSelectorVisibility();
        emit layerChanged(0);
        refreshResourceVisibility();
        notifySelectionChanged();
    });

    connect(m_layerCombo, &QComboBox::currentIndexChanged, [this](int index) {
        if (!isStormchantMapSelected()) {
            return;
        }
        const int layer = m_layerCombo->itemData(index).toInt();
        Logger::info("Control panel layer selection changed: map_id={} layer={} name={}",
            m_mapCombo->currentData().toString().toStdString(), layer,
            m_layerCombo->currentText().toStdString());
        emit layerChanged(layer);
        refreshResourceVisibility();
        notifySelectionChanged();
    });

    connect(selectAllButton, &QPushButton::clicked, [this]() {
        Logger::info("Control panel requested selection of all available resource types.");
        setAllResourcesChecked(true);
    });

    connect(clearAllButton, &QPushButton::clicked, [this]() {
        Logger::info("Control panel requested clearing all available resource types.");
        setAllResourcesChecked(false);
    });
}

bool ControlPanel::isStormchantMapSelected() const {
    return m_mapCombo && m_mapCombo->currentData().toString() == "6";
}

bool ControlPanel::isResourceAvailable(const ResourceBinding& binding) const {
    if (!m_mapCombo) {
        return false;
    }
    return m_resourceCatalog.isAvailable(
        m_mapCombo->currentData().toString().toStdString(), binding.key, currentLayer());
}

int ControlPanel::currentLayer() const {
    if (!isStormchantMapSelected() || !m_layerCombo) {
        return 0;
    }
    return m_layerCombo->currentData().toInt();
}

void ControlPanel::refreshResourceVisibility() {
    const std::string mapId = m_mapCombo ? m_mapCombo->currentData().toString().toStdString() : "<unset>";
    const int layer = currentLayer();
    std::size_t visibleResourceCount = 0;
    std::size_t visibleSectionCount = 0;

    if (m_resourceGrid) {
        while (auto* item = m_resourceGrid->takeAt(0)) {
            delete item;
        }
    }

    std::vector<std::pair<ResourceSectionView*, std::size_t>> visibleSections;
    visibleSections.reserve(m_resourceSections.size());

    for (auto& section : m_resourceSections) {
        section.card->setMinimumHeight(0);
        while (auto* item = section.layout->takeAt(0)) {
            delete item;
        }

        bool hasVisibleResource = false;
        for (auto* button : section.buttons) {
            const auto bindingIt = m_resourceButtons.find(button);
            const bool visible = bindingIt != m_resourceButtons.end() && isResourceAvailable(bindingIt->second);
            button->setVisible(visible);
            if (!visible) {
                continue;
            }

            hasVisibleResource = true;
            ++visibleResourceCount;
            section.layout->addWidget(button);
        }

        section.card->setVisible(hasVisibleResource);
        if (hasVisibleResource) {
            ++visibleSectionCount;
            std::size_t sectionResourceCount = 0;
            for (auto* button : section.buttons) {
                if (button->isVisible()) {
                    ++sectionResourceCount;
                }
            }
            visibleSections.emplace_back(&section, sectionResourceCount);
        }
    }

    const bool singleCategory = visibleSections.size() == 1;
    int row = 0;
    int column = 0;
    for (const auto& [section, sectionResourceCount] : visibleSections) {
        // Larger categories use the full resource width; regular categories fill one grid column.
        const bool wide = singleCategory || sectionResourceCount >= 12;
        section->card->setMinimumWidth(singleCategory ? kSingleCategoryMinWidth : 0);
        section->card->setMaximumWidth(singleCategory ? kSingleCategoryMaxWidth : QWIDGETSIZE_MAX);
        if (wide && column != 0) {
            ++row;
            column = 0;
        }
        if (m_resourceGrid) {
            const Qt::Alignment alignment = singleCategory ? (Qt::AlignLeft | Qt::AlignTop) : Qt::AlignTop;
            m_resourceGrid->addWidget(section->card, row, column, 1, wide ? 2 : 1, alignment);
        }
        // Ensure the card reserves one row for every wrapped chip before the grid receives its final geometry.
        static_cast<ResourceSectionFrame*>(section->card)->updateMinimumHeightForWidth(
            singleCategory ? kSingleCategoryMinWidth : std::max(1, section->card->sizeHint().width()));
        if (wide || column == 1) {
            ++row;
            column = 0;
        } else {
            ++column;
        }
    }
    if (m_resourceGrid) {
        m_resourceGrid->setRowStretch(row, 1);
        QTimer::singleShot(0, this, [this]() {
            if (!m_resourceGrid) {
                return;
            }

            m_resourceGrid->activate();
            for (auto& section : m_resourceSections) {
                if (section.card->isVisible() && section.card->width() > 0) {
                    static_cast<ResourceSectionFrame*>(section.card)->updateMinimumHeightForWidth(section.card->width());
                }
            }
        });
    }

    Logger::info("Control panel resource visibility refreshed: map_id={} layer={} visible_sections={} visible_resource_types={}",
        mapId, layer, visibleSectionCount, visibleResourceCount);
}

void ControlPanel::updateLayerSelectorVisibility() {
    const bool visible = isStormchantMapSelected();
    Logger::info("Control panel layer selector visibility: visible={} map_id={}",
        visible, m_mapCombo ? m_mapCombo->currentData().toString().toStdString() : "<unset>");
    if (m_layerField) m_layerField->setVisible(visible);
}

void ControlPanel::setAllResourcesChecked(bool checked) {
    size_t updatedCount = 0;
    for (auto const& [button, binding] : m_resourceButtons) {
        if (!isResourceAvailable(binding)) {
            continue;
        }
        ++updatedCount;
        button->blockSignals(true);
        button->setChecked(checked);
        button->blockSignals(false);
        const QString label = QString::fromUtf8(binding.displayName.c_str());
        button->setText(checked ? QString::fromUtf8("✓ %1").arg(label) : QString::fromUtf8("  %1").arg(label));
    }
    Logger::info("Control panel bulk selection applied: checked={} available_resource_types={}", checked, updatedCount);
    notifySelectionChanged();
}

void ControlPanel::notifySelectionChanged() {
    std::vector<std::string> selected;
    selected.reserve(m_resourceButtons.size());
    std::size_t availableCount = 0;
    for (auto const& [button, binding] : m_resourceButtons) {
        if (!isResourceAvailable(binding)) {
            continue;
        }
        ++availableCount;
        if (button->isChecked()) selected.push_back(binding.key);
    }
    if (m_selectionSummaryLabel) {
        m_selectionSummaryLabel->setText(
            QString::fromUtf8("已显示 %1 / %2").arg(selected.size()).arg(availableCount));
    }
    Logger::info("Control panel resource selection changed: count={} keys=[{}]", selected.size(), JoinResourceKeys(selected));
    emit selectionChanged(selected);
}
