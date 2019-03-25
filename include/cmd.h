#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED

#include "common.h"
#include "events.h"

#define cmdHead Event &eventIn, Event &eventOut
namespace cmd {
bool start(std::string comand, Event& eventIn, Event& eventOut);
}

#endif // CMD_H_INCLUDED
