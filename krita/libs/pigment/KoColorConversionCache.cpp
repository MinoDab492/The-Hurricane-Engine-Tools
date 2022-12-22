/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoColorConversionCache.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QThreadStorage>

#include <KoColorSpace.h>

struct KoColorConversionCacheKey {

    KoColorConversionCacheKey(const KoColorSpace* _src,
                              const KoColorSpace* _dst,
                              KoColorConversionTransformation::Intent _renderingIntent,
                              KoColorConversionTransformation::ConversionFlags _conversionFlags)
        : src(_src)
        , dst(_dst)
        , renderingIntent(_renderingIntent)
        , conversionFlags(_conversionFlags)
    {
    }

    bool operator==(const KoColorConversionCacheKey& rhs) const {
        return (*src == *(rhs.src)) && (*dst == *(rhs.dst))
                && (renderingIntent == rhs.renderingIntent)
                && (conversionFlags == rhs.conversionFlags);
    }

    const KoColorSpace* src;
    const KoColorSpace* dst;
    KoColorConversionTransformation::Intent renderingIntent;
    KoColorConversionTransformation::ConversionFlags conversionFlags;
};

uint qHash(const KoColorConversionCacheKey& key)
{
    return qHash(key.src) + qHash(key.dst) + qHash(key.renderingIntent) + qHash(key.conversionFlags);
}

struct KoColorConversionCache::CachedTransformation {

    CachedTransformation(KoColorConversionTransformation* _transfo)
        : transfo(_transfo), use(0)
    {}

    ~CachedTransformation() {
        delete transfo;
    }

    bool isNotInUse() {
        return !use;
    }

    KoColorConversionTransformation* transfo;
    QAtomicInt use;
};

typedef QPair<KoColorConversionCacheKey, KoCachedColorConversionTransformation> FastPathCacheItem;

struct KoColorConversionCache::Private {
    QMultiHash< KoColorConversionCacheKey, CachedTransformation*> cache;
    QMutex cacheMutex;

    QThreadStorage<FastPathCacheItem*> fastStorage;
};


KoColorConversionCache::KoColorConversionCache() : d(new Private)
{
}

KoColorConversionCache::~KoColorConversionCache()
{
    Q_FOREACH (CachedTransformation* transfo, d->cache) {
        delete transfo;
    }
    delete d;
}

KoCachedColorConversionTransformation KoColorConversionCache::cachedConverter(const KoColorSpace* src,
                                                                              const KoColorSpace* dst,
                                                                              KoColorConversionTransformation::Intent _renderingIntent,
                                                                              KoColorConversionTransformation::ConversionFlags _conversionFlags)
{
    KoColorConversionCacheKey key(src, dst, _renderingIntent, _conversionFlags);

    FastPathCacheItem *cacheItem =
        d->fastStorage.localData();

    if (cacheItem) {
        if (cacheItem->first == key) {
            return cacheItem->second;
        }
    }

    cacheItem = 0;

    QMutexLocker lock(&d->cacheMutex);
    QList< CachedTransformation* > cachedTransfos = d->cache.values(key);
    if (cachedTransfos.size() != 0) {
        Q_FOREACH (CachedTransformation* ct, cachedTransfos) {
            ct->transfo->setSrcColorSpace(src);
            ct->transfo->setDstColorSpace(dst);

            cacheItem = new FastPathCacheItem(key, KoCachedColorConversionTransformation(ct));
            break;
        }
    }
    if (!cacheItem) {
        KoColorConversionTransformation* transfo = src->createColorConverter(dst, _renderingIntent, _conversionFlags);
        CachedTransformation* ct = new CachedTransformation(transfo);
        d->cache.insert(key, ct);
        cacheItem = new FastPathCacheItem(key, KoCachedColorConversionTransformation(ct));
    }

    d->fastStorage.setLocalData(cacheItem);
    return cacheItem->second;
}

void KoColorConversionCache::colorSpaceIsDestroyed(const KoColorSpace* cs)
{
    d->fastStorage.setLocalData(0);

    QMutexLocker lock(&d->cacheMutex);
    QMultiHash< KoColorConversionCacheKey, CachedTransformation*>::iterator endIt = d->cache.end();
    for (QMultiHash< KoColorConversionCacheKey, CachedTransformation*>::iterator it = d->cache.begin(); it != endIt;) {
        if (it.key().src == cs || it.key().dst == cs) {
            Q_ASSERT(it.value()->isNotInUse()); // That's terribely evil, if that assert fails, that means that someone is using a color transformation with a color space which is currently being deleted
            delete it.value();
            it = d->cache.erase(it);
        } else {
            ++it;
        }
    }
}

//--------- KoCachedColorConversionTransformation ----------//

KoCachedColorConversionTransformation::KoCachedColorConversionTransformation(KoColorConversionCache::CachedTransformation* transfo)
    : m_transfo(transfo)
{
    m_transfo = transfo;
    m_transfo->use.ref();
}

KoCachedColorConversionTransformation::KoCachedColorConversionTransformation(const KoCachedColorConversionTransformation& rhs)
    : m_transfo(rhs.m_transfo)
{
    m_transfo->use.ref();
}

KoCachedColorConversionTransformation::~KoCachedColorConversionTransformation()
{
    Q_ASSERT(m_transfo->use > 0);
    m_transfo->use.deref();
}

const KoColorConversionTransformation* KoCachedColorConversionTransformation::transformation() const
{
    return m_transfo->transfo;
}

