#pragma once
#include "CustomLog_export.h"
#include <string>
#include <map>
#include <sstream>

class CustomLog
{
public:
    //  ===== Log Level (External API, Boost not exposed) =====
    enum class Level { trace, debug, info, warning, error, fatal };

    //  ===== Initialize the logging system =====
    //  cfg[channel] = {toFile, toConsole}
    CUSTOMLOG_API static void init(
        const std::map<std::string, std::pair<bool, bool>>& cfg,
        const std::string& prefix = "CustomLog",
        const std::string& path = "");

    //  ===== Write logs (underlying call) =====
    CUSTOMLOG_API static void log(Level lvl,
        const std::string& channel,
        const std::string& msg);

    //  ===== Set Global Level Filtering =====
    CUSTOMLOG_API static void set_level(Level lvl);

    //  ===== Manual Rotation =====
    CUSTOMLOG_API static void rotate(const std::string& channel);
    CUSTOMLOG_API static void rotate_all();
    CUSTOMLOG_API static void shutdown();

    // =====================================================
    //  RAII Log Stream Object (Simulating BOOST_LOG_SEV)
    // =====================================================
    class LogStream
    {
    public:
        LogStream(Level lvl, std::string channel)
            : m_level(lvl), m_channel(std::move(channel)) {}

        // Copy is disabled.
        LogStream(const LogStream&) = delete;
        LogStream& operator=(const LogStream&) = delete;

        // Move is allowed.
        LogStream(LogStream&&) = default;
        LogStream& operator=(LogStream&&) = default;

        ~LogStream() noexcept
        {
            try {
                CustomLog::log(m_level, m_channel, m_stream.str());
            } catch(...) {
            }
        }

        // General value types.
        template<typename T>
        LogStream& operator<<(const T& v)
        {
            m_stream << v;
            return *this;
        }

        // std::endl, std::flush, etc.
        LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            manip(m_stream);
            return *this;
        }

        // std::hex, std::dec, std::setw, etc. (ios_base manipulators)
        LogStream& operator<<(std::ios_base& (*manip)(std::ios_base&))
        {
            manip(m_stream);
            return *this;
        }

    private:
        Level m_level;
        std::string m_channel;
        std::ostringstream m_stream;
    };
};



// ================== Macros that simulate BOOST_LOG_SEV ==================

#define LOG_TRACE(channel) CustomLog::LogStream(CustomLog::Level::trace, channel)
#define LOG_DEBUG(channel) CustomLog::LogStream(CustomLog::Level::debug, channel)
#define LOG_INFO(channel)  CustomLog::LogStream(CustomLog::Level::info, channel)
#define LOG_WARNING(channel)  CustomLog::LogStream(CustomLog::Level::warning, channel)
#define LOG_ERROR(channel) CustomLog::LogStream(CustomLog::Level::error, channel)
#define LOG_FATAL(channel) CustomLog::LogStream(CustomLog::Level::fatal, channel)

