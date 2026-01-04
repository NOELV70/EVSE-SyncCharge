/*******************************************************************************
 * @file EvseLogger.h
 * @brief Lightweight structured logging helper for the EVSE project.
 *
 * @details
 * Provides `EvseLogger` class with leveled logging and printf-style helpers.
 * The project exposes a global `logger` instance from the corresponding
 * implementation file.
 *
 * @copyright (C) Noel Vellemans 2026
 * @license MIT
 * @version 1.0.0
 * @date 2026-01-02
 ******************************************************************************/

#ifndef EVSE_LOGGER_H
#define EVSE_LOGGER_H

#include <Arduino.h>
#include <cstdarg>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class EvseLogger {
private:
    Stream* output;  // Can be Serial, Telnet, or something else

    // Helper method to handle formatting for all *f methods
    void logfImpl(LogLevel level, const char* fmt, va_list args) {
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        log(level, buffer);
    }

public:
    EvseLogger(Stream& out = Serial) : output(&out) {}

    void log(LogLevel level, const char* msg) {
        unsigned long t = micros();
        unsigned long s = t / 1000000UL;
        unsigned long us = t % 1000000UL;
        char ts[32];
        snprintf(ts, sizeof(ts), "[%lu.%06lu] ", s, us);
        output->print(ts);

        const char* prefix = "";
        switch (level) {
            case LOG_DEBUG: prefix = "[DEBUG] "; break;
            case LOG_INFO:  prefix = "[INFO ] "; break;
            case LOG_WARN:  prefix = "[WARN ] "; break;
            case LOG_ERROR: prefix = "[ERROR] "; break;
        }
        output->print(prefix);
        output->println(msg);
    }

    void logf(LogLevel level, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        logfImpl(level, fmt, args);
        va_end(args);
    }

    // Convenience methods
    void debug(const char* msg) { log(LOG_DEBUG, msg); }
    void info(const char* msg) { log(LOG_INFO, msg); }
    void warn(const char* msg) { log(LOG_WARN, msg); }
    void error(const char* msg) { log(LOG_ERROR, msg); }

    void debugf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        logfImpl(LOG_DEBUG, fmt, args);
        va_end(args);
    }

    void infof(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        logfImpl(LOG_INFO, fmt, args);
        va_end(args);
    }

    void warnf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        logfImpl(LOG_WARN, fmt, args);
        va_end(args);
    }

    void errorf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        logfImpl(LOG_ERROR, fmt, args);
        va_end(args);
    }

    // Redirect output to different stream
    void setOutput(Stream& out) {
        output = &out;
    }
};

extern EvseLogger logger;

#endif // EVSE_LOGGER_H
