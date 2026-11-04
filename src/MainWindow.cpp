#include "MainWindow.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <array>
#include <cmath>

namespace {
QString q(const std::string& s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
std::string s(const QString& qtext) { return qtext.toUtf8().toStdString(); }
QLineEdit* edit(const char* name, const QString& initial = {}) {
    auto* e = new QLineEdit(initial); e->setObjectName(name); return e;
}
QWidget* row(std::initializer_list<QWidget*> children) {
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget); layout->setContentsMargins(0,0,0,0);
    for (auto* child : children) layout->addWidget(child);
    return widget;
}
class NodeItem : public QGraphicsEllipseItem {
public:
    std::function<void()> selected;
    using QGraphicsEllipseItem::QGraphicsEllipseItem;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (selected) selected();
        QGraphicsEllipseItem::mousePressEvent(event);
    }
};
class TreeGraphicsView : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0/1.15;
            const double next = transform().m11()*factor;
            if (next >= 0.05 && next <= 4.0) scale(factor, factor);
            event->accept();
        } else QGraphicsView::wheelEvent(event);
    }
};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("ЛР 1 · 3-арное дерево"));
    resize(1440, 920);
    buildUi(); updateTypeControls(); refresh();
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    auto* title = new QLabel(QStringLiteral("3-арное дерево"));
    QFont font = title->font(); font.setPointSize(20); font.setBold(true); title->setFont(font);
    type_ = new QComboBox; type_->setObjectName("typeCombo");
    type_->addItems({"Целые числа", "Вещественные числа", "Комплексные числа", "Строки", "Функции", "Студенты", "Преподаватели"});
    active_ = new QComboBox; active_->setObjectName("activeCombo");
    active_->addItems({"Дерево A", "Дерево B"});
    root->addWidget(row({title, new QLabel("Тип:"), type_, new QLabel("Активно:"), active_}));
    auto* help = new QLabel("При смене типа оба дерева очищаются. Результаты Map, Where и извлечения заменяют второе дерево.");
    help->setWordWrap(true); root->addWidget(help);
    summary_ = new QLabel; summary_->setObjectName("summaryLabel"); root->addWidget(summary_);

    auto* splitter = new QSplitter;
    auto* controls = new QTabWidget; controls->setMinimumWidth(390); controls->setMaximumWidth(540);
    auto* operations = new QWidget; auto* op = new QVBoxLayout(operations);
    hint_ = new QLabel; hint_->setWordWrap(true); op->addWidget(hint_);
    auto* fields = new QFormLayout;
    value_ = edit("valueEdit"); path_ = edit("pathEdit"); relative_ = edit("relativeEdit");
    path_->setPlaceholderText("Пусто — корень; например 12");
    relative_->setPlaceholderText("1, 2, 3 — потомок; ^ — родитель");
    slot_ = new QSpinBox; slot_->setObjectName("slotSpin"); slot_->setRange(1,3);
    fields->addRow("Значение:", value_); fields->addRow("Абсолютный путь:", path_);
    fields->addRow("Номер потомка:", slot_); fields->addRow("Относительный путь:", relative_);
    op->addLayout(fields);
    auto button = [&](const QString& label, const char* name, QVBoxLayout* layout, auto callback) {
        auto* b = new QPushButton(label); b->setObjectName(name); layout->addWidget(b);
        connect(b, &QPushButton::clicked, this, callback); return b;
    };
    button("Создать корень", "rootButton", op, [this] { run([&] { workspace_.setRoot(s(value_->text())); return "Корень создан."; }); });
    button("Вставить потомка по пути", "insertButton", op, [this] { run([&] { workspace_.insert(s(path_->text()), slot_->value(), s(value_->text())); return "Потомок добавлен."; }); });
    button("Изменить значение по пути", "setButton", op, [this] { run([&] { workspace_.setValue(s(path_->text()), s(value_->text())); return "Значение изменено."; }); });
    button("Найти значение", "findButton", op, [this] { run([&] { return workspace_.contains(s(value_->text())) ? "Значение найдено." : "Значение не найдено."; }, false); });
    button("Прочитать абсолютный путь", "atButton", op, [this] { run([&] { return "По пути: " + workspace_.at(s(path_->text())); }, false); });
    button("Прочитать относительный путь", "relativeButton", op, [this] { run([&] { return "По относительному пути: " + workspace_.at(s(path_->text()), s(relative_->text()), true); }, false); });
    auto* deletion = new QLabel("Удаление узла также удаляет всех его потомков."); deletion->setWordWrap(true); op->addWidget(deletion);
    button("Удалить поддерево по пути", "removePathButton", op, [this] { run([&] { return "Удалено узлов: " + std::to_string(workspace_.removeSubtree(s(path_->text()))); }); });
    button("Удалить первое вхождение значения", "removeValueButton", op, [this] { run([&] { return workspace_.removeValue(s(value_->text())) ? "Узел и его потомки удалены." : "Значение не найдено."; }); });
    button("Очистить активное дерево", "clearButton", op, [this] { run([&] { workspace_.clear(); return "Активное дерево очищено."; }); });
    op->addStretch();
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(operations);
    controls->addTab(scroll, "Узлы");

    auto* functions = new QWidget; auto* fn = new QVBoxLayout(functions);
    auto* secondary = new QLabel("Операции используют активное дерево как источник. «Второе» означает B при активном A и A при активном B.");
    secondary->setWordWrap(true); fn->addWidget(secondary);
    button("Копировать во второе дерево", "copyButton", fn, [this] { run([&] { workspace_.copyToOther(); return "Копия записана во второе дерево."; }); });
    button("Поменять A и B местами", "swapButton", fn, [this] { run([&] { workspace_.swapTrees(); return "Деревья обменены."; }); });
    button("Извлечь поддерево по пути → второе", "extractPathButton", fn, [this] { run([&] { workspace_.extract(s(path_->text()), false); return "Поддерево скопировано во второе дерево."; }); });
    button("Извлечь поддерево по значению → второе", "extractValueButton", fn, [this] { run([&] { workspace_.extract(s(value_->text()), true); return "Поддерево скопировано во второе дерево."; }); });
    button("Присоединить второе к пути и слоту", "mergeButton", fn, [this] { run([&] { workspace_.merge(s(path_->text()), slot_->value()); return "Копия второго дерева присоединена."; }); });
    button("Проверить вхождение второго дерева", "subtreeButton", fn, [this] { run([&] { return workspace_.containsOther() ? "Второе дерево входит как поддерево." : "Поддерево не найдено."; }, false); });
    button("Сравнить A и B", "equalButton", fn, [this] { run([&] { return workspace_.equal() ? "Деревья равны." : "Деревья различаются."; }, false); });
    auto* transform = new QFormLayout;
    map_ = new QComboBox; map_->setObjectName("mapCombo"); filter_ = new QComboBox; filter_->setObjectName("filterCombo");
    parameter_ = edit("parameterEdit", "1");
    transform->addRow("Map:", map_); transform->addRow("Where:", filter_); transform->addRow("Параметр:", parameter_); fn->addLayout(transform);
    button("Map → второе дерево", "mapButton", fn, [this] { run([&] { workspace_.mapToOther(map_->currentIndex(), s(parameter_->text())); return "Результат Map во втором дереве."; }); });
    button("Where → второе дерево", "filterButton", fn, [this] { run([&] { workspace_.filterToOther(filter_->currentIndex(), s(parameter_->text())); return "Результат Where во втором дереве; выбранные значения упакованы по уровням."; }); });
    auto* reduceHelp = new QLabel("Reduce: сумма чисел / ID; для строк — соединение через параметр; для функций — сумма f(x), где x задан параметром. Порядок берётся из вкладки «Строка».");
    reduceHelp->setWordWrap(true); fn->addWidget(reduceHelp);
    button("Вычислить Reduce", "reduceButton", fn, [this] { run([&] { return "Reduce = " + workspace_.reduce(s(parameter_->text()), s(order_->text())); }, false); });
    fn->addStretch();
    auto* fnScroll = new QScrollArea; fnScroll->setWidgetResizable(true); fnScroll->setWidget(functions);
    controls->addTab(fnScroll, "Операции");

    auto* serialization = new QWidget; auto* ser = new QVBoxLayout(serialization);
    storage_ = new QComboBox; storage_->setObjectName("storageCombo");
    storage_->addItems({"Фиксированный обход", "Строка форматирования", "Список узел–родитель"});
    order_ = edit("orderEdit", "K123"); format_ = edit("formatEdit", "{K}(1)[2]{3}");
    auto* serialFields = new QFormLayout; serialFields->addRow("Режим:", storage_); serialFields->addRow("Порядок:", order_); serialFields->addRow("Формат:", format_); ser->addLayout(serialFields);
    auto* spec = new QLabel("Порядок: перестановка латинской K и цифр 1, 2, 3. Формат: те же поля (допустима К), разделители произвольные. # — пустое поддерево; значение записано как длина_в_байтах:значение.");
    spec->setWordWrap(true); ser->addWidget(spec);
    text_ = new QPlainTextEdit; text_->setObjectName("serializationEdit"); text_->setPlaceholderText("Здесь появится строка дерева. Можно вставить свою."); ser->addWidget(text_, 1);
    button("Сохранить дерево в строку", "serializeButton", ser, [this] { run([&] { text_->setPlainText(q(workspace_.serialize(storageMode(), s(order_->text()), s(format_->text())))); return "Строка подготовлена."; }, false); });
    button("Прочитать строку в активное дерево", "deserializeButton", ser, [this] { run([&] { workspace_.deserialize(s(text_->toPlainText()), storageMode(), s(order_->text()), s(format_->text())); return "Дерево прочитано из строки."; }); });
    button("Показать обход", "traverseButton", ser, [this] { run([&] { return workspace_.traverse(s(order_->text())); }, false); });
    button("Сохранить дерево в файл…", "saveButton", ser, [this] { saveFile(); });
    button("Открыть дерево из файла…", "openButton", ser, [this] { openFile(); });
    controls->addTab(serialization, "Строка");

    auto* generation = new QWidget; auto* gen = new QVBoxLayout(generation);
    auto* genHelp = new QLabel("Генератор заменяет активное дерево. Узлы располагаются по уровням. Для вещественных чисел границы делятся на 10; для комплексных формируется (n/10,n/20); для функций выбирается имя из каталога.");
    genHelp->setWordWrap(true); gen->addWidget(genHelp);
    auto* genFields = new QFormLayout;
    count_ = new QSpinBox; count_->setObjectName("countSpin"); count_->setRange(0,100000); count_->setValue(13);
    minimum_ = new QSpinBox; minimum_->setObjectName("minimumSpin"); minimum_->setRange(-1000000,1000000); minimum_->setValue(0);
    maximum_ = new QSpinBox; maximum_->setObjectName("maximumSpin"); maximum_->setRange(-1000000,1000000); maximum_->setValue(99);
    seed_ = new QSpinBox; seed_->setObjectName("seedSpin"); seed_->setRange(0,2147483647); seed_->setValue(42);
    genFields->addRow("Количество:", count_); genFields->addRow("Минимум n:", minimum_); genFields->addRow("Максимум n:", maximum_); genFields->addRow("Seed:", seed_); gen->addLayout(genFields);
    button("Сгенерировать дерево", "generateButton", gen, [this] { run([&] { workspace_.generate(static_cast<std::size_t>(count_->value()), minimum_->value(), maximum_->value(), static_cast<unsigned>(seed_->value())); return "Дерево сгенерировано."; }); });
    gen->addStretch(); controls->addTab(generation, "Ввод"); splitter->addWidget(controls);

    auto* right = new QWidget; auto* rightLayout = new QVBoxLayout(right);
    notice_ = new QLabel; notice_->setWordWrap(true); rightLayout->addWidget(notice_);
    auto* displays = new QTabWidget; displays->setObjectName("displayTabs");
    scene_ = new QGraphicsScene(this);
    view_ = new TreeGraphicsView(scene_); view_->setObjectName("graphicsView"); view_->setRenderHint(QPainter::Antialiasing);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setDragMode(QGraphicsView::ScrollHandDrag); displays->addTab(view_, "Схема");
    tree_ = new QTreeWidget; tree_->setObjectName("treeWidget"); tree_->setHeaderLabels({"Значение", "Путь", "Потомок"});
    tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents); displays->addTab(tree_, "Структура");
    rightLayout->addWidget(displays, 1);
    auto* fit = new QPushButton("Уместить схему"); connect(fit, &QPushButton::clicked, this, [this] { view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio); }); rightLayout->addWidget(fit);
    log_ = new QPlainTextEdit; log_->setObjectName("logEdit"); log_->setReadOnly(true); log_->setMaximumBlockCount(500); log_->setMaximumHeight(180);
    rightLayout->addWidget(new QLabel("Результаты и ошибки")); rightLayout->addWidget(log_); splitter->addWidget(right);
    splitter->setStretchFactor(0,0); splitter->setStretchFactor(1,1);
    root->addWidget(splitter, 1); setCentralWidget(central);
    connect(type_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        run([&] { workspace_.setType(static_cast<lab::DataType>(index)); QSignalBlocker block(active_); active_->setCurrentIndex(0); updateTypeControls(); return "Тип изменён; оба дерева очищены."; });
    });
    connect(active_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        workspace_.setActive(index); path_->clear(); refresh();
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        path_->setText(item->data(0, Qt::UserRole).toString());
        value_->setText(item->data(0, Qt::UserRole + 1).toString());
    });
}

void MainWindow::updateTypeControls() {
    hint_->setText("Значение: " + q(workspace_.hint())); value_->setPlaceholderText(q(workspace_.hint())); value_->clear();
    map_->clear(); filter_->clear();
    for (const auto& label : workspace_.mapNames()) map_->addItem(q(label));
    for (const auto& label : workspace_.filterNames()) filter_->addItem(q(label));
    parameter_->setText(workspace_.type() == lab::DataType::String ? "" : workspace_.type() == lab::DataType::Function ? "inc1" : "1");
    path_->clear(); relative_->clear(); text_->clear();
}
void MainWindow::refresh() {
    summary_->setText(QString("Активно %1 · узлов: %2 · во втором дереве: %3")
        .arg(workspace_.active() == 0 ? "A" : "B").arg(static_cast<qulonglong>(workspace_.size())).arg(static_cast<qulonglong>(workspace_.otherSize())));
    const auto nodes = workspace_.snapshot(400);
    notice_->setText(workspace_.size() > 200 ? "Схема: первые 200 узлов; структура: первые 400. Операции работают со всем деревом. Ctrl+колесо — масштаб." : "Нажмите на узел, чтобы выбрать его путь. Рёбра: потомки 1–3. Ctrl+колесо — масштаб.");
    tree_->clear(); std::deque<QTreeWidgetItem*> items;
    for (const auto& n : nodes) {
        auto* item = n.parent ? new QTreeWidgetItem(items.at(n.parent-1)) : new QTreeWidgetItem(tree_);
        QString display = q(n.value); if (display.size() > 100) display = display.left(100) + "…";
        item->setText(0, display); item->setText(1, n.path.empty() ? "корень" : q(n.path)); item->setText(2, n.parent ? QString::number(n.slot) : "—");
        item->setData(0, Qt::UserRole, q(n.path)); item->setData(0, Qt::UserRole+1, q(n.value)); item->setToolTip(0, q(n.value)); items.push_back(item);
    }
    tree_->expandToDepth(3); drawTree(nodes);
}
void MainWindow::drawTree(const std::deque<lab::ViewNode>& nodes) {
    scene_->clear(); const std::size_t count = std::min<std::size_t>(nodes.size(), 200);
    if (!count) { scene_->addText("Пустое дерево. Создайте корень или используйте генератор."); scene_->setSceneRect(scene_->itemsBoundingRect()); return; }
    std::deque<std::array<int,3>> children(count, std::array<int,3>{{-1,-1,-1}});
    std::deque<double> xs(count, 0); double leaf = 0;
    for (std::size_t i = 1; i < count; ++i) children[nodes[i].parent-1][nodes[i].slot-1] = static_cast<int>(i);
    for (std::size_t i = count; i-- > 0;) {
        double sum = 0; int n = 0;
        for (int child : children[i]) if (child >= 0) { sum += xs[static_cast<std::size_t>(child)]; ++n; }
        xs[i] = n ? sum / n : leaf++ * 155;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& n = nodes[i]; const double x = -xs[i], y = static_cast<double>(n.path.size()) * 105;
        if (n.parent) {
            const std::size_t p = n.parent-1;
            const double px = -xs[p], py = static_cast<double>(nodes[p].path.size()) * 105;
            scene_->addLine(px+60, py+48, x+60, y, QPen(QColor("#8a98ac"), 1.5));
            auto* label = scene_->addSimpleText(QString::number(n.slot));
            QFont edgeFont = label->font(); edgeFont.setPointSize(12); label->setFont(edgeFont);
            label->setPos((px+x)/2+65, (py+y)/2);
        }
        auto* circle = new NodeItem(0, 0, 120, 48); circle->setPos(x,y);
        circle->setBrush(QColor(i == 0 ? "#d8e8ff" : "#eef3f9")); circle->setPen(QPen(QColor("#376399"), 1.5));
        circle->setToolTip(q(n.value) + "\nПуть: " + (n.path.empty() ? QString("корень") : q(n.path)));
        const QString selectedPath = q(n.path), selectedValue = q(n.value);
        circle->selected = [this, selectedPath, selectedValue] { path_->setText(selectedPath); value_->setText(selectedValue); };
        scene_->addItem(circle);
        QString label = q(n.value); if (label.size() > 14) label = label.left(13) + "…";
        label.replace('\n', ' ');
        auto* caption = new QGraphicsSimpleTextItem(label, circle); caption->setAcceptedMouseButtons(Qt::NoButton);
        QFont nodeFont = caption->font(); nodeFont.setPointSize(12); nodeFont.setBold(true); caption->setFont(nodeFont);
        caption->setPos((120-caption->boundingRect().width())/2, (48-caption->boundingRect().height())/2);
    }
    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-30,-30,30,30));
    view_->resetTransform();
    // Не уменьшаем крупные деревья до нечитаемого масштаба: доступны прокрутка и кнопка уместить.
    if (count <= 40) {
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
        if (view_->transform().m11() < 0.6) {
            view_->resetTransform(); view_->scale(0.6,0.6); view_->centerOn(-xs[0]+60, 50);
        }
    } else view_->centerOn(-xs[0]+60, 50);
}
void MainWindow::run(const std::function<std::string()>& action, bool redraw) {
    try { const std::string result = action(); if (redraw) refresh(); report(q(result)); }
    catch (const std::exception& error) { report(q(error.what()), true); }
}
void MainWindow::report(const QString& message, bool error) {
    log_->appendPlainText((error ? "Ошибка: " : "") + message); statusBar()->showMessage(message, 10000);
}
lab::Storage MainWindow::storageMode() const { return static_cast<lab::Storage>(storage_->currentIndex()); }

void MainWindow::saveFile() {
    const QString path = QFileDialog::getSaveFileName(this, "Сохранить дерево", "tree.ttree.json", "Дерево (*.ttree.json)");
    if (path.isEmpty()) return;
    run([&] { saveToPath(path); return "Дерево и параметры формата сохранены."; }, false);
}
void MainWindow::saveToPath(const QString& path) {
    const QString data = q(workspace_.serialize(storageMode(), s(order_->text()), s(format_->text())));
    QJsonObject document{{"version",1}, {"type",static_cast<int>(workspace_.type())}, {"mode",storage_->currentIndex()},
                         {"order",order_->text()}, {"format",format_->text()}, {"data",data}};
    QSaveFile file(path); const QByteArray bytes = QJsonDocument(document).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error(s(file.errorString()));
    text_->setPlainText(data);
}
void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Открыть дерево", {}, "Дерево (*.ttree.json);;Все файлы (*)");
    if (path.isEmpty()) return;
    run([&] { loadFromPath(path); return "Дерево загружено из файла."; }, false);
}
void MainWindow::loadFromPath(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(s(file.errorString()));
    if (file.size() > 16 * 1024 * 1024) throw std::invalid_argument("Для интерфейса допустим файл до 16 МиБ.");
    QJsonParseError error; const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) throw std::invalid_argument("Некорректный JSON-файл дерева.");
    const QJsonObject obj = doc.object();
    const int type = obj.value("type").toInt(-1), mode = obj.value("mode").toInt(-1);
    if (obj.value("version").toInt() != 1 || type < 0 || type > 6 || mode < 0 || mode > 2 ||
        !obj.value("data").isString() || !obj.value("order").isString() || !obj.value("format").isString())
        throw std::invalid_argument("Неподдерживаемый формат файла.");
    lab::Workspace candidate = workspace_;
    if (static_cast<int>(candidate.type()) != type) candidate.setType(static_cast<lab::DataType>(type));
    candidate.deserialize(s(obj.value("data").toString()), static_cast<lab::Storage>(mode), s(obj.value("order").toString()), s(obj.value("format").toString()));
    workspace_ = std::move(candidate);
    QSignalBlocker blockType(type_), blockActive(active_);
    type_->setCurrentIndex(type); active_->setCurrentIndex(workspace_.active()); updateTypeControls(); storage_->setCurrentIndex(mode);
    order_->setText(obj.value("order").toString()); format_->setText(obj.value("format").toString()); text_->setPlainText(obj.value("data").toString());
    refresh();
}
