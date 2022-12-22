/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#define GL_GLEXT_PROTOTYPES
#include "kis_texture_tile.h"
#include "kis_texture_tile_update_info.h"
#include "KisOpenGLBufferCircularStorage.h"

#include <kis_debug.h>
#if !defined(QT_OPENGL_ES)
#include <QOpenGLBuffer>
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x814F
#endif


void KisTextureTile::setTextureParameters()
{

    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, m_numMipmapLevels);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_numMipmapLevels);

    if ((m_texturesInfo->internalFormat == GL_RGBA8 && m_texturesInfo->format == GL_RGBA)
#ifdef Q_OS_MACOS
        || (m_texturesInfo->internalFormat == GL_RGBA16 && m_texturesInfo->format == GL_RGBA)
#elif defined(QT_OPENGL_ES_3)
        || (m_texturesInfo->internalFormat == GL_RGBA16_EXT && m_texturesInfo->format == GL_RGBA)
#endif
    ) {
        // If image format is RGBA8, swap the red and blue channels for the proper color
        // This is for OpenGL ES support and only used if lacking GL_EXT_texture_format_BGRA8888
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    }

    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

inline QRectF relativeRect(const QRect &br /* baseRect */,
                           const QRect &cr /* childRect */,
                           const KisGLTexturesInfo *texturesInfo)
{
    const qreal x = qreal(cr.x() - br.x()) / texturesInfo->width;
    const qreal y = qreal(cr.y() - br.y()) / texturesInfo->height;
    const qreal w = qreal(cr.width()) / texturesInfo->width;
    const qreal h = qreal(cr.height()) / texturesInfo->height;

    return QRectF(x, y, w, h);
}

#include "kis_debug.h"

KisTextureTile::KisTextureTile(const QRect &imageRect, const KisGLTexturesInfo *texturesInfo,
                               const QByteArray &fillData, KisOpenGL::FilterMode filter,
                               KisOpenGLBufferCircularStorage *bufferStorage, int numMipmapLevels, QOpenGLFunctions *fcn)

    : m_textureId(0)
    , m_tileRectInImagePixels(imageRect)
    , m_filter(filter)
    , m_texturesInfo(texturesInfo)
    , m_needsMipmapRegeneration(false)
    , m_preparedLodPlane(0)
    , m_numMipmapLevels(numMipmapLevels)
    , f(fcn)
    , m_bufferStorage(bufferStorage)
{
    const GLvoid *fd = fillData.constData();

    m_textureRectInImagePixels =
            kisGrowRect(m_tileRectInImagePixels, texturesInfo->border);

    m_tileRectInTexturePixels = relativeRect(m_textureRectInImagePixels,
                                             m_tileRectInImagePixels,
                                             m_texturesInfo);

    f->glGenTextures(1, &m_textureId);
    f->glBindTexture(GL_TEXTURE_2D, m_textureId);

    setTextureParameters();

    KisOpenGLBufferCircularStorage::BufferBinder binder(
        m_bufferStorage, &fd, fillData.size());

    f->glTexImage2D(GL_TEXTURE_2D, 0,
                 m_texturesInfo->internalFormat,
                 m_texturesInfo->width,
                 m_texturesInfo->height, 0,
                 m_texturesInfo->format,
                 m_texturesInfo->type, fd);

    setNeedsMipmapRegeneration();
}

KisTextureTile::~KisTextureTile()
{
    f->glDeleteTextures(1, &m_textureId);
}

int KisTextureTile::bindToActiveTexture(bool blockMipmapRegeneration)
{
    f->glBindTexture(GL_TEXTURE_2D, m_textureId);

    if (m_needsMipmapRegeneration && !blockMipmapRegeneration) {
        f->glGenerateMipmap(GL_TEXTURE_2D);
        setPreparedLodPlane(0);
    }

    return m_preparedLodPlane;
}

void KisTextureTile::setNeedsMipmapRegeneration()
{
    if (m_filter == KisOpenGL::TrilinearFilterMode ||
        m_filter == KisOpenGL::HighQualityFiltering) {

        m_needsMipmapRegeneration = true;
    }
}

void KisTextureTile::setPreparedLodPlane(int lod)
{
    m_preparedLodPlane = lod;
    m_needsMipmapRegeneration = false;
}

void KisTextureTile::update(const KisTextureTileUpdateInfo &updateInfo, bool blockMipmapRegeneration)
{
    f->initializeOpenGLFunctions();
    f->glBindTexture(GL_TEXTURE_2D, m_textureId);

    setTextureParameters();

    const int patchLevelOfDetail = updateInfo.patchLevelOfDetail();
    const QSize patchSize = updateInfo.realPatchSize();
    const QPoint patchOffset = updateInfo.realPatchOffset();

    const GLvoid *fd = updateInfo.data();

    /**
     * In some special case, when the Lod0 stroke is cancelled the
     * following situation is possible:
     *
     * 1)  The stroke  is  cancelled,  Lod0 update  is  issued by  the
     *     image. LodN level of the openGL times is still dirty.
     *
     * 2) [here, ideally, the canvas should be re-rendered, so that
     *     the mipmap would be regenerated in bindToActiveTexture()
     *     call, by in some cases (if you cancel and paint to quickly),
     *     that doesn't have time to happen]
     *
     * 3) The new LodN stroke issues a *partial* update of a LodN
     *    plane of the tile. But the plane is still *dirty*! We update
     *    a part of it, but we cannot regenerate the mipmap anymore,
     *    because the Lod0 level is not known yet!
     *
     * To avoid this issue, we should regenerate the dirty mipmap
     * *before* doing anything with the low-resolution plane.
     */
    if (!blockMipmapRegeneration &&
        patchLevelOfDetail > 0 &&
        m_needsMipmapRegeneration &&
        !updateInfo.isEntireTileUpdated()) {

        f->glGenerateMipmap(GL_TEXTURE_2D);
        m_needsMipmapRegeneration = false;
    }


    if (updateInfo.isEntireTileUpdated()) {
        KisOpenGLBufferCircularStorage::BufferBinder b(
            m_bufferStorage, &fd, updateInfo.patchPixelsLength());

        f->glTexImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                     m_texturesInfo->internalFormat,
                     patchSize.width(),
                     patchSize.height(), 0,
                     m_texturesInfo->format,
                     m_texturesInfo->type,
                     fd);
    }
    else {
        const int size = patchSize.width() * patchSize.height() * updateInfo.pixelSize();
        KisOpenGLBufferCircularStorage::BufferBinder b(
            m_bufferStorage, &fd, size);

        f->glTexSubImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                        patchOffset.x(), patchOffset.y(),
                        patchSize.width(), patchSize.height(),
                        m_texturesInfo->format,
                        m_texturesInfo->type,
                        fd);

    }

    /**
     * On the boundaries of KisImage, there is a border-effect as well.
     * So we just repeat the bounding pixels of the image to make
     * bilinear interpolator happy.
     */

    /**
     * WARN: The width of the stripes will be equal to the broader
     *       width of the tiles.
     */

    const int pixelSize = updateInfo.pixelSize();
    const QSize tileSize = updateInfo.realTileSize();

    if(updateInfo.isTopmost()) {
        int start = 0;
        int end = patchOffset.y() - 1;

        const GLvoid *fd = updateInfo.data();
        const int size = patchSize.width() * pixelSize;
        KisOpenGLBufferCircularStorage::BufferBinder g(
            m_bufferStorage, &fd, size);

        for (int i = start; i <= end; i++) {
            f->glTexSubImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                               patchOffset.x(), i,
                               patchSize.width(), 1,
                               m_texturesInfo->format,
                               m_texturesInfo->type,
                               fd);
        }
    }

    if (updateInfo.isBottommost()) {
        int shift = patchSize.width() * (patchSize.height() - 1) *
                pixelSize;

        int start = patchOffset.y() + patchSize.height();
        int end = tileSize.height() - 1;

        const GLvoid *fd = updateInfo.data() + shift;
        const int size = patchSize.width() * pixelSize;
        KisOpenGLBufferCircularStorage::BufferBinder g(
            m_bufferStorage, &fd, size);

        for (int i = start; i < end; i++) {
            f->glTexSubImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                            patchOffset.x(), i,
                            patchSize.width(), 1,
                            m_texturesInfo->format,
                            m_texturesInfo->type,
                            fd);
        }
    }

    if (updateInfo.isLeftmost()) {

        QByteArray columnBuffer(patchSize.height() * pixelSize, 0);

        quint8 *srcPtr = updateInfo.data();
        quint8 *dstPtr = (quint8*) columnBuffer.data();
        for(int i = 0; i < patchSize.height(); i++) {
            memcpy(dstPtr, srcPtr, pixelSize);

            srcPtr += patchSize.width() * pixelSize;
            dstPtr += pixelSize;
        }

        int start = 0;
        int end = patchOffset.x() - 1;

        const GLvoid *fd = columnBuffer.constData();
        const int size = columnBuffer.size();
        KisOpenGLBufferCircularStorage::BufferBinder g(
            m_bufferStorage, &fd, size);

        for (int i = start; i <= end; i++) {
            f->glTexSubImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                            i, patchOffset.y(),
                            1, patchSize.height(),
                            m_texturesInfo->format,
                            m_texturesInfo->type,
                            fd);
        }
    }

    if (updateInfo.isRightmost()) {

        QByteArray columnBuffer(patchSize.height() * pixelSize, 0);

        quint8 *srcPtr = updateInfo.data() + (patchSize.width() - 1) * pixelSize;
        quint8 *dstPtr = (quint8*) columnBuffer.data();
        for(int i = 0; i < patchSize.height(); i++) {
            memcpy(dstPtr, srcPtr, pixelSize);

            srcPtr += patchSize.width() * pixelSize;
            dstPtr += pixelSize;
        }

        int start = patchOffset.x() + patchSize.width();
        int end = tileSize.width() - 1;

        const GLvoid *fd = columnBuffer.constData();
        const int size = columnBuffer.size();
        KisOpenGLBufferCircularStorage::BufferBinder g(
            m_bufferStorage, &fd, size);

        for (int i = start; i <= end; i++) {
            f->glTexSubImage2D(GL_TEXTURE_2D, patchLevelOfDetail,
                            i, patchOffset.y(),
                            1, patchSize.height(),
                            m_texturesInfo->format,
                            m_texturesInfo->type,
                            fd);
        }
    }

    //// Uncomment this warning if you see any weird flickering when
    //// Instant Preview updates
    // if (!updateInfo.isEntireTileUpdated() &&
    //     !(!patchLevelOfDetail || !m_preparedLodPlane || patchLevelOfDetail == m_preparedLodPlane)) {
    //     qDebug() << "WARNING: LodN switch is requested for the partial tile update!. Flickering is possible..." << ppVar(patchSize);
    //     qDebug() << "    " << ppVar(m_preparedLodPlane);
    //     qDebug() << "    " << ppVar(patchLevelOfDetail);
    // }

    if (!patchLevelOfDetail) {
        setNeedsMipmapRegeneration();
    } else {
        setPreparedLodPlane(patchLevelOfDetail);
    }
}

QRectF KisTextureTile::imageRectInTexturePixels(const QRect &imageRect) const
{
    return relativeRect(m_textureRectInImagePixels,
                        imageRect,
                        m_texturesInfo);

}
