// The screen saver settings, laid out like the classic dialog: a picture of a
// monitor showing the chosen screen saver, a list to choose from, Settings and
// Preview, how long to wait, and whether to lock on return.
#pragma once

#include <QImage>
#include <QProcess>
#include <QWidget>
#include <vector>

extern "C" {
#include "../common.h"
}

class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;

// The monitor at the top, with a still of the screen saver on its screen.
class Monitor : public QWidget {
    Q_OBJECT
public:
    explicit Monitor(QWidget *parent = nullptr);
    void show(const QImage &picture, const QString &caption);
    QSize sizeHint() const override { return {300, 236}; }
    QSize screenSize() const { return {248, 140}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage picture_;
    QString caption_;
};

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(QWidget *parent = nullptr);
    ~Panel() override;
    bool apply();           // save, and tell the service; false if it could not be saved
    bool changed() const { return changed_; }

signals:
    void changedChanged(bool);

private:
    void load();
    void chosen();          // the list changed: update the monitor and the buttons
    void touch();           // something was edited
    void preview();
    void settings();
    void makeStill(const Saver &s);
    QString current() const;
    const Saver *saver(const QString &id) const;

    Saver savers_[MAX_SAVERS];
    int nsavers_ = 0;
    Config cfg_ = {};
    std::vector<QString> random_; // the screen savers Random draws from; none = all
    bool changed_ = false, loading_ = false;

    Monitor *monitor_;
    QComboBox *list_;
    QPushButton *settings_, *preview_;
    QSpinBox *wait_, *rest_;
    QCheckBox *lock_;
    QProcess still_;
    QString stillFor_, stillFile_;
};
