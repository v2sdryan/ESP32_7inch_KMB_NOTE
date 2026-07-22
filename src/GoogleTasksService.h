#pragma once

#include "ConfigManager.h"
#include "GoogleTaskModel.h"

#include <string>
#include <vector>

enum class GoogleTasksStatus
{
    Disabled,
    Ready,
    Error,
};

void GoogleTasks_Begin();
bool GoogleTasks_Refresh(const Config &config);
GoogleTasksStatus GoogleTasks_GetStatus();
std::vector<GoogleTaskItem> GoogleTasks_GetForDate(const std::string &yyyyMmDd);
std::string GoogleTasks_GetError();
