/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkDocument_DEFINED
#define SkDocument_DEFINED

#include "SkBitmap.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkRefCnt.h"

class SkCanvas;
class SkWStream;

/** SK_ScalarDefaultDPI is 72 DPI.
*/
#define SK_ScalarDefaultRasterDPI           72.0f

/**
 *  High-level API for creating a document-based canvas. To use..
 *
 *  1. Create a document, specifying a stream to store the output.
 *  2. For each "page" of content:
 *      a. canvas = doc->beginPage(...)
 *      b. draw_my_content(canvas);
 *      c. doc->endPage();
 *  3. Close the document with doc->close().
 */
class SK_API SkDocument : public SkRefCnt {
public:
    /**
     *  Create a PDF-backed document, writing the results into a SkWStream.
     *
     *  PDF pages are sized in point units. 1 pt == 1/72 inch == 127/360 mm.
     *
     *  @param SkWStream* A PDF document will be written to this
     *         stream.  The document may write to the stream at
     *         anytime during its lifetime, until either close() is
     *         called or the document is deleted.
     *  @param dpi The DPI (pixels-per-inch) at which features without
     *         native PDF support will be rasterized (e.g. draw image
     *         with perspective, draw text with perspective, ...)  A
     *         larger DPI would create a PDF that reflects the
     *         original intent with better fidelity, but it can make
     *         for larger PDF files too, which would use more memory
     *         while rendering, and it would be slower to be processed
     *         or sent online or to printer.
     *  @returns NULL if there is an error, otherwise a newly created
     *           PDF-backed SkDocument.
     */
    static SkDocument* CreatePDF(SkWStream*,
                                 SkScalar dpi = SK_ScalarDefaultRasterDPI);

    /**
     *  Create a PDF-backed document, writing the results into a file.
     */
    static SkDocument* CreatePDF(const char outputFilePath[],
                                 SkScalar dpi = SK_ScalarDefaultRasterDPI);

    /**
     *  Create a XPS-backed document, writing the results into the stream.
     *  Returns NULL if XPS is not supported.
     */
    static SkDocument* CreateXPS(SkWStream* stream,
                                 SkScalar dpi = SK_ScalarDefaultRasterDPI);

    /**
     *  Create a XPS-backed document, writing the results into a file.
     *  Returns NULL if XPS is not supported.
     */
    static SkDocument* CreateXPS(const char path[],
                                 SkScalar dpi = SK_ScalarDefaultRasterDPI);
    /**
     *  Begin a new page for the document, returning the canvas that will draw
     *  into the page. The document owns this canvas, and it will go out of
     *  scope when endPage() or close() is called, or the document is deleted.
     */
    SkCanvas* beginPage(SkScalar width, SkScalar height,
                        const SkRect* content = NULL);

    /**
     *  Call endPage() when the content for the current page has been drawn
     *  (into the canvas returned by beginPage()). After this call the canvas
     *  returned by beginPage() will be out-of-scope.
     */
    void endPage();

    /**
     *  Call close() when all pages have been drawn. This will close the file
     *  or stream holding the document's contents. After close() the document
     *  can no longer add new pages. Deleting the document will automatically
     *  call close() if need be.
     *  Returns true on success or false on failure.
     */
    bool close();

    /**
     *  Call abort() to stop producing the document immediately.
     *  The stream output must be ignored, and should not be trusted.
     */
    void abort();

protected:
    SkDocument(SkWStream*, void (*)(SkWStream*, bool aborted));

    // note: subclasses must call close() in their destructor, as the base class
    // cannot do this for them.
    virtual ~SkDocument();

    virtual SkCanvas* onBeginPage(SkScalar width, SkScalar height,
                                  const SkRect& content) = 0;
    virtual void onEndPage() = 0;
    virtual bool onClose(SkWStream*) = 0;
    virtual void onAbort() = 0;

    // Allows subclasses to write to the stream as pages are written.
    SkWStream* getStream() { return fStream; }

    enum State {
        kBetweenPages_State,
        kInPage_State,
        kClosed_State
    };
    State getState() const { return fState; }

private:
    SkWStream* fStream;
    void       (*fDoneProc)(SkWStream*, bool aborted);
    State      fState;

    typedef SkRefCnt INHERITED;
};

#endif
