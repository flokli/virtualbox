/* $Id$ */
/** @file
 * IPRT Testcase - REST C++ classes.
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
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;


static char *toJson(RTCRestObjectBase const *pObj)
{
    RTCString str;
    RTCRestOutputToString Dst(&str, false);
    pObj->serializeAsJson(Dst);

    static char s_szReturnBuffer[4096];
    RTStrCopy(s_szReturnBuffer, sizeof(s_szReturnBuffer), str.c_str());
    return s_szReturnBuffer;
}


static int deserializeFromJson(RTCRestObjectBase *pObj, const char *pszJson, PRTERRINFOSTATIC pErrInfo, const char *pszName)
{
    RTJSONVAL hValue;
    RTTESTI_CHECK_RC_OK_RET(RTJsonParseFromString(&hValue, pszJson, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL), rcCheck);
    RTCRestJsonPrimaryCursor Cursor(hValue, pszName, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL);
    return pObj->deserializeFromJson(Cursor.m_Cursor);
}


static int fromString(RTCRestObjectBase *pObj, const char *pszString, PRTERRINFOSTATIC pErrInfo, const char *pszName)
{
    RTCString strValue(pszString);
    return pObj->fromString(strValue, pszName, pErrInfo ? RTErrInfoInitStatic(pErrInfo) : NULL);
}


static void testBool(void)
{
    RTTestSub(g_hTest, "RTCRestBool");

    {
        RTCRestBool obj1;
        RTTESTI_CHECK(obj1.m_fValue == false);
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), "bool") == 0);
        RTTESTI_CHECK(obj1.typeClass() == RTCRestObjectBase::kTypeClass_Bool);
    }

    {
        RTCRestBool obj2(true);
        RTTESTI_CHECK(obj2.m_fValue == true);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RTCRestBool obj2(false);
        RTTESTI_CHECK(obj2.m_fValue == false);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RTCRestBool obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(true);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(false);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(true);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(true);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RTCRestBool obj3True(true);
        RTTESTI_CHECK(obj3True.m_fValue == true);
        RTTESTI_CHECK(obj3True.isNull() == false);
        RTCRestBool obj3False(false);
        RTTESTI_CHECK(obj3False.m_fValue == false);
        RTTESTI_CHECK(obj3False.isNull() == false);
        RTCRestBool obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.m_fValue == false);
        RTTESTI_CHECK(obj3Null.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3True), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3False), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3True;
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3False;
        RTTESTI_CHECK(obj3.m_fValue == false);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3True;
        RTTESTI_CHECK(obj3.m_fValue == true);
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_fValue == false);

        /* Copy constructors: */
        {
            RTCRestBool obj3a(obj3True);
            RTTESTI_CHECK(obj3a.m_fValue == true);
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RTCRestBool obj3b(obj3False);
            RTTESTI_CHECK(obj3b.m_fValue == false);
            RTTESTI_CHECK(obj3b.isNull() == false);
        }
        {
            RTCRestBool obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.m_fValue == false);
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3True);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "true") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3False);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "false") == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3True.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:true"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3True.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("true"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3False.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:false"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3False.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("false"), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        RTCRestBool obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "true", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        /* object goes to default state on failure: */
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "0", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.assignValue(true);
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"false\"", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_BOOL);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "true", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "false", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.m_fValue = true;
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, " TrUe ", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == true);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\tfAlSe;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\nfAlSe\n;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\tNuLl\n;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_fValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        RTTESTI_CHECK_RC(fromString(&obj4, "1", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_UNABLE_TO_PARSE_STRING_AS_BOOL);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        RTTESTI_CHECK_RC(fromString(&obj4, "0", NULL, RT_XSTR(__LINE__)), VERR_REST_UNABLE_TO_PARSE_STRING_AS_BOOL);
    }
}

class Int64Constants
{
public:
    const char *getSubName()  const { return "RTCRestInt64"; }
    int64_t     getMin()      const { return INT64_MIN; }
    const char *getMinStr()   const { return "-9223372036854775808"; }
    int64_t     getMax()      const { return INT64_MAX; }
    const char *getMaxStr()   const { return "9223372036854775807"; }
    const char *getTypeName() const { return "int64_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int64; }
};

class Int32Constants
{
public:
    const char *getSubName()  const { return "RTCRestInt32"; }
    int32_t     getMin()      const { return INT32_MIN; }
    const char *getMinStr()   const { return "-2147483648"; }
    int32_t     getMax()      const { return INT32_MAX; }
    const char *getMaxStr()   const { return "2147483647"; }
    const char *getTypeName() const { return "int32_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int32; }
};

class Int16Constants
{
public:
    const char *getSubName()  const { return "RTCRestInt16"; }
    int16_t     getMin()      const { return INT16_MIN; }
    const char *getMinStr()   const { return "-32768"; }
    int16_t     getMax()      const { return INT16_MAX; }
    const char *getMaxStr()   const { return "32767"; }
    const char *getTypeName() const { return "int16_t"; }
    RTCRestObjectBase::kTypeClass getTypeClass() const { return RTCRestObjectBase::kTypeClass_Int16; }
};

template<typename RestType, typename IntType, typename ConstantClass>
void testInteger(void)
{
    ConstantClass const Consts;
    RTTestSub(g_hTest, Consts.getSubName());

    {
        RestType obj1;
        RTTESTI_CHECK(obj1.m_iValue == 0);
        RTTESTI_CHECK(obj1.isNull() == false);
        RTTESTI_CHECK(strcmp(obj1.typeName(), Consts.getTypeName()) == 0);
        RTTESTI_CHECK(obj1.typeClass() == Consts.getTypeClass());
    }

    {
        RestType obj2(2398);
        RTTESTI_CHECK(obj2.m_iValue == 2398);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        RestType obj2(-7345);
        RTTESTI_CHECK(obj2.m_iValue == -7345);
        RTTESTI_CHECK(obj2.isNull() == false);
    }

    {
        /* Value assignments: */
        RestType obj3;
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(-1);
        RTTESTI_CHECK(obj3.m_iValue == -1);
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        obj3.assignValue(42);
        RTTESTI_CHECK(obj3.m_iValue == 42);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(Consts.getMax());
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(Consts.getMin());
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3.assignValue(42);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.resetToDefault(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == false);

        /* Copy assignments: */
        RestType obj3Max(Consts.getMax());
        RTTESTI_CHECK(obj3Max.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3Max.isNull() == false);
        RestType obj3Min(Consts.getMin());
        RTTESTI_CHECK(obj3Min.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3Min.isNull() == false);
        RestType obj3Null;
        obj3Null.setNull();
        RTTESTI_CHECK(obj3Null.m_iValue == 0);
        RTTESTI_CHECK(obj3Null.isNull() == true);

        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Max), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Null), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        RTTESTI_CHECK_RC(obj3.assignCopy(obj3Min), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);

        obj3 = obj3Null;
        RTTESTI_CHECK(obj3.m_iValue == 0);
        RTTESTI_CHECK(obj3.isNull() == true);

        obj3 = obj3Min;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj3.isNull() == false);

        /* setNull implies resetToDefault: */
        obj3 = obj3Max;
        RTTESTI_CHECK(obj3.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj3.isNull() == false);
        RTTESTI_CHECK_RC(obj3.setNull(), VINF_SUCCESS);
        RTTESTI_CHECK(obj3.isNull() == true);
        RTTESTI_CHECK(obj3.m_iValue == 0);

        /* Copy constructors: */
        {
            RestType obj3a(obj3Max);
            RTTESTI_CHECK(obj3a.m_iValue == Consts.getMax());
            RTTESTI_CHECK(obj3a.isNull() == false);
        }
        {
            RestType obj3b(obj3Min);
            RTTESTI_CHECK(obj3b.m_iValue == Consts.getMin());
            RTTESTI_CHECK(obj3b.isNull() == false);
        }
        {
            RestType obj3c(obj3Null);
            RTTESTI_CHECK(obj3c.m_iValue == 0);
            RTTESTI_CHECK(obj3c.isNull() == true);
        }

        /* Serialization to json: */
        const char *pszJson = toJson(&obj3Max);
        RTTESTI_CHECK_MSG(strcmp(pszJson, Consts.getMaxStr()) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Min);
        RTTESTI_CHECK_MSG(strcmp(pszJson, Consts.getMinStr()) == 0, ("pszJson=%s\n", pszJson));
        pszJson = toJson(&obj3Null);
        RTTESTI_CHECK_MSG(strcmp(pszJson, "null") == 0, ("pszJson=%s\n", pszJson));

        /* Serialization to string. */
        RTCString str;
        RTCString strExpect;
        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Max.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", Consts.getMaxStr());
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Max.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(Consts.getMaxStr()), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Min.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        strExpect.printf("lead-in:%s", Consts.getMinStr());
        RTTESTI_CHECK_MSG(str.equals(strExpect), ("str=%s strExpect=%s\n", str.c_str(), strExpect.c_str()));
        RTTESTI_CHECK_RC(obj3Min.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals(Consts.getMinStr()), ("str=%s\n", str.c_str()));

        str = "lead-in:";
        RTTESTI_CHECK_RC(obj3Null.toString(&str, RTCRestObjectBase::kToString_Append), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("lead-in:null"), ("str=%s\n", str.c_str()));
        RTTESTI_CHECK_RC(obj3Null.toString(&str), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(str.equals("null"), ("str=%s\n", str.c_str()));
    }

    /* deserialize: */
    RTERRINFOSTATIC ErrInfo;
    {
        /* from json: */
        RestType obj4;
        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 42);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "-22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == -22);
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, Consts.getMaxStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, Consts.getMinStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == false);
        RTTESTI_CHECK(obj4.isNull() == true);

        /* object goes to default state on failure: */
        obj4.assignValue(Consts.getMin());
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "0.0", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.assignValue(Consts.getMax());
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "\"false\"", &ErrInfo, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        obj4.setNull();
        RTTESTI_CHECK_RC(deserializeFromJson(&obj4, "[ null ]", NULL, RT_XSTR(__LINE__)), VERR_REST_WRONG_JSON_TYPE_FOR_INTEGER);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == false);

        /* from string: */
        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, "22", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 22);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "-42", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == -42);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, Consts.getMaxStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMax());
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, Consts.getMinStr(), &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == Consts.getMin());
        RTTESTI_CHECK(obj4.isNull() == false);

        obj4.m_iValue = 33;
        RTTESTI_CHECK_RC(fromString(&obj4, "null", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.m_iValue = 33;
        RTTESTI_CHECK_RC(fromString(&obj4, " nULl;", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0);
        RTTESTI_CHECK(obj4.isNull() == true);

        obj4.setNull();
        RTTESTI_CHECK_RC(fromString(&obj4, " 0x42 ", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0x42);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\t010\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 8);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "\r\t0X4FDB\t", &ErrInfo, RT_XSTR(__LINE__)), VINF_SUCCESS);
        RTTESTI_CHECK(obj4.m_iValue == 0x4fdb);
        RTTESTI_CHECK(obj4.isNull() == false);

        RTTESTI_CHECK_RC(fromString(&obj4, "1.1", &ErrInfo, RT_XSTR(__LINE__)), VERR_TRAILING_CHARS);
        RTTESTI_CHECK(RTErrInfoIsSet(&ErrInfo.Core));

        RTTESTI_CHECK_RC(fromString(&obj4, "false", NULL, RT_XSTR(__LINE__)), VERR_NO_DIGITS);
    }
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTRest-1", &g_hTest);
    if (rcExit == RTEXITCODE_SUCCESS )
    {
        testBool();
        testInteger<RTCRestInt64, int64_t, Int64Constants>();
        testInteger<RTCRestInt32, int32_t, Int32Constants>();
        testInteger<RTCRestInt16, int16_t, Int16Constants>();


        rcExit = RTTestSummaryAndDestroy(g_hTest);
    }
    return rcExit;
}


