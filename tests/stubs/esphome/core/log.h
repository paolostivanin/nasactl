#pragma once
#include <cstdio>
#include <string>
#include <vector>
namespace esphome::test {
inline std::vector<std::string> logs;
template<typename... Args>
void log(const char *level, const char *tag, const char *format, Args... args) {
  char buffer[4096];
  if constexpr (sizeof...(Args) == 0)
    snprintf(buffer, sizeof(buffer), "%s", format);
  else
    snprintf(buffer, sizeof(buffer), format, args...);
  logs.push_back(std::string(level) + " " + tag + " " + buffer);
}
}
#define ESP_LOGD(...) ::esphome::test::log("D", __VA_ARGS__)
#define ESP_LOGI(...) ::esphome::test::log("I", __VA_ARGS__)
#define ESP_LOGW(...) ::esphome::test::log("W", __VA_ARGS__)
#define ESP_LOGV(...) ::esphome::test::log("V", __VA_ARGS__)
