#pragma once

// Mount the dashboard's LittleFS partition. The explicit partition label is
// important on Arduino-ESP32 3.x, where relying on the first matching data
// partition can fail even when the on-flash image is valid.
bool Storage_Begin();

bool Storage_IsMounted();
