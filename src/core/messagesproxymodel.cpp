// This file is part of RSS Guard.
//
// Copyright (C) 2011-2015 by Martin Rotter <rotter.martinos@gmail.com>
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

#include "core/messagesproxymodel.h"

#include "core/messagesmodel.h"


MessagesProxyModel::MessagesProxyModel(QObject *parent)
  : QSortFilterProxyModel(parent) {
  m_sourceModel = new MessagesModel(this);

  setObjectName(QSL("MessagesProxyModel"));
  setSortRole(Qt::EditRole);
  setSortCaseSensitivity(Qt::CaseInsensitive);
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  setFilterKeyColumn(-1);
  setFilterRole(Qt::EditRole);
  setDynamicSortFilter(false);
  setSourceModel(m_sourceModel);
}

MessagesProxyModel::~MessagesProxyModel() {
  qDebug("Destroying MessagesProxyModel instance.");
}

bool MessagesProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
  if (left.column() == MSG_DB_TITLE_INDEX && right.column() == MSG_DB_TITLE_INDEX) {
    return QString::localeAwareCompare(m_sourceModel->data(left).toString(),
                                       m_sourceModel->data(right).toString()) < 0;
  }
  else {
    return QSortFilterProxyModel::lessThan(left, right);
  }
}

QModelIndexList MessagesProxyModel::mapListFromSource(const QModelIndexList &indexes, bool deep) {
  QModelIndexList mapped_indexes;

  foreach (const QModelIndex &index, indexes) {
    if (deep) {
      // Construct new source index.
      mapped_indexes << mapFromSource(m_sourceModel->index(index.row(), index.column()));
    }
    else {
      mapped_indexes << mapFromSource(index);
    }
  }

  return mapped_indexes;
}

QModelIndexList MessagesProxyModel::match(const QModelIndex &start, int role,
                                          const QVariant &entered_value, int hits, Qt::MatchFlags flags) const {
  QModelIndexList result;
  uint match_type = flags & 0x0F;
  Qt::CaseSensitivity case_sensitivity = Qt::CaseInsensitive;
  bool wrap = flags & Qt::MatchWrap;
  bool all_hits = (hits == -1);
  QString entered_text;
  int from = start.row();
  int to = rowCount();

  for (int i = 0; (wrap && i < 2) || (!wrap && i < 1); i++) {
    for (int r = from; (r < to) && (all_hits || result.count() < hits); r++) {
      QModelIndex idx = index(r, start.column());

      if (!idx.isValid()) {
        continue;
      }

      QVariant item_value = m_sourceModel->data(mapToSource(idx).row(), MSG_DB_TITLE_INDEX, role);

      // QVariant based matching.
      if (match_type == Qt::MatchExactly) {
        if (entered_value == item_value) {
          result.append(idx);
        }
      }
      // QString based matching.
      else {
        if (entered_text.isEmpty()) {
          entered_text = entered_value.toString();
        }

        QString item_text = item_value.toString();

        switch (match_type) {
          case Qt::MatchRegExp:
            if (QRegExp(entered_text, case_sensitivity).exactMatch(item_text)) {
              result.append(idx);
            }
            break;

          case Qt::MatchWildcard:
            if (QRegExp(entered_text, case_sensitivity, QRegExp::Wildcard).exactMatch(item_text)) {
              result.append(idx);
            }
            break;

          case Qt::MatchStartsWith:
            if (item_text.startsWith(entered_text, case_sensitivity)) {
              result.append(idx);
            }
            break;

          case Qt::MatchEndsWith:
            if (item_text.endsWith(entered_text, case_sensitivity)) {
              result.append(idx);
            }
            break;

          case Qt::MatchFixedString:
            if (item_text.compare(entered_text, case_sensitivity) == 0) {
              result.append(idx);
            }
            break;

          case Qt::MatchContains:
          default:
            if (item_text.contains(entered_text, case_sensitivity)) {
              result.append(idx);
            }
            break;
        }
      }
    }

    // Prepare for the next iteration.
    from = 0;
    to = start.row();
  }

  return result;
}

QModelIndexList MessagesProxyModel::mapListToSource(const QModelIndexList &indexes) {
  QModelIndexList source_indexes;

  foreach (const QModelIndex &index, indexes) {
    source_indexes << mapToSource(index);
  }

  return source_indexes;
}
