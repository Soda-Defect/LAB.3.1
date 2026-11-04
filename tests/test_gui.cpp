#include "MainWindow.h"
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTreeWidget>

class GuiTests : public QObject {
    Q_OBJECT
    template<class Widget> Widget* get(MainWindow& w, const char* name) {
        auto* widget=w.findChild<Widget*>(name);
        if (!widget) qFatal("Widget not found: %s",name);
        return widget;
    }
    void enter(MainWindow& w,const char* name,const QString& value) {get<QLineEdit>(w,name)->setText(value);}
    void click(MainWindow& w,const char* name) {get<QPushButton>(w,name)->click();}
    void root(MainWindow& w,const QString& value) {enter(w,"valueEdit",value);click(w,"rootButton");}

private slots:
    void createsAndInserts() {
        MainWindow w; root(w,"10"); enter(w,"valueEdit","20");
        get<QSpinBox>(w,"slotSpin")->setValue(2); click(w,"insertButton");
        QCOMPARE(w.workspace().size(),std::size_t(2)); QCOMPARE(QString::fromStdString(w.workspace().at("2")),QString("20"));
        QCOMPARE(get<QTreeWidget>(w,"treeWidget")->topLevelItemCount(),1);
        QCOMPARE(get<QTreeWidget>(w,"treeWidget")->topLevelItem(0)->childCount(),1);
    }
    void invalidInputReportsErrorWithoutMutation() {
        MainWindow w; root(w,"10"); enter(w,"valueEdit","bad"); click(w,"insertButton");
        QCOMPARE(w.workspace().size(),std::size_t(1));
        QVERIFY(get<QPlainTextEdit>(w,"logEdit")->toPlainText().contains("Ошибка:"));
    }
    void occupiedSlotIsRejected() {
        MainWindow w; root(w,"1"); enter(w,"valueEdit","2");click(w,"insertButton");click(w,"insertButton");
        QCOMPARE(w.workspace().size(),std::size_t(2)); QVERIFY(get<QPlainTextEdit>(w,"logEdit")->toPlainText().contains("Ошибка:"));
    }
    void mapsToSecondTree() {
        MainWindow w; root(w,"5");get<QComboBox>(w,"mapCombo")->setCurrentIndex(2); enter(w,"parameterEdit","3");click(w,"mapButton");
        QCOMPARE(w.workspace().otherSize(),std::size_t(1)); QCOMPARE(QString::fromStdString(w.workspace().at("")),QString("5"));
        get<QComboBox>(w,"activeCombo")->setCurrentIndex(1); QCOMPARE(QString::fromStdString(w.workspace().at("")),QString("15"));
    }
    void serializedTextCanRestoreTree() {
        MainWindow w; root(w,"8");click(w,"serializeButton");
        const QString encoded=get<QPlainTextEdit>(w,"serializationEdit")->toPlainText(); QVERIFY(!encoded.isEmpty());
        click(w,"clearButton");QVERIFY(w.workspace().size()==0);click(w,"deserializeButton");QCOMPARE(w.workspace().size(),std::size_t(1));
    }
    void badSerializedTextPreservesTree() {
        MainWindow w; root(w,"8");get<QPlainTextEdit>(w,"serializationEdit")->setPlainText("broken");click(w,"deserializeButton");
        QCOMPARE(QString::fromStdString(w.workspace().at("")),QString("8"));
    }
    void unicodeStringCanBeEdited() {
        MainWindow w;get<QComboBox>(w,"typeCombo")->setCurrentIndex(3); root(w,QString::fromUtf8("Привет"));
        QCOMPARE(QString::fromStdString(w.workspace().at("")),QString::fromUtf8("Привет"));
        click(w,"serializeButton");click(w,"clearButton");click(w,"deserializeButton");QCOMPARE(w.workspace().size(),std::size_t(1));
    }
    void relativeReadAndSubtreeRemoval() {
        MainWindow w;root(w,"1");enter(w,"valueEdit","2");click(w,"insertButton");
        enter(w,"pathEdit","1");enter(w,"relativeEdit","^");click(w,"relativeButton");
        QVERIFY(get<QPlainTextEdit>(w,"logEdit")->toPlainText().contains("По относительному пути: 1"));
        click(w,"removePathButton");QCOMPARE(w.workspace().size(),std::size_t(1));
    }
    void allStorageModesRoundtrip() {
        for(int mode=0;mode<3;++mode) {
            MainWindow w;root(w,"11");get<QComboBox>(w,"storageCombo")->setCurrentIndex(mode);
            click(w,"serializeButton");click(w,"clearButton");click(w,"deserializeButton");QCOMPARE(w.workspace().size(),std::size_t(1));
        }
    }
    void allTypesCanCreateRoot() {
        const QStringList values={"1","2.5","(1,2)","строка","square","1 | Иван | 2000-01-01","2 | Анна | 1980-01-01 | Математика"};
        for(int i=0;i<values.size();++i){MainWindow w;get<QComboBox>(w,"typeCombo")->setCurrentIndex(i);root(w,values[i]);QCOMPARE(w.workspace().size(),std::size_t(1));}
    }
    void generatorAndTypeReset() {
        MainWindow w;get<QSpinBox>(w,"countSpin")->setValue(31);click(w,"generateButton");QCOMPARE(w.workspace().size(),std::size_t(31));
        click(w,"copyButton");get<QComboBox>(w,"typeCombo")->setCurrentIndex(1);QCOMPARE(w.workspace().size(),std::size_t(0));QCOMPARE(w.workspace().otherSize(),std::size_t(0));
    }
    void selectionFillsPathAndValue() {
        MainWindow w;w.show();root(w,"1");enter(w,"valueEdit","7");get<QSpinBox>(w,"slotSpin")->setValue(3);click(w,"insertButton");
        get<QTabWidget>(w,"displayTabs")->setCurrentIndex(1);
        auto* tree=get<QTreeWidget>(w,"treeWidget");auto* child=tree->topLevelItem(0)->child(0);
        tree->expandAll();QApplication::processEvents();
        const QRect rect=tree->visualItemRect(child);QVERIFY(!rect.isEmpty());
        QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());
        QCOMPARE(get<QLineEdit>(w,"pathEdit")->text(),QString("3"));QCOMPARE(get<QLineEdit>(w,"valueEdit")->text(),QString("7"));
        QVERIFY(!w.grab().isNull());
    }
    void filePreservesTypeFormatAndUnicode() {
        QTemporaryDir directory;QVERIFY(directory.isValid());
        MainWindow source;get<QComboBox>(source,"typeCombo")->setCurrentIndex(3);root(source,"текст # { } Привет");
        get<QComboBox>(source,"storageCombo")->setCurrentIndex(1);
        source.saveToPath(directory.filePath("tree.ttree.json"));
        MainWindow restored;root(restored,"42");restored.loadFromPath(directory.filePath("tree.ttree.json"));
        QCOMPARE(static_cast<int>(restored.workspace().type()),3);
        QCOMPARE(QString::fromStdString(restored.workspace().at("")),QString("текст # { } Привет"));
        QCOMPARE(get<QComboBox>(restored,"storageCombo")->currentIndex(),1);
    }
    void badFileKeepsBothTrees() {
        QTemporaryDir directory;QVERIFY(directory.isValid());
        const QString path=directory.filePath("bad.ttree.json");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"version\":1,\"type\":3,\"mode\":0,\"order\":\"K123\",\"format\":\"K123\",\"data\":\"broken\"}");file.close();
        MainWindow w;root(w,"7");click(w,"copyButton");
        QVERIFY_EXCEPTION_THROWN(w.loadFromPath(path),std::invalid_argument);
        QCOMPARE(static_cast<int>(w.workspace().type()),0);QVERIFY(w.workspace().equal());QCOMPARE(w.workspace().size(),std::size_t(1));
    }
};
QTEST_MAIN(GuiTests)
#include "test_gui.moc"
