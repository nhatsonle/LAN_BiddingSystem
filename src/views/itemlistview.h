#ifndef ITEMLISTVIEW_H
#define ITEMLISTVIEW_H

#include <QWidget>
#include <QListView>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QItemSelection>
#include "../models/itemlistmodel.h"

class ItemListView : public QWidget
{
    Q_OBJECT

public:
    explicit ItemListView(QWidget *parent = nullptr);
    void setModel(ItemListModel *model);

signals:
    void itemSelected(AuctionItem *item);
    void itemDoubleClicked(AuctionItem *item);

private slots:
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onDoubleClicked(const QModelIndex &index);
    void onSearchTextChanged(const QString &text);

private:
    QListView *m_listView;
    QLineEdit *m_searchEdit;
    ItemListModel *m_model;
    QSortFilterProxyModel *m_filteredModel;
    QLabel *m_titleLabel;
};

#endif // ITEMLISTVIEW_H

