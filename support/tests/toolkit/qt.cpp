#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QTabWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QTest>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("anto-toolkit-qt") + QString::number(QT_VERSION_MAJOR));
    QWidget window;
    window.setWindowTitle("Anto426 Qt palette and Kvantum test");
    window.resize(900, 580);
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(18);
    layout->addWidget(new QLabel(QStringLiteral("Qt ") + qVersion() + " · Kvantum + shared native controls"));
    auto *entry = new QLineEdit;
    entry->setPlaceholderText("A readable placeholder");
    layout->addWidget(entry);
    auto *button = new QPushButton("Shared shape, accent and native keyboard action");
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&] { clicks++; });
    layout->addWidget(button);
    auto *checkbox = new QCheckBox("Keep text opaque"); checkbox->setChecked(true); layout->addWidget(checkbox);
    auto *progress = new QProgressBar; progress->setValue(64); layout->addWidget(progress);
    auto *row = new QHBoxLayout;
    auto *combo = new QComboBox; combo->addItems({"Current palette", "Saved palette"}); row->addWidget(combo);
    auto *spin = new QSpinBox; spin->setValue(24); row->addWidget(spin);
    auto *slider = new QSlider(Qt::Horizontal); slider->setValue(64); row->addWidget(slider);
    layout->addLayout(row);
    auto *tabs = new QTabWidget;
    auto *tree = new QTreeWidget; tree->setHeaderLabels({"Shared view headers", "Value"});
    new QTreeWidgetItem(tree, {"Native item selection", "Current"});
    tabs->addTab(tree, "Views"); tabs->addTab(new QPlainTextEdit("Native text editor"), "Editor");
    tabs->setMaximumHeight(100); layout->addWidget(tabs);
    layout->addStretch();
    window.show();
    QTimer::singleShot(1500, [&] {
        QPalette palette = app.palette();
        QJsonObject report{{"toolkit", QStringLiteral("Qt") + QString::number(QT_VERSION_MAJOR)},
                           {"version", qVersion()}, {"style", app.style()->metaObject()->className()},
                           {"highlight", palette.color(QPalette::Highlight).name()},
                           {"placeholder", palette.color(QPalette::PlaceholderText).name()},
                           {"placeholderAlpha", palette.color(QPalette::PlaceholderText).alphaF()},
                           {"entryPlaceholder", entry->palette().color(QPalette::PlaceholderText).name()},
                           {"foregroundAlpha", palette.color(QPalette::WindowText).alphaF()},
                           {"translucentBackground", window.testAttribute(Qt::WA_TranslucentBackground)}};
        report["sharedGeometryInstalled"] = app.styleSheet().contains("border-radius: 12px") && app.styleSheet().contains("QComboBox::drop-down");
        button->setFocus(); QTest::keyClick(button, Qt::Key_Space);
        report["buttonKeyboard"] = clicks == 1;
        entry->setFocus(); QTest::keyClick(entry, Qt::Key_A);
        report["fieldKeyboard"] = entry->text() == "a"; entry->clear();
        QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, QPoint(10, checkbox->height()/2));
        report["checkboxPointer"] = !checkbox->isChecked(); checkbox->setChecked(true);
        spin->setFocus(); QTest::keyClick(spin, Qt::Key_Up);
        report["spinKeyboard"] = spin->value() == 25;
        combo->showPopup(); QTest::keyClick(combo, Qt::Key_Down); QTest::keyClick(combo, Qt::Key_Return);
        report["comboPopup"] = combo->currentIndex() == 1;
        window.activateWindow(); entry->setFocus(); QTest::qWait(150); app.processEvents();
        const QImage fieldImage = entry->grab().toImage();
        report["paintedFocusBorder"] = fieldImage.pixelColor(fieldImage.width()/2, 0).name();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        report["accent"] = palette.color(QPalette::Accent).name();
#endif
        QFile file(qEnvironmentVariable("ANTO_TOOLKIT_REPORT"));
        file.open(QFile::WriteOnly); file.write(QJsonDocument(report).toJson());
    });
    return app.exec();
}
