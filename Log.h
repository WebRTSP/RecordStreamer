#pragma once

#include <memory>

#include <spdlog/spdlog.h>


void InitStreamerLogger(spdlog::level::level_enum level);

const std::shared_ptr<spdlog::logger>& RecordStreamerLog();
