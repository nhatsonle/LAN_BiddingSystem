#ifndef ITEMLISTMODEL_H
#define ITEMLISTMODEL_H

#include "auctionitem.h"
#include <QAbstractListModel>
#include <QList>

class ItemListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ItemRoles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        CurrentBidRole,
        EndTimeRole,
        ImageUrlRole
    };

    explicit ItemListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<AuctionItem*> &items);
    AuctionItem* getItem(int index) const;

public slots:
    void onItemUpdated(AuctionItem *item);

private:
    QList<AuctionItem*> m_items;
};

#endif // ITEMLISTMODEL_H

