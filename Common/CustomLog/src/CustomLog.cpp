#include <CustomLog/CustomLog.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/support/date_time.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

using file_sink_t = sinks::synchronous_sink<sinks::text_file_backend>;
using console_sink_t = sinks::synchronous_sink<sinks::text_ostream_backend>;

using scLogger = boost::log::sources::severity_channel_logger_mt<
    logging::trivial::severity_level,
    std::string>;

static std::once_flag g_init_flag;
static logging::trivial::severity_level g_min_level = logging::trivial::trace;
static std::atomic_bool g_shutdown_requested{ false };

static std::map<std::string, scLogger>& loggers()
{
    static auto* value = new std::map<std::string, scLogger>();
    return *value;
}

static std::map<std::string, boost::shared_ptr<file_sink_t>>& file_sinks()
{
    static auto* value = new std::map<std::string, boost::shared_ptr<file_sink_t>>();
    return *value;
}

static void mark_shutdown_requested()
{
    g_shutdown_requested.store(true, std::memory_order_release);
}


enum class LogColor {
    Default,
    Grey,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Bright
};


static std::map<LogColor, std::string> color_table = {
    {LogColor::Default, "\033[0m"},
    {LogColor::Grey,    "\033[2m"},
    {LogColor::Red,     "\033[31m"},
    {LogColor::Green,   "\033[32m"},
    {LogColor::Yellow,  "\033[33m"},
    {LogColor::Blue,    "\033[34m"},
    {LogColor::Magenta, "\033[35m"},
    {LogColor::Cyan,    "\033[36m"},
    {LogColor::White,   "\033[37m"},
    {LogColor::Bright,  "\033[1m"}
};

//static const std::map <std::string, std::string > color_map =
//{
//    {"bright", "\x1B[1m"},      // bright color
//    {"grey", "\x1B[2m"},        // grey
//    {"italic", "\x1B[3m"},      // italic
//    {"underline", "\x1B[4m"},   // underline
//    {"reverse", "\x1B[7m"},     // reverse
//    {"hidden", "\x1B[8m"},      // hidden
//    {"black", "\x1B[30m"},      // black
//    {"red", "\x1B[31m"},        // red
//    {"green", "\x1B[32m"},      // green
//    {"yellow", "\x1B[33m"},     // yellow
//    {"blue", "\x1B[34m"},       // blue
//    {"magenta", "\x1B[35m"},    // magenta
//    {"cyan", "\x1B[36m"},       // cyan
//    {"white", "\x1B[37m"},      // white
//
//    {"blackBG", "\x1B[40m"},    // blackBG
//    {"redBG", "\x1B[41m"},      // redBG
//    {"greenBG", "\x1B[42m"},    // greenBG
//    {"yellowBG", "\x1B[43m"},   // yellowBG
//    {"blueBG", "\x1B[44m"},     // blueBG
//    {"magentaBG", "\x1B[45m"},  // magentaBG
//    {"cyanBG", "\x1B[46m"},     // cyanBG
//    {"whiteBG", "\x1B[47m"}     // whiteBG
//};

// ---------- Level convert ----------
static logging::trivial::severity_level to_boost(CustomLog::Level lvl)
{
    using L = CustomLog::Level;
    switch (lvl)
    {
    case L::trace:   return logging::trivial::trace;
    case L::debug:   return logging::trivial::debug;
    case L::info:    return logging::trivial::info;
    case L::warning: return logging::trivial::warning;
    case L::error:   return logging::trivial::error;
    case L::fatal:   return logging::trivial::fatal;
    }
    return logging::trivial::info;
}

// ---------- Console Color Formatter ----------
static void console_formatter(
    logging::record_view const& rec,
    logging::formatting_ostream& strm)
{
    auto sev = rec[boost::log::trivial::severity];

    if (sev)
    {
        switch (sev.get())
        {
        case logging::trivial::trace:   strm << color_table[LogColor::White]; break;
        case logging::trivial::debug:   strm << color_table[LogColor::Cyan]; break;
        case logging::trivial::info:    strm << color_table[LogColor::Green]; break;
        case logging::trivial::warning: strm << color_table[LogColor::Yellow]; break;
        case logging::trivial::error:   strm << color_table[LogColor::Red]; break;
        case logging::trivial::fatal:   strm << color_table[LogColor::Magenta]; break;
        }
    }

    strm << "["
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
            "TimeStamp", "%H:%M:%S")(rec)
        << "]";

    if (sev)
        strm << "[" << sev.get() << "] ";

    strm << rec[boost::log::expressions::smessage];

    if (sev)
        strm << color_table[LogColor::Default];
}

// ---------- Init ----------
void CustomLog::init(const std::map<std::string, std::pair<bool, bool>>& cfg,
    const std::string& prefix,
    const std::string& path)
{
    std::string actual_path = path;
    if (actual_path.empty()) {
        // Get the log directory under the current directory.
        actual_path = (std::filesystem::current_path() / "log").string();
    }


    std::call_once(g_init_flag, [&]()
        {
            std::atexit(mark_shutdown_requested);
            std::filesystem::create_directories(actual_path);
            logging::core::get()->remove_all_sinks();
            logging::add_common_attributes();

            for (auto& c : cfg)
            {
                const std::string& channel = c.first;

                // Console sink
                if (c.second.second)
                {
                    auto backend = boost::make_shared<sinks::text_ostream_backend>();
                    auto stream = boost::shared_ptr<std::ostream>(&std::clog, [](std::ostream*) {});
                    backend->add_stream(stream);
                    backend->auto_flush(true);

                    auto sink = boost::make_shared<console_sink_t>(backend);
                    sink->set_filter(expr::attr<std::string>("Channel") == channel);
                    sink->set_formatter(&console_formatter);

                    logging::core::get()->add_sink(sink);
                }

                // File sink
                if (c.second.first)
                {
                    auto backend = boost::make_shared<sinks::text_file_backend>(
                        keywords::file_name = actual_path + "/" + prefix + "_" + channel + "_%Y%m%d_%H%M%S_%5N.log",
                        keywords::rotation_size = 10 * 1024 * 1024
                        );
                    backend->auto_flush(true);

                    auto sink = boost::make_shared<file_sink_t>(backend);
                    sink->set_filter(expr::attr<std::string>("Channel") == channel);
                    sink->set_formatter(
                        expr::stream
                        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S")
                        << "] [" << expr::attr<logging::trivial::severity_level>("Severity")
                        << "] " << expr::smessage             //  No blank or '\t'
                        //<< "] " << expr::message
                    );

                    logging::core::get()->add_sink(sink);
                    file_sinks()[channel] = sink;
                }

                loggers().emplace(channel, scLogger(keywords::channel = channel));
            }

            logging::core::get()->set_filter(
                logging::trivial::severity >= g_min_level);
        });
}

// ---------- Log ----------
void CustomLog::log(Level lvl, const std::string& channel, const std::string& msg)
{
    if (g_shutdown_requested.load(std::memory_order_acquire))
        return;

    auto& loggerMap = loggers();
    auto it = loggerMap.find(channel);
    if (it == loggerMap.end()) return;

    BOOST_LOG_SEV(it->second, to_boost(lvl)) << msg;
}

// ---------- Level ----------
void CustomLog::set_level(Level lvl)
{
    if (g_shutdown_requested.load(std::memory_order_acquire))
        return;

    g_min_level = to_boost(lvl);
    logging::core::get()->set_filter(logging::trivial::severity >= g_min_level);
}

// ---------- Rotation ----------
void CustomLog::rotate(const std::string& channel)
{
    if (g_shutdown_requested.load(std::memory_order_acquire))
        return;

    auto& sinks = file_sinks();
    auto it = sinks.find(channel);
    if (it != sinks.end())
        it->second->locked_backend()->rotate_file();
}

void CustomLog::rotate_all()
{
    if (g_shutdown_requested.load(std::memory_order_acquire))
        return;

    for (auto& s : file_sinks())
        s.second->locked_backend()->rotate_file();
}

void CustomLog::shutdown()
{
    if (g_shutdown_requested.exchange(true, std::memory_order_acq_rel))
        return;

    try {
        logging::core::get()->remove_all_sinks();
        file_sinks().clear();
        loggers().clear();
    } catch(...) {
    }
}
