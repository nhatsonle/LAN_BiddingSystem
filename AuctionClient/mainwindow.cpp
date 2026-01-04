#include "mainwindow.h"
#include "createroomdialog.h"
#include "profiledialog.h" // <-- Added here
#include "registerdialog.h"
#include "ui_mainwindow.h"
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QTableWidgetItem>
#include <QLabel>
#include <QFont>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  // --- Product list table (Room) ---
  ui->tblRoomProducts->setColumnCount(2);
  ui->tblRoomProducts->setHorizontalHeaderLabels(
      QStringList() << "Sản phẩm"
                    << "Trạng thái");
  ui->tblRoomProducts->horizontalHeader()->setStretchLastSection(true);
  ui->tblRoomProducts->verticalHeader()->setVisible(false);
  ui->tblRoomProducts->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui->tblRoomProducts->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tblRoomProducts->setSelectionMode(QAbstractItemView::SingleSelection);

  m_btnMyRoomsButton = new QPushButton("My Rooms", ui->pageLobby);
  m_btnMyRoomsButton->setGeometry(260, 480, 101, 31);
  m_btnMyRoomsButton->setFixedSize(101, 31);
  connect(m_btnMyRoomsButton, &QPushButton::clicked, this,
          &MainWindow::showMyRoomsView);

  m_pageMyRooms = new QWidget(this);
  QVBoxLayout *roomLayout = new QVBoxLayout(m_pageMyRooms);
  roomLayout->setContentsMargins(20, 20, 20, 20);
  roomLayout->setSpacing(12);
  QLabel *roomsTitle = new QLabel("My Rooms", m_pageMyRooms);
  QFont titleFont = roomsTitle->font();
  titleFont.setPointSize(14);
  titleFont.setBold(true);
  roomsTitle->setFont(titleFont);
  roomsTitle->setAlignment(Qt::AlignCenter);
  roomLayout->addWidget(roomsTitle);

  m_tblMyRooms = new QTableWidget(0, 4, m_pageMyRooms);
  QStringList roomHeaders;
  roomHeaders << "ID"
              << "Name"
              << "Start Time"
              << "Status";
  m_tblMyRooms->setHorizontalHeaderLabels(roomHeaders);
  m_tblMyRooms->horizontalHeader()->setStretchLastSection(true);
  m_tblMyRooms->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tblMyRooms->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tblMyRooms->setEditTriggers(QAbstractItemView::NoEditTriggers);
  roomLayout->addWidget(m_tblMyRooms);

  QHBoxLayout *roomButtons = new QHBoxLayout;
  m_btnReloadMyRooms = new QPushButton("Reload", m_pageMyRooms);
  m_btnEditMyRoom = new QPushButton("Edit", m_pageMyRooms);
  m_btnStopMyRoom = new QPushButton("Stop", m_pageMyRooms);
  m_btnBackToLobby = new QPushButton("Back", m_pageMyRooms);
  for (auto *btn : {m_btnReloadMyRooms, m_btnEditMyRoom, m_btnStopMyRoom,
                    m_btnBackToLobby}) {
    btn->setFixedSize(101, 31);
  }
  roomButtons->addWidget(m_btnReloadMyRooms);
  roomButtons->addWidget(m_btnEditMyRoom);
  roomButtons->addWidget(m_btnStopMyRoom);
  roomButtons->addStretch();
  roomButtons->addWidget(m_btnBackToLobby);
  roomLayout->addLayout(roomButtons);
  roomLayout->addStretch();

  m_btnEditMyRoom->setEnabled(false);
  m_btnStopMyRoom->setEnabled(false);

  ui->stackedWidget->insertWidget(2, m_pageMyRooms);

  m_myRoomsGateTimer = new QTimer(this);
  m_myRoomsGateTimer->setInterval(1000);
  connect(m_myRoomsGateTimer, &QTimer::timeout, this,
          &MainWindow::myRoomsSelectionChanged);

  connect(m_tblMyRooms, &QTableWidget::itemSelectionChanged, this,
          &MainWindow::myRoomsSelectionChanged);
  connect(m_btnReloadMyRooms, &QPushButton::clicked, this,
          &MainWindow::reloadMyRooms);
  connect(m_btnEditMyRoom, &QPushButton::clicked, this,
          &MainWindow::editSelectedRoom);
  connect(m_btnStopMyRoom, &QPushButton::clicked, this,
          &MainWindow::stopSelectedRoom);
  connect(m_btnBackToLobby, &QPushButton::clicked, this,
          &MainWindow::backToLobbyFromMyRooms);

  m_myRoomsServerEpoch = 0;

  // Khởi tạo Socket
  m_socket = new QTcpSocket(this);
  connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
  m_roomStartTimer = new QTimer(this);
  m_roomStartTimer->setInterval(1000);
  connect(m_roomStartTimer, &QTimer::timeout, this,
          &MainWindow::onRoomStartTimeout);
  resetRoomStartState();

  // Mặc định hiển thị trang Login (Index 0)
  ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow() { delete ui; }

// --- XỬ LÝ NÚT BẤM ---

void MainWindow::on_btnBid_clicked() {
  if (m_isCurrentRoomHost) {
    QMessageBox::warning(
        this, "Không hợp lệ",
        "Chủ phòng không được phép đấu giá trong phòng của mình.");
    return;
  }
  if (!m_roomStartReached) {
    ui->txtRoomLog->append(
        "<span style=\"color:#e67e22;\">Phòng đấu giá chưa bắt đầu.</span>");
    return;
  }
  // 1. Lấy giá hiện tại từ giao diện (bóc tách từ chuỗi hiển thị hoặc lưu biến
  // riêng)

  int currentPrice = m_currentPriceValue;
  int bidAmount = ui->txtBidAmount->text().toInt();

  // Chặn đặt giá cao hơn giá mua ngay -> hỏi chuyển sang mua ngay
  if (m_buyNowPriceValue > 0 && bidAmount >= m_buyNowPriceValue) {
    auto res =
        QMessageBox::question(this, "Xác nhận mua ngay",
                              "Giá bạn nhập đã chạm/qua giá MUA NGAY.\n"
                              "Bạn có muốn mua ngay với giá " +
                                  formatPrice(m_buyNowPriceValue) + " ?");
    if (res == QMessageBox::Yes) {
      QString cmd = QString("BUY_NOW|%1\n").arg(m_currentRoomId);
      m_socket->write(cmd.toUtf8());
    }
    return;
  }

  // 2. Kiểm tra quy tắc +10.000
  if (bidAmount < currentPrice + 10000) {
    QMessageBox::warning(
        this, "Không hợp lệ",
        "Giá đấu phải cao hơn hoặc bằng giá hiện tại + 10.000 VND!");
    return; // Dừng lại ngay, KHÔNG gửi gì lên Server cả
  }

  // 3. Nếu OK thì mới gửi
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    QString msg = QString("BID|%1|%2\n").arg(m_currentRoomId).arg(bidAmount);
    m_socket->write(msg.toUtf8());
  }
}

void MainWindow::on_btnQuickBid_clicked() {
  if (m_isCurrentRoomHost) {
    QMessageBox::warning(
        this, "Không hợp lệ",
        "Chủ phòng không được phép đấu giá trong phòng của mình.");
    return;
  }
  if (!m_roomStartReached) {
    ui->txtRoomLog->append(
        "<span style=\"color:#e67e22;\">Phòng đấu giá chưa bắt đầu.</span>");
    return;
  }
  int currentPrice = m_currentPriceValue;
  int bidAmount = currentPrice + 10000;
  ui->txtBidAmount->setText(QString::number(bidAmount));

  // Chặn đặt giá cao hơn giá mua ngay -> hỏi chuyển sang mua ngay
  if (m_buyNowPriceValue > 0 && bidAmount >= m_buyNowPriceValue) {
    auto res =
        QMessageBox::question(this, "Xác nhận mua ngay",
                              "Giá Đặt nhanh sẽ chạm/qua giá MUA NGAY.\n"
                              "Bạn có muốn mua ngay với giá " +
                                  formatPrice(m_buyNowPriceValue) + " ?");
    if (res == QMessageBox::Yes) {
      QString cmd = QString("BUY_NOW|%1\n").arg(m_currentRoomId);
      m_socket->write(cmd.toUtf8());
    }
    return;
  }

  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    QString msg = QString("BID|%1|%2\n").arg(m_currentRoomId).arg(bidAmount);
    m_socket->write(msg.toUtf8());
  }
}

void MainWindow::on_btnLogin_clicked() {
  // 1. Kết nối tới Server (nếu chưa)
  if (m_socket->state() != QAbstractSocket::ConnectedState) {
    m_socket->connectToHost("127.0.0.1",
                            8080); // Hardcode IP cho nhanh, hoặc lấy từ ô nhập
    if (!m_socket->waitForConnected(3000)) {
      QMessageBox::critical(this, "Lỗi", "Không thể kết nối Server!");
      return;
    }
  }

  // 2. Gửi lệnh Login
  QString user = ui->txtUser->text();
  QString pass = ui->txtPass->text();

  if (user.isEmpty())
    return;

  m_username = user; // Lưu lại
  QString command = "LOGIN|" + user + "|" + pass + "\n";
  m_socket->write(command.toUtf8());
}

void MainWindow::on_btnRefresh_clicked() {
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    qDebug() << "Client: Sending LIST_ROOMS..."; // <--- THÊM DÒNG NÀY

    // KIỂM TRA KỸ: Có ký tự xuống dòng \n ở cuối không?
    m_socket->write("LIST_ROOMS\n");

    // Nếu thiếu \n, Server (dùng getline hoặc buffer) có thể đợi mãi ký tự kết
    // thúc dòng
  } else {
    qDebug() << "Client: Socket is NOT connected. State:" << m_socket->state();
    QMessageBox::warning(this, "Lỗi", "Mất kết nối với Server!");
  }
}
void MainWindow::on_btnLeave_clicked() {
  // 1. Gửi thông báo cho Server biết mình rút lui
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    QString cmd = QString("LEAVE_ROOM|%1\n").arg(m_currentRoomId);
    m_socket->write(cmd.toUtf8());
  }

  // 2. Xử lý giao diện (UI)
  ui->stackedWidget->setCurrentIndex(1); // Quay về trang Lobby

  resetRoomStartState();

  // Xóa log chat cũ để lần sau vào phòng khác không bị lẫn lộn
  ui->txtRoomLog->clear();
  ui->txtChatLog->clear();
  ui->tblRoomProducts->setRowCount(0);

  // (Tùy chọn) Reset ID phòng hiện tại
  m_currentRoomId = -1;
  m_currentPriceValue = 0;
  m_buyNowPriceValue = 0;
  m_isCurrentRoomHost = false;
  m_activeProductId = -1;
  m_activeProductDescription.clear();
  updateRoomActionPermissions();

  // (Tùy chọn) Refresh lại danh sách phòng ngoài sảnh để cập nhật giá mới nhất
  on_btnRefresh_clicked();
}

void MainWindow::on_btnLogout_clicked() {
  // 1. Gửi lệnh LOGOUT cho Server
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    m_socket->write("LOGOUT\n");
    m_socket->flush();
    m_socket->disconnectFromHost(); // Ngắt kết nối luôn
  }

  // 2. Xóa dữ liệu phiên
  m_username = "";
  m_currentRoomId = -1;
  m_currentPriceValue = 0;
  m_buyNowPriceValue = 0;
  m_isCurrentRoomHost = false;
  resetRoomStartState();
  m_activeProductId = -1;
  m_activeProductDescription.clear();
  m_ownedRoomIds.clear();

  // 3. Reset giao diện
  ui->txtUser->clear();
  ui->txtPass->clear();
  ui->listRooms->clear();
  ui->txtChatLog->clear();
  ui->txtRoomLog->clear();
  ui->tblRoomProducts->setRowCount(0);

  // 4. Chuyển về màn hình Login
  ui->stackedWidget->setCurrentIndex(0);

  updateRoomActionPermissions();
  QMessageBox::information(this, "Đăng xuất", "Bạn đã đăng xuất thành công.");
}

void MainWindow::on_btnCreateRoom_clicked() {
  CreateRoomDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    QString roomName = dlg.getRoomName();
    auto products = dlg.getProductList();
    QString startTime = dlg.getStartTimeString();

    // Xây dựng chuỗi danh sách sản phẩm
    // Ví dụ: "iPhone,1000,2000,60,Mo ta;TaiNghe,500,1000,30,Mo ta"
    QStringList productStrings;
    for (const auto &p : products) {
      QString itemStr = QString("%1,%2,%3,%4,%5")
                            .arg(p.name)
                            .arg(p.startPrice)
                            .arg(p.buyNowPrice)
                            .arg(p.duration)
                            .arg(p.description);
      productStrings << itemStr;
    }

    // Gửi lệnh: CREATE_ROOM|RoomName|ProductListString|StartTime
    QString msg = QString("CREATE_ROOM|%1|%2|%3\n")
                      .arg(roomName)
                      .arg(productStrings.join(";"))
                      .arg(startTime);

    m_socket->write(msg.toUtf8());
  }
}

void MainWindow::on_btnJoin_clicked() {
  // 1. Lấy dòng đang chọn
  QListWidgetItem *item = ui->listRooms->currentItem();
  if (!item) {
    QMessageBox::warning(this, "Lỗi", "Vui lòng chọn một phòng!");
    return;
  }

  // 2. Lấy ID từ dữ liệu ẩn
  int roomId = item->data(Qt::UserRole + 2).toInt();

  // Debug ngay lập tức
  qDebug() << "[DEBUG] Dang chon phong ID:" << roomId;

  // 3. Nếu vẫn bằng 0 -> Có thể do chưa cập nhật danh sách
  if (roomId == 0) {
    QMessageBox::critical(this, "Lỗi logic",
                          "Không lấy được ID phòng (ID=0).\nHãy ấn nút 'Làm "
                          "mới' để tải lại danh sách.");
    return;
  }

  // 4. Gửi lệnh Join
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    m_currentRoomId = roomId;
    resetRoomStartState();

    // Reset UI room trước khi join để tránh xoá mất broadcast (CHAT/COUNT) đến sớm
    ui->txtRoomLog->clear();
    ui->txtRoomLog->append("--- Bắt đầu phiên đấu giá ---");
    ui->txtChatLog->clear();
    ui->txtChatLog->append("=== Chat phòng ===");

    // Gửi lệnh JOIN_ROOM|ID
    QString msg = "JOIN_ROOM|" + QString::number(roomId) + "\n";
    m_socket->write(msg.toUtf8());

    // Log lại lệnh đã gửi
    qDebug() << "[DEBUG] Sending:" << msg;
  } else {
    QMessageBox::warning(this, "Lỗi", "Mất kết nối Server!");
  }
}

void MainWindow::on_btnBuyNow_clicked() {
  if (m_isCurrentRoomHost) {
    QMessageBox::warning(
        this, "Không hợp lệ",
        "Chủ phòng không được phép mua sản phẩm trong phòng của mình.");
    return;
  }
  if (!m_roomStartReached) {
    ui->txtRoomLog->append(
        "<span style=\"color:#e67e22;\">Phòng đấu giá chưa bắt đầu.</span>");
    return;
  }
  // Hiển thị hộp thoại xác nhận cho chắc ăn
  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(this, "Xác nhận",
                                "Bạn có chắc muốn mua ngay sản phẩm này?",
                                QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    // Gửi lệnh
    m_socket->write(QString("BUY_NOW|%1\n").arg(m_currentRoomId).toUtf8());
  }
}

void MainWindow::on_btnSendChat_clicked() {
  if (m_socket->state() != QAbstractSocket::ConnectedState)
    return;
  if (m_currentRoomId < 0)
    return;

  QString chatText = ui->txtChatInput->text().trimmed();
  if (chatText.isEmpty())
    return;

  // Loại bỏ newline và ký tự '|' để tránh cắt gói/protocol
  chatText.replace("\n", " ");
  chatText.replace("|", " ");
  QString cmd = QString("CHAT|%1|%2\n").arg(m_currentRoomId).arg(chatText);
  m_socket->write(cmd.toUtf8());
  ui->txtChatInput->clear();
}

void MainWindow::onRoomStartTimeout() {
  updateRoomStartState();
}

void MainWindow::on_btnShowDescription_clicked() {
  if (m_currentRoomId < 0) {
    return;
  }

  if (m_activeProductId < 0) {
    QMessageBox::information(this, "Mô tả sản phẩm",
                             "Chưa có sản phẩm đang đấu giá.");
    return;
  }

  QString desc = m_activeProductDescription.trimmed();
  if (desc.isEmpty()) {
    desc = "Chưa có mô tả.";
  }

  QString title = ui->lblRoomName->text().trimmed();
  if (title.isEmpty()) {
    title = "Mô tả sản phẩm";
  }
  QMessageBox::information(this, title, desc);
}

void MainWindow::on_tblRoomProducts_cellClicked(int row, int column) {
  (void)column;
  QTableWidgetItem *nameItem = ui->tblRoomProducts->item(row, 0);
  if (!nameItem) {
    return;
  }

  QString name = nameItem->text().trimmed();
  QString desc = nameItem->data(Qt::UserRole + 2).toString().trimmed();
  if (desc.isEmpty()) {
    desc = "Chưa có mô tả.";
  }

  QMessageBox::information(this, "Mô tả - " + name, desc);
}

void MainWindow::on_btnOpenRegister_clicked() {
  RegisterDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    QString u = dlg.getUsername();
    QString p = dlg.getPassword();

    // 1. KIỂM TRA VÀ TỰ ĐỘNG KẾT NỐI NẾU CẦN
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
      // Thay "127.0.0.1" và 8080 bằng IP/Port server của bạn nếu khác
      m_socket->connectToHost("127.0.0.1", 8080);

      // Chờ tối đa 3 giây để kết nối
      if (!m_socket->waitForConnected(3000)) {
        QMessageBox::critical(
            this, "Lỗi kết nối",
            "Không thể kết nối đến Server!\nVui lòng kiểm tra lại Server.");
        return;
      }
    }

    // 2. Gửi lệnh khi đã chắc chắn có kết nối
    QString msg = "REGISTER|" + u + "|" + p + "\n";
    m_socket->write(msg.toUtf8());
  }
}

// --- XỬ LÝ MẠNG (CORE) ---

void MainWindow::onConnected() {
  // Có thể hiện thông báo nhỏ ở thanh status bar
  statusBar()->showMessage("Đã kết nối Server");
}

void MainWindow::onReadyRead() {
  auto statusToText = [](const QString &status) -> QString {
    if (status == "ACTIVE")
      return "Đang đấu giá";
    if (status == "WAITING")
      return "Chờ đấu giá";
    if (status == "SOLD")
      return "Đã bán";
    if (status == "NO_SALE")
      return "No sale";
    return status;
  };

  auto statusToColor = [](const QString &status) -> QColor {
    if (status == "ACTIVE")
      return QColor("#1E90FF");
    if (status == "SOLD" || status == "NO_SALE")
      return QColor("#7f8c8d");
    return QColor(Qt::black);
  };

  auto refreshProductTableStyles = [&]() {
    for (int row = 0; row < ui->tblRoomProducts->rowCount(); ++row) {
      QTableWidgetItem *nameItem = ui->tblRoomProducts->item(row, 0);
      QTableWidgetItem *statusItem = ui->tblRoomProducts->item(row, 1);
      if (!nameItem || !statusItem)
        continue;

      QString status = nameItem->data(Qt::UserRole + 1).toString();
      QColor color = statusToColor(status);

      nameItem->setForeground(color);
      statusItem->setForeground(color);

      QFont f0 = nameItem->font();
      f0.setBold(status == "ACTIVE");
      nameItem->setFont(f0);

      QFont f1 = statusItem->font();
      f1.setBold(status == "ACTIVE");
      statusItem->setFont(f1);
    }
  };

  while (m_socket->canReadLine()) {
    QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
    qDebug() << "Server msg:"
             << line; // <-- Nhìn vào đây xem có thấy NEW_BID không?
    // --- PHÂN TÍCH GÓI TIN TỪ SERVER ---

    // 1. Phản hồi Đăng nhập
    if (line.startsWith("OK|LOGIN_SUCCESS")) {
      // Chuyển sang trang Lobby (Index 1)
      ui->stackedWidget->setCurrentIndex(1);

      // Hiển thị tên người dùng sau khi đăng nhập
      ui->label->setText("Xin chào, " + m_username);

      // Tự động lấy danh sách phòng luôn
      on_btnRefresh_clicked();
    } else if (line.startsWith("ERR|WRONG_PASS")) {
      QMessageBox::warning(this, "Lỗi", "Sai mật khẩu!");
    }

    // 2. Phản hồi Danh sách phòng
    // Server gửi: OK|LIST|1:Laptop:1500;2:Phone:500;
    else if (line.startsWith("OK|LIST")) {
      ui->listRooms
          ->clear(); // Đảm bảo tên widget đúng là lstRooms hoặc listRooms

      // 1. Lấy phần dữ liệu thực tế (Bỏ qua "OK|LIST|")
      // section('|', 2) sẽ lấy tất cả nội dung từ sau dấu gạch đứng thứ 2 trở
      // đi
      QString data = line.section('|', 2);

      // Debug xem dữ liệu thô sau khi cắt header là gì
      qDebug() << "[DEBUG] List Data Raw:" << data;

      QStringList rooms = data.split(';', Qt::SkipEmptyParts);

      for (const QString &roomStr : rooms) {
        // Format từng dòng: ID:Name:Price:BuyNow
        QStringList parts = roomStr.split(':');

        // Phải có ít nhất 2 phần tử (ID và Name)
        if (parts.size() >= 2) {
          bool ok;
          int rId = parts[0].toInt(&ok); // Convert ID

          if (!ok) {
            qDebug() << "[ERROR] ID không phải số:" << parts[0];
            continue;
          }

          QString name = parts[1];
          QString price = (parts.size() >= 3) ? parts[2] : "0";

          QString displayText =
              QString("Phòng %1: %2 - Giá: %3").arg(rId).arg(name).arg(price);
          QListWidgetItem *item = new QListWidgetItem(displayText);

          // --- QUAN TRỌNG: LƯU ID VÀO DỮ LIỆU ẨN ---
          // Đây là chìa khóa để nút JOIN hoạt động đúng
          item->setData(Qt::UserRole + 1, price.toInt()); // Sort giá
          item->setData(Qt::UserRole + 2, rId);           // Lấy ID khi Join
          // ------------------------------------------

          ui->listRooms->addItem(item);

          qDebug() << "[SUCCESS] Đã thêm phòng ID:" << rId;
        }
      }
    } else if (line.startsWith("OK|ROOM_CREATED")) {
      // Server replies: OK|ROOM_CREATED|5 (5 is the new room's ID)
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        int newRoomId = parts[2].toInt();
        m_ownedRoomIds.insert(newRoomId);
      }
      QMessageBox::information(this, "Thành công", "Đã tạo phòng đấu giá mới");
      // Tự động refresh lại list để thấy phòng mình vừa tạo
      on_btnRefresh_clicked();
    }
    else if (line.startsWith("OK|MY_ROOMS")) {
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        bool okEpoch = false;
        qint64 epoch = parts[2].toLongLong(&okEpoch);
        if (okEpoch) {
          m_myRoomsServerEpoch = epoch;
          m_myRoomsElapsed.restart();
        } else {
          m_myRoomsServerEpoch = 0;
          m_myRoomsElapsed.invalidate();
        }
      }
      QString payload = line.section('|', 3);
      populateMyRoomsTable(payload);
    }
    else if (line.startsWith("OK|ROOM_EDIT_DATA")) {
      // OK|ROOM_EDIT_DATA|server_epoch|roomId|roomName|startTime|productsPayload
      QStringList parts = line.split('|');
      if (parts.size() < 7)
        continue;

      bool okEpoch = false;
      qint64 epoch = parts[2].toLongLong(&okEpoch);
      if (okEpoch) {
        m_myRoomsServerEpoch = epoch;
        m_myRoomsElapsed.restart();
      }

      int roomId = parts[3].toInt();
      QString roomName = parts[4];
      QString startTime = parts[5];
      QString productsPayload = line.section('|', 6);

      std::vector<ProductInfo> products;
      QStringList entries = productsPayload.split(';', Qt::SkipEmptyParts);
      products.reserve(entries.size());
      for (const QString &entry : entries) {
        QStringList fields = entry.split(',');
        if (fields.size() < 5)
          continue;
        ProductInfo p;
        p.name = fields[0];
        p.startPrice = fields[1].toInt();
        p.buyNowPrice = fields[2].toInt();
        p.duration = fields[3].toInt();
        p.description = fields[4];
        products.push_back(p);
      }

      CreateRoomDialog dlg(this);
      dlg.setRoomName(roomName);
      dlg.setStartTimeString(startTime);
      dlg.setProductList(products);
      if (dlg.exec() != QDialog::Accepted)
        continue;

      std::vector<ProductInfo> newProducts = dlg.getProductList();
      if (newProducts.empty()) {
        QMessageBox::warning(this, "Missing products",
                             "Please add at least one product.");
        continue;
      }

      QString payload = buildProductPayload(newProducts);
      QString cmd = QString("EDIT_ROOM|%1|%2|%3|%4\n")
                        .arg(roomId)
                        .arg(dlg.getRoomName())
                        .arg(payload)
                        .arg(dlg.getStartTimeString());
      sendRequest(cmd);
    }
    else if (line.startsWith("OK|ROOM_UPDATED")) {
      QMessageBox::information(this, "Room Updated",
                               "Room details were updated successfully.");
      requestMyRooms();
    }
    else if (line.startsWith("OK|ROOM_STOPPED")) {
      QMessageBox::information(this, "Room Stopped",
                               "Room has been stopped by the host.");
      requestMyRooms();
    }
    // 1. XỬ LÝ VÀO PHÒNG THÀNH CÔNG
    // Server: OK|JOINED|<id>|<name>|<price>
	    else if (line.startsWith("OK|JOINED")) {
	      QStringList parts = line.split('|');

      // Server gửi:
      // OK|JOINED|ID|Name|CurrentPrice|BuyNowPrice|Host|Leader|BidCount|ParticipantCount
      if (parts.size() >= 10) {
        m_currentRoomId = parts[2].toInt();
        QString name = parts[3];
        int price = parts[4].toInt();
        int buyNow = parts[5].toInt(); // <-- Lấy giá mua ngay
        QString host = parts[6];
        QString leader = parts[7];
        int bidCount = parts[8].toInt();
        QString participants = parts[9];

        // Cập nhật UI
        ui->lblRoomName->setText("Đang đấu giá: " + name);
        m_currentPriceValue = price;
        m_buyNowPriceValue = buyNow;
        ui->lblCurrentPrice->setText(formatPrice(price));
	
	        // --- HIỂN THỊ GIÁ MUA NGAY ---
	        ui->lblBuyNowPrice->setText(formatPrice(buyNow));
	        // -----------------------------

        updateRoomInfoUI(host, leader, bidCount, participants);

        QString startTime = (parts.size() >= 11) ? parts[10] : QString();
        bool startedFlag = parts.size() >= 12 ? parts[11].toInt() != 0 : true;
        if (parts.size() >= 13) {
          bool okEpoch = false;
          qint64 epoch = parts[12].toLongLong(&okEpoch);
          if (okEpoch) {
            m_roomServerEpoch = epoch;
            m_roomServerElapsed.restart();
          } else {
            m_roomServerEpoch = 0;
            m_roomServerElapsed.invalidate();
          }
        } else {
          m_roomServerEpoch = 0;
          m_roomServerElapsed.invalidate();
        }
        m_roomHasStartTime = false;
        m_roomStartReached = true;
        m_roomStartTime = QDateTime();
        if (!startTime.isEmpty()) {
          QDateTime parsed =
              QDateTime::fromString(startTime, "yyyy-MM-dd HH:mm:ss");
          if (parsed.isValid()) {
            m_roomHasStartTime = true;
            m_roomStartTime = parsed;
            m_roomStartReached = startedFlag;
          }
        }
        if (m_roomStartTimer)
          m_roomStartTimer->stop();
        // Quyết định quyền host dựa vào hostName do server trả về,
        // tránh mất dấu khi danh sách owned bị reset (logout/restart).
        m_isCurrentRoomHost = (host == m_username);
        updateRoomStartState(true);
        updateRoomActionPermissions();

        // Load full product list for this room
        ui->tblRoomProducts->setRowCount(0);
        sendRequest(QString("GET_PRODUCTS|%1\n").arg(m_currentRoomId));

        ui->stackedWidget->setCurrentIndex(3);
	      }
	    }

	    else if (line.startsWith("OK|PRODUCT_LIST")) {
	      int roomId = line.section('|', 2, 2).toInt();
	      QString data = line.section('|', 3);
	      if (roomId != m_currentRoomId)
	        continue;

	      // Case 1: product list của room đang ở màn hình phòng đấu giá
	      if (roomId == m_currentRoomId) {
	        ui->tblRoomProducts->setRowCount(0);

	        QStringList entries = data.split(';', Qt::SkipEmptyParts);
	        for (const QString &entry : entries) {
	          QStringList fields = entry.split(',');
	          if (fields.size() < 7)
	            continue;

	          int productId = fields[0].toInt();
	          QString status = fields[1].trimmed();
	          QString name = fields[2];
	          int startPrice = fields[3].toInt();
	          int buyNowPrice = fields[4].toInt();
	          int duration = fields[5].toInt();
	          QString description = fields[6];

	          int row = ui->tblRoomProducts->rowCount();
	          ui->tblRoomProducts->insertRow(row);

	          auto *nameItem = new QTableWidgetItem(name);
	          nameItem->setData(Qt::UserRole, productId);
	          nameItem->setData(Qt::UserRole + 1, status);
	          nameItem->setData(Qt::UserRole + 2, description);
	          nameItem->setData(Qt::UserRole + 3, startPrice);
	          nameItem->setData(Qt::UserRole + 4, buyNowPrice);
	          nameItem->setData(Qt::UserRole + 5, duration);

	          auto *statusItem = new QTableWidgetItem(statusToText(status));
	          statusItem->setData(Qt::UserRole, productId);
	          statusItem->setData(Qt::UserRole + 1, status);

	          ui->tblRoomProducts->setItem(row, 0, nameItem);
	          ui->tblRoomProducts->setItem(row, 1, statusItem);
	        }
	        refreshProductTableStyles();

	        // Cache active product description for the top button
	        m_activeProductId = -1;
	        m_activeProductDescription.clear();
	        for (int row = 0; row < ui->tblRoomProducts->rowCount(); ++row) {
	          QTableWidgetItem *nameItem = ui->tblRoomProducts->item(row, 0);
	          if (!nameItem)
	            continue;
	          QString status = nameItem->data(Qt::UserRole + 1).toString();
	          if (status == "ACTIVE") {
	            m_activeProductId = nameItem->data(Qt::UserRole).toInt();
	            m_activeProductDescription =
	                nameItem->data(Qt::UserRole + 2).toString();
	            break;
	          }
	        }
	      }

	    }

	    // 2. XỬ LÝ KHI CÓ NGƯỜI RA GIÁ (BROADCAST)
	    // Server: NEW_BID|<price>|<username>
	    else if (line.startsWith("NEW_BID")) {
      // In ra chuỗi thô để xem có ký tự lạ không
      qDebug() << "1. Raw string received:" << line;

      QStringList parts = line.split('|');
      qDebug() << "2. Split size:" << parts.size(); // Phải >= 3 mới đúng

      if (parts.size() >= 3) {
        int newPrice = parts[1].toInt();
        QString userName = parts[2];
        int bidCount = parts.size() >= 4 ? parts[3].toInt() : 0;

        qDebug() << "3. New Price extracted:" << newPrice;

        // --- CẬP NHẬT GIAO DIỆN ---

        // Cố tình update cứng để test xem Label có chết không
        // Nếu dòng này chạy mà giao diện không đổi -> Label bị lỗi kết nối UI
        m_currentPriceValue = newPrice;
        ui->lblCurrentPrice->setText(formatPrice(newPrice));

        QString coloredName = colorizeName(userName);
        ui->txtRoomLog->append(coloredName +
                               " trả giá: " + formatPrice(newPrice));
        ui->lblBidCount->setText(QString::number(bidCount));
        ui->lblLeader->setText(coloredName);

        qDebug() << "4. UI Updated successfully!";
      } else {
        qDebug() << "ERROR: Split size wrong! Check protocol separator.";
      }
	    } else if (line.startsWith("PRODUCT_STATUS")) {
	      // Server: PRODUCT_STATUS|RoomID|ProductID|Status
	      QStringList parts = line.split('|');
	      if (parts.size() >= 4) {
	        int roomId = parts[1].toInt();
	        int productId = parts[2].toInt();
	        QString status = parts[3].trimmed();

	        if (roomId == m_currentRoomId) {
	          for (int row = 0; row < ui->tblRoomProducts->rowCount(); ++row) {
	            QTableWidgetItem *nameItem = ui->tblRoomProducts->item(row, 0);
	            QTableWidgetItem *statusItem = ui->tblRoomProducts->item(row, 1);
	            if (!nameItem || !statusItem)
	              continue;

	            if (nameItem->data(Qt::UserRole).toInt() == productId) {
	              nameItem->setData(Qt::UserRole + 1, status);
	              statusItem->setData(Qt::UserRole + 1, status);
	              statusItem->setText(statusToText(status));
	              if (status == "ACTIVE") {
	                m_activeProductId = productId;
	                m_activeProductDescription =
	                    nameItem->data(Qt::UserRole + 2).toString();
	              }
	              break;
	            }
	          }
	          refreshProductTableStyles();
	        }
	      }
    } else if (line.startsWith("ROOM_MEMBER_COUNT")) {
      // Server: ROOM_MEMBER_COUNT|RoomID|Count
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        int roomId = parts[1].toInt();
        int count = parts[2].toInt();
        if (roomId == m_currentRoomId) {
          ui->lblParticipants->setText(QString::number(count));
        }
      }
    } else if (line.startsWith("ROOM_STATUS")) {
      // Server: ROOM_STATUS|RoomID|STARTED|StartTime
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        int roomId = parts[1].toInt();
        QString status = parts[2];
        if (roomId == m_currentRoomId && status == "STARTED") {
          if (parts.size() >= 4) {
            QDateTime dt =
                QDateTime::fromString(parts[3], "yyyy-MM-dd HH:mm:ss");
            if (dt.isValid()) {
              m_roomHasStartTime = true;
              m_roomStartTime = dt;
            }
          }
          m_roomStartReached = true;
          updateRoomStartState(true);
        } else if (roomId == m_currentRoomId && status == "STOPPED") {
          ui->txtRoomLog->append(
              "<span style=\"color:#e74c3c;\">Room has been stopped.</span>");
          m_roomHasStartTime = false;
          m_roomStartReached = true;
          updateRoomActionPermissions();
        }
      }
    } else if (line.startsWith("CHAT")) {
      // Server: CHAT|<username>|<message>
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        QString sender = parts[1];
        QString message = parts[2];
        ui->txtChatLog->append(colorizeName(sender) + ": " +
                               message.toHtmlEscaped());
      }
    }
    // 1. XỬ LÝ ĐỒNG HỒ ĐẾM NGƯỢC
    // Server: TIME_UPDATE|299
    else if (line.startsWith("TIME_UPDATE")) {
      int seconds = line.section('|', 1, 1).toInt();

      // Hiển thị lên LCD Number hoặc Label
      ui->lcdTimer->display(seconds);

      // Đổi màu nếu sắp hết giờ (< 30s)
      if (seconds < 30) {
        ui->lcdTimer->setStyleSheet("color: red;");
      } else {
        ui->lcdTimer->setStyleSheet("color: black;"); // Mặc định
      }
    } else if (line.startsWith("TIME_ALERT")) {
      int sec = line.section('|', 1, 1).toInt();
      ui->txtRoomLog->append("<span style=\"color:#e67e22;\">Cảnh báo: còn " +
                             QString::number(sec) + " giây.</span>");
    }

    // 2. XỬ LÝ KẾT THÚC PHIÊN (SOLD / SOLD_ITEM)
    else if (line.startsWith("SOLD_ITEM")) {
      // SOLD_ITEM|itemName|price|winner
      QStringList parts = line.split('|');
      if (parts.size() >= 4) {
        QString item = parts[1];
        int priceVal = parts[2].toInt();
        QString winner = parts[3];
        ui->txtRoomLog->append("--- KẾT THÚC ---");
        ui->txtRoomLog->append("Sản phẩm " + item + " đã thuộc về " +
                               colorizeName(winner) + " với giá " +
                               formatPrice(priceVal));
        qDebug() << "Client: Sold Item:" << item << priceVal << winner;
      }
    } else if (line.startsWith("SOLD|")) {
      QString priceTok = line.section('|', 1, 1);
      QString winnerTok = line.section('|', 2, 2);
      bool okPrice = false;
      int priceVal = priceTok.toInt(&okPrice);
      if (!okPrice) {
        bool okAlt = false;
        int altPrice = winnerTok.toInt(&okAlt);
        if (okAlt) {
          priceVal = altPrice;
          winnerTok = priceTok;
        }
      }

      ui->lcdTimer->display(0);
      ui->txtRoomLog->append("--- KẾT THÚC ---");
      ui->txtRoomLog->append("Sản phẩm đã thuộc về " + colorizeName(winnerTok) +
                             " với giá " + formatPrice(priceVal));

      QMessageBox::information(this, "Kết thúc", "Sản phẩm đã được bán!");

      // Disable các nút để không bấm được nữa
      ui->btnBid->setEnabled(false);
      ui->btnQuickBid->setEnabled(false);
    } else if (line.startsWith("ERR|INVALID_PRICE_CONFIG")) {
      QMessageBox::critical(
          this, "Lỗi tạo phòng",
          "Server từ chối: Giá mua ngay phải lớn hơn giá khởi điểm.");
    } else if (line.startsWith("ERR|PRICE_TOO_LOW")) {
      QMessageBox::warning(
          this, "Đấu giá thất bại",
          "Giá bạn đặt không hợp lệ!\n\n"
          "Quy tắc: Bạn phải đặt giá cao hơn giá hiện tại ít nhất 10.000 VND.");

      // (Tùy chọn) Reset lại ô nhập liệu cho sạch
      ui->txtBidAmount->clear();
      ui->txtBidAmount->setFocus();
    } else if (line.startsWith("ERR|ROOM_NOT_STARTED")) {
      ui->txtRoomLog->append(
          "<span style=\"color:#e74c3c;\">Phòng đấu giá chưa bắt đầu.</span>");
    } else if (line.startsWith("ERR|LOGIN_FAILED")) {
      QMessageBox::critical(this, "Đăng nhập thất bại",
                            "Tài khoản hoặc mật khẩu không chính xác!\n"
                            "Vui lòng kiểm tra lại hoặc Đăng ký mới.");
    } else if (line.startsWith("ERR|ALREADY_LOGGED_IN")) {
      QMessageBox::warning(
          this, "Cảnh báo đăng nhập",
          "Tài khoản này đang trực tuyến ở một thiết bị khác.\n"
          "Không thể đăng nhập cùng lúc.");
    } else if (line.startsWith("OK|PASS_CHANGED")) {
      // Tìm Dialog đang mở (nếu có)
      ProfileDialog *dlg = this->findChild<ProfileDialog *>();
      if (dlg && dlg->isVisible()) {
        dlg->onChangePassResult(true, "Đổi mật khẩu thành công!");
      }
    } else if (line.startsWith("ERR|WRONG_PASS")) {
      ProfileDialog *dlg = this->findChild<ProfileDialog *>();
      if (dlg && dlg->isVisible()) {
        dlg->onChangePassResult(false, "Mật khẩu cũ không đúng.");
      }
    } else if (line.startsWith("OK|WON_LIST")) {
      QString data = line.section('|', 2);
      ProfileDialog *dlg = this->findChild<ProfileDialog *>();
      if (dlg && dlg->isVisible()) {
        dlg->updateWonList(data);
      }
    } else if (line.startsWith("OK|HISTORY")) {
      QString data = line.section('|', 2);
      ProfileDialog *dlg = this->findChild<ProfileDialog *>();
      if (dlg && dlg->isVisible()) {
        dlg->updateHistory(data);
      }
    } else if (line.startsWith("OK|REGISTER_SUCCESS")) {
      QMessageBox::information(
          this, "Thành công",
          "Đăng ký tài khoản thành công!\nBạn có thể đăng nhập ngay bây giờ.");
    } else if (line.startsWith("ERR|USER_EXISTS")) {
      QMessageBox::critical(
          this, "Đăng ký thất bại",
          "Tên đăng nhập này đã tồn tại.\nVui lòng chọn tên khác.");
    }
    else if (line.startsWith("ERR|") &&
             !line.startsWith("ERR|WRONG_PASS") &&
             !line.startsWith("ERR|INVALID_PRICE_CONFIG") &&
             !line.startsWith("ERR|PRICE_TOO_LOW") &&
             !line.startsWith("ERR|ROOM_NOT_STARTED") &&
             !line.startsWith("ERR|LOGIN_FAILED") &&
             !line.startsWith("ERR|ALREADY_LOGGED_IN") &&
             !line.startsWith("ERR|USER_EXISTS")) {
      QString err = line.section('|', 1, 1);
      QMessageBox::warning(this, "Server Error", err);
    }
    // ...
	    else if (line.startsWith("NEXT_ITEM")) {
	      // Server: NEXT_ITEM|Name|Price|BuyNow|Duration
	      QStringList parts = line.split('|');
	      if (parts.size() >= 5) {
	        QString newName = parts[1];
        int newPrice = parts[2].toInt();
        int newBuyNow = parts[3].toInt();
        int newDuration = parts[4].toInt();
	
	        // 1. Cập nhật UI
	        ui->lblRoomName->setText("Đang đấu giá: " + newName);
	        m_currentPriceValue = newPrice;
	        m_buyNowPriceValue = newBuyNow;
	        ui->lblCurrentPrice->setText(formatPrice(newPrice));
        ui->lblBuyNowPrice->setText(formatPrice(newBuyNow));
        ui->lcdTimer->display(newDuration);

        // 2. Thông báo chuyển đổi
        ui->txtRoomLog->append("\n------------------------------");
        ui->txtRoomLog->append(">>> CHUYỂN SANG SẢN PHẨM TIẾP THEO <<<");
        ui->txtRoomLog->append("Sản phẩm: " + newName);
	        ui->txtRoomLog->append("Giá khởi điểm: " + formatPrice(newPrice));
	        ui->lblLeader->setText("-");
	        ui->lblBidCount->setText("0");

	        // 3. Reset các nút (nếu bị disable do sold trước đó)
	        ui->btnBid->setEnabled(true);
	        ui->btnBuyNow->setEnabled(true);
        ui->btnQuickBid->setEnabled(true);
        updateRoomActionPermissions();

        // 4. Reset style đồng hồ về màu đen (nếu đang đỏ)
        ui->lcdTimer->setStyleSheet("color: black;");

        QMessageBox::information(this, "Thông báo",
                                 "Sản phẩm tiếp theo: " + newName);
      }
    } else if (line.startsWith("SOLD_ITEM")) {
      // Chỉ hiện thông báo text, KHÔNG disable nút vì còn sản phẩm sau
      QStringList parts = line.split('|');
      int soldPrice = parts.size() >= 3 ? parts[2].toInt() : 0;
      QString winner = parts.size() >= 4 ? parts[3] : "Unknown";
      ui->txtRoomLog->append("!!! KẾT THÚC: " + parts[1] + " thuộc về " +
                             colorizeName(winner) + " với giá " +
                             formatPrice(soldPrice));
    }
    // ...

    // 3. XỬ LÝ KHÔNG AI MUA (CLOSED)
    else if (line.startsWith("CLOSED")) {
      ui->lcdTimer->display(0);
      ui->txtRoomLog->append("--- KẾT THÚC ---");
      ui->txtRoomLog->append("Không có ai đấu giá. Phiên bị hủy.");
      ui->btnBid->setEnabled(false);
      ui->btnQuickBid->setEnabled(false);
    }
  }
}

void MainWindow::on_txtSearch_textChanged(const QString &arg1) {
  // Lấy từ khóa người dùng đang nhập (arg1)
  QString keyword = arg1.trimmed();

  // Duyệt qua tất cả các dòng trong danh sách phòng
  for (int i = 0; i < ui->listRooms->count(); ++i) {
    QListWidgetItem *item = ui->listRooms->item(i);
    QString roomText = item->text();
    // roomText ví dụ: "Phòng 1: iPhone 15 - Giá: 2000"

    // Logic lọc:
    // 1. Nếu từ khóa rỗng -> Hiện hết
    // 2. Nếu trong chuỗi có chứa từ khóa (Không phân biệt hoa thường) -> Hiện
    // 3. Ngược lại -> Ẩn

    if (keyword.isEmpty()) {
      item->setHidden(false);
    } else if (roomText.contains(keyword, Qt::CaseInsensitive)) {
      item->setHidden(false);
    } else {
      item->setHidden(true);
    }
  }
}

void MainWindow::on_cboSort_currentIndexChanged(int index) {
  // 1. Lấy toàn bộ Item từ ListWidget ra một danh sách tạm
  QList<QListWidgetItem *> items;
  int count = ui->listRooms->count();
  for (int i = 0; i < count; ++i) {
    items.append(ui->listRooms->takeItem(0)); // Lấy ra và xóa khỏi UI
  }

  // 2. Định nghĩa logic sắp xếp dựa trên index của ComboBox
  // Index 0: Mới nhất (ID giảm dần)
  // Index 1: Giá tăng dần
  // Index 2: Giá giảm dần

  if (index == 0) {
    // Sắp xếp theo ID (UserRole + 2) giảm dần (Phòng mới tạo ID sẽ to hơn)
    std::sort(items.begin(), items.end(),
              [](QListWidgetItem *a, QListWidgetItem *b) {
                return a->data(Qt::UserRole + 2).toInt() >
                       b->data(Qt::UserRole + 2).toInt();
              });
  } else if (index == 1) {
    // Giá tăng dần (UserRole + 1)
    std::sort(items.begin(), items.end(),
              [](QListWidgetItem *a, QListWidgetItem *b) {
                return a->data(Qt::UserRole + 1).toInt() <
                       b->data(Qt::UserRole + 1).toInt();
              });
  } else if (index == 2) {
    // Giá giảm dần
    std::sort(items.begin(), items.end(),
              [](QListWidgetItem *a, QListWidgetItem *b) {
                return a->data(Qt::UserRole + 1).toInt() >
                       b->data(Qt::UserRole + 1).toInt();
              });
  }

  // 3. Đưa các Item đã sắp xếp quay lại UI
  for (QListWidgetItem *item : items) {
    ui->listRooms->addItem(item);
  }
}

void MainWindow::resetRoomStartState() {
  if (m_roomStartTimer)
    m_roomStartTimer->stop();
  m_roomStartTime = QDateTime();
  m_roomHasStartTime = false;
  m_roomStartReached = true;
  m_roomServerEpoch = 0;
  m_roomServerElapsed.invalidate();
}

void MainWindow::updateRoomStartState(bool force) {
  if (!m_roomHasStartTime) {
    if (force || !m_roomStartReached) {
      m_roomStartReached = true;
      updateRoomActionPermissions();
    }
    return;
  }

  QDateTime now = QDateTime::currentDateTime();
  if (m_roomServerEpoch > 0 && m_roomServerElapsed.isValid()) {
    qint64 nowEpoch = m_roomServerEpoch + (m_roomServerElapsed.elapsed() / 1000);
    now = QDateTime::fromSecsSinceEpoch(nowEpoch);
  }
  bool reached = now >= m_roomStartTime;
  if (force || reached != m_roomStartReached) {
    m_roomStartReached = reached;
    updateRoomActionPermissions();
    if (reached) {
      ui->txtRoomLog->append(
          "<span style=\"color:#2ecc71;\">Phiên đấu giá đã bắt đầu.</span>");
      if (m_roomStartTimer && m_roomStartTimer->isActive())
        m_roomStartTimer->stop();
    } else {
      ui->txtRoomLog->append("<span style=\"color:#e67e22;\">Phòng chưa bắt "
                             "đầu (lúc " +
                             m_roomStartTime.toString("yyyy-MM-dd HH:mm:ss") +
                             ").</span>");
      if (m_roomStartTimer && !m_roomStartTimer->isActive())
        m_roomStartTimer->start();
    }
  }
}

void MainWindow::updateRoomActionPermissions() {
  bool canAct = !m_isCurrentRoomHost && m_roomStartReached;
  ui->btnBid->setEnabled(canAct);
  ui->btnBuyNow->setEnabled(canAct);
  ui->btnQuickBid->setEnabled(canAct);
}

QString MainWindow::formatPrice(int value) const {
  QLocale vn(QLocale::Vietnamese);
  return vn.toString(value) + " VND";
}

QString MainWindow::colorizeName(const QString &name) const {
  return "<span style=\"color:#1E90FF; font-weight:bold;\">" +
         name.toHtmlEscaped() + "</span>";
}

void MainWindow::updateRoomInfoUI(const QString &host, const QString &leader,
                                  int bidCount,
                                  const QString &participants) {
  ui->lblHost->setText(host.isEmpty() ? "N/A" : host);
  ui->lblLeader->setText(leader.isEmpty() ? "-" : colorizeName(leader));
  ui->lblBidCount->setText(QString::number(bidCount));
  ui->lblParticipants->setText(participants.isEmpty() ? "0" : participants);
}

// --- User Profile ---

void MainWindow::on_btnProfile_clicked() {
  ProfileDialog dlg(this, this);
  dlg.exec(); // Modal dialog
}

void MainWindow::sendRequest(const QString &cmd) {
  if (m_socket->state() == QAbstractSocket::ConnectedState) {
    m_socket->write(cmd.toUtf8());
  } else {
    QMessageBox::warning(this, "Lỗi", "Chưa kết nối Server.");
  }
}

void MainWindow::showMyRoomsView() {
  ui->stackedWidget->setCurrentIndex(2);
  requestMyRooms();
  if (m_myRoomsGateTimer && !m_myRoomsGateTimer->isActive())
    m_myRoomsGateTimer->start();
}

void MainWindow::reloadMyRooms() { requestMyRooms(); }

void MainWindow::backToLobbyFromMyRooms() {
  ui->stackedWidget->setCurrentIndex(1);
  if (m_myRoomsGateTimer && m_myRoomsGateTimer->isActive())
    m_myRoomsGateTimer->stop();
}

void MainWindow::myRoomsSelectionChanged() {
  bool hasSelection = !m_tblMyRooms->selectedItems().isEmpty();
  if (!hasSelection) {
    m_btnEditMyRoom->setEnabled(false);
    m_btnStopMyRoom->setEnabled(false);
    return;
  }
  int row = m_tblMyRooms->currentRow();
  if (row < 0) {
    m_btnEditMyRoom->setEnabled(false);
    m_btnStopMyRoom->setEnabled(false);
    return;
  }
  m_btnEditMyRoom->setEnabled(canEditMyRoomRow(row));
  m_btnStopMyRoom->setEnabled(canStopMyRoomRow(row));
}

void MainWindow::editSelectedRoom() {
  if (m_tblMyRooms->selectedItems().isEmpty())
    return;
  int row = m_tblMyRooms->currentRow();
  if (row < 0)
    return;
  int roomId = m_tblMyRooms->item(row, 0)->text().toInt();

  QString reason;
  if (!canEditMyRoomRow(row, &reason)) {
    QMessageBox::warning(this, "Edit Not Allowed",
                         reason.isEmpty() ? "Không đủ điều kiện chỉnh sửa."
                                          : reason);
    return;
  }
  // Lấy đầy đủ dữ liệu edit từ server (roomName + startTime + product list)
  sendRequest(QString("GET_ROOM_EDIT_DATA|%1\n").arg(roomId));
}

void MainWindow::stopSelectedRoom() {
  if (m_tblMyRooms->selectedItems().isEmpty())
    return;
  int row = m_tblMyRooms->currentRow();
  if (row < 0)
    return;
  if (!canStopMyRoomRow(row)) {
    QMessageBox::warning(this, "Stop Not Allowed",
                         "Chỉ dừng được khi phòng đang OPEN.");
    return;
  }
  int roomId = m_tblMyRooms->item(row, 0)->text().toInt();
  sendRequest(QString("STOP_ROOM|%1\n").arg(roomId));
}

void MainWindow::requestMyRooms() {
  if (m_socket->state() != QAbstractSocket::ConnectedState) {
    QMessageBox::warning(this, "Lỗi", "Chưa kết nối Server.");
    return;
  }
  m_tblMyRooms->setRowCount(0);
  m_btnEditMyRoom->setEnabled(false);
  m_btnStopMyRoom->setEnabled(false);
  sendRequest("MY_ROOMS\n");
}

void MainWindow::populateMyRoomsTable(const QString &payload) {
  m_tblMyRooms->setRowCount(0);
  QStringList entries = payload.split(';', Qt::SkipEmptyParts);
  for (const QString &entry : entries) {
    QStringList fields = entry.split(',');
    if (fields.size() < 4)
      continue;

    int row = m_tblMyRooms->rowCount();
    m_tblMyRooms->insertRow(row);

    auto *idItem = new QTableWidgetItem(fields[0]);
    idItem->setData(Qt::UserRole, fields[0].toInt());
    m_tblMyRooms->setItem(row, 0, idItem);
    m_tblMyRooms->setItem(row, 1, new QTableWidgetItem(fields[1]));

    QString startTime = fields[2].isEmpty() ? "-" : fields[2];
    auto *startItem = new QTableWidgetItem(startTime);
    startItem->setData(Qt::UserRole, fields[2]);
    m_tblMyRooms->setItem(row, 2, startItem);

    auto *statusItem = new QTableWidgetItem(fields[3]);
    m_tblMyRooms->setItem(row, 3, statusItem);
  }
  myRoomsSelectionChanged();
}

QString MainWindow::buildProductPayload(const std::vector<ProductInfo> &products) const {
  QStringList parts;
  for (const auto &p : products) {
    parts.append(QString("%1,%2,%3,%4,%5")
                     .arg(p.name)
                     .arg(p.startPrice)
                     .arg(p.buyNowPrice)
                     .arg(p.duration)
                     .arg(p.description));
  }
  return parts.join(';');
}

qint64 MainWindow::myRoomsNowEpoch() const {
  if (m_myRoomsServerEpoch > 0 && m_myRoomsElapsed.isValid()) {
    return m_myRoomsServerEpoch + (m_myRoomsElapsed.elapsed() / 1000);
  }
  return QDateTime::currentDateTime().toSecsSinceEpoch();
}

bool MainWindow::canStopMyRoomRow(int row) const {
  if (!m_tblMyRooms || row < 0 || row >= m_tblMyRooms->rowCount())
    return false;
  QTableWidgetItem *statusItem = m_tblMyRooms->item(row, 3);
  if (!statusItem)
    return false;
  return statusItem->text().trimmed() == "OPEN";
}

bool MainWindow::canEditMyRoomRow(int row, QString *outReason) const {
  if (outReason)
    outReason->clear();
  if (!canStopMyRoomRow(row)) {
    if (outReason)
      *outReason = "Chỉ chỉnh sửa được khi phòng đang OPEN.";
    return false;
  }

  QString rawStartTime;
  if (QTableWidgetItem *startItem = m_tblMyRooms->item(row, 2))
    rawStartTime = startItem->data(Qt::UserRole).toString().trimmed();

  if (rawStartTime.isEmpty()) {
    if (outReason)
      *outReason = "Phòng cần có start time để chỉnh sửa.";
    return false;
  }

  QDateTime start = QDateTime::fromString(rawStartTime, "yyyy-MM-dd HH:mm:ss");
  if (!start.isValid()) {
    if (outReason)
      *outReason = "Start time không hợp lệ.";
    return false;
  }

  const qint64 secondsRemaining = start.toSecsSinceEpoch() - myRoomsNowEpoch();
  if (secondsRemaining < 5 * 60) {
    if (outReason)
      *outReason =
          "Bạn chỉ được chỉnh sửa khi còn ít nhất 5 phút trước start time.";
    return false;
  }

  return true;
}
