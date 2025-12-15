#ifndef ITEMDETAILVIEW_H
#define ITEMDETAILVIEW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include "../models/auctionitem.h"

class ItemDetailView : public QWidget
{
    Q_OBJECT

public:
    explicit ItemDetailView(QWidget *parent = nullptr);
    void setItem(AuctionItem *item);
    void clear();

private:
    QLabel *m_titleLabel;
    QLabel *m_bidLabel;
    QLabel *m_timeLabel;
    QTextEdit *m_descriptionEdit;
    AuctionItem *m_currentItem;
};

#endif // ITEMDETAILVIEW_H

