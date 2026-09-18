#ifndef CORE_DBSCHEMA_H
#define CORE_DBSCHEMA_H

#include <QVariantMap>
#include <QVector>

// =============================================================================
//  Where an application field lives in the database
// =============================================================================
//  Two vocabularies describe the same row. QML and the domain services speak
//  camelCase keys in a QVariantMap ("partNo", "unitPrice"); SQL speaks
//  snake_case columns ("part_no", "unit_price"). This file is the single place
//  the two are matched up, so renaming a column is one edit here rather than a
//  hunt through every query.
//
//  Adding a column? Add the DbField entry to the table below AND the matching
//  ALTER TABLE to DatabaseManager::migrateSchema(), or existing installations
//  will read and write a column that is not there.
// =============================================================================

struct DbField {
    const char *column;   // SQL column name
    const char *key;      // QVariantMap key used by the app / QML
    char type;            // 's' string, 'i' int, 'd' double
};

extern const QVector<DbField> kVendorFields;
extern const QVector<DbField> kItemFields;
extern const QVector<DbField> kPoFields;
extern const QVector<DbField> kPoItemFields;
extern const QVector<DbField> kGrnFields;
extern const QVector<DbField> kMovementFields;
extern const QVector<DbField> kDcFields;
extern const QVector<DbField> kDcItemFields;
extern const QVector<DbField> kPrFields;
extern const QVector<DbField> kPrItemFields;
extern const QVector<DbField> kIssueFields;

// ---- Row conversion --------------------------------------------------------
// Values are coerced on the way through, because SQLite stores whatever was
// written regardless of the column's declared type: a qty read back from an
// old row can arrive as the string "5" rather than the number 5.

QVariantMap dbRowToApp(const QVector<DbField> &fields, const QVariantMap &dbRow);
QVariantMap appRowToDb(const QVector<DbField> &fields, const QVariantMap &appRow);

QVector<QVariantMap> dbRowsToApp(const QVector<DbField> &fields,
                                 const QVector<QVariantMap> &dbRows);
QVector<QVariantMap> appRowsToDb(const QVector<DbField> &fields,
                                 const QVector<QVariantMap> &appRows);

#endif // CORE_DBSCHEMA_H
