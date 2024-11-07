#pragma once

#include "nn/util/util_snprintf.hpp"

#define PRINT(...)                                         \
	{                                                      \
		int len = nn::util::SNPrintf(buf, sizeof(buf), __VA_ARGS__); \
		svcOutputDebugString(buf, len);                    \
	}        
    