// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qboxserver.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QProcess>
#include <QRect>
#include <QLoggingCategory>


int main(int argc, char **argv)
{
    wlr_log_init(WLR_DEBUG, NULL);
    QGuiApplication app(argc, argv);

    app.setApplicationName("qwlbox");
    app.setApplicationVersion("0.0.1");

    QCommandLineOption startup("s", "startup command", "command");
    QCommandLineParser cl;

    cl.addOption(startup);
    cl.addHelpOption();
    cl.addVersionOption();
    cl.process(app);

    QBoxServer server;
    if (!server.start())
        return -1;

    if (cl.isSet(startup)) {
        const QString command = cl.value(startup);
        QProcess::startDetached("/bin/sh", {"-c", command});
    }

    return app.exec();
}
