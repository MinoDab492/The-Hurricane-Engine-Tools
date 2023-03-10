/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "KoOptimizedCompositeOpFactoryPerArch.h"

#include "KoColorSpaceTraits.h"
#include "KoCompositeOpAlphaDarken.h"
#include "KoAlphaDarkenParamsWrapper.h"
#include "KoCompositeOpOver.h"
#include "KoCompositeOpCopy2.h"

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHard32>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHard32>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoBgrU8Traits, KoAlphaDarkenParamsWrapperHard>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamy32>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamy32>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoBgrU8Traits, KoAlphaDarkenParamsWrapperCreamy>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver32>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver32>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpOver<KoBgrU8Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHard128>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHard128>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoRgbF32Traits, KoAlphaDarkenParamsWrapperHard>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamy128>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamy128>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoRgbF32Traits, KoAlphaDarkenParamsWrapperCreamy>(param);
}


template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver128>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOver128>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpOver<KoRgbF32Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOverU64>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpOverU64>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpOver<KoBgrU16Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy128>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy128>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpCopy2<KoRgbF32Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopyU64>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopyU64>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpCopy2<KoBgrU16Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy32>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpCopy32>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpCopy2<KoBgrU8Traits>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHardU64>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenHardU64>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoBgrU16Traits, KoAlphaDarkenParamsWrapperHard>(param);
}

template<>
template<>
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamyU64>::ReturnType
KoOptimizedCompositeOpFactoryPerArch<KoOptimizedCompositeOpAlphaDarkenCreamyU64>::create<xsimd::generic>(ParamType param)
{
    return new KoCompositeOpAlphaDarken<KoBgrU16Traits, KoAlphaDarkenParamsWrapperCreamy>(param);
}

