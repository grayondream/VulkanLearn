#pragma once

#include <iostream>
#include <format>
#include <string>

namespace sys{
    enum class LogLevel : int32_t{
        Verbos,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    class Logcat{
    public:
        void setFile(const std::string &file = {}){
            _logFile = file;
        }

        void setLevel(const LogLevel level = LogLevel::Verbos){
            _level = level;
        }

        template<typename ...Args>
        void log(const LogLevel level, std::format_string<Args...> fmt, Args&& ...args) {
            if(_level < level){
                return;
            }

            if(_logFile.empty()){
                std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
            }
            //TODO: write log message into file
        }

        static Logcat& instance(){
            static Logcat inst;
            return inst;
        }
    public:
        LogLevel _level{ LogLevel::Debug };
        std::string _logFile;
    };

}

inline void LOGSETFILE(const std::string &file){
    return sys::Logcat::instance().setFile(file);
}

inline void LOGSETLEVEL(const sys::LogLevel level){
    return sys::Logcat::instance().setLevel(level);
}

template<typename ...Args>
void LOGV(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Verbos, fmt, std::forward<Args>(args)...);
}

template<typename ...Args>
void LOGD(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Debug, fmt, std::forward<Args>(args)...);
}

template<typename ...Args>
void LOGI(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Info, fmt, std::forward<Args>(args)...);
}

template<typename ...Args>
void LOGW(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Warn, fmt, std::forward<Args>(args)...);
}

template<typename ...Args>
void LOGE(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Error, fmt, std::forward<Args>(args)...);
}

template<typename ...Args>
void LOGF(std::format_string<Args...> fmt, Args&& ...args){
    return sys::Logcat::instance().log(sys::LogLevel::Fatal, fmt, std::forward<Args>(args)...);
}
