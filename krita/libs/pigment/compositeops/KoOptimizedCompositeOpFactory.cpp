/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "KoOptimizedCompositeOpFactoryPerArch.h"
#include "KoOptimizedCompositeOpFactory.h"

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpHard32(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenHard32>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy32(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenCreamy32>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createOverOp32(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver32> >(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createCopyOp32(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy32> >(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpHard128(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenHard128>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy128(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenCreamy128>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createOverOp128(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver128> >(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createCopyOp128(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy128> >(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpHardU64(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenHardU64>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamyU64(const KoColorSpace *cs)
{
    return createOptimizedClass<
        KoOptimizedCompositeOpFactoryPerArch<
            KoOptimizedCompositeOpAlphaDarkenCreamyU64>>(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createOverOpU64(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOverU64> >(cs);
}

KoCompositeOp* KoOptimizedCompositeOpFactory::createCopyOpU64(const KoColorSpace *cs)
{
    return createOptimizedClass<KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopyU64> >(cs);
}
