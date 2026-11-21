#include <Arduino.h>
#include <roo_collections_immutable_flat_small_hash_map.h>

namespace {

constexpr roo_collections::ImmutableFlatSmallHashMap kCommandIds(
    roo_collections::WithStringMapping<int>(
        {{"start", 1}, {"stop", 2}, {"reset", 3}}));

static_assert(kCommandIds.at("start") == 1, "command table is constexpr");

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("Enter start, stop, or reset followed by a newline.");
}

void loop() {
  if (Serial.available() <= 0) return;

  String command = Serial.readStringUntil('\n');
  command.trim();
  const auto match = kCommandIds.find(command);
  if (match == kCommandIds.end()) {
    Serial.println("Unknown command");
  } else {
    Serial.println(match->second);
  }
}
