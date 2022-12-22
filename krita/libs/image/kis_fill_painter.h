/*
 *  SPDX-FileCopyrightText: 2004 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2004 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_FILL_PAINTER_H_
#define KIS_FILL_PAINTER_H_

#include <QRect>

#include <KoColor.h>
#include <KoColorSpaceRegistry.h>
#include <KoPattern.h>

#include "kis_painter.h"
#include "kis_types.h"
#include "kis_selection.h"

#include <kritaimage_export.h>


class KisFilterConfiguration;

// XXX: Filling should set dirty rect.
/**
 * This painter can be used to fill paint devices in different ways. This can also be used
 * for flood filling related operations.
 */
class KRITAIMAGE_EXPORT KisFillPainter : public KisPainter
{

public:

    /**
     * Construct an empty painter. Use the begin(KisPaintDeviceSP) method to attach
     * to a paint device
     */
    KisFillPainter();
    /**
     * Start painting on the specified paint device
     */
    KisFillPainter(KisPaintDeviceSP device);

    KisFillPainter(KisPaintDeviceSP device, KisSelectionSP selection);

private:

    void initFillPainter();

public:
    /**
     * Fill current selection of KisPainter with a specified \p color.
     *
     * The filling rect is limited by \p rc to allow multithreaded
     * filling/processing.
     */
    void fillSelection(const QRect &rc, const KoColor &color);

    /**
     * Fill a rectangle with a certain color and opacity.
     */
    void fillRect(qint32 x,
                  qint32 y,
                  qint32 w,
                  qint32 h,
                  const KoColor &c,
                  quint8 opacity);

    /**
     * Overloaded version of the above function.
     */
    inline void fillRect(const QRect &rc, const KoColor &c, quint8 opacity)
    {
        fillRect(rc.x(), rc.y(), rc.width(), rc.height(), c, opacity);
    }

    /**
     * Fill a rectangle with a certain color.
     */
    inline void
    fillRect(qint32 x, qint32 y, qint32 w, qint32 h, const KoColor &c)
    {
        fillRect(x, y, w, h, c, OPACITY_OPAQUE_U8);
    }

    /**
     * Overloaded version of the above function.
     */
    inline void fillRect(const QRect &rc, const KoColor &c)
    {
        fillRect(rc.x(), rc.y(), rc.width(), rc.height(), c, OPACITY_OPAQUE_U8);
    }

    /**
     * Fill a rectangle with a certain pattern. The pattern is repeated if it does not fit the
     * entire rectangle.
     */
    void fillRect(qint32 x1, qint32 y1, qint32 w, qint32 h, const KoPatternSP pattern, const QPoint &offset = QPoint());

    /**
     * Fill a rectangle with a certain pattern. The pattern is repeated if it does not fit the
     * entire rectangle.
     *
     * This one uses blitting and thus makes use of proper composition.
     */
    void fillRect(qint32 x1, qint32 y1, qint32 w, qint32 h, const KisPaintDeviceSP device, const QRect& deviceRect);

    /**
     * Overloaded version of the above function.
     */
    void fillRect(const QRect &rc, const KisPaintDeviceSP device, const QRect &deviceRect);

    /**
     * Overloaded version of the above function.
     */
    void fillRect(const QRect& rc, const KoPatternSP pattern, const QPoint &offset = QPoint());

    /**
     * Fill a rectangle with black transparent pixels (0, 0, 0, 0 for RGBA).
     */
    inline void eraseRect(qint32 x1, qint32 y1, qint32 w, qint32 h)
    {
        const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
        KoColor c(Qt::black, cs);
        fillRect(x1, y1, w, h, c, OPACITY_TRANSPARENT_U8);
    }

    /**
     * Overloaded version of the above function.
     */
    inline void eraseRect(const QRect &rc)
    {
        const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
        KoColor c(Qt::black, cs);
        fillRect(rc.x(),
                 rc.y(),
                 rc.width(),
                 rc.height(),
                 c,
                 OPACITY_TRANSPARENT_U8);
    }

    /**
     * @brief fillRect
     * Fill a rectangle with a certain pattern. The pattern is repeated if it does not fit the
     * entire rectangle. Differs from other functions that it uses a transform, does not support
     * composite ops in turn.
     * @param rc rectangle to fill.
     * @param pattern pattern to use.
     * @param transform transformation to apply to the pattern.
     */
    void fillRectNoCompose(const QRect& rc, const KoPatternSP pattern, const QTransform transform);

    /**
     * Fill a rectangle with a certain pattern. The pattern is repeated if it does not fit the
     * entire rectangle.
     *
     * This one supports transforms, but does not use blitting.
     */
    void fillRectNoCompose(qint32 x1, qint32 y1, qint32 w, qint32 h, const KisPaintDeviceSP device, const QRect& deviceRect, const QTransform transform);

    /**
     * Fill the specified area with the output of the generator plugin that is configured
     * in the generator parameter
     */
    void fillRect(qint32 x1, qint32 y1, qint32 w, qint32 h, const KisFilterConfigurationSP  generator);

    /**
     * Fills the enclosed area around the point with the set color. If
     * there is a selection, the whole selection is filled. Note that
     * you must have set the width and height on the painter if you
     * don't have a selection.
     *
     * @param startX the X position where the floodfill starts
     * @param startY the Y position where the floodfill starts
     * @param sourceDevice the sourceDevice that determines the area that
     * is floodfilled if sampleMerged is on
     */
    void fillColor(int startX, int startY, KisPaintDeviceSP sourceDevice);

    /**
     * Fills the enclosed area around the point with the set pattern.
     * If there is a selection, the whole selection is filled. Note
     * that you must have set the width and height on the painter if
     * you don't have a selection.
     *
     * @param startX the X position where the floodfill starts
     * @param startY the Y position where the floodfill starts
     * @param sourceDevice the sourceDevice that determines the area that
     * is floodfilled if sampleMerged is on
     * @param patternTransform transform applied to the pattern;
     */
    void fillPattern(int startX, int startY, KisPaintDeviceSP sourceDevice, QTransform patternTransform = QTransform());

    /**
     * Returns a selection mask for the floodfill starting at the specified position.
     * This variant basically creates a new selection object and passes it down
     *   to the other variant of the function.
     *
     * @param startX the X position where the floodfill starts
     * @param startY the Y position where the floodfill starts
     * @param sourceDevice the sourceDevice that determines the area that
     * is floodfilled if sampleMerged is on
     */
    KisPixelSelectionSP createFloodSelection(int startX, int startY,
                                             KisPaintDeviceSP sourceDevice, KisPaintDeviceSP existingSelection);

    /**
     * Returns a selection mask for the floodfill starting at the specified position.
     * This variant requires an empty selection object. It is used in cases where the pointer
     *    to the selection must be known beforehand, for example when the selection is filled
     *    in a stroke and then the pointer to the pixel selection is needed later.
     *
     * @param selection empty new selection object
     * @param startX the X position where the floodfill starts
     * @param startY the Y position where the floodfill starts
     * @param sourceDevice the sourceDevice that determines the area that
     * is floodfilled if sampleMerged is on
     */
    KisPixelSelectionSP createFloodSelection(KisPixelSelectionSP newSelection, int startX, int startY,
                                             KisPaintDeviceSP sourceDevice, KisPaintDeviceSP existingSelection);

    /**
     * Set the threshold for floodfill. The range is 0-255: 0 means the fill will only
     * fill parts that are the exact same color, 255 means anything will be filled
     */
    inline void setFillThreshold(int threshold)
    {
        m_threshold = threshold;
    }

    /** Returns the fill threshold, see setFillThreshold for details */
    int fillThreshold() const {
        return m_threshold;
    }

    /**
     * Set the opacity spread for floodfill. The range is 0-100: 0% means that
     * the fully opaque area only encompasses the pixels exactly equal to the
     * seed point with the other pixels of the selected region being
     * semi-transparent (depending on how similar they are to the seed pixel)
     * up to the region boundary (given by the threshold value). 100 means that
     * the fully opaque area will encompass all the pixels of the selected
     * region up to the contour. Any value inbetween will make the fully opaque
     * portion of the region vary in size, with semi-transparent pixels
     * inbetween it and  the region boundary
     */
    void setOpacitySpread(int opacitySpread)
    {
        m_opacitySpread = opacitySpread;
    }

    /** Returns the fill opacity spread, see setOpacitySpread for details */
    int opacitySpread() const {
        return m_opacitySpread;
    }

    bool useCompositioning() const {
        return m_useCompositioning;
    }

    void setUseCompositioning(bool useCompositioning) {
        m_useCompositioning = useCompositioning;
    }

    /** Sets the width of the paint device */
    void setWidth(int w) {
        m_width = w;
    }

    /** Sets the height of the paint device */
    void setHeight(int h) {
        m_height = h;
    }

    /** If true, floodfill doesn't fill outside the selected area of a layer */
    bool careForSelection() const {
        return m_careForSelection;
    }

    /** Set caring for selection. See careForSelection for details */
    void setCareForSelection(bool set) {
        m_careForSelection = set;
    }

    /** Sets if antiAlias should be applied to the selection */
    void setAntiAlias(bool antiAlias) {
        m_antiAlias = antiAlias;
    }
    
    /** Get if antiAlias should be applied to the selection */
    bool antiAlias() const {
        return m_antiAlias;
    }

    /** Sets the auto growth/shrinking radius */
    void setSizemod(int sizemod) {
        m_sizemod = sizemod;
    }
    
    /** Sets how much to auto-grow or shrink (if @p sizemod is negative) the selection
    flood before painting, this affects every fill operation except fillRect */
    int sizemod() const {
        return m_sizemod;
    }
    
    /** Sets feathering radius */
    void setFeather(int feather) {
        m_feather = feather;
    }
    
    /** defines the feathering radius for selection flood operations, this affects every
    fill operation except fillRect */
    uint feather() const {
        return m_feather;
    }

    /** Sets selection borders being treated as boundary */
    void setUseSelectionAsBoundary(bool useSelectionAsBoundary) {
        m_useSelectionAsBoundary = useSelectionAsBoundary;
    }

    /** defines if the selection borders are treated as boundary in flood fill or not */
    uint useSelectionAsBoundary() const {
        return m_useSelectionAsBoundary;
    }

protected:
    void setCurrentFillSelection(KisSelectionSP fillSelection)
    {
        m_fillSelection = fillSelection;
    }

    KisSelectionSP currentFillSelection() const
    {
        return m_fillSelection;
    }

    // for floodfill
    void genericFillStart(int startX, int startY, KisPaintDeviceSP sourceDevice);
    void genericFillEnd(KisPaintDeviceSP filled);

private:
    KisSelectionSP m_fillSelection;

    int m_feather;
    int m_sizemod;
    bool m_antiAlias;
    int m_threshold;
    int m_opacitySpread;
    int m_width, m_height;
    QRect m_rect;
    bool m_careForSelection;
    bool m_useCompositioning;
    bool m_useSelectionAsBoundary;
};

#endif //KIS_FILL_PAINTER_H_
