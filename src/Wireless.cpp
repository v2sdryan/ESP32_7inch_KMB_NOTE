#include "Wireless.h"
#include "ConfigManager.h"
#include "PortalIdentity.h"
#include "WebPortal.h"
#include <WiFi.h>

void WiFi_Connect()
{
  const Config &cfg = ConfigMgr.getConfig();

  printf("Connecting using %u configured WiFi network(s)...\n",
         (unsigned)(cfg.wifi_networks.empty() ? 1 : cfg.wifi_networks.size()));

  // Try credentials in the user-selected order. WiFiMulti chooses the
  // strongest RSSI and therefore cannot implement a preferred network.
  WiFi.mode(WIFI_STA);
  const auto tryNetwork = [](const WiFiCredential &network) {
    if (network.ssid.isEmpty()) return false;
    printf("Trying WiFi: %s\n", network.ssid.c_str());
    WiFi.disconnect();
    delay(100);
    WiFi.begin(network.ssid.c_str(), network.password.c_str());
    const uint32_t deadline = millis() + 10000;
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<int32_t>(millis() - deadline) < 0)
    {
      delay(250);
      printf(".");
    }
    return WiFi.status() == WL_CONNECTED;
  };

  if (cfg.wifi_networks.empty())
    tryNetwork(WiFiCredential{cfg.wifi_ssid, cfg.wifi_pass});
  else
    for (const WiFiCredential &network : cfg.wifi_networks)
      if (tryNetwork(network)) break;

  if (WiFi.status() == WL_CONNECTED)
  {
    printf("\nWiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    printf("\nWiFi connection failed → Entering AP Mode\n");
    StartAPMode();
  }
}

bool WiFi_IsConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

void StartAPMode()
{
  const Config &cfg = ConfigMgr.getConfig();

  // Force AP mode and stop any STA attempts
  WiFi.disconnect(true);        // Disconnect STA
  WiFi.mode(WIFI_AP);           // Explicitly set AP mode

  WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_pass.c_str());

  printf("AP Mode Started → SSID: %s | Password: %s | IP: %s\n",
         cfg.ap_ssid.c_str(),
         cfg.ap_pass.c_str(),
         WiFi.softAPIP().toString().c_str());
}

String GetWiFiInfoString()
{
  char info[90] = "";
  const Config &cfg = ConfigMgr.getConfig();

  if (WiFi.getMode() == WIFI_AP)
  {
    // AP Mode
    snprintf(info, sizeof(info), "AP:%s P:%s IP:%s",
             WiFi.softAPSSID().c_str(),
             cfg.ap_pass.c_str(),
             WiFi.softAPIP().toString().c_str());
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    // Station (normal WiFi) Mode
    snprintf(info, sizeof(info), "WiFi:%s IP:%s",
             WiFi.SSID().c_str(),
             WiFi.localIP().toString().c_str());
  }
  else
  {
    strcpy(info, "WiFi Not Connected");
  }

  // Limit to ~32 UTF-8 characters for Nextion display
  if (strlen(info) > 90)
    info[90] = '\0';

  return String(info);
}

// Cycle-friendly form: short fragments that each fit in a static label.
// Each call returns one frame; the display layer rotates the index every few
// seconds so the user sees all of them in sequence — no scrolling animation
// (which previously caused PSRAM-bandwidth contention with the LCD EDMA).
int GetWiFiInfoFrameCount()
{
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) return 3;
  if (WiFi.status() == WL_CONNECTED)
    return WebPortal_MdnsActive() ? 3 : 2;
  return 1;
}

void GetWiFiInfoFrame(int frameIndex, char *out, size_t outSize)
{
  if (!out || outSize == 0) return;

  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
  {
    const Config &cfg = ConfigMgr.getConfig();
    switch (((frameIndex % 3) + 3) % 3)
    {
      case 0: snprintf(out, outSize, "AP: %s",            WiFi.softAPSSID().c_str());          break;
      case 1: snprintf(out, outSize, "Pwd: %s",           cfg.ap_pass.c_str());                break;
      case 2: snprintf(out, outSize, "Setup: http://%s",  WiFi.softAPIP().toString().c_str()); break;
    }
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    if (!WebPortal_MdnsActive())
    {
      switch (((frameIndex % 2) + 2) % 2)
      {
        case 0: snprintf(out, outSize, "WiFi: %s",      WiFi.SSID().c_str());               break;
        case 1: snprintf(out, outSize, "IP: http://%s", WiFi.localIP().toString().c_str()); break;
      }
    }
    else
    {
      switch (((frameIndex % 3) + 3) % 3)
      {
        case 0: snprintf(out, outSize, "WiFi: %s",      WiFi.SSID().c_str());               break;
        case 1: snprintf(out, outSize, "Web: %s",       PortalIdentity::kLocalHost);         break;
        case 2: snprintf(out, outSize, "IP: http://%s", WiFi.localIP().toString().c_str()); break;
      }
    }
  }
  else
  {
    snprintf(out, outSize, "WiFi Not Connected");
  }
}
