pragma Singleton
import QtQuick

// =============================================================================
//  App - application-wide UI state
// =============================================================================
//  A dialog in its own file cannot see Main.qml's id, but it still needs to
//  know how big the window is to size itself against it. Main.qml binds these
//  once; every dialog reads them.
//
//  Keep this small. It is for state that genuinely belongs to the whole
//  window, not a convenient place to park anything two screens happen to
//  share - that is what a signal or an explicit property is for.
// =============================================================================
QtObject {
    // The application window's current size. Bound by Main.qml at startup.
    property int windowWidth: 1600
    property int windowHeight: 900
}
