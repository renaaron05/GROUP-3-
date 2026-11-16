#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QDir>
#include <QDate>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QStatusBar>
#include <QSet>
#include <QFileDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QColor>
#include <QVector>
#include <QStringList>
#include <algorithm>

static constexpr int MAX_CARS = 100;
static constexpr int MAX_SALES = 500;
static constexpr double VAT_RATE = 0.12;
static constexpr int LOW_STOCK_LEVEL = 2;
static const QStringList MONTHS = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

struct Car {
    int id = 0;
    QString brand, model;
    int year = 2026;
    double price = 0;
    int stock = 0;
};

struct Sale {
    int carId = 0;
    QString carName, customer, phone, address;
    int qty = 0;
    QDate date = QDate::currentDate();
    double subtotal = 0, discountPercent = 0, discountAmount = 0, vat = 0, total = 0;
};

static QString money(double n) { return QString("₱%1").arg(n, 0, 'f', 2); }
static QString safeField(QString s) { s.replace('|', '/'); s.replace('\n', ' '); s.replace('\r', ' '); return s.trimmed(); }

class CarDialog : public QDialog {
public:
    CarDialog(QWidget *parent = nullptr, const Car *car = nullptr) : QDialog(parent) {
        setWindowTitle(car ? "Edit vehicle" : "Add vehicle");
        setMinimumWidth(390);
        auto *v = new QVBoxLayout(this);
        auto *f = new QFormLayout;
        brand = new QLineEdit(car ? car->brand : "");
        model = new QLineEdit(car ? car->model : "");
        year = new QSpinBox; year->setRange(1990, QDate::currentDate().year() + 2); year->setValue(car ? car->year : QDate::currentDate().year());
        price = new QDoubleSpinBox; price->setRange(1, 999999999); price->setDecimals(2); price->setPrefix("₱ "); price->setValue(car ? car->price : 1);
        stock = new QSpinBox; stock->setRange(0, 1000); stock->setValue(car ? car->stock : 0);
        f->addRow("Brand", brand); f->addRow("Model", model); f->addRow("Year", year); f->addRow("Price", price); f->addRow("Stock", stock);
        v->addLayout(f);
        auto *row = new QHBoxLayout; row->addStretch();
        auto *cancel = new QPushButton("Cancel"); auto *save = new QPushButton(car ? "Save changes" : "Add vehicle");
        save->setObjectName("primaryButton"); row->addWidget(cancel); row->addWidget(save); v->addLayout(row);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, [this] {
            if (brand->text().trimmed().isEmpty() || model->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Missing information", "Brand and model are required."); return;
            }
            accept();
        });
    }
    Car result() const {
        Car c; c.brand = safeField(brand->text()); c.model = safeField(model->text());
        c.year = year->value(); c.price = price->value(); c.stock = stock->value(); return c;
    }
private:
    QLineEdit *brand, *model;
    QSpinBox *year, *stock;
    QDoubleSpinBox *price;
};

class SaleDialog : public QDialog {
public:
    SaleDialog(QWidget *parent, const Car &car) : QDialog(parent), vehicle(car) {
        setWindowTitle("Record a sale"); setMinimumWidth(440);
        auto *v = new QVBoxLayout(this); auto *f = new QFormLayout;
        auto *vehicleLabel = new QLabel(QString("%1 %2 (ID: %3)").arg(car.brand, car.model).arg(car.id));
        customer = new QLineEdit; phone = new QLineEdit; address = new QLineEdit;
        qty = new QSpinBox; qty->setRange(1, car.stock); qty->setValue(1);
        discount = new QDoubleSpinBox; discount->setRange(0, 20); discount->setDecimals(2); discount->setSuffix("%");
        f->addRow("Vehicle", vehicleLabel); f->addRow("Available stock", new QLabel(QString::number(car.stock)));
        f->addRow("Customer name", customer); f->addRow("Contact number", phone); f->addRow("Address", address);
        f->addRow("Quantity", qty); f->addRow("Discount (max 20%)", discount); v->addLayout(f);
        preview = new QLabel; preview->setObjectName("salePreview"); v->addWidget(preview);
        auto update = [this] {
            double sub = vehicle.price * qty->value(), disc = sub * discount->value() / 100.0;
            double vat = (sub - disc) * VAT_RATE;
            preview->setText(QString("Subtotal: %1\nDiscount: -%2\nVAT (12%): %3\nTOTAL: %4")
                .arg(money(sub), money(disc), money(vat), money(sub - disc + vat)));
        };
        connect(qty, qOverload<int>(&QSpinBox::valueChanged), this, [update](int){ update(); });
        connect(discount, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [update](double){ update(); });
        update();
        auto *row = new QHBoxLayout; row->addStretch(); auto *cancel = new QPushButton("Cancel");
        auto *confirm = new QPushButton("Confirm sale"); confirm->setObjectName("primaryButton");
        row->addWidget(cancel); row->addWidget(confirm); v->addLayout(row);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(confirm, &QPushButton::clicked, this, [this] {
            if (customer->text().trimmed().isEmpty()) { QMessageBox::warning(this, "Required", "Enter the customer's name."); return; }
            accept();
        });
    }
    Sale result() const {
        Sale s; s.carId = vehicle.id; s.carName = vehicle.brand + " " + vehicle.model;
        s.customer = safeField(customer->text()); s.phone = safeField(phone->text()); s.address = safeField(address->text());
        s.qty = qty->value(); s.date = QDate::currentDate(); s.subtotal = vehicle.price * s.qty;
        s.discountPercent = discount->value(); s.discountAmount = s.subtotal * s.discountPercent / 100.0;
        s.vat = (s.subtotal - s.discountAmount) * VAT_RATE; s.total = s.subtotal - s.discountAmount + s.vat;
        return s;
    }
private:
    Car vehicle;
    QLineEdit *customer, *phone, *address;
    QSpinBox *qty;
    QDoubleSpinBox *discount;
    QLabel *preview;
};

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("GROUP 03 | Car Dealership Management");
        resize(1350, 850); setMinimumSize(1080, 700);
        loadData(); buildUi(); refreshAll();
        statusBar()->showMessage("Ready");
    }
private:
    QVector<Car> cars;
    QVector<Sale> sales;
    int nextCarId = 1001;
    QStackedWidget *pages = nullptr;
    QTableWidget *inventoryTable = nullptr, *salesTable = nullptr, *recentTable = nullptr, *monthlyTable = nullptr;
    QLineEdit *searchBox = nullptr;
    QComboBox *sortBox = nullptr, *availabilityBox = nullptr;
    QLabel *modelsValue = nullptr, *stockValue = nullptr, *inventoryValueLabel = nullptr, *salesValue = nullptr;
    QLabel *lowStockValue = nullptr, *monthlyRevenueValue = nullptr, *lowStockList = nullptr;
    QLabel *reportSummary = nullptr;

    QString dataPath(const QString &name) const { return QDir(QCoreApplication::applicationDirPath()).filePath(name); }

    void loadData() {
        cars.clear(); sales.clear();
        QFile cf(dataPath("cars.txt"));
        if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&cf);
            while (!in.atEnd() && cars.size() < MAX_CARS) {
                QStringList f = in.readLine().split('|'); if (f.size() != 6) continue;
                bool a,b,c,d; Car x;
                x.id=f[0].toInt(&a); x.brand=f[1]; x.model=f[2]; x.year=f[3].toInt(&b); x.price=f[4].toDouble(&c); x.stock=f[5].toInt(&d);
                if (a&&b&&c&&d&&x.id>0&&x.year>=1990&&x.price>0&&x.stock>=0) { cars.push_back(x); nextCarId=qMax(nextCarId,x.id+1); }
            }
        }
        QFile sf(dataPath("sales.txt"));
        if (sf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&sf);
            while (!in.atEnd() && sales.size() < MAX_SALES) {
                QStringList f=in.readLine().split('|'); Sale s; bool ok=true;
                if (f.size()==6) { // Backward compatibility with the original project format.
                    s.carId=f[0].toInt(&ok); s.carName=f[1]; s.customer=f[2]; s.qty=f[3].toInt(&ok);
                    int month=f[4].toInt(&ok); s.date=QDate(QDate::currentDate().year(), qBound(1,month,12), 1);
                    s.total=f[5].toDouble(&ok); s.subtotal=s.total/1.12; s.vat=s.total-s.subtotal;
                } else if (f.size()>=11) {
                    s.carId=f[0].toInt(&ok); s.carName=f[1]; s.customer=f[2]; s.phone=f[3]; s.address=f[4];
                    s.qty=f[5].toInt(&ok); s.date=QDate::fromString(f[6], Qt::ISODate);
                    s.subtotal=f[7].toDouble(&ok); s.discountPercent=f[8].toDouble(&ok);
                    s.discountAmount=f[9].toDouble(&ok); s.vat=f[10].toDouble(&ok);
                    if (f.size()>11) s.total=f[11].toDouble(&ok);
                    else s.total=s.subtotal-s.discountAmount+s.vat;
                } else ok=false;
                if (ok && s.carId>0 && s.qty>0 && s.date.isValid()) sales.push_back(s);
            }
        }
    }

    bool saveData(bool showMessage=true) {
        QSaveFile cf(dataPath("cars.txt"));
        if (!cf.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(this,"Save error","Cannot save cars.txt: "+cf.errorString()); return false; }
        QTextStream co(&cf);
        for (const Car &c: cars) co<<c.id<<'|'<<safeField(c.brand)<<'|'<<safeField(c.model)<<'|'<<c.year<<'|'<<QString::number(c.price,'f',2)<<'|'<<c.stock<<'\n';
        if (!cf.commit()) { QMessageBox::critical(this,"Save error","Could not safely finish saving cars.txt."); return false; }
        QSaveFile sf(dataPath("sales.txt"));
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(this,"Save error","Cannot save sales.txt: "+sf.errorString()); return false; }
        QTextStream so(&sf);
        for (const Sale &s: sales) so<<s.carId<<'|'<<safeField(s.carName)<<'|'<<safeField(s.customer)<<'|'<<safeField(s.phone)<<'|'<<safeField(s.address)<<'|'<<s.qty<<'|'<<s.date.toString(Qt::ISODate)<<'|'<<QString::number(s.subtotal,'f',2)<<'|'<<QString::number(s.discountPercent,'f',2)<<'|'<<QString::number(s.discountAmount,'f',2)<<'|'<<QString::number(s.vat,'f',2)<<'|'<<QString::number(s.total,'f',2)<<'\n';
        if (!sf.commit()) { QMessageBox::critical(this,"Save error","Could not safely finish saving sales.txt."); return false; }
        if (showMessage) statusBar()->showMessage("Data saved successfully",3000);
        return true;
    }

    void backupData() {
        QString dir=QFileDialog::getExistingDirectory(this,"Choose backup folder");
        if(dir.isEmpty()) return;
        QDir target(dir);
        for(const QString &name: {QString("cars.txt"),QString("sales.txt")}) {
            if(QFile::exists(target.filePath(name))) QFile::remove(target.filePath(name));
            if(QFile::exists(dataPath(name)) && !QFile::copy(dataPath(name),target.filePath(name))) {
                QMessageBox::critical(this,"Backup failed","Could not copy "+name); return;
            }
        }
        QMessageBox::information(this,"Backup complete","Your cars.txt and sales.txt files were backed up.");
    }
    void restoreData() {
        QString dir=QFileDialog::getExistingDirectory(this,"Choose backup folder");
        if(dir.isEmpty()) return;
        if(!QFile::exists(QDir(dir).filePath("cars.txt")) || !QFile::exists(QDir(dir).filePath("sales.txt"))) {
            QMessageBox::warning(this,"Invalid backup","The selected folder must contain both cars.txt and sales.txt."); return;
        }
        if(QMessageBox::question(this,"Restore backup","This will replace the current records. Continue?")!=QMessageBox::Yes) return;
        for(const QString &name: {QString("cars.txt"),QString("sales.txt")}) {
            QFile::remove(dataPath(name));
            if(!QFile::copy(QDir(dir).filePath(name),dataPath(name))) { QMessageBox::critical(this,"Restore failed","Could not restore "+name); return; }
        }
        loadData(); refreshAll(); QMessageBox::information(this,"Restore complete","Backup data restored.");
    }

    double inventoryValue() const { double n=0; for(const Car &c:cars)n+=c.price*c.stock; return n; }
    double totalSales() const { double n=0; for(const Sale &s:sales)n+=s.total; return n; }
    int totalStock() const { int n=0; for(const Car &c:cars)n+=c.stock; return n; }
    int lowStockCount() const { int n=0; for(const Car &c:cars)if(c.stock<=LOW_STOCK_LEVEL)++n; return n; }
    double currentMonthRevenue() const { double n=0; QDate now=QDate::currentDate(); for(const Sale&s:sales)if(s.date.year()==now.year()&&s.date.month()==now.month())n+=s.total; return n; }

    QPushButton *navButton(const QString &txt,int index) {
        auto *b=new QPushButton(txt); b->setObjectName("navButton"); b->setMinimumHeight(45); b->setCursor(Qt::PointingHandCursor);
        connect(b,&QPushButton::clicked,this,[this,index]{pages->setCurrentIndex(index); if(index==1)refreshInventory(); if(index==2)refreshSales(); if(index==3)refreshReports();});
        return b;
    }
    QFrame *statCard(const QString &title,QLabel **out,const QString &accent) {
        auto *f=new QFrame; f->setObjectName("statCard"); auto *v=new QVBoxLayout(f);
        auto *t=new QLabel(title); t->setObjectName("muted"); *out=new QLabel("0"); (*out)->setObjectName("statValue"); (*out)->setStyleSheet("color:"+accent);
        v->addWidget(t); v->addWidget(*out); return f;
    }
    void setupTable(QTableWidget *t) {
        t->setAlternatingRowColors(true); t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setSelectionMode(QAbstractItemView::SingleSelection); t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->verticalHeader()->setVisible(false); t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        t->setShowGrid(false);
    }
    QWidget *makeDashboard() {
        auto *p=new QWidget; auto *v=new QVBoxLayout(p); v->setContentsMargins(28,24,28,24); v->setSpacing(14);
        auto *h=new QLabel("Dashboard"); h->setObjectName("pageHeading"); v->addWidget(h);
        auto *sub=new QLabel("Live overview of your dealership performance."); sub->setObjectName("muted"); v->addWidget(sub);
        auto *g=new QGridLayout; g->setSpacing(12);
        g->addWidget(statCard("CAR MODELS",&modelsValue,"#2563EB"),0,0);
        g->addWidget(statCard("UNITS IN STOCK",&stockValue,"#0F766E"),0,1);
        g->addWidget(statCard("INVENTORY VALUE",&inventoryValueLabel,"#7C3AED"),0,2);
        g->addWidget(statCard("TOTAL SALES",&salesValue,"#1D4ED8"),0,3);
        v->addLayout(g);
        auto *g2=new QGridLayout;
        auto *low=new QFrame; low->setObjectName("panel"); auto *lv=new QVBoxLayout(low);
        auto *lt=new QLabel("LOW-STOCK WARNINGS"); lt->setObjectName("sectionTitle");
        lowStockValue=new QLabel; lowStockValue->setObjectName("warningValue"); lowStockList=new QLabel; lowStockList->setWordWrap(true); lowStockList->setObjectName("muted");
        lv->addWidget(lt); lv->addWidget(lowStockValue); lv->addWidget(lowStockList);
        auto *month=new QFrame; month->setObjectName("panel"); auto *mv=new QVBoxLayout(month);
        auto *mt=new QLabel("CURRENT MONTH REVENUE"); mt->setObjectName("sectionTitle");
        monthlyRevenueValue=new QLabel; monthlyRevenueValue->setObjectName("statValue"); monthlyRevenueValue->setStyleSheet("color:#2563EB");
        mv->addWidget(mt); mv->addWidget(monthlyRevenueValue); mv->addWidget(new QLabel(QDate::currentDate().toString("MMMM yyyy")));
        g2->addWidget(low,0,0); g2->addWidget(month,0,1); v->addLayout(g2);
        auto *recentTitle=new QLabel("Recent sales activity"); recentTitle->setObjectName("sectionTitle"); v->addWidget(recentTitle);
        recentTable=new QTableWidget(0,5); recentTable->setHorizontalHeaderLabels({"Date","Customer","Vehicle","Qty","Total"}); setupTable(recentTable); v->addWidget(recentTable,1);
        return p;
    }
    QWidget *makeInventory() {
        auto *p=new QWidget; auto *v=new QVBoxLayout(p); v->setContentsMargins(28,24,28,24); v->setSpacing(12);
        auto *h=new QLabel("Car inventory"); h->setObjectName("pageHeading"); v->addWidget(h);
        auto *bar=new QHBoxLayout; searchBox=new QLineEdit; searchBox->setPlaceholderText("Search by ID, brand, or model..."); searchBox->setClearButtonEnabled(true);
        availabilityBox=new QComboBox; availabilityBox->addItems({"All availability","In stock","Low stock","Out of stock"});
        sortBox=new QComboBox; sortBox->addItems({"Default order","Price: low to high","Price: high to low","Year: newest first","Brand: A-Z","Stock: low to high","Stock: high to low"});
        bar->addWidget(searchBox,2); bar->addWidget(availabilityBox); bar->addWidget(sortBox);
        auto *add=new QPushButton("+ Add vehicle"); add->setObjectName("primaryButton");
        auto *edit=new QPushButton("Edit"); auto *del=new QPushButton("Delete"); del->setObjectName("dangerButton"); auto *sell=new QPushButton("Sell selected"); sell->setObjectName("secondaryButton");
        bar->addWidget(add); bar->addWidget(edit); bar->addWidget(del); bar->addWidget(sell); v->addLayout(bar);
        inventoryTable=new QTableWidget(0,7); inventoryTable->setHorizontalHeaderLabels({"ID","Brand","Model","Year","Price","Stock","Status"}); setupTable(inventoryTable); v->addWidget(inventoryTable,1);
        connect(searchBox,&QLineEdit::textChanged,this,&MainWindow::refreshInventory);
        connect(availabilityBox,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){refreshInventory();});
        connect(sortBox,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){refreshInventory();});
        connect(add,&QPushButton::clicked,this,&MainWindow::addCar); connect(edit,&QPushButton::clicked,this,&MainWindow::editCar);
        connect(del,&QPushButton::clicked,this,&MainWindow::deleteCar); connect(sell,&QPushButton::clicked,this,&MainWindow::sellSelectedCar);
        connect(inventoryTable,&QTableWidget::cellDoubleClicked,this,[this](int,int){editCar();}); return p;
    }
    QWidget *makeSales() {
        auto *p=new QWidget; auto *v=new QVBoxLayout(p); v->setContentsMargins(28,24,28,24); v->setSpacing(12);
        auto *h=new QLabel("Sales history"); h->setObjectName("pageHeading"); v->addWidget(h);
        auto *bar=new QHBoxLayout; bar->addStretch();
        auto *reset=new QPushButton("Reset sales history"); reset->setObjectName("dangerButton");
        auto *record=new QPushButton("+ Record a sale"); record->setObjectName("primaryButton");
        bar->addWidget(reset); bar->addWidget(record); v->addLayout(bar);
        salesTable=new QTableWidget(0,8); salesTable->setHorizontalHeaderLabels({"Date","Car ID","Vehicle","Customer","Contact","Qty","Discount","Total"}); setupTable(salesTable); v->addWidget(salesTable,1);
        auto *hint=new QLabel("Select a transaction and use 'View / Print receipt' to print a customer copy."); hint->setObjectName("muted"); v->addWidget(hint);
        auto *print=new QPushButton("View / Print receipt"); print->setObjectName("secondaryButton"); v->addWidget(print,0,Qt::AlignRight);
        connect(record,&QPushButton::clicked,this,&MainWindow::sellCar);
        connect(reset,&QPushButton::clicked,this,&MainWindow::resetSalesHistory);
        connect(print,&QPushButton::clicked,this,&MainWindow::printSelectedReceipt); return p;
    }
    QWidget *makeReports() {
        auto *p=new QWidget; auto *v=new QVBoxLayout(p); v->setContentsMargins(28,24,28,24); v->setSpacing(14);
        auto *h=new QLabel("Reports & data management"); h->setObjectName("pageHeading"); v->addWidget(h);
        auto *panel=new QFrame; panel->setObjectName("panel"); auto *pv=new QVBoxLayout(panel);
        auto *title=new QLabel("Inventory & sales statistics"); title->setObjectName("sectionTitle"); pv->addWidget(title);
        reportSummary=new QLabel; reportSummary->setWordWrap(true); reportSummary->setObjectName("reportSummary"); pv->addWidget(reportSummary); v->addWidget(panel);
        auto *monthlyTitle=new QLabel("Monthly revenue overview"); monthlyTitle->setObjectName("sectionTitle"); v->addWidget(monthlyTitle);
        monthlyTable=new QTableWidget(0,4); monthlyTable->setHorizontalHeaderLabels({"Month","Transactions","Units sold","Revenue"}); setupTable(monthlyTable); v->addWidget(monthlyTable,1);
        auto *actions=new QHBoxLayout; auto *backup=new QPushButton("Back up data"); backup->setObjectName("secondaryButton");
        auto *restore=new QPushButton("Restore backup"); restore->setObjectName("secondaryButton");
        auto *save=new QPushButton("Save data now"); save->setObjectName("primaryButton");
        actions->addWidget(backup); actions->addWidget(restore); actions->addWidget(save); actions->addStretch(); v->addLayout(actions);
        connect(backup,&QPushButton::clicked,this,&MainWindow::backupData); connect(restore,&QPushButton::clicked,this,&MainWindow::restoreData);
        connect(save,&QPushButton::clicked,this,[this]{saveData();}); return p;
    }
    void buildUi() {
        auto *central=new QWidget; setCentralWidget(central); auto *root=new QHBoxLayout(central); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
        auto *sidebar=new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(225); auto *s=new QVBoxLayout(sidebar); s->setContentsMargins(16,22,16,18);
        auto *brand=new QLabel("AUTO<span style='color:#2563EB'>DEALER</span>"); brand->setTextFormat(Qt::RichText); brand->setObjectName("brand");
        auto *cap=new QLabel("MANAGEMENT SYSTEM"); cap->setObjectName("sideCaption"); s->addWidget(brand); s->addWidget(cap); s->addSpacing(22);
        s->addWidget(navButton("▦   Dashboard",0)); s->addWidget(navButton("▤   Car inventory",1)); s->addWidget(navButton("₱   Sales history",2)); s->addWidget(navButton("▥   Reports & data",3)); s->addStretch();
        auto *foot=new QLabel("GROUP 03 PROJECT\nC++ / Qt 6"); foot->setObjectName("sideCaption"); s->addWidget(foot);
        pages=new QStackedWidget; pages->addWidget(makeDashboard()); pages->addWidget(makeInventory()); pages->addWidget(makeSales()); pages->addWidget(makeReports());
        root->addWidget(sidebar); root->addWidget(pages,1);
        setStyleSheet(R"(
            QWidget { background:#F4F7FC; color:#172554; font-family:'Segoe UI'; font-size:13px; }
            #sidebar { background:#FFFFFF; border-right:1px solid #E2E8F0; }
            #brand { font-size:22px; font-weight:800; color:#1E3A8A; }
            #sideCaption,#muted { color:#64748B; }
            #sideCaption { font-size:10px; font-weight:700; letter-spacing:1px; }
            #navButton { text-align:left; padding:12px; color:#475569; background:transparent; border:0; border-radius:8px; font-weight:600; }
            #navButton:hover { background:#EFF6FF; color:#2563EB; }
            #pageHeading { font-size:27px; font-weight:800; color:#172554; }
            #statCard,#panel { background:#FFFFFF; border:1px solid #E2E8F0; border-radius:12px; }
            #statCard { padding:12px; min-height:88px; }
            #statValue { font-size:24px; font-weight:800; }
            #sectionTitle { font-size:15px; font-weight:700; color:#172554; }
            #warningValue { color:#DC2626; font-size:22px; font-weight:800; }
            QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox { background:#FFFFFF; border:1px solid #CBD5E1; border-radius:7px; padding:8px; color:#172554; }
            QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus { border:1px solid #2563EB; }
            QTableWidget { background:#FFFFFF; alternate-background-color:#F8FAFC; border:1px solid #E2E8F0; border-radius:9px; selection-background-color:#DBEAFE; selection-color:#1E3A8A; }
            QHeaderView::section { background:#EFF6FF; color:#1E40AF; padding:10px; border:0; font-weight:700; }
            QTableWidget::item { padding:8px; border:0; }
            QPushButton { padding:9px 13px; border-radius:7px; font-weight:700; background:#FFFFFF; border:1px solid #CBD5E1; }
            #primaryButton { background:#2563EB; color:white; border:0; }
            #primaryButton:hover { background:#1D4ED8; }
            #secondaryButton { background:#EFF6FF; color:#1D4ED8; border:1px solid #BFDBFE; }
            #dangerButton { background:#FEE2E2; color:#B91C1C; border:1px solid #FECACA; }
            #salePreview { background:#EFF6FF; border:1px solid #BFDBFE; border-radius:8px; padding:12px; color:#1E3A8A; }
            #reportSummary { color:#334155; font-size:14px; }
            QStatusBar { background:#FFFFFF; color:#64748B; border-top:1px solid #E2E8F0; }
        )");
    }

    void refreshAll() {
        modelsValue->setText(QString::number(cars.size())); stockValue->setText(QString::number(totalStock()));
        inventoryValueLabel->setText(money(inventoryValue())); salesValue->setText(money(totalSales()));
        monthlyRevenueValue->setText(money(currentMonthRevenue()));
        lowStockValue->setText(QString("%1 model(s)").arg(lowStockCount()));
        QStringList alerts; for(const Car&c:cars)if(c.stock<=LOW_STOCK_LEVEL)alerts<<QString("%1 %2: %3 left").arg(c.brand,c.model).arg(c.stock);
        lowStockList->setText(alerts.isEmpty()?"No low-stock vehicles. All stock levels are healthy.":alerts.join("\n"));
        refreshInventory(); refreshSales(); refreshReports(); refreshRecentSales();
    }
    void refreshInventory() {
        if(!inventoryTable)return;
        QVector<Car> list=cars; QString key=searchBox?searchBox->text().trimmed().toLower():"";
        int filter=availabilityBox?availabilityBox->currentIndex():0;
        list.erase(std::remove_if(list.begin(),list.end(),[&](const Car&c){
            QString text=QString("%1 %2 %3").arg(c.id).arg(c.brand,c.model).toLower();
            if(!text.contains(key))return true;
            if(filter==1&&c.stock<=0)return true; if(filter==2&&(c.stock<=0||c.stock>LOW_STOCK_LEVEL))return true;
            if(filter==3&&c.stock!=0)return true; return false;
        }),list.end());
        int sort=sortBox?sortBox->currentIndex():0;
        if(sort==1)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.price<b.price;});
        if(sort==2)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.price>b.price;});
        if(sort==3)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.year>b.year;});
        if(sort==4)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.brand.toLower()<b.brand.toLower();});
        if(sort==5)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.stock<b.stock;});
        if(sort==6)std::sort(list.begin(),list.end(),[](const Car&a,const Car&b){return a.stock>b.stock;});
        inventoryTable->setRowCount(0);
        for(const Car&c:list){int r=inventoryTable->rowCount();inventoryTable->insertRow(r);
            QString status=c.stock==0?"OUT OF STOCK":(c.stock<=LOW_STOCK_LEVEL?"LOW STOCK":"IN STOCK");
            QStringList vals={QString::number(c.id),c.brand,c.model,QString::number(c.year),money(c.price),QString::number(c.stock),status};
            for(int col=0;col<7;++col){auto*it=new QTableWidgetItem(vals[col]);if(col==6){it->setForeground(c.stock==0?QColor("#DC2626"):(c.stock<=LOW_STOCK_LEVEL?QColor("#D97706"):QColor("#15803D")));it->setFont(QFont("Segoe UI",9,QFont::Bold));}inventoryTable->setItem(r,col,it);}
        }
    }
    void refreshSales() {
        if(!salesTable)return; salesTable->setRowCount(0);
        for(int i=sales.size()-1;i>=0;--i){const Sale&s=sales[i];int r=salesTable->rowCount();salesTable->insertRow(r);
            QStringList vals={s.date.toString("yyyy-MM-dd"),QString::number(s.carId),s.carName,s.customer,s.phone,QString::number(s.qty),QString("%1%").arg(s.discountPercent,0,'f',2),money(s.total)};
            for(int c=0;c<vals.size();++c)salesTable->setItem(r,c,new QTableWidgetItem(vals[c]));
        }
    }
    void refreshRecentSales() {
        if(!recentTable)return; recentTable->setRowCount(0);
        for(int i=sales.size()-1;i>=0&&recentTable->rowCount()<6;--i){const Sale&s=sales[i];int r=recentTable->rowCount();recentTable->insertRow(r);
            QStringList vals={s.date.toString("MMM d, yyyy"),s.customer,s.carName,QString::number(s.qty),money(s.total)};
            for(int c=0;c<vals.size();++c)recentTable->setItem(r,c,new QTableWidgetItem(vals[c]));
        }
    }
    void refreshReports() {
        if(!reportSummary||!monthlyTable)return;
        QString summary=QString("Vehicle models: %1\nUnits currently in stock: %2\nTotal inventory value: %3\nLifetime sales revenue: %4\nLow-stock models: %5\nTransactions recorded: %6")
            .arg(cars.size()).arg(totalStock()).arg(money(inventoryValue())).arg(money(totalSales())).arg(lowStockCount()).arg(sales.size());
        reportSummary->setText(summary);
        double rev[12]={};int units[12]={},transactions[12]={};
        for(const Sale&s:sales){int m=s.date.month()-1;rev[m]+=s.total;units[m]+=s.qty;transactions[m]++;}
        monthlyTable->setRowCount(0);
        for(int m=0;m<12;++m){if(!transactions[m])continue;int r=monthlyTable->rowCount();monthlyTable->insertRow(r);
            monthlyTable->setItem(r,0,new QTableWidgetItem(MONTHS[m]));monthlyTable->setItem(r,1,new QTableWidgetItem(QString::number(transactions[m])));
            monthlyTable->setItem(r,2,new QTableWidgetItem(QString::number(units[m])));monthlyTable->setItem(r,3,new QTableWidgetItem(money(rev[m])));
        }
    }
    int selectedCarId()const{if(!inventoryTable||inventoryTable->currentRow()<0)return -1;auto*i=inventoryTable->item(inventoryTable->currentRow(),0);return i?i->text().toInt():-1;}
    int carIndex(int id)const{for(int i=0;i<cars.size();++i)if(cars[i].id==id)return i;return -1;}
    void addCar(){if(cars.size()>=MAX_CARS){QMessageBox::warning(this,"Inventory full","Maximum of 100 models reached.");return;}
        CarDialog d(this);if(d.exec()!=QDialog::Accepted)return;Car c=d.result();c.id=nextCarId++;cars.push_back(c);if(saveData())refreshAll();}
    void editCar(){int idx=carIndex(selectedCarId());if(idx<0){QMessageBox::information(this,"Select vehicle","Select a vehicle row first.");return;}
        CarDialog d(this,&cars[idx]);if(d.exec()!=QDialog::Accepted)return;Car c=d.result();c.id=cars[idx].id;cars[idx]=c;if(saveData())refreshAll();}
    void deleteCar(){int idx=carIndex(selectedCarId());if(idx<0){QMessageBox::information(this,"Select vehicle","Select a vehicle row first.");return;}
        Car c=cars[idx];if(QMessageBox::question(this,"Delete vehicle",QString("Delete %1 %2 (ID %3)?").arg(c.brand,c.model).arg(c.id))!=QMessageBox::Yes)return;
        cars.removeAt(idx);if(saveData())refreshAll();}
    void recordSale(int idx){if(idx<0||idx>=cars.size())return;if(sales.size()>=MAX_SALES){QMessageBox::warning(this,"Sales limit","Maximum of 500 transactions reached.");return;}
        if(cars[idx].stock<=0){QMessageBox::warning(this,"Out of stock","This vehicle is out of stock.");return;}
        SaleDialog d(this,cars[idx]);if(d.exec()!=QDialog::Accepted)return;Sale s=d.result();
        if(s.qty>cars[idx].stock){QMessageBox::warning(this,"Insufficient stock","Quantity exceeds available stock.");return;}
        cars[idx].stock-=s.qty;sales.push_back(s);
        if(!saveData()) { sales.removeLast(); cars[idx].stock+=s.qty; return; }
        refreshAll(); QMessageBox::information(this,"Sale recorded","Transaction saved successfully.");
        if(QMessageBox::question(this,"Receipt","Would you like to view and print the receipt now?")==QMessageBox::Yes)showReceipt(s,true);
    }
    void sellSelectedCar(){int idx=carIndex(selectedCarId());if(idx<0){QMessageBox::information(this,"Select vehicle","Select a vehicle row first.");return;}recordSale(idx);}
    void sellCar(){if(cars.isEmpty()){QMessageBox::information(this,"No inventory","Add a vehicle first.");return;}
        QStringList choices;for(const Car&c:cars)choices<<QString("%1 | %2 %3 | Stock: %4").arg(c.id).arg(c.brand,c.model).arg(c.stock);
        bool ok=false;QString choice=QInputDialog::getItem(this,"Select vehicle","Vehicle to sell:",choices,0,false,&ok);if(ok){int i=choices.indexOf(choice);if(i>=0)recordSale(i);}}
    QString receiptHtml(const Sale&s)const{
        return QString("<html><body style='font-family:Segoe UI;color:#172554'><h1 style='color:#1d4ed8'>AUTO DEALER</h1><h2>SALES RECEIPT</h2><p><b>Date:</b> %1<br><b>Transaction:</b> %2<br><b>Customer:</b> %3<br><b>Contact:</b> %4<br><b>Address:</b> %5</p><hr><p><b>Vehicle:</b> %6<br><b>Car ID:</b> %7<br><b>Quantity:</b> %8</p><hr><table width='100%'><tr><td>Subtotal</td><td align='right'>%9</td></tr><tr><td>Discount (%10%)</td><td align='right'>-%11</td></tr><tr><td>VAT (12%)</td><td align='right'>%12</td></tr><tr><td><b>TOTAL</b></td><td align='right'><b>%13</b></td></tr></table><hr><p>Thank you for your purchase!</p></body></html>")
        .arg(s.date.toString("MMMM d, yyyy")).arg(s.carId).arg(s.customer.toHtmlEscaped()).arg(s.phone.toHtmlEscaped()).arg(s.address.toHtmlEscaped())
        .arg(s.carName.toHtmlEscaped()).arg(s.carId).arg(s.qty).arg(money(s.subtotal)).arg(s.discountPercent,0,'f',2).arg(money(s.discountAmount)).arg(money(s.vat)).arg(money(s.total));
    }
    void showReceipt(const Sale&s,bool printNow){
        QTextDocument doc;doc.setHtml(receiptHtml(s));
        if(!printNow){QMessageBox box(this);box.setWindowTitle("Receipt");box.setTextFormat(Qt::RichText);box.setText(receiptHtml(s));box.exec();return;}
        QPrinter printer(QPrinter::HighResolution);QPrintDialog dialog(&printer,this);dialog.setWindowTitle("Print sales receipt");
        if(dialog.exec()==QDialog::Accepted)doc.print(&printer);
    }
    void resetSalesHistory(){
        if(sales.isEmpty()){
            QMessageBox::information(this,"Sales history is empty","There are no sales records to reset.");
            return;
        }
        const auto answer=QMessageBox::warning(this,"Reset sales history",
            "This will permanently clear all recorded sales transactions.\n\n"
            "Your car inventory and current stock quantities will remain unchanged.\n"
            "A backup of the current sales history will be created first.\n\n"
            "Do you want to continue?",
            QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
        if(answer!=QMessageBox::Yes)return;

        const QString backupName=QString("sales_backup_before_reset_%1.txt")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        const QString source=dataPath("sales.txt");
        if(QFile::exists(source)&&!QFile::copy(source,dataPath(backupName))){
            QMessageBox::critical(this,"Backup failed",
                "The sales history could not be backed up, so nothing was reset.");
            return;
        }

        const QVector<Sale> previousSales=sales;
        sales.clear();
        if(!saveData(false)){
            sales=previousSales;
            refreshAll();
            return;
        }
        refreshAll();
        statusBar()->showMessage("Sales history reset. Inventory was preserved. Backup: "+backupName,6000);
        QMessageBox::information(this,"Reset complete",
            "Sales history has been cleared successfully.\n"
            "Your car inventory and stock quantities were not changed.\n\n"
            "Backup saved as: "+backupName);
    }
    void printSelectedReceipt(){if(!salesTable||salesTable->currentRow()<0){QMessageBox::information(this,"Select transaction","Select a sales record first.");return;}
        int reverseIndex=sales.size()-1-salesTable->currentRow();if(reverseIndex>=0&&reverseIndex<sales.size())showReceipt(sales[reverseIndex],true);}
};

int main(int argc,char *argv[]){
    QApplication app(argc,argv);
    QApplication::setApplicationName("GROUP 03 Car Dealership");
    MainWindow window;window.show();return app.exec();
}
