#include "BuildInfo.h"
#include "ChipperBuildInfo.h"
namespace chipper::buildInfo
{
const char* label() noexcept { return build::label; }
const char* builtAtUtc() noexcept { return build::builtAtUtc; }
const char* gitState() noexcept { return build::gitState; }
}
