#include "control_panel.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QScrollArea>
#include <QSlider>
#include <set>
#include "config_manager.h"

#pragma execution_character_set("utf-8")

ControlPanel::ControlPanel(QWidget* parent) : QWidget(parent) {
    // 1. 窗口基础设置
    setWindowTitle(QString::fromUtf8("永劫无间资源地图控制中心"));
    setMinimumSize(1100, 650); // 设置较大的初始尺寸，显得大气

    // 2. 全局样式美化 (QSS)
    this->setStyleSheet(R"(
        QWidget { 
            background-color: #1e1e1e; 
            color: #dcdcdc; 
            font-family: 'Microsoft YaHei'; 
            font-size: 14px; 
        }
        QGroupBox {
            border: 1px solid #3a3a3a;
            border-radius: 6px;
            margin-top: 15px;
            font-weight: bold;
            color: #a0a0a0;
            background-color: #252526;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QCheckBox {
            spacing: 8px;
            padding: 5px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #555;
            border-radius: 3px;
        }
        QCheckBox::indicator:unchecked { background-color: #333; }
        QCheckBox::indicator:unchecked:hover { border: 1px solid #8e2a2b; }
        QCheckBox::indicator:checked { background-color: #8e2a2b; border: 1px solid #ff4d4d; }
        
        QComboBox {
            background-color: #333;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 5px 15px;
            min-width: 120px;
            color: white;
        }
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 6px 20px;
            color: #efefef;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border: 1px solid #8e2a2b;
        }
        QPushButton#actionBtn {
            background-color: #8e2a2b;
            border: 1px solid #a00000;
            font-weight: bold;
        }
        QPushButton#actionBtn:hover {
            background-color: #a03536;
        }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // --- 3. 顶部标题与工具栏 ---
    auto* headerLayout = new QHBoxLayout();

    QLabel* titleLabel = new QLabel(QString::fromUtf8("地图资源辅助 - 控制面板"));
    titleLabel->setStyleSheet("font-size: 20px; color: #ffffff; font-weight: bold;");

    m_mapCombo = new QComboBox(this);
    m_mapCombo->addItem(QString::fromUtf8("聚窟州"), "0");
    m_mapCombo->addItem(QString::fromUtf8("火罗国"), "1");
    m_mapCombo->addItem(QString::fromUtf8("龙隐洞天"), "2");
    m_mapCombo->setCurrentIndex(2);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(new QLabel(QString::fromUtf8("当前地图:")));
    headerLayout->addWidget(m_mapCombo);

    // 在构造函数 headerLayout 布局部分：
    headerLayout->addSpacing(20);
    QCheckBox* cbShowBg = new QCheckBox(QString::fromUtf8("显示背景地图"), this);
    cbShowBg->setChecked(false); // 默认关闭
    headerLayout->addWidget(cbShowBg);

    // 绑定勾选事件 (假设信号名为 toggleBackground)
    connect(cbShowBg, &QCheckBox::stateChanged, [this](int state) {
        emit toggleBackground(state == Qt::Checked);
        });

    headerLayout->addSpacing(20);
    auto* btnSelectAll = new QPushButton(QString::fromUtf8("全部显示"), this);
    auto* btnClearAll = new QPushButton(QString::fromUtf8("全部隐藏"), this);
    btnSelectAll->setObjectName("actionBtn");

    headerLayout->addWidget(btnSelectAll);
    headerLayout->addWidget(btnClearAll);
    mainLayout->addLayout(headerLayout);

    // --- 4. 资源分组逻辑 ---
    struct ResourceItem { const char* name; const char* key; };

    auto createGroup = [&](QString title, std::vector<ResourceItem> groupItems) {
        QGroupBox* groupBox = new QGroupBox(title, this);
        QGridLayout* gLayout = new QGridLayout(groupBox);
        gLayout->setContentsMargins(15, 20, 15, 15);
        gLayout->setSpacing(10);

        int row = 0, col = 0;
        int maxCols = 5; // 每组内部最多5列

        for (const auto& item : groupItems) {
            auto* cb = new QCheckBox(QString::fromUtf8(item.name), this);
            m_boxMap[cb] = item.key;
            gLayout->addWidget(cb, row, col);

            // 默认勾选逻辑
            static std::set<std::string> defaultKeys = { "firefly", "wishingWell", "miniShrine", "goldenToad", "flyingTarget", "fireflyCage" };
            if (defaultKeys.count(item.key)) cb->setChecked(true);

            connect(cb, &QCheckBox::stateChanged, this, &ControlPanel::notifySelectionChanged);

            col++;
            if (col >= maxCols) { col = 0; row++; }
        }
        return groupBox;
        };

    // --- 分组定义 ---
    mainLayout->addWidget(createGroup(QString::fromUtf8("常用消耗 & 采集"), {
        {"萤火虫", "firefly"}, {"许愿井", "wishingWell"}, {"小土地", "miniShrine"},
        {"金蟾", "goldenToad"}, {"鸟靶", "flyingTarget"}, {"萤火笼", "fireflyCage"},
        {"梨", "pear"}, {"刺梨", "pricklyPear"}, {"蛇皮果", "salak"}, {"蒲公英", "dandelion"}, {"锦鲤", "koi"}
        }));

    mainLayout->addWidget(createGroup(QString::fromUtf8("任务 & 挑战"), {
        {"任务(卷轴)", "questSerialCache"}, {"任务2", "questCache"}, {"任务土地", "questShrine"},
        {"任务钟", "questBell"}, {"悬赏", "bounty"}, {"叫阵", "reverseBounty"}, {"地脉仪", "strongPoint"}
        }));

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(createGroup(QString::fromUtf8("重要设施"), {
        {"商店", "riftDealer"}, {"反魂台", "soulAltar"}, {"武器架", "weaponRack"},
        {"宝库", "treasureCave"}, {"奥义封印", "forbiddenSeal"}, {"回阳镜", "gateOfYang"},
        {"食人花", "carnivorousYam"}, {"雪莲", "snowLotus"}, {"泉源", "springSource"}
        }));

    bottomRow->addWidget(createGroup(QString::fromUtf8("其他"), {
        {"金堆", "gold"}, {"绿堆", "green"}, {"漂浮堆", "floatingPile"},
        {"钟", "bell"}, {"攻城弩", "ballista"}, {"滴滴打车", "soaringArm"},
        {"捕兽夹", "bearTrap"}, {"野生动物", "wildlife"}, {"烤火", "bonfire"},
        {"疗愈之树", "healingTree"}, {"老鼠", "rat"}, {"土地庙", "prayerShrine"}, {"铜钱", "treasureCoin"}
        }));
    mainLayout->addLayout(bottomRow);

    // --- 5. 信号处理 ---
    connect(m_mapCombo, &QComboBox::currentIndexChanged, [this](int index) {
        emit mapChanged(m_mapCombo->currentData().toString().toStdString(), m_mapCombo->currentText().toStdString());
        notifySelectionChanged();
        });

    connect(btnSelectAll, &QPushButton::clicked, [this]() {
        for (auto const& [box, key] : m_boxMap) { box->blockSignals(true); box->setChecked(true); box->blockSignals(false); }
        notifySelectionChanged();
        });

    connect(btnClearAll, &QPushButton::clicked, [this]() {
        for (auto const& [box, key] : m_boxMap) { box->blockSignals(true); box->setChecked(false); box->blockSignals(false); }
        notifySelectionChanged();
        });

    mainLayout->addStretch(); // 底部撑开

    QTimer::singleShot(100, this, &ControlPanel::notifySelectionChanged);
}

void ControlPanel::notifySelectionChanged() {
    std::vector<std::string> selected;
    for (auto const& [box, key] : m_boxMap) {
        if (box->isChecked()) selected.push_back(key);
    }
    emit selectionChanged(selected);
}