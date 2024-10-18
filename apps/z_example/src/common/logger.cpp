#include "logger.h"

#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>

#include "tiger/base.hpp"

namespace ti
{
    void laserLogOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        static log4cplus::Logger main_logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("rootLogger"));
        static log4cplus::Logger error_logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("error"));
        static QDir src_dir(PROJECT_SRC_DIR);

        switch (type)
        {
        case QtDebugMsg:
            LOG4CPLUS_DEBUG_FMT(main_logger, LOG4CPLUS_TEXT("DEBUG  |  %s:%u, %s \n %s\n"),
                                src_dir.relativeFilePath(context.file).toStdWString().c_str(),
                                context.line,
                                QString(context.function).toStdWString().c_str(),
                                msg.toStdWString().c_str());
            break;
        case QtInfoMsg:
            LOG4CPLUS_INFO_FMT(main_logger, LOG4CPLUS_TEXT("INFO  |  %s:%u, %s \n %s\n"),
                               src_dir.relativeFilePath(context.file).toStdWString().c_str(),
                               context.line,
                               QString(context.function).toStdWString().c_str(),
                               msg.toStdWString().c_str());
            break;
        case QtWarningMsg:
            LOG4CPLUS_WARN_FMT(main_logger, LOG4CPLUS_TEXT("WARNING  |  %s:%u, %s \n %s\n"),
                               src_dir.relativeFilePath(context.file).toStdWString().c_str(),
                               context.line,
                               QString(context.function).toStdWString().c_str(),
                               msg.toStdWString().c_str());
            break;
        case QtCriticalMsg:
            LOG4CPLUS_ERROR_FMT(error_logger, LOG4CPLUS_TEXT("ERROR  |  %s:%u, %s \n %s\n"),
                                src_dir.relativeFilePath(context.file).toStdWString().c_str(),
                                context.line,
                                QString(context.function).toStdWString().c_str(),
                                msg.toStdWString().c_str());
            break;
        case QtFatalMsg:
            LOG4CPLUS_FATAL_FMT(error_logger, LOG4CPLUS_TEXT("FATAL  |  %s:%u, %s \n %s\n"),
                                src_dir.relativeFilePath(context.file).toStdWString().c_str(),
                                context.line,
                                QString(context.function).toStdWString().c_str(),
                                msg.toStdWString().c_str());
            break;
        }
    }
}
