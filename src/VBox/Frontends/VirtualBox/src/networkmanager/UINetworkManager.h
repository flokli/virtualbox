/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkManager class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_networkmanager_UINetworkManager_h
#define FEQT_INCLUDED_SRC_networkmanager_UINetworkManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class CHostNetworkInterface;
class QAbstractButton;
class QTreeWidgetItem;
class QVBoxLayout;
class QIDialogButtonBox;
class QITabWidget;
class QITreeWidget;
class UIActionPool;
class UINetworkDetailsWidget;
class UIItemHostNetwork;
class QIToolBar;
struct UIDataHostNetwork;


/** QWidget extension providing GUI with the pane to control network related functionality. */
class UINetworkManagerWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about network details-widget @a fVisible. */
    void sigDetailsVisibilityChangedHostNetwork(bool fVisible);
    /** Notifies listeners about network details data @a fDiffers. */
    void sigDetailsDataChangedHostNetwork(bool fDiffers);

public:

    /** Constructs Network Manager widget.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  pActionPool   Brings the action-pool reference.
      * @param  fShowToolbar  Brings whether we should create/show toolbar. */
    UINetworkManagerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                           bool fShowToolbar = true, QWidget *pParent = 0);

    /** Returns the menu. */
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** @} */

public slots:

    /** @name Details-widget stuff.
      * @{ */
        /** Handles command to reset details changes. */
        void sltResetDetailsChanges();
        /** Handles command to apply details changes. */
        void sltApplyDetailsChanges();
    /** @} */

private slots:

    /** @name Menu/action stuff.
      * @{ */
        /** Handles command to create host network. */
        void sltCreateHostNetwork();
        /** Handles command to remove host network. */
        void sltRemoveHostNetwork();
        /** Handles command to make host network details @a fVisible. */
        void sltToggleDetailsVisibilityHostNetwork(bool fVisible);
        /** Handles command to refresh host networks. */
        void sltRefreshHostNetworks();
    /** @} */

    /** @name Tree-widget stuff.
      * @{ */
        /** Handles command to adjust tree-widget. */
        void sltAdjustTreeWidgets();

        /** Handles host network tree-widget @a pItem change. */
        void sltHandleItemChangeHostNetwork(QTreeWidgetItem *pItem);
        /** Handles host network tree-widget current item change. */
        void sltHandleCurrentItemChangeHostNetwork();
        /** Handles host network context-menu request for tree-widget @a position. */
        void sltHandleContextMenuRequestHostNetwork(const QPoint &position);

        /** Handles command to apply host network details changes. */
        void sltApplyDetailsChangesHostNetwork();
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares actions. */
        void prepareActions();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares toolbar. */
        void prepareToolBar();
        /** Prepares tab-widget. */
        void prepareTabWidget();
        /** Prepares host network tab. */
        void prepareTabHostNetwork();
        /** Prepares host network tree-widget. */
        void prepareTreeWidgetHostNetwork();
        /** Prepares host network details-widget. */
        void prepareDetailsWidgetHostNetwork();
        /** Load settings: */
        void loadSettings();
    /** @} */

    /** @name Loading stuff.
      * @{ */
        /** Loads host networks. */
        void loadHostNetworks();
        /** Loads host @a comInterface data to passed @a data container. */
        void loadHostNetwork(const CHostNetworkInterface &comInterface, UIDataHostNetwork &data);
    /** @} */

    /** @name Tree-widget stuff.
      * @{ */
        /** Creates a new tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void createItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem);
        /** Updates the passed tree-widget item on the basis of passed @a data, @a fChooseItem if requested. */
        void updateItemForNetworkHost(const UIDataHostNetwork &data, bool fChooseItem, UIItemHostNetwork *pItem);
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the widget embedding type. */
        const EmbedTo  m_enmEmbedding;
        /** Holds the action-pool reference. */
        UIActionPool  *m_pActionPool;
        /** Holds whether we should create/show toolbar. */
        const bool     m_fShowToolbar;
    /** @} */

    /** @name Toolbar and menu variables.
      * @{ */
        /** Holds the toolbar instance. */
        QIToolBar *m_pToolBar;
    /** @} */

    /** @name Splitter variables.
      * @{ */
        /** Holds the tab-widget instance. */
        QITabWidget            *m_pTabWidget;

        /** Holds the host network tab. */
        QWidget                *m_pTabHostNetwork;
        /** Holds the host network layout. */
        QVBoxLayout            *m_pLayoutHostNetwork;
        /** Holds the host network tree-widget instance. */
        QITreeWidget           *m_pTreeWidgetHostNetwork;
        /** Holds the host network details-widget instance. */
        UINetworkDetailsWidget *m_pDetailsWidgetHostNetwork;
    /** @} */
};


/** QIManagerDialogFactory extension used as a factory for Host Network Manager dialog. */
class UINetworkManagerFactory : public QIManagerDialogFactory
{
public:

    /** Constructs Media Manager factory acquiring additional arguments.
      * @param  pActionPool  Brings the action-pool reference. */
    UINetworkManagerFactory(UIActionPool *pActionPool = 0);

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;
};


/** QIManagerDialog extension providing GUI with the dialog to control host network related functionality. */
class UINetworkManager : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data change rejected and should be reseted. */
    void sigDataChangeRejected();
    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

private slots:

    /** @name Button-box stuff.
      * @{ */
        /** Handles button-box button click. */
        void sltHandleButtonBoxClick(QAbstractButton *pButton);
    /** @} */

private:

    /** Constructs Host Network Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to.
      * @param  pActionPool    Brings the action-pool reference. */
    UINetworkManager(QWidget *pCenterWidget, UIActionPool *pActionPool);

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
        virtual void configure() /* override */;
        /** Configures central-widget. */
        virtual void configureCentralWidget() /* override */;
        /** Configures button-box. */
        virtual void configureButtonBox() /* override */;
        /** Perform final preparations. */
        virtual void finalize() /* override */;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Returns the widget. */
        virtual UINetworkManagerWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class UINetworkManagerFactory;
};

#endif /* !FEQT_INCLUDED_SRC_networkmanager_UINetworkManager_h */

