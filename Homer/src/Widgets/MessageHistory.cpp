/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: Implementation of a modified QTextEdit for message history
 * Author:  Thomas Volkert
 * Since:   2008-12-15
 */

#include <Widgets/MessageHistory.h>
#include <Logger.h>
#include <ContactsManager.h>
#include <Configuration.h>

#include <QTextBrowser>
#include <QEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QScrollBar>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QMovie>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MessageHistory::MessageHistory(QWidget* pParent) :
        QTextBrowser(pParent)
{
    mSomeTextSelected = false;
    connect(this, SIGNAL(copyAvailable(bool)), this, SLOT(textSelected(bool)));
    AddAnimation(QUrl(URL_SMILE), PATH_SMILE);
}

MessageHistory::~MessageHistory()
{
}

///////////////////////////////////////////////////////////////////////////////

void MessageHistory::AddAnimation(const QUrl &pUrl, const QString &pFile)
{
    QMovie *tMovie = new QMovie(this);
    tMovie->setFileName(pFile);
    mUrls.insert(tMovie, pUrl);
    connect(tMovie, SIGNAL(frameChanged(int)), this, SLOT(Animate()));
    tMovie->start();
}

void MessageHistory::Animate()
{
    if (QMovie *tMovie = qobject_cast<QMovie*>(sender()))
    {
        // update picture
        document()->addResource(QTextDocument::ImageResource, mUrls.value(tMovie), tMovie->currentPixmap());

        // force reload
        setLineWrapColumnOrWidth(lineWrapColumnOrWidth());
    }
}

void MessageHistory::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    if (mSomeTextSelected)
    {
        tAction = tMenu.addAction("Copy to clipboard");
        QIcon tIcon3;
        tIcon3.addPixmap(QPixmap(":/images/22_22/Save_Clipboard.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon3);
    }

    tAction = tMenu.addAction("Save history");
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/22_22/Save.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);

    tAction = tMenu.addAction("Close messages");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Close.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Copy to clipboard") == 0)
        {
            copy();
            return;
        }
        if (tPopupRes->text().compare("Close messages") == 0)
        {
        	QApplication::postEvent(parentWidget()->parentWidget(), new QCloseEvent());
            return;
        }
        if (tPopupRes->text().compare("Save history") == 0)
        {
            Save();
            return;
        }
    }
}

void MessageHistory::Update(QString pNewHistory)
{
    // save old widget state
    int tOldVertSliderPosition = verticalScrollBar()->sliderPosition();
    int tOldHorizSliderPosition = horizontalScrollBar()->sliderPosition();
    bool tWasVertMaximized = (tOldVertSliderPosition == verticalScrollBar()->maximum());

    // set the new history
    setText(pNewHistory);

    // restore old widget state
    if (tWasVertMaximized)
        verticalScrollBar()->setSliderPosition(verticalScrollBar()->maximum());
    else
        verticalScrollBar()->setSliderPosition(tOldVertSliderPosition);
    horizontalScrollBar()->setSliderPosition(tOldHorizSliderPosition);
}

void MessageHistory::textSelected(bool pAvail)
{
    mSomeTextSelected = pAvail;
}

void MessageHistory::Save()
{
    QString tFileName = QFileDialog::getSaveFileName(this,
                                                     "Save message history",
                                                     QDir::homePath() + "/MessageHistory.txt",
                                                     "Text File (*.txt)",
                                                     NULL,
                                                     CONF_NATIVE_DIALOGS);

    if (tFileName.isEmpty())
        return;

    // write history to file
    QFile tFile(tFileName);
    if (!tFile.open(QIODevice::ReadWrite))
        return;
    if (!tFile.write(toPlainText().toStdString().c_str(), toPlainText().size()))
        return;
    tFile.close();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
