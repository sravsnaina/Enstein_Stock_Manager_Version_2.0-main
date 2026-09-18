#ifndef DOMAIN_SERVICE_H
#define DOMAIN_SERVICE_H

#include <QObject>
#include <QString>

class Counters;
class DatabaseManager;
class ExcelTableModel;
class Session;

// What every service is handed: the shared database connection, the document
// numbering, and who is logged in. Passed as one struct so adding a fourth
// shared thing later is not a change to nine constructors.
struct AppContext {
    DatabaseManager *db = nullptr;
    Counters *counters = nullptr;
    Session *session = nullptr;
    // The stock grid. Shared rather than owned by one service because
    // receiving goods, issuing them and correcting a cell by hand all move the
    // same quantities, and they are different parts of the business.
    ExcelTableModel *model = nullptr;
};

// =============================================================================
//  Service - what every domain service has in common
// =============================================================================
//  A service owns one area of the business: vendors, the item master, purchase
//  orders. It holds that area's rows in memory, reads and writes them through
//  the shared DatabaseManager, and enforces the rules that go with them.
//
//  Three rules keep the domain layer workable:
//
//   1. A service owns its rows. Nothing outside it touches its containers;
//      other code asks through its public methods. This is what makes a change
//      to how vendors are stored a change to one file.
//
//   2. A service knows nothing about QML. When something goes wrong it emits
//      errorOccurred with a message meant for a person, and the bridge decides
//      whether that becomes a dialog, a status line or a log entry. That is
//      what lets the same service be driven from a test or a script.
//
//   3. Dependencies point one way. A challan needs the item master to fill in
//      an HSN code, so DeliveryChallanService holds an ItemMasterService; the
//      item master knows nothing about challans. The full graph is drawn in
//      docs/ARCHITECTURE.md - if you find yourself wanting an arrow back the
//      other way, emit a signal instead.
// =============================================================================
class Service : public QObject
{
    Q_OBJECT

public:
    explicit Service(const AppContext &ctx, QObject *parent = nullptr)
        : QObject(parent), m_db(ctx.db), m_counters(ctx.counters),
          m_session(ctx.session), m_model(ctx.model), m_ctx(ctx) {}

signals:
    // Something the user asked for could not be done. The text is shown to
    // them, so it says what failed in their words, not the SQL error.
    void errorOccurred(const QString &error);

protected:
    // All shared with every other service and owned by the bridge, not by us.
    DatabaseManager *m_db;
    Counters *m_counters;
    Session *m_session;
    ExcelTableModel *m_model;
    AppContext m_ctx;      // to hand on when one service constructs another
};

#endif // DOMAIN_SERVICE_H
