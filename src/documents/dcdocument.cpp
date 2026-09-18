#include "documents/dcdocument.h"
#include "documents/docrender.h"

#include <QLocale>

namespace {

// House colours, taken from the delivery-challan template this reproduces.
const char *kBlue      = "#1b9dd9";   // title band, section labels, table head
const char *kBlueLight = "#7fd3f0";   // the slashes decorating the title band
const char *kRule      = "#1b9dd9";   // the full-width separators
const char *kZebra     = "#f1f5f8";   // alternating item rows
const char *kGrid      = "#bcd7e6";   // rules inside the items table

QString esc(const QVariant &v)
{
    return QString(v.toString().trimmed()).toHtmlEscaped();
}

// Quantities are whole numbers most of the time but may be weighed or metered,
// so trailing ".00" is dropped rather than printed on every line.
QString qty(double v)
{
    static const QLocale in(QLocale::English, QLocale::India);
    if (qFuzzyCompare(v, qRound(v)))
        return in.toString(qRound(v));
    return in.toString(v, 'f', 2);
}

// A block of "Label : value" rows. Values arrive already escaped so callers can
// pass markup (a multi-line address) through untouched. labelWidthPct keeps the
// colons tucked in behind the labels whatever width the block is placed in.
QString fieldBlock(const QVector<QPair<QString, QString>> &rows, int labelWidthPct = 34)
{
    QString html = QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"font-size:8pt\">");
    for (const auto &row : rows) {
        html += QStringLiteral(
            "<tr>"
            "<td width=\"%1%\" valign=\"top\" style=\"padding:2px 0\">%2</td>"
            "<td width=\"4%\"  valign=\"top\" style=\"padding:2px 0\">:</td>"
            "<td width=\"%3%\" valign=\"top\" style=\"padding:2px 0\">%4</td>"
            "</tr>")
            .arg(QString::number(labelWidthPct), row.first,
                 QString::number(96 - labelWidthPct), row.second);
    }
    return html + QStringLiteral("</table>");
}

// A full-width coloured separator. Drawn as a table row rather than an <hr>
// because only a cell background gives a rule of a chosen colour and weight.
QString rule(const char *color, int topMargin = 8)
{
    return QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"margin-top:%1px\"><tr>"
        "<td bgcolor=\"%2\" style=\"font-size:2pt\">&nbsp;</td>"
        "</tr></table>").arg(QString::number(topMargin), QString(color));
}

// The address of a party: whatever lines were filled in, joined so a missing
// field never leaves a blank gap in the block.
QString addressLines(const QString &address)
{
    QStringList kept;
    for (const QString &part : address.split('\n')) {
        const QString t = part.trimmed();
        if (!t.isEmpty()) kept << t.toHtmlEscaped();
    }
    return kept.join(QStringLiteral("<br/>"));
}

} // namespace

namespace DcDocument {

QString buildHtml(const QVariantMap &company,
                  const QVariantMap &dc,
                  const QVariantList &items)
{
    const QString companyName = company.value("name").toString().trimmed().isEmpty()
                                    ? QStringLiteral("Company Name")
                                    : company.value("name").toString().trimmed();

    // The company's own address block, assembled from the profile fields.
    QStringList companyAddress;
    for (const char *key : {"addressLine1", "addressLine2", "city"}) {
        const QString v = company.value(QLatin1String(key)).toString().trimmed();
        if (!v.isEmpty()) companyAddress << v.toHtmlEscaped();
    }

    const QString companyBlock = fieldBlock({
        {QStringLiteral("Address"),  companyAddress.join(QStringLiteral("<br/>"))},
        {QStringLiteral("Phone No"), esc(company.value("phone"))},
        {QStringLiteral("Email ID"), esc(company.value("email"))},
        {QStringLiteral("GSTIN"),    esc(company.value("gstin"))}
    }, 13);

    const QString partyBlock = fieldBlock({
        {QStringLiteral("Party Name"), esc(dc.value("partyName"))},
        {QStringLiteral("Address"),    addressLines(dc.value("partyAddress").toString())},
        {QStringLiteral("Phone No"),   esc(dc.value("partyPhone"))},
        {QStringLiteral("Email ID"),   esc(dc.value("partyEmail"))},
        {QStringLiteral("GSTIN"),      esc(dc.value("partyGstin"))}
    });

    const QString shipBlock = fieldBlock({
        {QStringLiteral("Shipping Name"), esc(dc.value("shipName"))},
        {QStringLiteral("Address"),       addressLines(dc.value("shipAddress").toString())},
        {QStringLiteral("Phone No"),      esc(dc.value("shipPhone"))},
        {QStringLiteral("Email ID"),      esc(dc.value("shipEmail"))},
        {QStringLiteral("GSTIN"),         esc(dc.value("shipGstin"))}
    });

    // Item lines. The total is only labelled with a unit when every line shares
    // one, since adding kilograms to pieces would be a lie.
    double totalQty = 0.0;
    QString commonUnit;
    bool unitsAgree = true;
    QString itemRows;
    int index = 0;
    for (const QVariant &v : items) {
        const QVariantMap it = v.toMap();
        const double quantity = it.value("qty").toDouble();
        const QString unit = it.value("unit").toString().trimmed();
        totalQty += quantity;
        if (index == 0)             commonUnit = unit;
        else if (unit != commonUnit) unitsAgree = false;

        QString name = esc(it.value("itemName"));
        const QString partNo = it.value("partNo").toString().trimmed();
        if (!partNo.isEmpty())
            name += QStringLiteral("<br/><span style=\"font-size:7pt;color:#7f8c8d\">")
                    + partNo.toHtmlEscaped() + QStringLiteral("</span>");

        const QString bg = (index % 2) ? QString(kZebra) : QStringLiteral("#ffffff");
        itemRows += QStringLiteral(
            "<tr bgcolor=\"%1\">"
            "<td width=\"9%\"  align=\"center\" style=\"padding:4px\">%2</td>"
            "<td width=\"45%\" style=\"padding:4px\">%3</td>"
            "<td width=\"18%\" align=\"center\" style=\"padding:4px\">%4</td>"
            "<td width=\"14%\" align=\"center\" style=\"padding:4px\">%5</td>"
            "<td width=\"14%\" align=\"center\" style=\"padding:4px\">%6</td>"
            "</tr>")
            .arg(bg, QString::number(index + 1), name,
                 esc(it.value("hsnCode")), qty(quantity), esc(it.value("unit")));
        ++index;
    }

    // Keep the table looking like a form even when only a couple of lines were
    // delivered, exactly as the paper template does.
    const int minRows = 7;
    for (int i = index; i < minRows; ++i) {
        const QString bg = (i % 2) ? QString(kZebra) : QStringLiteral("#ffffff");
        itemRows += QStringLiteral(
            "<tr bgcolor=\"%1\">"
            "<td style=\"padding:4px\">&nbsp;</td>"
            "<td style=\"padding:4px\">&nbsp;</td>"
            "<td style=\"padding:4px\">&nbsp;</td>"
            "<td style=\"padding:4px\">&nbsp;</td>"
            "<td style=\"padding:4px\">&nbsp;</td></tr>").arg(bg);
    }

    const QString totalText = unitsAgree && !commonUnit.isEmpty()
                                  ? qty(totalQty) + QStringLiteral("&nbsp;") + commonUnit.toHtmlEscaped()
                                  : qty(totalQty);

    const QString terms = dc.value("terms").toString().trimmed().isEmpty()
        ? QStringLiteral("&nbsp;")
        : dc.value("terms").toString().trimmed().toHtmlEscaped().replace("\n", "<br/>");

    // The acknowledgement feet: whoever handed the goods over and whoever took
    // delivery both sign, so the names are printed if known and left blank if not.
    const QString receivedBlock = fieldBlock({
        {QStringLiteral("Name"),      esc(dc.value("receivedBy"))},
        {QStringLiteral("Comment"),   QStringLiteral("&nbsp;")},
        {QStringLiteral("Date"),      QStringLiteral("&nbsp;")},
        {QStringLiteral("Signature"), QStringLiteral("&nbsp;")}
    });
    const QString deliveredBlock = fieldBlock({
        {QStringLiteral("Name"),      esc(dc.value("deliveredBy"))},
        {QStringLiteral("Comment"),   QStringLiteral("&nbsp;")},
        {QStringLiteral("Date"),      QStringLiteral("&nbsp;")},
        {QStringLiteral("Signature"), QStringLiteral("&nbsp;")}
    });

    QString html = QStringLiteral(
        "<html><body style=\"font-family:'Helvetica','Arial',sans-serif; "
        "font-size:9pt; color:#1a1a1a\">");

    // ---- Title band ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">"
        "<tr>"
        "  <td width=\"58%\" bgcolor=\"%1\" valign=\"middle\" style=\"padding:8px 10px\">"
        "    <span style=\"color:#ffffff; font-size:19pt; font-weight:bold\">DELIVERY CHALLAN</span></td>"
        "  <td width=\"16%\" bgcolor=\"%1\" align=\"right\" valign=\"middle\" style=\"padding:8px 6px\">"
        "    <span style=\"color:%2; font-size:17pt; font-weight:bold\">///</span></td>"
        "  <td width=\"26%\" align=\"right\" valign=\"middle\" style=\"padding:8px 0 8px 10px\">"
        "    <span style=\"color:%1; font-size:11pt; font-weight:bold\">%3</span></td>"
        "</tr></table>").arg(QString(kBlue), QString(kBlueLight), companyName.toHtmlEscaped());

    // ---- Company ----
    html += QStringLiteral(
        "<p style=\"margin-top:10px; margin-bottom:2px\">"
        "<span style=\"color:%1; font-size:10pt; font-weight:bold\">%2 :</span></p>")
        .arg(QString(kBlue), companyName.toHtmlEscaped());
    html += companyBlock;
    html += rule(kRule, 8);

    // ---- Party / shipping ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:8px\">"
        "<tr>"
        "  <td width=\"48%\" valign=\"top\">"
        "    <span style=\"color:%1; font-size:9pt; font-weight:bold\">Delivery Challan For :</span></td>"
        "  <td width=\"4%\">&nbsp;</td>"
        "  <td width=\"48%\" valign=\"top\">"
        "    <span style=\"color:%1; font-size:9pt; font-weight:bold\">Shipping To :</span></td>"
        "</tr>"
        "<tr>"
        "  <td valign=\"top\" style=\"padding-top:4px\">%2</td>"
        "  <td>&nbsp;</td>"
        "  <td valign=\"top\" style=\"padding-top:4px\">%3</td>"
        "</tr></table>").arg(QString(kBlue), partyBlock, shipBlock);
    html += rule(kRule, 8);

    // ---- Challan number, date, delivery time ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:8px\">"
        "<tr>"
        "  <td width=\"48%\" valign=\"top\">%1</td>"
        "  <td width=\"4%\">&nbsp;</td>"
        "  <td width=\"48%\" valign=\"top\">%2</td>"
        "</tr></table>")
        .arg(fieldBlock({
                 {QStringLiteral("Challan No"), esc(dc.value("dcNo"))},
                 {QStringLiteral("Date"),       esc(dc.value("date"))}
             }, 28),
             fieldBlock({
                 {QStringLiteral("Delivery Time"), esc(dc.value("deliveryTime"))}
             }, 28));

    // ---- Items ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"4\" border=\"1\" "
        "style=\"margin-top:10px; border-color:%1; border-collapse:collapse; font-size:8pt\">"
        "<thead><tr bgcolor=\"%2\">"
        "  <td align=\"center\"><span style=\"color:#ffffff; font-weight:bold\">SL NO.</span></td>"
        "  <td align=\"center\"><span style=\"color:#ffffff; font-weight:bold\">Item Name</span></td>"
        "  <td align=\"center\"><span style=\"color:#ffffff; font-weight:bold\">HSN/SAC Code</span></td>"
        "  <td align=\"center\"><span style=\"color:#ffffff; font-weight:bold\">Quantity</span></td>"
        "  <td align=\"center\"><span style=\"color:#ffffff; font-weight:bold\">Unit</span></td>"
        "</tr></thead>%3"
        "<tr bgcolor=\"%2\">"
        "  <td colspan=\"3\" style=\"padding:4px\"><span style=\"color:#ffffff; font-weight:bold\">TOTAL</span></td>"
        "  <td align=\"center\" style=\"padding:4px\"><span style=\"color:#ffffff; font-weight:bold\">%4</span></td>"
        "  <td>&nbsp;</td>"
        "</tr></table>").arg(QString(kGrid), QString(kBlue), itemRows, totalText);

    // ---- Terms and the authorised signature ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:12px\">"
        "<tr>"
        "  <td width=\"58%\" valign=\"top\">"
        "    <span style=\"font-size:8pt; font-weight:bold\">Terms &amp; Conditions :</span><br/>"
        "    <span style=\"font-size:8pt\">%1</span></td>"
        "  <td width=\"42%\" valign=\"top\" align=\"right\">"
        "    <span style=\"font-size:8pt\">For, %2</span>"
        "    <table width=\"90%\" cellspacing=\"0\" cellpadding=\"0\" align=\"right\" "
        "           style=\"margin-top:44px\">"
        "      <tr><td bgcolor=\"#8a9199\" style=\"font-size:1pt\">&nbsp;</td></tr>"
        "      <tr><td align=\"right\" style=\"padding-top:2px\">"
        "        <span style=\"font-size:8pt\">Authorised Signature</span></td></tr>"
        "    </table></td>"
        "</tr></table>").arg(terms, companyName.toHtmlEscaped());

    html += rule(kRule, 10);

    // ---- Acknowledgement feet ----
    html += QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:8px\">"
        "<tr>"
        "  <td width=\"48%\" valign=\"top\">"
        "    <span style=\"font-size:8pt; font-weight:bold\">Received By :</span></td>"
        "  <td width=\"4%\">&nbsp;</td>"
        "  <td width=\"48%\" valign=\"top\">"
        "    <span style=\"font-size:8pt; font-weight:bold\">Delivered By :</span></td>"
        "</tr>"
        "<tr>"
        "  <td valign=\"top\" style=\"padding-top:4px\">%1</td>"
        "  <td>&nbsp;</td>"
        "  <td valign=\"top\" style=\"padding-top:4px\">%2</td>"
        "</tr></table>").arg(receivedBlock, deliveredBlock);

    html += QStringLiteral("</body></html>");
    return html;
}

bool writePdf(const QString &html, const QString &path)
{
    return DocRender::writePdf(html, path, QStringLiteral("Delivery Challan"));
}

QStringList renderPages(const QString &html, const QString &dir, int dpi)
{
    return DocRender::renderPages(html, dir, QStringLiteral("dc-preview"), dpi);
}

} // namespace DcDocument
