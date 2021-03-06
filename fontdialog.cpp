#include "fontdialog.h"
#include "ui_fontdialog.h"

FontDialog::FontDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FontDialog)
{
    ui->setupUi(this);

    buttonGroup = new QButtonGroup();
    buttonGroup->addButton(ui->drawInPointButton);
    buttonGroup->addButton(ui->drawOutPointButton);
    buttonGroup->addButton(ui->drawLimitsButton);

    contextMenu = new QMenu();
    contextMenu->addAction(tr("Delete"));
    contextMenu->addAction(tr("Copy to choosen symbol"));

    symbolsFileDialog = new QFileDialog(0, tr("Choose"), "",
                                        tr("SVG") +
                                           "(*.svg);;" +
                                        tr("All Files") +
                                           "(*.*)");

    connect(ui->choosenSymbolTextEdit, SIGNAL(textChanged()),
            this, SLOT(limitTextEdit()));
    connect(ui->fontFilePushButton, SIGNAL(clicked()),
            this, SLOT(loadFont()));
    connect(ui->SymbolFilesPushButton, SIGNAL(clicked()),
            this, SLOT(addNewSymbols()));
    connect(ui->autoLoadPushButton, SIGNAL(clicked()),
            this, SLOT(autoLoadSymbols()));
    connect(ui->buttonBox, SIGNAL(accepted()),
            this, SLOT(saveFont()));
    connect(ui->buttonBox, SIGNAL(rejected()),
            this, SLOT(rejectChanges()));
    connect(ui->treeWidget, SIGNAL(itemClicked(QTreeWidgetItem*,int)),
            this, SLOT(setTextFromItem(QTreeWidgetItem*)));
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showTreeWidgetContextMenu(QPoint)));
    connect(contextMenu->actions()[ContextAction::Delete], SIGNAL(triggered(bool)),
            this, SLOT(deleteItem()));
    connect(contextMenu->actions()[ContextAction::Copy], SIGNAL(triggered(bool)),
            this, SLOT(copyToChoosenSymbol()));
    connect(ui->drawInPointButton, SIGNAL(toggled(bool)),
            ui->symbolDataEditor, SLOT(enableInPointChanges()));
    connect(ui->drawOutPointButton, SIGNAL(toggled(bool)),
            ui->symbolDataEditor, SLOT(enableOutPointChanges()));
    connect(ui->drawLimitsButton, SIGNAL(toggled(bool)),
            ui->symbolDataEditor, SLOT(enableLimitsChanges()));
    connect(symbolsFileDialog, SIGNAL(directoryEntered(QString)),
            this, SLOT(resetSymbolsFileDialog(QString)));

    ui->drawInPointButton->setShortcut(Qt::AltModifier + Qt::Key_1);
    ui->drawOutPointButton->setShortcut(Qt::AltModifier + Qt::Key_2);
    ui->drawLimitsButton->setShortcut(Qt::AltModifier + Qt::Key_3);

    ui->drawInPointButton->setCheckable(true);
    ui->drawOutPointButton->setCheckable(true);
    ui->drawLimitsButton->setCheckable(true);

    ui->drawInPointButton->setIcon(QIcon("://dark_cyan_dot.png"));
    ui->drawOutPointButton->setIcon(QIcon("://dark_magnetta_dot.png"));
    ui->drawLimitsButton->setIcon(QIcon("://border.png"));

    ui->drawInPointButton->setToolTip(tr("In Point"));
    ui->drawOutPointButton->setToolTip(tr("Out Point"));
    ui->drawLimitsButton->setToolTip(tr("Limits"));

    ui->treeWidget->setColumnCount(1);
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeWidget->setSortingEnabled(true);
    ui->treeWidget->sortByColumn(0, Qt::AscendingOrder);

    ui->splitter->setSizes(QList <int> () << 200 << 350);
    contextMenu->actions()[ContextAction::Copy]->setEnabled(false);
    ui->fontFileTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    lastItem = nullptr;

    setWindowFlags(Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
}

FontDialog::~FontDialog()
{
    delete ui;
    delete buttonGroup;
    delete contextMenu;
    delete symbolsFileDialog;
}

void FontDialog::loadFont()
{
    QString newFileName = QFileDialog::getSaveFileName(0, tr("Choose"), "",
                                                          tr("INI") +
                                                          "(*.ini);;" +
                                                          tr("All Files") +
                                                          "(*.*)",
                                                       0, QFileDialog::DontConfirmOverwrite);
    if (newFileName.isEmpty())
        return;
    else
        fontFileName = newFileName;

    lastItem = nullptr;
    ui->fontFileTextEdit->setText(fontFileName);
    ui->autoLoadPushButton->setEnabled(true);

    QSettings fontSettings(fontFileName, QSettings::IniFormat);
    fontSettings.beginGroup("Font");
    fontSettings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    if (fontSettings.allKeys().size() == 0)
    {
        fontSettings.endGroup();
        return;
    }

    font.clear();
    for (const QString &key : fontSettings.childKeys())
        for (const SymbolData &value : fontSettings.value(key).value<QList<SymbolData>>())
        {
            if (key == "slash")
                font.insert('/', value);
            else if (key == "backslash")
                font.insert('\\', value);
            else
                font.insert(key.at(0).toLower(), value);
        }

    //It's a dirty hack, which helps to distinguish uppercase and lowercase
    //letters on a freaking case-insensetive Windows
    fontSettings.beginGroup("UpperCase");
    for (const QString &key : fontSettings.childKeys())
        for (const SymbolData &value : fontSettings.value(key).value<QList<SymbolData>>())
            font.insert(key.at(0).toUpper(), value);
    fontSettings.endGroup();

    fontSettings.endGroup();

    ui->treeWidget->clear();

    for (QChar key : font.uniqueKeys())
    {
        QTreeWidgetItem *symbolItem = getSymbolItem(key);
        for (const SymbolData &data : font.values(key))
        {
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(symbolItem, QStringList(data.fileName));
            symbolItem->addChild(fileItem);
        }
    }

    ui->symbolDataEditor->clear();
    enableDrawButtons(false);
}

void FontDialog::addNewSymbols()
{
    symbolsFileDialog->setDirectory(fontFileName);
    symbolsFileDialog->exec();
    QStringList files = symbolsFileDialog->selectedFiles();

    if (files.isEmpty())
        return;

    QChar key = ui->choosenSymbolTextEdit->toPlainText().at(0);

    if (font.contains(key))
        for (int i = 0; i < files.count(); i++)
            for (int j = 0; j < font.values(key).count(); j++)
                if (QFileInfo(files.at(i)).fileName() == font.values(key).at(j).fileName)
                {
                    files.removeAt(i);
                    i--;
                    break;
                }

    QTreeWidgetItem *symbolItem = getSymbolItem(key);

    for (QString fileName : files)
    {
        SymbolData symbolData = { QFileInfo(fileName).fileName(),
                        QPointF(),
                        QPointF(),
                        QRectF()};
        font.insert(key, symbolData);
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(symbolItem, QStringList(symbolData.fileName));
        symbolItem->addChild(fileItem);
        ui->treeWidget->setCurrentItem(fileItem);
        setTextFromItem(fileItem);
    }
}

void FontDialog::saveFont()
{
    if (fontFileName.isEmpty())
        return;

    QFile file (fontFileName);
    file.remove();

    QSettings fontSettings(fontFileName, QSettings::IniFormat);
    fontSettings.beginGroup("Font");
    fontSettings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    loadFromEditorToFont();

    for (QChar &key : font.uniqueKeys())
        if (key.isUpper())
        {
            fontSettings.beginGroup("UpperCase");
            fontSettings.remove(key);
            fontSettings.setValue(key, QVariant::fromValue(font.values(key)));
            fontSettings.endGroup();
        }
        else
        {
            if (key == '/')
            {
                fontSettings.remove("slash");
                fontSettings.setValue("slash", QVariant::fromValue(font.values(key)));
                continue;
            }
            if (key == '\\')
            {
                fontSettings.remove("backslash");
                fontSettings.setValue("backslash", QVariant::fromValue(font.values(key)));
                continue;
            }
            fontSettings.remove(key);
            fontSettings.setValue(key, QVariant::fromValue(font.values(key)));
        }

    fontSettings.endGroup();

    emit fontReady();
}

void FontDialog::rejectChanges()
{
    font.clear();
    fontFileName.clear();
    ui->SymbolFilesPushButton->setEnabled(false);
    ui->autoLoadPushButton->setEnabled(false);
    ui->choosenSymbolTextEdit->clear();
    ui->fontFileTextEdit->clear();
    ui->treeWidget->clear();
    resetSymbolDataEditor();
}

void FontDialog::limitTextEdit()
{
    QString text = ui->choosenSymbolTextEdit->toPlainText();

    if (!fontFileName.isEmpty())
    {
        ui->SymbolFilesPushButton->setEnabled(!text.isEmpty());
        contextMenu->actions()[ContextAction::Copy]->setEnabled(!text.isEmpty());
    }

    if (text.length() > 1)
    {
        text = text.left(1);
        ui->choosenSymbolTextEdit->setText(text);
    }
}

void FontDialog::setTextFromItem(QTreeWidgetItem *item)
{
    loadFromEditorToFont();

    if (isSymbolItem(item) || isCategoryItem(item))
    {
        if (isSymbolItem(item))
            ui->choosenSymbolTextEdit->setText(item->text(0));

        resetSymbolDataEditor();
    }
    else
    {
        QString fileName = QFileInfo(fontFileName).path() + '/' + item->text(0);

        if (!QFileInfo(fileName).isReadable())
        {
            resetSymbolDataEditor();
            showError();
            return;
        }

        enableDrawButtons(true, item->parent()->text(0).at(0).isLetter());
        ui->choosenSymbolTextEdit->setText(item->parent()->text(0));
        ui->symbolDataEditor->load(fileName);
        QList<SymbolData> dataList = font.values(item->parent()->text(0).at(0));

        for (const SymbolData &data : dataList)
            if (data.fileName == item->text(0))
            {
                ui->symbolDataEditor->setSymbolData(data.inPoint, data.outPoint, data.limits);
                break;
            }

        lastItem = item;

        if (!item->parent()->text(0).at(0).isLetter())
            ui->symbolDataEditor->disablePoints();
    }
}

void FontDialog::loadFromEditorToFont()
{
    if (lastItem != nullptr && isFileItem(lastItem))
    {
        SymbolData newData;
        newData.fileName = lastItem->text(0);
        newData.inPoint = ui->symbolDataEditor->getInPoint();
        newData.outPoint = ui->symbolDataEditor->getOutPoint();
        newData.limits = ui->symbolDataEditor->getLimits();
        QChar key = lastItem->parent()->text(0).at(0);
        QList<SymbolData> dataList = font.values(key);

        for (SymbolData &data : dataList)
            if (data.fileName == lastItem->text(0))
            {
                data = newData;
                break;
            }

        font.remove(key);

        for (const SymbolData &data : dataList)
            font.insert(key, data);
    }
}

void FontDialog::deleteItem()
{
    QTreeWidgetItem *selectedItem = ui->treeWidget->itemAt(ui->treeWidget->mapFromGlobal(contextMenu->pos()));

    QChar key = isSymbolItem(selectedItem) ? selectedItem->text(0).at(0) : selectedItem->parent()->text(0).at(0) ;
    QTreeWidgetItem *symbolItem = getSymbolItem(key);
    QTreeWidgetItem *categoryItem = symbolItem->parent();

    if (isFileItem(selectedItem))
    {
        QList<SymbolData> dataList = font.values(key);

        for (const SymbolData &symbolData : dataList)
            if (symbolData.fileName == selectedItem->text(0))
            {
                font.remove(key, symbolData);
                delete selectedItem;
                break;
            }

        if (symbolItem->childCount() == 0)
            delete symbolItem;
    }
    else
    {
        for (QTreeWidgetItem *item : selectedItem->takeChildren())
            delete item;

        delete selectedItem;
        font.remove(key);
    }

    if (categoryItem->childCount() == 0)
        delete categoryItem;

    resetSymbolDataEditor();
}

void FontDialog::enableDrawButtons(bool enable, bool isLetter)
{
    if (!enable && buttonGroup->checkedButton() != nullptr)
    {
        buttonGroup->setExclusive(false);
        buttonGroup->checkedButton()->setChecked(false);
        buttonGroup->setExclusive(true);
    }

    ui->drawInPointButton->setEnabled(enable && isLetter);
    ui->drawOutPointButton->setEnabled(enable && isLetter);
    ui->drawLimitsButton->setEnabled(enable);
}

void FontDialog::showTreeWidgetContextMenu(QPoint pos)
{
    if (ui->treeWidget->itemAt(pos) == nullptr)
        return;

    if (ui->treeWidget->itemAt(pos)->parent() == nullptr)
    {
        contextMenu->actions()[ContextAction::Copy]->setEnabled(false);
        contextMenu->actions()[ContextAction::Delete]->setEnabled(false);
    }
    else
    {
        contextMenu->actions()[ContextAction::Copy]->setEnabled(!ui->choosenSymbolTextEdit->toPlainText().isEmpty());
        contextMenu->actions()[ContextAction::Delete]->setEnabled(true);
    }

    contextMenu->exec(QCursor::pos());
}

void FontDialog::copyToChoosenSymbol()
{
    if (ui->choosenSymbolTextEdit->toPlainText().isEmpty())
        return;

    QTreeWidgetItem *selectedItem = ui->treeWidget->selectedItems().at(0);

    QChar key = isSymbolItem(selectedItem) ? selectedItem->text(0).at(0) : selectedItem->parent()->text(0).at(0);
    QChar newKey = ui->choosenSymbolTextEdit->toPlainText().at(0);
    QTreeWidgetItem *symbolItem = getSymbolItem(newKey);

    if (!isSymbolItem(selectedItem))
    {
        for (SymbolData &symbolData : font.values(key))
            if (symbolData.fileName == selectedItem->text(0))
            {
                if (font.contains(newKey, symbolData))
                    return;

                font.insert(newKey, symbolData);
                QTreeWidgetItem *fileItem = new QTreeWidgetItem(symbolItem, QStringList(symbolData.fileName));
                symbolItem->addChild(fileItem);
                break;
            }
    }
    else
    {
        for (SymbolData &symbolData : font.values(key))
        {
            if (font.contains(newKey, symbolData))
                continue;

            font.insert(newKey, symbolData);
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(symbolItem, QStringList(symbolData.fileName));
            symbolItem->addChild(fileItem);
        }
    }
}

void FontDialog::autoLoadSymbols()
{
    symbolsFileDialog->setDirectory(fontFileName);
    symbolsFileDialog->exec();
    QStringList files = symbolsFileDialog->selectedFiles();

    if (files.isEmpty())
        return;

    QMap <QString, QChar> markNames;
    markNames.insert("dot", '.');
    markNames.insert("colon", ':');
    markNames.insert("semicolon", ';');
    markNames.insert("slash", '/');
    markNames.insert("backslash", '\\');
    markNames.insert("question", '?');
    markNames.insert("vertical", '|');
    markNames.insert("asterisk", '*');
    markNames.insert("less", '<');
    markNames.insert("greater", '>');
    markNames.insert("caret", '^');
    markNames.insert("quotes", '"');

    QRegularExpression upLetters("^UP_._?[0-9]*\\.svg");
    upLetters.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QRegularExpression symbols("^._?[0-9]*\\.svg");
    symbols.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    QString marks("_?[0-9]*\\.svg");

    for (QString fileName : files)
    {
        fileName = QFileInfo(fileName).fileName();
        QChar symbol;

        if (symbols.match(fileName).hasMatch())
            symbol = fileName.at(0).toLower();

        if (upLetters.match(fileName).hasMatch())
            if (fileName.at(3).isLetter())
                symbol = fileName.at(3).toUpper();

        for (QString name : markNames.keys())
            if (QRegularExpression(name + marks).match(fileName.toLower()).hasMatch())
            {
                 symbol = markNames[name];
                 break;
            }

        if (symbol.isNull())
            continue;

        if (font.contains(symbol))
        {
            bool fileExists = false;
            for (int j = 0; j < font.values(symbol).count(); j++)
                if (QFileInfo(fileName).fileName() == font.values(symbol).at(j).fileName)
                {
                    fileExists = true;
                    break;
                }

            if (fileExists)
                continue;
        }

        QTreeWidgetItem *symbolItem = getSymbolItem(symbol);

        SymbolData symbolData = { fileName,
                                 QPointF(),
                                 QPointF(),
                                 QRectF()};
        font.insert(symbol, symbolData);
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(symbolItem, QStringList(symbolData.fileName));
        symbolItem->addChild(fileItem);
        ui->treeWidget->setCurrentItem(fileItem);
        setTextFromItem(fileItem);
    }
}

QTreeWidgetItem * FontDialog::getSymbolItem(QChar key)
{
    QTreeWidgetItem *topLevelItem = nullptr;
    QTreeWidgetItem *categoryItem = getCategoryItem(key);

    for (int i = 0; i < categoryItem->childCount(); i++)
        if (categoryItem->child(i)->text(0).at(0) == key)
        {
            topLevelItem = categoryItem->child(i);
            break;
        }

    if (topLevelItem == nullptr)
    {
        topLevelItem = new QTreeWidgetItem(categoryItem, QStringList(QString(key)));
        categoryItem->addChild(topLevelItem);
    }

    return topLevelItem;
}

QTreeWidgetItem * FontDialog::getCategoryItem(QChar key)
{
    QTreeWidgetItem *categoryItem;
    QString category;

    category = tr("Other symbols");

    if (key.isNumber())
        category = tr("Numbers");

    if (key.isLetter())
        category = tr("Non-latin letters");

    if ((key >= 'a' && key <= 'z') ||
        (key >= 'A' && key <= 'Z'))
        category = tr("Latin letters");

    if (key.isMark())
        category = tr("Other marks");

    if (key.isPunct())
        category = tr("Punctuation marks");

    QList<QTreeWidgetItem *> itemList = ui->treeWidget->findItems(category, Qt::MatchExactly);

    if (itemList.isEmpty())
    {
        categoryItem = new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList(category));
        ui->treeWidget->insertTopLevelItem(ui->treeWidget->topLevelItemCount(), categoryItem);
    }
    else
        categoryItem = itemList.first();

    return categoryItem;
}

bool FontDialog::isSymbolItem(QTreeWidgetItem *item)
{
    if (isCategoryItem(item))
        return false;

   return item->childCount() > 0;
}

bool FontDialog::isCategoryItem(QTreeWidgetItem *item)
{
    if (item != nullptr)
        return item->parent() == nullptr;
    else
        return false;
}

bool FontDialog::isFileItem(QTreeWidgetItem *item)
{
    if (item != nullptr)
        return !(isCategoryItem(item) || isSymbolItem(item));
    else
        return false;

}

void FontDialog::showError()
{
    QMessageBox messageBox;
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setText(tr("Cannot read this file."));
    messageBox.exec();
}

void FontDialog::resetSymbolDataEditor()
{
    enableDrawButtons(false);
    ui->symbolDataEditor->disableChanges();
    ui->symbolDataEditor->clear();
    lastItem = nullptr;
}

void FontDialog::resetSymbolsFileDialog(const QString &directory)
{
    if (QFileInfo(fontFileName).absolutePath() == QDir(directory).absolutePath())
        return;

    symbolsFileDialog->setDirectory(fontFileName);

    QMessageBox messageBox;
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setText(tr("Symbols must be in the same folder as the font."));
    messageBox.exec();
}
