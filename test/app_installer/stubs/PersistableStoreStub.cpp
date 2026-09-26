#include "PersistableStore.h"

bool PersistableStoreBase::writeDocToFile(const char*, const JsonDocument&) {
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char*, JsonDocument&) {
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst, bool&) {
  return "";
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst, bool&, size_t, bool&) {
  return "";
}
