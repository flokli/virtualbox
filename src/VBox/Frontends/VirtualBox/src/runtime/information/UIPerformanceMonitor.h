/* $Id$ */
/** @file
 * VBox Qt GUI - UIPerformanceMonitor class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h
#define FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QMap>
#include <QQueue>


/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CPerformanceCollector.h"

/* GUI includes: */
#include "UIMainEventListener.h"

/* Forward declarations: */
class QTimer;
class QGridLayout;
class UIChart;

class UISubMetric
{
public:

    UISubMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize);
    UISubMetric();
    const QString &name() const;

    void setMaximum(float fMaximum);
    float maximum() const;

    void setMinimum(float fMinimum);
    float minimum() const;

    void setAverage(float fAverage);
    float average() const;

    void setUnit(QString strUnit);
    const QString &unit() const;

    void addData(ULONG fData);
    const QQueue<ULONG> &data() const;

    bool isPercentage() const;

private:

    QString m_strName;
    QString m_strUnit;
    float m_fMaximum;
    float m_fMinimum;
    float m_fAverage;
    QQueue<ULONG> m_data;
    int m_iMaximumQueueSize;
};

class UIPerformanceMonitor : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs information-tab passing @a pParent to the QWidget base-class constructor.
      * @param machine is machine reference.
      * @param console is machine console reference. */
    UIPerformanceMonitor(QWidget *pParent, const CMachine &machine, const CConsole &console);
    ~UIPerformanceMonitor();

private slots:

    void sltTimeout();

private:

    /** Prepares layout. */
    void prepareObjects();
    void preparePerformaceCollector();
    bool m_fGuestAdditionsAvailable;
    /** Holds the machine instance. */
    CMachine m_machine;
    /** Holds the console instance. */
    CConsole m_console;
    CGuest m_comGuest;

    CPerformanceCollector m_performanceMonitor;
    CMachineDebugger      m_machineDebugger;
    /** Holds the instance of layout we create. */
    QGridLayout *m_pMainLayout;
    QTimer *m_pTimer;

    QVector<QString> m_nameList;
    QVector<CUnknown> m_objectList;

    QMap<QString, UISubMetric> m_subMetrics;
    QMultiMap<QString,UIChart*>  m_charts;
    QString m_strCPUMetricName;
    QString m_strRAMMetricName;

};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIPerformanceMonitor_h */