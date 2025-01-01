#include "logger.h"

#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>
#include <log4cplus/loggingmacros.h>

#include "tiger/base.hpp"

namespace ti {

void TigerLogOutput(QtMsgType type, const QMessageLogContext& context,
                    const QString& msg) {
    static log4cplus::Logger main_logger =
        log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("rootLogger"));
    static log4cplus::Logger error_logger =
        log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("error"));
    static QDir src_dir(PROJECT_SRC_DIR);

    // Get relative file path
    const std::string relative_file =
        src_dir.relativeFilePath(context.file).toStdString();
    const std::string function_name = QString(context.function).toStdString();
    const std::string message = msg.toStdString();

    switch (type) {
        case QtDebugMsg:
            LOG4CPLUS_DEBUG_FMT(main_logger,
                                LOG4CPLUS_TEXT("DEBUG | %s:%u, %s\n%s"),
                                relative_file.c_str(), context.line,
                                function_name.c_str(), message.c_str());
            break;

        case QtInfoMsg:
            LOG4CPLUS_INFO_FMT(main_logger,
                               LOG4CPLUS_TEXT("INFO | %s:%u, %s\n%s"),
                               relative_file.c_str(), context.line,
                               function_name.c_str(), message.c_str());
            break;

        case QtWarningMsg:
            LOG4CPLUS_WARN_FMT(main_logger,
                               LOG4CPLUS_TEXT("WARNING | %s:%u, %s\n%s"),
                               relative_file.c_str(), context.line,
                               function_name.c_str(), message.c_str());
            break;

        case QtCriticalMsg:
            LOG4CPLUS_ERROR_FMT(error_logger,
                                LOG4CPLUS_TEXT("ERROR | %s:%u, %s\n%s"),
                                relative_file.c_str(), context.line,
                                function_name.c_str(), message.c_str());
            break;

        case QtFatalMsg:
            LOG4CPLUS_FATAL_FMT(error_logger,
                                LOG4CPLUS_TEXT("FATAL | %s:%u, %s\n%s"),
                                relative_file.c_str(), context.line,
                                function_name.c_str(), message.c_str());
            break;

        default:
            LOG4CPLUS_WARN_FMT(main_logger,
                               LOG4CPLUS_TEXT("UNKNOWN | %s:%u, %s\n%s"),
                               relative_file.c_str(), context.line,
                               function_name.c_str(), message.c_str());
            break;
    }
}

}  // namespace ti
