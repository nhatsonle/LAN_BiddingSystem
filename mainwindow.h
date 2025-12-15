#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include "src/models/auctionrepository.h"
#include "src/models/itemlistmodel.h"
#include "src/views/itemlistview.h"
#include "src/views/itemdetailview.h"
#include "src/views/bidpanel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onItemSelected(AuctionItem *item);
    void onItemDoubleClicked(AuctionItem *item);
    void onBidPlaced(int itemId, const QString &bidderName, double amount);
    void onBidPlacedSuccess(Bid *bid);
    void onItemUpdated(AuctionItem *item);
    void showDetailView();
    void showBidPanel();

private:
    void setupUI();
    void setupConnections();
    void showStatusMessage(const QString &message, int timeout = 3000);

    Ui::MainWindow *ui;
    AuctionRepository *m_repository;
    ItemListModel *m_itemModel;
    ItemListView *m_itemListView;
    ItemDetailView *m_detailView;
    BidPanel *m_bidPanel;
    QStackedWidget *m_rightStack;
    QSplitter *m_splitter;
    AuctionItem *m_currentItem;
};

#endif // MAINWINDOW_H
