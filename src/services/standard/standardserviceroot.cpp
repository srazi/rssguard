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

#include "services/standard/standardserviceroot.h"

#include "definitions/definitions.h"
#include "miscellaneous/application.h"
#include "miscellaneous/settings.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/databasequeries.h"
#include "miscellaneous/mutex.h"
#include "core/feedsmodel.h"
#include "gui/messagebox.h"
#include "exceptions/applicationexception.h"
#include "services/abstract/recyclebin.h"
#include "services/standard/standardserviceentrypoint.h"
#include "services/standard/standardfeed.h"
#include "services/standard/standardcategory.h"
#include "services/standard/standardfeedsimportexportmodel.h"
#include "services/standard/gui/formstandardcategorydetails.h"
#include "services/standard/gui/formstandardfeeddetails.h"
#include "services/standard/gui/formstandardimportexport.h"

#include <QStack>
#include <QAction>
#include <QSqlTableModel>
#include <QClipboard>


StandardServiceRoot::StandardServiceRoot(RootItem *parent)
  : ServiceRoot(parent), m_recycleBin(new RecycleBin(this)),
    m_actionExportFeeds(nullptr), m_actionImportFeeds(nullptr), m_serviceMenu(QList<QAction*>()),
    m_feedContextMenu(QList<QAction*>()), m_actionFeedFetchMetadata(nullptr) {

  setTitle(qApp->system()->getUsername() + QL1S("@") + QL1S(APP_LOW_NAME));
  setIcon(StandardServiceEntryPoint().icon());
  setDescription(tr("This is obligatory service account for standard RSS/RDF/ATOM feeds."));
}

StandardServiceRoot::~StandardServiceRoot() {
  qDeleteAll(m_serviceMenu);
  qDeleteAll(m_feedContextMenu);
}

void StandardServiceRoot::start(bool freshly_activated) {
  loadFromDatabase();

  if (freshly_activated) {
    // In other words, if there are no feeds or categories added.
    if (MessageBox::show(qApp->mainFormWidget(), QMessageBox::Question, QObject::tr("Load initial set of feeds"),
                         tr("This new account does not include any feeds. You can now add default set of feeds."),
                         tr("Do you want to load initial set of feeds?"),
                         QString(), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      QString target_opml_file = APP_INITIAL_FEEDS_PATH + QDir::separator() + FEED_INITIAL_OPML_PATTERN;
      QString current_locale = qApp->localization()->loadedLanguage();
      QString file_to_load;

      if (QFile::exists(target_opml_file.arg(current_locale))) {
        file_to_load = target_opml_file.arg(current_locale);
      }
      else if (QFile::exists(target_opml_file.arg(DEFAULT_LOCALE))) {
        file_to_load = target_opml_file.arg(DEFAULT_LOCALE);
      }

      FeedsImportExportModel model;
      QString output_msg;

      try {
        model.importAsOPML20(IOFactory::readTextFile(file_to_load), false);
        model.checkAllItems();

        if (mergeImportExportModel(&model, this, output_msg)) {
          requestItemExpand(getSubTree(), true);
        }
      }
      catch (ApplicationException &ex) {
        MessageBox::show(qApp->mainFormWidget(), QMessageBox::Critical, tr("Error when loading initial feeds"), ex.message());
      }
    }
  }

  checkArgumentsForFeedAdding();
}

void StandardServiceRoot::stop() {
  qDebug("Stopping StandardServiceRoot instance.");
}

QString StandardServiceRoot::code() const {
  return StandardServiceEntryPoint().code();
}

bool StandardServiceRoot::canBeEdited() const {
  return false;
}

bool StandardServiceRoot::canBeDeleted() const {
  return true;
}

bool StandardServiceRoot::deleteViaGui() {
  return ServiceRoot::deleteViaGui();
}

bool StandardServiceRoot::supportsFeedAdding() const {
  return true;
}

bool StandardServiceRoot::supportsCategoryAdding() const {
  return true;
}

void StandardServiceRoot::addNewFeed(const QString &url) {
  if (!qApp->feedUpdateLock()->tryLock()) {
    // Lock was not obtained because
    // it is used probably by feed updater or application
    // is quitting.
    qApp->showGuiMessage(tr("Cannot add item"),
                         tr("Cannot add feed because another critical operation is ongoing."),
                         QSystemTrayIcon::Warning, qApp->mainFormWidget(), true);
    // Thus, cannot delete and quit the method.
    return;
  }

  QScopedPointer<FormStandardFeedDetails> form_pointer(new FormStandardFeedDetails(this, qApp->mainFormWidget()));
  form_pointer.data()->exec(nullptr, nullptr, url);

  qApp->feedUpdateLock()->unlock();
}

QVariant StandardServiceRoot::data(int column, int role) const {
  switch (role) {
    case Qt::ToolTipRole:
      if (column == FDS_MODEL_TITLE_INDEX) {
        return tr("This is service account for standard RSS/RDF/ATOM feeds.\n\nAccount ID: %1").arg(accountId());
      }
      else {
        return ServiceRoot::data(column, role);
      }

    default:
      return ServiceRoot::data(column, role);
  }
}

Qt::ItemFlags StandardServiceRoot::additionalFlags() const {
  return Qt::ItemIsDropEnabled;
}

RecycleBin *StandardServiceRoot::recycleBin() const {
  return m_recycleBin;
}

void StandardServiceRoot::loadFromDatabase(){
  QSqlDatabase database = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);
  Assignment categories = DatabaseQueries::getCategories(database, accountId());
  Assignment feeds = DatabaseQueries::getFeeds(database, accountId());

  // All data are now obtained, lets create the hierarchy.
  assembleCategories(categories);
  assembleFeeds(feeds);

  // As the last item, add recycle bin, which is needed.
  appendChild(m_recycleBin);
  updateCounts(true);
}

void StandardServiceRoot::checkArgumentsForFeedAdding() {
  foreach (QString arg, qApp->arguments().mid(1)) {
    checkArgumentForFeedAdding(arg);
  }
}

QMap<int,QVariant> StandardServiceRoot::storeCustomFeedsData() {
  return QMap<int,QVariant>();
}

void StandardServiceRoot::restoreCustomFeedsData(const QMap<int,QVariant> &data, const QHash<int,Feed*> &feeds) {
  Q_UNUSED(feeds)
  Q_UNUSED(data)
}

QString StandardServiceRoot::processFeedUrl(const QString &feed_url) {
  if (feed_url.startsWith(QL1S(URI_SCHEME_FEED_SHORT))) {
    QString without_feed_prefix = feed_url.mid(5);

    if (without_feed_prefix.startsWith(QL1S("https:")) || without_feed_prefix.startsWith(QL1S("http:"))) {
      return without_feed_prefix;
    }
    else {
      return feed_url;
    }
  }
  else {
    return feed_url;
  }
}

void StandardServiceRoot::checkArgumentForFeedAdding(const QString &argument) {
  if (argument.startsWith(QL1S(URI_SCHEME_FEED_SHORT))) {
    addNewFeed(processFeedUrl(argument));
  }
}

QList<QAction*> StandardServiceRoot::getContextMenuForFeed(StandardFeed *feed) {
  if (m_feedContextMenu.isEmpty()) {
    // Initialize.
    m_actionFeedFetchMetadata = new QAction(qApp->icons()->fromTheme(QSL("emblem-downloads")), tr("Fetch metadata"), nullptr);
    m_feedContextMenu.append(m_actionFeedFetchMetadata);
  }

  // Make connections.
  disconnect(m_actionFeedFetchMetadata, SIGNAL(triggered()), 0, 0);
  connect(m_actionFeedFetchMetadata, SIGNAL(triggered()), feed, SLOT(fetchMetadataForItself()));

  return m_feedContextMenu;
}

bool StandardServiceRoot::mergeImportExportModel(FeedsImportExportModel *model, RootItem *target_root_node, QString &output_message) {
  QStack<RootItem*> original_parents; original_parents.push(target_root_node);
  QStack<RootItem*> new_parents; new_parents.push(model->rootItem());
  bool some_feed_category_error = false;

  // Iterate all new items we would like to merge into current model.
  while (!new_parents.isEmpty()) {
    RootItem *target_parent = original_parents.pop();
    RootItem *source_parent = new_parents.pop();

    foreach (RootItem *source_item, source_parent->childItems()) {
      if (!model->isItemChecked(source_item)) {
        // We can skip this item, because it is not checked and should not be imported.
        // NOTE: All descendants are thus skipped too.
        continue;
      }

      if (source_item->kind() == RootItemKind::Category) {
        StandardCategory *source_category = static_cast<StandardCategory*>(source_item);
        StandardCategory *new_category = new StandardCategory(*source_category);
        QString new_category_title = new_category->title();

        // Add category to model.
        new_category->clearChildren();

        if (new_category->addItself(target_parent)) {
          requestItemReassignment(new_category, target_parent);

          // Process all children of this category.
          original_parents.push(new_category);
          new_parents.push(source_category);
        }
        else {
          delete new_category;

          // Add category failed, but this can mean that the same category (with same title)
          // already exists. If such a category exists in current parent, then find it and
          // add descendants to it.
          RootItem *existing_category = nullptr;
          foreach (RootItem *child, target_parent->childItems()) {
            if (child->kind() == RootItemKind::Category && child->title() == new_category_title) {
              existing_category = child;
            }
          }

          if (existing_category != nullptr) {
            original_parents.push(existing_category);
            new_parents.push(source_category);
          }
          else {
            some_feed_category_error = true;
          }
        }
      }
      else if (source_item->kind() == RootItemKind::Feed) {
        StandardFeed *source_feed = static_cast<StandardFeed*>(source_item);
        StandardFeed *new_feed = new StandardFeed(*source_feed);

        // Append this feed and end this iteration.
        if (new_feed->addItself(target_parent)) {
          requestItemReassignment(new_feed, target_parent);
        }
        else {
          delete new_feed;
          some_feed_category_error = true;
        }
      }
    }
  }

  if (some_feed_category_error) {
    output_message = tr("Import successful, but some feeds/categories were not imported due to error.");
  }
  else {
    output_message = tr("Import was completely successful.");
  }

  return !some_feed_category_error;
}

void StandardServiceRoot::addNewCategory() {
  if (!qApp->feedUpdateLock()->tryLock()) {
    // Lock was not obtained because
    // it is used probably by feed updater or application
    // is quitting.
    qApp->showGuiMessage(tr("Cannot add category"),
                         tr("Cannot add category because another critical operation is ongoing."),
                         QSystemTrayIcon::Warning, qApp->mainFormWidget(), true);
    // Thus, cannot delete and quit the method.
    return;
  }

  QScopedPointer<FormStandardCategoryDetails> form_pointer(new FormStandardCategoryDetails(this, qApp->mainFormWidget()));
  form_pointer.data()->exec(nullptr, nullptr);

  qApp->feedUpdateLock()->unlock();
}

void StandardServiceRoot::importFeeds() {
  QScopedPointer<FormStandardImportExport> form(new FormStandardImportExport(this, qApp->mainFormWidget()));
  form.data()->setMode(FeedsImportExportModel::Import);
  form.data()->exec();
}

void StandardServiceRoot::exportFeeds() {
  QScopedPointer<FormStandardImportExport> form(new FormStandardImportExport(this, qApp->mainFormWidget()));
  form.data()->setMode(FeedsImportExportModel::Export);
  form.data()->exec();
}

QList<QAction*> StandardServiceRoot::serviceMenu() {
  if (m_serviceMenu.isEmpty()) {
    m_actionExportFeeds = new QAction(qApp->icons()->fromTheme("document-export"), tr("Export feeds"), this);
    m_actionImportFeeds = new QAction(qApp->icons()->fromTheme("document-import"), tr("Import feeds"), this);

    connect(m_actionExportFeeds, SIGNAL(triggered()), this, SLOT(exportFeeds()));
    connect(m_actionImportFeeds, SIGNAL(triggered()), this, SLOT(importFeeds()));

    m_serviceMenu.append(m_actionExportFeeds);
    m_serviceMenu.append(m_actionImportFeeds);
  }

  return m_serviceMenu;
}
