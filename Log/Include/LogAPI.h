#ifndef LOG_API_H
#define LOG_API_H

// Windows
#if defined(_WIN32) || defined(_WIN64)
#ifdef LOG_EXPORTS
#define LOG_API __declspec(dllexport)
#else
#define LOG_API __declspec(dllimport)
#endif
// Linux/Unix
#elif defined(__GNUC__) || defined(__clang__)
#ifdef LOG_EXPORTS
#define LOG_API __attribute__((visibility("default")))
#else
#define LOG_API
#endif
// MacOS
#elif defined(__APPLE__)
#ifdef LOG_EXPORTS
#define LOG_API __attribute__((visibility("default")))
#else
#define LOG_API
#endif
#else
#define LOG_API
#endif

#endif // LOG_API_H
