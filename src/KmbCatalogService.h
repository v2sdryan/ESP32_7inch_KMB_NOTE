#pragma once

#include <Arduino.h>

#include <cstdint>
#include <string>
#include <vector>

struct KmbDirectionOption {
  std::string direction;
  int serviceType = 1;
  std::string destinationTc;
};

struct KmbStopOption {
  std::string stopId;
  std::string stopNameTc;
  int sequence = 0;
};

enum class KmbCatalogStatus {
  Idle,
  Loading,
  Ready,
  Error,
};

struct KmbCatalogSnapshot {
  KmbCatalogStatus status = KmbCatalogStatus::Idle;
  uint32_t generation = 0;
  std::string route;
  std::string direction;
  int serviceType = 1;
  std::string message;
  std::vector<KmbDirectionOption> directions;
  std::vector<KmbStopOption> stops;
};

class KmbCatalogService {
public:
  void begin();
  bool requestDirections(const std::string &route);
  bool requestStops(const std::string &route,
                    const std::string &direction,
                    int serviceType);
  KmbCatalogSnapshot snapshot() const;
  void processOneRequest();
  bool resolveInitialPresets();

private:
  enum class RequestType {
    None,
    Directions,
    Stops,
  };

  struct Request {
    RequestType type = RequestType::None;
    std::string route;
    std::string direction;
    int serviceType = 1;
  };

  void publish(KmbCatalogSnapshot next, bool processing);
  bool loadDirections(const std::string &route,
                      std::vector<KmbDirectionOption> &directions,
                      std::string &error);
  bool loadStops(const std::string &route,
                 const std::string &direction,
                 int serviceType,
                 std::vector<KmbStopOption> &stops,
                 std::string &error,
                 bool publishProgress);

  mutable SemaphoreHandle_t mutex_ = nullptr;
  Request pending_;
  KmbCatalogSnapshot snapshot_;
  bool processing_ = false;
};

extern KmbCatalogService KmbCatalog;
