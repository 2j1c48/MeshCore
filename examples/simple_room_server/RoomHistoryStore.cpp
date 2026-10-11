#include "RoomHistoryStore.h"

#if defined(TBEAM_1W) && defined(HAS_SDCARD)

#include <SD.h>
#include <SPI.h>
#include <target.h>

namespace {
constexpr const char* ROOM_HISTORY_FILE = "/ka0s_room_history.bin";
constexpr uint32_t ROOM_HISTORY_MAGIC = 0x3150524B;  // "KRP1" on disk
constexpr size_t ROOM_HISTORY_TEXT_BYTES = MAX_POST_TEXT_LEN + 1;
constexpr size_t ROOM_HISTORY_RECORD_SIZE =
    sizeof(uint32_t) + sizeof(uint32_t) + PUB_KEY_SIZE + sizeof(uint16_t) + ROOM_HISTORY_TEXT_BYTES;

bool readExact(File& f, void* dst, size_t len) {
  return f.read(static_cast<uint8_t*>(dst), len) == len;
}

bool writeExact(File& f, const void* src, size_t len) {
  return f.write(static_cast<const uint8_t*>(src), len) == len;
}
}  // namespace

bool RoomHistoryStore::begin(MyMesh& mesh) {
  _ready = false;
  _stored_records = 0;

  // Keep both chip selects inactive while the SD stack claims the same SPI bus
  // that the SX1262 already uses.
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH);

  if (!SD.begin(SDCARD_CS, tbeam_shared_spi, 4000000)) {
    Serial.println("KA0S Room SD history: no card or mount failed; using RAM only");
    _last_seen_next_idx = mesh.getNextPostIndex();
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("KA0S Room SD history: card not detected; using RAM only");
    SD.end();
    _last_seen_next_idx = mesh.getNextPostIndex();
    return false;
  }

  uint32_t restored = restoreRecentPosts(mesh);
  _last_seen_next_idx = mesh.getNextPostIndex();
  _ready = true;

  Serial.printf("KA0S Room SD history: ready, restored %lu post(s), archive has %lu record(s)\n",
                static_cast<unsigned long>(restored),
                static_cast<unsigned long>(_stored_records));
  return true;
}

bool RoomHistoryStore::appendPost(const PostInfo& post) {
  File f = SD.open(ROOM_HISTORY_FILE, FILE_APPEND);
  if (!f) return false;

  const uint16_t text_len = static_cast<uint16_t>(strnlen(post.text, MAX_POST_TEXT_LEN));
  char text_block[ROOM_HISTORY_TEXT_BYTES];
  memset(text_block, 0, sizeof(text_block));
  memcpy(text_block, post.text, text_len);

  bool ok = writeExact(f, &ROOM_HISTORY_MAGIC, sizeof(ROOM_HISTORY_MAGIC)) &&
            writeExact(f, &post.post_timestamp, sizeof(post.post_timestamp)) &&
            writeExact(f, post.author.pub_key, PUB_KEY_SIZE) &&
            writeExact(f, &text_len, sizeof(text_len)) &&
            writeExact(f, text_block, sizeof(text_block));

  f.close();
  if (ok) _stored_records++;
  return ok;
}

uint32_t RoomHistoryStore::restoreRecentPosts(MyMesh& mesh) {
  File f = SD.open(ROOM_HISTORY_FILE, FILE_READ);
  if (!f) {
    _stored_records = 0;
    return 0;
  }

  const size_t file_size = f.size();
  const size_t complete_records = file_size / ROOM_HISTORY_RECORD_SIZE;
  _stored_records = static_cast<uint32_t>(complete_records);

  const size_t records_to_restore = min(static_cast<size_t>(MAX_UNSYNCED_POSTS), complete_records);
  const size_t first_record = complete_records - records_to_restore;

  if (!f.seek(first_record * ROOM_HISTORY_RECORD_SIZE)) {
    f.close();
    return 0;
  }

  uint32_t restored = 0;
  for (size_t i = 0; i < records_to_restore; ++i) {
    uint32_t magic = 0;
    uint32_t timestamp = 0;
    uint8_t author[PUB_KEY_SIZE];
    uint16_t text_len = 0;
    char text_block[ROOM_HISTORY_TEXT_BYTES];

    if (!readExact(f, &magic, sizeof(magic)) ||
        !readExact(f, &timestamp, sizeof(timestamp)) ||
        !readExact(f, author, sizeof(author)) ||
        !readExact(f, &text_len, sizeof(text_len)) ||
        !readExact(f, text_block, sizeof(text_block))) {
      break;
    }

    if (magic != ROOM_HISTORY_MAGIC || timestamp == 0 || text_len > MAX_POST_TEXT_LEN) {
      continue;
    }

    PostInfo post;
    memcpy(post.author.pub_key, author, PUB_KEY_SIZE);
    post.post_timestamp = timestamp;
    memset(post.text, 0, sizeof(post.text));
    memcpy(post.text, text_block, text_len);
    post.text[text_len] = 0;

    mesh.restorePost(post);
    restored++;
  }

  f.close();
  return restored;
}

void RoomHistoryStore::captureNewPosts(MyMesh& mesh) {
  if (!_ready) return;

  const int current_next_idx = mesh.getNextPostIndex();
  while (_last_seen_next_idx != current_next_idx) {
    PostInfo post;
    if (mesh.copyPostAt(_last_seen_next_idx, post) && post.post_timestamp != 0) {
      if (!appendPost(post)) {
        Serial.println("KA0S Room SD history: write failed; falling back to RAM only");
        _ready = false;
        return;
      }
    }
    _last_seen_next_idx = (_last_seen_next_idx + 1) % MAX_UNSYNCED_POSTS;
  }
}

#else

bool RoomHistoryStore::begin(MyMesh& mesh) {
  _last_seen_next_idx = mesh.getNextPostIndex();
  return false;
}

void RoomHistoryStore::captureNewPosts(MyMesh& mesh) {
  (void)mesh;
}

#endif
