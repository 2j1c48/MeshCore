#pragma once

#include "MyMesh.h"

// Optional persistent Room history for boards with an SD card. The class is
// deliberately independent of normal MeshCore storage: SPIFFS continues to
// hold identity/configuration, while this layer only mirrors Room posts.
class RoomHistoryStore {
public:
  RoomHistoryStore() : _ready(false), _last_seen_next_idx(0), _stored_records(0) {}

  bool begin(MyMesh& mesh);
  void captureNewPosts(MyMesh& mesh);

  bool isReady() const { return _ready; }
  uint32_t getStoredRecordCount() const { return _stored_records; }

private:
  bool _ready;
  int _last_seen_next_idx;
  uint32_t _stored_records;

#if defined(TBEAM_1W) && defined(HAS_SDCARD)
  bool appendPost(const PostInfo& post);
  uint32_t restoreRecentPosts(MyMesh& mesh);
#endif
};
