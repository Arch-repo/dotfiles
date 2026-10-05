#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QTimer>
#include <QQuickItem>
#include <QTest>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName("anto-toolkit-quick");
    QQmlApplicationEngine engine;
    int warningCount = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        warningCount += errors.size();
        for (const auto &error : errors) fprintf(stderr, "%s\n", qPrintable(error.toString()));
    });
    engine.loadData(R"(
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
ApplicationWindow {
  id: window
  property int actionClicks: 0
  visible: true; width: 900; height: 580
  title: "Anto426 Qt Quick palette test"
  ColumnLayout {
    anchors.left: parent.left; anchors.right: parent.right
    anchors.top: parent.top; anchors.margins: 32; spacing: 18
    Label { text: "Qt Quick Controls · shared application palette" }
    TextField { objectName: "field"; placeholderText: "Readable placeholder"; Layout.fillWidth: true }
    Button { objectName: "action"; text: "Shared shape, color and states"; Layout.fillWidth: true; onClicked: window.actionClicks++ }
    Switch { objectName: "toggle"; text: "Shared accent"; checked: true }
    ProgressBar { value: 0.64; Layout.fillWidth: true }
    RowLayout {
      CheckBox { text: "Selection"; checked: true }
      RadioButton { text: "Choice"; checked: true }
      ComboBox { objectName: "combo"; model: ["Current palette", "Saved palette"] }
      SpinBox { value: 24 }
    }
    Slider { value: 0.64; Layout.fillWidth: true }
  }
  Item {
    visible: false
    BusyIndicator { running: false }
    DelayButton { text: "Hold" }
    Dial {}
    RangeSlider {}
    ScrollIndicator {}
    PageIndicator { count: 3 }
    SplitView {}
    Drawer {}
    StackView {}
    SwipeView {}
    RoundButton { text: "+" }
    ToolButton { text: "Tool" }
    TabButton { text: "Tab" }
    ItemDelegate { text: "Item" }
    MenuBarItem { text: "Menu" }
    CheckDelegate { text: "Check" }
    RadioDelegate { text: "Radio" }
    SwitchDelegate { text: "Switch" }
    TextArea { placeholderText: "Text" }
    Pane {}
    Page {}
    Frame {}
    ToolBar {}
    TabBar {}
    MenuBar {}
    GroupBox { title: "Group" }
    Popup {}
    Menu { MenuItem { text: "Action" }
    MenuSeparator {} }
    Dialog { title: "Dialog"; standardButtons: Dialog.Ok | Dialog.Cancel }
    ToolTip { text: "Help" }
    ToolSeparator {}
    ScrollView { width: 100; height: 50; contentWidth: 200; contentHeight: 200 }
  }
}
)");
    if (engine.rootObjects().isEmpty()) return 1;
    QTimer::singleShot(1500, [&] {
        QPalette palette = app.palette();
        QJsonObject report{{"toolkit", QStringLiteral("QtQuick") + QString::number(QT_VERSION_MAJOR)}, {"version", qVersion()},
                           {"highlight", palette.color(QPalette::Highlight).name()},
                           {"placeholder", palette.color(QPalette::PlaceholderText).name()},
                           {"foregroundAlpha", palette.color(QPalette::WindowText).alphaF()},
                           {"windowAlpha", palette.color(QPalette::Window).alphaF()}};
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        report["accent"] = palette.color(QPalette::Accent).name();
#endif
        report["qmlWarnings"] = warningCount;
        report["style"] = QQuickStyle::name();
        QObject *root = engine.rootObjects().first();
        auto *action = root->findChild<QQuickItem *>("action");
        auto *field = root->findChild<QQuickItem *>("field");
        auto *toggle = root->findChild<QQuickItem *>("toggle");
        auto *window = qobject_cast<QWindow *>(root);
        auto *combo = root->findChild<QQuickItem *>("combo");
        QObject *buttonBackground = action ? action->property("background").value<QObject *>() : nullptr;
        report["buttonRadius"] = buttonBackground ? buttonBackground->property("radius").toDouble() : -1;
        QObject *fieldBackground = field ? field->property("background").value<QObject *>() : nullptr;
        report["fieldRadius"] = fieldBackground ? fieldBackground->property("radius").toDouble() : -1;
        action->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        report["buttonKeyboard"] = root->property("actionClicks").toInt() == 1;
        field->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_A);
        report["fieldKeyboard"] = field->property("text").toString() == "a";
        field->setProperty("text", "");
        QPoint center = toggle->mapToScene(QPointF(toggle->width()/2,toggle->height()/2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        report["switchPointer"] = !toggle->property("checked").toBool();
        toggle->setProperty("checked", true);
        combo->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Space);
        app.processEvents();
        QObject *popup = combo->property("popup").value<QObject *>();
        report["comboPopup"] = popup && popup->property("visible").toBool();
        QTest::keyClick(window, Qt::Key_Down); QTest::keyClick(window, Qt::Key_Return);
        report["comboKeyboard"] = combo->property("currentIndex").toInt() == 1;
        field->forceActiveFocus();
        QObject *border = fieldBackground->property("border").value<QObject *>();
        report["fieldFocusBorder"] = border ? border->property("color").value<QColor>().name() : "missing";
        report["paintedWindowAlpha"] = root->property("background").value<QObject *>()->property("color").value<QColor>().alphaF();
        QFile file(qEnvironmentVariable("ANTO_TOOLKIT_REPORT"));
        file.open(QFile::WriteOnly); file.write(QJsonDocument(report).toJson());
    });
    return app.exec();
}
