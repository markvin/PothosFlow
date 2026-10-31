// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "MessageWindow/LoggerDisplay.hpp"
#include "MessageWindow/LoggerChannel.hpp"
#include "GraphEditor/Constants.hpp"
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QTimer>
#include <chrono>
#include <format>

static const long CHECK_MSGS_TIMEOUT_MS = 100;
static const size_t MAX_MSGS_PER_TIMEOUT = 3;
static const size_t MAX_HISTORY_MSGS = 4096;

LoggerDisplay::LoggerDisplay(QWidget *parent):
    QStackedWidget(parent),
    _channel(new LoggerChannel(nullptr)),
    _text(new QPlainTextEdit(this)),
    _clearButton(new QToolButton(_text)),
    _timer(new QTimer(this))
{
    this->addWidget(_text);
    _text->setReadOnly(true);
    _text->setMaximumBlockCount(MAX_HISTORY_MSGS);
    _text->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    _clearButton->hide();
    _clearButton->setIcon(makeIconFromTheme("edit-clear-list"));
    _clearButton->setToolTip(tr("Clear message history"));
    connect(_clearButton, &QToolButton::clicked, _text, &QPlainTextEdit::clear);
    connect(_timer, &QTimer::timeout, this, &LoggerDisplay::handleCheckMsgs);
    _timer->start(CHECK_MSGS_TIMEOUT_MS);
}

LoggerDisplay::~LoggerDisplay(void)
{
    _channel->disconnect();
}

void LoggerDisplay::handleCheckMsgs(void)
{
    const bool autoScroll = _text->verticalScrollBar()->value()+50 > _text->verticalScrollBar()->maximum();

    size_t numMsgs = 0;
    Pothos::System::LogRecord record;
    while (_channel->pop(record))
    {
        this->handleLogMessage(record);
        if (++numMsgs == MAX_MSGS_PER_TIMEOUT) break;
    }

    if (numMsgs != 0 and autoScroll)
    {
        auto c =  _text->textCursor();
        c.movePosition(QTextCursor::End);
        _text->setTextCursor(c);
    }
}

void LoggerDisplay::handleLogMessage(const Pothos::System::LogRecord &record)
{
    using Pothos::System::LogLevel;
    QString color;
    switch (record.level)
    {
    case LogLevel::Information: color = defaultPaletteForeground(); break;
    case LogLevel::Notice: color = "green"; break;
    case LogLevel::Warning: color = "orange"; break;
    case LogLevel::Error: color = "red"; break;
    case LogLevel::Critical: color = "red"; break;
    case LogLevel::Fatal: color = "red"; break;
    default: color = defaultPaletteForeground();
    }

    QString sep(":");
    //UTC time with microseconds (the format of the earlier versions)
    const auto timeStr = std::format("{:%H:%M:%S}", std::chrono::floor<std::chrono::microseconds>(record.time));

    auto body = QString::fromStdString(record.text).toHtmlEscaped();
    if (body.count("\n") > 1)
    {
        sep = " &rArr;";
        body = "<pre style='background-color:#eaecee;'>"+body+"</pre><br />";
    }

    auto line = QString("<font color=\"%1\"><b>[%2] %3%4</b></font> %5").arg(
        color, QString::fromStdString(timeStr),
        QString::fromStdString(record.source),
        sep, body);

    _text->appendHtml(line);
}

void LoggerDisplay::resizeEvent(QResizeEvent *event)
{
    _clearButton->move(_text->viewport()->width()-_clearButton->width(), 0);
    return QStackedWidget::resizeEvent(event);
}

void LoggerDisplay::enterEvent(QEnterEvent *event)
{
    _clearButton->show();
    _clearButton->move(_text->viewport()->width()-_clearButton->width(), 0);
    return QStackedWidget::enterEvent(event);
}

void LoggerDisplay::leaveEvent(QEvent *event)
{
    _clearButton->hide();
    return QStackedWidget::leaveEvent(event);
}
