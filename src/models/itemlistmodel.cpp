#include "itemlistmodel.h"
#include <QDebug>

ItemListModel::ItemListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ItemListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_items.size();
}

QVariant ItemListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) {
        return QVariant();
    }

    AuctionItem *item = m_items[index.row()];
    if (!item) {
        return QVariant();
    }

    switch (role) {
    case IdRole:
        return item->id();
    case TitleRole:
        return item->title();
    case DescriptionRole:
        return item->description();
    case CurrentBidRole:
        return item->currentBid();
    case EndTimeRole:
        return item->endTime();
    case ImageUrlRole:
        return item->imageUrl();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ItemListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "itemId";
    roles[TitleRole] = "title";
    roles[DescriptionRole] = "description";
    roles[CurrentBidRole] = "currentBid";
    roles[EndTimeRole] = "endTime";
    roles[ImageUrlRole] = "imageUrl";
    return roles;
}

void ItemListModel::setItems(const QList<AuctionItem*> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

AuctionItem* ItemListModel::getItem(int index) const
{
    if (index >= 0 && index < m_items.size()) {
        return m_items[index];
    }
    return nullptr;
}

void ItemListModel::onItemUpdated(AuctionItem *item)
{
    int index = m_items.indexOf(item);
    if (index >= 0) {
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {CurrentBidRole});
    }
}

