# =============================================================================
#  EnsteinUi - the QML frontend
# =============================================================================
#  Every screen, dialog and reusable control the user sees. This module links
#  only against Qt Quick: it reaches the backend at runtime through the single
#  `excelHandler` object, never by including a C++ header. That is deliberate,
#  and it is what keeps the frontend buildable and reviewable on its own.
#
#  Adding a .qml file? Add it to QML_FILES below or it will not be compiled
#  into the binary, and the app will fail to find it at runtime.
#
#  Owned by: the frontend developer. The backend never needs to edit this file.
# =============================================================================

# ----- Singletons -------------------------------------------------------------
# A singleton needs BOTH `pragma Singleton` at the top of the .qml file and this
# property. Miss either and the engine treats it as an ordinary component, so
# every use of it fails at runtime rather than at build time.
#
# This must come before qt_add_qml_module(), which reads these properties while
# building the module's type registration.
set_source_files_properties(
    util/Format.qml
    util/App.qml
    util/Departments.qml
    theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(EnsteinUi
    URI Enstein
    VERSION 1.0

    # Static, so the generated EnsteinUiplugin is a linkable library rather
    # than a runtime-loadable module. A shared backing library gets dropped by
    # the linker here - the executable calls none of its symbols, it only wants
    # its compiled QML - and the resources vanish with it.
    STATIC

    # Puts the files at qrc:/Enstein/... . src/app/main.cpp loads
    # qrc:/Enstein/Main.qml, so changing this prefix changes that URL too.
    RESOURCE_PREFIX "/"

    # Must end in the module's target path, or qmllint and the QML language
    # server cannot resolve `import Enstein` and the frontend loses its
    # autocomplete and error checking.
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Enstein

    QML_FILES
        Main.qml

        # ---- Shared foundation ------------------------------------------
        # One instance each for the whole application, reachable from any file
        # with `import Enstein`.
        util/Format.qml     # number and date formatting
        util/App.qml        # window size, for dialogs to fit inside
        util/Departments.qml # controlled master-data department names
        theme/Theme.qml     # colours, spacing and type sizes

        # ---- Dialogs, grouped by the part of the business they serve -----
        dialogs/common/ErrorDialog.qml
        dialogs/common/LoginDialog.qml
        dialogs/common/StockSearchDialog.qml
        dialogs/common/DatePickerDialog.qml

        dialogs/vendor/VendorMasterDialog.qml
        dialogs/vendor/VendorDetailsDialog.qml
        dialogs/item/ItemMasterDialog.qml
        dialogs/item/ItemDetailsDialog.qml

        dialogs/inventory/ReportsDialog.qml
        dialogs/inventory/IssueNotesDialog.qml
        dialogs/inventory/StockMovementsDialog.qml
        dialogs/inventory/LowStockDialog.qml

        dialogs/po/PoSavedDialog.qml
        dialogs/po/PoPartDetailsDialog.qml
        dialogs/po/PoDetailsDialog.qml
        dialogs/po/PoApprovalDialog.qml
        dialogs/po/PoPdfPreviewDialog.qml

        dialogs/pr/PrDetailsDialog.qml

        dialogs/settings/CompanyProfileDialog.qml

        dialogs/dc/DcSavedDialog.qml
        dialogs/dc/DcPdfPreviewDialog.qml
)

target_link_libraries(EnsteinUi PRIVATE Qt6::Quick)
