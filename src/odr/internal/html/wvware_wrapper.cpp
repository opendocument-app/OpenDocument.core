#include <odr/internal/html/wvware_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>

#include <wv/wv.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

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
  explicit TranslationState(html::HtmlWriter _out)
      : expand_data{}, out(std::move(_out)) {}

  char *charset = nullptr;
  PAP *ppap = nullptr;

  struct {
    int message = 0;
  } special_char_handler_state = {};

  html::HtmlWriter out;
};

/// Originally from `text.c` `wvConvertUnicodeToHtml`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/text.c#L1999-L2154
int convert_unicode_to_html(wvParseStruct *ps, U16 char16) {
  auto *data = (TranslationState *)ps->userData;
  auto &out = data->out;

  switch (char16) {
  case 11:
    out.out() << "<br>";
    return 1;
  case 31:                /* non-required hyphen */
    out.out() << "&shy;"; /*vladimir@lukianov.name HTML 4.01 spec*/
    return 1;
  case 30:
  case 45:
  case 0x2013:
    out.out() << "-"; /* en-dash */
    return 1;
  case 12:
  case 13:
  case 14:
  case 7:
    return 1;
  case 34:
    out.out() << "&quot;";
    return 1;
  case 38:
    out.out() << "&amp;";
    return 1;
  case 60:
    out.out() << "&lt;";
    return 1;
  case 62:
    out.out() << "&gt;";
    return 1;
    /*
     german characters, im assured that this is the right way to handle them
     by Markus Schulte <markus@dom.de>

           As the output encoding for HTML was chosen as UTF-8,
           we don't need &Auml; etc. etc. I removed all but sz
           -- MV 6.4.2000
     */

  case 0xdf:
    out.out() << "&szlig;";
    return 1;
    /* end german characters */
  case 0x2026:
#if 0
/*
this just looks awful in netscape 4.5, so im going to do a very foolish
thing and just put ... instead of this
*/
	  printf ("&#133;");
/*is there a proper html name for ... &ellipse;? Yes, &hellip; -- MV */
#endif
    out.out() << "&hellip;";
    return 1;
  case 0x2019:
    out.out() << "'";
    return 1;
  case 0x2215:
    out.out() << "/";
    return 1;
  case 0xF8E7: /* without this, things should work in theory, but not for me */
    out.out() << "_";
    return 1;
  case 0x2018:
    out.out() << "`";
    return 1;

    /* Windows specials (MV): */
  case 0x0160:
    out.out() << "&Scaron;";
    return 1;
  case 0x0161:
    out.out() << "&scaron;";
    return 1;
  case 0x2014:
    out.out() << "&mdash;";
    return 1;
  case 0x201c:
    out.out() << "&ldquo;"; /* inverted double quotation mark */
    return 1;
  case 0x201d:
    out.out() << "&rdquo;"; /* double q.m. */
    return 1;
  case 0x201e:
    out.out() << "&bdquo;"; /* below double q.m. */
    return 1;
  case 0x2020:
    out.out() << "&dagger;";
    return 1;
  case 0x2021:
    out.out() << "&Dagger;";
    return 1;
  case 0x2022:
    out.out() << "&bull;";
    return 1;
  case 0x0152:
    out.out() << "&OElig;";
    return 1;
  case 0x0153:
    out.out() << "&oelig;";
    return 1;
  case 0x0178:
    out.out() << "&Yuml;";
    return 1;
  case 0x2030:
    out.out() << "&permil;";
    return 1;
  case 0x20ac:
    out.out() << "&euro;";
    return 1;

    /* Mac specials (MV): */

  case 0xf020:
    out.out() << " ";
    return 1;
  case 0xf02c:
    out.out() << ",";
    return 1;
  case 0xf028:
    out.out() << "(";
    return 1;

  case 0xf03e:
    out.out() << "&gt;";
    return 1;
  case 0xf067:
    out.out() << "&gamma;";
    return 1;
  case 0xf064:
    out.out() << "&delta;";
    return 1;
  case 0xf072:
    out.out() << "&rho;";
    return 1;
  case 0xf073:
    out.out() << "&sigma;";
    return 1;
  case 0xf0ae:
    out.out() << "&rarr;"; /* right arrow */
    return 1;
  case 0xf0b6:
    out.out() << "&part;"; /* partial deriv. */
    return 1;
  case 0xf0b3:
    out.out() << "&ge;";
    return 1;
  default:
    break;
  }
  /* Debugging aid: */
  /* if (char16 >= 0x100) printf("[%x]", char16); */
  return 0;
}

/// Originally from `text.c` `wvOutputFromUnicode`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/text.c#L757-L840
void output_from_unicode(wvParseStruct *ps, U16 eachchar, char *outputtype) {
  auto *data = (TranslationState *)ps->userData;
  auto &out = data->out;

  // TODO static
  static char cached_outputtype[33]; /* Last outputtype                  */
  static GIConv g_iconv_handle = (GIConv)-1; /* Cached iconv descriptor */
  static int need_swapping;
  gchar *ibuf, *obuf;
  size_t ibuflen, obuflen, len, count, i;
  U8 buffer[2], buffer2[5];

  if (convert_unicode_to_html(ps, eachchar) != 0) {
    return;
  }

  if ((g_iconv_handle == (GIConv)-1) ||
      strcmp(cached_outputtype, outputtype) != 0) {
    if ((g_iconv_handle != (GIConv)-1)) {
      g_iconv_close(g_iconv_handle);
    }

    g_iconv_handle = g_iconv_open(outputtype, "UCS-2");
    if (g_iconv_handle == (GIConv)-1) {
      std::cerr << "g_iconv_open fail: " << errno
                << ", cannot convert UCS-2 to " << outputtype << "\n";
      out.out() << "?";
      return;
    }

    /* safe to cache the output type here */
    strcpy(cached_outputtype, outputtype);

    /* Determining if unicode biteorder is swapped (glibc < 2.2) */
    need_swapping = 1;

    buffer[0] = 0x20;
    buffer[1] = 0;
    ibuf = reinterpret_cast<gchar *>(buffer);
    obuf = reinterpret_cast<gchar *>(buffer2);
    ibuflen = 2;
    obuflen = 5;

    count = g_iconv(g_iconv_handle, &ibuf, &ibuflen, &obuf, &obuflen);
    if (count >= 0) {
      need_swapping = buffer2[0] != 0x20;
    }
  }

  if (need_swapping) {
    buffer[0] = (eachchar >> 8) & 0x00ff;
    buffer[1] = eachchar & 0x00ff;
  } else {
    buffer[0] = eachchar & 0x00ff;
    buffer[1] = (eachchar >> 8) & 0x00ff;
  }

  ibuf = reinterpret_cast<gchar *>(buffer);
  obuf = reinterpret_cast<gchar *>(buffer2);

  ibuflen = 2;
  len = obuflen = 5;

  count = g_iconv(g_iconv_handle, &ibuf, &ibuflen, &obuf, &obuflen);
  if (count == (size_t)-1) {
    std::cerr << "iconv failed, errno: " << errno << ", char: 0x" << std::hex
              << eachchar << ", UCS-2 -> " << outputtype << "\n";

    /* I'm torn here - do i just announce the failure, continue, or copy over to
     * the other buffer? */

    /* errno is usually 84 (illegal byte sequence)
       should i reverse the bytes and try again? */
    out.out() << ibuf[1];
  } else {
    len = len - obuflen;

    for (i = 0; i < len; i++) {
      out.out() << buffer2[i];
    }
  }
}

/// Originally from `wvWare.c` `wvStrangeNoGraphicData`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L661-L676
/// simplified to HTML output
void strange_no_graphic_data(wvParseStruct *ps, int graphicstype) {
  auto *data = (TranslationState *)ps->userData;
  auto &out = data->out;

  std::cerr << "Strange No Graphic Data in the 0x01/0x08 graphic\n";

  // TODO
  out.out() << R"(<img alt=")" << std::hex << graphicstype
            << R"( graphic" src="StrangeNoGraphicData"/><br/>)";
}

/// Originally from `wvWare.c` `wvPrintGraphics`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1239-L1287
/// simplified to HTML output
void print_graphics(wvParseStruct *ps, int graphicstype, int width, int height,
                    char *source) {
  // upstream converts to PNG, we just use the original format as the browser
  // should support them

  auto *data = (TranslationState *)ps->userData;
  auto &out = data->out;

  // TODO export/embed image

  out.out() << R"(<img width=")" << width << R"(" height=")" << height
            << R"(" alt=")" << std::hex << graphicstype << R"( graphic" src=")"
            << source << R"("/><br/>)";
}

int handle_bitmap(wvParseStruct *ps, char *name, BitmapBlip *bitmap) {
  wvStream *pwv = bitmap->m_pvBits;
  FILE *fd = nullptr;
  size_t size = 0, i;

  fd = fopen(name, "wb");
  if (fd == nullptr) {
    fprintf(stderr, "\nCannot open %s for writing\n", name);
    exit(1);
  }
  size = wvStream_size(pwv);
  wvStream_rewind(pwv);

  for (i = 0; i < size; i++) {
    fputc(read_8ubit(pwv), fd);
  }
  fclose(fd);
  wvTrace(("Name is %s\n", name));
  return 0;
}

int handle_metafile(wvParseStruct *ps, char *name, MetaFileBlip *bitmap) {
  wvStream *pwv = bitmap->m_pvBits;
  FILE *fd = nullptr;
  size_t size = 0, i;
  U8 decompressf = 0;

  fd = fopen(name, "wb");
  if (fd == nullptr) {
    fprintf(stderr, "\nCannot open %s for writing\n", name);
    exit(1);
  }
  size = wvStream_size(pwv);
  wvStream_rewind(pwv);

  if (bitmap->m_fCompression == msocompressionDeflate)
    decompressf = setdecom();

  if (!decompressf) {
    for (i = 0; i < size; i++)
      fputc(read_8ubit(pwv), fd);
  } else /* decompress here */
  {
    FILE *tmp = tmpfile();
    FILE *out = tmpfile();

    for (i = 0; i < size; i++) {
      fputc(read_8ubit(pwv), tmp);
    }

    rewind(tmp);
    decompress(tmp, out, bitmap->m_cbSave, bitmap->m_cb);
    fclose(tmp);

    rewind(out);

    for (i = 0; i < bitmap->m_cb; i++)
      fputc(fgetc(out), fd);

    fclose(out);
  }

  fclose(fd);
  wvTrace(("Name is %s\n", name));
  return 0;
}

char *html_graphic(wvParseStruct *ps, Blip *blip) {
  char *name;
  wvStream *fd;
  char test[3];

  // TODO handle figure name
  name = nullptr;
  if (name == nullptr) {
    return nullptr;
  }

  /*
     temp hack to test older included bmps in word 6 and 7,
     should be wrapped in a modern escher strucure before getting
     to here, and then handled as normal
   */
  wvTrace(("type is %d\n", blip->type));
  switch (blip->type) {
  case msoblipJPEG:
  case msoblipDIB:
  case msoblipPNG:
    fd = (blip->blip.bitmap.m_pvBits);
    test[2] = '\0';
    test[0] = read_8ubit(fd);

    test[1] = read_8ubit(fd);
    wvStream_rewind(fd);
    if (!(strcmp(test, "BM"))) {
      wvAppendStr(&name, ".bmp");
      if (0 != handle_bitmap(ps, name, &blip->blip.bitmap)) {
        return nullptr;
      }
      return name;
    }
  default:
    break;
  }

  switch (blip->type) {
  case msoblipWMF:
    wvAppendStr(&name, ".wmf");
    if (0 != handle_metafile(ps, name, &blip->blip.metafile)) {
      return nullptr;
    }
    break;
  case msoblipEMF:
    wvAppendStr(&name, ".emf");
    if (0 != handle_metafile(ps, name, &blip->blip.metafile)) {
      return nullptr;
    }
    break;
  case msoblipPICT:
    wvAppendStr(&name, ".pict");
    if (0 != handle_metafile(ps, name, &blip->blip.metafile)) {
      return nullptr;
    }
    break;
  case msoblipJPEG:
    wvAppendStr(&name, ".jpg");
    if (0 != handle_bitmap(ps, name, &blip->blip.bitmap)) {
      return nullptr;
    }
    break;
  case msoblipDIB:
    wvAppendStr(&name, ".dib");
    if (0 != handle_bitmap(ps, name, &blip->blip.bitmap)) {
      return nullptr;
    }
    break;
  case msoblipPNG:
    wvAppendStr(&name, ".png");
    if (0 != handle_bitmap(ps, name, &blip->blip.bitmap)) {
      return nullptr;
    }
    break;
  }
  return name;
}

/// Originally from `wvWare.c` `myelehandler`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L503-L599
int element_handler(wvParseStruct *ps, wvTag tag, void *props, int /*dirty*/) {
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

  wvSetEntityConverter(data);
  data->filename = ps->filename;
  data->whichcell = 0;
  data->whichrow = 0;
  data->asep = nullptr;
  wvInitPAP(&data->lastpap);
  data->nextpap = nullptr;
  data->ps = ps;

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
  case 20:
    fieldCharProc(ps, eachchar, chartype, lid);
    ps->fieldmiddle = 1;
    return 0;
  case 21:
    ps->fieldmiddle = 0;
    ps->fieldstate--;
    fieldCharProc(ps, eachchar, chartype, lid); /* temp */
    return 0;
  case 0x08:
    std::cerr << "hmm did we loose the fSpec flag ?, this is possibly a bug\n";
    break;
  default:
    break;
  }

  if (ps->fieldstate != 0 && fieldCharProc(ps, eachchar, chartype, lid) != 0) {
    return 0;
  }

  // from `wvOutputHtmlChar`
  {
    char *outputtype =
        data->charset != nullptr ? data->charset : wvAutoCharset(ps);
    if (chartype != 0) {
      eachchar = wvHandleCodePage(eachchar, lid);
    }
    output_from_unicode(ps, eachchar, outputtype);
  }

  return 0;
}

/// Originally from `wvWare.c` `mySpecCharProc`
/// https://github.com/opendocument-app/wvWare/blob/c015326b001f1ad6dfb1f5e718461c16c56cca5f/wvWare.c#L1289-L1553
int special_char_handler(wvParseStruct *ps, U16 eachchar, CHP *achp) {
  auto *data = (TranslationState *)ps->userData;
  auto &state = data->special_char_handler_state;
  auto &out = data->out;

  PICF picf;
  FSPA *fspa = nullptr;

  switch (eachchar) {
  case 19:
    // field began
    ps->fieldstate++;
    ps->fieldmiddle = 0;
    fieldCharProc(ps, eachchar, 0, 0x400); /* temp */
    return 0;
  case 20:
    if (achp->fOle2 != 0) {
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
  default:
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

    if (achp->fOle2 != 0) {
      return 0;
    }

    wvStream_goto(ps->data, achp->fcPic_fcObj_lTagObj);
    wvGetPICF(wvQuerySupported(&ps->fib, nullptr), &picf, ps->data);
    f = picf.rgb;
    if (wv0x01(&blip, f, picf.lcb - picf.cbHeader) != 0) {
      name = html_graphic(ps, &blip);
      print_graphics(ps, 0x01, (int)wvTwipsToHPixels(picf.dxaGoal),
                     (int)wvTwipsToVPixels(picf.dyaGoal), name);
      wvFree(name);
    } else {
      strange_no_graphic_data(ps, 0x01);
    }

    wvStream_goto(ps->data, p);
    return 0;
  }
  case 0x08: {
    Blip blip;
    char *name;
    if (wvQuerySupported(&ps->fib, nullptr) == WORD8) {
      if (ps->nooffspa > 0) {
        fspa =
            wvGetFSPAFromCP(ps->currentcp, ps->fspa, ps->fspapos, ps->nooffspa);

        if (fspa == nullptr) {
          std::cerr << "No fspa! Insanity abounds!\n";
          return 0;
        }

        data->props = fspa;
        if (wv0x08(&blip, (int)fspa->spid, ps) != 0) {
          name = html_graphic(ps, &blip);
          print_graphics(
              ps, 0x08,
              (int)wvTwipsToHPixels((short)(fspa->xaRight - fspa->xaLeft)),
              (int)wvTwipsToVPixels((short)(fspa->yaBottom - fspa->yaTop)),
              name);
          wvFree(name);
        } else {
          strange_no_graphic_data(ps, 0x08);
        }
      } else {
        std::cerr << "nooffspa was <=0!  Ignoring.\n";
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
      if ((state.message == 0) && (strcasecmp("UTF-8", data->charset) != 0)) {
        std::cerr
            << "Symbol font detected (too late sorry!), rerun wvHtml with option --charset utf-8\n\
option to support correct symbol font conversion to a viewable format.\n";
        state.message++;
      }
      output_from_unicode(ps, wvConvertSymbolToUnicode(achp->xchSym - 61440),
                          data->charset);
      return 0;
    } else if (0 == memcmp(mtextra, ps->fonts.ffn[achp->ftcSym].xszFfn, 16)) {
      if ((state.message == 0) && (strcasecmp("UTF-8", data->charset) != 0)) {
        std::cerr
            << "MT Extra font detected (too late sorry!), rerun wvHtml with option --charset utf-8\n\
option to support correct symbol font conversion to a viewable format.\n";
        state.message++;
      }
      output_from_unicode(ps, wvConvertMTExtraToUnicode(achp->xchSym - 61440),
                          data->charset);
      return 0;
    } else if (0 == memcmp(wingdings, ps->fonts.ffn[achp->ftcSym].xszFfn, 18)) {
      if (state.message == 0) {
        std::cerr << "Wingdings font detected, i need a mapping table to "
                     "unicode for this\n";
        state.message++;
      }
    } else {
      if (state.message == 0) {
        char *fontname = wvWideStrToMB(ps->fonts.ffn[achp->ftcSym].xszFfn);
        std::cerr << "Special font " << fontname
                  << ", I need a mapping table to unicode for this\n";
        wvFree(fontname);
        out.out() << "*";
        state.message++;
      }
      return 0;
    }
  }
  default:
    break;
  }

  return 0;
}

} // namespace

Html html::translate_wvware_oldms_file(
    const WvWareLegacyMicrosoftFile &oldms_file, const std::string &output_path,
    const HtmlConfig &config) {
  HtmlResourceLocator resourceLocator =
      local_resource_locator(output_path, config);

  auto output_file_path = output_path + "/document.html";

  std::ofstream ostream(output_file_path, std::ios::out);
  if (!ostream.is_open()) {
    throw FileWriteError();
  }
  html::HtmlWriter out(ostream, config.format_html, config.html_indent);

  wvParseStruct &ps = oldms_file.parse_struct();

  wvSetElementHandler(&ps, element_handler);
  wvSetDocumentHandler(&ps, document_handler);
  wvSetCharHandler(&ps, char_handler);
  wvSetSpecialCharHandler(&ps, special_char_handler);

  state_data handle;
  TranslationState translation_state(out);

  wvInitStateData(&handle);

  translation_state.sd = &handle;
  ps.userData = &translation_state;

  out.write_begin();
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  out.write_header_viewport(
      "width=device-width,initial-scale=1.0,user-scalable=yes");
  out.write_header_end();
  out.write_body_begin();

  if (wvHtml(&ps) != 0) {
    throw std::runtime_error("wvHtml failed");
  }

  out.write_body_end();
  out.write_end();

  return {
      FileType::legacy_word_document, config, {{"document", output_file_path}}};
}

} // namespace odr::internal
