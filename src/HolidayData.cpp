#include "HolidayData.h"
#include <FS.h>
#include <LittleFS.h> // ← Changed to LittleFS
#include <ArduinoJson.h>
#include "Storage.h"

JsonDocument holidayDoc;

void Holiday_Init()
{
  if (!Storage_Begin())
  {
    printf("Holiday feature disabled because storage is unavailable.\n");
    return;
  }

  File file = LittleFS.open("/holiday.json", "r");
  if (!file)
  {
    printf("Failed to open /holiday.json\n");
    return;
  }

  DeserializationError error = deserializeJson(holidayDoc, file);
  file.close();

  if (error)
  {
    printf("Failed to parse holiday.json\n");
  }
  else
  {
    printf("Holiday list loaded successfully (%d holidays)\n",
           (int)holidayDoc["holidays"].size());
  }
}
