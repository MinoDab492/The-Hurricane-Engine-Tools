/*
 * SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_exr_test.h"

#include <simpletest.h>
#include <QCoreApplication>

#include  <sdk/tests/testui.h>

#include <half.h>
#include <KisMimeDatabase.h>
#include "filestest.h"

#ifndef FILES_DATA_DIR
#error "FILES_DATA_DIR not set. A directory with the data used for testing the importing of files in krita"
#endif

const QString ExrMimetype = "application/x-extension-exr";

void KisExrTest::testFiles()
{
    TestUtil::testFiles(QString(FILES_DATA_DIR) + "/sources", QStringList(), QString(), 5);
}

void KisExrTest::testImportFromWriteonly()
{
    TestUtil::testImportFromWriteonly(ExrMimetype);
}

void KisExrTest::testExportToReadonly()
{
    TestUtil::testExportToReadonly(ExrMimetype);
}

void KisExrTest::testImportIncorrectFormat()
{
    TestUtil::testImportIncorrectFormat(ExrMimetype);
}

void KisExrTest::testRoundTrip()
{
    QString inputFileName(TestUtil::fetchDataFileLazy("CandleGlass.exr"));

    KisDocument *doc1 = KisPart::instance()->createDocument();

    doc1->setFileBatchMode(true);
    bool r = doc1->importDocument(inputFileName);

    QVERIFY(r);
    QVERIFY(doc1->errorMessage().isEmpty());
    QVERIFY(doc1->image());

    QTemporaryFile savedFile(QDir::tempPath() + QLatin1String("/krita_XXXXXX") + QLatin1String(".exr"));
    savedFile.setAutoRemove(true);
    savedFile.open();

    QString savedFileName(savedFile.fileName());

    QString typeName = KisMimeDatabase::mimeTypeForFile(savedFileName, false);
    QByteArray mimeType(typeName.toLatin1());

    r = doc1->exportDocumentSync(savedFileName, mimeType);
    QVERIFY(r);
    QVERIFY(QFileInfo(savedFileName).exists());

    {
        KisDocument *doc2 = KisPart::instance()->createDocument();
        doc2->setFileBatchMode(true);
        r = doc2->importDocument(savedFileName);

        QVERIFY(r);
        QVERIFY(doc2->errorMessage().isEmpty());
        QVERIFY(doc2->image());

        doc1->image()->root()->firstChild()->paintDevice()->convertToQImage(0).save("1.png");
        doc2->image()->root()->firstChild()->paintDevice()->convertToQImage(0).save("2.png");

        QVERIFY(TestUtil::comparePaintDevicesClever<half>(
                    doc1->image()->root()->firstChild()->paintDevice(),
                    doc2->image()->root()->firstChild()->paintDevice(),
                    0.01 /* meaningless alpha */));

        delete doc2;
    }

    savedFile.close();

    delete doc1;

}

KISTEST_MAIN(KisExrTest)


