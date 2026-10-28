#include "TriviaActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;

namespace {

constexpr const char* kDir = "/trivia";
constexpr const char* kPackPath = "/trivia/pack.dat";
constexpr const char* kPartPath = "/trivia/pack.dat.part";
// A rolling PRERELEASE, so the OTA's releases/latest can never see it and a
// 6MB question pack never lands in a firmware update. Same arrangement as the
// xkcd archive; see docs/apps/trivia-pack-format.md.
constexpr const char* kPackUrl = "https://github.com/ma-r-s/crossplay/releases/download/trivia-pack/pack.dat";
constexpr const char* kStatePath = "/trivia/pack.state";
// Published in the SAME release upload as pack.dat, so the two cannot skew.
// Fetched straight from GitHub rather than through a service: the device is not
// a browser, so the CORS problem that put api/firmware.js on the site does not
// apply to it, and a freshness check needing no service beats one that does.
constexpr const char* kManifestUrl = "https://github.com/ma-r-s/crossplay/releases/download/trivia-pack/pack.json";
// Metadata identifies the downloaded pack when checking for updates.
constexpr const char* kMetaPath = "/trivia/pack.meta";
// The existing first byte is difficulty; an optional second byte stores the
// Play-only US question preference. Older one-byte files keep their meaning.
constexpr const char* kPrefsPath = "/trivia/prefs";

// A ByteSource over a HalFile. Every read seeks first: an index entry and the
// record it points at are in different places, so sequential reads are the
// exception rather than the rule.
class FileSource final : public trivia::WritableByteSource {
 public:
  void attach(HalFile& file) {
    file_ = &file;
    size_ = static_cast<uint32_t>(file.size());
  }
  bool read(const uint32_t offset, void* dst, const uint32_t length) override {
    if (length == 0) return true;
    if (file_ == nullptr || offset > size_ || length > size_ - offset) return false;
    if (!file_->seekSet(offset)) return false;
    return file_->read(dst, length) == static_cast<int>(length);
  }
  bool write(const uint32_t offset, const void* src, const uint32_t length) override {
    if (length == 0) return true;
    if (file_ == nullptr) return false;
    if (offset > size_ || length > size_ - offset) return false;
    if (!file_->seekSet(offset)) return false;
    if (file_->write(static_cast<const uint8_t*>(src), length) != length) return false;
    return true;
  }
  bool flush() override {
    // HalFile::flush has no return value; read-back validation happens at open.
    if (file_ == nullptr) return false;
    file_->flush();
    return true;
  }
  uint32_t size() const override { return size_; }

 private:
  HalFile* file_ = nullptr;
  uint32_t size_ = 0;
};

HalFile g_packFile;
HalFile g_stateFile;
FileSource g_packSource;
FileSource g_stateSource;

}  // namespace

std::unique_ptr<Activity> TriviaActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TriviaActivity>(renderer, mappedInput);
}

bool TriviaActivity::ensureState(const uint32_t count) {
  g_stateFile = Storage.open(kStatePath, O_RDWR);
  if (!g_stateFile.isOpen() || static_cast<uint32_t>(g_stateFile.size()) != count) {
    // Missing, or a DIFFERENT length from the pack because the pack was
    // replaced under it. Rewriting loses which questions have been seen, which
    // is the right trade: reading a stale byte for a question it does not
    // describe is worse.
    //
    // `!=`, not `<`. A pack that SHRANK left a longer state file, size() >= count
    // held, and every byte was reused against a pack whose record order had
    // changed -- so SEEN bits deprioritised arbitrary questions and, worse,
    // FLAGGED bits made arbitrary questions permanently unservable with no way
    // for the player to clear them. Both this call site and PackState::open
    // check the length; the guard is at the boundary as well as at the caller
    // because this is the only caller today and will not be the last.
    HalFile fresh;
    if (!Storage.openFileForWrite("TRIVIA", kStatePath, fresh)) {
      LOG_ERR("TRIVIA", "Cannot create %s", kStatePath);
      return false;
    }
    uint8_t zeros[256] = {};
    for (uint32_t written = 0; written < count;) {
      const uint32_t chunk = (count - written) < sizeof(zeros) ? (count - written) : sizeof(zeros);
      if (fresh.write(zeros, chunk) != chunk) {
        LOG_ERR("TRIVIA", "Short write creating %s", kStatePath);
        return false;
      }
      written += chunk;
    }
    fresh.flush();
    fresh.close();
    g_stateFile = Storage.open(kStatePath, O_RDWR);
  }
  if (!g_stateFile.isOpen()) return false;
  g_stateSource.attach(g_stateFile);
  return state_.open(g_stateSource, count);
}

bool TriviaActivity::openPack() {
  if (!Storage.openFileForRead("TRIVIA", kPackPath, g_packFile)) {
    LOG_ERR("TRIVIA", "No pack at %s", kPackPath);
    return false;
  }
  g_packSource.attach(g_packFile);
  if (!pack_.open(g_packSource)) {
    LOG_ERR("TRIVIA", "%s is not a readable trivia pack", kPackPath);
    return false;
  }
  if (!ensureState(pack_.count())) return false;
  readPackMeta(pack_.count(), g_packSource.size());
  chooser_.begin(pack_, state_, rng_);
  return true;
}

// CrossInk Play checks published packs but does not send question reports.
// Existing CrossPlay reports.dat files are left untouched.
void TriviaActivity::readPackMeta(const uint32_t count, const uint32_t packBytes) {
  meta_ = trivia::PackMeta{};
  HalFile metaFile = Storage.open(kMetaPath, O_RDONLY);
  if (metaFile.isOpen() && metaFile.size() > 0 && metaFile.size() < 512) {
    char text[512] = {};
    const int read = metaFile.read(text, static_cast<size_t>(metaFile.size()));
    if (read > 0) meta_ = trivia::parseMeta(text, static_cast<size_t>(read), count, packBytes);
  }
}

// One line for the SETTINGS row, saying what the card holds. Written here
// rather than in the screen because only the activity knows the pack.
void TriviaActivity::describePack(char* out, const size_t capacity) const {
  if (!pack_.isOpen()) {
    std::snprintf(out, capacity, "%s", "NO PACK ON THIS CARD YET");
    return;
  }
  if (!meta_.valid) {
    if (pack_.count() == 1) {
      std::snprintf(out, capacity, "1 QUESTION. BUILD UNKNOWN UNTIL YOU SYNC");
    } else {
      std::snprintf(out, capacity, "%u QUESTIONS. BUILD UNKNOWN UNTIL YOU SYNC", static_cast<unsigned>(pack_.count()));
    }
    return;
  }
  // Singular spelled out, like questionCount() in TriviaScreens.cpp -- whose own
  // comment says the one-question seed pack is exactly the case that exposes
  // this, and which is the screenshot this line was caught in.
  if (pack_.count() == 1) {
    std::snprintf(out, capacity, "1 QUESTION, BUILD %s", meta_.id);
  } else {
    std::snprintf(out, capacity, "%u QUESTIONS, BUILD %s", static_cast<unsigned>(pack_.count()), meta_.id);
  }
}

namespace {

void loadPreferences(int& difficulty, bool& showUsCentric) {
  difficulty = 0;
  showUsCentric = false;
  HalFile f = Storage.open(kPrefsPath, O_RDONLY);
  if (!f.isOpen()) return;
  uint8_t bytes[2] = {};
  const int read = f.read(bytes, sizeof(bytes));
  if (read > 0 && bytes[0] <= trivia::kDifficulties) difficulty = bytes[0];
  if (read > 1) showUsCentric = bytes[1] == 1;
}

void savePreferences(const int difficulty, const bool showUsCentric) {
  HalFile f;
  if (!Storage.openFileForWrite("TRIVIA", kPrefsPath, f)) return;
  const uint8_t bytes[] = {static_cast<uint8_t>(difficulty), static_cast<uint8_t>(showUsCentric)};
  f.write(bytes, sizeof(bytes));
}

}  // namespace

void TriviaActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);

  rng_.seed(static_cast<uint32_t>(millis()) | 1u);
  loadPreferences(difficulty_, showUsCentric_);

  if (openPack()) {
    view_ = View::Menu;
    LOG_INF("TRIVIA", "Pack open: %u questions", static_cast<unsigned>(pack_.count()));
  } else {
    showNotice("NO QUESTIONS",
               "There is no question pack on this card yet. Connect to WiFi and fetch one; it is about 6MB.",
               "GET THE QUESTIONS", triviaui::ActionGetPack);
  }

  flashOnNextPaint_ = true;
  requestUpdate();
}

void TriviaActivity::onExit() {
  g_packFile.close();
  g_stateFile.close();
  Activity::onExit();
}

void TriviaActivity::showNotice(const char* headline, const char* body, const char* actionLabel,
                                const freeink::ui::ActionId action) {
  std::snprintf(noticeHead_, sizeof(noticeHead_), "%s", headline);
  // Guarded because the natural call is the dangerous one: reading the current
  // body, deciding to keep it, and passing it straight back makes this an
  // snprintf of a buffer onto itself, which is undefined behaviour for
  // overlapping copies. Aliasing means "leave it as it is", so skipping is
  // also the right answer semantically, not merely the safe one.
  if (body != noticeBody_) {
    std::snprintf(noticeBody_, sizeof(noticeBody_), "%s", body);
  }
  noticeAction_ = actionLabel;
  noticeActionId_ = action;
  // Cleared on EVERY notice, so the extra row cannot survive onto a screen that
  // did not ask for it. A caller that wants WHY?/UNDO sets them AFTER this
  // call; anything else gets a notice with one button, as before.
  noticeSecond_ = nullptr;
  noticeSecondId_ = 0;
  noticeThird_ = nullptr;
  noticeThirdId_ = 0;
  view_ = View::Notice;
  interactionsReady_ = false;
  flashOnNextPaint_ = true;
  requestUpdate();
}

// One asset, to a .part name, renamed only once it is whole -- so a torn
// download can never masquerade as a corrupt pack. The app simply finds no
// pack, exactly as before the attempt.
//
// Synchronous: loop() is blocked but the render task is not, so progress paints
// through requestUpdateAndWait(). The input pump in the progress callback is
// the sanctioned exception to the one-pump rule -- nothing else pumps while
// this blocks, and without it Back could not cancel a multi-minute download.
void TriviaActivity::onWifiChosen(const bool connected) {
  if (!connected) {
    // Cancelling the picker is a decision, not a failure: say what did not
    // happen and leave the button that starts it again.
    showNotice("NO WIFI", "The pack needs WiFi to download. The card is unchanged.", "TRY AGAIN",
               triviaui::ActionGetPack);
    return;
  }
  // Queued rather than run here: the download blocks for minutes, and this is
  // the result handler of the activity that is still unwinding.
  downloadQueued_ = true;
}

void TriviaActivity::runPackDownload() {
  // exists() first, because SdFat's mkdir returns FALSE for a directory that
  // is already there. Treating that as failure meant the second attempt at a
  // download could never succeed: the first one creates /trivia, and every run
  // after it reports NO ROOM on a card with gigabytes free.
  //
  // Found on hardware and invisible in the simulator, whose SD is an ordinary
  // host directory where mkdir on an existing path succeeds. Every other
  // caller in this fork already does it this way -- StudyActivity, ScreenshotUtil,
  // BookmarkFile -- and this was the one that invented its own.
  if (!Storage.exists(kDir) && !Storage.mkdir(kDir)) {
    showNotice("NO ROOM", "Could not create /trivia on the card. Is the card in, and writable?", "TRY AGAIN",
               triviaui::ActionGetPack);
    return;
  }

  // Ask the card how much room is left BEFORE writing ~6MB to it, because this
  // app is not the only thing on the card. A card filled by a trivia pack is a
  // card where Study's review log cannot be written, and that failure loses
  // answers rather than refusing -- the cost of overfilling lands on a different
  // app, silently, later.
  //
  // The decision itself lives in TriviaCore so it can be tested: this file
  // includes WiFi.h and cannot be built on the host at all.
  uint64_t freeNow = 0;
  const bool queryOk = Storage.freeBytes(freeNow);
  switch (trivia::roomFor(queryOk, freeNow, trivia::kPackFreeFloorBytes)) {
    case trivia::Room::Unknown:
      // NOT the same screen as NO ROOM: freeBytes() returns false for "could not
      // answer" and never for "full". Saying the card is full when we do not know
      // that would be the same conflation the HAL call exists to prevent.
      // The body must describe what the BUTTON does. The first version said the
      // card "may need re-seating" beside a button marked TRY AGAIN: the
      // instruction and the only available action were about different things,
      // and it sent people to fiddle with hardware for a fault that is usually
      // transient. Not asserting anything about the physical slot, which this
      // code cannot know and nobody has checked.
      showNotice("CAN'T TELL",
                 "The card did not answer when asked how much room is left, so nothing was written. "
                 "Trying again usually works.",
                 "TRY AGAIN", triviaui::ActionGetPack);
      return;
    case trivia::Room::TooSmall: {
      // Local buffer: showNotice snprintf()s body INTO noticeBody_, so passing
      // noticeBody_ as the body argument would be an overlapping self-copy.
      char body[160];
      // Says what would make the retry succeed. Reporting both numbers and then
      // offering only TRY AGAIN told the user they were stuck while implying
      // the button might help; it is the only action on screen, so the sentence
      // has to name the thing that makes it work.
      std::snprintf(body, sizeof(body),
                    "The questions need about %u MB free and the card has %u MB. "
                    "Delete something from the card, then try again. Nothing was written.",
                    static_cast<unsigned>(trivia::kPackFreeFloorBytes >> 20), static_cast<unsigned>(freeNow >> 20));
      showNotice("NO ROOM", body, "TRY AGAIN", triviaui::ActionGetPack);
      return;
    }
    case trivia::Room::Ok:
      break;
  }

  g_packFile.close();
  g_stateFile.close();
  downloadCancel_ = false;

  size_t lastPainted = 0;
  const auto progress = [this, &lastPainted](const size_t got, const size_t total) {
    // Every ~1MB: each paint is an e-ink refresh, and finer steps would spend
    // more time refreshing than downloading.
    if (got - lastPainted >= 1024u * 1024u || (total > 0 && got == total)) {
      lastPainted = got;
      if (total > 0) {
        std::snprintf(noticeBody_, sizeof(noticeBody_), "Fetching the questions: %u of %u MB. Back stops it.",
                      static_cast<unsigned>(got >> 20), static_cast<unsigned>(total >> 20));
      } else {
        std::snprintf(noticeBody_, sizeof(noticeBody_), "Fetching the questions: %u MB so far. Back stops it.",
                      static_cast<unsigned>(got >> 20));
      }
      requestUpdateAndWait();
    }
    mappedInput.update();
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) downloadCancel_ = true;
    if (mappedInput.wasHomeGesture()) downloadCancel_ = true;
  };

  std::snprintf(noticeHead_, sizeof(noticeHead_), "%s", "FETCHING");
  std::snprintf(noticeBody_, sizeof(noticeBody_), "%s", "Starting. Back stops it.");
  noticeAction_ = nullptr;
  requestUpdateAndWait();

  const auto err = HttpDownloader::downloadToFile(kPackUrl, kPartPath, progress, &downloadCancel_);
  if (err != HttpDownloader::OK) {
    Storage.remove(kPartPath);
    // Every one of these offers TRY AGAIN. A screen that reports a failure and
    // gives you nothing to press is a dead end -- the user's only way out is to
    // leave the app, and nothing on screen says so. Get Books shipped exactly
    // that and Mario found it on the device.
    if (err == HttpDownloader::ABORTED) {
      showNotice("STOPPED", "Download stopped. Nothing was kept.", "TRY AGAIN", triviaui::ActionGetPack);
    } else if (err == HttpDownloader::FILE_ERROR) {
      showNotice("CARD TROUBLE", "The card would not take the file. Nothing was kept.", "TRY AGAIN",
                 triviaui::ActionGetPack);
    } else {
      showNotice("NO ANSWER", "The download did not answer. The card is unchanged.", "TRY AGAIN",
                 triviaui::ActionGetPack);
    }
    return;
  }

  Storage.remove(kPackPath);  // a half pack from an earlier era must not block the rename
  if (!Storage.rename(kPartPath, kPackPath)) {
    showNotice("CARD TROUBLE", "Downloaded, but the card refused the final rename.", "TRY AGAIN",
               triviaui::ActionGetPack);
    return;
  }

  // The state file describes the OLD pack. openPack rewrites it when the
  // question count no longer matches, which loses which questions have been
  // seen -- the right trade against reading a stale byte for a question it
  // does not describe.
  if (!openPack()) {
    showNotice("BAD PACK", "Downloaded, but the pack did not open.", "TRY AGAIN", triviaui::ActionGetPack);
    return;
  }
  // Fetch the matching manifest while the new pack is available.
  // A failure here is not a failed download: the pack is good, only the id is
  // unknown, and the next SYNC will settle it.
  if (!adoptManifest(fetchManifest())) {
    LOG_INF("TRIVIA", "Pack downloaded but its build is unknown; SYNC will settle it");
  }
  describePack(packLine_, sizeof(packLine_));

  char body[96];
  std::snprintf(body, sizeof(body), "%u questions on the card. Ready when you are.",
                static_cast<unsigned>(pack_.count()));
  showNotice("READY", body, "PLAY", triviaui::ActionMenuRow);
  selected_ = -1;
}

trivia::PackManifest TriviaActivity::fetchManifest() {
  std::string text;
  if (!HttpDownloader::fetchUrl(kManifestUrl, text)) return trivia::PackManifest{};
  return trivia::parseManifest(text.c_str(), text.size());
}

// Store the build identifier used by later update checks.
bool TriviaActivity::adoptManifest(const trivia::PackManifest& published) {
  if (!published.valid || !pack_.isOpen()) return false;
  // A manifest with a different count or length describes a different pack.
  if (published.count != pack_.count() || published.bytes != g_packSource.size()) {
    LOG_INF("TRIVIA", "Manifest describes %u questions / %u bytes; this card has %u / %u",
            static_cast<unsigned>(published.count), static_cast<unsigned>(published.bytes),
            static_cast<unsigned>(pack_.count()), static_cast<unsigned>(g_packSource.size()));
    return false;
  }

  char text[128] = {};
  const size_t n = trivia::formatMeta(text, sizeof(text), published.id, published.count, published.bytes);
  if (n == 0) return false;
  HalFile out;
  if (!Storage.openFileForWrite("TRIVIA", kMetaPath, out)) {
    LOG_ERR("TRIVIA", "Cannot write %s", kMetaPath);
    return false;
  }
  const bool wrote = out.write(reinterpret_cast<const uint8_t*>(text), n) == n;
  out.flush();
  out.close();
  if (!wrote) return false;

  // Re-read it the same way any other launch would, so a meta this device
  // cannot read back is caught here rather than on the next open.
  readPackMeta(pack_.count(), g_packSource.size());
  LOG_INF("TRIVIA", "Pack build is %s", published.id);
  return meta_.valid;
}

// Check for a newer pack without contributing reports or device analytics.
void TriviaActivity::runSync() {
  showNotice("SYNCING", "Checking for a newer pack.", nullptr, 0);
  requestUpdateAndWait();

  const trivia::PackManifest published = fetchManifest();
  // If this card does not know its build yet, and the published manifest
  // describes exactly the pack it holds, that IS the answer -- adopt it. This
  // identifies an existing pack without downloading it again.
  if (!meta_.valid && adoptManifest(published)) {
    LOG_INF("TRIVIA", "Adopted the published build for the pack already on the card");
  }

  char body[192];
  switch (trivia::compare(meta_, published)) {
    case trivia::Freshness::Current:
      showNotice("UP TO DATE", "You have the newest questions.", "BACK", triviaui::ActionCloseSettings);
      break;
    case trivia::Freshness::Newer:
      // Says what it will cost BEFORE spending it. A sync that only spins is
      // what card #253 calls honest and useless, and this pack is minutes over
      // WiFi -- the size is the whole reason a player would rather not.
      std::snprintf(body, sizeof(body), "A newer pack is ready: %u questions, %u MB. This takes a few minutes.",
                    static_cast<unsigned>(published.count), static_cast<unsigned>(published.bytes / 1000000u));
      showNotice("NEWER PACK", body, "GET IT", triviaui::ActionGetPack);
      break;
    case trivia::Freshness::Unknown:
      // NOT "a newer pack is ready", which is a claim the device cannot make.
      // Unknown means adoptManifest refused: the published pack is a different
      // size from the one on this card, so it IS different -- but this card's
      // own build is still unidentified, and saying so is the only honest line.
      // Sharing the Newer arm made SYNC offer a download that changed nothing
      // and left the build unknown afterwards, forever.
      std::snprintf(body, sizeof(body),
                    "This card holds %u questions and a different pack is published. Fetching it will settle it.",
                    static_cast<unsigned>(pack_.count()));
      showNotice("DIFFERENT PACK", body, "GET IT", triviaui::ActionGetPack);
      break;
    case trivia::Freshness::NoManifest:
      showNotice("NOT CHECKED", "Could not check for a newer pack.", "BACK", triviaui::ActionCloseSettings);
      break;
  }
  requestUpdate();
}

void TriviaActivity::go(const View next) {
  view_ = next;
  interactionsReady_ = false;
  flashOnNextPaint_ = true;  // a mode change is a page turn, so spend the flash
  requestUpdate();
}

void TriviaActivity::deal() {
  const bool solo = view_ == View::Solo;
  revealed_ = false;
  chosen_ = -1;
  haveQuestion_ = false;

  uint32_t index = 0;
  // US-centric questions are hidden unless the player opted in from Settings.
  const bool allowUsCentric = showUsCentric_;
  if (!chooser_.next(index, solo, difficulty_, allowUsCentric)) {
    LOG_ERR("TRIVIA", "No question available (difficulty %d, solo %d, us %d)", difficulty_, solo ? 1 : 0,
            allowUsCentric ? 1 : 0);
    return;
  }
  if (!pack_.read(index, question_)) {
    LOG_ERR("TRIVIA", "Unreadable record %u", static_cast<unsigned>(index));
    return;
  }
  current_ = index;
  haveQuestion_ = true;
  state_.setFlag(index, trivia::kSeen);
  if (solo) trivia::buildChoices(question_, rng_, choices_);
  requestUpdate();
}

void TriviaActivity::routeAction(const int action, const int value) {
  switch (action) {
    case triviaui::ActionGetPack:
      // The radio first. Entering the TLS stack with WiFi never started is not
      // a failed download, it is a panic: the socket layer takes a mutex that
      // does not exist yet and FreeRTOS asserts on the null handle
      // (xQueueSemaphoreTake, queue.c:1709). The notice this button sits under
      // has always said "connect to WiFi and fetch one"; nothing did.
      WiFi.mode(WIFI_STA);
      startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                             [this](const ActivityResult& result) { onWifiChosen(!result.isCancelled); });
      break;
    case triviaui::ActionMenuRow:
      if (view_ == View::Notice) {  // the READY notice's PLAY button
        go(View::Menu);
        break;
      }
      selected_ = value;
      // Switched on the row rather than on "0, 1, or anything else". The old
      // trailing else cycled the difficulty for every value that was not 0 or
      // 1, so the SETTINGS row added beside DIFFICULTY would silently have been
      // a second difficulty control.
      //
      // And NO `default:`. A default that breaks is the same silence in a new
      // coat: the next row added to MenuRow would compile and do nothing. With
      // every enumerator listed, -Wswitch makes it a build failure instead, and
      // this tree is built -Wall -Wextra -Werror. Count is named for that
      // reason alone; it is not a row.
      switch (static_cast<triviaui::MenuRow>(value)) {
        case triviaui::MenuRow::Quizmaster:
          go(View::Quizmaster);
          deal();
          break;
        case triviaui::MenuRow::Solo:
          score_.reset();
          go(View::Solo);
          deal();
          break;
        case triviaui::MenuRow::Difficulty:
          // Stays on the front door: a per-session mood, not a preference.
          difficulty_ = (difficulty_ + 1) % (trivia::kDifficulties + 1);
          savePreferences(difficulty_, showUsCentric_);
          requestUpdate();
          break;
        case triviaui::MenuRow::Settings:
          // Rendered once here rather than on every paint: the pack does not
          // change while the screen is up, and building it per paint would put
          // an snprintf in the render path for a line that never moves.
          describePack(packLine_, sizeof(packLine_));
          go(View::Settings);
          break;
        case triviaui::MenuRow::Count:
          break;
      }
      break;
    case triviaui::ActionSettingsRow:
      switch (static_cast<triviaui::SettingRow>(value)) {
        case triviaui::SettingRow::UsCentric:
          showUsCentric_ = !showUsCentric_;
          savePreferences(difficulty_, showUsCentric_);
          break;
        case triviaui::SettingRow::Sync:
          // The radio first, exactly as ActionGetPack does. Entering the TLS
          // stack with WiFi never started is a panic rather than a failure --
          // the socket layer takes a mutex that does not exist yet.
          WiFi.mode(WIFI_STA);
          startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                                 [this](const ActivityResult& result) {
                                   if (result.isCancelled) return;
                                   // Queued rather than run here: the sync
                                   // blocks for a long time and pumps input
                                   // itself, which must not happen while an
                                   // action is still being routed.
                                   syncQueued_ = true;
                                   requestUpdate();
                                 });
          return;
        case triviaui::SettingRow::Hidden:
          if (state_.flaggedCount() > 0) {
            uint32_t restored = 0;
            for (uint32_t i = 0; i < state_.count(); ++i) {
              if (!state_.flagged(i)) continue;
              state_.clearFlag(i, trivia::kFlagged);
              ++restored;
            }
            LOG_INF("TRIVIA", "Un-hid %u question(s)", static_cast<unsigned>(restored));
          }
          break;
        case triviaui::SettingRow::Count:
          break;
      }
      requestUpdate();
      break;
    case triviaui::ActionCloseSettings:
      go(View::Menu);
      break;
    case triviaui::ActionQuit: {
      // Quizmaster keeps no score -- it is a person reading to a room, and
      // score_ belongs to solo. Ending there returns to the menu rather than
      // reporting a total that was never counted, or worse, a stale one left
      // over from an earlier solo round.
      if (view_ == View::Quizmaster) {
        go(View::Menu);
        break;
      }
      // Ending the round IS the summary. The two findings -- "cannot leave the
      // quiz" and "no final score" -- were one omission: with no deliberate way
      // to stop, there was no moment at which a score could be shown, so the
      // only exit (the HOME key) also threw the result away.
      //
      // Reuses the notice machinery, whose ActionMenuRow already returns to the
      // menu from a Notice, rather than adding a fifth View for one screen.
      char body[160];
      if (score_.asked == 0) {
        std::snprintf(body, sizeof(body), "%s", "No questions answered, so nothing to score.");
      } else {
        std::snprintf(body, sizeof(body), "You got %d of %d.", score_.right, score_.asked);
      }
      showNotice("ROUND OVER", body, "BACK TO MENU", triviaui::ActionMenuRow);
      break;
    }
    case triviaui::ActionReveal:
      revealed_ = true;
      requestUpdate();
      break;
    case triviaui::ActionNext:
      // From the HIDDEN notice, put the mode back first: deal() reads view_ to
      // decide whether it needs a question with distractors.
      if (view_ == View::Notice) go(flagReturn_);
      deal();
      break;
    case triviaui::ActionOption:
      if (chosen_ < 0 && value >= 0 && value < trivia::kOptions) {
        chosen_ = value;
        score_.record(value == choices_.correct);
        requestUpdate();
      }
      break;
    case triviaui::ActionFlag:
      // Hiding and undo are local to this device.
      if (haveQuestion_) {
        state_.setFlag(current_, trivia::kFlagged);
        hiddenIndex_ = current_;
        hiddenIndexValid_ = true;
        LOG_INF("TRIVIA", "Hid question %u", static_cast<unsigned>(current_));
        // Say what happened. Before this the question simply changed, which is
        // exactly what NEXT does, so a cold tester could not tell the button
        // had any effect at all and stopped pressing it.
        flagReturn_ = view_;
        char body[160];
        std::snprintf(body, sizeof(body), "That question will not come back. %u hidden so far.",
                      static_cast<unsigned>(state_.flaggedCount()));
        showNotice("HIDDEN", body, "NEXT QUESTION", triviaui::ActionNext);
        // Keep the same primary action position while offering Undo.
        noticeThird_ = "UNDO";
        noticeThirdId_ = triviaui::ActionUnhide;
      }
      break;

    case triviaui::ActionUnhide:
      // Restore a question hidden by an accidental tap.
      if (hiddenIndexValid_) {
        state_.clearFlag(hiddenIndex_, trivia::kFlagged);
        hiddenIndexValid_ = false;
        LOG_INF("TRIVIA", "Un-hid question %u", static_cast<unsigned>(hiddenIndex_));
        go(flagReturn_);
        deal();
      }
      break;
    default:
      break;
  }
}

void TriviaActivity::loop() {
  // Started from the action handler rather than inside it: the download blocks
  // for minutes and pumps input itself, which must not happen while an action
  // is still being routed.
  if (downloadQueued_) {
    downloadQueued_ = false;
    runPackDownload();
    return;
  }
  if (syncQueued_) {
    syncQueued_ = false;
    runSync();
    return;
  }

  // Back, which on this board is the left-edge swipe: wasBackGesture() is
  // folded into Button::Back, so this one read serves both the gesture and the
  // key on boards that have one. It has to be read HERE, before the early
  // return below -- the whole of card #250 is that this loop() returned on
  // "no tap" and a swipe is not a tap, so the gesture reached nothing and
  // Trivia was the one app in apps_local with no way back out of its front
  // door. The target per screen is trivia::backFrom(); see TriviaCore.h.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (trivia::backFrom(view_, pack_.isOpen())) {
      case trivia::Back::LeaveApp:
        shelf::leave(renderer, mappedInput);
        return;
      case trivia::Back::EndRound:
        // The END button's own handler, not a copy of it.
        routeAction(triviaui::ActionQuit, 0);
        return;
      case trivia::Back::ToMenu:
        go(View::Menu);
        return;
      case trivia::Back::ToNotice:
        go(View::Notice);
        return;
    }
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  // Touch only, like the rest of this fork's games. The app never blocks the
  // loop, so it must not call mappedInput.update(): the shell already did.
  if (!input.touchReleased || !interactionsReady_) return;

  const fui::ActionEvent action = interactions_.route(input);
  routeAction(static_cast<int>(action.action), static_cast<int>(action.value));
}

void TriviaActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Faces per view, not per app. The clue is a page of prose and wants the
  // reading serif; the front door is a menu and must look like every other
  // menu in the fork, which is Jersey. proseMenuFaces exists for exactly this
  // shape -- a headline with a sentence under it -- because at the 20px UI cut
  // a sentence runs off the panel and is truncated with an ellipsis Jersey
  // does not carry, so the line simply stops. See ToyboxTheme.h.
  const bool prose = view_ == View::Quizmaster || view_ == View::Solo || view_ == View::Notice;
  fui::GfxRendererTarget target =
      toybox::makeTarget(renderer, prose ? toybox::readingChromeFaces() : toybox::proseMenuFaces());
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions_);
  toybox::Screen screen(frame);

  switch (view_) {
    case View::Reason:  // Retained in the shared upstream view enum.
    case View::Notice: {
      triviaui::NoticeModel model;
      model.headline = noticeHead_;
      model.body = noticeBody_;
      model.actionLabel = noticeAction_;
      model.action = noticeActionId_;
      model.secondLabel = noticeSecond_;
      model.secondAction = noticeSecondId_;
      model.thirdLabel = noticeThird_;
      model.thirdAction = noticeThirdId_;
      triviaui::buildNotice(screen, model);
      break;
    }
    case View::Menu: {
      triviaui::MenuModel model;
      model.selected = selected_;
      model.difficulty = difficulty_;
      model.packCount = pack_.count();
      model.seenCount = state_.seenCount();
      triviaui::buildMenu(screen, model);
      break;
    }
    case View::Settings: {
      triviaui::SettingsModel model;
      model.usCentric = showUsCentric_;
      model.hidden = state_.flaggedCount();
      model.packLine = packLine_;
      triviaui::buildSettings(screen, model);
      break;
    }
    case View::Quizmaster: {
      triviaui::QuestionModel model;
      if (haveQuestion_) {
        model.clue = question_.clue();
        model.difficulty = question_.difficulty();
        if (revealed_) model.answer = question_.answer();
      } else {
        model.clue = "No question available. Try another difficulty.";
      }
      triviaui::buildQuestion(screen, model);
      break;
    }
    case View::Solo: {
      triviaui::ChoiceModel model;
      model.asked = score_.asked;
      model.right = score_.right;
      model.chosen = chosen_;
      if (haveQuestion_) {
        model.clue = question_.clue();
        model.difficulty = question_.difficulty();
        model.correct = choices_.correct;
        for (int i = 0; i < trivia::kOptions; ++i) model.option[i] = choices_.option[i];
      } else {
        model.clue = "No multiple-choice question available at this difficulty.";
      }
      triviaui::buildChoice(screen, model);
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Trivia");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(flashOnNextPaint_ ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint_ = false;
}
