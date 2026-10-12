#include "panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

static const char NONE[] = "(none)", RANDOM[] = "(random)";

// ---- the monitor ----

Monitor::Monitor(QWidget *parent) : QWidget(parent) { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }

void Monitor::show(const QImage &picture, const QString &caption)
{
    picture_ = picture;
    caption_ = caption;
    update();
}

void Monitor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize s = screenSize();
    const QRectF screen((width() - s.width()) / 2.0, 14, s.width(), s.height());
    const QRectF body = screen.adjusted(-12, -12, 12, 18);
    const QColor casing = palette().color(QPalette::Mid), dark = palette().color(QPalette::Dark);
    // the stand, then the casing, then the screen
    QPainterPath neck;
    neck.addRect(QRectF(width() / 2.0 - 22, body.bottom() - 2, 44, 20));
    p.fillPath(neck, dark);
    p.setPen(Qt::NoPen);
    p.setBrush(casing);
    p.drawRoundedRect(QRectF(width() / 2.0 - 70, body.bottom() + 16, 140, 12), 5, 5);
    p.setPen(QPen(dark, 1.5));
    p.drawRoundedRect(body, 9, 9);
    p.fillRect(screen, Qt::black);
    if (!picture_.isNull()) p.drawImage(screen, picture_);
    else if (!caption_.isEmpty()) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(screen.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, caption_);
    }
    p.setPen(QPen(dark.darker(140), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(screen);
    p.setPen(Qt::NoPen); // the little light
    p.setBrush(QColor(110, 200, 110));
    p.drawEllipse(QPointF(body.right() - 16, body.bottom() - 9), 2.5, 2.5);
}

// ---- the panel ----

Panel::Panel(QWidget *parent) : QWidget(parent)
{
    monitor_ = new Monitor(this);
    list_ = new QComboBox(this);
    list_->setMinimumContentsLength(22);
    settings_ = new QPushButton(tr("Se&ttings..."), this);
    preview_ = new QPushButton(tr("Pre&view"), this);
    wait_ = new QSpinBox(this);
    wait_->setRange(1, 9999);
    lock_ = new QCheckBox(tr("On &resume, display lock screen"), this);
    rest_ = new QSpinBox(this);
    rest_->setRange(0, 9999);
    rest_->setSpecialValueText(tr("never"));
    rest_->setSuffix(tr(" min"));

    auto *top = new QHBoxLayout;
    top->addStretch(); top->addWidget(monitor_); top->addStretch();

    auto *box = new QGroupBox(tr("Screen saver"), this);
    auto *row1 = new QHBoxLayout;
    row1->addWidget(list_, 1); row1->addWidget(settings_); row1->addWidget(preview_);
    auto *waitLabel = new QLabel(tr("&Wait:"), this);
    waitLabel->setBuddy(wait_);
    auto *row2 = new QHBoxLayout;
    row2->addWidget(waitLabel); row2->addWidget(wait_); row2->addWidget(new QLabel(tr("minutes"), this));
    row2->addSpacing(12); row2->addWidget(lock_); row2->addStretch();
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->addLayout(row1); boxLayout->addLayout(row2);

    auto *power = new QGroupBox(tr("Power management"), this);
    auto *note = new QLabel(tr("A screen saver keeps the screen lit. To rest it, the screen saver stops after a while, and the "
                               "display's own power settings then decide when it turns off."), this);
    note->setWordWrap(true);
    auto *restLabel = new QLabel(tr("&Stop the screen saver after:"), this);
    restLabel->setBuddy(rest_);
    auto *row3 = new QHBoxLayout;
    row3->addWidget(restLabel); row3->addWidget(rest_); row3->addStretch();
    auto *powerLayout = new QVBoxLayout(power);
    powerLayout->addWidget(note); powerLayout->addLayout(row3);

    auto *all = new QVBoxLayout(this);
    all->addLayout(top); all->addWidget(box); all->addWidget(power); all->addStretch();

    connect(list_, &QComboBox::currentIndexChanged, this, [this] { chosen(); touch(); });
    connect(wait_, &QSpinBox::valueChanged, this, &Panel::touch);
    connect(rest_, &QSpinBox::valueChanged, this, &Panel::touch);
    connect(lock_, &QCheckBox::toggled, this, &Panel::touch);
    connect(preview_, &QPushButton::clicked, this, &Panel::preview);
    connect(settings_, &QPushButton::clicked, this, &Panel::settings);
    connect(&still_, &QProcess::finished, this, [this] {
        if (stillFor_ == current()) monitor_->show(QImage(stillFile_), QString());
    });
    load();
}

Panel::~Panel()
{
    still_.kill();
    still_.waitForFinished(500);
    savers_free(savers_, nsavers_);
    config_free(&cfg_);
}

const Saver *Panel::saver(const QString &id) const { return saver_by_id(savers_, nsavers_, id.toUtf8().constData()); }
QString Panel::current() const { return list_->currentData().toString(); }

void Panel::load()
{
    loading_ = true;
    savers_free(savers_, nsavers_);
    nsavers_ = savers_find(savers_, MAX_SAVERS);
    config_load(&cfg_);
    list_->clear();
    list_->addItem(tr("(None)"), QString::fromLatin1(NONE));
    list_->addItem(tr("(Random)"), QString::fromLatin1(RANDOM));
    for (int i = 0; i < nsavers_; i++) list_->addItem(QString::fromUtf8(savers_[i].name), QString::fromUtf8(savers_[i].id));
    random_.clear();
    QString pick = QString::fromLatin1(RANDOM);
    if (!cfg_.enabled) pick = QString::fromLatin1(NONE);
    else if (!qstrcmp(cfg_.mode, "single") && cfg_.nchosen > 0 && saver(QString::fromUtf8(cfg_.chosen[0]))) pick = QString::fromUtf8(cfg_.chosen[0]);
    else for (int i = 0; i < cfg_.nchosen; i++) random_.push_back(QString::fromUtf8(cfg_.chosen[i]));
    list_->setCurrentIndex(qMax(0, list_->findData(pick)));
    wait_->setValue(qMax(1, cfg_.idle / 60));
    rest_->setValue(cfg_.blank / 60);
    lock_->setChecked(cfg_.lock);
    loading_ = false;
    changed_ = false;
    chosen();
}

void Panel::touch()
{
    if (loading_ || changed_) return;
    changed_ = true;
    emit changedChanged(true);
}

void Panel::chosen()
{
    const QString id = current();
    const Saver *s = saver(id);
    preview_->setEnabled(id != QLatin1String(NONE) && nsavers_ > 0);
    settings_->setEnabled(id != QLatin1String(NONE));
    wait_->setEnabled(id != QLatin1String(NONE));
    if (id == QLatin1String(NONE)) monitor_->show(QImage(), QString());
    else if (id == QLatin1String(RANDOM)) monitor_->show(QImage(), nsavers_ ? tr("A different screen saver each time") : tr("No screen savers are installed"));
    else if (s) makeStill(*s);
}

// A still for the monitor: the screen saver is asked to draw one, once, and it is
// kept for next time.
void Panel::makeStill(const Saver &s)
{
    const QString id = QString::fromUtf8(s.id);
    if (!s.snapshot) { monitor_->show(QImage(), QString::fromUtf8(s.name)); return; }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/" NAME "/stills");
    QDir().mkpath(dir);
    const QString file = dir + QLatin1Char('/') + id + QStringLiteral(".bmp");
    if (QFileInfo::exists(file)) { monitor_->show(QImage(file), QString()); return; }
    monitor_->show(QImage(), QString::fromUtf8(s.name));
    if (still_.state() != QProcess::NotRunning) { still_.kill(); still_.waitForFinished(500); }
    QString cmd = QString::fromUtf8(s.snapshot);
    const QSize size = monitor_->screenSize() * 2;
    cmd.replace(QStringLiteral("%o"), file).replace(QStringLiteral("%w"), QString::number(size.width())).replace(QStringLiteral("%h"), QString::number(size.height()));
    char **argv = split_exec(cmd.toUtf8().constData());
    QStringList args;
    for (int i = 1; argv && argv[0] && argv[i]; i++) args << QString::fromUtf8(argv[i]);
    stillFor_ = id; stillFile_ = file;
    if (argv && argv[0]) still_.start(QString::fromUtf8(argv[0]), args);
    free_argv(argv);
}

void Panel::preview()
{
    QString id = current();
    if (id == QLatin1String(RANDOM)) {
        std::vector<QString> from = random_;
        if (from.empty()) for (int i = 0; i < nsavers_; i++) from.push_back(QString::fromUtf8(savers_[i].id));
        if (from.empty()) return;
        id = from[size_t(rand()) % from.size()];
    }
    QProcess::startDetached(QStringLiteral("screensaverctl"), {QStringLiteral("test"), id});
}

void Panel::settings()
{
    const QString id = current();
    if (id == QLatin1String(RANDOM)) {
        // which screen savers Random draws from, and how often it changes
        QDialog d(this);
        d.setWindowTitle(tr("Random Screen Saver"));
        auto *names = new QListWidget(&d);
        for (int i = 0; i < nsavers_; i++) {
            auto *item = new QListWidgetItem(QString::fromUtf8(savers_[i].name), names);
            item->setData(Qt::UserRole, QString::fromUtf8(savers_[i].id));
            bool on = random_.empty() || std::find(random_.begin(), random_.end(), QString::fromUtf8(savers_[i].id)) != random_.end();
            item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
            item->setToolTip(QString::fromUtf8(savers_[i].comment));
        }
        auto *every = new QSpinBox(&d);
        every->setRange(0, 999); every->setSpecialValueText(tr("never")); every->setSuffix(tr(" min")); every->setValue(cfg_.cycle / 60);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Change to another every:"), &d)); row->addWidget(every); row->addStretch();
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
        connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        auto *l = new QVBoxLayout(&d);
        l->addWidget(new QLabel(tr("Use these screen savers:"), &d)); l->addWidget(names); l->addLayout(row); l->addWidget(buttons);
        if (d.exec() != QDialog::Accepted) return;
        random_.clear();
        bool every_one = true;
        for (int i = 0; i < names->count(); i++) {
            if (names->item(i)->checkState() == Qt::Checked) random_.push_back(names->item(i)->data(Qt::UserRole).toString());
            else every_one = false;
        }
        if (every_one) random_.clear();
        cfg_.cycle = every->value() * 60;
        touch();
        return;
    }
    const Saver *s = saver(id);
    if (s && s->settings) {
        char **argv = split_exec(s->settings);
        QStringList args;
        for (int i = 1; argv && argv[0] && argv[i]; i++) args << QString::fromUtf8(argv[i]);
        if (argv && argv[0]) QProcess::startDetached(QString::fromUtf8(argv[0]), args);
        free_argv(argv);
    } else QMessageBox::information(this, tr("Screen Saver"), tr("This screen saver has no options that you can set."));
}

bool Panel::apply()
{
    const QString id = current();
    cfg_.enabled = id != QLatin1String(NONE);
    cfg_.idle = wait_->value() * 60;
    cfg_.blank = rest_->value() * 60;
    cfg_.lock = lock_->isChecked();
    std::vector<QString> use;
    if (id == QLatin1String(RANDOM)) { qstrcpy(cfg_.mode, "random"); use = random_; }
    else if (id != QLatin1String(NONE)) { qstrcpy(cfg_.mode, "single"); use.push_back(id); }
    if (id != QLatin1String(NONE)) { // (None) keeps the last choice, for when it is turned on again
        config_free(&cfg_);
        cfg_.chosen = static_cast<char **>(calloc(use.size() + 1, sizeof(char *)));
        for (const QString &u : use) cfg_.chosen[cfg_.nchosen++] = strdup(u.toUtf8().constData());
    }
    if (config_save(&cfg_) != 0) {
        QMessageBox::warning(this, tr("Screen Saver"), tr("The settings could not be saved."));
        return false;
    }
    // the service runs as part of the session; make sure it is there, and tell it
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("enable"), QStringLiteral("--now"), QStringLiteral("screensaverd.service")});
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("reload"), QStringLiteral("screensaverd.service")});
    changed_ = false;
    emit changedChanged(false);
    return true;
}
