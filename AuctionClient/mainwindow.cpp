#include "mainwindow.h"
#include "createroomdialog.h"
#include "profiledialog.h" // <-- Added here
#include "registerdialog.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  // Khởi tạo Socket
  m_socket = new QTcpSocket(this);
  connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

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

  // Xóa log chat cũ để lần sau vào phòng khác không bị lẫn lộn
  ui->txtRoomLog->clear();
  ui->txtChatLog->clear();

  // (Tùy chọn) Reset ID phòng hiện tại
  m_currentRoomId = -1;
  m_currentPriceValue = 0;
  m_buyNowPriceValue = 0;
  m_isCurrentRoomHost = false;
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
  m_ownedRoomIds.clear();

  // 3. Reset giao diện
  ui->txtUser->clear();
  ui->txtPass->clear();
  ui->tableRooms->setRowCount(0);
  ui->txtChatLog->clear();
  ui->txtRoomLog->clear();

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
    // Ví dụ: "iPhone,1000,2000,60;TaiNghe,500,1000,30"
    QStringList productStrings;
    for (const auto &p : products) {
      QString itemStr = QString("%1,%2,%3,%4")
                            .arg(p.name)
                            .arg(p.startPrice)
                            .arg(p.buyNowPrice)
                            .arg(p.duration);
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
  int row = ui->tableRooms->currentRow();
  if (row < 0) {
    QMessageBox::warning(this, "Lỗi", "Vui lòng chọn một phòng!");
    return;
  }

  // 2. Lấy ID từ cột 0, user role
  QTableWidgetItem *item = ui->tableRooms->item(row, 0);
  if (!item)
    return;

  int roomId = item->data(Qt::UserRole).toInt();

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
      // Server gửi: OK|LIST|1:Laptop:1500;2:Phone:500;
      // 1. Phân dữ liệu
      QString data = line.section('|', 2);
      qDebug() << "[DEBUG] List Data Raw:" << data;

      QStringList rooms = data.split(';', Qt::SkipEmptyParts);

      // 2. Setup Table
      ui->tableRooms->setRowCount(0); // Clear old data
      ui->tableRooms->setColumnCount(2);
      QStringList headers;
      headers << "Phòng" << "Giá";
      ui->tableRooms->setHorizontalHeaderLabels(headers);
      ui->tableRooms->horizontalHeader()->setStretchLastSection(true);
      ui->tableRooms->setColumnWidth(0, 300); // Give Room column more space

      // 3. Populate
      for (const QString &roomStr : rooms) {
        // Format: ID:Name:Price:BuyNow
        QStringList parts = roomStr.split(':');

        if (parts.size() >= 2) {
          bool ok;
          int rId = parts[0].toInt(&ok);
          if (!ok)
            continue;

          QString name = parts[1];
          QString priceStr = (parts.size() >= 3) ? parts[2] : "0";
          int price = priceStr.toInt();

          int row = ui->tableRooms->rowCount();
          ui->tableRooms->insertRow(row);

          // Column 0: Name (with ID hidden)
          QString displayRoom = QString("Phòng %1: %2").arg(rId).arg(name);
          QTableWidgetItem *itemRoom = new QTableWidgetItem(displayRoom);
          itemRoom->setData(Qt::UserRole, rId); // Store ID for logic
          itemRoom->setFlags(itemRoom->flags() ^ Qt::ItemIsEditable);
          ui->tableRooms->setItem(row, 0, itemRoom);

          // Column 1: Price
          QTableWidgetItem *itemPrice =
              new QTableWidgetItem(formatPrice(price));
          itemPrice->setFlags(itemPrice->flags() ^ Qt::ItemIsEditable);
          ui->tableRooms->setItem(row, 1, itemPrice);

          qDebug() << "[SUCCESS] Added room ID:" << rId;
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
    // 1. XỬ LÝ VÀO PHÒNG THÀNH CÔNG
    // Server: OK|JOINED|<id>|<name>|<price>
    else if (line.startsWith("OK|JOINED")) {
      QStringList parts = line.split('|');

      // Server gửi:
      // OK|JOINED|ID|Name|CurrentPrice|BuyNowPrice|Host|Leader|BidCount|ParticipantCount|NextName|NextStart|NextDuration
      if (parts.size() >= 12) {
        m_currentRoomId = parts[2].toInt();
        QString name = parts[3];
        int price = parts[4].toInt();
        int buyNow = parts[5].toInt(); // <-- Lấy giá mua ngay
        QString host = parts[6];
        QString leader = parts[7];
        int bidCount = parts[8].toInt();
        QString participants = parts[9];
        QString nextName = parts[10];
        int nextStart = parts[11].toInt();
        int nextDuration = parts.size() > 12 ? parts[12].toInt() : 0;

        // Cập nhật UI
        ui->lblRoomName->setText("Đang đấu giá: " + name);
        ui->lblItemName->setText("Sản phẩm: " + name);
        m_currentPriceValue = price;
        m_buyNowPriceValue = buyNow;
        ui->lblCurrentPrice->setText(formatPrice(price));

        // --- HIỂN THỊ GIÁ MUA NGAY ---
        ui->lblBuyNowPrice->setText(formatPrice(buyNow));
        // -----------------------------

        ui->txtRoomLog->clear();
        ui->txtRoomLog->append("--- Bắt đầu phiên đấu giá ---");
        ui->txtChatLog->clear();
        ui->txtChatLog->append("=== Chat phòng ===");

        updateRoomInfoUI(host, leader, bidCount, participants, nextName,
                         nextStart, nextDuration);

        // Quyết định quyền host dựa vào hostName do server trả về,
        // tránh mất dấu khi danh sách owned bị reset (logout/restart).
        m_isCurrentRoomHost = (host == m_username);
        updateRoomActionPermissions();

        ui->stackedWidget->setCurrentIndex(2);
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
        ui->lblItemName->setText("Sản phẩm: " + newName);
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
        ui->lblNextInfo->setText("-");
        ui->lblParticipants->setText("-");

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
  QString keyword = arg1.trimmed();

  for (int i = 0; i < ui->tableRooms->rowCount(); ++i) {
    QTableWidgetItem *item = ui->tableRooms->item(i, 0); // Check "Room" column
    if (!item)
      continue;

    QString roomText = item->text();
    // Assuming "Phòng ID: Name" format

    bool match =
        keyword.isEmpty() || roomText.contains(keyword, Qt::CaseInsensitive);
    ui->tableRooms->setRowHidden(i, !match);
  }
}

void MainWindow::on_cboSort_currentIndexChanged(int index) {
  // Index 0: Newest (ID Descending) -> Table implicitly sorted by insertion if
  // we insert bottom. Actually, let's use QTableWidget's sorting.

  if (index == 0) {
    // Mới nhất -> Sort by ID (UserRole in Col 0) descending
    // But QTableWidget sorts by text by default.
    // Simplified: Just re-trigger refresh to get order from server (simplest)
    // OR: Implement manual sort.
    // Let's rely on server side order for "Newest" (since server sends them
    // that way usually) But if we want client side sort: We will perform a
    // refresh which is safest as client logic was complex. However, to keep it
    // fast, let's just clear and ask server again if feasible. But wait, the
    // original logic sorted locally.

    // Let's implement simple local sort.
    // Since QTableWidgetItem sorting is lexical by default, numbers might sort
    // wrong (10 < 2). Ideally we subclass QTableWidgetItem but for now let's
    // just Refresh. It acts as a "Reload" filter too.
    on_btnRefresh_clicked();
  } else if (index == 1) {
    // Price Ascending -> Sort by Column 1
    ui->tableRooms->sortItems(1, Qt::AscendingOrder);
  } else if (index == 2) {
    // Price Descending -> Sort by Column 1
    ui->tableRooms->sortItems(1, Qt::DescendingOrder);
  }
}

void MainWindow::updateRoomActionPermissions() {
  bool canAct = !m_isCurrentRoomHost;
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
                                  int bidCount, const QString &participants,
                                  const QString &nextName, int nextStart,
                                  int nextDuration) {
  ui->lblHost->setText(host.isEmpty() ? "N/A" : host);
  ui->lblLeader->setText(leader.isEmpty() ? "-" : colorizeName(leader));
  ui->lblBidCount->setText(QString::number(bidCount));
  ui->lblParticipants->setText(participants.isEmpty() ? "0" : participants);

  if (nextName == "-" || nextName.isEmpty()) {
    ui->lblNextInfo->setText("-");
  } else {
    ui->lblNextInfo->setText(nextName + " | " + formatPrice(nextStart) + " | " +
                             QString::number(nextDuration) + "s");
  }
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
