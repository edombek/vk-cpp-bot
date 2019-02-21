#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED

#include "common.h"
#include "events.h"

namespace cmd {
bool start(std::string comand, Event* eventIn, Event* eventOut);
}

#endif // CMD_H_INCLUDED
