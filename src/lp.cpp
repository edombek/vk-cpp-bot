#include "lp.h"
#include "common.h"
#include "workers.h"
#include <iostream>

using namespace std;

Lp::Lp()
    : vk(this->net)
{
    this->group_id = this->vk.send("groups.getById")["response"][0]["id"];
    this->getServer();
}

void Lp::getServer()
{
    json resp = this->vk.send("groups.getLongPollServer", { { "group_id", to_string(this->group_id) } })["response"];
    this->key = resp["key"];
    this->ts = resp["ts"];
    this->server = resp["server"];
}

void Lp::loop()
{
    Workers workers(wCount);
    while (true) {
		if (workers.stopped)
			return;
        string buff = this->net.send(this->server, { { "act", "a_check" }, { "key", this->key }, { "ts", this->ts }, { "wait", "25" } });
        while (buff == "")
            string buff = this->net.send(this->server, { { "act", "a_check" }, { "key", this->key }, { "ts", this->ts }, { "wait", "25" } });
        json resp = json::parse(buff);
        if (resp["failed"].is_number()) {
            switch (resp["failed"].get<int>()) {
            case 1:
                this->ts = resp["ts"];
                break;
            default:
                this->getServer();
                break;
            }
        } else {
            this->ts = resp["ts"];
            for (json update : resp["updates"]) {
                workers.add_event(update);
            }
        }
    }
}
