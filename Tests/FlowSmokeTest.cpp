// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Smoke test of PothosFlow without a display (QT_QPA_PLATFORM=offscreen):
//the main window comes into view, the block cache gets the blocks,
//Help -> About shows the about window, the message window shows a log
//message, and the window closes without an error.

#include "MainWindow/FlowSetup.hpp"
#include "MainWindow/MainWindow.hpp"
#include "MainWindow/MainActions.hpp"
#include "BlockTree/BlockCache.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphEditorTabs.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include <Pothos/Exception.hpp>
#include <Pothos/System/Logger.hpp>
#include <QAction>
#include <QAbstractButton>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QThreadPool>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Pothos::System::Logger;
using Pothos::System::LogLevel;
using Pothos::System::LogRecord;

static int failures = 0;

static void check(const std::string &name, const bool ok, const std::string &reason = "")
{
    if (ok) std::cout << "PASS " << name << std::endl;
    else
    {
        std::cout << "FAIL " << name << ": " << reason << std::endl;
        failures++;
    }
}

//process the Qt events until the condition is true or the timeout expires
static bool waitFor(const std::function<bool(void)> &condition, const int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (not condition())
    {
        if (timer.elapsed() > timeoutMs) return false;
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

//a visible message box with the text; not the title: a message box on macOS has no title
static QMessageBox *findMessageBox(const QString &text)
{
    for (auto widget : QApplication::topLevelWidgets())
    {
        auto box = qobject_cast<QMessageBox *>(widget);
        if (box != nullptr and box->isVisible() and box->text().contains(text)) return box;
    }
    return nullptr;
}

int main(int argc, char **argv)
{
    //a hang anywhere (also in a blocking call) ends the test with a failure
    std::thread([]
    {
        std::this_thread::sleep_for(std::chrono::seconds(150));
        std::cout << "FAIL timeout: the smoke test did not end in 150 s" << std::endl;
        std::_Exit(EXIT_FAILURE);
    }).detach();

    //every record at Error level or worse is a failure, also in the exit
    //path (the destruction of the application and the syslog listener)
    std::mutex errorsMutex;
    std::vector<LogRecord> errors;
    const auto errorListener = Logger::addListener([&](const LogRecord &r)
    {
        if (r.level > LogLevel::Error) return;
        std::lock_guard<std::mutex> lock(errorsMutex);
        errors.push_back(r);
    });

    {
    FlowProcessSetup processSetup;
    QApplication app(argc, argv);
    configureFlowApplication(app);

    POTHOS_EXCEPTION_TRY
    {
        //1) the main window comes into view
        auto window = new MainWindow(nullptr);
        check("window_visible", waitFor([window]{return window->isVisible();}, 30000), "the main window is not visible");

        //2) the block cache gets the descriptions of the blocks
        //the first update can end in the constructor (the splash screen processes
        //the events): connect, then update the cache again
        bool cacheReady = false, copierFound = false;
        QJsonObject copierDesc;
        QObject::connect(BlockCache::global(), &BlockCache::blockDescUpdate, [&](const QJsonArray &descs)
        {
            cacheReady = true;
            for (const auto &desc : descs)
            {
                if (desc.toObject()["path"].toString() != "/blocks/copier") continue;
                copierFound = true;
                copierDesc = desc.toObject();
            }
        });
        BlockCache::global()->update();
        check("block_cache", waitFor([&cacheReady]{return cacheReady;}, 60000) and copierFound,
            cacheReady? "no description of /blocks/copier (is PothosBlocks installed?)" : "no blockDescUpdate signal");
        check("background_work", QThreadPool::globalInstance()->waitForDone(60000), "the start work of Qt Concurrent did not end");

        //3) Help -> About shows the about window
        //on Linux and Windows QMessageBox::about() blocks in exec(): a timer finds the window
        bool aboutFound = false, aboutModal = false;
        QTimer finder;
        QObject::connect(&finder, &QTimer::timeout, [&]
        {
            auto box = findMessageBox("Pothos v"); //the about window of MainWindow::handleShowAbout()
            if (box == nullptr) return;
            aboutFound = true;
            aboutModal = (QApplication::activeModalWidget() == box);
            finder.stop();
            box->accept();
        });
        finder.start(100);
        MainActions::global()->showAboutAction->trigger();
        waitFor([&aboutFound]{return aboutFound;}, 10000);
        finder.stop();
        #ifdef Q_OS_MACOS
        if (aboutFound and not aboutModal) std::cout << "INFO about window is not modal on macOS (QMessageBox::about)" << std::endl;
        check("about_window", aboutFound, "no visible about window");
        #else
        check("about_window", aboutFound and aboutModal, aboutFound? "the about window is not modal" : "no visible about window");
        #endif

        //4) the message window shows a log message (the core log listener)
        //the display keeps only the latest records: log the marker again until it is shown
        const auto marker = "smoke-marker-" + std::to_string(QApplication::applicationPid());
        const auto textEdits = window->findChildren<QPlainTextEdit *>();
        bool markerShown = false;
        for (int i = 0; i < 10 and not markerShown; i++)
        {
            POTHOS_LOG_INFORMATION("PothosFlow.SmokeTest", "{}", marker);
            Logger::flush();
            markerShown = waitFor([&]
            {
                for (auto edit : textEdits) if (edit->toPlainText().contains(QString::fromStdString(marker))) return true;
                return false;
            }, 1000);
        }
        check("message_window", markerShown, "the message window does not show " + marker);

        //5) a selected block in the graph editor: the exit destroys it with the scene
        auto editorTabs = window->findChild<GraphEditorTabs *>();
        auto editor = (editorTabs == nullptr)? nullptr : editorTabs->getCurrentGraphEditor();
        bool blockAdded = false;
        if (editor != nullptr and not copierDesc.isEmpty())
        {
            auto draw = editor->getCurrentGraphDraw();
            editor->handleAddBlock(copierDesc, QPointF(100, 100), draw);
            for (auto item : draw->scene()->items())
            {
                if (item->flags() & QGraphicsItem::ItemIsSelectable) {item->setSelected(true); blockAdded = true;}
            }
        }
        check("graph_block", blockAdded, "no graph editor or no block in its scene");

        //6) close the window: the destructor unloads the plugins and stops the server
        //the topology has unsaved changes: answer "No" to the exit question
        QTimer exitAnswer;
        QObject::connect(&exitAnswer, &QTimer::timeout, [&exitAnswer]
        {
            auto box = findMessageBox("Would you like to save changes?");
            if (box == nullptr) return;
            exitAnswer.stop();
            box->button(QMessageBox::No)->click();
        });
        exitAnswer.start(100);
        window->close();
        exitAnswer.stop();
        delete window;
        QApplication::processEvents();
        check("window_closed", true);
    }
    POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
    {
        check("exception", false, ex.displayText());
    }
    } //the application and the syslog listener end here

    Logger::flush();
    Logger::removeListener(errorListener);
    for (const auto &r : errors) std::cout << "  error record: [" << r.source << "] " << r.text << std::endl;
    check("errors", errors.empty(), std::to_string(errors.size()) + " records at Error level or worse");

    std::cout << (failures == 0? "SUCCESS" : "FAILURE") << std::endl;
    return (failures == 0)? EXIT_SUCCESS : EXIT_FAILURE;
}
