#pragma once

#include <cstdint>

namespace odr::internal::oldms::presentation {

// Filled by copying file bytes straight in (see ppt_io), so multi-byte fields
// use host byte order — correct only on little-endian hosts (see ppt_io.hpp).

#pragma pack(push, 1)

/// Every record in the "PowerPoint Document" stream starts with this 8-byte
/// header. See [MS-PPT] 2.3.1 RecordHeader.
struct RecordHeader {
  /// low 4 bits = recVer, high 12 bits = recInstance
  std::uint16_t recVerAndInstance;
  std::uint16_t recType;
  std::uint32_t recLen; //< bytes of body that follow the header

  [[nodiscard]] std::uint8_t rec_ver() const {
    return static_cast<std::uint8_t>(recVerAndInstance & 0x000F);
  }
  [[nodiscard]] std::uint16_t rec_instance() const {
    return static_cast<std::uint16_t>(recVerAndInstance >> 4);
  }
  /// recVer == 0xF marks a container whose body is a sequence of records.
  [[nodiscard]] bool is_container() const { return rec_ver() == 0xF; }
};

/// Fixed prefix of the CurrentUserAtom ([MS-PPT] 2.3.2); the variable user-name
/// tail is ignored.
struct CurrentUserAtomHead {
  RecordHeader rh;
  std::uint32_t size;
  std::uint32_t headerToken;
  std::uint32_t offsetToCurrentEdit; //< offset of the newest UserEditAtom
};

/// Body of a UserEditAtom ([MS-PPT] 2.3.3); the optional trailing
/// encryptSessionPersistIdRef is not read.
struct UserEditAtomBody {
  std::uint32_t lastSlideIdRef;
  std::uint16_t version;
  std::uint8_t minorVersion;
  std::uint8_t majorVersion;
  std::uint32_t offsetLastEdit;         //< previous UserEditAtom, 0 = none
  std::uint32_t offsetPersistDirectory; //< PersistDirectoryAtom for this edit
  std::uint32_t docPersistIdRef;        //< persist id of the DocumentContainer
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

/// Record types relevant to text extraction. See [MS-PPT] 2.13.24 RecordType.
enum RecordType : std::uint16_t {
  RT_DocumentContainer = 0x03E8,  //< top-level document
  RT_SlideContainer = 0x03EE,     //< a slide (drawing + placeholders)
  RT_Notes = 0x03F0,              //< notes page (skipped)
  RT_SlidePersistAtom = 0x03F3,   //< delimits each slide's text in the list
  RT_MainMaster = 0x03F8,         //< master slide (skipped)
  RT_OutlineTextRefAtom = 0x0F9E, //< box text lives in the slide list, by index
  RT_TextHeaderAtom = 0x0F9F,     //< type of the text block that follows
  RT_TextCharsAtom = 0x0FA0,      //< UTF-16 text (two bytes per code unit)
  RT_TextBytesAtom = 0x0FA8,      //< "compressed" text: one byte per character
  RT_SlideListWithText = 0x0FF0,  //< outline text for all slides
  RT_UserEditAtom = 0x0FF5,    //< a user edit (offsets to dir + previous edit)
  RT_CurrentUserAtom = 0x0FF6, //< in the "Current User" stream
  RT_PersistDirectoryAtom = 0x1772, //< persist id -> stream offset directory

  // Office Art (Escher) drawing records holding the slide's text boxes:
  // OfficeArt* are [MS-ODRAW], client records (textbox/anchor) are [MS-PPT].
  RT_Drawing = 0x040C,              //< DrawingContainer        [MS-PPT] 2.5.13
  RT_OfficeArtDgContainer = 0xF002, //< [MS-ODRAW] 2.2.13
  RT_OfficeArtSpgrContainer = 0xF003, //< shape group [MS-ODRAW] 2.2.16
  RT_OfficeArtSpContainer = 0xF004, //< one shape (a text box) [MS-ODRAW] 2.2.14
  RT_OfficeArtClientTextbox = 0xF00D, //< a shape's text        [MS-PPT] 2.9.76
  RT_OfficeArtClientAnchor = 0xF010,  //< a shape's position    [MS-PPT] 2.7.1
};

/// recInstance of a RT_SlideListWithText container: which list it is (slides /
/// master / notes) inside the DocumentContainer ([MS-PPT] 2.4.14.1/3/6).
enum SlideListInstance : std::uint16_t {
  SlideListInstance_Slides = 0x000,
  SlideListInstance_Master = 0x001,
  SlideListInstance_Notes = 0x002,
};

/// A shape's position/size from an OfficeArtClientAnchor ([MS-PPT] 2.12.7/8),
/// in master units (1/576 inch) of the slide's coordinate system.
struct Anchor {
  std::int32_t top{0};
  std::int32_t left{0};
  std::int32_t right{0};
  std::int32_t bottom{0};
};

} // namespace odr::internal::oldms::presentation
