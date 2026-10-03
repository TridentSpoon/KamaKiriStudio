// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <memory>
namespace Studio {
// Receives only activity/idle notifications; it never reads keys, text, or pointer positions.
class InputMonitor : public QObject {
    Q_OBJECT
public:
    explicit InputMonitor(QObject *parent=nullptr);
    ~InputMonitor() override;
    bool prepare();
    bool arm();
    void stop();
signals:
    void activity();
    void unavailable();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
