// screensaver-settings: the settings window on its own.
#include "panel.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("screensaver-settings"));
    QApplication::setApplicationVersion(QStringLiteral(VERSION));
    QApplication::setDesktopFileName(QStringLiteral("screensaver-settings"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-screensaver")));

    QDialog window;
    window.setWindowTitle(QObject::tr("Screen Saver Settings"));
    auto *tabs = new QTabWidget(&window);
    auto *panel = new Panel(tabs);
    tabs->addTab(panel, QObject::tr("Screen Saver"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, &window);
    QPushButton *applyButton = buttons->button(QDialogButtonBox::Apply);
    applyButton->setEnabled(false);
    QObject::connect(panel, &Panel::changedChanged, applyButton, &QPushButton::setEnabled);
    QObject::connect(applyButton, &QPushButton::clicked, panel, &Panel::apply);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &window, [&] { if (!panel->changed() || panel->apply()) window.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &window, &QDialog::reject);
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
    window.setFixedSize(window.sizeHint());
    // --grab FILE: save a picture of the window and leave (for the documentation)
    const QStringList args = QApplication::arguments();
    const int grab = args.indexOf(QStringLiteral("--grab"));
    if (grab > 0 && grab + 1 < args.size()) {
        const QString file = args.at(grab + 1);
        QTimer::singleShot(4000, &window, [&window, file] { window.grab().save(file); QApplication::quit(); });
    }
    window.show();
    return app.exec();
}
