#include "core/dbschema.h"

// The field tables. Each row is: SQL column, application/QML key, type.
// Order is irrelevant to correctness but is kept close to the CREATE TABLE
// statements in dbmanager.cpp so the two read side by side.

const QVector<DbField> kVendorFields = {
    {"vendor_name",    "vendorName",    's'},
    {"vendor_address", "vendorAddress", 's'},
    {"bank_branch",    "bankBranch",    's'},
    {"ifsc",           "ifsc",          's'},
    {"account_number", "accountNumber", 's'},
    {"cin",            "cin",           's'},
    {"gstin",          "gstin",         's'},
    {"pan_number",     "panNumber",     's'},
    {"pan_name",       "panName",       's'},
    {"contact_person", "contactPerson", 's'},
    {"email",          "email",         's'},
    {"phone",          "phone",         's'},
    {"department",     "department",    's'},
    {"item_category",  "itemCategory",  's'},
};

const QVector<DbField> kItemFields = {
    {"part_no",      "partNo",      's'},
    {"part_name",    "partName",    's'},
    {"category",     "category",    's'},
    {"department",   "department",  's'},
    {"vendor",       "vendor",      's'},
    {"required_qty", "requiredQty", 'i'},
    {"unit_price",   "unitPrice",   'd'},
    {"stock_qty",    "stockQty",    'i'},
    {"hsn_code",     "hsnCode",     's'},
    {"unit",         "unit",        's'},
    {"item_type",    "itemType",    's'},
    {"sac_code",     "sacCode",     's'},
};

const QVector<DbField> kPoFields = {
    {"po_no",         "poNo",         's'},
    {"po_date",       "date",         's'},
    {"vendor",        "vendor",       's'},
    {"part_name",     "partName",     's'},
    {"part_no",       "partNo",       's'},
    {"department",    "department",   's'},
    {"qty",           "qty",          'i'},
    {"unit_price",    "unitPrice",    'd'},
    {"total_amount",  "totalAmount",  'd'},
    {"expected_date", "expectedDate", 's'},
    {"expected_end_date", "expectedEndDate", 's'},
    {"status",        "status",       's'},
    {"received_qty",  "receivedQty",  'i'},
    {"prepared_by",   "preparedBy",   's'},
    {"approved_by",   "approvedBy",   's'},
    {"received_by",   "receivedBy",   's'},
    {"received_date", "receivedDate", 's'},
};

const QVector<DbField> kPoItemFields = {
    {"id",           "id",           'i'},
    {"po_no",        "poNo",         's'},
    {"part_name",    "partName",     's'},
    {"part_no",      "partNo",       's'},
    {"vendor",       "vendor",       's'},
    {"department",   "department",   's'},
    {"qty",          "qty",          'i'},
    {"unit_price",   "unitPrice",    'd'},
    {"total_amount", "totalAmount",  'd'},
    {"received_qty", "receivedQty",  'i'},
};

const QVector<DbField> kGrnFields = {
    {"grn_no",       "grnNo",       's'},
    {"po_no",        "poNo",        's'},
    {"grn_date",     "date",        's'},
    {"part_name",    "partName",    's'},
    {"received_qty", "receivedQty", 'i'},
    {"accepted_qty", "acceptedQty", 'i'},
    {"rejected_qty", "rejectedQty", 'i'},
    {"remarks",      "remarks",     's'},
    {"received_by",  "receivedBy",  's'},
};

const QVector<DbField> kMovementFields = {
    {"mov_date",  "date",     's'},
    {"part_name", "partName", 's'},
    {"part_no",   "partNo",   's'},
    {"type",      "type",     's'},
    {"qty",       "qty",      'i'},
    {"reference", "reference",'s'},
    {"done_by",   "doneBy",   's'},
};

const QVector<DbField> kDcFields = {
    {"dc_no",         "dcNo",         's'},
    {"dc_date",       "date",         's'},
    {"delivery_time", "deliveryTime", 's'},
    {"party_name",    "partyName",    's'},
    {"party_address", "partyAddress", 's'},
    {"party_phone",   "partyPhone",   's'},
    {"party_email",   "partyEmail",   's'},
    {"party_gstin",   "partyGstin",   's'},
    {"ship_name",     "shipName",     's'},
    {"ship_address",  "shipAddress",  's'},
    {"ship_phone",    "shipPhone",    's'},
    {"ship_email",    "shipEmail",    's'},
    {"ship_gstin",    "shipGstin",    's'},
    {"terms",         "terms",        's'},
    {"status",        "status",       's'},
    {"total_qty",     "totalQty",     'd'},
    {"prepared_by",   "preparedBy",   's'},
    {"delivered_by",  "deliveredBy",  's'},
    {"received_by",   "receivedBy",   's'},
};

const QVector<DbField> kDcItemFields = {
    {"id",        "id",       'i'},
    {"dc_no",     "dcNo",     's'},
    {"item_name", "itemName", 's'},
    {"part_no",   "partNo",   's'},
    {"hsn_code",  "hsnCode",  's'},
    {"qty",       "qty",      'd'},
    {"unit",      "unit",     's'},
};

const QVector<DbField> kPrFields = {
    {"pr_no",        "prNo",        's'},
    {"pr_date",      "date",        's'},
    {"requested_by", "requestedBy", 's'},
    {"department",   "department",  's'},
    {"needed_by",    "neededBy",    's'},
    {"priority",     "priority",    's'},
    {"status",       "status",      's'},
    {"remarks",      "remarks",     's'},
    {"reviewed_by",  "reviewedBy",  's'},
    {"review_note",  "reviewNote",  's'},
    {"po_no",        "poNo",        's'},
};

const QVector<DbField> kPrItemFields = {
    {"id",              "id",             'i'},
    {"pr_no",           "prNo",           's'},
    {"item_name",       "itemName",       's'},
    {"part_no",         "partNo",         's'},
    {"qty",             "qty",            'i'},
    {"unit",            "unit",           's'},
    {"estimated_price", "estimatedPrice", 'd'},
    {"vendor",          "vendor",         's'},
};

const QVector<DbField> kIssueFields = {
    {"issue_no",   "issueNo",    's'},
    {"issue_date", "date",       's'},
    {"part_name",  "partName",   's'},
    {"qty",        "qty",        'i'},
    {"department", "department", 's'},
    {"issued_by",  "issuedBy",   's'},
};

static QVariant coerce(const QVariant &v, char type) {
    switch (type) {
    case 'i': return v.toInt();
    case 'd': return v.toDouble();
    default:  return v.toString();
    }
}

QVariantMap dbRowToApp(const QVector<DbField> &fields, const QVariantMap &dbRow) {
    QVariantMap out;
    for (const DbField &f : fields)
        out[f.key] = coerce(dbRow.value(f.column), f.type);
    return out;
}

QVariantMap appRowToDb(const QVector<DbField> &fields, const QVariantMap &appRow) {
    QVariantMap out;
    for (const DbField &f : fields)
        out[f.column] = coerce(appRow.value(f.key), f.type);
    return out;
}

QVector<QVariantMap> dbRowsToApp(const QVector<DbField> &fields, const QVector<QVariantMap> &dbRows) {
    QVector<QVariantMap> out;
    out.reserve(dbRows.size());
    for (const QVariantMap &r : dbRows) out.append(dbRowToApp(fields, r));
    return out;
}

QVector<QVariantMap> appRowsToDb(const QVector<DbField> &fields, const QVector<QVariantMap> &appRows) {
    QVector<QVariantMap> out;
    out.reserve(appRows.size());
    for (const QVariantMap &r : appRows) out.append(appRowToDb(fields, r));
    return out;
}
