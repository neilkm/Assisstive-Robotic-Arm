#include "StateMachineUi/StateMachineWindow.hpp"

#include "JetsonQtApp/QtUiCommon/AssetPaths.hpp"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QShortcut>
#include <QVBoxLayout>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

QString toQString(const std::string& value) {
    return QString::fromStdString(value);
}

}  // namespace

namespace statemachineui {

StateMachineWindow::StateMachineWindow(QWidget* parent)
    : QMainWindow(parent),
      stateMachine_(jetsonqt::statemachine::buildCookingUiStates()) {
    setWindowTitle(QStringLiteral("Jetson Nano State UI"));
    resize(kWindowWidth, kWindowHeight);

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    setCentralWidget(centralWidget);

    auto* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(60, 52, 50, 40);
    rootLayout->setSpacing(50);

    auto* leftPanel = new QWidget(centralWidget);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(18);

    auto* currentStateCaption = new QLabel(QStringLiteral("Current state"), leftPanel);
    currentStateCaption->setObjectName(QStringLiteral("captionLabel"));
    leftLayout->addWidget(currentStateCaption);

    stateLabel_ = new QLabel(leftPanel);
    stateLabel_->setObjectName(QStringLiteral("stateLabel"));
    stateLabel_->setMinimumHeight(72);
    leftLayout->addWidget(stateLabel_);

    auto* actionsCaption = new QLabel(QStringLiteral("Allowed actions"), leftPanel);
    actionsCaption->setObjectName(QStringLiteral("captionLabel"));
    leftLayout->addWidget(actionsCaption);

    actionList_ = new QListWidget(leftPanel);
    actionList_->setObjectName(QStringLiteral("actionList"));
    actionList_->setFocusPolicy(Qt::NoFocus);
    actionList_->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(actionList_, 1);

    auto* controlsLabel = new QLabel(
        QStringLiteral("Arrow keys: change selected action    Enter: trigger action    i: reset    q or ESC: quit"),
        leftPanel);
    controlsLabel->setObjectName(QStringLiteral("controlsLabel"));
    controlsLabel->setWordWrap(true);
    leftLayout->addWidget(controlsLabel);

    imageLabel_ = new QLabel(centralWidget);
    imageLabel_->setObjectName(QStringLiteral("imagePanel"));
    imageLabel_->setMinimumSize(580, 590);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setFrameShape(QFrame::Box);
    imageLabel_->setScaledContents(false);

    rootLayout->addWidget(leftPanel, 1);
    rootLayout->addWidget(imageLabel_, 1);

    setStyleSheet(QStringLiteral(R"(
        #centralWidget {
            background: #181c24;
        }
        #captionLabel {
            color: #e6f0ff;
            font-size: 30px;
            font-weight: 700;
        }
        #stateLabel {
            color: #50dcff;
            font-size: 48px;
            font-weight: 800;
        }
        #controlsLabel {
            color: #afb9c3;
            font-size: 22px;
            font-weight: 600;
        }
        #actionList {
            background: transparent;
            border: none;
            color: #e6f0ff;
            font-size: 26px;
            font-weight: 700;
            outline: none;
        }
        #actionList::item {
            background: #262c36;
            border: 2px solid #464e5c;
            color: #e6f0ff;
            margin-bottom: 12px;
            min-height: 34px;
            padding: 4px 16px;
        }
        #actionList::item:selected {
            background: #5ab45a;
            border: 2px solid #b4ffb4;
            color: #121812;
        }
        #imagePanel {
            background: #20242c;
            border: 2px solid #464e5c;
            color: #bec6d0;
            font-size: 28px;
            font-weight: 700;
        }
    )"));

    loadStateImages();
    refreshUi();
}

void StateMachineWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_Left:
        case Qt::Key_J:
            selectPreviousAction();
            return;
        case Qt::Key_Down:
        case Qt::Key_Right:
        case Qt::Key_K:
            selectNextAction();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            triggerSelectedAction();
            return;
        case Qt::Key_I:
            stateMachine_.reset();
            refreshUi();
            return;
        case Qt::Key_Q:
        case Qt::Key_Escape:
            QApplication::quit();
            return;
        default:
            QMainWindow::keyPressEvent(event);
            return;
    }
}

void StateMachineWindow::loadStateImages() {
    const QString imageDir = jetsonqt::qtui::findExistingAssetDirectory(
        QStringLiteral(STATE_MACHINE_UI_ASSET_RELATIVE_PATH));

    for (const auto& state : stateMachine_.states()) {
        const QString stateName = toQString(state.name);
        const QString filename = imageFilenameForState(stateName);
        if (filename.isEmpty()) {
            continue;
        }

        QPixmap pixmap(imageDir + QLatin1Char('/') + filename);
        if (!pixmap.isNull()) {
            stateImages_.insert(stateName, pixmap);
        }
    }
}

void StateMachineWindow::refreshUi() {
    const auto& state = stateMachine_.currentState();
    const QString stateName = toQString(state.name);
    stateLabel_->setText(stateName);

    actionList_->clear();
    for (const std::string& action : state.actions) {
        actionList_->addItem(toQString(action));
    }

    if (actionList_->count() > 0) {
        actionList_->setCurrentRow(static_cast<int>(stateMachine_.selectedActionIndex()));
    }

    const auto imageIt = stateImages_.constFind(stateName);
    if (imageIt == stateImages_.constEnd()) {
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->setText(QStringLiteral("No image yet for this state"));
        return;
    }

    imageLabel_->setText(QString());
    imageLabel_->setPixmap(imageIt.value().scaled(540, 530, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void StateMachineWindow::selectPreviousAction() {
    stateMachine_.selectPreviousAction();
    refreshUi();
}

void StateMachineWindow::selectNextAction() {
    stateMachine_.selectNextAction();
    refreshUi();
}

void StateMachineWindow::triggerSelectedAction() {
    stateMachine_.triggerSelectedAction();
    refreshUi();
}

QString StateMachineWindow::imageFilenameForState(const QString& stateName) const {
    if (stateName == QStringLiteral("Init")) {
        return QStringLiteral("init.png");
    }
    if (stateName == QStringLiteral("Selecting spice")) {
        return QStringLiteral("selectspice.png");
    }
    if (stateName == QStringLiteral("Spice selected")) {
        return QStringLiteral("spiceconfirm.png");
    }
    if (stateName == QStringLiteral("Shaking spice into pot")) {
        return QStringLiteral("usingspice.png");
    }
    if (stateName == QStringLiteral("Selecting utensil")) {
        return QStringLiteral("selectutensil.png");
    }
    if (stateName == QStringLiteral("Utensil selected")) {
        return QStringLiteral("confirmutensil.png");
    }
    if (stateName == QStringLiteral("Using utensil")) {
        return QStringLiteral("usingutensil.png");
    }

    return {};
}

}  // namespace statemachineui
