#include "Storage.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace
{
constexpr const char *kMountPath = "/littlefs";
constexpr const char *kPartitionLabel = "storage";
bool s_mounted = false;
}

bool Storage_Begin()
{
  if (s_mounted) return true;

  // Never format automatically: this partition contains the user's private
  // Wi-Fi and service settings. A mount error should preserve those bytes for
  // diagnosis and recovery.
  s_mounted = LittleFS.begin(false, kMountPath, 10, kPartitionLabel);
  if (s_mounted)
  {
    printf("LittleFS mounted: label=%s, used=%u, total=%u\n",
           kPartitionLabel,
           static_cast<unsigned>(LittleFS.usedBytes()),
           static_cast<unsigned>(LittleFS.totalBytes()));
  }
  else
  {
    printf("LittleFS mount failed: label=%s (filesystem preserved)\n",
           kPartitionLabel);
  }
  return s_mounted;
}

bool Storage_IsMounted()
{
  return s_mounted;
}
