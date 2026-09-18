# =============================================================================
#  EnsteinCore - the C++ backend
# =============================================================================
#  Everything the application knows how to *do* lives here: talking to the
#  database, the supply-chain rules, and turning orders and challans into PDFs.
#
#  This library knows nothing about the user interface. The one place it faces
#  QML is bridge/, which is the documented contract the frontend calls
#  (docs/QML_API_REFERENCE.md). Keeping that the only crossing point is what
#  lets the two halves of this project be worked on independently.
#
#  Owned by: the backend developer. The frontend never needs to edit this file.
# =============================================================================

qt_add_library(EnsteinCore STATIC
    # ---- core/ : infrastructure shared by every domain -------------------
    #  Database connections, schema and migrations, and provisioning this
    #  machine as the shared server. No business rules live here.
    core/dbmanager.h            core/dbmanager.cpp
    core/dbschema.h             core/dbschema.cpp
    core/appsettings.h          core/appsettings.cpp
    core/serversetup.h          core/serversetup.cpp

    # ---- models/ : data exposed to the views ------------------------------
    #  Qt item models the QML views bind to directly.
    models/exceltablemodel.h    models/exceltablemodel.cpp

    # ---- domain/ : the business, one area per service ---------------------
    #  Each service owns its rows and the rules that go with them, and knows
    #  nothing about QML. service.h explains the three rules they all follow.
    domain/service.h
    domain/vendorservice.h          domain/vendorservice.cpp
    domain/itemmasterservice.h      domain/itemmasterservice.cpp
    domain/deliverychallanservice.h domain/deliverychallanservice.cpp
    domain/purchaserequestservice.h domain/purchaserequestservice.cpp
    domain/stockmovementservice.h   domain/stockmovementservice.cpp
    domain/stockservice.h           domain/stockservice.cpp
    domain/materialissueservice.h   domain/materialissueservice.cpp
    domain/purchaseorderservice.h   domain/purchaseorderservice.cpp
    domain/goodsreceiptservice.h    domain/goodsreceiptservice.cpp
    domain/lowstockservice.h        domain/lowstockservice.cpp

    # ---- documents/ : printable output -----------------------------------
    #  Builds the HTML for a purchase order or delivery challan and renders it
    #  to PDF. Given plain data, so it can be tested without a database.
    documents/docrender.h       documents/docrender.cpp
    documents/podocument.h      documents/podocument.cpp
    documents/dcdocument.h      documents/dcdocument.cpp

    # ---- bridge/ : the QML-facing surface ---------------------------------
    #  ExcelHandler is the single object QML talks to. Every method here is
    #  part of the frontend contract; changing one is a breaking change and
    #  belongs in docs/QML_API_REFERENCE.md.
    bridge/excelhandler.h       bridge/excelhandler.cpp
)

# Headers are included by their path from src/, so an include line says which
# layer it reaches into:  #include "core/dbmanager.h"  not  "dbmanager.h".
target_include_directories(EnsteinCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EnsteinCore
    PUBLIC
        Qt6::Core
        Qt6::Gui        # QTextDocument, QPdfWriter for the PDF documents
        Qt6::Qml        # bridge/ exposes QObject types to QML
        Qt6::Sql
        Qt6::QuickControls2
    PRIVATE
        Qt6::Network
        Qt6::Widgets    # native file dialogs
        QXlsx::QXlsx    # .xlsx import and export
)
