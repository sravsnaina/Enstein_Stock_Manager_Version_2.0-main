#include "documents/docrender.h"

#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainter>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QMarginsF>

namespace DocRender {

const qreal kPageWidthMm  = 210.0;     // A4 portrait
const qreal kPageHeightMm = 297.0;
const qreal kMarginMm     = 12.0;

bool writePdf(const QString &html, const QString &path, const QString &title)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(kMarginMm, kMarginMm, kMarginMm, kMarginMm),
                          QPageLayout::Millimeter);
    writer.setResolution(300);
    writer.setTitle(title);

    const QSizeF pagePx = writer.pageLayout()
                              .paintRectPixels(writer.resolution())
                              .size();

    QTextDocument doc;
    // Binding the layout to the writer is what makes point sizes scale to 300
    // dpi; without it the page is laid out full size but typeset for a screen.
    doc.documentLayout()->setPaintDevice(&writer);
    doc.setHtml(html);
    doc.setPageSize(pagePx);

    // Paginated by hand rather than via QTextDocument::print(), which stamps a
    // page number onto every sheet. This also keeps the PDF and the on-screen
    // preview on one shared drawing path.
    QPainter painter;
    if (!painter.begin(&writer))
        return false;

    const int pages = qMax(1, doc.pageCount());
    for (int i = 0; i < pages; ++i) {
        if (i > 0)
            writer.newPage();
        painter.save();
        painter.translate(0, -i * pagePx.height());
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = QRectF(0, i * pagePx.height(), pagePx.width(), pagePx.height());
        doc.documentLayout()->draw(&painter, ctx);
        painter.restore();
    }
    painter.end();

    return QFileInfo::exists(path) && QFileInfo(path).size() > 0;
}

QStringList renderPages(const QString &html, const QString &dir,
                        const QString &prefix, int dpi)
{
    QDir().mkpath(dir);

    const qreal pxPerMm = dpi / 25.4;
    const QSizeF pagePx((kPageWidthMm  - 2 * kMarginMm) * pxPerMm,
                        (kPageHeightMm - 2 * kMarginMm) * pxPerMm);
    const int marginPx = qRound(kMarginMm * pxPerMm);
    const QSize canvas(qRound(pagePx.width()) + 2 * marginPx,
                       qRound(pagePx.height()) + 2 * marginPx);

    // Carries the target resolution to the text layout, so point sizes scale to
    // this dpi the same way they scale to the PDF writer's.
    const int dotsPerMeter = qRound(dpi / 0.0254);
    QImage probe(1, 1, QImage::Format_ARGB32);
    probe.setDotsPerMeterX(dotsPerMeter);
    probe.setDotsPerMeterY(dotsPerMeter);

    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&probe);
    doc.setHtml(html);
    doc.setPageSize(pagePx);

    QStringList written;
    const int pages = qMax(1, doc.pageCount());
    for (int i = 0; i < pages; ++i) {
        QImage img(canvas, QImage::Format_ARGB32);
        img.setDotsPerMeterX(dotsPerMeter);
        img.setDotsPerMeterY(dotsPerMeter);
        img.fill(Qt::white);

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.translate(marginPx, marginPx - i * pagePx.height());

        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = QRectF(0, i * pagePx.height(), pagePx.width(), pagePx.height());
        doc.documentLayout()->draw(&p, ctx);
        p.end();

        const QString file = QDir(dir).filePath(
            QStringLiteral("%1-%2.png").arg(prefix).arg(i + 1));
        if (img.save(file, "PNG"))
            written << file;
    }
    return written;
}

} // namespace DocRender
