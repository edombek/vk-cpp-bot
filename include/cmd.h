#pragma once

#include "common.h"
#include "events.h"
#include "worker.h"

#define cmdHead Event &eventIn, Event &eventOut, Worker *worker
namespace cmd
{
bool start(std::string comand, cmdHead);
}
