/*
 *  SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ocio_display_filter_vfx2020.h"

#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

#include <kis_config.h>

#include <opengl/kis_opengl.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLFunctions_3_0>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLExtraFunctions>

#if defined(QT_OPENGL_ES_2)
#define GL_RGBA16F_ARB GL_RGBA16F_EXT
#define GL_RGB16F_ARB GL_RGB16F_EXT
#endif

#if defined(QT_OPENGL_ES_2) && !defined(QT_OPENGL_ES_3)
#define GL_R32F GL_R32F_EXT
#define GL_RED GL_RED_EXT
#define GL_TEXTURE_WRAP_R GL_TEXTURE_WRAP_R_OES
#endif

#include "kis_context_thread_locale.h"

OcioDisplayFilter::OcioDisplayFilter(KisExposureGammaCorrectionInterface *interface, QObject *parent)
    : KisDisplayFilter(parent)
    , m_interface(interface)
{
}

OcioDisplayFilter::~OcioDisplayFilter()
{
}

KisExposureGammaCorrectionInterface* OcioDisplayFilter::correctionInterface() const
{
    return m_interface;
}

void OcioDisplayFilter::filter(quint8 *pixels, quint32 numPixels)
{
    // processes that data _in_ place
    if (m_processor) {
        OCIO::PackedImageDesc img(reinterpret_cast<float*>(pixels), numPixels, 1, 4);
        m_processor->apply(img);
    }
}

void OcioDisplayFilter::approximateInverseTransformation(quint8 *pixels, quint32 numPixels)
{
    // processes that data _in_ place
    if (m_revereseApproximationProcessor) {
        OCIO::PackedImageDesc img(reinterpret_cast<float*>(pixels), numPixels, 1, 4);
        m_revereseApproximationProcessor->apply(img);
    }
}

void OcioDisplayFilter::approximateForwardTransformation(quint8 *pixels, quint32 numPixels)
{
    // processes that data _in_ place
    if (m_forwardApproximationProcessor) {
        OCIO::PackedImageDesc img(reinterpret_cast<float*>(pixels), numPixels, 1, 4);
        m_forwardApproximationProcessor->apply(img);
    }
}

bool OcioDisplayFilter::useInternalColorManagement() const
{
    return forceInternalColorManagement;
}

bool OcioDisplayFilter::lockCurrentColorVisualRepresentation() const
{
    return m_lockCurrentColorVisualRepresentation;
}

void OcioDisplayFilter::setLockCurrentColorVisualRepresentation(bool value)
{
    m_lockCurrentColorVisualRepresentation = value;
}

QString OcioDisplayFilter::program() const
{
    return m_program;
}

void OcioDisplayFilter::updateProcessor()
{
    if (!config) {
        return;
    }

    if (!displayDevice) {
        displayDevice = config->getDefaultDisplay();
    }

    if (!view) {
        view = config->getDefaultView(displayDevice);
    }

    if (!inputColorSpaceName) {
        inputColorSpaceName = config->getColorSpaceNameByIndex(0);
    }
    if (!look) {
    look = config->getLookNameByIndex(0);
    }

    if (!displayDevice || !view || !inputColorSpaceName) {
        return;
    }

    OCIO::DisplayTransformRcPtr transform = OCIO::DisplayTransform::Create();
    transform->setInputColorSpaceName(inputColorSpaceName);
    transform->setDisplay(displayDevice);
    transform->setView(view);

    /**
     * Look support:
     * As the OCIO docs will tell you, looks are a aesthetic transform that is
     * added onto the mix.
     * A view+display can have it's own assigned Look, or list of looks, and these
     * can be overridden optionally.
     * What the OCIO docs won't tell you is that a display transform won't use the
     * looks attached to it unless "skipColorSpaceConversions" is false...
     * I have no idea what "skipColorSpaceConversions" is beyond what it says on the
     * tin. It is not mentioned in the documentation anywhere. Or on the website.
     * Or how to set it. Or unset it. Why it is apparently set true to begin with.
     * Only that, apparently, this was done with non-color data in mind...
     *
     * Until there's clear documentation on how to use this feature, I am afraid the
     * override is all we can offer.
     */
    if (config->getLook(look)) {
       transform->setLooksOverride(look);
       transform->setLooksOverrideEnabled(true);
    }

    OCIO::GroupTransformRcPtr approximateTransform = OCIO::GroupTransform::Create();

    // fstop exposure control -- not sure how that translates to our exposure
    {
        float exposureGain = powf(2.0f, exposure);

        const qreal minRange = 0.001;
        if (qAbs(blackPoint - whitePoint) < minRange) {
            whitePoint = blackPoint + minRange;
        }

        const float oldMin[] = { blackPoint, blackPoint, blackPoint, 0.0f };
        const float oldMax[] = { whitePoint, whitePoint, whitePoint, 1.0f };

        const float newMin[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const float newMax[] = { exposureGain, exposureGain, exposureGain, 1.0f };

        float m44[16];
        float offset4[4];
        OCIO::MatrixTransform::Fit(m44, offset4, oldMin, oldMax, newMin, newMax);
        OCIO::MatrixTransformRcPtr mtx =  OCIO::MatrixTransform::Create();
        mtx->setValue(m44, offset4);
        transform->setLinearCC(mtx);

        // approximation (no color correction);
        approximateTransform->push_back(mtx);
    }

    // channel swizzle
    {
        int channelHot[4];
        switch (swizzle) {
        case LUMINANCE:
            channelHot[0] = 1;
            channelHot[1] = 1;
            channelHot[2] = 1;
            channelHot[3] = 0;
            break;
        case RGBA:
            channelHot[0] = 1;
            channelHot[1] = 1;
            channelHot[2] = 1;
            channelHot[3] = 1;
            break;
        case R:
            channelHot[0] = 1;
            channelHot[1] = 0;
            channelHot[2] = 0;
            channelHot[3] = 0;
            break;
        case G:
            channelHot[0] = 0;
            channelHot[1] = 1;
            channelHot[2] = 0;
            channelHot[3] = 0;
            break;
        case B:
            channelHot[0] = 0;
            channelHot[1] = 0;
            channelHot[2] = 1;
            channelHot[3] = 0;
            break;
        case A:
            channelHot[0] = 0;
            channelHot[1] = 0;
            channelHot[2] = 0;
            channelHot[3] = 1;
        default:
            ;
        }
        float lumacoef[3];
        config->getDefaultLumaCoefs(lumacoef);
        float m44[16];
        float offset[4];
        OCIO::MatrixTransform::View(m44, offset, channelHot, lumacoef);
        OCIO::MatrixTransformRcPtr swizzleTransform = OCIO::MatrixTransform::Create();
        swizzleTransform->setValue(m44, offset);
        transform->setChannelView(swizzleTransform);
    }

    // Post-display transform gamma
    {
        float exponent = 1.0f/std::max(1e-6f, static_cast<float>(gamma));
        const float exponent4f[] = { exponent, exponent, exponent, exponent };
        OCIO::ExponentTransformRcPtr expTransform =  OCIO::ExponentTransform::Create();
        expTransform->setValue(exponent4f);
        transform->setDisplayCC(expTransform);

        // approximation (no color correction);
        approximateTransform->push_back(expTransform);
    }

    try {
        AutoSetAndRestoreThreadLocale l;
        m_processor = config->getProcessor(transform);
    } catch (OCIO::Exception &e) {
        // XXX: How to not break the OCIO shader now?
        errKrita << "OCIO exception while parsing the current context:" << e.what();
        m_shaderDirty = false;
        return;
    }

    m_forwardApproximationProcessor = config->getProcessor(approximateTransform, OCIO::TRANSFORM_DIR_FORWARD);

    try {
        m_revereseApproximationProcessor = config->getProcessor(approximateTransform, OCIO::TRANSFORM_DIR_INVERSE);
    } catch (...) {
        warnKrita << "OCIO inverted matrix does not exist!";
        //m_revereseApproximationProcessor;
    }

    m_shaderDirty = true;
}

bool OcioDisplayFilter::updateShader()
{
    if (KisOpenGL::hasOpenGLES()) {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();

        KIS_ASSERT_RECOVER_RETURN_VALUE(ctx, false);
        if (ctx->format().majorVersion() >= 3) {
            QOpenGLExtraFunctions *f = ctx->extraFunctions();
            if (f) {
                return updateShaderImpl(f);
            }
        } else if (ctx->hasExtension("GL_OES_texture_half_float") && ctx->hasExtension("GL_EXT_color_buffer_half_float")
                && ctx->hasExtension("GL_OES_texture_half_float_linear")) {
                QOpenGLExtraFunctions *f = ctx->extraFunctions();
                if (f) {
                    return updateShaderImpl(f);
                }
        } else {
            dbgKrita << "OcioDisplayFilter::updateShader (2020)"
                     << "OpenGL ES v2+ support detected but no OES_texture_half_float,"
                        "GL_EXT_color_buffer_half_float or GL_OES_texture_half_float_linear were found";
            return false;
        }
#if defined(QT_OPENGL_3)
    } else if (KisOpenGL::hasOpenGL3()) {
        QOpenGLFunctions_3_2_Core *f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>();
        if (f) {
            return updateShaderImpl(f);
        }
#endif
    }

    // XXX This option can be removed once we move to Qt 5.7+
    if (KisOpenGL::supportsLoD()) {
#if defined(QT_OPENGL_3)
#if defined(Q_OS_MAC) && defined(QT_OPENGL_3_2)
        QOpenGLFunctions_3_2_Core *f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>();
#else
        QOpenGLFunctions_3_0 *f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_0>();
#endif
        if (f) {
            return updateShaderImpl(f);
        }
#endif
    }
#if !defined(QT_OPENGL_ES_2)
    QOpenGLFunctions_2_0 *f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_0>();
    if (f) {
        return updateShaderImpl(f);
    }
#endif

    return false;
}

template <class F>
bool OcioDisplayFilter::updateShaderImpl(F *f) {
    // check whether we are allowed to use shaders -- though that should
    // work for everyone these days
    KisConfig cfg(true);
    if (!cfg.useOpenGL()) return false;

    if (!m_shaderDirty) return false;

    if (!f) {
        qWarning() << "Failed to get valid OpenGL functions for OcioDisplayFilter!";
        return false;
    }

    f->initializeOpenGLFunctions();

    bool shouldRecompileShader = false;

    const int lut3DEdgeSize = cfg.ocioLutEdgeSize();

    if (m_lut3d.size() == 0) {
        //dbgKrita << "generating lut";
        f->glGenTextures(1, &m_lut3dTexID);

        int num3Dentries = 3 * lut3DEdgeSize * lut3DEdgeSize * lut3DEdgeSize;
        m_lut3d.fill(0.0, num3Dentries);

        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_3D, m_lut3dTexID);

        f->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        f->glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F_ARB,
                        lut3DEdgeSize, lut3DEdgeSize, lut3DEdgeSize,
                        0, GL_RGB, GL_FLOAT, &m_lut3d.constData()[0]);
    }

    // Step 1: Create a GPU Shader Description
    OCIO::GpuShaderDesc shaderDesc;

    if (KisOpenGL::supportsLoD()) {
        shaderDesc.setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_3);
    }
    else {
        shaderDesc.setLanguage(OCIO::GPU_LANGUAGE_GLSL_1_0);
    }


    shaderDesc.setFunctionName("OCIODisplay");
    shaderDesc.setLut3DEdgeLen(lut3DEdgeSize);


    // Step 2: Compute the 3D LUT
    QString lut3dCacheID = QString::fromLatin1(m_processor->getGpuLut3DCacheID(shaderDesc));
    if (lut3dCacheID != m_lut3dcacheid) {
        //dbgKrita << "Computing 3DLut " << m_lut3dcacheid;
        m_lut3dcacheid = lut3dCacheID;
        m_processor->getGpuLut3D(&m_lut3d[0], shaderDesc);

        f->glBindTexture(GL_TEXTURE_3D, m_lut3dTexID);
        f->glTexSubImage3D(GL_TEXTURE_3D, 0,
                                  0, 0, 0,
                                  lut3DEdgeSize, lut3DEdgeSize, lut3DEdgeSize,
                                  GL_RGB, GL_FLOAT, &m_lut3d[0]);
    }

    // Step 3: Generate the shader text
    QString shaderCacheID = QString::fromLatin1(m_processor->getGpuShaderTextCacheID(shaderDesc));
    if (m_program.isEmpty() || shaderCacheID != m_shadercacheid) {
        //dbgKrita << "Computing Shader " << m_shadercacheid;

        m_shadercacheid = shaderCacheID;

        std::ostringstream os;
        os << m_processor->getGpuShaderText(shaderDesc) << "\n";

        m_program = QString::fromLatin1(os.str().c_str());
        shouldRecompileShader = true;
    }

    m_shaderDirty = false;
    return shouldRecompileShader;
}

void OcioDisplayFilter::setupTextures(GLFunctions *f, QOpenGLShaderProgram *program) const
{
    f->glActiveTexture(GL_TEXTURE0 + 1);
    f->glBindTexture(GL_TEXTURE_3D, m_lut3dTexID);
    program->setUniformValue(program->uniformLocation("texture1"), 1);
}
