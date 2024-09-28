#include <odr/internal/html/wvware_wrapper.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>

#include <wv/wv.h>

#include <fstream>
#include <iostream>

namespace odr::internal {

/// A lot of this code is duplicated from wvWare, mostly from `wvWare.c` and
/// `wvHtml.c`.
///
/// wvWare is writing to stdout, while we want to write to a file. Also, wvWare
/// is configurable to write not only HTML but also other formats. We only need
/// HTML.
///
/// We decided to duplicate the code instead of changing upstream wvWare code
/// because it is rather an application not a library, it is quite outdated and
/// not actively developed, and written in C. Duplication allows for a clean
/// separation between wvWare and our code while also being able to write modern
/// C++ code.
///
/// A copy of wvWare can be found here:
/// https://github.com/opendocument-app/wvWare
namespace {

/// Extension of `expand_data` see
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wv.h#L2776-L2814
/// to allow for more state variables.
struct TranslationState : public expand_data {
  int i = 0;
  char *charset = nullptr;
  PAP *ppap = nullptr;

  std::unique_ptr<std::ostream> output_stream;
};

/// Originally from `wvWare.c` `wvStrangeNoGraphicData`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L661-L676
/// simplified to HTML output
void strange_no_graphic_data(wvParseStruct *ps, int graphicstype) {
  std::cerr << "Strange No Graphic Data in the 0x01/0x08 graphic\n";

  // TODO print to output file
  printf(R"(<img alt="%#.2x graphic" src="%s"/><br/>)", graphicstype,
         "StrangeNoGraphicData");
}

/// Originally from `wvWare.c` `name_to_url`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1703-L1772
char *name_to_url(char *name) {
  // TODO get rid of static
  // TODO use std::string
  static char *url = 0;
  static long max = 0;
  char *ptr = 0;
  long count = 0;

  ptr = name;
  while (*ptr) {
    switch (*ptr) {
    case ' ':
      count += 3;
      break;
    default:
      count++;
      break;
    }
    ptr++;
  }
  count++;

  if (count > max) {
    char *more = nullptr;
    if (url == nullptr) {
      more = static_cast<char *>(malloc(count));
    } else {
      more = static_cast<char *>(realloc(url, count));
    }
    if (more != nullptr) {
      url = more;
      max = count;
    }
  }

  if (url != nullptr) {
    count = 0;
    ptr = name;
    while ((*ptr != 0) && (count < max)) {
      switch (*ptr) {
      case ' ':
        url[count++] = '%';
        if (count < max)
          url[count++] = '2';
        if (count < max)
          url[count++] = '0';
        break;
      default:
        url[count++] = *ptr;
        break;
      }
      ptr++;
    }
    url[max - 1] = 0;
  } else {
    std::cerr << "failed to convert name to URL\n";
    return name;
  }

  return url;
}

/// Originally from `wvWare.c` `wvPrintGraphics`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1239-L1287
/// simplified to HTML output
void print_graphics(int graphicstype, int width, int height, char *source) {
  // upstream converts to PNG, we just use the original format as the browser
  // should support them

  // TODO export/embed image

  // TODO replace printf
  printf(R"(<img width="%d" height="%d" alt="%#.2x graphic" src="%s"/><br/>)",
         width, height, graphicstype, name_to_url(source));
}

/// Originally from `wvWare.c` `myelehandler`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L503-L599
int element_handler(wvParseStruct *ps, wvTag tag, void *props, int dirty) {
  auto *data = (TranslationState *)ps->userData;
  data->anSttbfAssoc = &ps->anSttbfAssoc;
  data->lfo = &ps->lfo;
  data->lfolvl = ps->lfolvl;
  data->lvl = ps->lvl;
  data->nolfo = &ps->nolfo;
  data->nooflvl = &ps->nooflvl;
  data->stsh = &ps->stsh;
  data->lst = &ps->lst;
  data->noofLST = &ps->noofLST;
  data->liststartnos = &ps->liststartnos;
  data->listnfcs = &ps->listnfcs;
  data->finallvl = &ps->finallvl;
  data->fib = &ps->fib;
  data->dop = &ps->dop;
  data->intable = &ps->intable;
  data->cellbounds = &ps->cellbounds;
  data->nocellbounds = &ps->nocellbounds;
  data->endcell = &ps->endcell;
  data->vmerges = &ps->vmerges;
  data->norows = &ps->norows;
  data->nextpap = &ps->nextpap;
  if (data->charset == nullptr) {
    data->charset = wvAutoCharset(ps);
  }
  data->props = props;

  switch (tag) {
  case PARABEGIN: {
    S16 tilfo = 0;
    /* test begin */
    if (*(data->endcell) != 0) {
      tilfo = ((PAP *)(data->props))->ilfo;
      ((PAP *)(data->props))->ilfo = 0;
    }
    /* test end */
    data->ppap = (PAP *)data->props;
    wvBeginPara(data);
    if (tilfo != 0) {
      ((PAP *)(data->props))->ilfo = tilfo;
    }
  } break;
  case PARAEND: {
    S16 tilfo = 0;
    /* test begin */
    if (*(data->endcell) != 0) {
      tilfo = ((PAP *)(data->props))->ilfo;
      ((PAP *)(data->props))->ilfo = 0;
    }
    /* test end */
    wvEndCharProp(data); /* danger will break in the future */
    wvEndPara(data);
    if (tilfo != 0) {
      ((PAP *)(data->props))->ilfo = tilfo;
    }
    wvCopyPAP(&data->lastpap, (PAP *)(data->props));
  } break;
  case CHARPROPBEGIN:
    wvBeginCharProp(data, data->ppap);
    break;
  case CHARPROPEND:
    wvEndCharProp(data);
    break;
  case SECTIONBEGIN:
    wvBeginSection(data);
    break;
  case SECTIONEND:
    wvEndSection(data);
    break;
  case COMMENTBEGIN:
    wvBeginComment(data);
    break;
  case COMMENTEND:
    wvEndComment(data);
    break;
  default:
    break;
  }
  return 0;
}

/// Originally from `wvWare.c` `mydochandler`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L601-L659
int document_handler(wvParseStruct *ps, wvTag tag) {
  auto *data = (TranslationState *)ps->userData;
  data->anSttbfAssoc = &ps->anSttbfAssoc;
  data->lfo = &ps->lfo;
  data->lfolvl = ps->lfolvl;
  data->lvl = ps->lvl;
  data->nolfo = &ps->nolfo;
  data->nooflvl = &ps->nooflvl;
  data->stsh = &ps->stsh;
  data->lst = &ps->lst;
  data->noofLST = &ps->noofLST;
  data->liststartnos = &ps->liststartnos;
  data->listnfcs = &ps->listnfcs;
  data->finallvl = &ps->finallvl;
  data->fib = &ps->fib;
  data->dop = &ps->dop;
  data->intable = &ps->intable;
  data->cellbounds = &ps->cellbounds;
  data->nocellbounds = &ps->nocellbounds;
  data->endcell = &ps->endcell;
  data->vmerges = &ps->vmerges;
  data->norows = &ps->norows;
  if (data->i == 0) {
    wvSetEntityConverter(data);
    data->filename = ps->filename;
    data->whichcell = 0;
    data->whichrow = 0;
    data->asep = nullptr;
    data->i++;
    wvInitPAP(&data->lastpap);
    data->nextpap = nullptr;
    data->ps = ps;
  }

  if (data->charset == nullptr) {
    data->charset = wvAutoCharset(ps);
  }

  switch (tag) {
  case DOCBEGIN:
    wvBeginDocument(data);
    break;
  case DOCEND:
    wvEndDocument(data);
    break;
  default:
    break;
  }

  return 0;
}

/// Originally from `wvWare.c` `myCharProc`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1556-L1605
int char_handler(wvParseStruct *ps, U16 eachchar, U8 chartype, U16 lid) {
  auto *data = (TranslationState *)ps->userData;

  switch (eachchar) {
  case 19:
    ps->fieldstate++;
    ps->fieldmiddle = 0;
    fieldCharProc(ps, eachchar, chartype, lid); /* temp */
    return 0;
    break;
  case 20:
    fieldCharProc(ps, eachchar, chartype, lid);
    ps->fieldmiddle = 1;
    return 0;
    break;
  case 21:
    ps->fieldmiddle = 0;
    ps->fieldstate--;
    fieldCharProc(ps, eachchar, chartype, lid); /* temp */
    return 0;
    break;
  case 0x08:
    std::cerr << "hmm did we loose the fSpec flag ?, this is possibly a bug\n";
    break;
  default:
    break;
  }

  if (ps->fieldstate != 0) {
    if (fieldCharProc(ps, eachchar, chartype, lid) != 0) {
      return 0;
    }
  }

  if (data->charset != nullptr) {
    wvOutputHtmlChar(eachchar, chartype, data->charset, lid);
  } else {
    wvOutputHtmlChar(eachchar, chartype, wvAutoCharset(ps), lid);
  }

  return 0;
}

/// Originally from `wvWare.c` `mySpecCharProc`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1289-L1553
int special_char_handler(wvParseStruct *ps, U16 eachchar, CHP *achp) {
  static int message;
  PICF picf;
  FSPA *fspa;
  auto *data = (TranslationState *)ps->userData;

  switch (eachchar) {
  case 19:
    std::cerr << "field began\n";
    ps->fieldstate++;
    ps->fieldmiddle = 0;
    fieldCharProc(ps, eachchar, 0, 0x400); /* temp */
    return 0;
  case 20:
    if (achp->fOle2) {
      std::cerr << "this field has an associated embedded object of id "
                << achp->fcPic_fcObj_lTagObj << "\n";
    }
    fieldCharProc(ps, eachchar, 0, 0x400); /* temp */
    ps->fieldmiddle = 1;
    return 0;
  case 21:
    ps->fieldstate--;
    ps->fieldmiddle = 0;
    fieldCharProc(ps, eachchar, 0, 0x400); /* temp */
    return 0;
    break;
  }

  if (ps->fieldstate) {
    if (fieldCharProc(ps, eachchar, 0, 0x400))
      return 0;
  }

  switch (eachchar) {
  case 0x05:
    /* this should be handled by the COMMENTBEGIN and COMMENTEND events */
    return 0;
  case 0x01: {
    wvStream *f;
    Blip blip;
    char *name;
    long p = wvStream_tell(ps->data);
    std::cerr << "picture 0x01 here, at offset " << achp->fcPic_fcObj_lTagObj
              << " in Data Stream, obj is " << achp->fObj << ", ole is "
              << achp->fOle2 << "\n";

    if (achp->fOle2) {
      return 0;
    }
    if (no_graphics != 0) {
      wvStream_goto(ps->data, achp->fcPic_fcObj_lTagObj);
      wvGetPICF(wvQuerySupported(&ps->fib, nullptr), &picf, ps->data);
      f = picf.rgb;
      if (wv0x01(&blip, f, picf.lcb - picf.cbHeader) != 0) {
        name = wvHtmlGraphic(ps, &blip);
        print_graphics(0x01, (int)wvTwipsToHPixels(picf.dxaGoal),
                       (int)wvTwipsToVPixels(picf.dyaGoal), name);
        wvFree(name);
      } else {
        strange_no_graphic_data(ps, 0x01);
      }
    }

    wvStream_goto(ps->data, p);
    return 0;
  }
  case 0x08: {
    Blip blip;
    char *name;
    if (wvQuerySupported(&ps->fib, nullptr) == WORD8) {
      if (!no_graphics) {
        if (ps->nooffspa > 0) {
          fspa = wvGetFSPAFromCP(ps->currentcp, ps->fspa, ps->fspapos,
                                 ps->nooffspa);

          if (fspa == nullptr) {
            std::cerr << "No fspa! Insanity abounds!\n";
            return 0;
          }

          data->props = fspa;
          if (wv0x08(&blip, fspa->spid, ps)) {
            name = wvHtmlGraphic(ps, &blip);
            print_graphics(
                0x08, (int)wvTwipsToHPixels(fspa->xaRight - fspa->xaLeft),
                (int)wvTwipsToVPixels(fspa->yaBottom - fspa->yaTop), name);
            wvFree(name);
          } else
            strange_no_graphic_data(ps, 0x08);
        } else {
          std::cerr << "nooffspa was <=0!  Ignoring.\n";
        }
      }
    } else {
      FDOA *fdoa;
      std::cerr << "pre word8 0x08 graphic, unsupported at the moment\n";
      fdoa =
          wvGetFDOAFromCP(ps->currentcp, ps->fdoa, ps->fdoapos, ps->nooffdoa);
      data->props = fdoa;
    }

    // Potentially relevant disabled code section in `wvWare.c`?
    // https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1443-L1459

    return 0;
  }
  case 0x28: {
    U16 symbol[6] = {'S', 'y', 'm', 'b', 'o', 'l'};
    U16 wingdings[9] = {'W', 'i', 'n', 'g', 'd', 'i', 'n', 'g', 's'};
    U16 mtextra[8] = {'M', 'T', ' ', 'E', 'x', 't', 'r', 'a'};

    if (0 == memcmp(symbol, ps->fonts.ffn[achp->ftcSym].xszFfn, 12)) {
      if ((!message) && (strcasecmp("UTF-8", data->charset) != 0)) {
        std::cerr
            << "Symbol font detected (too late sorry!), rerun wvHtml with option --charset utf-8\n\
option to support correct symbol font conversion to a viewable format.\n";
        message++;
      }
      wvOutputFromUnicode(wvConvertSymbolToUnicode(achp->xchSym - 61440),
                          data->charset);
      return 0;
    } else if (0 == memcmp(mtextra, ps->fonts.ffn[achp->ftcSym].xszFfn, 16)) {
      if ((message == 0) && (strcasecmp("UTF-8", data->charset) != 0)) {
        std::cerr
            << "MT Extra font detected (too late sorry!), rerun wvHtml with option --charset utf-8\n\
option to support correct symbol font conversion to a viewable format.\n";
        message++;
      }
      wvOutputFromUnicode(wvConvertMTExtraToUnicode(achp->xchSym - 61440),
                          data->charset);
      return 0;
    } else if (0 == memcmp(wingdings, ps->fonts.ffn[achp->ftcSym].xszFfn, 18)) {
      if (message == 0) {
        std::cerr << "Wingdings font detected, i need a mapping table to "
                     "unicode for this\n";
        message++;
      }
    } else {
      if (message == 0) {
        char *fontname = wvWideStrToMB(ps->fonts.ffn[achp->ftcSym].xszFfn);
        std::cerr << "Special font " << fontname
                  << ", i need a mapping table to unicode for this\n";
        wvFree(fontname);
        // TODO replace printf
        printf("*");
      }
      return 0;
    }
  }
  default:
    return 0;
  }

  return 0;
}

} // namespace

Html html::translate_wvware_oldms_file(
    const WvWareLegacyMicrosoftFile &oldms_file, const std::string &output_path,
    const HtmlConfig &config) {
  auto output_file_path = output_path + "/document.html";

  wvParseStruct &ps = oldms_file.parse_struct();

  wvSetElementHandler(&ps, element_handler);
  wvSetDocumentHandler(&ps, document_handler);
  wvSetCharHandler(&ps, char_handler);
  wvSetSpecialCharHandler(&ps, special_char_handler);

  state_data handle;
  TranslationState translation_state;
  translation_state.output_stream =
      std::make_unique<std::ofstream>(output_file_path, std::ios::out);

  wvInitStateData(&handle);

  translation_state.sd = &handle;
  ps.userData = &translation_state;

  *translation_state.output_stream << "<!DOCTYPE html>\n<html>\n<head>\n"
                                   << "<meta charset=\"UTF-8\">\n"
                                   << "<title>Document</title>\n"
                                   << "</head>\n<body>\n";

  if (wvHtml(&ps) != 0) {
    throw std::runtime_error("wvHtml failed");
  }

  *translation_state.output_stream << "</body>\n</html>\n";

  translation_state.output_stream->flush();

  return {
      FileType::legacy_word_document, config, {{"document", output_file_path}}};
}

} // namespace odr::internal
