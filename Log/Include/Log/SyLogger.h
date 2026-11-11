#pragma once

// 临时兼容头（待删除）
// 旧代码仍使用 SY_* 日志宏，桥接到 APR_* 新宏。
// 所有文件迁移完成后删除本文件，统一改用 Log/AprLogger.h。
#include "Log/AprLogger.h"

#define SY_DEBUGF APR_DEBUGF
#define SY_INFOF  APR_INFOF
#define SY_WARNF  APR_WARNF
#define SY_ERRORF APR_ERRORF
