#ifndef DOMAIN_STOCKSERVICE_H
#define DOMAIN_STOCKSERVICE_H

#include "domain/service.h"

#include <QString>

// =============================================================================
//  StockService - what is actually on the shelf
// =============================================================================
//  The stock grid is the live quantity for every part, and it is the one piece
//  of state several different parts of the business move: receiving against a
//  GRN puts quantity in, issuing to a department takes it out, and someone
//  correcting a miscount edits a cell directly.
//
//  Because of that the grid itself lives in the shared AppContext rather than
//  being owned here. What this service owns is the rules around it: how the
//  grid is read from and written back to the database, how a part is found by
//  name, and what has to happen after a quantity changes.
//
//  Anything that moves a quantity should call persist() when it is done, not
//  saveToDb() - persist() is what tells the rest of the application that
//  derived figures like the low-stock count are now stale.
// =============================================================================
class StockService : public Service
{
    Q_OBJECT

public:
    explicit StockService(const AppContext &ctx, QObject *parent = nullptr)
        : Service(ctx, parent) {}

    // Fills the grid from the database. False means there was nothing stored
    // yet, not that something failed.
    bool loadFromDb();
    void saveToDb();

    // Writes the grid back and announces that quantities moved. Use this after
    // an issue, a receipt or a correction; saveToDb() alone leaves the
    // low-stock count and the workbook mirror out of date.
    void persist();

    // Grid row holding this part, or -1. Row 0 is the header, so a valid
    // answer is always >= 1.
    int findRowByName(const QString &partName);

signals:
    // Quantities changed. The bridge mirrors the grid to the permanent
    // workbook, if one is set, and refreshes the low-stock badge.
    void changed();

    // The grid was just filled from the database, so it holds no unsaved
    // edits.
    void loaded();

    // Someone else changed the stock while this machine was editing. The save
    // was abandoned rather than overwriting their work; the bridge reloads
    // everything and tells the user.
    void remoteChangesDetected();
};

#endif // DOMAIN_STOCKSERVICE_H
