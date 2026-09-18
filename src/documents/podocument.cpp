#include "documents/podocument.h"
#include "documents/docrender.h"

#include <QLocale>

namespace {

// House colours, taken from the purchase-order template this reproduces.
const char *kHeaderBlue = "#2f5597";   // section bars and table headings
const char *kTitleBlue  = "#8ea9db";   // the large PURCHASE ORDER wordmark
const char *kRule       = "#b4c6e7";   // light rules inside the items table
const char *kZebra      = "#f2f6fc";   // alternating item rows

QString esc(const QVariant &v)
{
    return QString(v.toString().trimmed()).toHtmlEscaped();
}

QString money(double v)
{
    // Indian digit grouping (1,23,456.78) to match the in-app formatting.
    static const QLocale in(QLocale::English, QLocale::India);
    return in.toString(v, 'f', 2);
}

// The expected date is a period. Renders as "from - to", collapsing to a
// single date when only one end is known and to "-" when neither is.
QString expectedPeriod(const QVariantMap &po)
{
    const QString from = po.value("expectedDate").toString().trimmed();
    const QString to   = po.value("expectedEndDate").toString().trimmed();
    if (from.isEmpty() && to.isEmpty()) return QStringLiteral("-");
    if (to.isEmpty() || from == to)     return esc(from.isEmpty() ? to : from);
    if (from.isEmpty())                 return QStringLiteral("up to ") + esc(to);
    return esc(from) + QStringLiteral(" &ndash; ") + esc(to);
}

// Joins the non-empty parts of an address into <br>-separated lines so a
// missing field never leaves a blank gap in the block.
QString lines(const QStringList &parts)
{
    QStringList kept;
    for (const QString &p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) kept << t.toHtmlEscaped();           
    }
    return kept.join("<br/>");
}

QString labelled(const QString &label, const QString &value)
{
    const QString v = value.trimmed();
    return v.isEmpty() ? QString() : (label + ": " + v.toHtmlEscaped());
}

} // namespace

namespace PoDocument {

QString buildHtml(const QVariantMap &company,
                  const QVariantMap &vendor,
                  const QVariantMap &po,
                  const QVariantList &items,
                  const QString &comments)
{
    const QString companyName = company.value("name").toString().trimmed().isEmpty()
                                    ? QStringLiteral("Company Name")
                                    : company.value("name").toString().trimmed();

    const QString companyBlock = lines({
        company.value("addressLine1").toString(),
        company.value("addressLine2").toString(),
        company.value("city").toString(),
        labelled("Phone", company.value("phone").toString()),
        labelled("Email", company.value("email").toString()),
        labelled("GSTIN", company.value("gstin").toString()),
        company.value("website").toString()
    });

    const QString vendorBlock = lines({
        vendor.value("vendorName").toString(),
        vendor.value("contactPerson").toString(),
        vendor.value("vendorAddress").toString(),
        labelled("Phone", vendor.value("phone").toString()),
        labelled("Email", vendor.value("email").toString()),
        labelled("GSTIN", vendor.value("gstin").toString())
    });

    // "Ship to" is our own site: goods come back to the company placing the order.
    const QString shipToBlock = lines({
        companyName,
        company.value("addressLine1").toString(),
        company.value("addressLine2").toString(),
        company.value("city").toString(),
        labelled("Phone", company.value("phone").toString())
    });

    double subtotal = 0.0;
    QString itemRows;
    int index = 0;
    for (const QVariant &v : items) {
        const QVariantMap it = v.toMap();
        const int qty = it.value("qty").toInt();
        const double price = it.value("unitPrice").toDouble();
        const double total = it.value("totalAmount").isValid()
                                 ? it.value("totalAmount").toDouble()
                                 : qty * price;
        subtotal += total;

        const QString bg = (index++ % 2) ? QString(kZebra) : QStringLiteral("#ffffff");
        QString description = esc(it.value("partName"));
        const QString dept = it.value("department").toString().trimmed();
        if (!dept.isEmpty())
            description += QStringLiteral("<br/><span style=\"font-size:7pt;color:#7f8c8d\">")
                           + dept.toHtmlEscaped() + QStringLiteral("</span>");

        itemRows += QStringLiteral(
            "<tr bgcolor=\"%1\">"
            "<td width=\"14%\" style=\"padding:4px\">%2</td>"
            "<td width=\"46%\" style=\"padding:4px\">%3</td>"
            "<td width=\"10%\" align=\"center\" style=\"padding:4px\">%4</td>"
            "<td width=\"15%\" align=\"right\" style=\"padding:4px\">%5</td>"
            "<td width=\"15%\" align=\"right\" style=\"padding:4px\">%6</td>"
            "</tr>")
            .arg(bg, esc(it.value("partNo")), description,
                 QString::number(qty), money(price), money(total));
    }

    // Keep the table looking like a form even when only a couple of lines were
    // ordered, exactly as the paper template does.
    const int minRows = 8;
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

    const QString commentText = comments.trimmed().isEmpty()
        ? QStringLiteral("&nbsp;")
        : comments.trimmed().toHtmlEscaped().replace("\n", "<br/>");

    return QStringLiteral(R"HTML(
<html><body style="font-family:'Helvetica','Arial',sans-serif; font-size:9pt; color:#1a1a1a">

<table width="100%" cellspacing="0" cellpadding="0">
  <tr>
    <td width="52%" valign="top">
      <span style="font-size:13pt; font-weight:bold; color:%1">%2</span><br/>
      <span style="font-size:8pt; color:#404040">%3</span>
    </td>
    <td width="48%" valign="top" align="right">
      <span style="font-size:19pt; font-weight:bold; color:%4">PURCHASE ORDER</span>
      <table width="72%" cellspacing="0" cellpadding="3" align="right">
        <tr>
          <td align="right" style="font-size:8pt; font-weight:bold">DATE</td>
          <td width="55%" bgcolor="#eef2fa" align="center" style="font-size:8pt">%5</td>
        </tr>
        <tr>
          <td align="right" style="font-size:8pt; font-weight:bold">PO #</td>
          <td bgcolor="#eef2fa" align="center" style="font-size:8pt; font-weight:bold">%6</td>
        </tr>
      </table>
    </td>
  </tr>
</table>

<table width="100%" cellspacing="0" cellpadding="0" style="margin-top:14px">
  <tr>
    <td width="48%" bgcolor="%1" style="padding:4px">
      <span style="color:#ffffff; font-weight:bold; font-size:8pt">VENDOR</span></td>
    <td width="4%">&nbsp;</td>
    <td width="48%" bgcolor="%1" style="padding:4px">
      <span style="color:#ffffff; font-weight:bold; font-size:8pt">SHIP TO</span></td>
  </tr>
  <tr>
    <td valign="top" style="padding:6px; font-size:8pt">%7</td>
    <td>&nbsp;</td>
    <td valign="top" style="padding:6px; font-size:8pt">%8</td>
  </tr>
</table>

<table width="100%" cellspacing="0" cellpadding="4" border="1"
       style="margin-top:12px; border-color:%9; border-collapse:collapse">
  <tr bgcolor="%1">
    <td width="25%" align="center"><span style="color:#ffffff; font-weight:bold; font-size:8pt">REQUISITIONER</span></td>
    <td width="25%" align="center"><span style="color:#ffffff; font-weight:bold; font-size:8pt">REQUIRED PERIOD</span></td>
    <td width="25%" align="center"><span style="color:#ffffff; font-weight:bold; font-size:8pt">STATUS</span></td>
    <td width="25%" align="center"><span style="color:#ffffff; font-weight:bold; font-size:8pt">APPROVED BY</span></td>
  </tr>
  <tr>
    <td align="center" style="font-size:8pt">%10</td>
    <td align="center" style="font-size:8pt">%11</td>
    <td align="center" style="font-size:8pt">%12</td>
    <td align="center" style="font-size:8pt">%13</td>
  </tr>
</table>

<table width="100%" cellspacing="0" cellpadding="4" border="1"
       style="margin-top:12px; border-color:%9; border-collapse:collapse; font-size:8pt">
  <thead>
  <tr bgcolor="%1">
    <td align="center"><span style="color:#ffffff; font-weight:bold">ITEM #</span></td>
    <td align="center"><span style="color:#ffffff; font-weight:bold">DESCRIPTION</span></td>
    <td align="center"><span style="color:#ffffff; font-weight:bold">QTY</span></td>
    <td align="center"><span style="color:#ffffff; font-weight:bold">UNIT PRICE</span></td>
    <td align="center"><span style="color:#ffffff; font-weight:bold">TOTAL</span></td>
  </tr>
  </thead>
  %14
</table>

<table width="100%" cellspacing="0" cellpadding="0" style="margin-top:12px">
  <tr>
    <td width="58%" valign="top">
      <table width="100%" cellspacing="0" cellpadding="4" border="1"
             style="border-color:%9; border-collapse:collapse">
        <tr bgcolor="#d9d9d9">
          <td><span style="font-weight:bold; font-size:8pt">Comments or Special Instructions</span></td>
        </tr>
        <tr><td height="70" valign="top" style="font-size:8pt">%15</td></tr>
      </table>
    </td>
    <td width="4%">&nbsp;</td>
    <td width="38%" valign="top">
      <table width="100%" cellspacing="0" cellpadding="4" style="font-size:8pt">
        <tr><td align="right" style="font-weight:bold">SUBTOTAL</td>
            <td align="right" bgcolor="#eef2fa">%16</td></tr>
        <tr><td align="right" style="font-weight:bold">TAX</td>
            <td align="right" bgcolor="#eef2fa">-</td></tr>
        <tr><td align="right" style="font-weight:bold">SHIPPING</td>
            <td align="right" bgcolor="#eef2fa">-</td></tr>
        <tr><td align="right" style="font-weight:bold">OTHER</td>
            <td align="right" bgcolor="#eef2fa">-</td></tr>
        <tr bgcolor="%1">
            <td align="right"><span style="color:#ffffff; font-weight:bold">TOTAL</span></td>
            <td align="right"><span style="color:#ffffff; font-weight:bold">%17 %16</span></td></tr>
      </table>
    </td>
  </tr>
</table>

<p align="center" style="margin-top:18px; font-size:7.5pt; color:#7f8c8d">
If you have any questions about this purchase order, please contact<br/>%18
</p>

</body></html>
)HTML")
        .arg(kHeaderBlue, companyName, companyBlock, kTitleBlue,
             esc(po.value("date")), esc(po.value("poNo")),
             vendorBlock, shipToBlock, kRule)
        .arg(esc(po.value("preparedBy")),
             expectedPeriod(po),
             esc(po.value("status")),
             po.value("approvedBy").toString().trimmed().isEmpty()
                 ? QStringLiteral("-") : esc(po.value("approvedBy")),
             itemRows, commentText, money(subtotal),
             QString(QChar(0x20B9)),
             lines({companyName,
                    labelled("Phone", company.value("phone").toString()),
                    labelled("Email", company.value("email").toString())})
                 .replace("<br/>", " &nbsp;|&nbsp; "));
}

bool writePdf(const QString &html, const QString &path)
{
    return DocRender::writePdf(html, path, QStringLiteral("Purchase Order"));
}

QStringList renderPages(const QString &html, const QString &dir, int dpi)
{
    return DocRender::renderPages(html, dir, QStringLiteral("po-preview"), dpi);
}

} // namespace PoDocument
