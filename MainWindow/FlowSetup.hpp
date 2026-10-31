// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>

class QApplication;

/*!
 * The process settings of PothosFlow, for the application and the smoke test:
 * the Qt messages go to the Pothos logger, and the syslog listener gets the
 * logs of the server processes that flow starts (POTHOS_SYSLOG_ADDR).
 * The destructor stops the listener and removes the Qt message handler.
 */
class FlowProcessSetup
{
public:
    FlowProcessSetup(void);
    ~FlowProcessSetup(void);
    FlowProcessSetup(const FlowProcessSetup &) = delete;
    FlowProcessSetup &operator=(const FlowProcessSetup &) = delete;
};

//! Set the application name, organization, version and icon
void configureFlowApplication(QApplication &app);
