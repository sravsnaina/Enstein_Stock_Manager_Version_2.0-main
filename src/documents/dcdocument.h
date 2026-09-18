#ifndef DCDOCUMENT_H
#define DCDOCUMENT_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

// ==================== DcDocument ====================
//
// Builds the printable delivery challan: an HTML rich-text document that Qt can
// render both to a PDF file and to page images for on-screen preview.
//
// Reproduces the paper challan the company already uses - blue title band,
// company block, party and shipping blocks side by side, the SL/Item/HSN/Qty/
// Unit table, and the received-by / delivered-by acknowledgement feet.
//
// Kept free of ExcelHandler so the layout can be rendered from plain data in
// isolation, which is what makes it possible to eyeball the design without
// standing up a database.
//
namespace DcDocument {

// company: name, addressLine1/2, city, phone, email, website, gstin
// dc:      dcNo, date, deliveryTime, terms, deliveredBy, receivedBy,
//          partyName, partyAddress, partyPhone, partyEmail, partyGstin,
//          shipName, shipAddress, shipPhone, shipEmail, shipGstin
// items:   list of { itemName, partNo, hsnCode, qty, unit }
QString buildHtml(const QVariantMap &company,
                  const QVariantMap &dc,
                  const QVariantList &items);

// Renders to an A4 PDF. Returns false if the file could not be written.
bool writePdf(const QString &html, const QString &path);

// Rasterises each page to <dir>/dc-preview-<n>.png for the preview dialog.
// Returns the files written, in page order.
QStringList renderPages(const QString &html, const QString &dir, int dpi = 110);

} // namespace DcDocument

#endif // DCDOCUMENT_H
