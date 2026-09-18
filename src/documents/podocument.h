#ifndef PODOCUMENT_H
#define PODOCUMENT_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

// ==================== PoDocument ====================
//
// Builds the printable purchase order: an HTML rich-text document that Qt can
// render both to a PDF file and to page images for on-screen preview.
//
// Kept free of ExcelHandler so the layout can be rendered from plain data in
// isolation, which is what makes it possible to eyeball the design without
// standing up a database.
//
namespace PoDocument {

// company: name, addressLine1/2, city, phone, email, website, gstin
// vendor:  vendorName, vendorAddress, contactPerson, phone, email, gstin
// po:      poNo, date, expectedDate, preparedBy, approvedBy, status
// items:   list of { partName, partNo, qty, unitPrice, totalAmount }
QString buildHtml(const QVariantMap &company,
                  const QVariantMap &vendor,
                  const QVariantMap &po,
                  const QVariantList &items,
                  const QString &comments = QString());

// Renders to an A4 PDF. Returns false if the file could not be written.
bool writePdf(const QString &html, const QString &path);

// Rasterises each page to <dir>/po-preview-<n>.png for the preview dialog.
// Returns the files written, in page order.
QStringList renderPages(const QString &html, const QString &dir, int dpi = 110);

} // namespace PoDocument

#endif // PODOCUMENT_H
