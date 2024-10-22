#include "timer.hpp"

#include <stdint.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static LARGE_INTEGER _start;

void timerBegin() {
	QueryPerformanceCounter(&_start);
}
float timerEnd() {
	LARGE_INTEGER _end;
	LARGE_INTEGER _freq;
	QueryPerformanceCounter(&_end);
	QueryPerformanceFrequency(&_freq);
	uint64_t elapsedMicrosec = ((_end.QuadPart - _start.QuadPart) * 1000000LL) / _freq.QuadPart;
	double sec = (float)elapsedMicrosec * .000001f;
	//double ms = (float)elapsedMicrosec * .001f;
	return sec;
}
