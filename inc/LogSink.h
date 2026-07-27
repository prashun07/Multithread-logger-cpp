// sink means consumer or final destination. Major logger frameworks use sink as the name convention
// alternatively, LogSink can be renamed as LogWriter,etc
#pragma once
#include "LogRecord.h"
#include "LogFormatter.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <system_error>
#include <stdexcept>


class ILogSink { // Pure Virtual function or Abstract class
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
    virtual void set_formatter(std::unique_ptr<ILogFormatter> formatter) = 0;
};

/* ---------- ConsoleSink ---------- */
class ConsoleSink : public ILogSink {
    mutable std::mutex mtx_;
    std::unique_ptr<ILogFormatter> formatter_;
    std::ostream& out_;
    bool use_color_;

public:
    explicit ConsoleSink(std::ostream& out = std::cout, bool color = true)
        : formatter_(std::make_unique<TextFormatter>()), out_(out), use_color_(color) {}

    void set_formatter(std::unique_ptr<ILogFormatter> formatter) override {
        std::lock_guard lock(mtx_);
        formatter_ = std::move(formatter);
    }

    void write(const LogRecord& record) override {
        auto formatted = formatter_->format(record);          // format outside lock (Why?)
        std::lock_guard lock(mtx_);
        if (use_color_ && &out_ == &std::cout) {
            out_ << color_code(record.level) << formatted << "\033[0m\n";
        } else {
            out_ << formatted << '\n';
        }
    }

    void flush() override {
        std::lock_guard lock(mtx_);
        out_.flush();
    }

private:
    static const char* color_code(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:   return "\033[37m"; // White
            case LogLevel::Debug:   return "\033[36m"; // Cyan
            case LogLevel::Info:    return "\033[32m"; // Green
            case LogLevel::Warning: return "\033[33m"; // Yellow
            case LogLevel::Error:   return "\033[31m"; // Red
            case LogLevel::Fatal:   return "\033[35m"; // Magenta (Purple)
        }
        return "\033[0m"; // Reset to default color
    }
};

/* ---------- FileSink ---------- */
class FileSink : public ILogSink {
protected:
    mutable std::mutex mtx_;
    std::unique_ptr<ILogFormatter> formatter_;
    std::ofstream file_;
    std::filesystem::path path_;
    size_t bytes_written_ = 0;

public:
    explicit FileSink(const std::filesystem::path& path, bool truncate = false)
        : formatter_(std::make_unique<TextFormatter>()), path_(path) {
        open_file(truncate);
    }

    ~FileSink() override {
        std::lock_guard lock(mtx_);
        if (file_.is_open()) file_.close();
    }

    FileSink(const FileSink&) = delete;            // Disable copy constructor
    FileSink& operator=(const FileSink&) = delete; // Disable copy assignment operator
    FileSink(FileSink&&) = delete;                 // Disable move constructor
    FileSink& operator=(FileSink&&) = delete;      // Disable move assignment operator

    void set_formatter(std::unique_ptr<ILogFormatter> formatter) override {
        std::lock_guard lock(mtx_);
        formatter_ = std::move(formatter);
    }

    void write(const LogRecord& record) override {
        auto formatted = formatter_->format(record);
        std::lock_guard lock(mtx_);
        if (!file_.is_open()) return;
        file_ << formatted << '\n';
        bytes_written_ += formatted.size() + 1;
    }

    void flush() override {
        std::lock_guard lock(mtx_);
        if (file_.is_open()) file_.flush();
    }

    size_t bytes_written() const {
        std::lock_guard lock(mtx_);
        return bytes_written_;
    }

    const std::filesystem::path& path() const { return path_; }

protected:
    void open_file(bool truncate) {
        auto mode = std::ios::out | std::ios::app;
        if (truncate) mode = std::ios::out | std::ios::trunc;
        file_.open(path_, mode);
        if (!file_) throw std::runtime_error("Failed to open log file: " + path_.string());

        if (!truncate) {
            file_.seekp(0, std::ios::end);
            bytes_written_ = static_cast<size_t>(file_.tellp());
        } else {
            bytes_written_ = 0;
        }
    }

    void close_file() {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }

    void rename_file(const std::filesystem::path& new_path) {
        close_file();
        std::filesystem::rename(path_, new_path);
        open_file(false);
    }
};

/* ---------- RotatingFileSink ---------- */
class RotatingFileSink : public FileSink { //TODO
    size_t max_size_;
    size_t max_files_;

public:
    RotatingFileSink(const std::filesystem::path& path,
                     size_t max_size,
                     size_t max_files,
                     bool truncate = false)
        : FileSink(path, truncate), max_size_(max_size), max_files_(max_files) {}

    void write(const LogRecord& record) override {
        auto formatted = formatter_->format(record);
        std::lock_guard lock(mtx_);
        if (!file_.is_open()) return;

        if (bytes_written_ + formatted.size() + 1 > max_size_) {
            rotate();
        }

        file_ << formatted << '\n';
        bytes_written_ += formatted.size() + 1;
    }

private:
    void rotate() {
        close_file();
        std::error_code ec;

        auto oldest = path_.string() + "." + std::to_string(max_files_);
        std::filesystem::remove(oldest, ec);

        for (size_t i = max_files_ - 1; i > 0; --i) {
            auto from = path_.string() + "." + std::to_string(i);
            auto to   = path_.string() + "." + std::to_string(i + 1);
            if (std::filesystem::exists(from))
                std::filesystem::rename(from, to, ec);
        }

        auto first_backup = path_.string() + ".1";
        std::filesystem::rename(path_, first_backup, ec);

        open_file(true);
        bytes_written_ = 0;
    }
};
