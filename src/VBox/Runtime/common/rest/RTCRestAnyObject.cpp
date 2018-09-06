/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestAnyObject implementation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restanyobject.h>

#include <iprt/assert.h>
#include <iprt/err.h>



/**
 * Default constructor.
 */
RTCRestAnyObject::RTCRestAnyObject()
    : RTCRestObjectBase()
    , m_pData(NULL)
{
    m_fNullIndicator = true;
}


/**
 * Destructor.
 */
RTCRestAnyObject::~RTCRestAnyObject()
{
    if (m_pData)
    {
        delete m_pData;
        m_pData = NULL;
    }
}


/**
 * Copy constructor.
 */
RTCRestAnyObject::RTCRestAnyObject(RTCRestAnyObject const &a_rThat)
    : RTCRestObjectBase()
    , m_pData(NULL)
{
    int rc = assignCopy(a_rThat);
    if (RT_FAILURE(rc))
        throw std::bad_alloc();
}


/**
 * Copy assignment operator.
 */
RTCRestAnyObject &RTCRestAnyObject::operator=(RTCRestAnyObject const &a_rThat)
{
    int rc = assignCopy(a_rThat);
    if (RT_FAILURE(rc))
        throw std::bad_alloc();
    return *this;
}


/**
 * Safe copy assignment method.
 */
int RTCRestAnyObject::assignCopy(RTCRestAnyObject const &a_rThat)
{
    setNull();
    if (   !a_rThat.m_fNullIndicator
        && a_rThat.m_pData != NULL)
    {
        kTypeClass enmType = a_rThat.m_pData->typeClass();
        switch (enmType)
        {
            case kTypeClass_Bool:       return assignCopy(*(RTCRestBool const *)a_rThat.m_pData);
            case kTypeClass_Int64:      return assignCopy(*(RTCRestInt64 const *)a_rThat.m_pData);
            case kTypeClass_Int32:      return assignCopy(*(RTCRestInt32 const *)a_rThat.m_pData);
            case kTypeClass_Int16:      return assignCopy(*(RTCRestInt16 const *)a_rThat.m_pData);
            case kTypeClass_Double:     return assignCopy(*(RTCRestDouble const *)a_rThat.m_pData);
            case kTypeClass_String:     return assignCopy(*(RTCRestString const *)a_rThat.m_pData);
            case kTypeClass_Array:      return assignCopy(*(RTCRestArray<RTCRestAnyObject> const *)a_rThat.m_pData);
            case kTypeClass_StringMap:  return assignCopy(*(RTCRestStringMap<RTCRestAnyObject> const *)a_rThat.m_pData);

            /* Currently unused of invalid: */
            case kTypeClass_StringEnum:
            case kTypeClass_BinaryString:
            case kTypeClass_Object:
            case kTypeClass_Invalid:
                AssertFailedReturn(VERR_REST_INTERNAL_ERROR_7);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Safe copy assignment method, boolean variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestBool const &a_rThat)
{
    setNull();
    RTCRestBool *pData = new (std::nothrow) RTCRestBool();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, int64_t variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestInt64 const &a_rThat)
{
    setNull();
    RTCRestInt64 *pData = new (std::nothrow) RTCRestInt64();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, int32_t variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestInt32 const &a_rThat)
{
    setNull();
    RTCRestInt32 *pData = new (std::nothrow) RTCRestInt32();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, int16_t variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestInt16 const &a_rThat)
{
    setNull();
    RTCRestInt16 *pData = new (std::nothrow) RTCRestInt16();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, double variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestDouble const &a_rThat)
{
    setNull();
    RTCRestDouble *pData = new (std::nothrow) RTCRestDouble();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, string variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestString const &a_rThat)
{
    setNull();
    RTCRestString *pData = new (std::nothrow) RTCRestString();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, array variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestArray<RTCRestAnyObject> const &a_rThat)
{
    setNull();
    RTCRestArray<RTCRestAnyObject> *pData = new (std::nothrow) RTCRestArray<RTCRestAnyObject>();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe copy assignment method, string map variant.
 */
int RTCRestAnyObject::assignCopy(RTCRestStringMap<RTCRestAnyObject> const &a_rThat)
{
    setNull();
    RTCRestStringMap<RTCRestAnyObject> *pData = new (std::nothrow) RTCRestStringMap<RTCRestAnyObject>();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignCopy(a_rThat);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, boolean variant.
 */
int RTCRestAnyObject::assignValue(bool a_fValue)
{
    setNull();
    RTCRestBool *pData = new (std::nothrow) RTCRestBool();
    if (pData)
    {
        m_pData = pData;
        pData->assignValue(a_fValue);
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, int64_t variant.
 */
int RTCRestAnyObject::assignValue(int64_t a_iValue)
{
    setNull();
    RTCRestInt64 *pData = new (std::nothrow) RTCRestInt64();
    if (pData)
    {
        m_pData = pData;
        pData->assignValue(a_iValue);
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, int32_t variant.
 */
int RTCRestAnyObject::assignValue(int32_t a_iValue)
{
    setNull();
    RTCRestInt32 *pData = new (std::nothrow) RTCRestInt32();
    if (pData)
    {
        m_pData = pData;
        pData->assignValue(a_iValue);
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, int16_t variant.
 */
int RTCRestAnyObject::assignValue(int16_t a_iValue)
{
    setNull();
    RTCRestInt16 *pData = new (std::nothrow) RTCRestInt16();
    if (pData)
    {
        m_pData = pData;
        pData->assignValue(a_iValue);
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, double variant.
 */
int RTCRestAnyObject::assignValue(double a_iValue)
{
    setNull();
    RTCRestDouble *pData = new (std::nothrow) RTCRestDouble();
    if (pData)
    {
        m_pData = pData;
        pData->assignValue(a_iValue);
        m_fNullIndicator = false;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, string variant.
 */
int RTCRestAnyObject::assignValue(RTCString const &a_rValue)
{
    setNull();
    RTCRestString *pData = new (std::nothrow) RTCRestString();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignNoThrow(a_rValue);
    }
    return VERR_NO_MEMORY;
}


/**
 * Safe value assignment method, C-string variant.
 */
int RTCRestAnyObject::assignValue(const char *a_pszValue)
{
    setNull();
    RTCRestString *pData = new (std::nothrow) RTCRestString();
    if (pData)
    {
        m_pData = pData;
        m_fNullIndicator = false;
        return pData->assignNoThrow(a_pszValue);
    }
    return VERR_NO_MEMORY;
}


int RTCRestAnyObject::setNull(void)
{
    if (m_pData)
    {
        delete m_pData;
        m_pData = NULL;
    }
    return RTCRestObjectBase::setNull();
}


int RTCRestAnyObject::resetToDefault()
{
    if (m_pData)
        return m_pData->resetToDefault();
    return VINF_SUCCESS;
}


RTCRestOutputBase &RTCRestAnyObject::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    if (m_pData)
        return m_pData->serializeAsJson(a_rDst);
    a_rDst.printf("null");
    return a_rDst;
}


int RTCRestAnyObject::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    setNull();

    RTJSONVALTYPE enmType = RTJsonValueGetType(a_rCursor.m_hValue);
    switch (enmType)
    {
        case RTJSONVALTYPE_OBJECT:
        {
            RTCRestStringMap<RTCRestAnyObject> *pMap = new (std::nothrow) RTCRestStringMap<RTCRestAnyObject>();
            if (pMap)
            {
                m_pData = pMap;
                m_fNullIndicator = false;
                return pMap->deserializeFromJson(a_rCursor);
            }
            break;
        }

        case RTJSONVALTYPE_ARRAY:
        {
            RTCRestArray<RTCRestAnyObject> *pArray = new (std::nothrow) RTCRestArray<RTCRestAnyObject>();
            if (pArray)
            {
                m_pData = pArray;
                m_fNullIndicator = false;
                return pArray->deserializeFromJson(a_rCursor);
            }
            break;
        }

        case RTJSONVALTYPE_STRING:
        {
            RTCRestString *pString = new (std::nothrow) RTCRestString();
            if (pString)
            {
                m_pData = pString;
                m_fNullIndicator = false;
                return pString->deserializeFromJson(a_rCursor);
            }
            break;
        }

        case RTJSONVALTYPE_INTEGER:
        {
            RTCRestInt64 *pInt64 = new (std::nothrow) RTCRestInt64();
            if (pInt64)
            {
                m_pData = pInt64;
                m_fNullIndicator = false;
                return pInt64->deserializeFromJson(a_rCursor);
            }
            break;
        }

        case RTJSONVALTYPE_NUMBER:
        {
            RTCRestDouble *pDouble = new (std::nothrow) RTCRestDouble();
            if (pDouble)
            {
                m_pData = pDouble;
                m_fNullIndicator = false;
                return pDouble->deserializeFromJson(a_rCursor);
            }
            break;
        }

        case RTJSONVALTYPE_NULL:
            return VINF_SUCCESS;

        case RTJSONVALTYPE_TRUE:
        case RTJSONVALTYPE_FALSE:
        {
            RTCRestBool *pBool = new (std::nothrow) RTCRestBool();
            if (pBool)
            {
                m_pData = pBool;
                m_fNullIndicator = false;
                pBool->assignValue(enmType == RTJSONVALTYPE_TRUE);
                return VINF_SUCCESS;
            }
            break;
        }

        /* no break. */
        case RTJSONVALTYPE_INVALID:
        case RTJSONVALTYPE_32BIT_HACK:
            break;
    }
    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_WRONG_TYPE, "RTCRestAnyObject found %d (%s)",
                                          enmType, RTJsonValueTypeName(enmType));
}


int RTCRestAnyObject::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    if (m_pData)
        return m_pData->toString(a_pDst, a_fFlags);
    if (a_fFlags & kToString_Append)
        return a_pDst->appendNoThrow(RT_STR_TUPLE("null"));
    return a_pDst->assignNoThrow(RT_STR_TUPLE("null"));
}


int RTCRestAnyObject::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo /*= NULL*/,
                                 uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    return RTCRestObjectBase::fromString(a_rValue, a_pszName, a_pErrInfo, a_fFlags);
}


RTCRestObjectBase::kTypeClass RTCRestAnyObject::typeClass(void) const
{
    return kTypeClass_Object;
}


const char *RTCRestAnyObject::typeName(void) const
{
    if (m_pData)
    {
        kTypeClass enmType = m_pData->typeClass();
        switch (enmType)
        {
            case kTypeClass_Bool:       return "RTCRestAnyObject[Bool]";
            case kTypeClass_Int64:      return "RTCRestAnyObject[Int64]";
            case kTypeClass_Int32:      return "RTCRestAnyObject[Int32]";
            case kTypeClass_Int16:      return "RTCRestAnyObject[Int16]";
            case kTypeClass_Double:     return "RTCRestAnyObject[Double]";
            case kTypeClass_String:     return "RTCRestAnyObject[String]";
            case kTypeClass_Array:      return "RTCRestAnyObject[Array]";
            case kTypeClass_StringMap:  return "RTCRestAnyObject[StringMap]";

            /* Currently unused of invalid: */
            case kTypeClass_StringEnum:
            case kTypeClass_BinaryString:
            case kTypeClass_Object:
            case kTypeClass_Invalid:
                AssertFailed();
        }
    }
    return "RTCRestAnyObject";
}


/**
 * Factory method.
 */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestAnyObject::createInstance(void)
{
    return new (std::nothrow) RTCRestAnyObject();
}

