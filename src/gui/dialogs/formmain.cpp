// This file is part of RSS Guard.
//
// Copyright (C) 2011-2016 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#include "gui/dialogs/formmain.h"

#include "definitions/definitions.h"
#include "miscellaneous/settings.h"
#include "miscellaneous/application.h"
#include "miscellaneous/systemfactory.h"
#include "miscellaneous/mutex.h"
#include "miscellaneous/databasefactory.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/feedreader.h"
#include "network-web/webfactory.h"
#include "gui/feedsview.h"
#include "gui/messagebox.h"
#include "gui/systemtrayicon.h"
#include "gui/tabbar.h"
#include "gui/statusbar.h"
#include "gui/messagesview.h"
#include "gui/feedmessageviewer.h"
#include "gui/plaintoolbutton.h"
#include "gui/feedstoolbar.h"
#include "gui/messagestoolbar.h"
#include "gui/dialogs/formabout.h"
#include "gui/dialogs/formsettings.h"
#include "gui/dialogs/formupdate.h"
#include "gui/dialogs/formdatabasecleanup.h"
#include "gui/dialogs/formbackupdatabasesettings.h"
#include "gui/dialogs/formrestoredatabasesettings.h"
#include "gui/dialogs/formaddaccount.h"
#include "services/abstract/serviceroot.h"
#include "services/abstract/recyclebin.h"
#include "services/standard/gui/formstandardimportexport.h"
#include "services/owncloud/network/owncloudnetworkfactory.h"

#include <QCloseEvent>
#include <QRect>
#include <QScopedPointer>
#include <QDesktopWidget>
#include <QTimer>
#include <QFileDialog>


FormMain::FormMain(QWidget *parent, Qt::WindowFlags f)
  : QMainWindow(parent, f), m_ui(new Ui::FormMain) {
  m_ui->setupUi(this);
  qApp->setMainForm(this);

  // Add these actions to the list of actions of the main window.
  // This allows to use actions via shortcuts
  // even if main menu is not visible.
  addActions(allActions());

  m_statusBar = new StatusBar(this);
  setStatusBar(m_statusBar);

  // Prepare main window and tabs.
  prepareMenus();

  // Prepare tabs.
  //m_ui->m_tabWidget->initializeTabs();
  tabWidget()->feedMessageViewer()->feedsToolBar()->loadChangeableActions();
  tabWidget()->feedMessageViewer()->messagesToolBar()->loadChangeableActions();

  // Establish connections.
  createConnections();

  updateMessageButtonsAvailability();
  updateFeedButtonsAvailability();

  // Setup some appearance of the window.
  setupIcons();
  loadSize();

  m_statusBar->loadChangeableActions();
}

FormMain::~FormMain() {
  qDebug("Destroying FormMain instance.");
}

QMenu *FormMain::trayMenu() const {
  return m_trayMenu;
}

TabWidget *FormMain::tabWidget() const {
  return m_ui->m_tabWidget;
}

StatusBar *FormMain::statusBar() const {
  return m_statusBar;
}

void FormMain::showDbCleanupAssistant() {
  if (qApp->feedUpdateLock()->tryLock()) {
    QScopedPointer<FormDatabaseCleanup> form_pointer(new FormDatabaseCleanup(this));
    form_pointer.data()->setCleaner(qApp->feedReader()->databaseCleaner());
    form_pointer.data()->exec();

    qApp->feedUpdateLock()->unlock();

    tabWidget()->feedMessageViewer()->messagesView()->reloadSelections(false);
    qApp->feedReader()->feedsModel()->reloadCountsOfWholeModel();
  }
  else {
    qApp->showGuiMessage(tr("Cannot cleanup database"),
                         tr("Cannot cleanup database, because another critical action is running."),
                         QSystemTrayIcon::Warning, qApp->mainFormWidget(), true);
  }
}

QList<QAction*> FormMain::allActions() const {
  QList<QAction*> actions;

  // Add basic actions.
  actions << m_ui->m_actionSettings;
  actions << m_ui->m_actionDownloadManager;
  actions << m_ui->m_actionRestoreDatabaseSettings;
  actions << m_ui->m_actionBackupDatabaseSettings;
  actions << m_ui->m_actionQuit;
  actions << m_ui->m_actionFullscreen;
  actions << m_ui->m_actionAboutGuard;
  actions << m_ui->m_actionSwitchFeedsList;
  actions << m_ui->m_actionSwitchMainWindow;
  actions << m_ui->m_actionSwitchMainMenu;
  actions << m_ui->m_actionSwitchToolBars;
  actions << m_ui->m_actionSwitchListHeaders;
  actions << m_ui->m_actionSwitchStatusBar;
  actions << m_ui->m_actionSwitchMessageListOrientation;

  // Add feeds/messages actions.
  actions << m_ui->m_actionOpenSelectedSourceArticlesExternally;
  actions << m_ui->m_actionOpenSelectedMessagesInternally;
  actions << m_ui->m_actionMarkAllItemsRead;
  actions << m_ui->m_actionMarkSelectedItemsAsRead;
  actions << m_ui->m_actionMarkSelectedItemsAsUnread;
  actions << m_ui->m_actionClearSelectedItems;
  actions << m_ui->m_actionClearAllItems;
  actions << m_ui->m_actionShowOnlyUnreadItems;
  actions << m_ui->m_actionMarkSelectedMessagesAsRead;
  actions << m_ui->m_actionMarkSelectedMessagesAsUnread;
  actions << m_ui->m_actionSwitchImportanceOfSelectedMessages;
  actions << m_ui->m_actionDeleteSelectedMessages;
  actions << m_ui->m_actionUpdateAllItems;
  actions << m_ui->m_actionUpdateSelectedItems;
  actions << m_ui->m_actionStopRunningItemsUpdate;
  actions << m_ui->m_actionEditSelectedItem;
  actions << m_ui->m_actionDeleteSelectedItem;
  actions << m_ui->m_actionServiceAdd;
  actions << m_ui->m_actionServiceEdit;
  actions << m_ui->m_actionServiceDelete;
  actions << m_ui->m_actionCleanupDatabase;
  actions << m_ui->m_actionAddFeedIntoSelectedAccount;
  actions << m_ui->m_actionAddCategoryIntoSelectedAccount;
  actions << m_ui->m_actionViewSelectedItemsNewspaperMode;
  actions << m_ui->m_actionSelectNextItem;
  actions << m_ui->m_actionSelectPreviousItem;
  actions << m_ui->m_actionSelectNextMessage;
  actions << m_ui->m_actionSelectPreviousMessage;
  actions << m_ui->m_actionSelectNextUnreadMessage;
  actions << m_ui->m_actionExpandCollapseItem;

#if defined(USE_WEBENGINE)
  actions << m_ui->m_actionTabNewWebBrowser;
#endif

  actions << m_ui->m_actionTabsCloseAll;
  actions << m_ui->m_actionTabsCloseAllExceptCurrent;

  return actions;
}

void FormMain::prepareMenus() {
  // Setup menu for tray icon.
  if (SystemTrayIcon::isSystemTrayAvailable()) {
#if defined(Q_OS_WIN)
    m_trayMenu = new TrayIconMenu(APP_NAME, this);
#else
    m_trayMenu = new QMenu(APP_NAME, this);
#endif

    // Add needed items to the menu.
    m_trayMenu->addAction(m_ui->m_actionSwitchMainWindow);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_ui->m_actionUpdateAllItems);
    m_trayMenu->addAction(m_ui->m_actionMarkAllItemsRead);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_ui->m_actionSettings);
    m_trayMenu->addAction(m_ui->m_actionQuit);

    qDebug("Creating tray icon menu.");
  }

#if !defined(USE_WEBENGINE)
  m_ui->m_menuWebBrowserTabs->removeAction(m_ui->m_actionTabNewWebBrowser);
  m_ui->m_menuWebBrowserTabs->setTitle(tr("Tabs"));
#endif
}

void FormMain::switchFullscreenMode() {
  if (!isFullScreen()) {
    qApp->settings()->setValue(GROUP(GUI), GUI::IsMainWindowMaximizedBeforeFullscreen, isMaximized());
    showFullScreen();
  }
  else {
    if (qApp->settings()->value(GROUP(GUI), SETTING(GUI::IsMainWindowMaximizedBeforeFullscreen)).toBool()) {
      setWindowState((windowState() & ~Qt::WindowFullScreen) | Qt::WindowMaximized);
    }
    else {
      showNormal();
    }
  }
}

void FormMain::updateAddItemMenu() {
  // NOTE: Clear here deletes items from memory but only those OWNED by the menu.
  m_ui->m_menuAddItem->clear();

  foreach (ServiceRoot *activated_root, qApp->feedReader()->feedsModel()->serviceRoots()) {
    QMenu *root_menu = new QMenu(activated_root->title(), m_ui->m_menuAddItem);
    root_menu->setIcon(activated_root->icon());
    root_menu->setToolTip(activated_root->description());

    QList<QAction*> specific_root_actions = activated_root->addItemMenu();

    if (activated_root->supportsCategoryAdding()) {
      QAction *action_new_category = new QAction(qApp->icons()->fromTheme(QSL("folder")),
                                                 tr("Add new category"),
                                                 m_ui->m_menuAddItem);
      root_menu->addAction(action_new_category);
      connect(action_new_category, SIGNAL(triggered()), activated_root, SLOT(addNewCategory()));
    }

    if (activated_root->supportsFeedAdding()) {
      QAction *action_new_feed = new QAction(qApp->icons()->fromTheme(QSL("application-rss+xml")),
                                             tr("Add new feed"),
                                             m_ui->m_menuAddItem);
      root_menu->addAction(action_new_feed);
      connect(action_new_feed, SIGNAL(triggered()), activated_root, SLOT(addNewFeed()));
    }

    if (!specific_root_actions.isEmpty()) {
      if (!root_menu->isEmpty()) {
        root_menu->addSeparator();
      }

      root_menu->addActions(specific_root_actions);
    }

    m_ui->m_menuAddItem->addMenu(root_menu);
  }

  if (!m_ui->m_menuAddItem->isEmpty()) {
    m_ui->m_menuAddItem->addSeparator();
    m_ui->m_menuAddItem->addAction(m_ui->m_actionAddCategoryIntoSelectedAccount);
    m_ui->m_menuAddItem->addAction(m_ui->m_actionAddFeedIntoSelectedAccount);
  }
  else {
    m_ui->m_menuAddItem->addAction(m_ui->m_actionNoActions);
  }
}

void FormMain::updateRecycleBinMenu() {
  m_ui->m_menuRecycleBin->clear();

  foreach (const ServiceRoot *activated_root, qApp->feedReader()->feedsModel()->serviceRoots()) {
    QMenu *root_menu = new QMenu(activated_root->title(), m_ui->m_menuRecycleBin);
    root_menu->setIcon(activated_root->icon());
    root_menu->setToolTip(activated_root->description());

    RecycleBin *bin = activated_root->recycleBin();
    QList<QAction*> context_menu;

    if (bin == nullptr) {
      QAction *no_action = new QAction(qApp->icons()->fromTheme(QSL("dialog-error")),
                                       tr("No recycle bin"),
                                       m_ui->m_menuRecycleBin);
      no_action->setEnabled(false);
      root_menu->addAction(no_action);
    }
    else if ((context_menu = bin->contextMenu()).isEmpty()) {
      QAction *no_action = new QAction(qApp->icons()->fromTheme(QSL("dialog-error")),
                                       tr("No actions possible"),
                                       m_ui->m_menuRecycleBin);
      no_action->setEnabled(false);
      root_menu->addAction(no_action);
    }
    else {
      root_menu->addActions(context_menu);
    }

    m_ui->m_menuRecycleBin->addMenu(root_menu);
  }

  if (!m_ui->m_menuRecycleBin->isEmpty()) {
    m_ui->m_menuRecycleBin->addSeparator();
  }

  m_ui->m_menuRecycleBin->addAction(m_ui->m_actionRestoreAllRecycleBins);
  m_ui->m_menuRecycleBin->addAction(m_ui->m_actionEmptyAllRecycleBins);
}

void FormMain::updateAccountsMenu() {
  m_ui->m_menuAccounts->clear();

  foreach (ServiceRoot *activated_root, qApp->feedReader()->feedsModel()->serviceRoots()) {
    QMenu *root_menu = new QMenu(activated_root->title(), m_ui->m_menuAccounts);
    root_menu->setIcon(activated_root->icon());
    root_menu->setToolTip(activated_root->description());

    QList<QAction*> root_actions = activated_root->serviceMenu();

    if (root_actions.isEmpty()) {
      QAction *no_action = new QAction(qApp->icons()->fromTheme(QSL("dialog-error")),
                                       tr("No possible actions"),
                                       m_ui->m_menuAccounts);
      no_action->setEnabled(false);
      root_menu->addAction(no_action);
    }
    else {
      root_menu->addActions(root_actions);
    }

    m_ui->m_menuAccounts->addMenu(root_menu);
  }

  if (m_ui->m_menuAccounts->actions().size() > 0) {
    m_ui->m_menuAccounts->addSeparator();
  }

  m_ui->m_menuAccounts->addAction(m_ui->m_actionServiceAdd);
  m_ui->m_menuAccounts->addAction(m_ui->m_actionServiceEdit);
  m_ui->m_menuAccounts->addAction(m_ui->m_actionServiceDelete);
}

void FormMain::onFeedUpdatesFinished(FeedDownloadResults results) {
  Q_UNUSED(results)

  statusBar()->clearProgressFeeds();
  tabWidget()->feedMessageViewer()->messagesView()->reloadSelections(false);
}

void FormMain::onFeedUpdatesStarted() {
  m_ui->m_actionStopRunningItemsUpdate->setEnabled(true);
  statusBar()->showProgressFeeds(0, tr("Feed update started"));
}

void FormMain::onFeedUpdatesProgress(const Feed *feed, int current, int total) {
  statusBar()->showProgressFeeds((current * 100.0) / total,
                                 //: Text display in status bar when particular feed is updated.
                                 tr("Updated feed '%1'").arg(feed->title()));
}

void FormMain::updateMessageButtonsAvailability() {
  const bool one_message_selected = tabWidget()->feedMessageViewer()->messagesView()->selectionModel()->selectedRows().size() == 1;
  const bool atleast_one_message_selected = !tabWidget()->feedMessageViewer()->messagesView()->selectionModel()->selectedRows().isEmpty();
  const bool bin_loaded = tabWidget()->feedMessageViewer()->messagesView()->sourceModel()->loadedItem() != nullptr && tabWidget()->feedMessageViewer()->messagesView()->sourceModel()->loadedItem()->kind() == RootItemKind::Bin;

  m_ui->m_actionDeleteSelectedMessages->setEnabled(atleast_one_message_selected);
  m_ui->m_actionRestoreSelectedMessages->setEnabled(atleast_one_message_selected && bin_loaded);
  m_ui->m_actionMarkSelectedMessagesAsRead->setEnabled(atleast_one_message_selected);
  m_ui->m_actionMarkSelectedMessagesAsUnread->setEnabled(atleast_one_message_selected);
  m_ui->m_actionOpenSelectedMessagesInternally->setEnabled(atleast_one_message_selected);
  m_ui->m_actionOpenSelectedSourceArticlesExternally->setEnabled(atleast_one_message_selected);
  m_ui->m_actionSendMessageViaEmail->setEnabled(one_message_selected);
  m_ui->m_actionSwitchImportanceOfSelectedMessages->setEnabled(atleast_one_message_selected);
}

void FormMain::updateFeedButtonsAvailability() {
  const bool is_update_running = qApp->feedReader()->isFeedUpdateRunning();
  const bool critical_action_running = qApp->feedUpdateLock()->isLocked();
  const RootItem *selected_item = tabWidget()->feedMessageViewer()->feedsView()->selectedItem();
  const bool anything_selected = selected_item != nullptr;
  const bool feed_selected = anything_selected && selected_item->kind() == RootItemKind::Feed;
  const bool category_selected = anything_selected && selected_item->kind() == RootItemKind::Category;
  const bool service_selected = anything_selected && selected_item->kind() == RootItemKind::ServiceRoot;

  m_ui->m_actionStopRunningItemsUpdate->setEnabled(is_update_running);
  m_ui->m_actionBackupDatabaseSettings->setEnabled(!critical_action_running);
  m_ui->m_actionCleanupDatabase->setEnabled(!critical_action_running);
  m_ui->m_actionClearSelectedItems->setEnabled(anything_selected);
  m_ui->m_actionDeleteSelectedItem->setEnabled(!critical_action_running && anything_selected);
  m_ui->m_actionEditSelectedItem->setEnabled(!critical_action_running && anything_selected);
  m_ui->m_actionMarkSelectedItemsAsRead->setEnabled(anything_selected);
  m_ui->m_actionMarkSelectedItemsAsUnread->setEnabled(anything_selected);
  m_ui->m_actionUpdateAllItems->setEnabled(!critical_action_running);
  m_ui->m_actionUpdateSelectedItems->setEnabled(!critical_action_running && (feed_selected || category_selected || service_selected));
  m_ui->m_actionViewSelectedItemsNewspaperMode->setEnabled(anything_selected);
  m_ui->m_actionExpandCollapseItem->setEnabled(anything_selected);

  m_ui->m_actionServiceDelete->setEnabled(service_selected);
  m_ui->m_actionServiceEdit->setEnabled(service_selected);
  m_ui->m_actionAddFeedIntoSelectedAccount->setEnabled(anything_selected);
  m_ui->m_actionAddCategoryIntoSelectedAccount->setEnabled(anything_selected);

  m_ui->m_menuAddItem->setEnabled(!critical_action_running);
  m_ui->m_menuAccounts->setEnabled(!critical_action_running);
  m_ui->m_menuRecycleBin->setEnabled(!critical_action_running);
}

void FormMain::switchVisibility(bool force_hide) {
  if (force_hide || isVisible()) {
    if (SystemTrayIcon::isSystemTrayActivated()) {
      hide();
    }
    else {
      // Window gets minimized in single-window mode.
      showMinimized();
    }
  }
  else {
    display();
  }
}

void FormMain::display() {
  // Make sure window is not minimized.
  setWindowState(windowState() & ~Qt::WindowMinimized);

  // Display the window and make sure it is raised on top.
  show();
  activateWindow();
  raise();

  // Raise alert event. Check the documentation for more info on this.
  Application::alert(this);
}

void FormMain::setupIcons() {
  IconFactory *icon_theme_factory = qApp->icons();

  // Setup icons of this main window.
  m_ui->m_actionDownloadManager->setIcon(icon_theme_factory->fromTheme(QSL("emblem-downloads")));
  m_ui->m_actionSettings->setIcon(icon_theme_factory->fromTheme(QSL("document-properties")));
  m_ui->m_actionQuit->setIcon(icon_theme_factory->fromTheme(QSL("application-exit")));
  m_ui->m_actionAboutGuard->setIcon(icon_theme_factory->fromTheme(QSL("help-about")));
  m_ui->m_actionCheckForUpdates->setIcon(icon_theme_factory->fromTheme(QSL("system-upgrade")));
  m_ui->m_actionCleanupDatabase->setIcon(icon_theme_factory->fromTheme(QSL("edit-clear")));
  m_ui->m_actionReportBug->setIcon(icon_theme_factory->fromTheme(QSL("call-start")));
  m_ui->m_actionBackupDatabaseSettings->setIcon(icon_theme_factory->fromTheme(QSL("document-export")));
  m_ui->m_actionRestoreDatabaseSettings->setIcon(icon_theme_factory->fromTheme(QSL("document-import")));
  m_ui->m_actionDonate->setIcon(icon_theme_factory->fromTheme(QSL("applications-office")));
  m_ui->m_actionDisplayWiki->setIcon(icon_theme_factory->fromTheme(QSL("applications-science")));

  // View.
  m_ui->m_actionSwitchMainWindow->setIcon(icon_theme_factory->fromTheme(QSL("window-close")));
  m_ui->m_actionFullscreen->setIcon(icon_theme_factory->fromTheme(QSL("view-fullscreen")));
  m_ui->m_actionSwitchFeedsList->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));
  m_ui->m_actionSwitchMainMenu->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));
  m_ui->m_actionSwitchToolBars->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));
  m_ui->m_actionSwitchListHeaders->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));
  m_ui->m_actionSwitchStatusBar->setIcon(icon_theme_factory->fromTheme(QSL("dialog-information")));
  m_ui->m_actionSwitchMessageListOrientation->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));
  m_ui->m_menuShowHide->setIcon(icon_theme_factory->fromTheme(QSL("view-restore")));

  // Feeds/messages.
  m_ui->m_menuAddItem->setIcon(icon_theme_factory->fromTheme(QSL("list-add")));
  m_ui->m_actionStopRunningItemsUpdate->setIcon(icon_theme_factory->fromTheme(QSL("process-stop")));
  m_ui->m_actionUpdateAllItems->setIcon(icon_theme_factory->fromTheme(QSL("view-refresh")));
  m_ui->m_actionUpdateSelectedItems->setIcon(icon_theme_factory->fromTheme(QSL("view-refresh")));
  m_ui->m_actionClearSelectedItems->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-junk")));
  m_ui->m_actionClearAllItems->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-junk")));
  m_ui->m_actionDeleteSelectedItem->setIcon(icon_theme_factory->fromTheme(QSL("list-remove")));
  m_ui->m_actionDeleteSelectedMessages->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-junk")));
  m_ui->m_actionEditSelectedItem->setIcon(icon_theme_factory->fromTheme(QSL("document-edit")));
  m_ui->m_actionMarkAllItemsRead->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-read")));
  m_ui->m_actionMarkSelectedItemsAsRead->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-read")));
  m_ui->m_actionMarkSelectedItemsAsUnread->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-unread")));
  m_ui->m_actionMarkSelectedMessagesAsRead->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-read")));
  m_ui->m_actionMarkSelectedMessagesAsUnread->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-unread")));
  m_ui->m_actionSwitchImportanceOfSelectedMessages->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-important")));
  m_ui->m_actionOpenSelectedSourceArticlesExternally->setIcon(icon_theme_factory->fromTheme(QSL("document-open")));
  m_ui->m_actionOpenSelectedMessagesInternally->setIcon(icon_theme_factory->fromTheme(QSL("document-open")));
  m_ui->m_actionSendMessageViaEmail->setIcon(icon_theme_factory->fromTheme(QSL("mail-send")));
  m_ui->m_actionViewSelectedItemsNewspaperMode->setIcon(icon_theme_factory->fromTheme(QSL("format-justify-fill")));
  m_ui->m_actionSelectNextItem->setIcon(icon_theme_factory->fromTheme(QSL("go-down")));
  m_ui->m_actionSelectPreviousItem->setIcon(icon_theme_factory->fromTheme(QSL("go-up")));
  m_ui->m_actionSelectNextMessage->setIcon(icon_theme_factory->fromTheme(QSL("go-down")));
  m_ui->m_actionSelectPreviousMessage->setIcon(icon_theme_factory->fromTheme(QSL("go-up")));
  m_ui->m_actionSelectNextUnreadMessage->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-unread")));
  m_ui->m_actionShowOnlyUnreadItems->setIcon(icon_theme_factory->fromTheme(QSL("mail-mark-unread")));
  m_ui->m_actionExpandCollapseItem->setIcon(icon_theme_factory->fromTheme(QSL("format-indent-more")));
  m_ui->m_actionRestoreSelectedMessages->setIcon(icon_theme_factory->fromTheme(QSL("view-refresh")));
  m_ui->m_actionRestoreAllRecycleBins->setIcon(icon_theme_factory->fromTheme(QSL("view-refresh")));
  m_ui->m_actionEmptyAllRecycleBins->setIcon(icon_theme_factory->fromTheme(QSL("edit-clear")));
  m_ui->m_actionServiceAdd->setIcon(icon_theme_factory->fromTheme(QSL("list-add")));
  m_ui->m_actionServiceEdit->setIcon(icon_theme_factory->fromTheme(QSL("document-edit")));
  m_ui->m_actionServiceDelete->setIcon(icon_theme_factory->fromTheme(QSL("list-remove")));
  m_ui->m_actionAddFeedIntoSelectedAccount->setIcon(icon_theme_factory->fromTheme(QSL("application-rss+xml")));
  m_ui->m_actionAddCategoryIntoSelectedAccount->setIcon(icon_theme_factory->fromTheme(QSL("folder")));

  // Tabs & web browser.
  m_ui->m_actionTabNewWebBrowser->setIcon(icon_theme_factory->fromTheme(QSL("tab-new")));
  m_ui->m_actionTabsCloseAll->setIcon(icon_theme_factory->fromTheme(QSL("window-close")));
  m_ui->m_actionTabsCloseAllExceptCurrent->setIcon(icon_theme_factory->fromTheme(QSL("window-close")));

  // Setup icons on TabWidget too.
  m_ui->m_tabWidget->setupIcons();
}

void FormMain::loadSize() {
  const QRect screen = qApp->desktop()->screenGeometry();
  const Settings *settings = qApp->settings();

  // Reload main window size & position.
  resize(settings->value(GROUP(GUI), GUI::MainWindowInitialSize, size()).toSize());
  move(settings->value(GROUP(GUI), GUI::MainWindowInitialPosition, screen.center() - rect().center()).toPoint());

  if (settings->value(GROUP(GUI), SETTING(GUI::MainWindowStartsMaximized)).toBool()) {
    setWindowState(windowState() | Qt::WindowMaximized);

    // We process events so that window is really maximized fast.
    qApp->processEvents();
  }

  // If user exited the application while in fullsreen mode,
  // then re-enable it now.
  if (settings->value(GROUP(GUI), SETTING(GUI::MainWindowStartsFullscreen)).toBool()) {
    m_ui->m_actionFullscreen->setChecked(true);
  }

  // Hide the main menu if user wants it.
  m_ui->m_actionSwitchMainMenu->setChecked(settings->value(GROUP(GUI), SETTING(GUI::MainMenuVisible)).toBool());

  // Adjust dimensions of "feeds & messages" widget.
  m_ui->m_tabWidget->feedMessageViewer()->loadSize();
  m_ui->m_actionSwitchToolBars->setChecked(settings->value(GROUP(GUI), SETTING(GUI::ToolbarsVisible)).toBool());
  m_ui->m_actionSwitchListHeaders->setChecked(settings->value(GROUP(GUI), SETTING(GUI::ListHeadersVisible)).toBool());
  m_ui->m_actionSwitchStatusBar->setChecked(settings->value(GROUP(GUI), SETTING(GUI::StatusBarVisible)).toBool());

  // Make sure that only unread feeds are shown if user has that feature set on.
  m_ui->m_actionShowOnlyUnreadItems->setChecked(settings->value(GROUP(Feeds), SETTING(Feeds::ShowOnlyUnreadFeeds)).toBool());
}

void FormMain::saveSize() {
  Settings *settings = qApp->settings();
  bool is_fullscreen = isFullScreen();
  bool is_maximized = false;

  if (is_fullscreen) {
    m_ui->m_actionFullscreen->setChecked(false);

    // We (process events to really) un-fullscreen, so that we can determine if window is really maximized.
    qApp->processEvents();
  }

  if (isMaximized()) {
    is_maximized = true;

    // Window is maximized, we store that fact to settings and unmaximize.
    qApp->settings()->setValue(GROUP(GUI), GUI::IsMainWindowMaximizedBeforeFullscreen, isMaximized());
    setWindowState((windowState() & ~Qt::WindowMaximized) | Qt::WindowActive);

    // We process events to really have window un-maximized.
    qApp->processEvents();
  }

  settings->setValue(GROUP(GUI), GUI::MainMenuVisible, m_ui->m_actionSwitchMainMenu->isChecked());
  settings->setValue(GROUP(GUI), GUI::MainWindowInitialPosition, pos());
  settings->setValue(GROUP(GUI), GUI::MainWindowInitialSize, size());
  settings->setValue(GROUP(GUI), GUI::MainWindowStartsMaximized, is_maximized);
  settings->setValue(GROUP(GUI), GUI::MainWindowStartsFullscreen, is_fullscreen);
  settings->setValue(GROUP(GUI), GUI::StatusBarVisible, m_ui->m_actionSwitchStatusBar->isChecked());

  m_ui->m_tabWidget->feedMessageViewer()->saveSize();
}

void FormMain::createConnections() {
  // Status bar connections.
  connect(m_ui->m_menuAddItem, SIGNAL(aboutToShow()), this, SLOT(updateAddItemMenu()));
  connect(m_ui->m_menuRecycleBin, SIGNAL(aboutToShow()), this, SLOT(updateRecycleBinMenu()));
  connect(m_ui->m_menuAccounts, SIGNAL(aboutToShow()), this, SLOT(updateAccountsMenu()));

  connect(m_ui->m_actionServiceDelete, SIGNAL(triggered()), m_ui->m_actionDeleteSelectedItem, SIGNAL(triggered()));
  connect(m_ui->m_actionServiceEdit, SIGNAL(triggered()), m_ui->m_actionEditSelectedItem, SIGNAL(triggered()));

  // Menu "File" connections.
  connect(m_ui->m_actionBackupDatabaseSettings, SIGNAL(triggered()), this, SLOT(backupDatabaseSettings()));
  connect(m_ui->m_actionRestoreDatabaseSettings, SIGNAL(triggered()), this, SLOT(restoreDatabaseSettings()));
  connect(m_ui->m_actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
  connect(m_ui->m_actionServiceAdd, SIGNAL(triggered()), this, SLOT(showAddAccountDialog()));

  // Menu "View" connections.
  connect(m_ui->m_actionFullscreen, SIGNAL(toggled(bool)), this, SLOT(switchFullscreenMode()));
  connect(m_ui->m_actionSwitchMainMenu, SIGNAL(toggled(bool)), m_ui->m_menuBar, SLOT(setVisible(bool)));
  connect(m_ui->m_actionSwitchMainWindow, SIGNAL(triggered()), this, SLOT(switchVisibility()));
  connect(m_ui->m_actionSwitchStatusBar, SIGNAL(toggled(bool)), statusBar(), SLOT(setVisible(bool)));

  // Menu "Tools" connections.
  connect(m_ui->m_actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
  connect(m_ui->m_actionDownloadManager, SIGNAL(triggered()), m_ui->m_tabWidget, SLOT(showDownloadManager()));

  connect(m_ui->m_actionCleanupDatabase, SIGNAL(triggered()), this, SLOT(showDbCleanupAssistant()));

  // Menu "Help" connections.
  connect(m_ui->m_actionAboutGuard, SIGNAL(triggered()), this, SLOT(showAbout()));
  connect(m_ui->m_actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(showUpdates()));
  connect(m_ui->m_actionReportBug, SIGNAL(triggered()), this, SLOT(reportABug()));
  connect(m_ui->m_actionDonate, SIGNAL(triggered()), this, SLOT(donate()));
  connect(m_ui->m_actionDisplayWiki, SIGNAL(triggered()), this, SLOT(showWiki()));

  // Tab widget connections.
  connect(m_ui->m_actionTabsCloseAllExceptCurrent, &QAction::triggered, m_ui->m_tabWidget, &TabWidget::closeAllTabsExceptCurrent);
  connect(m_ui->m_actionTabsCloseAll, &QAction::triggered, m_ui->m_tabWidget, &TabWidget::closeAllTabs);
  connect(m_ui->m_actionTabNewWebBrowser, &QAction::triggered, m_ui->m_tabWidget, &TabWidget::addEmptyBrowser);

  connect(tabWidget()->feedMessageViewer()->feedsView(), &FeedsView::itemSelected, this, &FormMain::updateFeedButtonsAvailability);
  connect(qApp->feedUpdateLock(), &Mutex::locked, this, &FormMain::updateFeedButtonsAvailability);
  connect(qApp->feedUpdateLock(), &Mutex::unlocked, this, &FormMain::updateFeedButtonsAvailability);

  connect(tabWidget()->feedMessageViewer()->messagesView(), &MessagesView::currentMessageRemoved,
          this, &FormMain::updateMessageButtonsAvailability);
  connect(tabWidget()->feedMessageViewer()->messagesView(), &MessagesView::currentMessageChanged,
          this, &FormMain::updateMessageButtonsAvailability);

  connect(qApp->feedReader(), &FeedReader::feedUpdatesStarted, this, &FormMain::onFeedUpdatesStarted);
  connect(qApp->feedReader(), &FeedReader::feedUpdatesProgress, this, &FormMain::onFeedUpdatesProgress);
  connect(qApp->feedReader(), &FeedReader::feedUpdatesFinished, this, &FormMain::onFeedUpdatesFinished);

  // Toolbar forwardings.
  connect(m_ui->m_actionAddFeedIntoSelectedAccount, SIGNAL(triggered()),
          tabWidget()->feedMessageViewer()->feedsView(), SLOT(addFeedIntoSelectedAccount()));
  connect(m_ui->m_actionAddCategoryIntoSelectedAccount, SIGNAL(triggered()),
          tabWidget()->feedMessageViewer()->feedsView(), SLOT(addCategoryIntoSelectedAccount()));
  connect(m_ui->m_actionSwitchImportanceOfSelectedMessages,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(switchSelectedMessagesImportance()));
  connect(m_ui->m_actionDeleteSelectedMessages,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(deleteSelectedMessages()));
  connect(m_ui->m_actionMarkSelectedMessagesAsRead,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(markSelectedMessagesRead()));
  connect(m_ui->m_actionMarkSelectedMessagesAsUnread, &QAction::triggered,
          tabWidget()->feedMessageViewer()->messagesView(), &MessagesView::markSelectedMessagesUnread);
  connect(m_ui->m_actionOpenSelectedSourceArticlesExternally,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(openSelectedSourceMessagesExternally()));
  connect(m_ui->m_actionOpenSelectedMessagesInternally,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(openSelectedMessagesInternally()));
  connect(m_ui->m_actionSendMessageViaEmail,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(sendSelectedMessageViaEmail()));
  connect(m_ui->m_actionMarkAllItemsRead,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(markAllItemsRead()));
  connect(m_ui->m_actionMarkSelectedItemsAsRead,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(markSelectedItemRead()));
  connect(m_ui->m_actionExpandCollapseItem,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(expandCollapseCurrentItem()));
  connect(m_ui->m_actionMarkSelectedItemsAsUnread,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(markSelectedItemUnread()));
  connect(m_ui->m_actionClearSelectedItems,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(clearSelectedFeeds()));
  connect(m_ui->m_actionClearAllItems,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(clearAllFeeds()));
  connect(m_ui->m_actionUpdateSelectedItems,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(updateSelectedItems()));
  connect(m_ui->m_actionUpdateAllItems,
          SIGNAL(triggered()), qApp->feedReader(), SLOT(updateAllFeeds()));
  connect(m_ui->m_actionStopRunningItemsUpdate,
          SIGNAL(triggered()), qApp->feedReader(), SLOT(stopRunningFeedUpdate()));
  connect(m_ui->m_actionEditSelectedItem,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(editSelectedItem()));
  connect(m_ui->m_actionViewSelectedItemsNewspaperMode,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(openSelectedItemsInNewspaperMode()));
  connect(m_ui->m_actionDeleteSelectedItem,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(deleteSelectedItem()));
  connect(m_ui->m_actionSwitchFeedsList, &QAction::triggered,
          tabWidget()->feedMessageViewer(), &FeedMessageViewer::switchFeedComponentVisibility);
  connect(m_ui->m_actionSelectNextItem,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(selectNextItem()));
  connect(m_ui->m_actionSwitchToolBars, &QAction::toggled,
          tabWidget()->feedMessageViewer(), &FeedMessageViewer::setToolBarsEnabled);
  connect(m_ui->m_actionSwitchListHeaders, &QAction::toggled,
          tabWidget()->feedMessageViewer(), &FeedMessageViewer::setListHeadersEnabled);
  connect(m_ui->m_actionSelectPreviousItem,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->feedsView(), SLOT(selectPreviousItem()));
  connect(m_ui->m_actionSelectNextMessage,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(selectNextItem()));
  connect(m_ui->m_actionSelectNextUnreadMessage,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(selectNextUnreadItem()));
  connect(m_ui->m_actionSelectPreviousMessage,
          SIGNAL(triggered()), tabWidget()->feedMessageViewer()->messagesView(), SLOT(selectPreviousItem()));
  connect(m_ui->m_actionSwitchMessageListOrientation, &QAction::triggered,
          tabWidget()->feedMessageViewer(), &FeedMessageViewer::switchMessageSplitterOrientation);
  connect(m_ui->m_actionShowOnlyUnreadItems, &QAction::toggled,
          tabWidget()->feedMessageViewer(), &FeedMessageViewer::toggleShowOnlyUnreadFeeds);
  connect(m_ui->m_actionRestoreSelectedMessages, SIGNAL(triggered()),
          tabWidget()->feedMessageViewer()->messagesView(), SLOT(restoreSelectedMessages()));
  connect(m_ui->m_actionRestoreAllRecycleBins, SIGNAL(triggered()),
          tabWidget()->feedMessageViewer()->feedsView()->sourceModel(), SLOT(restoreAllBins()));
  connect(m_ui->m_actionEmptyAllRecycleBins, SIGNAL(triggered()),
          tabWidget()->feedMessageViewer()->feedsView()->sourceModel(), SLOT(emptyAllBins()));
}

void FormMain::backupDatabaseSettings() {
  QScopedPointer<FormBackupDatabaseSettings> form(new FormBackupDatabaseSettings(this));
  form->exec();
}

void FormMain::restoreDatabaseSettings() {
  QScopedPointer<FormRestoreDatabaseSettings> form(new FormRestoreDatabaseSettings(this));
  form->exec();
}

void FormMain::changeEvent(QEvent *event) {
  switch (event->type()) {
    case QEvent::WindowStateChange: {
      if (windowState() & Qt::WindowMinimized &&
          SystemTrayIcon::isSystemTrayActivated() &&
          qApp->settings()->value(GROUP(GUI), SETTING(GUI::HideMainWindowWhenMinimized)).toBool()) {
        event->ignore();
        QTimer::singleShot(CHANGE_EVENT_DELAY, this, SLOT(switchVisibility()));
      }

      break;
    }

    default:
      break;
  }

  QMainWindow::changeEvent(event);
}

void FormMain::showAbout() {
  QScopedPointer<FormAbout> form_pointer(new FormAbout(this));
  form_pointer->exec();
}

void FormMain::showUpdates() {
  QScopedPointer<FormUpdate> form_update(new FormUpdate(this));
  form_update->exec();
}

void FormMain::showWiki() {
  if (!WebFactory::instance()->openUrlInExternalBrowser(APP_URL_WIKI)) {
    qApp->showGuiMessage(tr("Cannot open external browser"),
                         tr("Cannot open external browser. Navigate to application website manually."),
                         QSystemTrayIcon::Warning, this, true);
  }
}

void FormMain::showAddAccountDialog() {
  QScopedPointer<FormAddAccount> form_update(new FormAddAccount(qApp->feedReader()->feedServices(),
                                                                qApp->feedReader()->feedsModel(),
                                                                this));
  form_update->exec();
}

void FormMain::reportABug() {
  if (!WebFactory::instance()->openUrlInExternalBrowser(APP_URL_ISSUES_NEW)) {
    qApp->showGuiMessage(tr("Cannot open external browser"),
                         tr("Cannot open external browser. Navigate to application website manually."),
                         QSystemTrayIcon::Warning, this, true);
  }
}

void FormMain::donate() {
  if (!WebFactory::instance()->openUrlInExternalBrowser(APP_DONATE_URL)) {
    qApp->showGuiMessage(tr("Cannot open external browser"),
                         tr("Cannot open external browser. Navigate to application website manually."),
                         QSystemTrayIcon::Warning, this, true);
  }
}

void FormMain::showSettings() {
  QScopedPointer<FormSettings> form_pointer(new FormSettings(this));
  form_pointer->exec();
}
