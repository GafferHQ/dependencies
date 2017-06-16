
/*
 * Copyright 2010 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkPDFGraphicState_DEFINED
#define SkPDFGraphicState_DEFINED

#include "SkPaint.h"
#include "SkPDFTypes.h"
#include "SkTemplates.h"
#include "SkChecksum.h"

class SkPDFCanon;
class SkPDFFormXObject;

/** \class SkPDFGraphicState
    SkPaint objects roughly correspond to graphic state dictionaries that can
    be installed. So that a given dictionary is only output to the pdf file
    once, we want to canonicalize them.
*/
class SkPDFGraphicState : public SkPDFObject {
    
public:
    enum SkPDFSMaskMode {
        kAlpha_SMaskMode,
        kLuminosity_SMaskMode
    };

    // Override emitObject so that we can populate the dictionary on
    // demand.
    virtual void emitObject(SkWStream* stream,
                            const SkPDFObjNumMap& objNumMap,
                            const SkPDFSubstituteMap& substitutes);

    /** Get the graphic state for the passed SkPaint. The reference count of
     *  the object is incremented and it is the caller's responsibility to
     *  unreference it when done. This is needed to accommodate the weak
     *  reference pattern used when the returned object is new and has no
     *  other references.
     *  @param paint  The SkPaint to emulate.
     */
    static SkPDFGraphicState* GetGraphicStateForPaint(SkPDFCanon* canon,
                                                      const SkPaint& paint);

    /** Make a graphic state that only sets the passed soft mask. The
     *  reference count of the object is incremented and it is the caller's
     *  responsibility to unreference it when done.
     *  @param sMask     The form xobject to use as a soft mask.
     *  @param invert    Indicates if the alpha of the sMask should be inverted.
     *  @param sMaskMode Whether to use alpha or luminosity for the sMask.
     *
     *  These are not de-duped.
     */
    static SkPDFDict* GetSMaskGraphicState(SkPDFFormXObject* sMask,
                                           bool invert,
                                           SkPDFSMaskMode sMaskMode);

    /** Get a graphic state that only unsets the soft mask. The reference
     *  count of the object is incremented and it is the caller's responsibility
     *  to unreference it when done. This is needed to accommodate the weak
     *  reference pattern used when the returned object is new and has no
     *  other references.
     *
     *  The returned object is a singleton.
     */
    static SkPDFDict* GetNoSMaskGraphicState();

    bool operator==(const SkPDFGraphicState& rhs) const {
        return 0 == memcmp(&fStrokeWidth, &rhs.fStrokeWidth, 12);
    }
    uint32_t hash() const { return SkChecksum::Murmur3(&fStrokeWidth, 12); }

private:
    const SkScalar fStrokeWidth;
    const SkScalar fStrokeMiter;
    const uint8_t fAlpha;
    const uint8_t fStrokeCap;   // SkPaint::Cap
    const uint8_t fStrokeJoin;  // SkPaint::Join
    const uint8_t fMode;        // SkXfermode::Mode

    SkPDFGraphicState(const SkPaint&);

    typedef SkPDFDict INHERITED;
};

#endif
