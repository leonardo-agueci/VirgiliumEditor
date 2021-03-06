#include "TextEditor.h"
#include "../../../cmake-build-debug/VirgiliumClient_autogen/include/ui_TextEditor.h"

#include <QComboBox>
#include <QFontComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextList>
#include <QToolButton>
#include <QColorDialog>
#include <QPrinter>
#include <utility>

TextEditor::TextEditor(QWidget *parent, ClientSocket *socket, const QString &fileName, User user,
                       PersonalPage *personalPage)
        : QMainWindow(parent), ui(new Ui::TextEditor) {

    ui->setupUi(this);
    this->setCentralWidget(ui->textEdit);
    this->setMinimumSize(900, 550);
    QRegExp tagExp("/");
    QStringList dataList = fileName.split(tagExp);
    this->setWindowTitle(dataList.at(1));
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/Icons/v.png"), QSize(), QIcon::Normal, QIcon::On);
    this->setWindowIcon(icon);
    this->fileName = fileName;
    this->currentUser = std::move(user);

    ui->textEdit->setStyleSheet("QTextEdit { padding-left:150; padding-right:150;}");

    QTextCursor cursor(ui->textEdit->textCursor());
    QTextBlockFormat textBlockFormat = cursor.block().blockFormat();
    alignment = QString::number(textBlockFormat.alignment());
    indentation = QString::number(textBlockFormat.indent());

    /* header has an horizontal layout, in which the first item is the imageLabel with the image, and the second
    item is the internalLayoutHeader, that contains user name, menubar and toolbar */
    auto *header = new QWidget();
    QLayout *hBox = new QHBoxLayout();
    header->setLayout(hBox);
    auto *internalLayoutHeader = new QWidget();
    internalLayoutHeader->setLayout(new QVBoxLayout());

    auto *imageLabel = new QLabel();
    imageLabel->setPixmap(QPixmap(":/Icons/file.png"));
    imageLabel->setFixedWidth(50);
    imageLabel->setFixedHeight(50);
    imageLabel->setScaledContents(true);

    /* image and internalLayout are inserted in the header */
    header->layout()->addWidget(imageLabel);
    header->layout()->addWidget(internalLayoutHeader);

    /* Here, internalLayoutHeader is populated */
    internalLayoutHeader->layout()->setContentsMargins(0, 0, 0, 0);
    auto *buttonsLayout = new QWidget();
    QLayout *btnLayout = new QHBoxLayout();
    btnLayout->setAlignment(Qt::AlignLeft);
    buttonsLayout->setLayout(btnLayout);

    comboUsers = new QComboBox();
    comboUsers->setObjectName("comboUsers");
    comboUsers->setEditable(false);
    comboUsers->setFixedWidth(250);
    buttonsLayout->layout()->addWidget(comboUsers);

    internalLayoutHeader->layout()->addWidget(buttonsLayout);
    internalLayoutHeader->layout()->addWidget(ui->menubar);
    internalLayoutHeader->layout()->addWidget(ui->toolBar);
    ui->menubar->setStyleSheet("background: #F0F0F0;");

    addToolBar(ui->toolBar);
    internalLayoutHeader->layout()->addWidget(ui->toolBar);
    this->layout()->setMenuBar(header);

    /* Font size comboBox */
    ui->toolBar->addSeparator();
    drawFontSizeComboBox();

    /* Used to leave some space between widgets */
    auto *empty = new QWidget();
    empty->setFixedSize(5, 5);
    ui->toolBar->addWidget(empty);

    /* Font comboBox and color menu */
    drawFontComboBox();
    drawColorButton();

    /* client instance is created and connections for editor management are inserted  */
    this->client = new Crdt_editor(nullptr, socket, this->fileName);
    QObject::connect(this->client, &Crdt_editor::insert_into_window, this, &TextEditor::insert_text);
    QObject::connect(this->client, &Crdt_editor::remove_into_window, this, &TextEditor::delete_text);
    QObject::connect(this->client, &Crdt_editor::change_block_format, this, &TextEditor::changeBlockFormat);
    QObject::connect(this->client, &Crdt_editor::change_char_format, this, &TextEditor::changeCharFormat);
    QObject::connect(this->client, &Crdt_editor::change_cursor_position, this, &TextEditor::changeCursorPosition);
    QObject::connect(ui->textEdit->document(), SIGNAL(contentsChange(int, int, int)), this,
                     SLOT(change(int, int, int)));
    QObject::connect(ui->textEdit, SIGNAL(cursorPositionChanged()), this, SLOT(cursorMoved()));
    QObject::connect(this->client, &Crdt_editor::load_response, this, &TextEditor::loadResponse);
    QObject::connect(this->client, &Crdt_editor::change_active_users, this, &TextEditor::changeActiveUser);
    QObject::connect(personalPage, &PersonalPage::closeEditor, this, &TextEditor::closeEditor);

    this->loadRequest(this->fileName, this->currentUser);
}

TextEditor::~TextEditor() {
    this->client->deleteFromActive(this->currentUser, this->fileName);
    delete client;
    delete ui;
}

void TextEditor::loadRequest(const QString &f, User user) {
    this->client->loadRequest(this->fileName, std::move(user));
}

void TextEditor::loadResponse(_int code, const QVector<Symbol> &symbols, const QList<User> &users) {
    if (code == LOAD_RESPONSE) {
        for (const Symbol &symbol : symbols) {
            //spdlog::debug("{0}, {1}, {2}", symbol.getLetter().toStdString(), symbols.indexOf(symbol), symbol.getFont().font.toStdString());
            insertOneChar(symbols.indexOf(symbol), symbol.getLetter(), symbol.getFont());
            if (symbols.indexOf(symbol) == 0)
                changeBlockFormat(symbol.getFont().font);
        }
    }

    changeActiveUser(users);
}

void TextEditor::changeActiveUser(const QList<User> &users) {
    if (users.size() < this->activeUsers.size()) {
        for (const User &user : this->activeUsers) {
            if (!users.contains(user)) {
                if (user.getLastCursorPos() != 0)
                    changeBackground(user.getLastCursorPos(), Qt::white);
            }
        }
    }

    this->activeUsers = users;
    this->comboUsers->clear();
    auto *model = dynamic_cast< QStandardItemModel * >( comboUsers->model());
    for (int i = 0; i < this->activeUsers.size(); i++) {
        QString str = this->activeUsers.at(i).getFirstName() + " " + this->activeUsers.at(i).getLastName();
        comboUsers->addItem(str);
        comboUsers->setItemData(i, QBrush(this->activeUsers.at(i).getAssignedColor()), Qt::TextColorRole);
        auto *item = model->item(i);
        item->setSelectable(false);
    }
}

void TextEditor::drawFontSizeComboBox() {
    /* The comboBox is created */
    auto *comboSize = new QComboBox(ui->toolBar);
    comboSize->setObjectName("comboSize");
    comboSize->setEditable(true);

    /* The item are added from a standard list of different size */
    const QList<int> standardSizes = QFontDatabase::standardSizes();
    for (int size : standardSizes) {
        comboSize->addItem(QString::number(size));
    }

    /* The default value is set and the comboBox is connected to the corresponding slot */
    comboSize->setCurrentIndex(6);
    ui->toolBar->addWidget(comboSize);
    connect(comboSize, SIGNAL(currentIndexChanged(QString)), SLOT(changeFontSize(QString)));
}

void TextEditor::changeFontSize(const QString &selected) {
    ui->textEdit->setFontPointSize(selected.toInt());
}

void TextEditor::drawFontComboBox() {
    /* The fontComboBox is created */
    myFont = new QFontComboBox;
    myFont->setEditable(false);
    ui->toolBar->addWidget(myFont);

    /* The comboBox is connected to a lambda function that change the font of the text */
    QObject::connect(myFont, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
                     [=](const QString &text) {
                         QTextCursor cursor(ui->textEdit->textCursor());
                         if (!cursor.hasSelection()) {
                             auto *qd = new QDialog;
                             qd->setWindowTitle("Change font");
                             QIcon icon;
                             icon.addFile(QString::fromUtf8(":/Icons/v.png"), QSize(), QIcon::Normal, QIcon::On);
                             qd->setWindowIcon(icon);
                             auto *vl = new QVBoxLayout;
                             qd->setLayout(vl);

                             auto label = new QLabel("Please, select the text that you want to change.");
                             auto *ok = new QPushButton("Ok");

                             vl->addWidget(label);
                             vl->addWidget(ok);

                             QObject::connect(ok, &QPushButton::clicked,
                                              [qd]() {
                                                  qd->close();
                                              });

                             qd->show();
                         } else {
                             ui->textEdit->setCurrentFont(text);
                         }
                     });
}

void TextEditor::drawColorButton() {
    /* General widget is created */
    auto *qw = new QWidget();
    auto *vl = new QVBoxLayout();
    vl->setSpacing(2);
    qw->setLayout(vl);

    /* Here, the button is designed */
    const QSize btnSize = QSize(15, 15);
    auto *b = new QPushButton();
    b->setFixedSize(btnSize);
    QPixmap pixmap(":/Icons/color.png");
    QIcon ButtonIcon1(pixmap);
    b->setIcon(ButtonIcon1);
    b->setIconSize(btnSize);
    QString buttonStyle = "QPushButton{border:none;}";
    b->setStyleSheet(buttonStyle);

    /* A label is inserted above the button in order to show the selected color */
    textColorLabel = new QLabel();
    QColor color = ui->textEdit->textColor();
    QString colorS = color.name();
    QString labelStyle = "QLabel{background-color: " + colorS + "}";
    textColorLabel->setStyleSheet(labelStyle);
    textColorLabel->setFixedWidth(15);
    textColorLabel->setFixedHeight(4);

    /* Button and Label are inserted in the widget layout */
    vl->addWidget(b);
    vl->addWidget(textColorLabel);

    /* widget is inserted on the toolbar */
    ui->toolBar->addWidget(qw);

    /* The button is connected to the slot that open the QColorDialog */
    QObject::connect(b, &QToolButton::clicked, [this]() { TextEditor::changeColorSlot(); });
}

void TextEditor::changeColorSlot() {
    /* The selected color is taken */
    QColor color = QColorDialog::getColor(Qt::yellow, this);

    /* If color is valid, text and label are changed */
    if (color.isValid()) {
        ui->textEdit->setTextColor(color.name());
        QString labelStyle = "QLabel{background-color: " + color.name() + "}";
        textColorLabel->setStyleSheet(labelStyle);
    }
}

void TextEditor::on_actionExit_triggered() {
    this->close();
}

void TextEditor::on_actionCopy_triggered() {
    ui->textEdit->copy();
}

void TextEditor::on_actionPaste_triggered() {
    ui->textEdit->paste();
}

void TextEditor::on_actionCut_triggered() {
    ui->textEdit->cut();
}

void TextEditor::on_actionSelect_all_triggered() {
    ui->textEdit->selectAll();
}

void TextEditor::on_actionUnderline_triggered() {
    QTextCursor cursor = ui->textEdit->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(cursor.selectionStart() + 1);
        ui->textEdit->setFontUnderline(!cursor.charFormat().fontUnderline());
    } else {
        ui->textEdit->setFontUnderline(!ui->textEdit->fontUnderline());
    }
}

void TextEditor::on_actionBold_triggered() {
    QTextCursor cursor = ui->textEdit->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(cursor.selectionStart() + 1);
        ui->textEdit->setFontWeight(cursor.charFormat().fontWeight() == 50 ? 75 : 50);
    } else {
        ui->textEdit->setFontWeight(ui->textEdit->fontWeight() == 50 ? 75 : 50);
    }
}

void TextEditor::on_actionItalic_triggered() {
    QTextCursor cursor = ui->textEdit->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(cursor.selectionStart() + 1);
        ui->textEdit->setFontItalic(!cursor.charFormat().fontItalic());
    } else {
        ui->textEdit->setFontItalic(!ui->textEdit->fontItalic());
    }
}

void TextEditor::on_actionFind_and_replace_triggered() {
    /* A dialog is created in order to take find and replace value */
    auto *qd = new QDialog;
    qd->setWindowTitle("Find and replace");
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/Icons/v.png"), QSize(), QIcon::Normal, QIcon::On);
    qd->setWindowIcon(icon);
    auto *vl = new QVBoxLayout;
    qd->setLayout(vl);
    auto *labelFind = new QLabel("Find");
    auto *labelReplace = new QLabel("Replace");
    auto *ok = new QPushButton("Search");

    vl->addWidget(labelFind);
    vl->addWidget(this->lineFind);
    vl->addWidget(labelReplace);
    vl->addWidget(this->lineReplace);
    vl->addWidget(ok);

    /* Inside the lambda function all occurrences of "find" are replaced with "replace" and QLineEdit are cleared */
    QObject::connect(ok, &QPushButton::clicked,
                     [this]() {
                         this->find = this->lineFind->text().toUtf8().data();
                         this->replace = this->lineReplace->text().toUtf8().data();

                         ui->textEdit->moveCursor(QTextCursor::Start);
                         QTextCursor cursor = ui->textEdit->textCursor();
                         while (ui->textEdit->find(find)) {
                             ui->textEdit->textCursor().insertText(replace);
                         }
                         this->lineFind->clear();
                         this->lineReplace->clear();
                     });

    qd->show();
}

void TextEditor::on_actionIncrease_indent_triggered() {
    changeIndentSpacing(this->indentation.toInt() + 1);
    QTextCursor cursor(ui->textEdit->textCursor());
    if (cursor.document()->isEmpty())
        sendBlockFormatChange();
}

void TextEditor::on_actionDecrease_indent_triggered() {
    QTextCursor cursor(ui->textEdit->textCursor());
    QTextBlockFormat blockFmt = cursor.blockFormat();
    if (blockFmt.indent() > 0)
        changeIndentSpacing(this->indentation.toInt() - 1);

    if (cursor.document()->isEmpty() && blockFmt.indent() > 0)
        sendBlockFormatChange();
}

void TextEditor::changeIndentSpacing(int num) {
    QTextCursor cursor(ui->textEdit->textCursor());

    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    ui->textEdit->setTextCursor(cursor);
    QTextBlockFormat blockFmt = cursor.blockFormat();
    blockFmt.setIndent(num);
    cursor.setBlockFormat(blockFmt);
    cursor.clearSelection();
    ui->textEdit->setTextCursor(cursor);
}

void TextEditor::on_actionExport_PDF_triggered() {
    QFileDialog fileDialog(this, tr("Export PDF"));
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setMimeTypeFilters(QStringList("application/pdf"));
    fileDialog.setDefaultSuffix("pdf");
    if (fileDialog.exec() != QDialog::Accepted)
        return;
    QString file_name = fileDialog.selectedFiles().first();
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(file_name);
    ui->textEdit->document()->print(&printer);
    statusBar()->showMessage(tr("Exported \"%1\"")
                                     .arg(QDir::toNativeSeparators(file_name)));
}

void TextEditor::on_actionRight_alignment_triggered() {
    QTextCursor cursor(ui->textEdit->textCursor());

    if (alignment != "2") {
        ui->textEdit->selectAll();
        ui->textEdit->setAlignment(Qt::AlignRight);
        cursor.clearSelection();
        ui->textEdit->setTextCursor(cursor);
    }

    if (cursor.document()->isEmpty())
        sendBlockFormatChange();
}

void TextEditor::on_actionLeft_alignment_triggered() {
    QTextCursor cursor(ui->textEdit->textCursor());

    if (alignment != "1") {
        ui->textEdit->selectAll();
        ui->textEdit->setAlignment(Qt::AlignLeft);
        cursor.clearSelection();
        ui->textEdit->setTextCursor(cursor);
    }

    if (cursor.document()->isEmpty())
        sendBlockFormatChange();
}

void TextEditor::on_actionCenter_alignment_triggered() {
    QTextCursor cursor(ui->textEdit->textCursor());

    if (alignment != "132") {
        ui->textEdit->selectAll();
        ui->textEdit->setAlignment(Qt::AlignCenter);
        cursor.clearSelection();
        ui->textEdit->setTextCursor(cursor);
    }

    if (cursor.document()->isEmpty())
        sendBlockFormatChange();
}

void TextEditor::on_actionJustify_triggered() {
    QTextCursor cursor(ui->textEdit->textCursor());

    if (alignment != "8") {
        ui->textEdit->selectAll();
        ui->textEdit->setAlignment(Qt::AlignJustify);
        cursor.clearSelection();
        ui->textEdit->setTextCursor(cursor);
    }

    if (cursor.document()->isEmpty())
        sendBlockFormatChange();
}

void TextEditor::changeCursorPosition(_int position, _int siteId) {
    /* The user that perform the action is searched */
    QTextCursor cursor(ui->textEdit->textCursor());
    if (position >= cursor.document()->characterCount())
        position = cursor.document()->characterCount() - 1;
    User u;
    for (User &user : this->activeUsers) {
        if (user.getSiteId() == siteId) {
            u = user;
            user.setLastCursorPos(position);
        }
    }

    /* If the last position of the cursor was 0, there wasn't any cursor show on the editor of the
     * other clients, so it is not necessary to delete the previous cursor */
    if (u.getLastCursorPos() != 0 && u.getLastCursorPos() <= cursor.document()->characterCount() - 1)
        changeBackground(u.getLastCursorPos(), Qt::white);

    /* When the previous cursor position is deleted, the new one is shown on the editor if the new
     * position is different than 0. */
    if (position != 0)
        changeBackground(position, u.getAssignedColor());
}

void TextEditor::changeBackground(_int position, const QColor &color) {
    ui->textEdit->document()->blockSignals(true);
    QTextCursor cursor(ui->textEdit->textCursor());
    cursor.setPosition(position, QTextCursor::MoveAnchor);
    QTextCharFormat textCharFormat = cursor.charFormat();
    textCharFormat.setBackground(color);

    cursor.setPosition(position - 1, QTextCursor::MoveAnchor);
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    cursor.setCharFormat(textCharFormat);
    ui->textEdit->document()->blockSignals(false);
}

void TextEditor::insert_text(_int pos, const QString &character, const Symbol::CharFormat &font, _int siteId) {
    insertOneChar(pos, character, font);
    this->changeCursorPosition(pos + 1, siteId);
}

void TextEditor::insertOneChar(_int pos, const QString &character, const Symbol::CharFormat &font) {
    QTextCursor cursor(ui->textEdit->textCursor());
    int originalPosition = cursor.position();
    cursor.setPosition(pos);
    QTextCharFormat textCharFormat = ui->textEdit->currentCharFormat();

    /* It is used to retrieve all information inside the font value, font family, size, weight,
     * italic, underline, background and foreground color.*/
    QRegExp tagExp("/");
    QStringList firstList = font.font.split(tagExp);

    QRegExp tagExp2(",");
    QStringList fontList = font.font.split(tagExp2);
    QFont insertedFont;
    insertedFont.setFamily(fontList.at(0));
    insertedFont.setPointSize(fontList.at(1).toInt());
    insertedFont.setWeight(fontList.at(4).toInt());
    insertedFont.setItalic(!(fontList.at(5) == "0"));
    insertedFont.setUnderline(!(fontList.at(6) == "0"));

    textCharFormat.setFont(insertedFont);
    textCharFormat.setForeground(font.foreground);

    /* Here, there is the actual change of the char. */
    ui->textEdit->document()->blockSignals(true);
    cursor.insertText(character, textCharFormat);
    cursor.setPosition(originalPosition);
    ui->textEdit->document()->blockSignals(false);
}

void TextEditor::delete_text(_int pos, _int siteId) {
    /* The cursor is moved in the position where there is the char that must be deleted. */
    QTextCursor cursor(ui->textEdit->textCursor());
    int originalPosition = cursor.position();
    cursor.movePosition(QTextCursor::End);
    int maxPos = cursor.position();
    cursor.setPosition(pos);

    this->changeCursorPosition(pos, siteId);
    /* The char is deleted. */
    ui->textEdit->document()->blockSignals(true);
    cursor.deleteChar();
    ui->textEdit->document()->blockSignals(false);

    /* The cursor is moved back to the previous position. */
    if (originalPosition == maxPos)
        cursor.setPosition(originalPosition - 1);
    else
        cursor.setPosition(originalPosition);

}

void TextEditor::changeBlockFormat(const QString &font) {
    QRegExp tagExp("/");
    QStringList firstList = font.split(tagExp);

    ui->textEdit->document()->blockSignals(true);
    QTextCursor cursor(ui->textEdit->textCursor());
    ui->textEdit->selectAll();
    switch (firstList.at(1).toInt()) {
        case 1:
            if (alignment != "1")
                ui->textEdit->setAlignment(Qt::AlignLeft);
            break;
        case 2:
            if (alignment != "2")
                ui->textEdit->setAlignment(Qt::AlignRight);
            break;
        case 8:
            if (alignment != "8")
                ui->textEdit->setAlignment(Qt::AlignJustify);
            break;
        case 132:
            if (alignment != "132")
                ui->textEdit->setAlignment(Qt::AlignCenter);
            break;
    }
    this->changeIndentSpacing(firstList.at(2).toInt());

    alignment = firstList.at(1);
    indentation = firstList.at(2);
    cursor.clearSelection();
    ui->textEdit->setTextCursor(cursor);
    ui->textEdit->document()->blockSignals(false);
}

void TextEditor::changeCharFormat(_int pos, const Symbol::CharFormat &charData) {
    ui->textEdit->document()->blockSignals(true);
    QTextCursor cursor(ui->textEdit->textCursor());
    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    QTextCharFormat textCharFormat = cursor.charFormat();

    QRegExp tagExp2(",");
    QStringList fontList = charData.font.split(tagExp2);
    QFont insertedFont;
    insertedFont.setFamily(fontList.at(0));
    insertedFont.setPointSize(fontList.at(1).toInt());
    insertedFont.setWeight(fontList.at(4).toInt());
    insertedFont.setItalic(!(fontList.at(5) == "0"));
    insertedFont.setUnderline(!(fontList.at(6) == "0"));

    textCharFormat.setFont(insertedFont);
    textCharFormat.setForeground(charData.foreground);

    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
    cursor.setCharFormat(textCharFormat);
    ui->textEdit->document()->blockSignals(false);
}

void TextEditor::cursorMoved() {
    QTextCursor cursor(ui->textEdit->textCursor());
    this->client->changeCursor(cursor.position());
}

void TextEditor::change(int pos, int del, int add) {
    /* Here, the format of the char is taken. */
    QTextCursor cursor(ui->textEdit->textCursor());
    cursor.setPosition(cursor.selectionEnd(), QTextCursor::MoveAnchor);
    QTextCharFormat textCharFormat = cursor.charFormat();
    QTextBlockFormat textBlockFormat = cursor.block().blockFormat();

    QString result = textCharFormat.font().toString() + "/"
                     + QString::number(textBlockFormat.alignment()) + "/"
                     + QString::number(textBlockFormat.indent());

    Symbol::CharFormat charData;
    charData.foreground = textCharFormat.foreground().color();
    charData.font = result;

    if (add != 0 && del == 0) { /* insert chars */

        QString added = ui->textEdit->toPlainText().mid(pos, add);
        if (add == 1) {
            this->client->localInsert(pos, added, charData);
            changeBackground(pos + 1, Qt::white);
        } else {
            multipleInsert(pos, added);
        }

    } else if (add == 0 && del != 0) { /* delete chars */

        if (del == 1)
            this->client->localErase(pos);
        else
            multipleErase(pos, del);

    } else { /* Some chars are deleted and some other else are inserted. */
        ui->textEdit->undo();
        QString removed = ui->textEdit->toPlainText().mid(pos, del);
        ui->textEdit->redo();
        QString added = ui->textEdit->toPlainText().mid(pos, add);
        if (add == del) {
            /* The for loop is used to check if some chars are changed or if the slot is called only because
             * the format of one or more chars is changed. */
            bool equal = true;
            for (int i = 0; i < added.size(); i++) {
                if (added[i] != removed[i]) {
                    equal = false;
                    break;
                }
            }

            if (equal) { /* If chars are equal, it means that the format is changed. */
                /* Check if alignment or indentation are changed. */
                if (alignment == QString::number(textBlockFormat.alignment()) &&
                    indentation == QString::number(textBlockFormat.indent())) {
                    multipleUpdate(pos, removed.size());
                } else {
                    alignment = QString::number(textBlockFormat.alignment());
                    indentation = QString::number(textBlockFormat.indent());

                    this->client->changeBlockFormat(charData);
                }
                return;
            }
        }

        /* If we arrive here, it means that some chars must be deleted and some other else
         * must be inserted. */
        int originalPos = cursor.position();
        cursor.movePosition(QTextCursor::End);
        int maxPos = cursor.position();
        cursor.setPosition(originalPos);
        if (del == maxPos + 1) {
            for (int i = pos + del - 2; i >= pos; i--) {
                this->client->localErase(i);
            }
            multipleInsert(pos, added);
            return;
        }
        multipleErase(pos, removed.size());
        multipleInsert(pos, added);
    }
}

void TextEditor::multipleInsert(int pos, const QString &added) {
    for (QString c : added) {
        QTextCursor cursor(ui->textEdit->textCursor());
        cursor.setPosition(pos + 1, QTextCursor::MoveAnchor);
        QTextCharFormat textCharFormat = cursor.charFormat();
        QTextBlockFormat textBlockFormat = cursor.block().blockFormat();

        QString result = textCharFormat.font().toString() + "/"
                         + QString::number(textBlockFormat.alignment()) + "/"
                         + QString::number(textBlockFormat.indent());

        Symbol::CharFormat charData;
        charData.foreground = textCharFormat.foreground().color();
        charData.font = result;

        cursor.setPosition(pos, QTextCursor::MoveAnchor);
        cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
        textCharFormat.setBackground(Qt::white);
        cursor.setCharFormat(textCharFormat);

        this->client->localInsert(pos, c, charData);
        pos++;
    }
}

void TextEditor::multipleErase(int pos, int del) {
    for (int i = pos + del - 1; i >= pos; i--) {
        this->client->localErase(i);
    }
}

void TextEditor::sendBlockFormatChange() {
    QTextCursor cursor(ui->textEdit->textCursor());
    cursor.setPosition(cursor.selectionEnd(), QTextCursor::MoveAnchor);
    QTextCharFormat textCharFormat = cursor.charFormat();
    QTextBlockFormat textBlockFormat = cursor.block().blockFormat();

    QString result = textCharFormat.font().toString() + "/"
                     + QString::number(textBlockFormat.alignment()) + "/"
                     + QString::number(textBlockFormat.indent());

    Symbol::CharFormat charData;
    charData.foreground = textCharFormat.foreground().color();
    charData.font = result;

    this->client->changeBlockFormat(charData);
}

void TextEditor::multipleUpdate(_int pos, _int size) {
    for (int i = 0; i < size; i++) {
        QTextCursor cursor(ui->textEdit->textCursor());
        cursor.setPosition(pos + 1, QTextCursor::MoveAnchor);
        QTextCharFormat textCharFormat = cursor.charFormat();
        QTextBlockFormat textBlockFormat = cursor.block().blockFormat();

        QString result = textCharFormat.font().toString() + "/"
                         + QString::number(textBlockFormat.alignment()) + "/"
                         + QString::number(textBlockFormat.indent());

        Symbol::CharFormat charData;
        charData.foreground = textCharFormat.foreground().color();
        charData.font = result;

        this->client->localUpdate(pos, charData);
        pos++;
    }
}

void TextEditor::closeEditor() {
    this->close();
}
