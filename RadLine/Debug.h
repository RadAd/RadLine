#pragma once

#include <sal.h>

void DebugOut(_In_ _Printf_format_string_ _Printf_format_string_params_(0) const char* format, ...);
void DebugOut(_In_ _Printf_format_string_ _Printf_format_string_params_(0) const wchar_t* format, ...);
