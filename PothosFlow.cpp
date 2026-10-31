// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/MainWindow.hpp"
#include "MainWindow/MainSettings.hpp"
#include "MainWindow/FlowSetup.hpp"
#include <Pothos/Exception.hpp>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <stdexcept>
#include <cstdlib> //EXIT_FAILURE

int main(int argc, char **argv)
{
    FlowProcessSetup processSetup;

    //did the user specified files on the command line?
    //stash the files so they are loaded into the editor
    //this replaces the currently stored file list
    QStringList files;
    for (int i = 1; i < argc; i++)
    {
        QString file(argv[i]);
        if (file.isEmpty()) continue;
        files.push_back(QDir(file).absolutePath());
    }
    if (not files.isEmpty())
    {
        auto settings = new MainSettings(nullptr);
        settings->setValue("GraphEditorTabs/files", files);
        delete settings;
    }

    //create the entry point to the GUI
    QApplication app(argc, argv);
    configureFlowApplication(app);

    POTHOS_EXCEPTION_TRY
    {
        //create the main window for the GUI
        MainWindow mainWindow(nullptr);

        //begin application execution
        return app.exec();
    }
    POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
    {
        QMessageBox msgBox(QMessageBox::Critical, "PothosFlow Application Error", QString::fromStdString(ex.displayText()));
        msgBox.exec();
        return EXIT_FAILURE;
    }
}
