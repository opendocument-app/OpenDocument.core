#pragma once

#include <cstdint>

namespace odr::internal::oldms::presentation {

#pragma pack(push, 1)

// Every record in the "PowerPoint Document" stream starts with this 8-byte
// header. See [MS-PPT] 2.3.1 RecordHeader.
struct RecordHeader {
  // low 4 bits = recVer, high 12 bits = recInstance
  std::uint16_t recVerAndInstance;
  std::uint16_t recType;
  std::uint32_t recLen; // bytes of body that follow the header

  [[nodiscard]] std::uint8_t rec_ver() const {
    return static_cast<std::uint8_t>(recVerAndInstance & 0x000F);
  }
  [[nodiscard]] std::uint16_t rec_instance() const {
    return static_cast<std::uint16_t>(recVerAndInstance >> 4);
  }
  // recVer == 0xF marks a container whose body is a sequence of records.
  [[nodiscard]] bool is_container() const { return rec_ver() == 0xF; }
};

// Fixed prefix of the CurrentUserAtom (the only record in the "Current User"
// stream). See [MS-PPT] 2.3.2. Only the leading fields up to the offset of the
// most recent edit are needed; the variable user-name tail is ignored.
struct CurrentUserAtomHead {
  RecordHeader rh;
  std::uint32_t size;
  std::uint32_t headerToken;
  std::uint32_t offsetToCurrentEdit; // offset of the newest UserEditAtom
};

// Body of a UserEditAtom (the part after its 8-byte RecordHeader). See
// [MS-PPT] 2.3.3. The optional trailing encryptSessionPersistIdRef is not read.
struct UserEditAtomBody {
  std::uint32_t lastSlideIdRef;
  std::uint16_t version;
  std::uint8_t minorVersion;
  std::uint8_t majorVersion;
  std::uint32_t offsetLastEdit;         // previous UserEditAtom, 0 = none
  std::uint32_t offsetPersistDirectory; // PersistDirectoryAtom for this edit
  std::uint32_t docPersistIdRef;        // persist id of the DocumentContainer
  std::uint32_t persistIdSeed;
  std::uint16_t lastView;
  std::uint16_t unused;
};

#pragma pack(pop)

static_assert(sizeof(RecordHeader) == 8, "RecordHeader should be 8 bytes");
static_assert(sizeof(CurrentUserAtomHead) == 20,
              "CurrentUserAtomHead should be 20 bytes");
static_assert(sizeof(UserEditAtomBody) == 28,
              "UserEditAtomBody should be 28 bytes");

// Record types relevant to text extraction. See [MS-PPT] 2.13.24 RecordType.
enum RecordType : std::uint16_t {
  RT_DocumentContainer = 0x03E8, // top-level document
  RT_SlideContainer = 0x03EE,    // a slide (drawing + placeholders)
  RT_Notes = 0x03F0,             // notes page (skipped)
  RT_SlidePersistAtom = 0x03F3,  // delimits each slide's text in the list
  RT_MainMaster = 0x03F8,        // master slide (skipped)
  RT_TextHeaderAtom = 0x0F9F,    // type of the text block that follows
  RT_TextCharsAtom = 0x0FA0,     // UTF-16LE text
  RT_TextBytesAtom = 0x0FA8,     // "compressed" text: 1 byte/char, high byte 0
  RT_SlideListWithText = 0x0FF0, // outline text for all slides
  RT_UserEditAtom = 0x0FF5,      // a user edit (offsets to dir + previous edit)
  RT_CurrentUserAtom = 0x0FF6,   // in the "Current User" stream
  RT_PersistDirectoryAtom = 0x1772, // persist id -> stream offset directory
};

// recInstance values of a RT_SlideListWithText container, distinguishing the
// slide, master and notes lists inside the DocumentContainer. Per [MS-PPT]
// 2.4.14.1/2.4.14.3/2.4.14.6: SlideListWithTextContainer (the presentation
// slides) is 0x000, MasterListWithTextContainer is 0x001, and
// NotesListWithTextContainer is 0x002.
enum SlideListInstance : std::uint16_t {
  SlideListInstance_Slides = 0x000,
  SlideListInstance_Master = 0x001,
  SlideListInstance_Notes = 0x002,
};

} // namespace odr::internal::oldms::presentation
