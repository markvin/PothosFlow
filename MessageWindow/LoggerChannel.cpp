// Copyright (c) 2013-2020 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "MessageWindow/LoggerChannel.hpp"

static const size_t QUEUE_MAX_DEPTH = 30;

using Pothos::System::Logger;

LoggerChannel::LoggerChannel(QObject *parent):
    QObject(parent),
    _oldLevel(Logger::getLevel()),
    _listenerId(0)
{
    Logger::setLevel(Pothos::System::LogLevel::Trace); //lowest level -> shows everything
    _listenerId = Logger::addListener([this](const Pothos::System::LogRecord &record){this->log(record);});
}

LoggerChannel::~LoggerChannel(void)
{
    this->disconnect();
}

void LoggerChannel::disconnect(void)
{
    if (_listenerId == 0) return;
    Logger::removeListener(_listenerId); //no call of log() after this returns
    _listenerId = 0;
    Logger::setLevel(_oldLevel);
}

void LoggerChannel::log(const Pothos::System::LogRecord &record)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.push(record);

    //remove oldest if we exceed the max depth
    if (_queue.size() > QUEUE_MAX_DEPTH) _queue.pop();
}

bool LoggerChannel::pop(Pothos::System::LogRecord &record)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_queue.empty()) return false;
    record = std::move(_queue.front());
    _queue.pop();
    return true;
}
