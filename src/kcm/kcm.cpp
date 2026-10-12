// The screen saver settings as a page in KDE's System Settings. It is the same
// panel as the stand-alone window, in the frame System Settings provides.
#include "../settings/panel.h"

#include <KCModule>
#include <KPluginFactory>
#include <QVBoxLayout>

class ScreenSaverPage : public KCModule {
    Q_OBJECT
public:
    ScreenSaverPage(QObject *parent, const KPluginMetaData &data) : KCModule(parent, data)
    {
        auto *layout = new QVBoxLayout(widget());
        panel_ = new Panel(widget());
        layout->addWidget(panel_);
        layout->addStretch();
        connect(panel_, &Panel::changedChanged, this, [this](bool changed) { setNeedsSave(changed); });
        setButtons(Apply);
    }
    void load() override { panel_->reload(); setNeedsSave(false); }
    void save() override { panel_->apply(); }

private:
    Panel *panel_;
};

K_PLUGIN_CLASS_WITH_JSON(ScreenSaverPage, "kcm_screensaverservice.json")

#include "kcm.moc"
