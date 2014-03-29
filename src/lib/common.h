//File: common.h
//Date: Sat Mar 29 01:36:05 2014 +0800
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once

#define REP(x, y) for (auto x = decltype(y){0}; x < (y); x ++)
#define REPL(x, y, z) for (auto x = decltype(z){y}; x < (z); x ++)
#define REPD(x, y, z) for (auto x = decltype(z){y}; x >= (z); x --)
#define FOR_ITR(x, y) for (auto x = (y).begin(); x != (y).end(); x ++)

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif
