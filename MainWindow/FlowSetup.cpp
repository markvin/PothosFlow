// Copyright (c) 2013-2021 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/FlowSetup.hpp"
#include "MainWindow/IconUtils.hpp"
#include <Pothos/System.hpp>
#include <Pothos/System/Logger.hpp>
#include <QApplication>
#include <QIcon>
#include <QtGlobal>

static void myQLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    using Pothos::System::LogLevel;
    LogLevel level(LogLevel::Information);
    switch (type) {
    case QtDebugMsg: level = LogLevel::Debug; break;
    case QtInfoMsg: level = LogLevel::Information; break;
    case QtWarningMsg: level = LogLevel::Warning; break;
    case QtCriticalMsg: level = LogLevel::Critical; break;
    case QtFatalMsg: level = LogLevel::Fatal; break;
    }
    const std::string source((context.category != nullptr)? context.category : "default");
    Pothos::System::Logger::log(level, source, msg.toStdString());
}

static QtMessageHandler previousQLogHandler = nullptr;

FlowProcessSetup::FlowProcessSetup(void)
{
    previousQLogHandler = qInstallMessageHandler(myQLogHandler);
    const auto port = Pothos::System::Logger::startSyslogListener();
    //FIXME listener server supports IPv4 only; must forward to IPv4
    qputenv("POTHOS_SYSLOG_ADDR", QByteArray::fromStdString("127.0.0.1:"+port));
}

FlowProcessSetup::~FlowProcessSetup(void)
{
    Pothos::System::Logger::stopSyslogListener();
    qInstallMessageHandler(previousQLogHandler);
}

void configureFlowApplication(QApplication &app)
{
    app.setApplicationName("Pothos");
    app.setOrganizationName("PothosWare");
    app.setOrganizationDomain("www.pothosware.com");
    app.setApplicationVersion(QString::fromStdString(Pothos::System::getApiVersion()));

    //setup the application icon
    app.setWindowIcon(QIcon(makeIconPath("PothosFlow.png")));
}
