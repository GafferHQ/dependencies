/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ComputedStyleConstants_h
#define ComputedStyleConstants_h

namespace blink {

// Sides used when drawing borders and outlines. The values should run clockwise from top.
enum BoxSide {
    BSTop,
    BSRight,
    BSBottom,
    BSLeft
};

enum StyleRecalcChange {
    NoChange,
    NoInherit,
    UpdatePseudoElements,
    Inherit,
    Force,
    Reattach,
    ReattachNoLayoutObject
};

static const size_t PrintColorAdjustBits = 1;
enum PrintColorAdjust {
    PrintColorAdjustEconomy,
    PrintColorAdjustExact
};

// Static pseudo styles. Dynamic ones are produced on the fly.
enum PseudoId {
    // The order must be NOP ID, public IDs, and then internal IDs.
    // If you add or remove a public ID, you must update _pseudoBits in ComputedStyle.
    NOPSEUDO, FIRST_LINE, FIRST_LETTER, BEFORE, AFTER, BACKDROP, SELECTION, FIRST_LINE_INHERITED, SCROLLBAR,
    // Internal IDs follow:
    SCROLLBAR_THUMB, SCROLLBAR_BUTTON, SCROLLBAR_TRACK, SCROLLBAR_TRACK_PIECE, SCROLLBAR_CORNER, RESIZER, INPUT_LIST_BUTTON,
    // Special values follow:
    AFTER_LAST_INTERNAL_PSEUDOID,
    FIRST_PUBLIC_PSEUDOID = FIRST_LINE,
    FIRST_INTERNAL_PSEUDOID = SCROLLBAR_THUMB,
    PUBLIC_PSEUDOID_MASK = ((1 << FIRST_INTERNAL_PSEUDOID) - 1) & ~((1 << FIRST_PUBLIC_PSEUDOID) - 1),
    PSEUDO_ELEMENT_MASK = (1 << (BEFORE - 1)) | (1 << (AFTER - 1)) | (1 << (BACKDROP - 1))
};

enum ColumnFill { ColumnFillBalance, ColumnFillAuto };

enum ColumnSpan { ColumnSpanNone = 0, ColumnSpanAll };

enum EBorderCollapse { BSEPARATE = 0, BCOLLAPSE = 1 };

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order! This order also must match the order in CSSValueKeywords.in.
enum EBorderStyle { BNONE, BHIDDEN, INSET, GROOVE, OUTSET, RIDGE, DOTTED, DASHED, SOLID, DOUBLE };

enum EBorderPrecedence { BOFF, BTABLE, BCOLGROUP, BCOL, BROWGROUP, BROW, BCELL };

enum OutlineIsAuto { AUTO_OFF = 0, AUTO_ON };

enum EPosition {
    StaticPosition = 0,
    RelativePosition = 1,
    AbsolutePosition = 2,
    // This value is required to pack our bits efficiently in LayoutObject.
    // FIXME: Is this still true now that we've remove position: sticky.
    FixedPosition = 6
};

enum EFloat {
    NoFloat, LeftFloat, RightFloat
};

enum EMarginCollapse { MCOLLAPSE, MSEPARATE, MDISCARD };

// Box decoration attributes. Not inherited.

enum EBoxDecorationBreak { DSLICE, DCLONE };

// Box attributes. Not inherited.

enum EBoxSizing { CONTENT_BOX, BORDER_BOX };

// Random visual rendering model attributes. Not inherited.

enum EOverflow {
    OVISIBLE, OHIDDEN, OSCROLL, OAUTO, OOVERLAY, OPAGEDX, OPAGEDY
};

enum EVerticalAlign {
    BASELINE, MIDDLE, SUB, SUPER, TEXT_TOP,
    TEXT_BOTTOM, TOP, BOTTOM, BASELINE_MIDDLE, LENGTH
};

enum EClear {
    CNONE = 0, CLEFT = 1, CRIGHT = 2, CBOTH = 3
};

enum ETableLayout {
    TAUTO, TFIXED
};

enum TextCombine {
    TextCombineNone, TextCombineHorizontal
};

enum EFillAttachment {
    ScrollBackgroundAttachment, LocalBackgroundAttachment, FixedBackgroundAttachment
};

enum EFillBox {
    BorderFillBox, PaddingFillBox, ContentFillBox, TextFillBox
};

enum EFillRepeat {
    RepeatFill, NoRepeatFill, RoundFill, SpaceFill
};

enum EFillLayerType {
    BackgroundFillLayer, MaskFillLayer
};

// CSS3 Background Values
enum EFillSizeType { Contain, Cover, SizeLength, SizeNone };

// CSS3 Background Position
enum BackgroundEdgeOrigin { TopEdge, RightEdge, BottomEdge, LeftEdge };

// CSS Mask Source Types
enum EMaskSourceType { MaskAlpha, MaskLuminance };

// Deprecated Flexible Box Properties

enum EBoxPack { Start, Center, End, Justify };
enum EBoxAlignment { BSTRETCH, BSTART, BCENTER, BEND, BBASELINE };
enum EBoxOrient { HORIZONTAL, VERTICAL };
enum EBoxLines { SINGLE, MULTIPLE };
enum EBoxDirection { BNORMAL, BREVERSE };

// CSS3 Flexbox Properties

enum EFlexDirection { FlowRow, FlowRowReverse, FlowColumn, FlowColumnReverse };
enum EFlexWrap { FlexNoWrap, FlexWrap, FlexWrapReverse };

enum ETextSecurity {
    TSNONE, TSDISC, TSCIRCLE, TSSQUARE
};

// CSS3 User Modify Properties

enum EUserModify {
    READ_ONLY, READ_WRITE, READ_WRITE_PLAINTEXT_ONLY
};

// CSS3 User Drag Values

enum EUserDrag {
    DRAG_AUTO, DRAG_NONE, DRAG_ELEMENT
};

// CSS3 User Select Values

enum EUserSelect {
    SELECT_NONE, SELECT_TEXT, SELECT_ALL
};

// CSS3 Image Values
enum ObjectFit { ObjectFitFill, ObjectFitContain, ObjectFitCover, ObjectFitNone, ObjectFitScaleDown };

// Word Break Values. Matches WinIE and CSS3

enum EWordBreak {
    NormalWordBreak, BreakAllWordBreak, KeepAllWordBreak, BreakWordBreak
};

enum EOverflowWrap {
    NormalOverflowWrap, BreakOverflowWrap
};

enum LineBreak {
    LineBreakAuto, LineBreakLoose, LineBreakNormal, LineBreakStrict, LineBreakAfterWhiteSpace
};

enum EResize {
    RESIZE_NONE, RESIZE_BOTH, RESIZE_HORIZONTAL, RESIZE_VERTICAL
};

// The order of this enum must match the order of the list style types in CSSValueKeywords.in.
enum EListStyleType {
    Disc,
    Circle,
    Square,
    DecimalListStyle,
    DecimalLeadingZero,
    ArabicIndic,
    Bengali,
    Cambodian,
    Khmer,
    Devanagari,
    Gujarati,
    Gurmukhi,
    Kannada,
    Lao,
    Malayalam,
    Mongolian,
    Myanmar,
    Oriya,
    Persian,
    Urdu,
    Telugu,
    Tibetan,
    Thai,
    LowerRoman,
    UpperRoman,
    LowerGreek,
    LowerAlpha,
    LowerLatin,
    UpperAlpha,
    UpperLatin,
    CjkEarthlyBranch,
    CjkHeavenlyStem,
    EthiopicHalehame,
    EthiopicHalehameAm,
    EthiopicHalehameTiEr,
    EthiopicHalehameTiEt,
    Hangul,
    HangulConsonant,
    KoreanHangulFormal,
    KoreanHanjaFormal,
    KoreanHanjaInformal,
    Hebrew,
    Armenian,
    LowerArmenian,
    UpperArmenian,
    Georgian,
    CJKIdeographic,
    SimpChineseFormal,
    SimpChineseInformal,
    TradChineseFormal,
    TradChineseInformal,
    Hiragana,
    Katakana,
    HiraganaIroha,
    KatakanaIroha,
    NoneListStyle
};

enum QuoteType {
    OPEN_QUOTE, CLOSE_QUOTE, NO_OPEN_QUOTE, NO_CLOSE_QUOTE
};

enum EAnimPlayState {
    AnimPlayStatePlaying,
    AnimPlayStatePaused
};

enum EWhiteSpace {
    NORMAL, PRE, PRE_WRAP, PRE_LINE, NOWRAP, KHTML_NOWRAP
};

// The order of this enum must match the order of the text align values in CSSValueKeywords.in.
enum ETextAlign {
    LEFT, RIGHT, CENTER, JUSTIFY, WEBKIT_LEFT, WEBKIT_RIGHT, WEBKIT_CENTER, TASTART, TAEND,
};

enum ETextTransform {
    CAPITALIZE, UPPERCASE, LOWERCASE, TTNONE
};

static const size_t TextDecorationBits = 4;
enum TextDecoration {
    TextDecorationNone = 0x0,
    TextDecorationUnderline = 0x1,
    TextDecorationOverline = 0x2,
    TextDecorationLineThrough = 0x4,
    TextDecorationBlink = 0x8
};
inline TextDecoration operator| (TextDecoration a, TextDecoration b) { return TextDecoration(int(a) | int(b)); }
inline TextDecoration& operator|= (TextDecoration& a, TextDecoration b) { return a = a | b; }

enum TextDecorationStyle {
    TextDecorationStyleSolid,
    TextDecorationStyleDouble,
    TextDecorationStyleDotted,
    TextDecorationStyleDashed,
    TextDecorationStyleWavy
};

enum TextAlignLast {
    TextAlignLastAuto, TextAlignLastStart, TextAlignLastEnd, TextAlignLastLeft, TextAlignLastRight, TextAlignLastCenter, TextAlignLastJustify
};

enum TextUnderlinePosition {
    // FIXME: Implement support for 'under left' and 'under right' values.
    TextUnderlinePositionAuto,
    TextUnderlinePositionUnder
};

enum EPageBreak {
    PBAUTO, PBALWAYS, PBAVOID
};

enum EEmptyCell {
    SHOW, HIDE
};

enum ECaptionSide {
    CAPTOP, CAPBOTTOM, CAPLEFT, CAPRIGHT
};

enum EListStylePosition { OUTSIDE, INSIDE };

enum EVisibility { VISIBLE, HIDDEN, COLLAPSE };

enum ECursor {
    // The following must match the order in CSSValueKeywords.in.
    CURSOR_AUTO,
    CURSOR_CROSS,
    CURSOR_DEFAULT,
    CURSOR_POINTER,
    CURSOR_MOVE,
    CURSOR_VERTICAL_TEXT,
    CURSOR_CELL,
    CURSOR_CONTEXT_MENU,
    CURSOR_ALIAS,
    CURSOR_PROGRESS,
    CURSOR_NO_DROP,
    CURSOR_NOT_ALLOWED,
    CURSOR_ZOOM_IN,
    CURSOR_ZOOM_OUT,
    CURSOR_E_RESIZE,
    CURSOR_NE_RESIZE,
    CURSOR_NW_RESIZE,
    CURSOR_N_RESIZE,
    CURSOR_SE_RESIZE,
    CURSOR_SW_RESIZE,
    CURSOR_S_RESIZE,
    CURSOR_W_RESIZE,
    CURSOR_EW_RESIZE,
    CURSOR_NS_RESIZE,
    CURSOR_NESW_RESIZE,
    CURSOR_NWSE_RESIZE,
    CURSOR_COL_RESIZE,
    CURSOR_ROW_RESIZE,
    CURSOR_TEXT,
    CURSOR_WAIT,
    CURSOR_HELP,
    CURSOR_ALL_SCROLL,
    CURSOR_WEBKIT_GRAB,
    CURSOR_WEBKIT_GRABBING,

    // The following are handled as exceptions so don't need to match.
    CURSOR_COPY,
    CURSOR_NONE
};

// The order of this enum must match the order of the display values in CSSValueKeywords.in.
enum EDisplay {
    INLINE, BLOCK, LIST_ITEM, INLINE_BLOCK,
    TABLE, INLINE_TABLE, TABLE_ROW_GROUP,
    TABLE_HEADER_GROUP, TABLE_FOOTER_GROUP, TABLE_ROW,
    TABLE_COLUMN_GROUP, TABLE_COLUMN, TABLE_CELL,
    TABLE_CAPTION, BOX, INLINE_BOX,
    FLEX, INLINE_FLEX,
    GRID, INLINE_GRID,
    NONE,
    FIRST_TABLE_DISPLAY = TABLE,
    LAST_TABLE_DISPLAY = TABLE_CAPTION
};

enum EInsideLink {
    NotInsideLink, InsideUnvisitedLink, InsideVisitedLink
};

enum EPointerEvents {
    PE_NONE, PE_AUTO, PE_STROKE, PE_FILL, PE_PAINTED, PE_VISIBLE,
    PE_VISIBLE_STROKE, PE_VISIBLE_FILL, PE_VISIBLE_PAINTED, PE_BOUNDINGBOX,
    PE_ALL
};

enum ETransformStyle3D {
    TransformStyle3DFlat, TransformStyle3DPreserve3D
};

enum MotionRotationType { MotionRotationAuto, MotionRotationFixed };

enum EBackfaceVisibility {
    BackfaceVisibilityVisible, BackfaceVisibilityHidden
};

enum ELineClampType { LineClampLineCount, LineClampPercentage };

enum Hyphens { HyphensNone, HyphensManual, HyphensAuto };

enum ESpeak { SpeakNone, SpeakNormal, SpeakSpellOut, SpeakDigits, SpeakLiteralPunctuation, SpeakNoPunctuation };

enum TextEmphasisFill { TextEmphasisFillFilled, TextEmphasisFillOpen };

enum TextEmphasisMark { TextEmphasisMarkNone, TextEmphasisMarkAuto, TextEmphasisMarkDot, TextEmphasisMarkCircle, TextEmphasisMarkDoubleCircle, TextEmphasisMarkTriangle, TextEmphasisMarkSesame, TextEmphasisMarkCustom };

enum TextEmphasisPosition { TextEmphasisPositionOver, TextEmphasisPositionUnder };

enum TextOrientation { TextOrientationVerticalRight, TextOrientationUpright, TextOrientationSideways, TextOrientationSidewaysRight };

enum TextOverflow { TextOverflowClip = 0, TextOverflowEllipsis };

enum EImageRendering { ImageRenderingAuto, ImageRenderingOptimizeSpeed, ImageRenderingOptimizeQuality, ImageRenderingOptimizeContrast, ImageRenderingPixelated };

enum ImageResolutionSource { ImageResolutionSpecified = 0, ImageResolutionFromImage };

enum ImageResolutionSnap { ImageResolutionNoSnap = 0, ImageResolutionSnapPixels };

enum Order { LogicalOrder = 0, VisualOrder };

enum WrapFlow { WrapFlowAuto, WrapFlowBoth, WrapFlowStart, WrapFlowEnd, WrapFlowMaximum, WrapFlowClear };

enum WrapThrough { WrapThroughWrap, WrapThroughNone };

enum RubyPosition { RubyPositionBefore, RubyPositionAfter };

static const size_t GridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
    InternalAutoFlowAlgorithmSparse = 0x1,
    InternalAutoFlowAlgorithmDense = 0x2
};

enum InternalGridAutoFlowDirection {
    InternalAutoFlowDirectionRow = 0x4,
    InternalAutoFlowDirectionColumn = 0x8
};

enum GridAutoFlow {
    AutoFlowRow = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionRow,
    AutoFlowColumn = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionColumn,
    AutoFlowRowDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionRow,
    AutoFlowColumnDense = InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionColumn
};

enum DraggableRegionMode { DraggableRegionNone, DraggableRegionDrag, DraggableRegionNoDrag };

static const size_t TouchActionBits = 6;
enum TouchAction {
    TouchActionAuto = 0x0,
    TouchActionNone = 0x1,
    TouchActionPanLeft = 0x2,
    TouchActionPanRight = 0x4,
    TouchActionPanX = TouchActionPanLeft | TouchActionPanRight,
    TouchActionPanUp = 0x8,
    TouchActionPanDown = 0x10,
    TouchActionPanY = TouchActionPanUp | TouchActionPanDown,
    TouchActionPinchZoom = 0x20,
};
inline TouchAction operator| (TouchAction a, TouchAction b) { return TouchAction(int(a) | int(b)); }
inline TouchAction& operator|= (TouchAction& a, TouchAction b) { return a = a | b; }
inline TouchAction operator& (TouchAction a, TouchAction b) { return TouchAction(int(a) & int(b)); }
inline TouchAction& operator&= (TouchAction& a, TouchAction b) { return a = a & b; }

enum EIsolation { IsolationAuto, IsolationIsolate };

enum ItemPosition {
    ItemPositionAuto,
    ItemPositionStretch,
    ItemPositionBaseline,
    ItemPositionLastBaseline,
    ItemPositionCenter,
    ItemPositionStart,
    ItemPositionEnd,
    ItemPositionSelfStart,
    ItemPositionSelfEnd,
    ItemPositionFlexStart,
    ItemPositionFlexEnd,
    ItemPositionLeft,
    ItemPositionRight
};

enum OverflowAlignment {
    OverflowAlignmentDefault,
    OverflowAlignmentTrue,
    OverflowAlignmentSafe
};

enum ItemPositionType {
    NonLegacyPosition,
    LegacyPosition
};

enum ContentPosition {
    ContentPositionAuto,
    ContentPositionBaseline,
    ContentPositionLastBaseline,
    ContentPositionCenter,
    ContentPositionStart,
    ContentPositionEnd,
    ContentPositionFlexStart,
    ContentPositionFlexEnd,
    ContentPositionLeft,
    ContentPositionRight
};

enum ContentDistributionType {
    ContentDistributionDefault,
    ContentDistributionSpaceBetween,
    ContentDistributionSpaceAround,
    ContentDistributionSpaceEvenly,
    ContentDistributionStretch
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some platforms (such as Windows).
static const float maximumAllowedFontSize = 1000000.0f;

enum TextIndentLine { TextIndentFirstLine, TextIndentEachLine };
enum TextIndentType { TextIndentNormal, TextIndentHanging };

enum CSSBoxType { BoxMissing = 0, MarginBox, BorderBox, PaddingBox, ContentBox };

enum ScrollSnapType {
    ScrollSnapTypeNone,
    ScrollSnapTypeMandatory,
    ScrollSnapTypeProximity
};

} // namespace blink

#endif // ComputedStyleConstants_h
