#pragma once

#include "common.h"
#include "events.h"
#include "workers.h"

#define cmdHead Event &eventIn, Event &eventOut, Workers *worker
namespace cmd
{
bool start(std::string comand, cmdHead);
}
