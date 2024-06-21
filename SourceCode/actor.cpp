#include "actor.h"

std::unordered_map<std::string, std::shared_ptr<actor>> actor::_actors;
std::mutex actor::_mutex;
