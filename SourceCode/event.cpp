#include "event.h"

std::unordered_multimap<std::string, std::function<void(const arguments&)>> event::_handlers;
std::mutex event::_mutex;
