#pragma once

#include <array>
#include <cstdint>

namespace odr::internal::oldms::presentation {

// Filled by copying file bytes straight in (see ppt_io), so multi-byte fields
// use host byte order — correct only on little-endian hosts (see ppt_io.hpp).
// Bit-fields additionally assume LSB-first allocation, which all supported
// compilers use on little-endian targets (shared oldms/ assumption).

/// Record types relevant to text extraction. See [MS-PPT] 2.13.24 RecordType.
enum RecordType : std::uint16_t {
  RT_DocumentContainer = 0x03E8,  //< top-level document
  RT_DocumentAtom = 0x03E9,       //< slide size etc. [MS-PPT] 2.4.2
  RT_SlideContainer = 0x03EE,     //< a slide (drawing + placeholders)
  RT_Notes = 0x03F0,              //< notes page (skipped)
  RT_Environment = 0x03F2,        //< DocumentTextInfoContainer [MS-PPT] 2.4.5
  RT_SlidePersistAtom = 0x03F3,   //< delimits each slide's text in the list
  RT_MainMaster = 0x03F8,         //< master slide (skipped)
  RT_FontCollection = 0x07D5,     //< document fonts [MS-PPT] 2.9.8
  RT_OutlineTextRefAtom = 0x0F9E, //< box text lives in the slide list, by index
  RT_TextHeaderAtom = 0x0F9F,     //< type of the text block that follows
  RT_TextCharsAtom = 0x0FA0,      //< UTF-16 text (two bytes per code unit)
  RT_StyleTextPropAtom = 0x0FA1,  //< text formatting runs [MS-PPT] 2.9.44
  RT_TextBytesAtom = 0x0FA8,      //< "compressed" text: one byte per character
  RT_FontEntityAtom = 0x0FB7,     //< one font; recInstance = index
  RT_SlideListWithText = 0x0FF0,  //< outline text for all slides
  RT_UserEditAtom = 0x0FF5,    //< a user edit (offsets to dir + previous edit)
  RT_CurrentUserAtom = 0x0FF6, //< in the "Current User" stream
  RT_PersistDirectoryAtom = 0x1772, //< persist id -> stream offset directory

  // Office Art (Escher) drawing records holding the slide's text boxes:
  // OfficeArt* are [MS-ODRAW], client records (textbox/anchor) are [MS-PPT].
  RT_Drawing = 0x040C,               //< DrawingContainer        [MS-PPT] 2.5.13
  RT_DrawingGroup = 0x040B,          //< DrawingGroupContainer   [MS-PPT] 2.4.3
  RT_OfficeArtDggContainer = 0xF000, //< drawing group data [MS-ODRAW] 2.2.12
  RT_OfficeArtBStoreContainer = 0xF001, //< the BLIP store [MS-ODRAW] 2.2.20
  RT_OfficeArtDgContainer = 0xF002,     //< [MS-ODRAW] 2.2.13
  RT_OfficeArtSpgrContainer = 0xF003,   //< shape group [MS-ODRAW] 2.2.16
  RT_OfficeArtSpContainer = 0xF004, //< one shape (a text box) [MS-ODRAW] 2.2.14
  RT_OfficeArtFBSE = 0xF007,        //< BLIP store entry [MS-ODRAW] 2.2.32
  RT_OfficeArtFSP = 0xF00A,         //< shape id + flags [MS-ODRAW] 2.2.40
  RT_OfficeArtFOPT = 0xF00B,        //< shape properties [MS-ODRAW] 2.2.9
  RT_OfficeArtClientTextbox = 0xF00D, //< a shape's text        [MS-PPT] 2.9.76
  RT_OfficeArtClientAnchor = 0xF010,  //< a shape's position    [MS-PPT] 2.7.1
  RT_OfficeArtBlipJPEG = 0xF01D,      //< JPEG BLIP [MS-ODRAW] 2.2.27
  RT_OfficeArtBlipPNG = 0xF01E,       //< PNG BLIP  [MS-ODRAW] 2.2.28
};

/// The pib property of an OfficeArtFOPT: the shape's picture as a one-based
/// index into the BLIP store ([MS-ODRAW] 2.3.23.2).
constexpr std::uint16_t office_art_property_pib = 0x0104;
/// The fillBlip property: a picture fill's BLIP, same index semantics
/// ([MS-ODRAW] 2.3.7.2). LibreOffice-exported .ppt places pictures this way.
constexpr std::uint16_t office_art_property_fill_blip = 0x0186;
/// OfficeArtFSP.fBackground: the shape is the slide background
/// ([MS-ODRAW] 2.2.40).
constexpr std::uint32_t office_art_fsp_background = 0x400;

/// recInstance of a RT_SlideListWithText container: which list it is (slides /
/// master / notes) inside the DocumentContainer ([MS-PPT] 2.4.14.1/3/6).
enum SlideListInstance : std::uint16_t {
  SlideListInstance_Slides = 0x000,
  SlideListInstance_Master = 0x001,
  SlideListInstance_Notes = 0x002,
};

#pragma pack(push, 1)

/// Every record in the "PowerPoint Document" stream starts with this 8-byte
/// header. See [MS-PPT] 2.3.1 RecordHeader.
struct RecordHeader {
  std::uint16_t recVer : 4;
  std::uint16_t recInstance : 12;
  std::uint16_t recType;
  std::uint32_t recLen; //< bytes of body that follow the header

  /// recVer == 0xF marks a container whose body is a sequence of records.
  [[nodiscard]] bool is_container() const { return recVer == 0xF; }
};
static_assert(sizeof(RecordHeader) == 8, "RecordHeader should be 8 bytes");

/// Fixed prefix of the CurrentUserAtom ([MS-PPT] 2.3.2); the variable user-name
/// tail is ignored.
struct CurrentUserAtomHead {
  RecordHeader rh;
  std::uint32_t size;
  std::uint32_t headerToken;
  std::uint32_t offsetToCurrentEdit; //< offset of the newest UserEditAtom
};
static_assert(sizeof(CurrentUserAtomHead) == 20,
              "CurrentUserAtomHead should be 20 bytes");

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
static_assert(sizeof(UserEditAtomBody) == 28,
              "UserEditAtomBody should be 28 bytes");

/// Fixed part of an OfficeArtFBSE ([MS-ODRAW] 2.2.32); an optional name and
/// an optional embedded BLIP follow.
struct OfficeArtFbseFixed {
  std::uint8_t btWin32; //< MSOBLIPTYPE
  std::uint8_t btMacOS;
  std::array<std::uint8_t, 16> rgbUid;
  std::uint16_t tag;
  std::uint32_t size;    //< BLIP size in the stream
  std::uint32_t cRef;    //< 0 = empty slot
  std::uint32_t foDelay; //< offset in the delay ("Pictures") stream, or -1
  std::uint8_t unused1;
  std::uint8_t cbName;
  std::uint8_t unused2;
  std::uint8_t unused3;
};
static_assert(sizeof(OfficeArtFbseFixed) == 36,
              "OfficeArtFbseFixed should be 36 bytes");

/// One property of an OfficeArtFOPT ([MS-ODRAW] 2.2.7/2.2.8); complex data
/// follows the property array.
struct OfficeArtFopte {
  std::uint16_t opid : 14;
  std::uint16_t fBid : 1;
  std::uint16_t fComplex : 1;
  std::uint32_t op;
};
static_assert(sizeof(OfficeArtFopte) == 6, "OfficeArtFopte should be 6 bytes");

#pragma pack(pop)

/// A shape's position/size from an OfficeArtClientAnchor ([MS-PPT] 2.12.7/8),
/// in master units (1/576 inch) of the slide's coordinate system.
struct Anchor {
  std::int32_t top{0};
  std::int32_t left{0};
  std::int32_t right{0};
  std::int32_t bottom{0};
};

} // namespace odr::internal::oldms::presentation
