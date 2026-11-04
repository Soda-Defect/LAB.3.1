#pragma once

#include "Workspace.h"
#include <QMainWindow>
#include <functional>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QSpinBox;
class QTreeWidget;
class QGraphicsView;
class QGraphicsScene;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    const lab::Workspace& workspace() const { return workspace_; }
    // Эти методы также позволяют проверить файловый обмен без модальных диалогов.
    void saveToPath(const QString& path);
    void loadFromPath(const QString& path);

private:
    lab::Workspace workspace_;
    QComboBox *type_, *active_, *map_, *filter_, *storage_;
    QLineEdit *value_, *path_, *relative_, *order_, *format_, *parameter_;
    QSpinBox *slot_, *count_, *minimum_, *maximum_, *seed_;
    QPlainTextEdit *text_, *log_;
    QLabel *summary_, *hint_, *notice_;
    QTreeWidget* tree_;
    QGraphicsView* view_;
    QGraphicsScene* scene_;

    void buildUi();
    void updateTypeControls();
    void refresh();
    void drawTree(const std::deque<lab::ViewNode>& nodes);
    void run(const std::function<std::string()>& action, bool redraw = true);
    void report(const QString& message, bool error = false);
    lab::Storage storageMode() const;
    void saveFile();
    void openFile();
};
