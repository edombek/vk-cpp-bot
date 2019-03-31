#include "timer.h"

using namespace std;

timer::timer()
{
	begin = std::chrono::system_clock::now();
}

uintmax_t timer::getWorked()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin).count();
}

timer::~timer()
{
}

time_t start;
void timer::init()
{
	start = time(NULL);
}

string getTime(time_t tp)
{
	string str = ctime(&tp);
	str.resize(str.size() - 1);
	return str;
}

string timer::getWorkTime()
{
	string str;
	time_t t = time(NULL) - start;

	str = to_string(t % 60) + "s";
	t = (t - t % 60) / 60;

	str = to_string(t % 60) + "m:" + str;
	t = (t - t % 60) / 60;

	str = to_string(t % 24) + "h:" + str;
	t = (t - t % 24) / 24;

	str = to_string(t) + "d:" + str;

	return str;
}