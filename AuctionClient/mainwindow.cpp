#include "mainwindow.h"
#include "createroomdialog.h"
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
  // 1. Lấy giá hiện tại từ giao diện (bóc tách từ chuỗi hiển thị hoặc lưu biến
  // riêng)

  int currentPrice = ui->lblCurrentPrice->text().toInt();
  int bidAmount = ui->txtBidAmount->text().toInt();

  // 2. Kiểm tra quy tắc +10.000
  if (bidAmount < currentPrice + 10000) {
    QMessageBox::warning(
        this, "Không hợp lệ",
        "Giá đấu phải cao hơn giá hiện tại ít nhất 10.000 VND!");
    return; // Dừng lại ngay, KHÔNG gửi gì lên Server cả
  }

  // 3. Nếu OK thì mới gửi
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

  // 3. Reset giao diện
  ui->txtUser->clear();
  ui->txtPass->clear();
  ui->listRooms->clear();
  ui->txtChatLog->clear();
  ui->txtRoomLog->clear();

  // 4. Chuyển về màn hình Login
  ui->stackedWidget->setCurrentIndex(0);

  QMessageBox::information(this, "Đăng xuất", "Bạn đã đăng xuất thành công.");
}

void MainWindow::on_btnCreateRoom_clicked() {
  CreateRoomDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    QString roomName = dlg.getRoomName();
    auto products = dlg.getProductList();

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

    // Gửi lệnh: CREATE_ROOM|RoomName|ProductListString
    QString msg =
        QString("CREATE_ROOM|%1|%2\n")
            .arg(roomName)
            .arg(productStrings.join(";")); // Nối các SP bằng dấu chấm phẩy

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

void MainWindow::on_btnHistory_clicked() { m_socket->write("VIEW_HISTORY\n"); }

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
      ui->listRooms
          ->clear(); // Đảm bảo tên widget đúng là lstRooms hoặc listRooms

      // 1. Lấy phần dữ liệu thực tế (Bỏ qua "OK|LIST|")
      // section('|', 2) sẽ lấy tất cả nội dung từ sau dấu gạch đứng thứ 2 trở
      // đi
      QString data = line.section('|', 2);

      // Debug xem dữ liệu thô sau khi cắt header là gì
      qDebug() << "[DEBUG] List Data Raw:" << data;

      QStringList rooms = data.split('\n', Qt::SkipEmptyParts);

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
      QMessageBox::information(this, "Thành công", "Đã tạo phòng đấu giá mới");
      // Tự động refresh lại list để thấy phòng mình vừa tạo
      on_btnRefresh_clicked();
    }
    // 1. XỬ LÝ VÀO PHÒNG THÀNH CÔNG
    // Server: OK|JOINED|<id>|<name>|<price>
    else if (line.startsWith("OK|JOINED")) {
      QStringList parts = line.split('|');

      // Server gửi: OK|JOINED|ID|Name|CurrentPrice|BuyNowPrice
      // Tổng cộng là 6 phần (index 0 đến 5)
      if (parts.size() >= 6) {
        m_currentRoomId = parts[2].toInt();
        QString name = parts[3];
        QString price = parts[4];
        QString buyNow = parts[5]; // <-- Lấy giá mua ngay

        // Cập nhật UI
        ui->lblRoomName->setText("Đang đấu giá: " + name);
        ui->lblCurrentPrice->setText(price);

        // --- HIỂN THỊ GIÁ MUA NGAY ---
        ui->lblBuyNowPrice->setText(buyNow);
        // -----------------------------

        ui->txtRoomLog->clear();
        ui->txtRoomLog->append("--- Bắt đầu phiên đấu giá ---");
        ui->txtChatLog->clear();
        ui->txtChatLog->append("=== Chat phòng ===");

        ui->stackedWidget->setCurrentIndex(2);
      }
    }

    // 2. XỬ LÝ KHI CÓ NGƯỜI RA GIÁ (BROADCAST)
    // Server: NEW_BID|<price>|<user_socket>
    else if (line.startsWith("NEW_BID")) {
      // In ra chuỗi thô để xem có ký tự lạ không
      qDebug() << "1. Raw string received:" << line;

      QStringList parts = line.split('|');
      qDebug() << "2. Split size:" << parts.size(); // Phải >= 3 mới đúng

      if (parts.size() >= 3) {
        QString newPrice = parts[1];
        QString userSocket = parts[2];

        qDebug() << "3. New Price extracted:" << newPrice;

        // --- CẬP NHẬT GIAO DIỆN ---

        // Cố tình update cứng để test xem Label có chết không
        // Nếu dòng này chạy mà giao diện không đổi -> Label bị lỗi kết nối UI
        ui->lblCurrentPrice->setText(newPrice);

        ui->txtRoomLog->append("User " + userSocket + " trả giá: " + newPrice);

        qDebug() << "4. UI Updated successfully!";
      } else {
        qDebug() << "ERROR: Split size wrong! Check protocol separator.";
      }
    } else if (line.startsWith("CHAT")) {
      // Server: CHAT|<socket>|<message>
      QStringList parts = line.split('|');
      if (parts.size() >= 3) {
        QString sender = parts[1];
        QString message = parts[2];
        ui->txtChatLog->append("Chat " + sender + ": " + message);
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
    }

    // 2. XỬ LÝ KẾT THÚC PHIÊN (SOLD)
    // Server: SOLD|<price>|<winner_socket>
    else if (line.startsWith("SOLD")) {
      QString price = line.section('|', 1, 1);
      QString winner = line.section('|', 2, 2);

      ui->lcdTimer->display(0);
      ui->txtRoomLog->append("--- KẾT THÚC ---");
      ui->txtRoomLog->append("Sản phẩm đã thuộc về User " + winner +
                             " với giá " + price);

      QMessageBox::information(this, "Kết thúc", "Sản phẩm đã được bán!");

      // Disable các nút để không bấm được nữa
      ui->btnBid->setEnabled(false);
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
    } else if (line.startsWith("OK|HISTORY")) {
      QString data =
          line.section('|', 2, 2); // Lấy phần dữ liệu sau OK|HISTORY|
      QStringList items = data.split(';', Qt::SkipEmptyParts);

      QString displayMsg = "=== CÁC PHIÊN ĐẤU GIÁ ĐÃ KẾT THÚC ===\n\n";
      for (const QString &item : items) {
        QStringList details = item.split(':');
        if (details.size() >= 3) {
          displayMsg += QString("Vật phẩm: %1\nGiá chốt: %2 VND\nNgười thắng: "
                                "%3\n----------------\n")
                            .arg(details[0], details[1], details[2]);
        }
      }

      if (items.isEmpty())
        displayMsg += "(Chưa có dữ liệu)";

      QMessageBox::information(this, "Lịch sử đấu giá", displayMsg);
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
        QString newPrice = parts[2];
        QString newBuyNow = parts[3];
        int newDuration = parts[4].toInt();

        // 1. Cập nhật UI
        ui->lblRoomName->setText("Đang đấu giá: " + newName);
        ui->lblCurrentPrice->setText(newPrice);
        ui->lblBuyNowPrice->setText("Mua ngay: " + newBuyNow);
        ui->lcdTimer->display(newDuration);

        // 2. Thông báo chuyển đổi
        ui->txtRoomLog->append("\n------------------------------");
        ui->txtRoomLog->append(">>> CHUYỂN SANG SẢN PHẨM TIẾP THEO <<<");
        ui->txtRoomLog->append("Sản phẩm: " + newName);
        ui->txtRoomLog->append("Giá khởi điểm: " + newPrice);

        // 3. Reset các nút (nếu bị disable do sold trước đó)
        ui->btnBid->setEnabled(true);
        ui->btnBuyNow->setEnabled(true);

        // 4. Reset style đồng hồ về màu đen (nếu đang đỏ)
        ui->lcdTimer->setStyleSheet("color: black;");

        QMessageBox::information(this, "Thông báo",
                                 "Sản phẩm tiếp theo: " + newName);
      }
    } else if (line.startsWith("SOLD_ITEM")) {
      // Chỉ hiện thông báo text, KHÔNG disable nút vì còn sản phẩm sau
      QStringList parts = line.split('|');
      ui->txtRoomLog->append("!!! KẾT THÚC: " + parts[1] + " thuộc về User " +
                             parts[3]);
    }
    // ...

    // 3. XỬ LÝ KHÔNG AI MUA (CLOSED)
    else if (line.startsWith("CLOSED")) {
      ui->lcdTimer->display(0);
      ui->txtRoomLog->append("--- KẾT THÚC ---");
      ui->txtRoomLog->append("Không có ai đấu giá. Phiên bị hủy.");
      ui->btnBid->setEnabled(false);
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
