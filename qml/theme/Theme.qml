pragma Singleton
import QtQuick

// =============================================================================
//  Theme - the application's colours, in one place
// =============================================================================
//  Every colour the UI paints should come from here rather than being written
//  as a hex literal at the point of use. The reason is not tidiness: the same
//  grey means "secondary text" in twenty places, and when it needs to change
//  it has to change in all twenty or the screens drift apart.
//
//  Names say what a colour is for, not what it looks like. `danger` stays the
//  right name if the red is ever adjusted; `red` would not.
//
//  Usage:  import Enstein      then   color: Theme.textPrimary
// =============================================================================
QtObject {

    // ---- Text ---------------------------------------------------------
    readonly property color textPrimary:   "#2c3e50"   // headings, key values
    readonly property color textSecondary: "#7f8c8d"   // labels, supporting detail
    readonly property color textMuted:     "#95a5a6"   // hints, empty states
    readonly property color textInverse:   "#ffffff"   // on a filled background

    // ---- Status -------------------------------------------------------
    // These carry meaning, so they are used consistently: money and completed
    // work in success, anything awaiting action in warning, anything the user
    // should not miss in danger.
    readonly property color success:      "#27ae60"
    readonly property color warning:      "#e67e22"
    readonly property color danger:       "#e74c3c"
    readonly property color dangerStrong: "#c0392b"    // pressed / emphasised
    readonly property color info:         "#3498db"
    readonly property color pending:      "#f39c12"

    // ---- Surfaces and lines -------------------------------------------
    readonly property color surface:      "#ffffff"
    readonly property color surfaceAlt:   "#f8f9fa"    // panels, entry forms
    readonly property color workspace:    "#f4f7fb"    // application canvas
    readonly property color border:       "#dee2e6"
    readonly property color borderSubtle: "#e0e0e0"    // inside lists
    readonly property color divider:      "#bdc3c7"

    // ---- Tinted backgrounds -------------------------------------------
    // Pale washes behind a status message, matched to the status colours.
    readonly property color successWash: "#e8f8e8"
    readonly property color warningWash: "#fff8e8"
    readonly property color dangerWash:  "#fdecea"
    readonly property color infoWash:    "#f0f8ff"

    // ---- Spacing ------------------------------------------------------
    // A small scale rather than free numbers, so gaps line up across screens.
    readonly property int spacingTight:  4
    readonly property int spacingSmall:  8
    readonly property int spacing:       10
    readonly property int spacingLarge:  16

    // ---- Shape and control rhythm ------------------------------------
    readonly property int radiusSmall:   4
    readonly property int radius:        6
    readonly property int controlHeight: 34

    // ---- Type sizes ---------------------------------------------------
    readonly property int fontSizeTitle:   16
    readonly property int fontSizeHeading: 14
    readonly property int fontSizeBody:    13
    readonly property int fontSizeLabel:   11
    readonly property int fontSizeSmall:   10
}
