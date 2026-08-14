#include "control_panel.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QColor>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>
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
    int columns;
    std::vector<ResourceItem> items;
};

namespace {
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
    rootLayout->setContentsMargins(24, 18, 24, 18);
    rootLayout->setSpacing(12);

    rootLayout->addWidget(createHeaderCard());

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("contentScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scrollArea);
    content->setObjectName("contentCanvas");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 6, 6, 6);
    contentLayout->setSpacing(12);

    for (const auto& group : buildResourceGroups()) {
        contentLayout->addWidget(createResourceSection(group));
    }
    contentLayout->addStretch();

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea, 1);

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
            7,
            {
                {"萤火虫", "firefly"}, {"许愿井", "wishingWell"}, {"小土地", "miniShrine"},
                {"金蟾", "goldenToad"}, {"鸟靶", "flyingTarget"}, {"萤火笼", "fireflyCage"},
                {"梨", "pear"}, {"刺梨", "pricklyPear"}, {"蛇皮果", "salak"},
                {"蒲公英", "dandelion"}, {"锦鲤", "koi"}
            }
        },
        {
            "任务 · 挑战",
            "卷轴、悬赏、地脉等关键交互",
            5,
            {
                {"任务(卷轴)", "questSerialCache"}, {"任务2", "questCache"}, {"任务土地", "questShrine"},
                {"任务钟", "questBell"}, {"悬赏", "bounty"}, {"叫阵", "reverseBounty"}, {"地脉仪", "strongPoint"}
            }
        },
        {
            "重要设施",
            "商店、返魂台、洞穴与功能设施",
            7,
            {
                {"商店", "riftDealer"}, {"返魂台", "soulAltar"}, {"武器架", "weaponRack"},
                {"宝库", "treasureCave"}, {"奥义封印", "forbiddenSeal"}, {"回阳镜", "gateOfYang"},
                {"食人花", "carnivorousYam"}, {"雪莲", "snowLotus"}, {"泉源", "springSource"},
                {"贺兰艺", "helanArt"}, {"高资源区", "highResourceZone"}
            }
        },
        {
            "其他",
            "地图上的补充资源点",
            7,
            {
                {"金堆", "gold"}, {"绿堆", "green"}, {"漂浮堆", "floatingPile"},
                {"钟", "bell"}, {"攻城弩", "ballista"}, {"滴滴打车", "soaringArm"},
                {"捕兽夹", "bearTrap"}, {"野生动物", "wildlife"}, {"烤火", "bonfire"},
                {"疗愈之树", "healingTree"}, {"老鼠", "rat"}, {"土地庙", "earthShrine"},
                {"铁蒺藜", "ironUrchin"}, {"铁棘龟", "ironbackTurtle"}, {"灵蚌", "spiritClam"},
                {"铜钱", "treasureCoin"}, {"神仙鱼", "angelfish"}, {"青雀鱼", "azureFinchFish"},
                {"碧波珊瑚", "azureWaveCoralline"}, {"金焰珊瑚", "goldenFlameCoralline"},
                {"巨蜥", "grandLizard"}, {"刺尾鱼", "stingtailFish"}, {"紫珊瑚", "violetCoralline"}
            }
        },
        {
            "摸金",
            "裂隙宝箱、机关与收藏容器",
            7,
            {
                {"博火箱·史诗", "riftChestEpic"},
                {"博火箱·史诗(必中)", "riftChestEpicGuaranteed"},
                {"博火箱·传说", "riftChestLegendary"},
                {"博火箱·史诗(沙)", "riftChestEpicSand"},
                {"博火箱·史诗(雪)", "riftChestEpicSnow"},
                {"博火箱·史诗(雷)", "riftChestEpicThunder"},
                {"博火箱·传说(雷)", "riftChestLegendaryThunder"},
                {"暗门", "riftSecret"},
                {"矿车", "riftMinecart"},
                {"守卫", "riftQuestGuard"},
                {"水井", "riftWaterWell"},
                {"闸门", "riftGateOfYang"},
                {"猎洞", "riftShovel"},
                {"据点", "riftStronghold"},
                {"据点(小Boss)", "riftStrongholdMiniBoss"},
                {"据点(Boss)", "riftStrongholdBoss"},
                {"骰子", "riftDice"},
                {"轿子", "riftSedanChair"},
                {"金遗物", "riftGoldRelic"},
                {"武器箱", "riftWeaponBox"},
                {"气流", "riftAirCurrent"},
                {"小篮", "riftSmallHamper"},
                {"大篮", "riftBigHamper"},
                {"存钱罐", "riftPiggyBank"},
                {"石碑·解读", "riftSteleDecipher"},
                {"洞钥匙", "riftCaveKey"},
                {"黑焰兵", "riftBlackFlameSoldier"},
                {"大药柜", "riftLargeMedicineCabinet"},
                {"信件", "riftLetter"},
                {"小妆盒", "riftSmallMakeupBox"},
                {"大收集箱", "riftBigCollectionContainer"},
                {"大收集柜", "riftLargeCollectionCabinet"},
                {"大杂物柜", "riftBigUtilityCabinet"},
                {"大杂货铺", "riftLargeGroceryStore"},
                {"大收集铺", "riftLargeCollectionShop"},
                {"断章", "riftSeveredStatutes"},
                {"安魂钟", "riftSoulCalmingBell"},
                {"裂隙出口", "riftSecretExitPortal"}
            }
        }
    };
}

void ControlPanel::setupWindow() {
    setWindowTitle(QString::fromUtf8("永劫无间资源地图控制中心"));
    setMinimumSize(1100, 680);
    resize(1280, 840);
}

void ControlPanel::setupStyle() {
    setStyleSheet(R"(
        QWidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f7f9fd, stop:0.45 #eef3fb, stop:1 #fbfbfd);
            color: #080b12;
            font-family: "HarmonyOS Sans SC", "Microsoft YaHei UI", "Microsoft YaHei";
            font-size: 13px;
            font-weight: 600;
        }
        QFrame#headerCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #ffffff, stop:0.58 #f6f9ff, stop:1 #edf5ff);
            border: 1px solid #d8dfed;
            border-radius: 20px;
        }
        QFrame#sectionCard {
            background: rgba(255, 255, 255, 246);
            border: 1px solid #dfe4ee;
            border-radius: 18px;
        }
        QLabel#heroKicker {
            background: #e8f1ff;
            color: #0057d9;
            border: 1px solid #c9ddff;
            border-radius: 8px;
            padding: 2px 8px;
            font-size: 10px;
            font-weight: 900;
        }
        QLabel#appTitle {
            background: transparent;
            color: #06080d;
            font-size: 26px;
            font-weight: 900;
        }
        QLabel#appSubtitle, QLabel#sectionSubtitle, QLabel#fieldLabel {
            background: transparent;
            color: #4d5563;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#sourceLink {
            background: transparent;
            color: #4d5563;
            font-size: 11px;
            font-weight: 800;
        }
        QLabel#sourceLink a {
            color: #4d5563;
            text-decoration: none;
        }
        QLabel#sourceLink a:hover {
            color: #006bff;
            text-decoration: underline;
        }
        QWidget#backgroundField {
            background: transparent;
        }
        QLabel#opacityLabel, QLabel#opacityValueLabel {
            background: transparent;
            color: #667085;
            font-size: 10px;
            font-weight: 800;
        }
        QLabel#opacityValueLabel {
            min-width: 32px;
            color: #0068e6;
        }
        QSlider#backgroundOpacitySlider {
            min-width: 132px;
            max-width: 170px;
            min-height: 16px;
        }
        QSlider#backgroundOpacitySlider::groove:horizontal {
            height: 4px;
            border-radius: 2px;
            background: #d9e1ee;
        }
        QSlider#backgroundOpacitySlider::sub-page:horizontal {
            border-radius: 2px;
            background: #0a84ff;
        }
        QSlider#backgroundOpacitySlider::add-page:horizontal {
            border-radius: 2px;
            background: #d9e1ee;
        }
        QSlider#backgroundOpacitySlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border: 2px solid #ffffff;
            border-radius: 7px;
            background: #0068e6;
        }
        QLabel#sectionTitle {
            background: transparent;
            color: #070a11;
            font-size: 16px;
            font-weight: 900;
        }
        QComboBox {
            min-width: 150px;
            min-height: 34px;
            padding: 4px 32px 4px 14px;
            border: 1px solid #c8cfdd;
            border-radius: 12px;
            background: #ffffff;
            color: #080b12;
            font-weight: 800;
        }
        QComboBox#mapCombo {
            min-width: 168px;
        }
        QComboBox#layerCombo {
            min-width: 128px;
        }
        QComboBox:hover {
            border-color: #9db7e8;
            background: #fbfdff;
        }
        QComboBox::drop-down {
            border: 0;
            width: 32px;
        }
        QCheckBox {
            spacing: 8px;
            color: #080b12;
        }
        QPushButton#resourceChip {
            min-height: 32px;
            max-height: 32px;
            padding: 4px 12px;
            border: 1px solid #d9deea;
            border-radius: 11px;
            background: #ffffff;
            color: #0a0d14;
            font-size: 12px;
            font-weight: 700;
            text-align: left;
        }
        QPushButton#resourceChip:hover {
            border-color: #94b8ff;
            background: #f5f9ff;
        }
        QPushButton#resourceChip:checked {
            border-color: #006bff;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #0a84ff, stop:1 #0057d9);
            color: #ffffff;
        }
        QPushButton#resourceChip:checked:hover {
            border-color: #0057d9;
            background: #0068e6;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 7px;
            border: 1px solid #bbc4d2;
            background: #ffffff;
        }
        QCheckBox::indicator:checked {
            border-color: #ffffff;
            background: #ffffff;
        }
        QCheckBox#switchCheck::indicator {
            width: 36px;
            height: 20px;
            border-radius: 10px;
            border: 1px solid #bcc6d4;
            background: #d9dee8;
        }
        QCheckBox#switchCheck::indicator:checked {
            border-color: #34c759;
            background: #34c759;
        }
        QPushButton {
            min-height: 34px;
            padding: 5px 14px;
            border-radius: 11px;
            font-weight: 900;
        }
        QPushButton#primaryButton {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #0a84ff, stop:1 #0061ff);
            border: 1px solid #0057d9;
        }
        QPushButton#primaryButton:hover {
            background: #0057d9;
        }
        QPushButton#secondaryButton {
            color: #0a0d14;
            background: #ffffff;
            border: 1px solid #c8cfdd;
        }
        QPushButton#secondaryButton:hover {
            background: #f4f7fb;
            border-color: #9db7e8;
        }
        QScrollArea#contentScroll, QWidget#contentCanvas {
            background: transparent;
            border: 0;
        }
        QScrollBar:vertical {
            width: 8px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            min-height: 48px;
            border-radius: 4px;
            background: #aeb8c8;
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
    layout->setContentsMargins(22, 16, 22, 16);
    layout->setSpacing(18);

    auto* titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(2);

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

    layout->addLayout(titleBlock, 1);

    auto* mapField = new QVBoxLayout();
    mapField->setSpacing(4);
    auto* mapLabel = new QLabel(QString::fromUtf8("当前地图"), card);
    mapLabel->setObjectName("fieldLabel");
    mapField->addWidget(mapLabel);

    m_mapCombo = new QComboBox(card);
    m_mapCombo->setObjectName("mapCombo");
    m_mapCombo->addItem(QString::fromUtf8("聚窟州"), "0");
    m_mapCombo->addItem(QString::fromUtf8("火罗国"), "1");
    m_mapCombo->addItem(QString::fromUtf8("龙隐洞天"), "2");
    m_mapCombo->addItem(QString::fromUtf8("风起火罗"), "3");
    m_mapCombo->addItem(QString::fromUtf8("满江红"), "4");
    m_mapCombo->addItem(QString::fromUtf8("宛渠"), "5");
    m_mapCombo->addItem(QString::fromUtf8("大风歌"), "6");
    m_mapCombo->setCurrentIndex(2);
    mapField->addWidget(m_mapCombo);
    layout->addLayout(mapField);

    auto* layerField = new QVBoxLayout();
    layerField->setSpacing(4);
    m_layerLabel = new QLabel(QString::fromUtf8("地图楼层"), card);
    m_layerLabel->setObjectName("fieldLabel");
    layerField->addWidget(m_layerLabel);
    m_layerCombo = new QComboBox(card);
    m_layerCombo->setObjectName("layerCombo");
    m_layerCombo->addItem(QString::fromUtf8("地表层"), 0);
    m_layerCombo->addItem(QString::fromUtf8("地下层"), 1);
    m_layerCombo->setCurrentIndex(0);
    layerField->addWidget(m_layerCombo);
    layout->addLayout(layerField);

    auto* backgroundField = new QWidget(card);
    backgroundField->setObjectName("backgroundField");
    backgroundField->setMinimumWidth(150);
    auto* backgroundLayout = new QVBoxLayout(backgroundField);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->setSpacing(2);

    auto* backgroundTopLine = new QHBoxLayout();
    backgroundTopLine->setContentsMargins(0, 0, 0, 0);
    backgroundTopLine->setSpacing(6);
    auto* showBackgroundBox = new QCheckBox(QString::fromUtf8("背景地图"), backgroundField);
    showBackgroundBox->setObjectName("switchCheck");
    showBackgroundBox->setChecked(false);
    backgroundTopLine->addWidget(showBackgroundBox);
    auto* opacityLabel = new QLabel(QString::fromUtf8("透明度"), backgroundField);
    opacityLabel->setObjectName("opacityLabel");
    backgroundTopLine->addWidget(opacityLabel);
    auto* opacityValueLabel = new QLabel(QString::fromUtf8("100%"), backgroundField);
    opacityValueLabel->setObjectName("opacityValueLabel");
    backgroundTopLine->addWidget(opacityValueLabel);
    backgroundTopLine->addStretch();
    backgroundLayout->addLayout(backgroundTopLine);

    auto* backgroundOpacitySlider = new QSlider(Qt::Horizontal, backgroundField);
    backgroundOpacitySlider->setObjectName("backgroundOpacitySlider");
    backgroundOpacitySlider->setRange(0, 100);
    backgroundOpacitySlider->setValue(100);
    backgroundOpacitySlider->setCursor(Qt::PointingHandCursor);
    backgroundLayout->addWidget(backgroundOpacitySlider);
    layout->addWidget(backgroundField);

    auto* allowMapZoomBox = new QCheckBox(QString::fromUtf8("允许缩放"), card);
    allowMapZoomBox->setObjectName("switchCheck");
    allowMapZoomBox->setChecked(false);
    allowMapZoomBox->setToolTip(QString::fromUtf8("开启后可使用鼠标滚轮同步缩放地图"));
    layout->addWidget(allowMapZoomBox);

    auto* selectAllButton = createCommandButton(QString::fromUtf8("全部显示"), "primaryButton");
    auto* clearAllButton = createCommandButton(QString::fromUtf8("全部隐藏"), "secondaryButton");
    layout->addWidget(selectAllButton);
    layout->addWidget(clearAllButton);

    auto* sourceLink = new QLabel(
        QString::fromUtf8("<a href=\"https://github.com/EscapeVelocity-Free/NarakaMapTool\">GitHub 开源</a>"), card);
    sourceLink->setObjectName("sourceLink");
    sourceLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    sourceLink->setOpenExternalLinks(true);
    sourceLink->setToolTip(QString::fromUtf8("在浏览器中打开 NarakaMapTool 开源仓库"));
    layout->addWidget(sourceLink);

    connectActions(selectAllButton, clearAllButton, showBackgroundBox,
        backgroundOpacitySlider, opacityValueLabel, allowMapZoomBox);
    return card;
}

QFrame* ControlPanel::createResourceSection(const ResourceGroup& group) {
    auto* card = new QFrame(this);
    card->setObjectName("sectionCard");
    applyCardShadow(card);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 16);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout();
    header->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8(group.title), card);
    title->setObjectName("sectionTitle");
    auto* subtitle = new QLabel(QString::fromUtf8(group.subtitle), card);
    subtitle->setObjectName("sectionSubtitle");
    header->addWidget(title);
    header->addWidget(subtitle);
    header->addStretch();
    layout->addLayout(header);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(9);
    grid->setVerticalSpacing(9);

    ResourceSectionView section{card, grid, group.columns, {}};
    section.buttons.reserve(group.items.size());
    for (const auto& item : group.items) {
        section.buttons.push_back(createResourceChip(item));
    }

    for (int i = 0; i < group.columns; ++i) {
        grid->setColumnStretch(i, 1);
    }

    layout->addLayout(grid);
    m_resourceSections.push_back(std::move(section));
    return card;
}

QPushButton* ControlPanel::createResourceChip(const ResourceItem& item) {
    auto* button = new QPushButton(QString::fromUtf8(item.name), this);
    button->setObjectName("resourceChip");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_resourceButtons[button] = ResourceBinding{item.key};

    if (DefaultResourceKeys().count(item.key)) {
        button->setChecked(true);
    }

    connect(button, &QPushButton::toggled, this, &ControlPanel::notifySelectionChanged);
    return button;
}

QPushButton* ControlPanel::createCommandButton(const QString& text, const QString& objectName) {
    auto* button = new QPushButton(text, this);
    button->setObjectName(objectName);
    return button;
}

void ControlPanel::applyCardShadow(QFrame* card) {
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(25, 33, 52, 28));
    card->setGraphicsEffect(shadow);
}

void ControlPanel::connectActions(QPushButton* selectAllButton, QPushButton* clearAllButton,
    QCheckBox* showBackgroundBox, QSlider* backgroundOpacitySlider, QLabel* opacityValueLabel,
    QCheckBox* allowMapZoomBox) {
    connect(showBackgroundBox, &QCheckBox::stateChanged, [this](int state) {
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

    for (auto& section : m_resourceSections) {
        while (auto* item = section.grid->takeAt(0)) {
            delete item;
        }

        int row = 0;
        int col = 0;
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
            section.grid->addWidget(button, row, col);
            if (++col >= section.columns) {
                col = 0;
                ++row;
            }
        }

        section.card->setVisible(hasVisibleResource);
        if (hasVisibleResource) {
            ++visibleSectionCount;
        }
    }

    Logger::info("Control panel resource visibility refreshed: map_id={} layer={} visible_sections={} visible_resource_types={}",
        mapId, layer, visibleSectionCount, visibleResourceCount);
}

void ControlPanel::updateLayerSelectorVisibility() {
    const bool visible = isStormchantMapSelected();
    Logger::info("Control panel layer selector visibility: visible={} map_id={}",
        visible, m_mapCombo ? m_mapCombo->currentData().toString().toStdString() : "<unset>");
    if (m_layerLabel) m_layerLabel->setVisible(visible);
    if (m_layerCombo) m_layerCombo->setVisible(visible);
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
    }
    Logger::info("Control panel bulk selection applied: checked={} available_resource_types={}", checked, updatedCount);
    notifySelectionChanged();
}

void ControlPanel::notifySelectionChanged() {
    std::vector<std::string> selected;
    selected.reserve(m_resourceButtons.size());
    for (auto const& [button, binding] : m_resourceButtons) {
        if (!isResourceAvailable(binding)) {
            continue;
        }
        if (button->isChecked()) selected.push_back(binding.key);
    }
    Logger::info("Control panel resource selection changed: count={} keys=[{}]", selected.size(), JoinResourceKeys(selected));
    emit selectionChanged(selected);
}
