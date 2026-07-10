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
#include <QTimer>
#include <QVBoxLayout>

#include <set>

#pragma execution_character_set("utf-8")

struct ControlPanel::ResourceItem {
    const char* name;
    const char* key;
};

struct ControlPanel::ResourceGroup {
    const char* title;
    const char* subtitle;
    int columns;
    bool riftOnly;
    std::vector<ResourceItem> items;
};

namespace {
const std::set<std::string>& DefaultResourceKeys() {
    static const std::set<std::string> keys = {
        "firefly", "wishingWell", "miniShrine", "goldenToad", "flyingTarget", "fireflyCage"
    };
    return keys;
}

}

ControlPanel::ControlPanel(QWidget* parent) : QWidget(parent) {
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

    updateRiftSectionVisibility();
    QTimer::singleShot(100, this, &ControlPanel::notifySelectionChanged);
}

std::vector<ControlPanel::ResourceGroup> ControlPanel::buildResourceGroups() {
    return {
        {
            "常用消耗 · 采集",
            "高频路线点，适合日常跑图",
            7,
            false,
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
            false,
            {
                {"任务(卷轴)", "questSerialCache"}, {"任务2", "questCache"}, {"任务土地", "questShrine"},
                {"任务钟", "questBell"}, {"悬赏", "bounty"}, {"叫阵", "reverseBounty"}, {"地脉仪", "strongPoint"}
            }
        },
        {
            "重要设施",
            "商店、返魂台、洞穴与功能设施",
            7,
            false,
            {
                {"商店", "riftDealer"}, {"返魂台", "soulAltar"}, {"武器架", "weaponRack"},
                {"宝库", "treasureCave"}, {"奥义封印", "forbiddenSeal"}, {"回阳镜", "gateOfYang"},
                {"食人花", "carnivorousYam"}, {"雪莲", "snowLotus"}, {"泉源", "springSource"},
                {"贺兰艺", "helanArt"}
            }
        },
        {
            "其他",
            "地图上的补充资源点",
            7,
            false,
            {
                {"金堆", "gold"}, {"绿堆", "green"}, {"漂浮堆", "floatingPile"},
                {"钟", "bell"}, {"攻城弩", "ballista"}, {"滴滴打车", "soaringArm"},
                {"捕兽夹", "bearTrap"}, {"野生动物", "wildlife"}, {"烤火", "bonfire"},
                {"疗愈之树", "healingTree"}, {"老鼠", "rat"}, {"土地庙", "prayerShrine"},
                {"铜钱", "treasureCoin"}
            }
        },
        {
            "摸金",
            "裂隙宝箱、机关与收藏容器",
            7,
            true,
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
                {"大收集铺", "riftLargeCollectionShop"}
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
        QLabel#sectionTitle {
            background: transparent;
            color: #070a11;
            font-size: 16px;
            font-weight: 900;
        }
        QComboBox {
            min-width: 190px;
            min-height: 34px;
            padding: 4px 32px 4px 14px;
            border: 1px solid #c8cfdd;
            border-radius: 12px;
            background: #ffffff;
            color: #080b12;
            font-weight: 800;
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

    auto* mapLabel = new QLabel(QString::fromUtf8("当前地图"), card);
    mapLabel->setObjectName("fieldLabel");
    layout->addWidget(mapLabel);

    m_mapCombo = new QComboBox(card);
    m_mapCombo->addItem(QString::fromUtf8("聚窟州"), "0");
    m_mapCombo->addItem(QString::fromUtf8("火罗国"), "1");
    m_mapCombo->addItem(QString::fromUtf8("龙隐洞天"), "2");
    m_mapCombo->addItem(QString::fromUtf8("风起火罗"), "3");
    m_mapCombo->addItem(QString::fromUtf8("满江红"), "4");
    m_mapCombo->addItem(QString::fromUtf8("宛渠"), "5");
    m_mapCombo->setCurrentIndex(2);
    layout->addWidget(m_mapCombo);

    auto* showBackgroundBox = new QCheckBox(QString::fromUtf8("背景地图"), card);
    showBackgroundBox->setObjectName("switchCheck");
    showBackgroundBox->setChecked(false);
    layout->addWidget(showBackgroundBox);

    auto* selectAllButton = createCommandButton(QString::fromUtf8("全部显示"), "primaryButton");
    auto* clearAllButton = createCommandButton(QString::fromUtf8("全部隐藏"), "secondaryButton");
    layout->addWidget(selectAllButton);
    layout->addWidget(clearAllButton);

    connectActions(selectAllButton, clearAllButton, showBackgroundBox);
    return card;
}

QFrame* ControlPanel::createResourceSection(const ResourceGroup& group) {
    auto* card = new QFrame(this);
    card->setObjectName("sectionCard");
    applyCardShadow(card);
    if (group.riftOnly) {
        m_riftOnlySections.push_back(card);
    }

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

    int row = 0;
    int col = 0;
    for (const auto& item : group.items) {
        grid->addWidget(createResourceChip(item, group.riftOnly), row, col);
        if (++col >= group.columns) {
            col = 0;
            ++row;
        }
    }

    for (int i = 0; i < group.columns; ++i) {
        grid->setColumnStretch(i, 1);
    }

    layout->addLayout(grid);
    return card;
}

QPushButton* ControlPanel::createResourceChip(const ResourceItem& item, bool riftOnly) {
    auto* button = new QPushButton(QString::fromUtf8(item.name), this);
    button->setObjectName("resourceChip");
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_resourceButtons[button] = ResourceBinding{item.key, riftOnly};

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

void ControlPanel::connectActions(QPushButton* selectAllButton, QPushButton* clearAllButton, QCheckBox* showBackgroundBox) {
    connect(showBackgroundBox, &QCheckBox::stateChanged, [this](int state) {
        emit toggleBackground(state == Qt::Checked);
    });

    connect(m_mapCombo, &QComboBox::currentIndexChanged, [this](int) {
        emit mapChanged(m_mapCombo->currentData().toString().toStdString(), m_mapCombo->currentText().toStdString());
        updateRiftSectionVisibility();
        notifySelectionChanged();
    });

    connect(selectAllButton, &QPushButton::clicked, [this]() {
        setAllResourcesChecked(true);
    });

    connect(clearAllButton, &QPushButton::clicked, [this]() {
        setAllResourcesChecked(false);
    });
}

bool ControlPanel::isRiftMapSelected() const {
    if (!m_mapCombo) {
        return false;
    }

    const QString mapId = m_mapCombo->currentData().toString();
    return mapId == "3" || mapId == "4";
}

bool ControlPanel::isResourceAvailable(const ResourceBinding& binding) const {
    return !binding.riftOnly || isRiftMapSelected();
}

void ControlPanel::updateRiftSectionVisibility() {
    const bool showRiftSections = isRiftMapSelected();
    for (auto* section : m_riftOnlySections) {
        section->setVisible(showRiftSections);
    }
}

void ControlPanel::setAllResourcesChecked(bool checked) {
    for (auto const& [button, binding] : m_resourceButtons) {
        if (!isResourceAvailable(binding)) {
            continue;
        }
        button->blockSignals(true);
        button->setChecked(checked);
        button->blockSignals(false);
    }
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
    emit selectionChanged(selected);
}
