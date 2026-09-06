#pragma once
namespace chipper::buildInfo
{
const char* label() noexcept;
const char* builtAtUtc() noexcept;
const char* gitState() noexcept;
}
