// Copyright (c) 2013-2014 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <Pothos/System/Logger.hpp>
#include <QObject>
#include <cstddef>
#include <queue>
#include <mutex>

/*!
 * Receives the log records of the process (a log listener of the core)
 * and keeps the latest ones in a queue for the message display.
 * While it is connected, the root log level is Trace (shows everything).
 */
class LoggerChannel : public QObject
{
    Q_OBJECT
public:
    LoggerChannel(QObject *parent);

    ~LoggerChannel(void);

    //! Remove the listener and set the old root log level again
    void disconnect(void);

    //! Take the oldest record, false when the queue is empty
    bool pop(Pothos::System::LogRecord &record);

private:
    void log(const Pothos::System::LogRecord &record);

    const Pothos::System::LogLevel _oldLevel;
    std::size_t _listenerId;
    std::queue<Pothos::System::LogRecord> _queue;
    std::mutex _mutex;
};
