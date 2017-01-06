/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QS60STYLE_P_H
#define QS60STYLE_P_H

#include "qs60style.h"
#include "qcommonstyle_p.h"
#include <QtCore/qhash.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

const int MAX_NON_CUSTOM_PIXELMETRICS = 92;
const int CUSTOMVALUESCOUNT = 7;

const int MAX_PIXELMETRICS = MAX_NON_CUSTOM_PIXELMETRICS + CUSTOMVALUESCOUNT;

typedef struct {
    unsigned short height;
    unsigned short width;
    int major_version;
    int minor_version;
    const char* layoutName;
} layoutHeader;

#ifdef Q_OS_SYMBIAN
NONSHARABLE_CLASS (QS60StyleEnums)
#else
class QS60StyleEnums
#endif
: public QObject
{
#ifndef Q_WS_S60
    Q_OBJECT
    Q_ENUMS(FontCategories)
    Q_ENUMS(SkinParts)
    Q_ENUMS(ColorLists)
#endif // !Q_WS_S60

public:

    // S60 definitions within theme
    enum ThemeDefinitions {
        TD_AnimationData,
    };

    //Defines which values are contained within animation data (retrieved using TD_AnimationData).
    //Additionally defines the order in which the items are given out in QList<QVariant>.
    enum AnimationData {
        AD_Interval = 0,
        AD_NumberOfFrames,
        AD_AnimationPlayMode,  //currently not used as themes seem to contain invalid data
    };

    // Animation modes
    enum AnimationMode {
        AM_PlayOnce = 0, //animation is played exactly once
        AM_Looping,      //animation is repeated until stopped
        AM_Bounce        //animation is played repeatedly until stopped,
                         //but frames are played in reverse order every second time
                         //(no support yet)
    };

    // S60 look-and-feel font categories
    enum FontCategories {
        FC_Undefined,
        FC_Primary,
        FC_Secondary,
        FC_Title,
        FC_PrimarySmall,
        FC_Digital
    };

    enum SkinParts {
        SP_QgnGrafBarWaitAnim,
        SP_QgnGrafBarFrameCenter,
        SP_QgnGrafBarFrameSideL,
        SP_QgnGrafBarFrameSideR,
        SP_QgnGrafBarProgress,
        SP_QgnGrafOrgBgGrid,
        SP_QgnGrafScrollArrowDown,
        SP_QgnGrafScrollArrowLeft,
        SP_QgnGrafScrollArrowRight,
        SP_QgnGrafScrollArrowUp,
        SP_QgnGrafTabActiveL,
        SP_QgnGrafTabActiveM,
        SP_QgnGrafTabActiveR,
        SP_QgnGrafTabPassiveL,
        SP_QgnGrafTabPassiveM,
        SP_QgnGrafTabPassiveR,
        SP_QgnGrafNsliderEndLeft,
        SP_QgnGrafNsliderEndRight,
        SP_QgnGrafNsliderMiddle,
        SP_QgnIndiCheckboxOff,
        SP_QgnIndiCheckboxOn,
        SP_QgnIndiHlColSuper,
        SP_QgnIndiHlExpSuper,
        SP_QgnIndiHlLineBranch,
        SP_QgnIndiHlLineEnd,
        SP_QgnIndiHlLineStraight,
        SP_QgnIndiMarkedAdd,
        SP_QgnIndiNaviArrowLeft,
        SP_QgnIndiNaviArrowRight,
        SP_QgnIndiRadiobuttOff,
        SP_QgnIndiRadiobuttOn,
        SP_QgnGrafNsliderMarker,
        SP_QgnGrafNsliderMarkerSelected,
        SP_QgnIndiSubmenu,
        SP_QgnNoteErased,
        SP_QgnNoteError,
        SP_QgnNoteInfo,
        SP_QgnNoteOk,
        SP_QgnNoteQuery,
        SP_QgnNoteWarning,
        SP_QgnPropFileSmall,
        SP_QgnPropFolderCurrent,
        SP_QgnPropFolderSmall,
        SP_QgnPropFolderSmallNew,
        SP_QgnPropPhoneMemcLarge,
        SP_QgnFrSctrlButtonCornerTl,        // Toolbar button
        SP_QgnFrSctrlButtonCornerTr,
        SP_QgnFrSctrlButtonCornerBl,
        SP_QgnFrSctrlButtonCornerBr,
        SP_QgnFrSctrlButtonSideT,
        SP_QgnFrSctrlButtonSideB,
        SP_QgnFrSctrlButtonSideL,
        SP_QgnFrSctrlButtonSideR,
        SP_QgnFrSctrlButtonCenter,
        SP_QgnFrSctrlButtonCornerTlPressed,    // Toolbar button, pressed
        SP_QgnFrSctrlButtonCornerTrPressed,
        SP_QgnFrSctrlButtonCornerBlPressed,
        SP_QgnFrSctrlButtonCornerBrPressed,
        SP_QgnFrSctrlButtonSideTPressed,
        SP_QgnFrSctrlButtonSideBPressed,
        SP_QgnFrSctrlButtonSideLPressed,
        SP_QgnFrSctrlButtonSideRPressed,
        SP_QgnFrSctrlButtonCenterPressed,
        SP_QsnCpScrollHandleBottomPressed, //ScrollBar handle, pressed state
        SP_QsnCpScrollHandleMiddlePressed,
        SP_QsnCpScrollHandleTopPressed,
        SP_QsnBgScreen,
        SP_QsnCpScrollBgBottom,
        SP_QsnCpScrollBgMiddle,
        SP_QsnCpScrollBgTop,
        SP_QsnCpScrollHandleBottom,
        SP_QsnCpScrollHandleMiddle,
        SP_QsnCpScrollHandleTop,
        SP_QsnFrButtonTbCornerTl,           // Button, normal state
        SP_QsnFrButtonTbCornerTr,
        SP_QsnFrButtonTbCornerBl,
        SP_QsnFrButtonTbCornerBr,
        SP_QsnFrButtonTbSideT,
        SP_QsnFrButtonTbSideB,
        SP_QsnFrButtonTbSideL,
        SP_QsnFrButtonTbSideR,
        SP_QsnFrButtonTbCenter,
        SP_QsnFrButtonTbCornerTlPressed,    // Button, pressed state
        SP_QsnFrButtonTbCornerTrPressed,
        SP_QsnFrButtonTbCornerBlPressed,
        SP_QsnFrButtonTbCornerBrPressed,
        SP_QsnFrButtonTbSideTPressed,
        SP_QsnFrButtonTbSideBPressed,
        SP_QsnFrButtonTbSideLPressed,
        SP_QsnFrButtonTbSideRPressed,
        SP_QsnFrButtonTbCenterPressed,
        SP_QsnFrCaleCornerTl,               // calendar grid item
        SP_QsnFrCaleCornerTr,
        SP_QsnFrCaleCornerBl,
        SP_QsnFrCaleCornerBr,
        SP_QsnFrCaleSideT,
        SP_QsnFrCaleSideB,
        SP_QsnFrCaleSideL,
        SP_QsnFrCaleSideR,
        SP_QsnFrCaleCenter,
        SP_QsnFrCaleHeadingCornerTl,        // calendar grid header
        SP_QsnFrCaleHeadingCornerTr,
        SP_QsnFrCaleHeadingCornerBl,
        SP_QsnFrCaleHeadingCornerBr,
        SP_QsnFrCaleHeadingSideT,
        SP_QsnFrCaleHeadingSideB,
        SP_QsnFrCaleHeadingSideL,
        SP_QsnFrCaleHeadingSideR,
        SP_QsnFrCaleHeadingCenter,
        SP_QsnFrInputCornerTl,              // Text input field
        SP_QsnFrInputCornerTr,
        SP_QsnFrInputCornerBl,
        SP_QsnFrInputCornerBr,
        SP_QsnFrInputSideT,
        SP_QsnFrInputSideB,
        SP_QsnFrInputSideL,
        SP_QsnFrInputSideR,
        SP_QsnFrInputCenter,
        SP_QsnFrListCornerTl,               // List background
        SP_QsnFrListCornerTr,
        SP_QsnFrListCornerBl,
        SP_QsnFrListCornerBr,
        SP_QsnFrListSideT,
        SP_QsnFrListSideB,
        SP_QsnFrListSideL,
        SP_QsnFrListSideR,
        SP_QsnFrListCenter,
        SP_QsnFrPopupCornerTl,              // Option menu background
        SP_QsnFrPopupCornerTr,
        SP_QsnFrPopupCornerBl,
        SP_QsnFrPopupCornerBr,
        SP_QsnFrPopupSideT,
        SP_QsnFrPopupSideB,
        SP_QsnFrPopupSideL,
        SP_QsnFrPopupSideR,
        SP_QsnFrPopupCenter,
        SP_QsnFrPopupPreviewCornerTl,       // tool tip background
        SP_QsnFrPopupPreviewCornerTr,
        SP_QsnFrPopupPreviewCornerBl,
        SP_QsnFrPopupPreviewCornerBr,
        SP_QsnFrPopupPreviewSideT,
        SP_QsnFrPopupPreviewSideB,
        SP_QsnFrPopupPreviewSideL,
        SP_QsnFrPopupPreviewSideR,
        SP_QsnFrPopupPreviewCenter,
        SP_QsnFrSetOptCornerTl,             // Settings list
        SP_QsnFrSetOptCornerTr,
        SP_QsnFrSetOptCornerBl,
        SP_QsnFrSetOptCornerBr,
        SP_QsnFrSetOptSideT,
        SP_QsnFrSetOptSideB,
        SP_QsnFrSetOptSideL,
        SP_QsnFrSetOptSideR,
        SP_QsnFrSetOptCenter,
        SP_QsnFrPopupSubCornerTl,           // Toolbar background
        SP_QsnFrPopupSubCornerTr,
        SP_QsnFrPopupSubCornerBl,
        SP_QsnFrPopupSubCornerBr,
        SP_QsnFrPopupSubSideT,
        SP_QsnFrPopupSubSideB,
        SP_QsnFrPopupSubSideL,
        SP_QsnFrPopupSubSideR,
        SP_QsnFrPopupSubCenter,
        SP_QsnFrButtonCornerTlInactive,     // Inactive button
        SP_QsnFrButtonCornerTrInactive,
        SP_QsnFrButtonCornerBlInactive,
        SP_QsnFrButtonCornerBrInactive,
        SP_QsnFrButtonSideTInactive,
        SP_QsnFrButtonSideBInactive,
        SP_QsnFrButtonSideLInactive,
        SP_QsnFrButtonSideRInactive,
        SP_QsnFrButtonCenterInactive,
        SP_QsnFrGridCornerTlPressed, // Pressed table item
        SP_QsnFrGridCornerTrPressed,
        SP_QsnFrGridCornerBlPressed,
        SP_QsnFrGridCornerBrPressed,
        SP_QsnFrGridSideTPressed,
        SP_QsnFrGridSideBPressed,
        SP_QsnFrGridSideLPressed,
        SP_QsnFrGridSideRPressed,
        SP_QsnFrGridCenterPressed,
        SP_QsnFrListCornerTlPressed,  // Pressed list item
        SP_QsnFrListCornerTrPressed,
        SP_QsnFrListCornerBlPressed,
        SP_QsnFrListCornerBrPressed,
        SP_QsnFrListSideTPressed,
        SP_QsnFrListSideBPressed,
        SP_QsnFrListSideLPressed,
        SP_QsnFrListSideRPressed,
        SP_QsnFrListCenterPressed,
        SP_QtgToolBarAdd,
        SP_QtgToolBarAddDetail,
        SP_QtgToolBarAgain,
        SP_QtgToolBarAgenda,
        SP_QtgToolBarAudioOff,
        SP_QtgToolBarAudioOn,
        SP_QtgToolBarBack,
        SP_QtgToolBarBluetoothOff,
        SP_QtgToolBarBluetoothOn,
        SP_QtgToolBarCancel,
        SP_QtgToolBarDelete,
        SP_QtgToolBarDetails,
        SP_QtgToolBarDone,
        SP_QtgToolBarEdit,
        SP_QtgToolBarEditDisabled,
        SP_QtgToolBarEmailSend,
        SP_QtgToolBarEmergencyCall,
        SP_QtgToolBarFavouriteAdd,
        SP_QtgToolBarFavouriteRemove,
        SP_QtgToolBarFavourites,
        SP_QtgToolBarForward,
        SP_QtgToolBarGo,
        SP_QtgToolBarHome,
        SP_QtgToolBarImageTools,
        SP_QtgToolBarList,
        SP_QtgToolBarLock,
        SP_QtgToolBarLogs,
        SP_QtgToolBarMenu,
        SP_QtgToolBarNewContact,
        SP_QtgToolBarNewGroup,
        SP_QtgToolBarNext,
        SP_QtgToolBarNextFrame,
        SP_QtgToolBarNowPlay,
        SP_QtgToolBarOptions,
        SP_QtgToolBarOther,
        SP_QtgToolBarOvi,
        SP_QtgToolBarPause,
        SP_QtgToolBarPlay,
        SP_QtgToolBarPrevious,
        SP_QtgToolBarPreviousFrame,
        SP_QtgToolBarRead,
        SP_QtgToolBarRedo,
        SP_QtgToolBarRedoDisabled,
        SP_QtgToolBarRefresh,
        SP_QtgToolBarRemoveDetail,
        SP_QtgToolBarRemoveDisabled,
        SP_QtgToolBarRepeat,
        SP_QtgToolBarRepeatOff,
        SP_QtgToolBarRepeatOne,
        SP_QtgToolBarRewind,
        SP_QtgToolBarSearch,
        SP_QtgToolBarSearchDisabled,
        SP_QtgToolBarSelectContent,
        SP_QtgToolBarSelfTimer,
        SP_QtgToolBarSend,
        SP_QtgToolBarSendDimmed,
        SP_QtgToolBarShare,
        SP_QtgToolBarShift,
        SP_QtgToolBarShuffle,
        SP_QtgToolBarShuffleOff,
        SP_QtgToolBarSignalOff,
        SP_QtgToolBarSignalOn,
        SP_QtgToolBarStop,
        SP_QtgToolBarSync,
        SP_QtgToolBarTools,
        SP_QtgToolBarTrim,
        SP_QtgToolBarUnlock,
        SP_QtgToolBarUnmark,
        SP_QtgToolBarView,
        SP_QtgToolBarWlanOff,
        SP_QtgToolBarWlanOn,
        SP_QtgGrafCameraButtonCaptureNormal,
        SP_QtgGrafCameraButtonCapturePressed,
        SP_QtgGrafCameraButtonPauseNormal,
        SP_QtgGrafCameraButtonPausePressed,
        SP_QtgGrafCameraButtonPlayNormal,
        SP_QtgGrafCameraButtonPlayPressed,
        SP_QtgGrafCameraButtonRecNormal,
        SP_QtgGrafCameraButtonRecPressed,
        SP_QtgGrafCameraButtonStopNormal,
        SP_QtgGrafCameraButtonStopPressed,
        SP_QtgTabAll,
        SP_QtgTabArtist,
        SP_QtgTabFavourite,
        SP_QtgTabGenre,
        SP_QtgTabLanguage,
        SP_QtgTabMusicAlbum,
        SP_QtgTabPhotosAlbum,
        SP_QtgTabPhotosAll,
        SP_QtgTabPlaylist,
        SP_QtgTabServices,
        SP_QtgTabSongs,
        SP_QtgTabVideos,
        SP_QgnIndiBrowserTbReload,
        SP_QgnIndiBrowserTbHome,
        SP_QgnIndiBrowserTbStop,
    };

    enum ColorLists {
        CL_QsnHighlightColors,
        CL_QsnIconColors,
        CL_QsnLineColors,
        CL_QsnOtherColors,
        CL_QsnParentColors,
        CL_QsnTextColors
    };
};

#ifdef Q_WS_S60
class CAknBitmapAnimation;
NONSHARABLE_CLASS (AnimationData) : public QObject
{
public:
    AnimationData(const QS60StyleEnums::SkinParts part, int frames, int interval);

    const QS60StyleEnums::SkinParts m_id;
    int m_frames;
    int m_interval;
    QS60StyleEnums::AnimationMode m_mode;
};


NONSHARABLE_CLASS (AnimationDataV2) : public AnimationData
{
public:
    AnimationDataV2(const AnimationData &data);
    ~AnimationDataV2();

    CAknBitmapAnimation *m_animation;
    int m_currentFrame;
    bool m_resourceBased;
    int m_timerId;
};


class QS60StyleAnimation : public QObject
{
public:
    QS60StyleAnimation(const QS60StyleEnums::SkinParts part, int frames, int interval);
    ~QS60StyleAnimation();

public:
    QS60StyleEnums::SkinParts animationId() const {return m_currentData->m_id;}
    int frameCount() const { return m_currentData->m_frames;}
    int interval() const {return m_currentData->m_interval;}
    QS60StyleEnums::AnimationMode playMode() const {return m_currentData->m_mode;}
    CAknBitmapAnimation* animationObject() const {return m_currentData->m_animation;}
    bool isResourceBased() const {return m_currentData->m_resourceBased;}
    int timerId() const {return m_currentData->m_timerId;}
    int currentFrame() const {return m_currentData->m_currentFrame;}

    void setFrameCount(int frameCount) {m_currentData->m_frames = frameCount;}
    void setInterval(int interval) {m_currentData->m_interval = interval;}
    void setAnimationObject(CAknBitmapAnimation* animation);
    void setResourceBased(bool resourceBased) {m_currentData->m_resourceBased = resourceBased;}
    void setTimerId(int timerId) {m_currentData->m_timerId = timerId;}
    void setCurrentFrame(int currentFrame) {m_currentData->m_currentFrame = currentFrame;}

    void resetToDefaults();

private: //data members
    //TODO: consider changing these to non-pointers as the classes are rather small anyway
    QScopedPointer<AnimationData> m_defaultData;
    QScopedPointer<AnimationDataV2> m_currentData;
};

#endif //Q_WS_S60


class QFocusFrame;
class QProgressBar;
class QS60StyleAnimation;

// Private class
#ifdef Q_OS_SYMBIAN
NONSHARABLE_CLASS (QS60StylePrivate)
#else
class QS60StylePrivate
#endif
: public QCommonStylePrivate
{
    Q_DECLARE_PUBLIC(QS60Style)

public:
    QS60StylePrivate();
    ~QS60StylePrivate();

    enum SkinElements {
        SE_ButtonNormal,
        SE_ButtonPressed,
        SE_FrameLineEdit,
        SE_ProgressBarGrooveHorizontal,
        SE_ProgressBarIndicatorHorizontal,
        SE_ProgressBarGrooveVertical,
        SE_ProgressBarIndicatorVertical,
        SE_ScrollBarGrooveHorizontal,
        SE_ScrollBarGrooveVertical,
        SE_ScrollBarHandleHorizontal,
        SE_ScrollBarHandleVertical,
        SE_SliderHandleHorizontal,
        SE_SliderHandleVertical,
        SE_SliderHandleSelectedHorizontal,
        SE_SliderHandleSelectedVertical,
        SE_SliderGrooveVertical,
        SE_SliderGrooveHorizontal,
        SE_TabBarTabEastActive,
        SE_TabBarTabEastInactive,
        SE_TabBarTabNorthActive,
        SE_TabBarTabNorthInactive,
        SE_TabBarTabSouthActive,
        SE_TabBarTabSouthInactive,
        SE_TabBarTabWestActive,
        SE_TabBarTabWestInactive,
        SE_ListHighlight,
        SE_PopupBackground,
        SE_SettingsList,
        SE_TableItem,
        SE_TableHeaderItem,
        SE_ToolTip, //own graphic available on 3.2+ releases,
        SE_ToolBar,
        SE_ToolBarButton,
        SE_ToolBarButtonPressed,
        SE_PanelBackground,
        SE_ScrollBarHandlePressedHorizontal,
        SE_ScrollBarHandlePressedVertical,
        SE_ButtonInactive,
        SE_Editor,
        SE_DropArea,
        SE_TableItemPressed,
        SE_ListItemPressed,
        SE_DialogBackground,
    };

    enum SkinFrameElements {
        SF_ButtonNormal,
        SF_ButtonPressed,
        SF_FrameLineEdit,
        SF_ListHighlight,
        SF_PopupBackground,
        SF_SettingsList,
        SF_TableItem,
        SF_TableHeaderItem,
        SF_ToolTip,
        SF_ToolBar,
        SF_ToolBarButton,
        SF_ToolBarButtonPressed,
        SF_PanelBackground,
        SF_ButtonInactive,
        SF_TableItemPressed,
        SF_ListItemPressed,
        SF_DialogBackground,
    };

    enum SkinElementFlag {
        SF_PointNorth =       0x0001, // North = the default
        SF_PointEast =        0x0002,
        SF_PointSouth =       0x0004,
        SF_PointWest =        0x0008,

        SF_StateEnabled =     0x0010, // Enabled = the default
        SF_StateDisabled =    0x0020,
        SF_ColorSkinned =     0x0040, // pixmap is colored with foreground pen color
        SF_Animation =        0x0080,
        SF_Mirrored_X_Axis =  0x0100,
        SF_Mirrored_Y_Axis =  0x0200
    };

    enum CacheClearReason {
        CC_UndefinedChange = 0,
        CC_LayoutChange,
        CC_ThemeChange
    };

    Q_DECLARE_FLAGS(SkinElementFlags, SkinElementFlag)

    // draws skin element
    static void drawSkinElement(SkinElements element, QPainter *painter,
        const QRect &rect, SkinElementFlags flags = KDefaultSkinElementFlags);
    // draws a specific skin part
    static void drawSkinPart(QS60StyleEnums::SkinParts part, QPainter *painter,
        const QRect &rect, SkinElementFlags flags = KDefaultSkinElementFlags);
    // gets pixel metrics value
    static short pixelMetric(int metric);
    // gets color. 'index' is NOT 0-based.
    // It corresponds to the enum key 1-based numbers of TAknsQsnXYZColorsIndex, not the values.
    static QColor s60Color(QS60StyleEnums::ColorLists list,
        int index, const QStyleOption *option);
    // gets state specific color
    static QColor stateColor(const QColor &color, const QStyleOption *option);
    // gets lighter color than base color
    static QColor lighterColor(const QColor &baseColor);
    //deduces if the given widget should have separately themeable background
    static bool drawsOwnThemeBackground(const QWidget *widget);

    QFont s60Font(QS60StyleEnums::FontCategories fontCategory,
        int pointSize = -1, bool resolveFontSize = true) const;
    // clears all style caches (fonts, colors, pixmaps)
    void clearCaches(CacheClearReason reason = CC_UndefinedChange);

    // themed main background oprations
    void setBackgroundTexture(QApplication *application) const;
    static void deleteBackground();

    static bool isSingleClickUi();
    static bool isWidgetPressed(const QWidget *widget);

#ifdef Q_WS_S60
    static void deleteStoredSettings();
    // calculates average color based on theme graphics (minus borders).
    QColor colorFromFrameGraphics(SkinFrameElements frame) const;
#endif
    QColor calculatedColor(SkinFrameElements frame) const;

    //set theme palette for application
    void setThemePalette(QApplication *application) const;
    //access to theme palette
    static QPalette* themePalette();

    static const layoutHeader m_layoutHeaders[];
    static const short data[][MAX_PIXELMETRICS];

    void setCurrentLayout(int layoutIndex);
    void setActiveLayout();
    // Pointer
    static short const *m_pmPointer;
    // number of layouts supported by the style
    static const int m_numberOfLayouts;

    mutable QHash<QPair<QS60StyleEnums::FontCategories , int>, QFont> m_mappedFontsCache;

    // Has one entry per SkinFrameElements
    static const struct frameElementCenter {
        SkinElements element;
        QS60StyleEnums::SkinParts center;
    } m_frameElementsData[];

    static QPixmap frame(SkinFrameElements frame, const QSize &size,
        SkinElementFlags flags = KDefaultSkinElementFlags);
    static QPixmap backgroundTexture(bool skipCreation = false);
    static QPixmap placeHolderTexture();

#ifdef Q_WS_S60
    void handleDynamicLayoutVariantSwitch();
    void handleSkinChange();
#endif // Q_WS_S60

    //Checks that the current brush is transparent or has BrushStyle NoBrush,
    //so that theme graphic background can be drawn.
    static bool canDrawThemeBackground(const QBrush &backgroundBrush, const QWidget *widget);

    static int currentAnimationFrame(QS60StyleEnums::SkinParts part);
#ifdef Q_WS_S60

    //No support for animations on emulated style
    void startAnimation(QS60StyleEnums::SkinParts animation);
    void stopAnimation(QS60StyleEnums::SkinParts animation);
    static QS60StyleAnimation* animationDefinition(QS60StyleEnums::SkinParts part);
    static void removeAnimations();

#endif

private:
    static void drawPart(QS60StyleEnums::SkinParts part, QPainter *painter,
        const QRect &rect, SkinElementFlags flags = KDefaultSkinElementFlags);
    static void drawRow(QS60StyleEnums::SkinParts start, QS60StyleEnums::SkinParts middle,
        QS60StyleEnums::SkinParts end, Qt::Orientation orientation, QPainter *painter,
        const QRect &rect, SkinElementFlags flags = KDefaultSkinElementFlags);
    static void drawFrame(SkinFrameElements frame, QPainter *painter,
        const QRect &rect, SkinElementFlags flags = KDefaultSkinElementFlags);

    static QPixmap cachedPart(QS60StyleEnums::SkinParts part, const QSize &size,
        QPainter *painter, SkinElementFlags flags = KDefaultSkinElementFlags);
    static QPixmap cachedFrame(SkinFrameElements frame, const QSize &size,
        SkinElementFlags flags = KDefaultSkinElementFlags);

    // set S60 font for widget
    void setFont(QWidget *widget) const;
    static void setThemePalette(QWidget *widget);
    void setThemePalette(QPalette *palette) const;
    static void setThemePaletteHash(QPalette *palette);
    static void storeThemePalette(QPalette *palette);
    static void deleteThemePalette();
    static bool equalToThemePalette(QColor color, QPalette::ColorRole role);
    static bool equalToThemePalette(qint64 cacheKey, QPalette::ColorRole role);

    static QSize partSize(QS60StyleEnums::SkinParts part,
        SkinElementFlags flags = KDefaultSkinElementFlags);
    static QPixmap part(QS60StyleEnums::SkinParts part, const QSize &size,
        QPainter *painter, SkinElementFlags flags = KDefaultSkinElementFlags);

    static QFont s60Font_specific(QS60StyleEnums::FontCategories fontCategory,
                                  int pointSize, bool resolveFontSize);

    static QSize screenSize();

    // Contains background texture.
    static QPixmap *m_background;
    // Placeholder pixmap for the real background texture.
    static QPixmap *m_placeHolderTexture;

    const static SkinElementFlags KDefaultSkinElementFlags;
    // defined theme palette
    static QPalette *m_themePalette;
    QPalette m_originalPalette;

    QPointer<QFocusFrame> m_focusFrame;
    static qint64 m_webPaletteKey;

    static QPointer<QWidget> m_pressedWidget;

#ifdef Q_WS_S60
    //list of progress bars having animation running
    QList<QProgressBar *> m_bars;
#endif

};

QT_END_NAMESPACE

#endif // QS60STYLE_P_H
