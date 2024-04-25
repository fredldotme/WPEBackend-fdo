#pragma once

#include <cstdio>

#define TRACE(...) { printf("%s: ", __PRETTY_FUNCTION__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); }
#define HYBRIS_ERROR_LOG TRACE
