#ifndef DOCRENDER_H
#define DOCRENDER_H

#include <QString>
#include <QStringList>

// ==================== DocRender ====================
//
// The shared paper every printable document in the app is put on: it takes the
// rich-text HTML a document module builds and turns it into an A4 PDF, or into
// page images for an on-screen preview.
//
// Both paths draw through one QTextDocument layout, which is what keeps a
// preview an honest picture of the file that gets written.
//
namespace DocRender {

// A4 portrait geometry, shared by the PDF and the preview.
extern const qreal kPageWidthMm;
extern const qreal kPageHeightMm;
extern const qreal kMarginMm;

// Renders to an A4 PDF. Returns false if the file could not be written.
bool writePdf(const QString &html, const QString &path, const QString &title);

// Rasterises each page to <dir>/<prefix>-<n>.png for a preview dialog.
// Returns the files written, in page order.
QStringList renderPages(const QString &html, const QString &dir,
                        const QString &prefix, int dpi = 110);

} // namespace DocRender

#endif // DOCRENDER_H
