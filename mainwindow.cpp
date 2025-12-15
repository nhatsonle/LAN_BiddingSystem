#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QStatusBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_repository(nullptr)
    , m_itemModel(nullptr)
    , m_itemListView(nullptr)
    , m_detailView(nullptr)
    , m_bidPanel(nullptr)
    , m_rightStack(nullptr)
    , m_splitter(nullptr)
    , m_currentItem(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("Online Auction System - Buyer View");
    resize(1200, 700);

    m_repository = new AuctionRepository(this);
    m_itemModel = new ItemListModel(this);
    m_itemModel->setItems(m_repository->getAllItems());

    setupUI();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    QToolBar *toolbar = addToolBar("Main Toolbar");
    QAction *detailAction = toolbar->addAction("View Details");
    QAction *bidAction = toolbar->addAction("Place Bid");
    connect(detailAction, &QAction::triggered, this, &MainWindow::showDetailView);
    connect(bidAction, &QAction::triggered, this, &MainWindow::showBidPanel);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_splitter);

    m_itemListView = new ItemListView(this);
    m_itemListView->setModel(m_itemModel);
    m_splitter->addWidget(m_itemListView);

    m_rightStack = new QStackedWidget(this);
    m_detailView = new ItemDetailView(this);
    m_bidPanel = new BidPanel(this);
    m_rightStack->addWidget(m_detailView);
    m_rightStack->addWidget(m_bidPanel);
    m_splitter->addWidget(m_rightStack);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 2);

    // Apply basic styling
    setStyleSheet(
        "QMainWindow { background-color: #f5f5f5; color: #212121; }"
        "QWidget { color: #212121; }"
        "QLabel { color: #212121; }"
        "QListView { background-color: white; border: 1px solid #ddd; border-radius: 4px; color: #212121; }"
        "QListView::item { padding: 5px; border-bottom: 1px solid #eee; }"
        "QListView::item:selected { background-color: #e3f2fd; color: #0d47a1; }"
        "QListView::item:hover { background-color: #f5f5f5; }"
        "QLineEdit { padding: 5px; border: 1px solid #ddd; border-radius: 4px; color: #212121; background: #fff; }"
        "QLineEdit::placeholder { color: #888; }"
        "QPushButton { padding: 8px 16px; background-color: #2196F3; color: white; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:disabled { background-color: #ccc; color: #666; }"
        "QDoubleSpinBox { padding: 5px; border: 1px solid #ddd; border-radius: 4px; color: #212121; background: #fff; }"
        "QTextEdit { border: 1px solid #ddd; border-radius: 4px; background-color: white; color: #212121; }"
    );

    statusBar()->showMessage("Ready - Select an item to view details or place a bid", 0);
}

void MainWindow::setupConnections()
{
    connect(m_itemListView, &ItemListView::itemSelected, this, &MainWindow::onItemSelected);
    connect(m_itemListView, &ItemListView::itemDoubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(m_bidPanel, &BidPanel::bidPlaced, this, &MainWindow::onBidPlaced);
    connect(m_repository, &AuctionRepository::bidPlaced, this, &MainWindow::onBidPlacedSuccess);
    connect(m_repository, &AuctionRepository::itemUpdated, this, &MainWindow::onItemUpdated);
}

void MainWindow::onItemSelected(AuctionItem *item)
{
    m_currentItem = item;
    if (item) {
        m_detailView->setItem(item);
        m_bidPanel->setItem(item);
        if (m_rightStack->currentIndex() == 0) {
            m_detailView->setItem(item);
        }
    }
}

void MainWindow::onItemDoubleClicked(AuctionItem *item)
{
    m_currentItem = item;
    showDetailView();
    m_detailView->setItem(item);
    m_bidPanel->setItem(item);
}

void MainWindow::onBidPlaced(int itemId, const QString &bidderName, double amount)
{
    bool success = m_repository->placeBid(itemId, bidderName, amount);
    if (!success) {
        QMessageBox::warning(this, "Bid Failed", 
                           "Failed to place bid. Make sure your bid is higher than the current bid.");
    }
}

void MainWindow::onBidPlacedSuccess(Bid *bid)
{
    Q_UNUSED(bid)
    showStatusMessage(QString("Bid placed successfully! New current bid: $%1")
                     .arg(m_currentItem ? m_currentItem->currentBid() : 0.0, 0, 'f', 2), 5000);
}

void MainWindow::onItemUpdated(AuctionItem *item)
{
    m_itemModel->onItemUpdated(item);
    if (m_currentItem && m_currentItem->id() == item->id()) {
        m_detailView->setItem(item);
        m_bidPanel->setItem(item);
    }
}

void MainWindow::showDetailView()
{
    m_rightStack->setCurrentIndex(0);
    if (m_currentItem) {
        m_detailView->setItem(m_currentItem);
    }
}

void MainWindow::showBidPanel()
{
    m_rightStack->setCurrentIndex(1);
    if (m_currentItem) {
        m_bidPanel->setItem(m_currentItem);
    }
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}
