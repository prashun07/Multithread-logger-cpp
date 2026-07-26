#pragma once
#include "LogRecord.h"
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;
    virtual std::string format(const LogRecord& record) = 0;
};


inline std::tm local_time(std::time_t t) {
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

inline std::string thread_id_to_string(std::thread::id id) {
    std::ostringstream oss;
    oss << id;
    return oss.str();
}

/* ---------- TextFormatter ---------- */
class TextFormatter : public ILogFormatter {
    std::string pattern_;
public:
    explicit TextFormatter(std::string pattern =
        "{timestamp} [{level}] [{thread}] {message}  ({file}:{line})")
        : pattern_(std::move(pattern)) {}

    std::string format(const LogRecord& record) override {
        auto time = std::chrono::system_clock::to_time_t(record.timestamp);
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            record.timestamp.time_since_epoch()) % 1'000'000;

        auto tm = local_time(time);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setw(6) << std::setfill('0') << us.count()
            << std::put_time(&tm, "%z");

        std::string result = pattern_;
        replace_all(result, "{timestamp}", oss.str());
        replace_all(result, "{level}",     std::string(to_string(record.level)));
        replace_all(result, "{thread}",    thread_id_to_string(record.thread_id));
        replace_all(result, "{message}",   record.message);
        replace_all(result, "{file}",      record.source.file_name());
        replace_all(result, "{line}",      std::to_string(record.source.line()));
        replace_all(result, "{function}",  record.source.function_name());
        return result;
    }

private:
    static void replace_all(std::string& str,
                            const std::string& from,
                            const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
};

/* ---------- JsonFormatter ---------- */
class JsonFormatter : public ILogFormatter {
public:
    std::string format(const LogRecord& record) override {
        auto time = std::chrono::system_clock::to_time_t(record.timestamp);
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            record.timestamp.time_since_epoch()) % 1'000'000;

        auto tm = local_time(time);

        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setw(6) << std::setfill('0') << us.count();

        std::ostringstream json;
        json << '{';
        json << "\"timestamp\":\"" << ts.str() << "\",";
        json << "\"level\":\""     << to_string(record.level) << "\",";
        json << "\"thread\":\""    << thread_id_to_string(record.thread_id) << "\",";
        json << "\"message\":\""   << escape_json(record.message) << "\",";
        json << "\"file\":\""      << escape_json(record.source.file_name()) << "\",";
        json << "\"line\":"        << record.source.line() << ',';
        json << "\"function\":\""  << escape_json(record.source.function_name()) << '"';
        json << '}';
        return json.str();
    }

private:
    static std::string escape_json(const std::string& s) {
        std::string o;
        o.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '\"': o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\b': o += "\\b";  break;
                case '\f': o += "\\f";  break;
                case '\n': o += "\\n";  break;
                case '\r': o += "\\r";  break;
                case '\t': o += "\\t";  break;
                default:   o += c;
            }
        }
        return o;
    }
};
