// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <Pothos/System/Logger.hpp>
#include <QStackedWidget>
#include <memory>

class LoggerChannel;
class QPlainTextEdit;
class QToolButton;
class QTimer;

class LoggerDisplay : public QStackedWidget
{
    Q_OBJECT
public:
    LoggerDisplay(QWidget *parent);
    ~LoggerDisplay(void);

private slots:
    void handleCheckMsgs(void);

private:
    void handleLogMessage(const Pothos::System::LogRecord &record);

    std::unique_ptr<LoggerChannel> _channel;
    QPlainTextEdit *_text;
    QToolButton *_clearButton;
    QTimer *_timer;

    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
};
